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
#include <gtk/gtkstatusicon.h>
#include <gconf/gconf-client.h>
#include <glib/gi18n.h>
#include <stdlib.h>
#include <strings.h>

#include "libnwamui.h"
#include "nwam-menu.h"
#include "status_icon.h"
#include "status_icon_tooltip.h"
#include "notify.h"

#include "nwam-menuitem.h"
#include "nwam-wifi-item.h"
#include "nwam-enm-item.h"
#include "nwam-env-item.h"
#include "nwam-ncu-item.h"

extern gboolean debug;

typedef struct _NwamStatusIconPrivate NwamStatusIconPrivate;
#define NWAM_STATUS_ICON_GET_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), NWAM_TYPE_STATUS_ICON, NwamStatusIconPrivate))

#define DEBUG()	g_debug ("[[ %20s : %-4d ]]", __func__, __LINE__)
#define NWAM_MANAGER_PROPERTIES "nwam-manager-properties"
#define STATIC_MENUITEM_ID "static_item_id"

#define	NONCU	"No network interfaces detected"

static glong                    update_wifi_timer_interval = 5*1000;

enum {
	SECTION_GLOBAL = 0,         /* Not really used */
	SECTION_WIFI,
	SECTION_LOC,
	SECTION_ENM,
	SECTION_NCU,
	N_SECTION
};

enum {
    MENUITEM_ENV_PREF,
    MENUITEM_CONN_PROF,
    MENUITEM_REFRESH_WLAN,
    MENUITEM_JOIN_WLAN,
    MENUITEM_VPN_PREF,
    MENUITEM_NET_PREF,
    MENUITEM_HELP,
    MENUITEM_TEST,
    /* Others */
    MENUITEM_NONCU,
    N_STATIC_MENUITEMS
};

struct _NwamStatusIconPrivate {
    GtkWidget *menu;
	gboolean enable_pop_up_menu;
    GtkWidget *tooltip_widget;
    GtkWidget *static_menuitems[N_STATIC_MENUITEMS];
	NwamuiDaemon *daemon;
    NwamuiNcp *active_ncp;
    gint current_status;

    gint icon_stock_index;
    guint animation_icon_update_timeout_id;

    guint update_wifi_timer_id;
    gulong activate_handler_id;

	gboolean has_wifi;
/* 	gboolean force_wifi_rescan_due_to_env_changed; */
	gboolean force_wifi_rescan;

};

static void nwam_status_icon_finalize (NwamStatusIcon *self);

static void nwam_menu_create_static_menuitems(NwamStatusIcon *self);

static GtkWidget* nwam_menu_item_create(NwamMenu *self, NwamuiObject *object);
static void nwam_menu_item_delete(NwamMenu *self, NwamuiObject *object);
static void nwam_menu_get_section_index(NwamMenu *self, GtkWidget *child, gint *index, gpointer user_data);

static void nwam_menu_start_update_wifi_timer(NwamStatusIcon *self);
static void nwam_menu_stop_update_wifi_timer(NwamStatusIcon *self);

static void nwam_menu_recreate_wifi_menuitems (NwamStatusIcon *self);
static void nwam_menu_recreate_ncu_menuitems (NwamStatusIcon *self);
static void nwam_menu_recreate_env_menuitems (NwamStatusIcon *self);
static void nwam_menu_recreate_enm_menuitems (NwamStatusIcon *self);

static gboolean animation_panel_icon_timeout (gpointer user_data);
static void trigger_animation_panel_icon (GConfClient *client,
                                          guint cnxn_id,
                                          GConfEntry *entry,
                                          gpointer user_data);

static void connect_nwam_object_signals(GObject *self, GObject *obj);
static void disconnect_nwam_object_signals(GObject *self, GObject *obj);

/* nwamui utilies */
static void join_wireless(NwamuiWifiNet *wifi);

/* call back */
static void on_activate_static_menuitems (GtkMenuItem *menuitem, gpointer user_data);
static void status_icon_wifi_key_needed(GtkStatusIcon *status_icon, GObject* object);

/* nwamui daemon events */
static void daemon_status_changed(NwamuiDaemon *daemon, nwamui_daemon_status_t status, gpointer user_data);
static void daemon_info(NwamuiDaemon *daemon, gint type, GObject *obj, gpointer data, gpointer user_data);
static void daemon_wifi_key_needed (NwamuiDaemon* daemon, NwamuiWifiNet* wifi, gpointer user_data);
static void daemon_wifi_selection_needed (NwamuiDaemon* daemon, NwamuiNcu* ncu, gpointer user_data);
static void daemon_ncu_up (NwamuiDaemon* daemon, NwamuiNcu* ncu, gpointer user_data);
static void daemon_ncu_down (NwamuiDaemon* daemon, NwamuiNcu* ncu, gpointer user_data);
static void nwam_menu_create_wifi_menuitems (GObject *daemon, GObject *wifi, gpointer data);
static void daemon_add_wifi_fav(NwamuiDaemon* daemon, NwamuiWifiNet* wifi, gpointer data);
static void daemon_remove_wifi_fav(NwamuiDaemon* daemon, NwamuiWifiNet* wifi, gpointer data);
static void daemon_active_ncp_changed(NwamuiDaemon* daemon, NwamuiNcp* ncp, gpointer data);
static void daemon_active_env_changed(NwamuiDaemon* daemon, NwamuiEnv* env, gpointer data);

/* nwamui ncp signals */
static void ncp_activate_ncu (NwamuiNcp *ncp, NwamuiNcu* ncu, gpointer user_data);
static void ncp_deactivate_ncu (NwamuiNcp *ncp, NwamuiNcu* ncu, gpointer user_data);
static void ncp_add_ncu(NwamuiNcp *ncp, NwamuiNcu* ncu, gpointer data);
static void ncp_remove_ncu(NwamuiNcp *ncp, NwamuiNcu* ncu, gpointer data);
static void on_ncp_notify( GObject *gobject, GParamSpec *arg1, gpointer user_data);
static void on_ncp_notify_many_wireless( GObject *gobject, GParamSpec *arg1, gpointer user_data);

/* GtkStatusIcon callbacks */
static void status_icon_popup(GtkStatusIcon *status_icon,
  guint button,
  guint activate_time,
  gpointer user_data);
static gboolean status_icon_size_changed(GtkStatusIcon *status_icon, gint size, gpointer user_data);
static gboolean status_icon_query_tooltip(GtkStatusIcon *status_icon,
  gint           x,
  gint           y,
  gboolean       keyboard_mode,
  GtkTooltip    *tooltip,
  gpointer       user_data);

/* notification event */
static void notifyaction_wifi_key_need(NotifyNotification *n,
  gchar *action,
  gpointer user_data);

static void notifyaction_popup_menus(NotifyNotification *n,
  gchar *action,
  gpointer user_data);

static void notification_listwireless(NotifyNotification *n,
  gchar *action,
  gpointer data);


G_DEFINE_TYPE (NwamStatusIcon, nwam_status_icon, GTK_TYPE_STATUS_ICON)

static void
status_icon_wifi_key_needed(GtkStatusIcon *status_icon, GObject* object)
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
daemon_status_changed(NwamuiDaemon *daemon, nwamui_daemon_status_t status, gpointer user_data)
{
    NwamStatusIcon *self = NWAM_STATUS_ICON(user_data);
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);
    const gchar* status_str;
    const gchar *body_str;
    static gboolean need_report_daemon_error = FALSE;

	/* should repopulate data here */

    switch( status) {
    case NWAMUI_DAEMON_STATUS_ACTIVE: {
        nwam_status_icon_set_status(self, NWAMUI_ENV_STATUS_WARNING, NULL);
        gtk_status_icon_set_visible(GTK_STATUS_ICON(self), TRUE);
        prv->enable_pop_up_menu = TRUE;

        if (need_report_daemon_error) {
            /* CFB Think this notification message is probably unhelpful
               in practice, commenting out for now... */
            /*status_str=_("Automatic network detection active");
            nwam_notification_show_message(status_str, NULL, NWAM_ICON_WIRED_CONNECTED);
            */
            need_report_daemon_error = FALSE;

        }

        {
            NwamuiNcp *ncp = nwamui_daemon_get_active_ncp(prv->daemon);
            NwamuiEnv *env = nwamui_daemon_get_active_env(prv->daemon);
            /* Hide menus for performance. */
            gtk_widget_hide(GTK_WIDGET(prv->menu));
            /* We don't need rescan wlans, because when daemon changes to active
             * it will emulate wlan changed signal
             */
            /* Initialize Ncp. */
            nwam_menu_section_delete(NWAM_MENU(prv->menu), SECTION_NCU);
            daemon_active_ncp_changed(prv->daemon, ncp, user_data);
            /* Initialize Env. */
            nwam_menu_section_delete(NWAM_MENU(prv->menu), SECTION_LOC);
            daemon_active_env_changed(prv->daemon, env, user_data);

            /* I don't believe we should actively create menu for wifi, env and
             * enm, we probably could wait daemon populating.
             */
            /* daemon_active_env_changed will trigger this func
            nwam_menu_recreate_wifi_menuitems(self);
            */
            /* Initialize Enm. */
            nwam_menu_section_delete(NWAM_MENU(prv->menu), SECTION_ENM);
            nwam_menu_recreate_enm_menuitems(self);
            /* Show menus. */
            gtk_widget_show(GTK_WIDGET(prv->menu));
        }
    }
        break;
    case NWAMUI_DAEMON_STATUS_INACTIVE:
        prv->enable_pop_up_menu = FALSE;

        if (debug) {
            gtk_status_icon_set_visible(GTK_STATUS_ICON(self), TRUE);
            prv->enable_pop_up_menu = TRUE;
        }

        if (gtk_status_icon_get_visible(GTK_STATUS_ICON(self))) {

            /* On intial run with NWAM disabled the icon won't be visible */
            nwam_status_icon_set_status(self, NWAMUI_ENV_STATUS_ERROR, NULL);
		
            status_str=_("Automatic network configuration daemon is unavailable.");
            body_str=  _("For further information please run\n\"svcs -xv nwam\" in a terminal.");
            nwam_notification_show_message(status_str, body_str, NULL, NOTIFY_EXPIRES_DEFAULT);
            need_report_daemon_error = TRUE;

        }
        
        {
            /* Hide the whole menus instead of deleting menuitems. */
/*             nwam_menu_section_delete(NWAM_MENU(prv->menu), SECTION_ENM); */
/*             nwam_menu_section_delete(NWAM_MENU(prv->menu), SECTION_LOC); */
/*             nwam_menu_section_delete(NWAM_MENU(prv->menu), SECTION_WIFI); */
/*             nwam_menu_section_delete(NWAM_MENU(prv->menu), SECTION_NCU); */

            /* Clean Ncp. */
            daemon_active_ncp_changed(prv->daemon, NULL, user_data);
            /* Clean Env. */
            daemon_active_env_changed(prv->daemon, NULL, user_data);
        }

        break;
    default:
        g_assert_not_reached();
        break;
    }
}

static void
daemon_info(NwamuiDaemon *daemon, gint type, GObject *obj, gpointer data, gpointer user_data)
{
    NwamStatusIcon *self = NWAM_STATUS_ICON(user_data);
    gchar *msg = NULL;
    switch (type) {
    case NWAMUI_DAEMON_INFO_WLAN_CONNECTED: {
        if ( NWAMUI_IS_WIFI_NET( obj ) ) {
            NwamuiNcu*  ncu;

            gchar *essid = nwamui_wifi_net_get_display_string(NWAMUI_WIFI_NET(obj), FALSE);
            msg = g_strdup_printf(_("Connected to wireless network '%s'"), essid, NULL);
            ncu = nwamui_wifi_net_get_ncu( NWAMUI_WIFI_NET(ncu) );
            nwam_status_icon_set_status(self, NWAMUI_ENV_STATUS_CONNECTED, ncu );
            if ( ncu ) {
                g_object_unref(ncu);
            }
            nwam_notification_show_message (msg, essid, NULL, NOTIFY_EXPIRES_DEFAULT );
            g_free(essid);
            g_free(msg);
            nwam_status_icon_set_activate_callback(self, NULL, NULL );
        }
    }
        break;
    case NWAMUI_DAEMON_INFO_WLAN_DISCONNECTED: {
            gboolean        show_message = FALSE;
            gboolean        is_active_ncu = FALSE;
            NwamuiWifiNet*  wifi = NULL;
            NwamuiNcu      *ncu = NULL;

            if ( obj != NULL && NWAMUI_IS_WIFI_NET( obj ) ) {
                wifi = NWAMUI_WIFI_NET(obj);
            }
            if ( wifi != NULL ) {
                NwamuiNcu*  ncu = nwamui_wifi_net_get_ncu( wifi );
                show_message = ncu_is_higher_priority_than_active_ncu( ncu, &is_active_ncu );
            }

            if ( show_message && nwamui_daemon_get_event_cause(daemon) != NWAMUI_DAEMON_EVENT_CAUSE_NONE ) {
                gchar *essid    = nwamui_wifi_net_get_display_string( wifi , FALSE);
                gchar *summary  = g_strdup_printf (_("Disconnected from Wireless Network '%s'"), essid);
                gchar *body     = g_strdup_printf(_("%s\nClick here to join another wireless network."),
                                         nwamui_daemon_get_event_cause_string(daemon) );
                /* Only change status if it's the active_ncu */
                if ( is_active_ncu ) {
                    nwam_status_icon_set_status(self, NWAMUI_ENV_STATUS_WARNING, ncu );
                }

                nwam_notification_show_message_with_action(summary, body,
                  NULL,
                  NULL,	/* action */
                  NULL,	/* label */
                  notifyaction_popup_menus,
                  (gpointer)self,
                  NULL,
                  NOTIFY_EXPIRES_DEFAULT);

                g_free(summary);
                g_free(body);
                g_free(essid);
            }
            if (ncu) {
                g_object_unref(ncu);
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
            NwamuiNcu*      ncu = NULL;

            if ( obj != NULL && NWAMUI_IS_WIFI_NET( obj ) ) {
                wifi = NWAMUI_WIFI_NET(obj);
            }
            if ( wifi != NULL ) {
                NwamuiNcu*  ncu = nwamui_wifi_net_get_ncu( wifi );
                show_message = ncu_is_higher_priority_than_active_ncu( ncu, &is_active_ncu );
            }

            if ( show_message ) {
                nwam_status_icon_set_status(self, NWAMUI_ENV_STATUS_WARNING, ncu );
                nwam_notification_show_message (_("Automatic Network Configuration Daemon"), 
                                                 (gchar *)data, NULL, NOTIFY_EXPIRES_DEFAULT);
            }
            if (ncu) {
                g_object_unref(ncu);
            }
        }
        break;
    default:
        nwam_notification_show_message (_("Automatic Network Configuration Daemon"), (gchar *)data, NULL, NOTIFY_EXPIRES_DEFAULT);
        break;
    }
}

static void
daemon_wifi_key_needed (NwamuiDaemon* daemon, NwamuiWifiNet* wifi, gpointer user_data)
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

        nwam_status_icon_set_activate_callback(self,
          (GCallback)status_icon_wifi_key_needed, G_OBJECT(wifi) );

        nwam_notification_show_message_with_action(summary, body,
          NULL,
          NULL,	/* action */
          NULL,	/* label */
          notifyaction_wifi_key_need,
          (gpointer)g_object_ref(wifi),
          (GFreeFunc)g_object_unref,
          NOTIFY_EXPIRES_NEVER);
		
          if (ncu) {
              g_object_unref(ncu);
          }
    } else {
        g_assert_not_reached();
    }
    g_free(body);
    g_free(summary);
}

static void
daemon_wifi_selection_needed (NwamuiDaemon* daemon, NwamuiNcu* ncu, gpointer user_data)
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
            nwam_status_icon_set_status(self, NWAMUI_ENV_STATUS_WARNING, ncu );
        }

        nwam_notification_show_message_with_action(summary, body,
          NULL,
          NULL,	/* action */
          NULL,	/* label */
          notifyaction_popup_menus,
          (gpointer)self,
          NULL,
          NOTIFY_EXPIRES_DEFAULT);

        g_free(summary);
        g_free(body);
    } 
}

static void
daemon_ncu_up (NwamuiDaemon* daemon, NwamuiNcu* ncu, gpointer user_data)
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

	/* set status to "CONNECTED" */
	nwam_status_icon_set_status(self, NWAMUI_ENV_STATUS_CONNECTED, ncu);

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

        nwam_notification_show_message (summary, body, 
          nwamui_util_get_ncu_status_icon(ncu), NOTIFY_EXPIRES_DEFAULT);
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

        nwam_notification_show_message (summary, body, NULL, NOTIFY_EXPIRES_DEFAULT);
        if ( wifi ) {
            g_object_unref(wifi);
        }
		break;
    }
    default:
        g_assert_not_reached ();
    }

    g_free (sname);
    g_free (ifname);
    g_free (addr);
    g_free (summary);
    g_free (body);
}

static void
daemon_ncu_down (NwamuiDaemon* daemon, NwamuiNcu* ncu, gpointer user_data)
{
    NwamStatusIcon *self = NWAM_STATUS_ICON(user_data);
    gchar *sname = NULL;
    gchar *summary = NULL;
    
    /* Only show this information, if it's an active ncu */
    if ( nwamui_ncu_get_active(ncu)  ) {
        /* set status to "WARNING", only relevant if it's the active ncu */
        nwam_status_icon_set_status(self, NWAMUI_ENV_STATUS_WARNING, ncu);

        switch (nwamui_ncu_get_ncu_type (ncu)) {
        case NWAMUI_NCU_TYPE_WIRED: {
                gchar *body = NULL;
                sname = nwamui_ncu_get_vanity_name (ncu);
                summary = g_strdup_printf (_("%s disconnected from network."), sname);
                if ( nwamui_daemon_get_event_cause(daemon) != NWAMUI_DAEMON_EVENT_CAUSE_NONE ) {
                    body = g_strdup_printf("Reason: %s",nwamui_daemon_get_event_cause_string(daemon));
                }
                nwam_notification_show_message (summary, body, NULL, NOTIFY_EXPIRES_DEFAULT);
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
                nwam_notification_show_message (summary, body, NULL, NOTIFY_EXPIRES_DEFAULT);
                g_free(body);
            }
            break;
        default:
            g_assert_not_reached ();
        }

    }

    g_free (sname);
    g_free (summary);
}

/**
 * nwam_menu_create_wifi_menuitems:
 *
 * dynamically create a menuitem for wireless/wired and insert in to top
 * of the basic pop up menus. I presume to create toggle menu item.
 * REF UI design P2.
 */

static void
nwam_menu_create_wifi_menuitems (GObject *daemon, GObject *wifi, gpointer data)
{
	NwamStatusIcon *self = NWAM_STATUS_ICON(data);
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);
	GtkWidget *item = NULL;
	
	if (wifi) {
        item = nwam_menu_section_get_item_by_proxy(prv->menu, SECTION_WIFI, wifi);
        if (item) {
            nwam_obj_proxy_refresh(NWAM_OBJ_PROXY_IFACE(item), NULL);
        } else {
            item = nwam_menu_item_create(NWAM_MENU(prv->menu), NWAMUI_OBJECT(wifi));
        }

		prv->has_wifi = TRUE;
	} else {
        g_debug("----------- menu item creation is  over -------------");

        /* we must clear force rescan flag if it is */
        if (prv->force_wifi_rescan) {
            prv->force_wifi_rescan = FALSE;
        }
    }
}

static void
ncp_add_ncu(NwamuiNcp *ncp, NwamuiNcu* ncu, gpointer data)
{
	NwamStatusIcon *self = NWAM_STATUS_ICON(data);
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);

    nwam_menu_item_create(NWAM_MENU(prv->menu), NWAMUI_OBJECT(ncu));
    nwam_tooltip_widget_add_ncu(NWAM_TOOLTIP_WIDGET(prv->tooltip_widget), NWAMUI_OBJECT(ncu));
}

static void
ncp_remove_ncu(NwamuiNcp *ncp, NwamuiNcu* ncu, gpointer data)
{
	NwamStatusIcon *self = NWAM_STATUS_ICON(data);
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);

    nwam_menu_item_delete(NWAM_MENU(prv->menu), NWAMUI_OBJECT(ncu));
    nwam_tooltip_widget_remove_ncu(NWAM_TOOLTIP_WIDGET(prv->tooltip_widget), NWAMUI_OBJECT(ncu));
}

static void
daemon_add_wifi_fav(NwamuiDaemon* daemon, NwamuiWifiNet* wifi, gpointer data)
{
    NwamStatusIcon *self = NWAM_STATUS_ICON(data);
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);
    gchar *summary, *body;
    gchar *name;

    nwam_menu_item_create(NWAM_MENU(prv->menu), NWAMUI_OBJECT(wifi));

    /* TODO - we should send notification iff wifi is automatically added */
    /* according to v1.5 P4 */
    name = nwamui_wifi_net_get_essid (wifi);
    summary = g_strdup_printf (_("Wireless network '%s' added to favorites"),
      name);
    body = g_strdup_printf (_("Click here to view or edit your list of favorite\nwireless networks."));
    
    nwam_notification_show_message_with_action (summary, body,
      NULL,
      NULL,	/* action */
      NULL,	/* label */
      notification_listwireless,
      (gpointer) g_object_ref (self),
      (GFreeFunc) g_object_unref,
      NOTIFY_EXPIRES_DEFAULT);
    g_free(body);
    g_free(summary);
}

static void
daemon_remove_wifi_fav(NwamuiDaemon* daemon, NwamuiWifiNet* wifi, gpointer data)
{
    NwamStatusIcon *self = NWAM_STATUS_ICON(data);
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);

    nwam_menu_item_delete(NWAM_MENU(prv->menu), NWAMUI_OBJECT(wifi));
}

static void
daemon_active_ncp_changed(NwamuiDaemon* daemon, NwamuiNcp* ncp, gpointer data)
{
	NwamStatusIcon *self = NWAM_STATUS_ICON(data);
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);
    gchar *sname;
    gchar *summary, *body;

    if (prv->active_ncp) {
        nwam_menu_stop_update_wifi_timer(self);

        disconnect_nwam_object_signals(G_OBJECT(self), G_OBJECT(prv->active_ncp));
        g_object_unref(prv->active_ncp);
    }

    if ((prv->active_ncp = ncp) != NULL) {

        sname = nwamui_object_get_name(NWAMUI_OBJECT(ncp));
        summary = g_strdup_printf (_("Switched to profile '%s'"), sname);
        body = g_strdup_printf (_("%s\n"), "Now configuring your network");
        nwam_notification_show_message (summary, body, NULL, NOTIFY_EXPIRES_DEFAULT);

        g_free (sname);
        g_free (summary);
        g_free (body);

        g_object_ref(prv->active_ncp);

        connect_nwam_object_signals(G_OBJECT(self), G_OBJECT(prv->active_ncp));

        nwam_menu_recreate_ncu_menuitems(self);

        nwam_tooltip_widget_update_ncp(NWAM_TOOLTIP_WIDGET(prv->tooltip_widget), NWAMUI_OBJECT(prv->active_ncp));

        nwam_menu_start_update_wifi_timer(self);

        nwamui_daemon_dispatch_wifi_scan_events_from_cache(daemon);
    } else {
        /* Update related menus here. */
        /* Disable wireless interface related menu items */
        gtk_widget_set_sensitive(prv->static_menuitems[MENUITEM_REFRESH_WLAN],
          FALSE);
        gtk_widget_set_sensitive(prv->static_menuitems[MENUITEM_JOIN_WLAN],
          FALSE);

        /* Make sure ref'ed, since menu is a container. */
        ADD_MENU_ITEM(NWAM_MENU(prv->menu), g_object_ref(prv->static_menuitems[MENUITEM_NONCU]));

    }
}

static void
daemon_active_env_changed (NwamuiDaemon* daemon, NwamuiEnv* env, gpointer data)
{
	NwamStatusIcon *self = NWAM_STATUS_ICON(data);
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);
    gchar *sname;
    gchar *summary, *body;

    if (env) {
        sname = nwamui_object_get_name(NWAMUI_OBJECT(env));
        summary = g_strdup_printf (_("Switched to location '%s'"), sname);
        body = g_strdup_printf (_("%s\n"), "Now configuring your network");
        nwam_notification_show_message (summary, body, NULL, NOTIFY_EXPIRES_DEFAULT);

        g_free (sname);
        g_free (summary);
        g_free (body);

/*         prv->force_wifi_rescan_due_to_env_changed = TRUE; */
        nwam_menu_recreate_wifi_menuitems (self);

        nwam_tooltip_widget_update_env(NWAM_TOOLTIP_WIDGET(prv->tooltip_widget), NWAMUI_OBJECT(env));
        nwam_menu_recreate_env_menuitems(self);
    } else {
    }
}

static void
connect_nwam_object_signals(GObject *self, GObject *obj)
{
	GType type = G_OBJECT_TYPE(obj);

	if (type == NWAMUI_TYPE_DAEMON) {
        NwamuiDaemon *daemon = NWAMUI_DAEMON(obj);

        g_signal_connect(daemon, "status_changed",
          G_CALLBACK(daemon_status_changed), (gpointer)self);

        g_signal_connect(daemon, "daemon_info",
          G_CALLBACK(daemon_info), (gpointer)self);

        g_signal_connect(daemon, "active_ncp_changed",
          G_CALLBACK(daemon_active_ncp_changed), (gpointer) self);

        g_signal_connect(daemon, "active_env_changed",
          G_CALLBACK(daemon_active_env_changed), (gpointer) self);

        g_signal_connect(daemon, "wifi_key_needed",
          G_CALLBACK(daemon_wifi_key_needed), (gpointer)self);

        g_signal_connect(daemon, "wifi_selection_needed",
          G_CALLBACK(daemon_wifi_selection_needed), (gpointer)self);

        g_signal_connect(daemon, "wifi_scan_result",
          (GCallback)nwam_menu_create_wifi_menuitems, (gpointer) self);

        g_signal_connect(daemon, "ncu_up",
          G_CALLBACK(daemon_ncu_up), (gpointer)self);
	
        g_signal_connect(daemon, "ncu_down",
          G_CALLBACK(daemon_ncu_down), (gpointer)self);

        g_signal_connect(daemon, "add_wifi_fav",
          G_CALLBACK(daemon_add_wifi_fav), (gpointer) self);

        g_signal_connect(daemon, "remove_wifi_fav",
          G_CALLBACK(daemon_remove_wifi_fav), (gpointer) self);

    } else if (type == NWAMUI_TYPE_NCP) {
        NwamuiNcp* ncp = NWAMUI_NCP(obj);

        g_signal_connect(ncp, "activate_ncu",
          G_CALLBACK(ncp_activate_ncu), (gpointer)self);

        g_signal_connect(ncp, "deactivate_ncu",
          G_CALLBACK(ncp_deactivate_ncu), (gpointer)self);

        g_signal_connect(ncp, "add_ncu",
          G_CALLBACK(ncp_add_ncu), (gpointer) self);

        g_signal_connect(ncp, "remove_ncu",
          G_CALLBACK(ncp_remove_ncu), (gpointer) self);

        g_signal_connect(ncp, "notify::ncu-list-store",
          G_CALLBACK(on_ncp_notify), (gpointer)self);

        g_signal_connect(ncp, "notify::many-wireless",
          G_CALLBACK(on_ncp_notify_many_wireless), (gpointer)self);

/* 	} else if (type == NWAMUI_TYPE_NCU) { */
/* 	} else if (type == NWAMUI_TYPE_ENV) { */
/* 	} else if (type == NWAMUI_TYPE_ENM) { */
	} else {
		g_error("%s unknown nwamui obj", __func__);
	}
}

static void
disconnect_nwam_object_signals(GObject *self, GObject *obj)
{
	GType type = G_OBJECT_TYPE(obj);

	if (type == NWAMUI_TYPE_DAEMON) {
    } else if (type == NWAMUI_TYPE_NCP) {
/* 	} else if (type == NWAMUI_TYPE_NCU) { */
/* 	} else if (type == NWAMUI_TYPE_ENV) { */
/* 	} else if (type == NWAMUI_TYPE_ENM) { */
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

static void
join_wireless(NwamuiWifiNet *wifi)
{
    if (wifi) {
        gchar *argv = NULL;
        gchar *name = NULL;
        /* add valid wireless */
        name = nwamui_wifi_net_get_essid (wifi);
        argv = g_strconcat ("--add-wireless-dialog=", name, NULL);
        nwam_exec(argv);
        g_free (name);
        g_free (argv);
    } else {
        /* add other wireless */
        nwam_exec("--add-wireless-dialog=");
    }
}

static gboolean
animation_panel_icon_timeout (gpointer user_data)
{
    NwamStatusIcon *self = NWAM_STATUS_ICON(user_data);
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);

	gtk_status_icon_set_from_pixbuf(GTK_STATUS_ICON(self),
      nwamui_util_get_env_status_icon(GTK_STATUS_ICON(self),
		(nwamui_env_status_t)(++prv->icon_stock_index)%NWAMUI_ENV_STATUS_LAST,
        0));
	return TRUE;
}

static void
trigger_animation_panel_icon (GConfClient *client,
                              guint cnxn_id,
                              GConfEntry *entry,
                              gpointer user_data)
{
    NwamStatusIcon *self = NWAM_STATUS_ICON(user_data);
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);

	prv->animation_icon_update_timeout_id = 0;
	GConfValue *value = NULL;
	
	g_assert (entry);
	value = gconf_entry_get_value (entry);
	g_assert (value);
	if (gconf_value_get_bool (value)) {
		g_assert (prv->animation_icon_update_timeout_id == 0);
		prv->animation_icon_update_timeout_id = 
          g_timeout_add (333, animation_panel_icon_timeout, (gpointer)self);
		g_assert (prv->animation_icon_update_timeout_id > 0);
	} else {
		g_assert (prv->animation_icon_update_timeout_id > 0);
		g_source_remove (prv->animation_icon_update_timeout_id);
		prv->animation_icon_update_timeout_id = 0;
		/* reset everything of animation_panel_icon here */
		gtk_status_icon_set_from_pixbuf(GTK_STATUS_ICON(self), 
          nwamui_util_get_env_status_icon(GTK_STATUS_ICON(self), nwamui_daemon_get_status(prv->daemon), 0));
		prv->icon_stock_index = 0;
	}
}

GtkStatusIcon* 
nwam_status_icon_new( void )
{
    GtkStatusIcon* self;
    self = GTK_STATUS_ICON(g_object_new(NWAM_TYPE_STATUS_ICON, NULL));
    return self;
}

/*
 * nwam_status_icon_run:
 * Make sure this function only be RUN ONCE.
 * Must handle SIGNALS HERE.
 */
void
nwam_status_icon_run(NwamStatusIcon *self)
{
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);

    g_assert(prv->daemon == NULL && prv->menu == NULL);

    g_debug("%s: Hide self firstly!\n", __func__);
    gtk_status_icon_set_visible(GTK_STATUS_ICON(self), FALSE);

    /* Initializing notification, must run first, because others may invoke
     * it.
     */
    nwam_notification_init(GTK_STATUS_ICON(self));

	prv->daemon = nwamui_daemon_get_instance ();
    prv->menu = g_object_ref_sink(nwam_menu_new(N_SECTION));
    g_signal_connect(G_OBJECT(prv->menu), "get_section_index",
      G_CALLBACK(nwam_menu_get_section_index), (gpointer)self);

    /* Must create static menus before connect to any signals. */
    nwam_menu_create_static_menuitems(self);

    /* Connect signals */
    g_signal_connect(G_OBJECT (self), "popup-menu",
      G_CALLBACK (status_icon_popup), NULL);

    g_signal_connect(G_OBJECT (self), "size-changed",
      G_CALLBACK (status_icon_size_changed), NULL);

    g_signal_connect(G_OBJECT (self), "query-tooltip",
      G_CALLBACK (status_icon_query_tooltip), NULL);

    /* Enable tooltip. */
    gtk_status_icon_set_has_tooltip(GTK_STATUS_ICON(self), TRUE);

    /* TODO - For Phase 1 */
    /* nwam_status_icon_set_activate_callback(self, G_CALLBACK(activate_cb), NULL); */

    /* Handle all daemon signals here */
    connect_nwam_object_signals(G_OBJECT(self), G_OBJECT(prv->daemon));

    /* Initially populate network info */
    daemon_status_changed(prv->daemon, nwamui_daemon_get_status(prv->daemon), (gpointer)self);
}

void
nwam_status_icon_set_activate_callback(NwamStatusIcon *self,
  GCallback activate_cb, gpointer user_data)
{
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);
    if (activate_cb) {
        g_assert(prv->activate_handler_id == 0);
        prv->activate_handler_id = g_signal_connect(self, "activate",
          G_CALLBACK(activate_cb), user_data);
    } else {
        g_assert(prv->activate_handler_id > 0);
        g_signal_handler_disconnect(self, prv->activate_handler_id);
        prv->activate_handler_id = 0;
    }
}

void
nwam_status_icon_set_status(NwamStatusIcon *self, gint env_status, NwamuiNcu* wireless_ncu )
{
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);

    prv->current_status = env_status;

    /* nwam_notification_set_default_icon(nwamui_util_get_env_status_icon(GTK_STATUS_ICON(self), env_status, 48)); */

    g_debug("%s: env_status = %d, wireless_ncu = %08X",  __func__, env_status, wireless_ncu );

    if ( wireless_ncu == NULL ) { 
        gtk_status_icon_set_from_pixbuf(GTK_STATUS_ICON(self),
          nwamui_util_get_env_status_icon(GTK_STATUS_ICON(self), env_status, 0));
    }
    else {
        gtk_status_icon_set_from_pixbuf(GTK_STATUS_ICON(self), 
                nwamui_util_get_ncu_status_icon( wireless_ncu ));
    }

}

void
nwam_status_icon_show_menu(NwamStatusIcon *self)
{
    status_icon_popup(GTK_STATUS_ICON(self),
      0, gtk_get_current_event_time(), NULL);
}

static void
status_icon_popup(GtkStatusIcon *status_icon,
  guint button,
  guint activate_time,
  gpointer user_data)
{
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(status_icon);

	if (prv->enable_pop_up_menu && prv->menu != NULL) {

		gtk_menu_popup(GTK_MENU(prv->menu),
          NULL,
          NULL,
          gtk_status_icon_position_menu,
          (gpointer)status_icon,
          button,
          activate_time);
/* 		gtk_widget_show_all(prv->menu); */
	}
}

static gboolean
status_icon_size_changed(GtkStatusIcon *status_icon, gint size, gpointer user_data)
{
/*     NwamStatusIcon *self = NWAM_STATUS_ICON(status_icon); */
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(status_icon);

    gtk_status_icon_set_from_pixbuf(status_icon, 
      nwamui_util_get_env_status_icon(status_icon, prv->current_status, 0));
    return TRUE;
}

static gboolean
status_icon_query_tooltip(GtkStatusIcon *status_icon,
  gint           x,
  gint           y,
  gboolean       keyboard_mode,
  GtkTooltip    *tooltip,
  gpointer       user_data)
{
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(status_icon);

/*     if (nwamui_daemon_get_status(prv->daemon) == NWAMUI_DAEMON_STATUS_ACTIVE) { */
    if (prv->enable_pop_up_menu) {
        g_object_ref(prv->tooltip_widget);
        gtk_tooltip_set_custom(tooltip, GTK_WIDGET(prv->tooltip_widget));
/*         gtk_widget_set_tooltip_window(GTK_WIDGET(prv->tooltip_treeview), NULL); */
    } else {
        gtk_tooltip_set_text(tooltip, _("Service is not active."));
    }
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
        nwam_status_icon_set_status(sicon, NWAMUI_ENV_STATUS_CONNECTED, ncu);

        nwam_notification_show_message(summary, NULL, NULL, NOTIFY_EXPIRES_DEFAULT);

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

                    nwam_status_icon_set_activate_callback(sicon,
                      (GCallback)status_icon_wifi_key_needed, G_OBJECT(wifi) );

                    nwam_notification_show_message_with_action(summary, body,
                      NULL,
                      NULL,	/* action */
                      NULL,	/* label */
                      notifyaction_wifi_key_need,
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
on_activate_static_menuitems (GtkMenuItem *menuitem, gpointer user_data)
{
    NwamStatusIcon *self = NWAM_STATUS_ICON(user_data);
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);
    gchar *argv = NULL;
    gint menuitem_id = (gint)g_object_get_data(G_OBJECT(menuitem), STATIC_MENUITEM_ID);

    switch (menuitem_id) {
    case MENUITEM_ENV_PREF:
		argv = "-l";
        break;
    case MENUITEM_CONN_PROF:
        /* TODO, specify a profile. */
		argv = "-p";
        break;
    case MENUITEM_REFRESH_WLAN:
        nwam_menu_recreate_wifi_menuitems(self);
        break;
    case MENUITEM_JOIN_WLAN:
		argv = "-w";
        break;
    case MENUITEM_VPN_PREF:
        argv = "-n";
        break;
    case MENUITEM_NET_PREF:
		argv = "-p";
        break;
    case MENUITEM_HELP:
        nwamui_util_show_help ("");
        break;
    case MENUITEM_TEST: {
        static gboolean flag = FALSE;
        if (flag = !flag) {
/*             nwam_menu_section_delete(NWAM_MENU(prv->menu), SECTION_LOC); */
/*             nwam_menu_section_set_visible(NWAM_MENU(prv->menu), SECTION_NCU, TRUE); */
            daemon_status_changed(prv->daemon, NWAMUI_DAEMON_STATUS_ACTIVE, (gpointer)self);
        } else {
/*             nwam_menu_recreate_env_menuitems(self); */
/*             nwam_menu_section_set_visible(NWAM_MENU(prv->menu), SECTION_NCU, FALSE); */
            daemon_status_changed(prv->daemon, NWAMUI_DAEMON_STATUS_INACTIVE, (gpointer)self);
        }
        /* Test - must enable pop up menu. */
        prv->enable_pop_up_menu = TRUE;
    }
        break;
    default:
        g_assert_not_reached ();
        /* About */
        gtk_show_about_dialog (NULL, 
          "name", _("NWAM Manager"),
          "title", _("About NWAM Manager"),
          "copyright", _("2009 Sun Microsystems, Inc  All rights reserved\nUse is subject to license terms."),
          "website", _("http://www.sun.com/"),
          NULL);
    }
    if (argv) {
        nwam_exec(argv);
    }
}

static gboolean
update_wifi_timer_func(gpointer user_data)
{
	NwamStatusIcon *self = NWAM_STATUS_ICON (user_data);
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);
    GList *ncu_list;
    NwamuiNcu *ncu;
    NwamuiWifiNet *wifi_net;

    /* TODO, it is required only the ncp has at least one active wireless link. */
    if (nwamui_ncp_get_wireless_link_num(prv->active_ncp) > 0) {
        ncu_list = nwamui_ncp_get_ncu_list(prv->active_ncp);

        while (ncu_list) {
            ncu = NWAMUI_NCU(ncu_list->data);
            ncu_list = g_list_delete_link(ncu_list, ncu_list);

            switch(nwamui_ncu_get_ncu_type(ncu)) {
            case NWAMUI_NCU_TYPE_WIRED:
            case NWAMUI_NCU_TYPE_TUNNEL:
                break;
            case NWAMUI_NCU_TYPE_WIRELESS:
                /* Trigger all related ui widgets. */
                if (nwamui_object_get_active(NWAMUI_OBJECT(ncu))) {
                    nwamui_wifi_signal_strength_t signal_strength;
                    signal_strength = nwamui_ncu_get_signal_strength_from_dladm(ncu);
                    wifi_net = nwamui_ncu_get_wifi_info(ncu);
                    if ( wifi_net && signal_strength < NWAMUI_WIFI_STRENGTH_LAST ) {
                        nwamui_wifi_net_set_signal_strength(wifi_net, signal_strength);
                    }
                    nwam_status_icon_set_status(self, prv->current_status, ncu );
                }
                break;
            default:
                g_assert_not_reached();
                break;
            }
        }
    }

    return TRUE;
}

static void
on_ncp_notify( GObject *gobject, GParamSpec *arg1, gpointer user_data)
{
    NwamStatusIcon *self = NWAM_STATUS_ICON(user_data);
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);

    g_assert(NWAMUI_IS_NCP(gobject));

    if (!arg1 || g_ascii_strcasecmp(arg1->name, "ncu-list-store") == 0) {
        nwam_menu_recreate_ncu_menuitems(self);
    }
}

static void
on_ncp_notify_many_wireless( GObject *gobject, GParamSpec *arg1, gpointer user_data)
{
    NwamStatusIcon *self = NWAM_STATUS_ICON(user_data);
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);
    gboolean wireless_num;

    g_assert(NWAMUI_IS_NCP(gobject));

    wireless_num = nwamui_ncp_get_wireless_link_num(NWAMUI_NCP(gobject));
    if (wireless_num > 0) {
        nwam_menu_section_foreach(NWAM_MENU(prv->menu), SECTION_WIFI,
          (GFunc)nwam_obj_proxy_refresh, NULL);
    } else {
        nwam_menu_section_delete(NWAM_MENU(prv->menu), SECTION_WIFI);
    }
    gtk_widget_set_sensitive(prv->static_menuitems[MENUITEM_REFRESH_WLAN],
      wireless_num > 0);
    gtk_widget_set_sensitive(prv->static_menuitems[MENUITEM_JOIN_WLAN],
      wireless_num > 0);
}

static void
notifyaction_wifi_key_need(NotifyNotification *n,
  gchar *action,
  gpointer user_data)
{
    join_wireless(user_data);
}

static void
notifyaction_popup_menus(NotifyNotification *n,
  gchar *action,
  gpointer user_data)
{
    nwam_status_icon_show_menu(NWAM_STATUS_ICON(user_data));
}

static void
notification_listwireless(NotifyNotification *n,
  gchar *action,
  gpointer data)
{
    nwam_exec("--list-fav-wireless=");
}

static void
nwam_status_icon_class_init (NwamStatusIconClass *klass)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->finalize = (void (*)(GObject*)) nwam_status_icon_finalize;

	g_type_class_add_private(klass, sizeof(NwamStatusIconPrivate));
}

static void
nwam_status_icon_init (NwamStatusIcon *self)
{
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);
    prv->tooltip_widget = nwam_tooltip_widget_new();
}

static void
nwam_status_icon_finalize (NwamStatusIcon *self)
{
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);

    nwam_menu_stop_update_wifi_timer(self);

    nwam_notification_cleanup();

    if (prv->animation_icon_update_timeout_id > 0) {
		g_source_remove (prv->animation_icon_update_timeout_id);
		prv->animation_icon_update_timeout_id = 0;
/* 		/\* reset everything of animation_panel_icon here *\/ */
/* 		prv->icon_stock_index = 0; */
    }

    g_object_unref(prv->menu);

    disconnect_nwam_object_signals(G_OBJECT(self), G_OBJECT(prv->daemon));

    g_object_unref(prv->tooltip_widget);

    /* Clean Ncp. */
    daemon_active_ncp_changed(prv->daemon, NULL, (gpointer)self);

	g_object_unref (G_OBJECT(prv->daemon));
	G_OBJECT_CLASS(nwam_status_icon_parent_class)->finalize(G_OBJECT(self));
}

static void
nwam_menu_recreate_wifi_menuitems (NwamStatusIcon *self)
{
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);

/*     nwam_menu_section_delete(NWAM_MENU(prv->menu), SECTION_WIFI); */

    /* set force rescan flag so that we can identify daemon scan wifi event */
    prv->force_wifi_rescan = TRUE;
    nwamui_daemon_wifi_start_scan(prv->daemon);
}

static void
nwam_menu_recreate_ncu_menuitems (NwamStatusIcon *self)
{
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);
    GList *ncu_list = nwamui_ncp_get_ncu_list(prv->active_ncp);
    GList *idx = NULL;
	GtkWidget* item = NULL;
    gboolean has_wireless_inf = FALSE;

    nwam_menu_section_delete(NWAM_MENU(prv->menu), SECTION_NCU);

    if (ncu_list) {
        for (idx = ncu_list; idx; idx = g_list_next(idx)) {
            //nwam_menuitem_proxy_new(action, ncu_group);
            item = nwam_menu_item_create(NWAM_MENU(prv->menu), NWAMUI_OBJECT(idx->data));
            switch (nwamui_ncu_get_ncu_type (NWAMUI_NCU(idx->data))) {
            case NWAMUI_NCU_TYPE_WIRELESS: {
                /* Enable wireless interface related menu items */
                has_wireless_inf = TRUE;
            }
                break;
            case NWAMUI_NCU_TYPE_WIRED:
            case NWAMUI_NCU_TYPE_TUNNEL:
                break;
            default:
                g_assert_not_reached ();
            }
            g_object_unref(idx->data);
        }
        g_list_free(ncu_list);

	} else {
        /* Make sure ref'ed, since menu is a container. */
        ADD_MENU_ITEM(NWAM_MENU(prv->menu), g_object_ref(prv->static_menuitems[MENUITEM_NONCU]));
    }

    gtk_widget_set_sensitive(prv->static_menuitems[MENUITEM_REFRESH_WLAN],
      has_wireless_inf);
    gtk_widget_set_sensitive(prv->static_menuitems[MENUITEM_JOIN_WLAN],
      has_wireless_inf);
}

static void
nwam_menu_recreate_env_menuitems (NwamStatusIcon *self)
{
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);
    GList *env_list = nwamui_daemon_get_env_list(prv->daemon);
    GList *idx = NULL;
	GtkWidget* item = NULL;

	for (idx = env_list; idx; idx = g_list_next(idx)) {
        //nwam_menuitem_proxy_new(action, env_group);
        item = nwam_menu_item_create(NWAM_MENU(prv->menu), NWAMUI_OBJECT(idx->data));
        g_object_unref(idx->data);
	}
    g_list_free(env_list);
}

static void
nwam_menu_recreate_enm_menuitems (NwamStatusIcon *self)
{
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);
    GList *enm_list = nwamui_daemon_get_enm_list(prv->daemon);
	GList *idx = NULL;
	GtkWidget* item = NULL;

	for (idx = enm_list; idx; idx = g_list_next(idx)) {
        //nwam_menuitem_proxy_new(action, vpn_group);
        item = nwam_menu_item_create(NWAM_MENU(prv->menu), NWAMUI_OBJECT(idx->data));
        g_object_unref(idx->data);
	}
    g_list_free(enm_list);
}

static void
nwam_menu_start_update_wifi_timer(NwamStatusIcon *self)
{
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);

    if (prv->update_wifi_timer_id > 0) {
        g_source_remove(prv->update_wifi_timer_id);
        prv->update_wifi_timer_id = 0;
    }

    prv->update_wifi_timer_id = g_timeout_add_full(G_PRIORITY_DEFAULT,
      update_wifi_timer_interval,
      update_wifi_timer_func,
      (gpointer)self,
      (GDestroyNotify)g_object_unref);
}

static void
nwam_menu_stop_update_wifi_timer(NwamStatusIcon *self)
{
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);

    if (prv->update_wifi_timer_id > 0) {
        g_source_remove(prv->update_wifi_timer_id);
        prv->update_wifi_timer_id = 0;
    }
}

static void
nwam_menu_create_static_menuitems (NwamStatusIcon *self)
{
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);
    GtkWidget *root_menu = prv->menu;
    GtkWidget *sub_menu;
    GtkWidget *menuitem;

#define ITEM_VARNAME menuitem
#define CACHE_STATIC_MENUITEMS(self, id)                                \
    {                                                                   \
        NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self); \
        prv->static_menuitems[id] = menuitem;                           \
        g_object_set_data(G_OBJECT(menuitem),                           \
          STATIC_MENUITEM_ID, (gpointer)id);                            \
    }

    sub_menu = nwam_menu_new(N_SECTION);
    g_signal_connect(sub_menu, "get_section_index",
      G_CALLBACK(nwam_menu_get_section_index), (gpointer)self);
    START_MENU_SECTION_SEPARATOR(sub_menu, SECTION_LOC, FALSE);
    END_MENU_SECTION_SEPARATOR(sub_menu, SECTION_LOC, TRUE);

    menu_append_item(sub_menu,
      GTK_TYPE_MENU_ITEM, _("_Location Preferences..."),
      on_activate_static_menuitems, self);
    CACHE_STATIC_MENUITEMS(self, MENUITEM_ENV_PREF);
    menu_append_item_with_submenu(root_menu,
      GTK_TYPE_MENU_ITEM, _("_Location"), sub_menu);

    sub_menu = nwam_menu_new(N_SECTION);
    g_signal_connect(sub_menu, "get_section_index",
      G_CALLBACK(nwam_menu_get_section_index), (gpointer)self);
    START_MENU_SECTION_SEPARATOR(sub_menu, SECTION_NCU, FALSE);
    END_MENU_SECTION_SEPARATOR(sub_menu, SECTION_NCU, TRUE);

    menu_append_item(sub_menu,
      GTK_TYPE_MENU_ITEM, _("_Connection Profile..."),
      on_activate_static_menuitems, self);
    CACHE_STATIC_MENUITEMS(self, MENUITEM_CONN_PROF);

    menu_append_item_with_submenu(root_menu,
      GTK_TYPE_MENU_ITEM, _("_Connections"), sub_menu);

    START_MENU_SECTION_SEPARATOR(root_menu, SECTION_WIFI, TRUE);
/*     END_MENU_SECTION_SEPARATOR(root_menu, SECTION_WIFI, TRUE); */
    menu_append_separator(root_menu);

    menu_append_item(root_menu,
      GTK_TYPE_MENU_ITEM, _("_Refresh Wireless Networks"),
      on_activate_static_menuitems, self);
    CACHE_STATIC_MENUITEMS(self, MENUITEM_REFRESH_WLAN);

    menu_append_item(root_menu,
      GTK_TYPE_MENU_ITEM, _("_Join Unlisted Wireless Network..."),
      on_activate_static_menuitems, self);
    CACHE_STATIC_MENUITEMS(self, MENUITEM_JOIN_WLAN);

    menu_append_separator(root_menu);

    /* Sub_Menu */
    sub_menu = nwam_menu_new(N_SECTION);
    g_signal_connect(sub_menu, "get_section_index",
      G_CALLBACK(nwam_menu_get_section_index), (gpointer)self);
    START_MENU_SECTION_SEPARATOR(sub_menu, SECTION_ENM, FALSE);
    END_MENU_SECTION_SEPARATOR(sub_menu, SECTION_ENM, TRUE);

    menu_append_item(sub_menu,
      GTK_TYPE_MENU_ITEM, _("_VPN Preferences..."),
      on_activate_static_menuitems, self);
    CACHE_STATIC_MENUITEMS(self, MENUITEM_VPN_PREF);
    menu_append_item_with_submenu(root_menu,
      GTK_TYPE_MENU_ITEM,_("_VPN Applications"), sub_menu);

    menu_append_separator(root_menu);

    menu_append_item(root_menu,
      GTK_TYPE_MENU_ITEM, _("_Network Preferences..."),
      on_activate_static_menuitems, self);
    CACHE_STATIC_MENUITEMS(self, MENUITEM_NET_PREF);

    menu_append_stock_item(root_menu,
      _("_Help"), GTK_STOCK_HELP,
      on_activate_static_menuitems, self);
    CACHE_STATIC_MENUITEMS(self, MENUITEM_HELP);

    /* Only available for testing. */
    if (debug) {
        menu_append_item(root_menu,
          GTK_TYPE_MENU_ITEM, _("TeSt MeNu"),
          on_activate_static_menuitems, self);
        CACHE_STATIC_MENUITEMS(self, MENUITEM_TEST);
    }

    /* Other dynamical menuitems */
    menuitem = gtk_menu_item_new_with_label(NONCU);
    gtk_widget_set_sensitive(menuitem, FALSE);
    CACHE_STATIC_MENUITEMS(self, MENUITEM_NONCU);

#undef ITEM_VARNAME
#undef CACHE_STATIC_MENUITEMS
}

static GtkWidget*
nwam_menu_item_create(NwamMenu *self, NwamuiObject *object)
{
    GType type = G_OBJECT_TYPE(object);
    GtkWidget *item;

    if (debug) {
        gchar *name = nwamui_object_get_name(object);
        g_debug("%s %s", __func__, name);
        g_free(name);
    }

    if (type == NWAMUI_TYPE_NCU) {
        item = nwam_ncu_item_new(NWAMUI_NCU(object));
    } else if (type == NWAMUI_TYPE_WIFI_NET) {
        item = nwam_wifi_item_new(NWAMUI_WIFI_NET(object));
	} else if (type == NWAMUI_TYPE_ENV) {
        item = nwam_env_item_new(NWAMUI_ENV(object));
	} else if (type == NWAMUI_TYPE_ENM) {
        item = nwam_enm_item_new(NWAMUI_ENM(object));
	} else {
        item = NULL;
		g_error("%s unknown nwamui object", __func__);
	}
    ADD_MENU_ITEM(self, item);
    return item;
}

static void
nwam_menu_item_delete(NwamMenu *self, NwamuiObject *object)
{
    GType type = G_OBJECT_TYPE(object);
    GtkWidget *item;
    gint sec_id;

    if (debug) {
        gchar *name = nwamui_object_get_name(object);
        g_debug("%s %s", __func__, name);
        g_free(name);
    }

    if (type == NWAMUI_TYPE_NCU) {
        sec_id = SECTION_NCU;
    } else if (type == NWAMUI_TYPE_WIFI_NET) {
        sec_id = SECTION_WIFI;
 	} else if (type == NWAMUI_TYPE_ENV) {
        sec_id = SECTION_LOC;
 	} else if (type == NWAMUI_TYPE_ENM) {
        sec_id = SECTION_ENM;
 	} else {
        sec_id = SECTION_GLOBAL;
 	}

    item = nwam_menu_section_get_item_by_proxy(self, sec_id, G_OBJECT(object));
    REMOVE_MENU_ITEM(self, item);
}

static void
nwam_menu_get_section_index(NwamMenu *self, GtkWidget *child, gint *index, gpointer user_data)
{
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(user_data);
    GType type = G_OBJECT_TYPE(child);

    if (type == NWAM_TYPE_WIFI_ITEM) {
        *index = SECTION_WIFI;
    } else if (type == NWAM_TYPE_NCU_ITEM) {
        *index = SECTION_NCU;
	} else if (type == NWAM_TYPE_ENV_ITEM) {
        *index = SECTION_LOC;
	} else if (type == NWAM_TYPE_ENM_ITEM) {
        *index = SECTION_ENM;
    } else if (child == (gpointer)prv->static_menuitems[MENUITEM_NONCU]) {
        *index = SECTION_NCU;
	} else {
        *index = -1;
	}
}

void
nwam_exec (const gchar *nwam_arg)
{
	GError *error = NULL;
	gchar *argv[4] = {0};
	gint argc = 0;
	argv[argc++] = g_find_program_in_path (NWAM_MANAGER_PROPERTIES);
	/* FIXME, seems to be Solaris specific */
	if (argv[0] == NULL) {
		gchar *base = NULL;
		base = g_path_get_dirname (getexecname());
		argv[0] = g_build_filename (base, NWAM_MANAGER_PROPERTIES, NULL);
		g_free (base);
	}
	argv[argc++] = (gchar *)nwam_arg;
	g_debug ("\n\nnwam-menu-exec: %s %s\n", argv[0], nwam_arg);

	if (!g_spawn_async(NULL,
		argv,
		NULL,
		G_SPAWN_LEAVE_DESCRIPTORS_OPEN,
		NULL,
		NULL,
		NULL,
		&error)) {
		fprintf(stderr, "Unable to exec: %s\n", error->message);
		g_error_free(error);
	}
	g_free (argv[0]);
}

