/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set expandtab ts=4 shiftwidth=4: */
/* 
 * Copyright 2007-2009 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 *
 * CDDL HEADER START
 * 
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 * 
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 * 
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 * 
 * CDDL HEADER END
 * 
 */
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include "notify.h"

#define     TEST_PROF_SET_OR_RETURN(pref) \
    { \
        NwamuiProf* prof = nwamui_prof_get_instance(); \
        if ( ! nwamui_prof_get_notification_#pref( prof )) ) \
            return; \
        g_object_unref(G_OBJECT( prof )); \
    }

static NotifyNotification*   notification           = NULL;
static GtkStatusIcon*        parent_status_icon     = NULL;
static GdkPixbuf            *default_icon           = NULL;
static GQueue                msg_q                  = G_QUEUE_INIT;

static guint adjust_source_id = 0;
static gint gconf_exp_time;
#define DEFAULT_EXP_TIME	gconf_exp_time
#define TIGHT_EXP_TIME	((gint) (0.5 * DEFAULT_EXP_TIME))

#define NOTIFY_EXP_DEFAULT_FLAG	(NOTIFY_EXPIRES_DEFAULT)
#define NOTIFY_EXP_ADJUSTED_FLAG	(NOTIFY_EXP_DEFAULT_FLAG - 1)
#define NOTIFY_POLL_STATUS_ICON_INVISIBLE	(1000)

#define NOTIFY_ICON_SIZE    (32)

typedef struct _msg
{
    NotifyNotification *n;
    gchar *summary;
    gchar *body;
    GdkPixbuf* icon;
    gchar *action;
    gchar *label;
    NotifyActionCallback cb;
    gpointer user_data;
    GFreeFunc free_func;
    gint timeout;
    GTimeVal t;
} msg;

static msg *nwam_notification_msg_new(NotifyNotification *n,
  const gchar *summary,
  const gchar *body,
  const GdkPixbuf* icon,
  const gchar *action,
  const gchar *label,
  NotifyActionCallback callback,
  gpointer user_data,
  GFreeFunc free_func,
  gint timeout);

static void nwam_notification_msg_free(msg *m);

static gboolean nwam_notification_show_message_cb(GQueue *q);

static void nwam_notification_show_nth_message(GQueue *q, guint nth);

static gboolean nwam_notification_hide_cb(NotifyNotification *n);

static gboolean notify_notification_adjust_first(GQueue *q);

static void notify_notification_adjust_nth(GQueue *q, guint nth, gint new_timeout);

static void on_prof_notification_default_timeout(GObject *gobject, GParamSpec *arg1, gpointer data);

static notification_style_t     notification_style = NOTIFICATION_STYLE_CREATE_ALWAYS;

extern void
notify_notification_set_notification_style( notification_style_t style )
{
    notification_style = style;
}

static msg *
nwam_notification_msg_new(NotifyNotification *n,
  const gchar *summary,
  const gchar *body,
  const GdkPixbuf* icon,
  const gchar *action,
  const gchar *label,
  NotifyActionCallback callback,
  gpointer user_data,
  GFreeFunc free_func,
  gint timeout)
{
    msg *m = g_new0(msg, 1);

    m->n = n;
    m->summary = g_strdup(summary);
    m->body = g_strdup(body);
    if (icon)
        m->icon = g_object_ref(G_OBJECT(icon));
    m->action = g_strdup(action);
    m->label = g_strdup(label);
    m->cb = callback;
    m->user_data = user_data;
    m->free_func = free_func;
    m->timeout = (timeout == NOTIFY_EXPIRES_DEFAULT) ? NOTIFY_EXP_DEFAULT_FLAG : timeout;

    nwamui_debug("NOTIFICATION_MESSAGE: Summary = '%s' ; Body = '%s' ; Action = '%s' ; Label = '%s'", 
                 summary?summary:"NULL",
                 body?body:"NULL",
                 action?action:"NULL",
                 label?label:"NULL");
    return m;
}

static void
nwam_notification_msg_free(msg *m)
{
    if ( notification_style != NOTIFICATION_STYLE_REUSE ) {
        notify_notification_close(m->n, NULL); /* Close notification now! */
        g_object_unref(m->n);
        m->n = NULL;
    }

    g_free(m->summary);
    g_free(m->body);
    if (m->icon)
        g_object_unref(m->icon);
    g_free(m->action);
    g_free(m->label);
    g_free(m);
}

static void
on_prof_notification_default_timeout(GObject *gobject, GParamSpec *arg1, gpointer data)
{
    NwamuiProf* prof = NWAMUI_PROF(gobject);
    msg *m;
    gint timeout;

    g_object_get(gobject, "notification_default_timeout", &gconf_exp_time, NULL);

    timeout = g_queue_get_length(&msg_q) == 1 ?
      DEFAULT_EXP_TIME : TIGHT_EXP_TIME;

    g_debug("#### Notification default timeout %d ####", DEFAULT_EXP_TIME);

    /* restart the timer */
    if (adjust_source_id > 0) {
        g_source_remove(adjust_source_id);
        adjust_source_id = 0;
    }
    /* modify the first show */
    notify_notification_adjust_nth(&msg_q, 0, timeout);
}

static void
on_notification_closed(NotifyNotification *n,
  gpointer user_data)
{
    GQueue *q = (GQueue *)user_data;
    msg *m = NULL;

    /* Pop up the msg triggered this signal. */
    if ((m = g_queue_pop_head(q)) != NULL) {
        if (m->cb) {
            notify_notification_clear_actions(m->n);
        }

        nwam_notification_msg_free(m);
    }

    /* Prepare the next show */
    if ((m = g_queue_peek_head(q)) != NULL) {
        g_timeout_add_full(G_PRIORITY_DEFAULT,
          0,
          (GSourceFunc)nwam_notification_show_message_cb,
          (gpointer) q,
          NULL);
    }
}

static NotifyNotification *
get_notification( void )
{
    gboolean always_new = FALSE;
    gboolean with_status_icon = TRUE;
    
    g_assert ( parent_status_icon != NULL );

    switch( notification_style ) {
        case NOTIFICATION_STYLE_CREATE_ALWAYS_NO_STATUS_ICON:
            with_status_icon = FALSE;
            /* Fall-Through */
        case NOTIFICATION_STYLE_CREATE_ALWAYS:
            always_new = TRUE;
            break;
        case NOTIFICATION_STYLE_REUSE:
            /* Fall-Through */
        default:
            /* Use default */
            break;
    }
    if ( always_new || notification == NULL ) {
        
        if ( with_status_icon ) {
            notification = notify_notification_new_with_status_icon(" ", " ", NULL, parent_status_icon  );
        }
        else {
            notification = notify_notification_new(" ", " ", NULL, NULL  );
        }

        g_signal_connect(notification, "closed",
          G_CALLBACK(on_notification_closed),
          (gpointer) &msg_q);

        /* nwamui preference signals */
        {
            NwamuiProf *prof = nwamui_prof_get_instance ();

            g_object_get (prof,
              "notification_default_timeout", &gconf_exp_time,
              NULL);
            
            /* TODO remove it after gconf value is enabled */
            if (gconf_exp_time <= 0) {
                gconf_exp_time = 3000;
            }

/*             g_debug("#### Notification default timeout %d ####", DEFAULT_EXP_TIME); */

            g_signal_connect(prof, "notify::notification-default-timeout",
              G_CALLBACK(on_prof_notification_default_timeout), NULL);

/*             nwamui_prof_notify_begin (prof); */

            g_object_unref (prof);
        }


    }
    return notification;
}

/* We use NOTIFY_EXPIRES_DEFAULT as a flag which means to use gconf
 * timeout
 */
static void
notify_notification_adjust_nth(GQueue *q, guint nth, gint new_timeout)
{
    msg *m;

    if ((m = g_queue_peek_nth(q, nth)) != NULL) {
        if (m->timeout == NOTIFY_EXP_DEFAULT_FLAG) {
            m->timeout = NOTIFY_EXP_ADJUSTED_FLAG;
        }
    }

    if (adjust_source_id == 0) {
        adjust_source_id = g_timeout_add_full(G_PRIORITY_DEFAULT,
          0,
          (GSourceFunc)notify_notification_adjust_first,
          (gpointer) q,
          NULL);
    }
}

static gboolean
notify_notification_adjust_first(GQueue *q)
{
    msg *m;

    g_assert(adjust_source_id > 0);

    /* Do not show notifications if status icon is invisible. */
    if (!gtk_status_icon_is_embedded(parent_status_icon) ||
      !gtk_status_icon_get_visible(parent_status_icon)) {
        adjust_source_id = g_timeout_add_full(G_PRIORITY_DEFAULT,
          NOTIFY_POLL_STATUS_ICON_INVISIBLE,
          (GSourceFunc)notify_notification_adjust_first,
          (gpointer) q,
          NULL);

        return FALSE;
    }

    /* reset source id since the timer should always be deleted/updated */
    adjust_source_id = 0;

    if ((m = g_queue_peek_head(q)) == NULL) {
        return FALSE;
    }

    if (m->t.tv_sec == 0 && m->t.tv_usec == 0) {
        nwam_notification_show_nth_message(q, 0);
        return FALSE;
    }

    /* It's showing */
    if (m->t.tv_sec > 0) {
        GTimeVal ct;
        gint timeout = 0;

        /* If adjustable */
        switch (m->timeout) {
        case NOTIFY_EXP_DEFAULT_FLAG:
            timeout = DEFAULT_EXP_TIME;
            break;
        case NOTIFY_EXP_ADJUSTED_FLAG:
            timeout = TIGHT_EXP_TIME;
            break;
        default:
            timeout = m->timeout;
            break;
        }

        g_get_current_time(&ct);

        timeout -= (gint) ((ct.tv_sec - m->t.tv_sec) * 1000 +
          (ct.tv_usec - m->t.tv_usec) / 1000);

/*         g_debug("#### %s adjust %d ####", __func__, timeout > 0 ? timeout : 0); */

        if (timeout > 0) {
            adjust_source_id = g_timeout_add_full(G_PRIORITY_DEFAULT,
              timeout,
              (GSourceFunc)notify_notification_adjust_first,
              (gpointer) q,
              NULL);
        } else {
            /* terminate the current show if there are pending msgs */
            if (g_queue_get_length(q) > 1) {
/*                 notify_notification_close(m->n, NULL); */
                nwam_notification_show_nth_message(q, 1);
            }
        }
    }
    return FALSE;
}

static void
notification_cleanup( void )
{
    if ( notification != NULL ) {
        g_object_unref( parent_status_icon );
        notify_notification_close(notification, NULL); /* Close notification now! */
        notify_uninit();
    }
}

static gboolean
nwam_notification_hide_cb(NotifyNotification *n)
{
    GError              *err = NULL;
    if (!notify_notification_close(n, &err)) {
        g_warning(err->message);
        g_error_free( err );
    }
    return FALSE;
}

static void
nwam_notification_show_nth_message(GQueue *q, guint nth)
{
    msg *m;

    /* pop up 0 ~ nth - 1 msgs */
    for (; nth > 0; nth--) {
        m = g_queue_pop_head(q);
        nwam_notification_msg_free(m);
    }

    if ((m = g_queue_peek_nth(q, nth)) != NULL) {
        GError              *err = NULL;
        const gchar* real_action = m->action;
        const gchar* real_label = m->label;
        gint timeout = 0;

        notify_notification_update(m->n, m->summary, m->body, NULL);
/*           ((m->icon!=NULL) ? (m->icon) : (default_icon))); */
        if (m->icon || default_icon)
            notify_notification_set_icon_from_pixbuf(m->n,
              ((m->icon!=NULL) ? (m->icon) : (default_icon)));

        if (m->cb) {
            if (m->action == NULL || *(m->action) == '\0') {
                real_action = "default";
            }
            if (m->label == NULL || *(m->label) == '\0') {
                real_label = "default";
            }
    
            notify_notification_add_action(m->n, real_action, real_label,
              m->cb, m->user_data, m->free_func);
        } else {
            notify_notification_clear_actions(m->n);
        }

        /* Set the timeout */
        switch (m->timeout) {
        case NOTIFY_EXP_DEFAULT_FLAG:
            timeout = DEFAULT_EXP_TIME;
            break;
        case NOTIFY_EXP_ADJUSTED_FLAG:
            timeout = TIGHT_EXP_TIME;
            break;
        default:
            timeout = m->timeout;
            break;
        }

        notify_notification_set_timeout(m->n, timeout);

        if (!notify_notification_show(m->n, &err)) {
            g_warning(err->message);
            g_error_free( err );
        }

        /* show time */
        g_get_current_time(&m->t);

        if (adjust_source_id == 0) {
            adjust_source_id = g_timeout_add_full(G_PRIORITY_DEFAULT,
              timeout,
              (GSourceFunc)notify_notification_adjust_first,
              (gpointer) q,
              NULL);
        }
    }
}

static gboolean
nwam_notification_show_message_cb(GQueue *q)
{
    msg *m;

    nwam_notification_show_nth_message(q, 0);

    return FALSE;
}

static void
nwam_notification_show_message(const gchar* summary,
  const gchar* body,
  const GdkPixbuf* icon,
  gint timeout)
{
    NotifyNotification  *n = get_notification();
    msg                 *m;
    
    g_assert (summary != NULL && *summary != '\0'); /* Must have a value! */
    
    m = nwam_notification_msg_new(n, summary, body, icon, NULL, NULL, NULL, NULL, NULL, timeout);

    g_queue_push_tail(&msg_q, m);

    /* adjust the original last show */
    notify_notification_adjust_nth(&msg_q,
      g_queue_get_length(&msg_q) - 2,
      TIGHT_EXP_TIME);
}

static void
nwam_notification_show_message_with_action (const gchar* summary,
  const gchar* body,
  const GdkPixbuf* icon,
  const gchar* action,
  const gchar* label,
  NotifyActionCallback callback,
  gpointer user_data,
  GFreeFunc free_func,
  gint timeout)
{
    NotifyNotification  *n = get_notification();
    msg                 *m;

    g_assert (summary != NULL && *summary != '\0'); /* Must have a value! */
    
    m = nwam_notification_msg_new(n, summary, body, icon, action, label, callback, user_data, free_func, timeout);

    g_queue_push_tail(&msg_q, m);

    /* adjust the original last show */
    notify_notification_adjust_nth(&msg_q,
      g_queue_get_length(&msg_q) - 2,
      TIGHT_EXP_TIME);
}

/* 
 * Show a message to tell the user that NWAM is unavailable.
 *
 * For a wireless ncu it will also show the ESSID.
 */
void
nwam_notification_show_nwam_unavailable( void )
{
    NwamuiDaemon    *daemon = nwamui_daemon_get_instance();
    GdkPixbuf       *icon = NULL;

    if ( !nwamui_prof_get_notification_nwam_unavailable(nwamui_prof_get_instance_noref()) ) {
        nwamui_debug("returning early since gconf settings is FALSE", NULL);
        return;
    }

    if ( daemon ) {
        icon = nwamui_util_get_env_status_icon( NULL, nwamui_daemon_get_status_icon_type(daemon), NOTIFY_ICON_SIZE );
    }
    nwam_notification_show_message(_("Automatic network configuration daemon is unavailable."),
            _("For further information please run\n\"svcs -xv nwam\" in a terminal."),
            icon,
            NOTIFY_EXPIRES_DEFAULT);

    if ( daemon != NULL ) {
        g_object_unref(G_OBJECT(daemon));
    }
}

/* 
 * Show a message to tell the user that a connection has been made.
 *
 * For a wireless ncu it will also show the ESSID.
 */
void
nwam_notification_show_ncu_connected( NwamuiNcu* ncu )
{
    NwamuiDaemon    *daemon = nwamui_daemon_get_instance();
    GdkPixbuf       *icon = NULL;
    gchar           *summary_str = NULL;
    gchar           *body_str = NULL;
    gchar           *display_name = NULL;
    guint            speed = 0;
    gchar           *address_str = NULL;
    

    g_return_if_fail( ncu != NULL );

    if ( !nwamui_prof_get_notification_ncu_connected(nwamui_prof_get_instance_noref()) ) {
        nwamui_debug("returning early since gconf settings is FALSE", NULL);
        return;
    }

    if ( daemon ) {
        icon = nwamui_util_get_ncu_status_icon( ncu, NOTIFY_ICON_SIZE );
    }

    display_name = nwamui_ncu_get_display_name( ncu );
    body_str = nwamui_ncu_get_connection_state_detail_string( ncu, TRUE );

    switch ( nwamui_ncu_get_ncu_type( ncu ) ) {
        case NWAMUI_NCU_TYPE_WIRELESS: {
                NwamuiWifiNet  *wifi_net = NULL;
                gchar          *essid = NULL;

                if ( (wifi_net = nwamui_ncu_get_wifi_info( ncu )) != NULL) {
                    essid = nwamui_object_get_name(NWAMUI_OBJECT(wifi_net));
                    g_object_unref(G_OBJECT(wifi_net));
                }

                if ( essid != NULL ) {
                    summary_str = g_strdup_printf(_("%s connected to %s"), 
                                                  display_name, essid );
                    g_free(essid);
                    break;
                }
                else {
                    /* Fall through */
                }
            }
#ifdef TUNNEL_SUPPORT
        case NWAMUI_NCU_TYPE_TUNNEL:
            /* Fall through */
#endif /* TUNNEL_SUPPORT */
        case NWAMUI_NCU_TYPE_WIRED:
            /* Fall through */
        default:
            summary_str = g_strdup_printf(_("%s connected"), display_name );
            break;
    }

    nwam_notification_show_message( summary_str, body_str,
                                    icon, NOTIFY_EXPIRES_DEFAULT);

    g_free(summary_str);
    g_free(body_str);

    if ( daemon != NULL ) {
        g_object_unref(G_OBJECT(daemon));
    }
}

/* 
 * Show a message to tell the user that a connection has been broken.
 *
 * For a wireless ncu it will also show the ESSID.
 */
void
nwam_notification_show_ncu_disconnected( NwamuiNcu*           ncu,
                                         NotifyActionCallback callback,
                                         GObject*             user_object )
{
    NwamuiDaemon    *daemon = nwamui_daemon_get_instance();
    GdkPixbuf       *icon = NULL;
    gchar           *summary_str = NULL;
    gchar           *body_str = NULL;
    gchar           *display_name = NULL;
    

    g_return_if_fail( ncu != NULL );

    if ( !nwamui_prof_get_notification_ncu_disconnected(nwamui_prof_get_instance_noref()) ) {
        nwamui_debug("returning early since gconf settings is FALSE", NULL);
        return;
    }

    if ( daemon ) {
        icon = nwamui_util_get_ncu_status_icon( ncu, NOTIFY_ICON_SIZE );
    }

    display_name = nwamui_ncu_get_display_name( ncu );

    switch ( nwamui_ncu_get_ncu_type( ncu ) ) {
        case NWAMUI_NCU_TYPE_WIRELESS: {
                NwamuiWifiNet  *wifi_net = NULL;
                gchar          *essid = NULL;

                if ( (wifi_net = nwamui_ncu_get_wifi_info( ncu )) != NULL) {
                    essid = nwamui_object_get_name(NWAMUI_OBJECT(wifi_net));
                    g_object_unref(G_OBJECT(wifi_net));
                }

                body_str = g_strdup(_("Click on this message to view other available networks."));

                if ( essid != NULL ) {
                    summary_str = g_strdup_printf(_("%s disconnected from %s"), 
                                                  display_name, essid );
                    g_free(essid);
                    break;
                }
                else {
                    /* Fall through */
                }
            }
#ifdef TUNNEL_SUPPORT
        case NWAMUI_NCU_TYPE_TUNNEL:
            /* Fall through */
#endif /* TUNNEL_SUPPORT */
        case NWAMUI_NCU_TYPE_WIRED:
            /* Fall through */
        default:
            summary_str = g_strdup_printf(_("%s disconnected"), display_name );
            if ( body_str == NULL ) {
                const gchar* aux_string = NULL;
                /* Get state reason from NWAM aux_state */
                nwamui_object_get_nwam_state(NWAMUI_OBJECT(ncu), NULL, &aux_string, NWAM_NCU_TYPE_LINK );
                if ( aux_string != NULL ) {
                    body_str = g_strdup( aux_string );
                }
            }
            break;
    }

    if ( callback != NULL ) {
        nwam_notification_show_message_with_action(summary_str, body_str,
          NULL,
          NULL,	/* action */
          NULL,	/* label */
          callback,
          (gpointer) (user_object)?g_object_ref (user_object):NULL,
          (GFreeFunc)(user_object)?g_object_unref:NULL,
          NOTIFY_EXPIRES_DEFAULT);
    }
    else {
        nwam_notification_show_message( summary_str, body_str,
                                        icon, NOTIFY_EXPIRES_DEFAULT);
    }

    g_free(display_name);
    g_free(summary_str);
    g_free(body_str);

    if ( daemon != NULL ) {
        g_object_unref(G_OBJECT(daemon));
    }
}

/* 
 * Show a message to tell the user that an attempt to connect to a wireless
 * ncu has failed.
 */
void
nwam_notification_show_ncu_wifi_connect_failed( NwamuiNcu* ncu )
{
    NwamuiDaemon    *daemon = nwamui_daemon_get_instance();
    GdkPixbuf       *icon = NULL;
    gchar           *summary_str = NULL;
    gchar           *body_str = NULL;
    gchar           *display_name = NULL;
    

    g_return_if_fail( ncu != NULL );

    if ( !nwamui_prof_get_notification_ncu_wifi_connect_failed(nwamui_prof_get_instance_noref()) ) {
        nwamui_debug("returning early since gconf settings is FALSE", NULL);
        return;
    }

    if ( daemon ) {
        icon = nwamui_util_get_ncu_status_icon( ncu, NOTIFY_ICON_SIZE );
    }

    switch ( nwamui_ncu_get_ncu_type( ncu ) ) {
        case NWAMUI_NCU_TYPE_WIRELESS: {
                NwamuiWifiNet  *wifi_net = NULL;
                gchar          *essid = NULL;

                if ( (wifi_net = nwamui_ncu_get_wifi_info( ncu )) != NULL) {
                    essid = nwamui_object_get_name(NWAMUI_OBJECT(wifi_net));
                    g_object_unref(G_OBJECT(wifi_net));
                }

                body_str = g_strdup(_("Click on this message to view other available networks."));

                if ( essid != NULL ) {
                    summary_str = g_strdup_printf(_("Unable to connect to %s"), essid );
                    g_free(essid);
                }
            }
            break;
#ifdef TUNNEL_SUPPORT
        case NWAMUI_NCU_TYPE_TUNNEL:
            /* Fall through */
#endif /* TUNNEL_SUPPORT */
        case NWAMUI_NCU_TYPE_WIRED:
            /* Fall through */
        default:
            /* Do nothing if not wireless */
            break;
    }

    if ( summary_str != NULL ) {
        nwam_notification_show_message( summary_str, body_str,
                                        icon, NOTIFY_EXPIRES_DEFAULT);
    }

    g_free(summary_str);
    g_free(body_str);

    if ( daemon != NULL ) {
        g_object_unref(G_OBJECT(daemon));
    }
}

/* 
 * Show a message to tell the user that wireless selection is needed.
 */
void
nwam_notification_show_ncu_wifi_selection_needed(   NwamuiNcu           *ncu, 
                                                    NotifyActionCallback callback,
                                                    GObject*             user_object )
{
    NwamuiDaemon    *daemon = nwamui_daemon_get_instance();
    GdkPixbuf       *icon = NULL;
    gchar           *body = NULL;
    gchar           *summary = NULL;

    g_return_if_fail( ncu != NULL );

    if ( !nwamui_prof_get_notification_ncu_wifi_selection_needed(nwamui_prof_get_instance_noref()) ) {
        nwamui_debug("returning early since gconf settings is FALSE", NULL);
        return;
    }

    if ( daemon ) {
/*         icon = nwamui_util_get_env_status_icon( NULL, nwamui_daemon_get_status_icon_type(daemon), NOTIFY_ICON_SIZE ); */
        icon = nwamui_util_get_ncu_status_icon( ncu, NOTIFY_ICON_SIZE );
    }

    summary = g_strdup(_("Wireless Selection Required"));
    body = g_strdup(_("Right-click the icon to connect to an available network."));


    /* XXXX - Need to also use dialog? */
    nwam_notification_show_message_with_action(
                summary, 
                body,
                icon,
                NULL,	/* action */
                NULL,	/* label */
                callback,
                (gpointer)  user_object?g_object_ref(user_object):NULL,
                (GFreeFunc) user_object?g_object_unref:NULL,
                NOTIFY_EXPIRES_DEFAULT);
		
    g_free(body);
    g_free(summary);

    if ( daemon != NULL ) {
        g_object_unref(G_OBJECT(daemon));
    }
}

/* 
 * Show a message to tell the user that wireless key is needed.
 */
void
nwam_notification_show_ncu_wifi_key_needed( NwamuiWifiNet       *wifi_net, 
                                            NotifyActionCallback callback )
{
    NwamuiDaemon    *daemon = nwamui_daemon_get_instance();
    GdkPixbuf       *icon = NULL;
    gchar           *name;
    gchar           *body = NULL;
    gchar           *summary = NULL;

    g_return_if_fail( wifi_net != NULL );

    if ( !nwamui_prof_get_notification_ncu_wifi_key_needed(nwamui_prof_get_instance_noref()) ) {
        nwamui_debug("returning early since gconf settings is FALSE", NULL);
        return;
    }

    if ( daemon ) {
/*         icon = nwamui_util_get_env_status_icon( NULL, nwamui_daemon_get_status_icon_type(daemon), NOTIFY_ICON_SIZE ); */
        icon = nwamui_util_get_ncu_status_icon( nwamui_wifi_net_get_ncu(wifi_net), NOTIFY_ICON_SIZE );
    }

    name = nwamui_wifi_net_get_display_string(wifi_net, FALSE);
    summary = g_strdup_printf (_("'%s' requires authentication"), name);
    body = g_strdup_printf (_("Click here to enter the wireless network key."));

    /* XXXX - Need to also use dialog? */

    nwam_notification_show_message_with_action(
                summary, 
                body,
                icon,
                NULL,	/* action */
                NULL,	/* label */
                callback,
                (gpointer)g_object_ref(wifi_net),
                (GFreeFunc)g_object_unref,
                NOTIFY_EXPIRES_DEFAULT);
		
    g_free(name);
    g_free(body);
    g_free(summary);

    if ( daemon != NULL ) {
        g_object_unref(G_OBJECT(daemon));
    }
}

/* 
 * Show a message to tell the user that no wireless networks could be found.
 */
void
nwam_notification_show_no_wifi_networks( NotifyActionCallback callback, GObject *user_object )
{
    NwamuiDaemon    *daemon = nwamui_daemon_get_instance();
    GdkPixbuf       *icon = NULL;

    if ( !nwamui_prof_get_notification_no_wifi_networks(nwamui_prof_get_instance_noref()) ) {
        nwamui_debug("returning early since gconf settings is FALSE", NULL);
        return;
    }

    if ( daemon ) {
        icon = nwamui_util_get_env_status_icon( NULL, nwamui_daemon_get_status_icon_type(daemon), NOTIFY_ICON_SIZE );
    }
    nwam_notification_show_message_with_action(_("No wireless networks found"),
      _("Click this message to join an unlisted network"),
      icon,
      NULL,	/* action */
      NULL,	/* label */
      callback,
      (gpointer)g_object_ref(user_object),
      (GFreeFunc)g_object_unref,
      NOTIFY_EXPIRES_DEFAULT);

    if ( daemon != NULL ) {
        g_object_unref(G_OBJECT(daemon));
    }
}

/* 
 * Show a message to tell the user that the active ncp has changed.
 */
void
nwam_notification_show_ncp_changed( NwamuiNcp* ncp )
{
    NwamuiDaemon    *daemon = nwamui_daemon_get_instance();
    GdkPixbuf       *icon = NULL;
    gchar           *summary_str = NULL;

    g_return_if_fail( ncp != NULL );

    if ( !nwamui_prof_get_notification_ncp_changed(nwamui_prof_get_instance_noref()) ) {
        nwamui_debug("returning early since gconf settings is FALSE", NULL);
        return;
    }

    if ( daemon ) {
        icon = nwamui_util_get_env_status_icon( NULL, nwamui_daemon_get_status_icon_type(daemon), NOTIFY_ICON_SIZE );
    }

    {
        gchar *name = nwamui_object_get_name(NWAMUI_OBJECT(ncp));
        summary_str = g_strdup_printf(_("Switched to Network Profile '%s'"), name);
        g_free(name);
    }

    nwam_notification_show_message(summary_str,
            "",
            icon,
            NOTIFY_EXPIRES_DEFAULT);

    g_free(summary_str);

    if ( daemon != NULL ) {
        g_object_unref(G_OBJECT(daemon));
    }
}

/* 
 * Show a message to tell the user that the active location has changed.
 */
void
nwam_notification_show_location_changed( NwamuiEnv* env )
{
    NwamuiDaemon    *daemon = nwamui_daemon_get_instance();
    GdkPixbuf       *icon = NULL;
    gchar           *summary_str = NULL;

    g_return_if_fail( env != NULL );

    if ( !nwamui_prof_get_notification_location_changed(nwamui_prof_get_instance_noref()) ) {
        nwamui_debug("returning early since gconf settings is FALSE", NULL);
        return;
    }

    if ( daemon ) {
        icon = nwamui_util_get_env_status_icon( NULL, nwamui_daemon_get_status_icon_type(daemon), NOTIFY_ICON_SIZE );
    }

    {
        gchar *name = nwamui_object_get_name(NWAMUI_OBJECT(env));
        summary_str = g_strdup_printf(_("Switch to Location '%s'"), name);
        g_free(name);
    }

    nwam_notification_show_message(summary_str,
            "",
            icon,
            NOTIFY_EXPIRES_DEFAULT);

    g_free(summary_str);

    if ( daemon != NULL ) {
        g_object_unref(G_OBJECT(daemon));
    }
}

void
nwam_notification_cleanup( void )
{
    notification_cleanup();
}

void
nwam_notification_init( GtkStatusIcon* status_icon )
{
    g_assert ( status_icon != NULL );
    
    notify_init( PACKAGE );

    g_object_ref( status_icon );
    parent_status_icon = status_icon;
}

void
nwam_notification_set_default_icon(GdkPixbuf *pixbuf)
{
    if (default_icon)
        g_object_unref(default_icon);

    if ((default_icon = pixbuf) != NULL)
        g_object_ref(default_icon);
}

