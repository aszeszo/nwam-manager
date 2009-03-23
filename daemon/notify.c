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

static NotifyNotification*   notification           = NULL;
static GtkStatusIcon*        parent_status_icon     = NULL;
static gchararray            default_icon           = NWAM_ICON_NETWORK_LOCATION;
static GQueue                msg_q                  = G_QUEUE_INIT;
static gchar                *last_message           = NULL;

static guint adjust_source_id = 0;
static gint gconf_exp_time;
#define DEFAULT_EXP_TIME	gconf_exp_time
#define TIGHT_EXP_TIME	((gint) (0.5 * DEFAULT_EXP_TIME))

#define NOTIFY_EXP_DEFAULT_FLAG	(NOTIFY_EXPIRES_DEFAULT)
#define NOTIFY_EXP_ADJUSTED_FLAG	(NOTIFY_EXP_DEFAULT_FLAG - 1)


typedef struct _msg
{
    NotifyNotification *n;
    gchar *summary;
    gchar *body;
    gchar *icon;
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
  const gchar *icon,
  const gchar *action,
  const gchar *label,
  NotifyActionCallback callback,
  gpointer user_data,
  GFreeFunc free_func,
  gint timeout);

static void set_last_message( const gchar* summary, const gchar* body );

static void nwam_notification_msg_free(msg *m);

static gboolean nwam_notification_show_message_cb(GQueue *q);

static void nwam_notification_show_nth_message(GQueue *q, guint nth);

static gboolean nwam_notification_hide_cb(NotifyNotification *n);

static gboolean notify_notification_adjust_first(GQueue *q);

static void notify_notification_adjust_nth(GQueue *q, guint nth, gint new_timeout);

static void on_prof_notification_default_timeout(NwamuiProf* prof,
  gint val,
  gpointer data);

static msg *
nwam_notification_msg_new(NotifyNotification *n,
  const gchar *summary,
  const gchar *body,
  const gchar *icon,
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
    m->icon = g_strdup(icon);
    m->action = g_strdup(action);
    m->label = g_strdup(label);
    m->cb = callback;
    m->user_data = user_data;
    m->free_func = free_func;
    m->timeout = (timeout == NOTIFY_EXPIRES_DEFAULT) ? NOTIFY_EXP_DEFAULT_FLAG : timeout;

    return m;
}

static void
nwam_notification_msg_free(msg *m)
{
    set_last_message(m->summary, m->body);
    g_free(m->summary);
    g_free(m->body);
    g_free(m->icon);
    g_free(m->action);
    g_free(m->label);
    g_free(m);
}

static void on_prof_notification_default_timeout(NwamuiProf* prof,
  gint val,
  gpointer data)
{
    msg *m;
    gint timeout;

    gconf_exp_time = val;
    timeout = g_queue_get_length(&msg_q) == 1 ?
      DEFAULT_EXP_TIME : TIGHT_EXP_TIME;

    g_debug("#### Notification default timeout %d ####\n", DEFAULT_EXP_TIME);

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
    
    g_assert ( parent_status_icon != NULL );

    if ( notification == NULL ) {
        notify_init( PACKAGE );
        
        notification = notify_notification_new_with_status_icon(" ", " ", default_icon, parent_status_icon  );

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

/*             g_debug("#### Notification default timeout %d ####\n", DEFAULT_EXP_TIME); */

            g_signal_connect(prof, "notification_default_timeout",
              G_CALLBACK(on_prof_notification_default_timeout), NULL);

            nwamui_prof_notify_begin (prof);

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

    if ((m = g_queue_peek_nth(q, nth)) == NULL) {
        return;
    }

    if (m->timeout == NOTIFY_EXP_DEFAULT_FLAG) {
        m->timeout = NOTIFY_EXP_ADJUSTED_FLAG;
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
    if ((m = g_queue_peek_head(q)) == NULL) {
        adjust_source_id = 0;
        return FALSE;
    }

    /* reset source id since the timer should always be deleted/updated */
    adjust_source_id = 0;

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

/*         g_debug("#### %s adjust %d ####\n", __func__, timeout > 0 ? timeout : 0); */

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

        notify_notification_update(m->n, m->summary, m->body,
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

void
nwam_notification_show_message(const gchar* summary,
  const gchar* body,
  const gchar* icon,
  gint timeout)
{
    NotifyNotification  *n = get_notification();
    msg                 *m;
    
    g_assert (summary != NULL && *summary != '\0'); /* Must have a value! */
    
    m = nwam_notification_msg_new(n, summary, body, icon, NULL, NULL, NULL, NULL, NULL, timeout);

    g_queue_push_tail(&msg_q, m);

    if (g_queue_get_length(&msg_q) == 1) {
        /* first show */
        nwam_notification_show_message_cb(&msg_q);
    } else {
        /* adjust the original last show */
        notify_notification_adjust_nth(&msg_q,
          g_queue_get_length(&msg_q) - 2,
          TIGHT_EXP_TIME);
    }
}

void
nwam_notification_show_message_with_action (const gchar* summary,
  const gchar* body,
  const gchar* icon,
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

    if (g_queue_get_length(&msg_q) == 1) {
        /* first show */
        nwam_notification_show_message_cb(&msg_q);
    } else {
        /* adjust the original last show */
        notify_notification_adjust_nth(&msg_q,
          g_queue_get_length(&msg_q) - 2,
          TIGHT_EXP_TIME);
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
    
    g_object_ref( status_icon );
    parent_status_icon = status_icon;
}

const gchar*
nwam_notification_get_status_msg(void) 
{
    return( last_message );
}

static void
set_last_message( const gchar* summary, const gchar* body )
{

    if ( last_message != NULL ) {
        g_free(last_message);
        last_message = NULL;
    }

    if ( body != NULL ) {
        last_message = g_strdup_printf(_("  %s\n    %s"), summary, body );
    }
    else {
        last_message = g_strdup_printf(_("  %s"), summary );
    }
}
