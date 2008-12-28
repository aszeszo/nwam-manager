/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set expandtab ts=4 shiftwidth=4: */
/* 
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
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
#include <gtk/gtkstatusicon.h>
#include <gconf/gconf-client.h>
#include <glib/gi18n.h>
#include <strings.h>

#include "nwam-menu.h"
#include "nwam-menuitem.h"
#include "status_icon.h"
#include "libnwamui.h"
#include "notify.h"

/* This might seem a bit overkill, but once we start handling more than 1 icon it should make it 
 * easier to do. Currently the index parameter is ignored...
 */

static GtkStatusIcon*           status_icon                     = NULL; /* TODO - Allow for more than 1 icon. */
static StatusIconCallback       status_icon_activate_callback    = NULL;
static GObject                 *status_icon_activate_callback_user_data    = NULL;
static gchar                   *status_icon_reason_message = NULL;
static gchar                   *tooltip_message = NULL;


struct _NwamStatusIconPrivate {
    NwamMenu *menu;
    gint icon_stock_index;
    gint animation_icon_update_timeout_id;

	NwamuiDaemon *daemon;
    gint current_status;
};

static void nwam_status_icon_finalize (NwamStatusIcon *self);
static void nwam_status_icon_create_static_status_iconitems (NwamStatusIcon *self);
static void nwam_status_icon_group_recreate(NwamStatusIcon *self, int group_id,
  const gchar *group_name);

static GtkStatusIcon*   get_status_icon( gint index );
static void             nwam_status_icon_refresh_tooltip(NwamStatusIcon *self);

static gboolean animation_panel_icon_timeout (gpointer user_data);
static void trigger_animation_panel_icon (GConfClient *client,
                                          guint cnxn_id,
                                          GConfEntry *entry,
                                          gpointer user_data);

static void connect_nwam_obj_signals(GObject *self, GObject *obj);
static void disconnect_nwam_obj_signals(GObject *self, GObject *obj);

static void on_ncp_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer user_data);
static void status_icon_popup_cb(GtkStatusIcon *status_icon,
  guint button,
  guint activate_time,
  gpointer user_data);

/* nwamui daemon events */
static void event_daemon_status_changed(NwamuiDaemon *daemon, nwamui_daemon_status_t status, gpointer user_data);

static void event_daemon_info(NwamuiDaemon *daemon, gint type, GObject *obj, gpointer data, gpointer user_data);

static void event_wifi_key_needed (NwamuiDaemon* daemon, NwamuiWifiNet* wifi, gpointer user_data);

static void event_wifi_selection_needed (NwamuiDaemon* daemon, NwamuiNcu* ncu, gpointer user_data);

static void event_ncu_up (NwamuiDaemon* daemon, NwamuiNcu* ncu, gpointer user_data);

static void event_ncu_down (NwamuiDaemon* daemon, NwamuiNcu* ncu, gpointer user_data);

/* nwamui ncp signals */
static void ncp_activate_ncu (NwamuiNcp *ncp, NwamuiNcu* ncu, gpointer user_data);
static void ncp_deactivate_ncu (NwamuiNcp *ncp, NwamuiNcu* ncu, gpointer user_data);

/* notification event */
static void on_notifyaction_wifi_key_need (NotifyNotification *n,
  gchar *action,
  gpointer user_data);

static void on_notifyaction_popup_menus(NotifyNotification *n,
  gchar *action,
  gpointer user_data);


G_DEFINE_TYPE (NwamStatusIcon, nwam_status_icon, GTK_TYPE_STATUS_ICON)

static void
on_status_activate_wifi_key_needed( GObject* object )
{
    join_wireless(NWAMUI_WIFI_NET(object));
}

static gboolean
ncu_is_higher_priority_than_active_ncu( NwamuiNcu* ncu, gboolean *is_active_ptr )
{
    NwamuiDaemon*  daemon  = nwamui_daemon_get_instance();
    NwamuiNcp*     active_ncp  = nwamui_daemon_get_active_ncp( daemon );
    NwamuiNcu*     active_ncu  = NULL;
    /* gint           ncu_prio    = nwamui_ncu_get_priority(ncu); */
    gint           active_ncu_prio = -1;
    gboolean       is_active_ncu = FALSE;
    gboolean       retval = FALSE;

    g_object_unref(daemon);
    
    return (TRUE);

    /* TODO: How to get the acitve ncu and priorities */
#if 0 
    if ( active_ncp != NULL && NWAMUI_NCP( active_ncp ) ) {
        active_ncu = nwamui_ncp_get_active_ncu( active_ncp );
        g_object_unref(active_ncp);
    }

    if ( active_ncu != NULL ) {
        active_ncu_prio = nwamui_ncu_get_priority(active_ncu);
        /* Return TRUE only if it's the same or a higher
         * priority ncu that the active ncu 
         * (lower value = higher prio)
         */
        is_active_ncu = (active_ncu == ncu);
        retval = (is_active_ncu || ncu_prio < active_ncu_prio);
        
        g_object_unref(active_ncu);
    }
    else {
        /* If there is no active_ncu, return TRUE */
        retval = TRUE;
    }

    if ( is_active_ptr != NULL ) {
        *is_active_ptr = is_active_ncu;
    }

    return( retval );
#endif /* 0 */
}

static void
event_daemon_status_changed(NwamuiDaemon *daemon, nwamui_daemon_status_t status, gpointer user_data)
{
    NwamStatusIcon *self = NWAM_STATUS_ICON(user_data);
    const gchar* status_str;
    const gchar *body_str;
    static gboolean need_report_daemon_error = FALSE;

	/* should repopulate data here */

    switch( status) {
    case NWAMUI_DAEMON_STATUS_ACTIVE:
        gtk_status_icon_set_visible(GTK_STATUS_ICON(self), TRUE);

		nwam_status_icon_set_status(self, NWAMUI_ENV_STATUS_CONNECTED, NULL );

        if (need_report_daemon_error) {
            /* CFB Think this notification message is probably unhelpful
               in practice, commenting out for now... */
            /*status_str=_("Automatic network detection active");
            nwam_notification_show_message(status_str, NULL, NWAM_ICON_EARTH_CONNECTED);
            */
            need_report_daemon_error = FALSE;

            nwam_status_icon_refresh_tooltip(self);

        }
        break;
    case NWAMUI_DAEMON_STATUS_INACTIVE:
        if (gtk_status_icon_get_visible(GTK_STATUS_ICON(self))) {
            /* On intial run with NWAM disabled the icon won't be visible */
            nwam_status_icon_set_status(self, NWAMUI_ENV_STATUS_ERROR, 
                                        _("Automatic network configuration daemon is unavailable.") );
		
            status_str=_("Automatic network configuration daemon is unavailable.");
            body_str=  _("For further information please run\n\"svcs -xv nwam\" in a terminal.");
            nwam_notification_show_message(status_str, body_str, NWAM_ICON_EARTH_ERROR, NOTIFY_EXPIRES_DEFAULT);
            nwam_status_icon_refresh_tooltip(self);
            need_report_daemon_error = TRUE;
        }
        break;
    default:
        g_assert_not_reached();
        break;
    }
}

static void
event_daemon_info(NwamuiDaemon *daemon, gint type, GObject *obj, gpointer data, gpointer user_data)
{
    NwamStatusIcon *self = NWAM_STATUS_ICON(user_data);
    gchar *msg = NULL;
    switch (type) {
    case NWAMUI_DAEMON_INFO_WLAN_CONNECTED: {
        if ( NWAMUI_IS_WIFI_NET( obj ) ) {
            gchar *essid = nwamui_wifi_net_get_display_string(NWAMUI_WIFI_NET(obj), FALSE);
            msg = g_strdup_printf(_("Connected to wireless network '%s'"), essid, NULL);
            nwam_status_icon_set_status(self, NWAMUI_ENV_STATUS_CONNECTED, msg );
            nwam_notification_show_message (msg, essid, NULL, NOTIFY_EXPIRES_DEFAULT );
            g_free(essid);
            g_free(msg);
            nwam_status_icon_set_activate_callback(self, NULL, NULL, NULL );
        }
    }
        break;
    case NWAMUI_DAEMON_INFO_WLAN_DISCONNECTED: {
            gboolean        show_message = FALSE;
            gboolean        is_active_ncu = FALSE;
            NwamuiWifiNet*  wifi = NULL;

            if ( obj != NULL && NWAMUI_IS_WIFI_NET( obj ) ) {
                wifi = NWAMUI_WIFI_NET(obj);
            }
            if ( wifi != NULL ) {
                NwamuiNcu*  ncu = nwamui_wifi_net_get_ncu( wifi );
                show_message = ncu_is_higher_priority_than_active_ncu( ncu, &is_active_ncu );
                g_object_unref(ncu);
            }

            if ( show_message && nwamui_daemon_get_event_cause(daemon) != NWAMUI_DAEMON_EVENT_CAUSE_NONE ) {
                gchar *essid    = nwamui_wifi_net_get_display_string( wifi , FALSE);
                gchar *summary  = g_strdup_printf (_("Disconnected from Wireless Network '%s'"), essid);
                gchar *body     = g_strdup_printf(_("%s\nClick here to join another wireless network."),
                                         nwamui_daemon_get_event_cause_string(daemon) );
                nwam_notification_show_message_with_action(summary, body,
                  NWAM_ICON_EARTH_WARNING,
                  NULL,	/* action */
                  NULL,	/* label */
                  on_notifyaction_popup_menus,
                  NULL,
                  NULL,
                  NOTIFY_EXPIRES_DEFAULT);

                /* Only change status if it's the active_ncu */
                if ( is_active_ncu ) {
                    nwam_status_icon_set_status(self, NWAMUI_ENV_STATUS_WARNING, summary );
                }

                g_free(summary);
                g_free(body);
                g_free(essid);
            }
        }
        break;
    case NWAMUI_DAEMON_INFO_NCU_SELECTED:
    case NWAMUI_DAEMON_INFO_NCU_UNSELECTED:
        break;
    case NWAMUI_DAEMON_INFO_WLAN_CHANGED: {
            gboolean show_message = FALSE;

            if ( obj != NULL && NWAMUI_IS_NCU(obj) ) {
                NwamuiNcu*  ncu         = NWAMUI_NCU(obj);

                show_message = ncu_is_higher_priority_than_active_ncu( ncu, NULL );
            }

            if ( show_message ) {
                nwam_notification_show_message (_("Automatic Network Configuration Daemon"), 
                                                 (gchar *)data, NULL, NOTIFY_EXPIRES_DEFAULT);
            }
        }
        break;
    case NWAMUI_DAEMON_INFO_WLAN_CONNECT_FAILED: {
            gboolean        show_message = FALSE;
            gboolean        is_active_ncu = FALSE;
            NwamuiWifiNet*  wifi = NULL;

            if ( obj != NULL && NWAMUI_IS_WIFI_NET( obj ) ) {
                wifi = NWAMUI_WIFI_NET(obj);
            }
            if ( wifi != NULL ) {
                NwamuiNcu*  ncu = nwamui_wifi_net_get_ncu( wifi );
                show_message = ncu_is_higher_priority_than_active_ncu( ncu, &is_active_ncu );
                g_object_unref(ncu);
            }

            if ( show_message ) {
                nwam_notification_show_message (_("Automatic Network Configuration Daemon"), 
                                                 (gchar *)data, NULL, NOTIFY_EXPIRES_DEFAULT);
                nwam_status_icon_set_status(self, NWAMUI_ENV_STATUS_WARNING, (gchar*)data );
            }
        }
        break;
    default:
        nwam_notification_show_message (_("Automatic Network Configuration Daemon"), (gchar *)data, NULL, NOTIFY_EXPIRES_DEFAULT);
        break;
    }
}

static void
event_wifi_key_needed (NwamuiDaemon* daemon, NwamuiWifiNet* wifi, gpointer user_data)
{
    NwamStatusIcon *self = NWAM_STATUS_ICON(user_data);
	gchar *summary = NULL;
	gchar *body = NULL;

	if (wifi) {
        NwamuiNcu * ncu = nwamui_wifi_net_get_ncu( wifi );
        gchar *name;
        name = nwamui_wifi_net_get_display_string(wifi, FALSE);
        summary = g_strdup_printf (_("'%s' requires authentication"), name);
        g_free(name);
        body = g_strdup_printf (_("Click here to enter the wireless network key."));

        nwam_status_icon_set_activate_callback(self, body, on_status_activate_wifi_key_needed, G_OBJECT(wifi) );
        nwam_status_icon_refresh_tooltip(self);

        nwam_notification_show_message_with_action(summary, body,
          NWAM_ICON_NETWORK_WIRELESS,
          NULL,	/* action */
          NULL,	/* label */
          on_notifyaction_wifi_key_need,
          (gpointer)g_object_ref(wifi),
          (GFreeFunc)g_object_unref,
          NOTIFY_EXPIRES_NEVER);
		
          g_object_unref(ncu);
    } else {
        g_assert_not_reached();
    }
    g_free(body);
    g_free(summary);
}

static void
event_wifi_selection_needed (NwamuiDaemon* daemon, NwamuiNcu* ncu, gpointer user_data)
{
    NwamStatusIcon *self = NWAM_STATUS_ICON(user_data);
	gchar      *summary = NULL;
	gchar      *body = NULL;
    gboolean    active_ncu = FALSE;

    /* Only show the message when it's relevant */
	if (ncu && ncu_is_higher_priority_than_active_ncu( ncu, &active_ncu )) {
        gchar *name = nwamui_ncu_get_vanity_name (ncu);
        summary = g_strdup_printf (_("Wireless networks available"));
        body = g_strdup_printf (_("Right-click the icon to connect \n"
                                   "%s network interface to an available network."), name);
        g_free(name);

        if ( active_ncu ) {
            nwam_status_icon_set_status(self, NWAMUI_ENV_STATUS_WARNING, 
                                        _("Right-click to select wireless network") );
        }

        nwam_notification_show_message_with_action(summary, body,
          NWAM_ICON_EARTH_WARNING,
          NULL,	/* action */
          NULL,	/* label */
          on_notifyaction_popup_menus,
          (gpointer)self,
          NULL,
          NOTIFY_EXPIRES_DEFAULT);

        g_free(summary);
        g_free(body);
    } 
}

static void
event_ncu_up (NwamuiDaemon* daemon, NwamuiNcu* ncu, gpointer user_data)
{
    NwamStatusIcon *self = NWAM_STATUS_ICON(user_data);
    gchar *sname = NULL;
    gchar *ifname = NULL;
    gchar *addr = NULL;
    gchar *summary = NULL, *body = NULL;

	/* generic info */
    ifname = nwamui_ncu_get_device_name (ncu);
    if (!nwamui_ncu_get_ipv6_active (ncu)) {
        addr = nwamui_ncu_get_ipv4_address (ncu);
    } else {
        addr = nwamui_ncu_get_ipv6_address (ncu);
    }

    nwam_status_icon_refresh_tooltip(self);

    switch (nwamui_ncu_get_ncu_type (ncu)) {
    case NWAMUI_NCU_TYPE_WIRED: {
        GString *gstr = g_string_new("");
        sname = nwamui_ncu_get_vanity_name (ncu);
        summary = g_strdup_printf (_("%s network interface is connected."), sname);
        g_string_append_printf( gstr, _("%s: %s"), _("Network Interface"), ifname );
        if ( addr != NULL && strlen(addr) > 0 ) {
            g_string_append_printf( gstr, _("\n%s: %s"), _("Network Address"), addr );
        }
        body = g_string_free(gstr, FALSE);

        nwam_notification_show_message (summary, body, NWAM_ICON_NETWORK_IDLE, NOTIFY_EXPIRES_DEFAULT);
        break;
    }
    case NWAMUI_NCU_TYPE_WIRELESS: {
        gint signal = -1;
        GString *gstr = g_string_new("");
        NwamuiWifiNet *wifi = nwamui_ncu_get_wifi_info (ncu);
        if ( wifi != NULL ) { /* Can happen on initial run when NWAM already active */
            sname = nwamui_wifi_net_get_display_string(wifi, FALSE);
            summary = g_strdup_printf (_("Connected to wireless network '%s'"), sname);
            signal = (gint) ((nwamui_wifi_net_get_signal_strength (wifi) * 100) / NWAMUI_WIFI_STRENGTH_LAST);
        }
        else {
            sname = nwamui_ncu_get_vanity_name (ncu);
            summary = g_strdup_printf (_("%s network interface is connected."), sname);
        }

        g_string_append_printf( gstr, _("%s: %s"), _("Interface"), ifname );
        if ( addr != NULL && strlen(addr) > 0 ) {
            g_string_append_printf( gstr, _("\n%s: %s"), _("Address"), addr );
        }
        if ( signal != -1 ) {
            g_string_append_printf( gstr, _("\n%s: %d"),  _("Signal Strength"), signal );
        }

        body = g_string_free(gstr, FALSE);

        nwam_notification_show_message (summary, body, NWAM_ICON_NETWORK_WIRELESS, NOTIFY_EXPIRES_DEFAULT);
        if ( wifi ) {
            g_object_unref(wifi);
        }
		break;
    }
    default:
        g_assert_not_reached ();
    }

	/* set status to "CONNECTED" */
	nwam_status_icon_set_status(self, NWAMUI_ENV_STATUS_CONNECTED, summary );

    g_free (sname);
    g_free (ifname);
    g_free (addr);
    g_free (summary);
    g_free (body);
}

static void
event_ncu_down (NwamuiDaemon* daemon, NwamuiNcu* ncu, gpointer user_data)
{
    NwamStatusIcon *self = NWAM_STATUS_ICON(user_data);
    gchar *sname = NULL;
    gchar *summary = NULL;
    NwamuiNcp*  active_ncp = nwamui_daemon_get_active_ncp( daemon );
    NwamuiNcu*  active_ncu = NULL;
    
    if ( active_ncp != NULL && NWAMUI_NCP( active_ncp ) ) {
        active_ncu = nwamui_ncp_get_active_ncu( active_ncp );
        g_object_unref(active_ncp);
    }

    /* Only show this information, if it's the active_ncu */
    if ( ncu == active_ncu ) {
        switch (nwamui_ncu_get_ncu_type (ncu)) {
        case NWAMUI_NCU_TYPE_WIRED: {
                gchar *body = NULL;
                sname = nwamui_ncu_get_vanity_name (ncu);
                summary = g_strdup_printf (_("%s disconnected from network."), sname);
                if ( nwamui_daemon_get_event_cause(daemon) != NWAMUI_DAEMON_EVENT_CAUSE_NONE ) {
                    body = g_strdup_printf("Reason: %s",nwamui_daemon_get_event_cause_string(daemon));
                }
                nwam_notification_show_message (summary, body, NWAM_ICON_EARTH_WARNING, NOTIFY_EXPIRES_DEFAULT);
                g_free(body);
            }
            break;
        case NWAMUI_NCU_TYPE_WIRELESS: {
                gchar *body = NULL;
                sname = nwamui_ncu_get_vanity_name (ncu);
                summary = g_strdup_printf (_("%s disconnected from network."), sname);
                if ( nwamui_daemon_get_event_cause(daemon) != NWAMUI_DAEMON_EVENT_CAUSE_NONE ) {
                    body = g_strdup_printf("Reason: %s",nwamui_daemon_get_event_cause_string(daemon));
                }
                nwam_notification_show_message (summary, body, NWAM_ICON_EARTH_WARNING, NOTIFY_EXPIRES_DEFAULT);
                g_free(body);
            }
            break;
        default:
            g_assert_not_reached ();
        }

        /* set status to "WARNING", only relevant if it's the active ncu */
        nwam_status_icon_set_status(self, NWAMUI_ENV_STATUS_WARNING, summary );
        nwam_status_icon_refresh_tooltip(self);
    }

    if ( active_ncu != NULL ) {
        g_object_unref(active_ncu);
    }

    g_free (sname);
    g_free (summary);
}

static void
connect_nwam_obj_signals(GObject *self, GObject *obj)
{
	GType type = G_OBJECT_TYPE(obj);

	if (type == NWAMUI_TYPE_DAEMON) {
        NwamuiDaemon *daemon = NWAMUI_DAEMON(obj);

        g_signal_connect(daemon, "status_changed",
          G_CALLBACK(event_daemon_status_changed),
          (gpointer)self);

        g_signal_connect(daemon, "daemon_info",
          G_CALLBACK(event_daemon_info),
          (gpointer)self);

        g_signal_connect(daemon, "wifi_key_needed",
          G_CALLBACK(event_wifi_key_needed),
          (gpointer)self);

        g_signal_connect(daemon, "wifi_selection_needed",
          G_CALLBACK(event_wifi_selection_needed),
          (gpointer)self);

        g_signal_connect(daemon, "ncu_up",
          G_CALLBACK(event_ncu_up),
          (gpointer)self);
	
        g_signal_connect(daemon, "ncu_down",
          G_CALLBACK(event_ncu_down),
          (gpointer)self);

    } else if (type == NWAMUI_TYPE_NCP) {
        NwamuiNcp* ncp = NWAMUI_DAEMON(obj);

        g_signal_connect(ncp, "activate_ncu",
          G_CALLBACK(ncp_activate_ncu),
          (gpointer)self);

        g_signal_connect(ncp, "deactivate_ncu",
          G_CALLBACK(ncp_deactivate_ncu),
          (gpointer)self);

        g_signal_connect(ncp, "notify::ncu-list-store",
          G_CALLBACK(on_ncp_notify_cb),
          (gpointer)self);

	} else if (type == NWAMUI_TYPE_NCU) {
	} else if (type == NWAMUI_TYPE_ENV) {
	} else if (type == NWAMUI_TYPE_ENM) {
	} else {
		g_error("%s unknown nwamui obj", __func__);
	}
}

static void
disconnect_nwam_obj_signals(GObject *self, GObject *obj)
{
	GType type = G_OBJECT_TYPE(obj);

	if (type == NWAMUI_TYPE_DAEMON) {
    } else if (type == NWAMUI_TYPE_NCP) {
	} else if (type == NWAMUI_TYPE_NCU) {
	} else if (type == NWAMUI_TYPE_ENV) {
	} else if (type == NWAMUI_TYPE_ENM) {
	} else {
		g_error("%s unknown nwamui obj", __func__);
	}
    g_signal_handlers_disconnect_matched(obj,
      G_SIGNAL_MATCH_DATA,
      0,
      NULL,
      NULL,
      NULL,
      (gpointer)self);
}

static gboolean
animation_panel_icon_timeout (gpointer user_data)
{
    NwamStatusIcon *self = NWAM_STATUS_ICON(user_data);
	gtk_status_icon_set_from_pixbuf(GTK_STATUS_ICON(self),
		nwamui_util_get_env_status_icon(
		(nwamui_env_status_t)(++self->prv->icon_stock_index)%NWAMUI_ENV_STATUS_LAST));
	return TRUE;
}

static void
trigger_animation_panel_icon (GConfClient *client,
                              guint cnxn_id,
                              GConfEntry *entry,
                              gpointer user_data)
{
    NwamStatusIcon *self = NWAM_STATUS_ICON(user_data);
	self->prv->animation_icon_update_timeout_id = 0;
	GConfValue *value = NULL;
	
	g_assert (entry);
	value = gconf_entry_get_value (entry);
	g_assert (value);
	if (gconf_value_get_bool (value)) {
		g_assert (self->prv->animation_icon_update_timeout_id == 0);
		self->prv->animation_icon_update_timeout_id = 
          g_timeout_add (333, animation_panel_icon_timeout, (gpointer)self);
		g_assert (self->prv->animation_icon_update_timeout_id > 0);
	} else {
		g_assert (self->prv->animation_icon_update_timeout_id > 0);
		g_source_remove (self->prv->animation_icon_update_timeout_id);
		self->prv->animation_icon_update_timeout_id = 0;
		/* reset everything of animation_panel_icon here */
		gtk_status_icon_set_from_pixbuf(status_icon, 
          nwamui_util_get_env_status_icon(NWAMUI_ENV_STATUS_CONNECTED));
		self->prv->icon_stock_index = 0;
	}
}

NwamStatusIcon* 
nwam_status_icon_new( void )
{
    NwamStatusIcon* self;
    self = NWAM_STATUS_ICON(g_object_new(NWAM_TYPE_STATUS_ICON, NULL));
    return self;
}

void
nwam_status_icon_set_activate_callback(NwamStatusIcon *self,
                                        const char* message, 
                                        StatusIconCallback activate_cb, 
                                        GObject* user_data )
{
    status_icon_activate_callback = activate_cb;
    if ( status_icon_activate_callback_user_data != NULL ) {
        g_object_unref(status_icon_activate_callback_user_data);
    }
    if ( user_data != NULL ) {
        status_icon_activate_callback_user_data = g_object_ref(user_data);
    }
    else {
        status_icon_activate_callback_user_data = NULL;
    }
}

void
nwam_status_icon_set_status(NwamStatusIcon *self, gint env_status, const gchar* reason )
{
    self->prv->current_status = env_status;

    gtk_status_icon_set_from_pixbuf(GTK_STATUS_ICON(self),
      nwamui_util_get_env_status_icon(env_status));

    if ( status_icon_reason_message != NULL ) {
        g_free( status_icon_reason_message );
    }

    if ( reason != NULL ) {
        status_icon_reason_message = g_strdup( reason );
    }
    else {
        status_icon_reason_message = NULL;
    }

    nwam_status_icon_refresh_tooltip(self);
}

static void
nwam_status_icon_refresh_tooltip(NwamStatusIcon *self)
{
    gchar          *newstr = NULL;

    if ( tooltip_message == NULL )
        return;

    if ( status_icon_reason_message  != NULL ) {
        newstr = g_strdup_printf(_("%s\n%s"), tooltip_message, status_icon_reason_message );
    }
    else {
        newstr = g_strdup_printf(_("%s"), tooltip_message );
    }

	gtk_status_icon_set_tooltip (GTK_STATUS_ICON(self), newstr);

    g_free(newstr);


	GString        *gstr = g_string_new("");
	gchar          *name = NULL;
	gchar          *cstr = NULL;
    const gchar    *status_msg = NULL;
    NwamuiNcp      *ncp = NULL;
    NwamuiNcu      *ncu = NULL;
    NwamuiNcu      *ncu_ref = NULL;

    g_debug("Update tooltip called");
    ncp = nwamui_daemon_get_active_ncp(self->prv->daemon);

    if ( ncu == NULL ) {
        ncu_ref = nwamui_ncp_get_active_ncu(ncp);
    }
    else {
        ncu_ref = g_object_ref(ncu);
    }

    if ( ncu_ref != NULL ) {
        name = nwamui_ncu_get_device_name (ncu_ref);

        if ( nwamui_ncu_get_ncu_type( ncu_ref ) == NWAMUI_NCU_TYPE_WIRELESS ) {
            g_string_append_printf( gstr, _("Wireless (%s): %s"), name);
        }
        else {
            g_string_append_printf( gstr, _("Wired (%s): %s"), name);
        }
        g_free (name);
        g_object_unref(ncu_ref);
    }
    else {
        g_string_append_printf( gstr, _("No network interface is active."), name);
    }

    cstr = g_string_free(gstr, FALSE);
    g_debug("Update tooltip setting tooltip to '%s'", cstr);
    g_free(cstr);

    g_object_unref(ncp);
}

void
nwam_status_icon_show_menu( gint index, guint button, guint activate_time ) 
{
	GtkStatusIcon* s_icon = get_status_icon( index );
    status_icon_popup_cb(s_icon,
      button,
      activate_time,
      NULL);
}

static void
status_icon_popup_cb(GtkStatusIcon *status_icon,
  guint button,
  guint activate_time,
  gpointer user_data)
{
    NwamStatusIcon *self = NWAM_STATUS_ICON(status_icon);
	GtkWidget*     s_menu = nwam_menu_get_statusicon_menu (self->prv->menu);

	/* TODO - handle dynamic menu, for now let's do nothing, if there's no menu. */
	if ( s_menu != NULL ) {
        NwamuiDaemon *daemon = nwamui_daemon_get_instance();
        /* Trigger a re-scan of the networks while we're at it */
        nwamui_daemon_wifi_start_scan(daemon);

		gtk_menu_popup(GTK_MENU(s_menu),
          NULL,
          NULL,
          gtk_status_icon_position_menu,
          NULL,
          button,
          activate_time);
		gtk_widget_show_all(s_menu);
	}
}

static void
status_icon_activate_cb(GtkStatusIcon *status_icon, GObject* user_data)
{
    NwamStatusIcon *self = NWAM_STATUS_ICON(status_icon);

    if ( status_icon_activate_callback != NULL ) {
        (*status_icon_activate_callback)( status_icon_activate_callback_user_data );
    }
    else {
        NwamuiDaemon *daemon = nwamui_daemon_get_instance();
        /* Trigger a re-scan of the networks while we're at it */
        nwamui_daemon_wifi_start_scan(daemon);
    }
}

static gboolean
status_icon_size_changed_cb(GtkStatusIcon *status_icon, gint size, gpointer user_data)
{
    NwamStatusIcon *self = NWAM_STATUS_ICON(status_icon);

    gtk_status_icon_set_from_pixbuf(status_icon, 
      nwamui_util_get_env_status_icon(self->prv->current_status ));
    return TRUE;
}

static void
ncp_activate_ncu (NwamuiNcp *ncp, NwamuiNcu* ncu, gpointer user_data)
{
    NwamStatusIcon *sicon = NWAM_STATUS_ICON(user_data);

    if ( ncu != NULL ) { /* Can be NULL if nothing is active, e.g. NWAM brought down */
        gchar *summary = NULL;
        gchar *body = NULL;
        gchar *name;

        name = nwamui_ncu_get_vanity_name(ncu);
        summary = g_strdup_printf(_("Activating %s network interface..."), name);
        nwam_notification_show_message(summary, NULL, NWAM_ICON_EARTH_WARNING, NOTIFY_EXPIRES_DEFAULT);
        nwam_status_icon_set_status(sicon, NWAMUI_ENV_STATUS_WARNING, NULL );

        g_free(name);
        g_free(summary);
        g_free(body);
    }
}

static void
ncp_deactivate_ncu (NwamuiNcp *ncp, NwamuiNcu* ncu, gpointer user_data)
{
    NwamStatusIcon *sicon = NWAM_STATUS_ICON(user_data);
    NwamuiDaemon *daemon = nwamui_daemon_get_instance ();

    if (nwamui_ncu_get_ncu_type(ncu) == NWAMUI_NCU_TYPE_WIRELESS) {
        NwamuiWifiNet* wifi;

        wifi = nwamui_ncu_get_wifi_info(ncu);

        if (wifi) {
            switch (nwamui_wifi_net_get_security(wifi)) {
            case NWAMUI_WIFI_SEC_WEP_HEX:
            case NWAMUI_WIFI_SEC_WEP_ASCII: {
                gchar *name;
                name = nwamui_wifi_net_get_display_string(wifi, FALSE);

                switch (nwamui_daemon_get_event_cause(daemon)) {
                case NWAMUI_DAEMON_EVENT_CAUSE_DHCP_DOWN:
                case NWAMUI_DAEMON_EVENT_CAUSE_DHCP_TIMER: {
                    gchar *summary = NULL;
                    gchar *body = NULL;

                    summary = g_strdup_printf(_("Failed to connect to wireless network '%s'. %s"), name,
                                                 nwamui_daemon_get_event_cause_string(daemon) );
                    body = g_strdup(_("The DHCP server may not be responding, or\n"
                                      "you may have entered the wrong wireless key.\n\n"
                                      "Click here to re-enter the wireless key."));

                    nwam_status_icon_set_activate_callback( sicon, body,
                                                            on_status_activate_wifi_key_needed, G_OBJECT(wifi) );

                    nwam_notification_show_message_with_action(summary, body,
                      NWAM_ICON_NETWORK_WIRELESS,
                      NULL,	/* action */
                      NULL,	/* label */
                      on_notifyaction_wifi_key_need,
                      (gpointer)g_object_ref(wifi),
                      (GFreeFunc)g_object_unref,
                      NOTIFY_EXPIRES_DEFAULT);

                    g_free(summary);
                    g_free(body);
                }
                    break;
                default:
                    break;
                }
                g_free(name);
            }
                break;
            default:
                break;
            }
            g_object_unref(wifi);
        }
    }
}

static void
on_ncp_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer user_data)
{
    NwamStatusIcon *sicon = NWAM_STATUS_ICON(user_data);

    g_assert(NWAMUI_IS_NCP(gobject));

    if (g_ascii_strcasecmp(arg1->name, "ncu-list-store") == 0) {
/*         update_tooltip( NULL, status_icon_index, NULL ); */
    }
}

static void
on_notifyaction_wifi_key_need (NotifyNotification *n,
  gchar *action,
  gpointer user_data)
{
    join_wireless(user_data ? NWAMUI_WIFI_NET(user_data) : NULL);
}

static void
on_notifyaction_popup_menus(NotifyNotification *n,
  gchar *action,
  gpointer user_data)
{
    NwamuiDaemon *daemon = nwamui_daemon_get_instance();
    /* Trigger a re-scan of the networks while we're at it */
    nwamui_daemon_wifi_start_scan(daemon);
    g_object_unref(daemon);

    nwam_status_icon_show_menu((gint)user_data, 0, gtk_get_current_event_time());
}

static GtkStatusIcon*
get_status_icon( gint index )
{ 
    if ( status_icon == NULL ) {  

        status_icon = GTK_STATUS_ICON(g_object_new(NWAM_TYPE_STATUS_ICON, NULL));

    }
    g_assert( status_icon != NULL );

    return (status_icon);
}

static void
nwam_status_icon_class_init (NwamStatusIconClass *klass)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->finalize = (void (*)(GObject*)) nwam_status_icon_finalize;
}

static void
nwam_status_icon_init (NwamStatusIcon *self)
{
	GError *error = NULL;

	self->prv = g_new0 (NwamStatusIconPrivate, 1);
	
	/* ref daemon instance */
	self->prv->daemon = nwamui_daemon_get_instance ();
    self->prv->current_status = NWAMUI_ENV_STATUS_CONNECTED;
    self->prv->menu = nwam_menu_new();

    /* TODO - For Phase 1 */
    /* nwam_status_icon_set_activate_callback(status_icon_index, G_CALLBACK(activate_cb)); */

    /* handle all daemon signals here */
    connect_nwam_obj_signals(G_OBJECT(self), G_OBJECT(self->prv->daemon));
    /* nwamui preference signals */
#if PHASE_1_0
    {
        NwamuiNcp* ncp = nwamui_daemon_get_active_ncp(self->prv->daemon);
        connect_nwam_obj_signals(G_OBJECT(self), G_OBJECT(ncp));
        g_object_unref(ncp);
    }
#endif
    /* Connect signals */
    g_signal_connect (G_OBJECT (self), "popup-menu",
      G_CALLBACK (status_icon_popup_cb), NULL);

    g_signal_connect (G_OBJECT (self), "activate",
      G_CALLBACK (status_icon_activate_cb), NULL);

    g_signal_connect (G_OBJECT (self), "size-changed",
      G_CALLBACK (status_icon_size_changed_cb), NULL);

    /* initially populate network info */
    event_daemon_status_changed(self->prv->daemon,
      nwamui_daemon_get_status(self->prv->daemon),
      (gpointer)self);
}

static void
nwam_status_icon_finalize (NwamStatusIcon *self)
{
    if (self->prv->animation_icon_update_timeout_id > 0) {
		g_source_remove (self->prv->animation_icon_update_timeout_id);
		self->prv->animation_icon_update_timeout_id = 0;
/* 		/\* reset everything of animation_panel_icon here *\/ */
/* 		gtk_status_icon_set_from_pixbuf(status_icon,  */
/*           nwamui_util_get_env_status_icon(NWAMUI_ENV_STATUS_CONNECTED)); */
/* 		self->prv->icon_stock_index = 0; */
    }

    g_object_unref (G_OBJECT(self->prv->menu));

    disconnect_nwam_obj_signals(G_OBJECT(self), G_OBJECT(self->prv->daemon));
	g_object_unref (G_OBJECT(self->prv->daemon));

	/* TODO, release all the resources */

	g_free(self->prv);
	self->prv = NULL;
	
	G_OBJECT_CLASS(nwam_status_icon_parent_class)->finalize(G_OBJECT(self));
}
