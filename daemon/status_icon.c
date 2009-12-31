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
#include "capplet/nwam_wireless_dialog.h"
#include "capplet/nwam_wireless_chooser.h"
#include "capplet/capplet-utils.h"

typedef struct _NwamStatusIconPrivate NwamStatusIconPrivate;
#define NWAM_STATUS_ICON_GET_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), NWAM_TYPE_STATUS_ICON, NwamStatusIconPrivate))

#define NWAM_MANAGER_PROPERTIES "nwam-manager-properties"
#define STATIC_MENUITEM_ID "static_item_id"

/* nwamui preference signal */
static void join_wifi_not_in_fav(GObject *gobject, GParamSpec *arg1, gpointer data);
static void join_any_fav_wifi(GObject *gobject, GParamSpec *arg1, gpointer data);
static void add_any_new_wifi_to_fav(GObject *gobject, GParamSpec *arg1, gpointer data);
static void action_on_no_fav_networks(GObject *gobject, GParamSpec *arg1, gpointer data);

static glong                              update_wifi_timer_interval     = 5*1000;
static nwamui_action_on_no_fav_networks_t prof_action_if_no_fav_networks = NWAMUI_NO_FAV_ACTION_NONE;
static gboolean                           prof_ask_join_open_network     = FALSE;
static gboolean                           prof_ask_join_fav_network      = FALSE;
static gboolean                           prof_ask_add_to_fav            = TRUE;


enum {
	SECTION_GLOBAL = 0,         /* Not really used */
	SECTION_WIFI,
	SECTION_LOC,
	SECTION_ENM,
	SECTION_NCU,
	SECTION_WIFI_CONTROL,
	N_SECTION
};

#define	NONCU               "No network interfaces detected"
#define NO_ENABLED_WIRELESS "No wireless connections enabled"

enum {
    MENUITEM_SWITCH_LOC_MANUALLY,
    MENUITEM_SWITCH_LOC_AUTO,
    MENUITEM_ENV_PREF,
    MENUITEM_CONN_PROF,
    MENUITEM_REFRESH_WLAN,
    MENUITEM_JOIN_WLAN,
    MENUITEM_VPN_PREF,
    MENUITEM_NET_PREF,
    MENUITEM_HELP,
    /* Others */
    MENUITEM_NONCU,
    MENUITEM_NO_ENABLED_WIRELESS,
    /* Test menuitems */
    MENUITEM_TEST,
    N_STATIC_MENUITEMS
};

struct _NwamStatusIconPrivate {
    GtkWidget    *menu;
	gboolean      enable_pop_up_menu;
    GtkWidget    *tooltip_widget;
    GtkWidget    *static_menuitems[N_STATIC_MENUITEMS];
    NwamuiProf   *prof;
	NwamuiDaemon *daemon;
    NwamuiNcp    *active_ncp;
    gint          current_status;
    gboolean      needs_wifi_selection_seen;
    NwamuiWifiNet*needs_wifi_key;

    gint icon_stock_index;
    guint animation_icon_update_timeout_id;

    guint    update_wifi_timer_id;
    guint    enable_sync_wifi_signals_timer_id;
    gulong   activate_handler_id;

    NwamPrefIFace   *chooser_dialog;
    NwamPrefIFace   *wifi_dialog;

	gboolean has_wifi;

    /* menu-item widget cache */
    GList *cached_menuitem_list[N_SECTION];
};

static void nwam_status_icon_finalize (NwamStatusIcon *self);

static void nwam_menu_create_static_menuitems(NwamStatusIcon *self);

static GtkWidget* nwam_status_icon_create_menu_item(NwamStatusIcon *self, NwamuiObject *object);
static void nwam_status_icon_delete_menu_item(NwamStatusIcon *self, NwamuiObject *object);
static void nwam_menu_get_section_index(NwamMenu *self, GtkWidget *child, gint *index, gpointer user_data);

static void nwam_menu_start_update_wifi_timer(NwamStatusIcon *self);
static void nwam_menu_stop_update_wifi_timer(NwamStatusIcon *self);

static void nwam_menu_recreate_wifi_menuitems (NwamStatusIcon *self, gboolean force_scan );
static void nwam_menu_recreate_ncu_menuitems (NwamStatusIcon *self);
static void nwam_menu_recreate_env_menuitems (NwamStatusIcon *self);
static void nwam_menu_recreate_enm_menuitems (NwamStatusIcon *self);

static gboolean animation_panel_icon_timeout (gpointer user_data);
static void trigger_animation_panel_icon (GConfClient *client,
  guint cnxn_id,
  GConfEntry *entry,
  gpointer user_data);

static void connect_nwam_object_signals(GObject *obj, GObject *self);
static void disconnect_nwam_object_signals(GObject *obj, GObject *self);
static void initial_notify_nwam_object(GObject *obj, GObject *self);

static gint ncp_find_enabled_wireless_ncu(gconstpointer a, gconstpointer b);
static void nwam_menu_update_wifi_section(NwamStatusIcon *self);

/* nwamui utilies */
static void set_window_urgency( GtkWindow *window, gboolean urgent, gboolean raise );
static void show_wireless_chooser( NwamStatusIcon *self );
static void set_wireless_chooser_urgency( NwamStatusIcon *self, gboolean urgent );
static void join_wireless(NwamStatusIcon* self, NwamuiWifiNet *wifi, gboolean do_connect );
static void set_join_wireless_urgency( NwamStatusIcon *self, gboolean urgent );
static gboolean daemon_status_is_good(NwamuiDaemon *daemon);

/* call back */
static void location_model_menuitems(GtkMenuItem *menuitem, gpointer user_data);
static void on_activate_static_menuitems (GtkMenuItem *menuitem, gpointer user_data);
static void activate_test_menuitems(GtkMenuItem *menuitem, gpointer user_data);
static void status_icon_default_activation(GtkStatusIcon *status_icon, GObject* object);

/* nwamui daemon events */
static void daemon_status_changed(NwamuiDaemon *daemon, GParamSpec *arg1, gpointer user_data);
static void daemon_info(NwamuiDaemon *daemon, gint type, GObject *obj, gpointer data, gpointer user_data);
static void daemon_wifi_key_needed (NwamuiDaemon* daemon, NwamuiWifiNet* wifi, gpointer user_data);
static void daemon_wifi_selection_needed (NwamuiDaemon* daemon, NwamuiNcu* ncu, gpointer user_data);
static void nwam_menu_scan_started(GObject *daemon, gpointer data);
static void nwam_menu_create_wifi_menuitems (GObject *daemon, GObject *wifi, gpointer data);
static void daemon_add_wifi_fav(NwamuiDaemon* daemon, NwamuiWifiNet* wifi, gpointer data);
static void daemon_remove_wifi_fav(NwamuiDaemon* daemon, NwamuiWifiNet* wifi, gpointer data);
static void daemon_active_ncp_changed(NwamuiDaemon *daemon, GParamSpec *arg1, gpointer user_data);
static void daemon_active_env_changed(NwamuiDaemon *daemon, GParamSpec *arg1, gpointer user_data);
static void daemon_enm_list_changed(NwamuiDaemon *daemon, GParamSpec *arg1, gpointer data);
static void daemon_env_list_changed(NwamuiDaemon *daemon, GParamSpec *arg1, gpointer data);
static void daemon_online_enm_num_notify(GObject *gobject, GParamSpec *arg1, gpointer data);

/* nwamui ncp signals */
static void ncp_activate_ncu (NwamuiNcp *ncp, NwamuiNcu* ncu, gpointer user_data);
static void ncp_deactivate_ncu (NwamuiNcp *ncp, NwamuiNcu* ncu, gpointer user_data);
static void ncp_add_ncu(NwamuiNcp *ncp, NwamuiNcu* ncu, gpointer data);
static void ncp_remove_ncu(NwamuiNcp *ncp, NwamuiNcu* ncu, gpointer data);
static void on_ncp_notify( GObject *gobject, GParamSpec *arg1, gpointer user_data);
static void on_ncp_notify_many_wireless( GObject *gobject, GParamSpec *arg1, gpointer user_data);

/* nwamui ncu signals */
static void ncu_notify_active(GObject *gobject, GParamSpec *arg1, gpointer data);

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
static void notifyaction_popup_menus(NotifyNotification *n,
  gchar *action,
  gpointer user_data);
static void notifyaction_join_wireless(NotifyNotification *n,
  gchar *action,
  gpointer user_data);

G_DEFINE_TYPE (NwamStatusIcon, nwam_status_icon, GTK_TYPE_STATUS_ICON)

static void
status_icon_default_activation(GtkStatusIcon *status_icon, GObject* object)
{
    const gchar *argv[2] = { NWAMUI_CAPPLET_OPT_NET_PREF_DIALOG_STR, NULL };

    /* Run properties editor */
    nwam_exec( argv );;
}

static gboolean
ncu_is_higher_priority_than_active_ncu( NwamuiNcu* ncu, gboolean *is_active_ptr )
{
    NwamuiDaemon*  daemon  = nwamui_daemon_get_instance();
    NwamuiObject*  active_ncp  = nwamui_daemon_get_active_ncp( daemon );
    NwamuiNcu*     active_ncu  = NULL;
    gint           ncu_prio    = nwamui_ncu_get_priority_group(ncu);
    gint           active_prio = -1;
    gboolean       is_active_ncu = FALSE;
    gboolean       retval = FALSE;

    if ( active_ncp ) {
        active_prio = nwamui_ncp_get_prio_group(NWAMUI_NCP(active_ncp));
        g_object_unref(active_ncp);
    }


    is_active_ncu = ( ncu_prio == active_prio );

    if ( is_active_ptr != NULL ) {
        *is_active_ptr = is_active_ncu;
    }

    g_object_unref(daemon);
    
    return (ncu_prio >= active_prio );
}

static void
daemon_status_changed(NwamuiDaemon *daemon, GParamSpec *arg1, gpointer user_data)
{
    NwamStatusIcon *self = NWAM_STATUS_ICON(user_data);
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);
    const gchar* status_str;
    const gchar *body_str;
    static gboolean need_report_daemon_error = FALSE;

	/* should repopulate data here */

    switch(nwamui_daemon_get_status(daemon)) {
    case NWAMUI_DAEMON_STATUS_NEEDS_ATTENTION:
    case NWAMUI_DAEMON_STATUS_ALL_OK: {
        nwam_status_icon_set_status(self, NULL);
        g_debug("%s: StatusIcon: show self now!", __func__);
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
    }
        break;
    case NWAMUI_DAEMON_STATUS_ERROR:
        prv->enable_pop_up_menu = FALSE;

        if (nwamui_util_is_debug_mode()) {
            g_debug("%s: StatusIcon (Debug): show self now!", __func__);
            gtk_status_icon_set_visible(GTK_STATUS_ICON(self), TRUE);
            prv->enable_pop_up_menu = TRUE;
        }

        if (gtk_status_icon_get_visible(GTK_STATUS_ICON(self))) {

            /* On intial run with NWAM disabled the icon won't be visible */
            nwam_status_icon_set_status(self, NULL);

            nwam_notification_show_nwam_unavailable();
		
            need_report_daemon_error = TRUE;
        }
        
        /* Hide the whole menus instead of deleting menuitems. */
/*         nwam_menu_section_delete(NWAM_MENU(prv->menu), SECTION_ENM); */
/*         nwam_menu_section_delete(NWAM_MENU(prv->menu), SECTION_WIFI); */

        /* Clean Ncp. */
        daemon_active_ncp_changed(prv->daemon, NULL, user_data);
        /* Clean Env. */
        daemon_active_env_changed(prv->daemon, NULL, user_data);

        break;
    case NWAMUI_DAEMON_STATUS_UNINITIALIZED:
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

            ncu = nwamui_wifi_net_get_ncu( NWAMUI_WIFI_NET(obj) );

            nwam_status_icon_set_status(self, ncu );

            if ( ncu ) {
                g_object_unref(ncu);
            }
        }
    }
        break;
    case NWAMUI_DAEMON_INFO_WLAN_DISCONNECTED: { /* unused */
        gboolean        show_message = FALSE;
        gboolean        is_active_ncu = FALSE;
        NwamuiWifiNet*  wifi = NULL;
        NwamuiNcu      *ncu = NULL;

        if ( obj != NULL && NWAMUI_IS_WIFI_NET( obj ) ) {
            wifi = NWAMUI_WIFI_NET(obj);
        }
        if ( wifi != NULL ) {
            ncu = nwamui_wifi_net_get_ncu( wifi );
            show_message = ncu_is_higher_priority_than_active_ncu( ncu, &is_active_ncu );
        }

        if ( show_message ) {
            /* Only change status if it's the active_ncu */
            nwam_notification_show_ncu_disconnected( ncu, notifyaction_popup_menus, G_OBJECT(self) );
        }
        if (ncu) {
            g_object_unref(ncu);
        }
    }
        break;
    case NWAMUI_DAEMON_INFO_WLAN_CHANGED: {
        gboolean show_message = FALSE;

        if ( obj != NULL && NWAMUI_IS_NCU(obj) ) {
            NwamuiNcu*  ncu         = NWAMUI_NCU(obj);

            show_message = ncu_is_higher_priority_than_active_ncu( ncu, NULL );
        }

        if ( show_message && nwamui_daemon_get_num_scanned_wifi( daemon ) == 0 ) {
            nwam_notification_show_no_wifi_networks(notifyaction_join_wireless, G_OBJECT(self));
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
            ncu = nwamui_wifi_net_get_ncu( wifi );
            show_message = ncu_is_higher_priority_than_active_ncu( ncu, &is_active_ncu );

        }
        if ( show_message ) {
            nwam_status_icon_set_status(self, ncu );
            nwam_notification_show_ncu_wifi_connect_failed( ncu );
        }

        if (ncu) {
            g_object_unref(ncu);
        }
    }
        break;
    default:
        break;
    }
}

static void
daemon_wifi_key_needed (NwamuiDaemon* daemon, NwamuiWifiNet* wifi, gpointer user_data)
{
    NwamStatusIcon *self = NWAM_STATUS_ICON(user_data);
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);

	if (wifi && wifi != prv->needs_wifi_key) {
        prv->needs_wifi_key = wifi; /* no need to ref, we don't use contents */

        join_wireless(self, wifi, FALSE );

    }
}

static void
daemon_wifi_selection_needed (NwamuiDaemon* daemon, NwamuiNcu* ncu, gpointer user_data)
{
    NwamStatusIcon *self = NWAM_STATUS_ICON(user_data);
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);
    gboolean        active_ncu = FALSE;

    if ( ncu == NULL || nwamui_ncu_get_ncu_type(ncu) != NWAMUI_NCU_TYPE_WIRELESS ) {
        return;
    }

    /* Only show the message when it's relevant */
	if (ncu && ncu_is_higher_priority_than_active_ncu( ncu, &active_ncu )) {

        if ( active_ncu ) {
            nwam_status_icon_set_status(self, ncu );
        }

        /* Don't show anything if we've not scanned any networks */
        if ( nwamui_daemon_get_num_scanned_wifi( daemon ) != 0 ) {
            if (!prv->needs_wifi_selection_seen) {
                if ( prof_action_if_no_fav_networks == NWAMUI_NO_FAV_ACTION_SHOW_LIST_DIALOG ) {
                    show_wireless_chooser( self );
                }
                else {
                    nwam_notification_show_ncu_wifi_selection_needed( ncu, notifyaction_popup_menus, G_OBJECT(self) );
                }

                prv->needs_wifi_selection_seen = TRUE;
            }
            else { /* If dialog still visibie raise it */
                set_wireless_chooser_urgency( self, TRUE );
            }
        }
    } 
}

static void
ncu_notify_active(GObject *gobject, GParamSpec *arg1, gpointer data)
{
    NwamStatusIcon *self       = NWAM_STATUS_ICON(data);
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);
    NwamuiNcu      *ncu        = NWAMUI_NCU(gobject);
    gboolean        active_ncu = FALSE;

	if (ncu && ncu_is_higher_priority_than_active_ncu( ncu, &active_ncu )) {
        if ( active_ncu ) {
            nwam_status_icon_set_status(self, ncu);
        }
    }
#if 0
    /* Wired only, wireless handled elsewhere. */
    if ( active_ncu &&
      nwamui_ncu_get_ncu_type( ncu ) == NWAMUI_NCU_TYPE_WIRED ) {
        static nwam_state_t     cached_nwam_state = NWAM_STATE_UNINITIALIZED;
        static nwam_aux_state_t cached_nwam_aux_state = NWAM_AUX_STATE_UNINITIALIZED;
        nwam_state_t            nwam_state;
        nwam_aux_state_t        nwam_aux_state;

        /* Use cached state */
        nwam_state = nwamui_object_get_nwam_state(NWAMUI_OBJECT(ncu), &nwam_aux_state, NULL);
        switch (nwam_state) {
        case NWAM_STATE_UNINITIALIZED:
        case NWAM_STATE_INITIALIZED:
        case NWAM_STATE_OFFLINE:
        case NWAM_STATE_OFFLINE_TO_ONLINE:
        case NWAM_STATE_ONLINE_TO_OFFLINE:
        case NWAM_STATE_ONLINE:
        case NWAM_STATE_MAINTENANCE:
        case NWAM_STATE_DEGRADED:
        case NWAM_STATE_DISABLED:
        }

/*             if (cached_nwam_state != nwam_state && cached_nwam_aux_state != nwam_aux_state) { */
        if ( nwam_state == NWAM_STATE_ONLINE_TO_OFFLINE  &&
          nwam_aux_state == NWAM_AUX_STATE_DOWN ) {
            /* Only show if in transition to down for sure */
            nwam_notification_show_ncu_disconnected(ncu, NULL, NULL);
        }
/*             } */
        cached_nwam_state = nwam_state;
        cached_nwam_aux_state = nwam_aux_state;
    }
#endif
    /* Ncu state filter. */
    switch(nwamui_ncu_get_updated_connection_state(ncu)) {
    case NWAMUI_STATE_CONNECTED_ESSID:
    case NWAMUI_STATE_CONNECTED:
        prv->needs_wifi_selection_seen = FALSE;
        prv->needs_wifi_key = NULL;
        if (nwamui_object_get_active(NWAMUI_OBJECT(ncu))) {
            nwam_notification_show_ncu_connected( ncu );
        }
        break;
    case NWAMUI_STATE_NOT_CONNECTED:
    case NWAMUI_STATE_CABLE_UNPLUGGED: 
        /* Only show if in transition to down for sure */
        nwam_notification_show_ncu_disconnected(ncu, NULL, NULL);
        break;
    case NWAMUI_STATE_NETWORK_UNAVAILABLE:
    case NWAMUI_STATE_CONNECTING_ESSID:
    case NWAMUI_STATE_NEEDS_KEY_ESSID:
    case NWAMUI_STATE_NEEDS_SELECTION:
    case NWAMUI_STATE_UNKNOWN:
    case NWAMUI_STATE_CONNECTING:
    default:
        break;
    }
}

/* 
 * nwam_menu_scan_started
 *
 * When we see a scan started message we should clean up the exiting menu
 * items.
 */
static void
nwam_menu_scan_started(GObject *daemon, gpointer data)
{
	NwamStatusIcon *self = NWAM_STATUS_ICON(data);
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);

    if (!GTK_WIDGET_VISIBLE(prv->menu)) {
        prv->cached_menuitem_list[SECTION_WIFI] = g_list_concat(nwam_menu_section_delete(NWAM_MENU(prv->menu), SECTION_WIFI, TRUE), prv->cached_menuitem_list[SECTION_WIFI]);

        g_signal_connect(daemon, "wifi_scan_result",
          (GCallback)nwam_menu_create_wifi_menuitems, (gpointer) self);
    }
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
        item = nwam_status_icon_create_menu_item(self, NWAMUI_OBJECT(wifi));

		prv->has_wifi = TRUE;
	} else {
        /* Sort the wifi menu-items. */
        nwam_menu_section_sort(NWAM_MENU(prv->menu), SECTION_WIFI);
        g_debug("----------- menu item creation is  over -------------");
        g_signal_handlers_disconnect_by_func(prv->daemon,
          (gpointer)nwam_menu_create_wifi_menuitems, (gpointer)self);
    }
}

static void
ncp_add_ncu(NwamuiNcp *ncp, NwamuiNcu* ncu, gpointer data)
{
	NwamStatusIcon *self = NWAM_STATUS_ICON(data);
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);

    connect_nwam_object_signals(G_OBJECT(ncu), G_OBJECT(self));
    nwam_status_icon_create_menu_item(self, NWAMUI_OBJECT(ncu));
    REMOVE_MENU_ITEM(NWAM_MENU(prv->menu), prv->static_menuitems[MENUITEM_NONCU]);
    nwam_tooltip_widget_update_daemon(NWAM_TOOLTIP_WIDGET(prv->tooltip_widget), NWAMUI_OBJECT(prv->daemon));
/*     nwam_tooltip_widget_add_ncu(NWAM_TOOLTIP_WIDGET(prv->tooltip_widget), NWAMUI_OBJECT(ncu)); */
}

static void
ncp_remove_ncu(NwamuiNcp *ncp, NwamuiNcu* ncu, gpointer data)
{
	NwamStatusIcon *self = NWAM_STATUS_ICON(data);
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);

    disconnect_nwam_object_signals(G_OBJECT(ncu), G_OBJECT(self));
    nwam_status_icon_delete_menu_item(self, NWAMUI_OBJECT(ncu));
    nwam_tooltip_widget_update_daemon(NWAM_TOOLTIP_WIDGET(prv->tooltip_widget), NWAMUI_OBJECT(prv->daemon));
/*     nwam_tooltip_widget_remove_ncu(NWAM_TOOLTIP_WIDGET(prv->tooltip_widget), NWAMUI_OBJECT(ncu)); */
}

static void
daemon_add_wifi_fav(NwamuiDaemon* daemon, NwamuiWifiNet* wifi, gpointer data)
{
    NwamStatusIcon *self = NWAM_STATUS_ICON(data);
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);

    nwam_status_icon_create_menu_item(self, NWAMUI_OBJECT(wifi));
}

static void
daemon_remove_wifi_fav(NwamuiDaemon* daemon, NwamuiWifiNet* wifi, gpointer data)
{
    NwamStatusIcon *self = NWAM_STATUS_ICON(data);
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);

    nwam_status_icon_delete_menu_item(self, NWAMUI_OBJECT(wifi));
}

static void
daemon_enm_list_changed(NwamuiDaemon *daemon, GParamSpec *arg1, gpointer data)
{
	NwamStatusIcon *self = NWAM_STATUS_ICON(data);
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);

    nwam_menu_recreate_enm_menuitems(self);
/*     nwam_tooltip_widget_update_daemon(NWAM_TOOLTIP_WIDGET(prv->tooltip_widget), NWAMUI_OBJECT(prv->daemon)); */
    daemon_online_enm_num_notify(NULL, NULL, (gpointer)self);
}

static void
daemon_env_list_changed(NwamuiDaemon *daemon, GParamSpec *arg1, gpointer data)
{
	NwamStatusIcon *self = NWAM_STATUS_ICON(data);
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);

    nwam_menu_recreate_env_menuitems(self);
}

static void
daemon_online_enm_num_notify(GObject *gobject, GParamSpec *arg1, gpointer data)
{
    NwamStatusIconPrivate *prv              = NWAM_STATUS_ICON_GET_PRIVATE(data);
    GList*                 enm_elem;
    NwamuiEnm*             enm;
    int                    enm_active_count = nwamui_daemon_get_online_enm_num(prv->daemon);
    gchar*                 enm_str          = NULL;

    
    if ( enm_active_count == 0 ) {
        enm_str = g_strdup(_("None Active"));
    } else if (enm_active_count > 1) {
        enm_str = g_strdup_printf("%d Active", enm_active_count);
    } else {
        for ( enm_elem = g_list_first(nwamui_daemon_get_enm_list(NWAMUI_DAEMON(prv->daemon)));
              enm_elem != NULL;
              enm_elem = g_list_delete_link(enm_elem, enm_elem) ) {
        
            enm = NWAMUI_ENM(enm_elem->data);
        
            if  ( nwamui_object_get_active(NWAMUI_OBJECT(enm))) {
                enm_str = g_strdup_printf("%s Active", nwamui_object_get_name(NWAMUI_OBJECT(enm)));
                break;
            }
        }
        g_list_free(enm_elem);
    }
        
    nwam_tooltip_widget_update_enm(NWAM_TOOLTIP_WIDGET(prv->tooltip_widget), enm_str);

    g_free( enm_str );
}

static void
daemon_active_ncp_changed(NwamuiDaemon *daemon, GParamSpec *arg1, gpointer data)
{
	NwamStatusIcon        *self = NWAM_STATUS_ICON(data);
    NwamStatusIconPrivate *prv  = NWAM_STATUS_ICON_GET_PRIVATE(self);
    gboolean               init = FALSE;
    NwamuiObject          *ncp  = daemon_status_is_good(prv->daemon) ? nwamui_daemon_get_active_ncp(prv->daemon) : NULL;

    if (prv->active_ncp != (NwamuiNcp*)ncp) {

        if (prv->active_ncp) {
            nwam_menu_stop_update_wifi_timer(self);

            disconnect_nwam_object_signals(G_OBJECT(prv->active_ncp), G_OBJECT(self));
            /* Disconnect ncu signals. */
            nwamui_ncp_foreach_ncu_list(prv->active_ncp, (GFunc)disconnect_nwam_object_signals, (gpointer)self);
            g_object_unref(prv->active_ncp);
        } else {
            /* Initically run. */
            init = TRUE;
        }

        if ((prv->active_ncp = (NwamuiNcp*)ncp) != NULL) {

            g_object_ref(prv->active_ncp);

            connect_nwam_object_signals(G_OBJECT(prv->active_ncp), G_OBJECT(self));
            /* Connect ncu signals. */
            nwamui_ncp_foreach_ncu_list(prv->active_ncp, (GFunc)connect_nwam_object_signals, (gpointer)self);

            nwam_notification_show_ncp_changed( prv->active_ncp );

            /* Init show NCU info. */
            if (init) {
                nwamui_ncp_foreach_ncu_list(prv->active_ncp, (GFunc)initial_notify_nwam_object, (gpointer)self);
            }

            nwam_menu_recreate_ncu_menuitems(self);

            nwam_tooltip_widget_update_daemon(NWAM_TOOLTIP_WIDGET(prv->tooltip_widget), NWAMUI_OBJECT(prv->daemon));

/*             nwam_tooltip_widget_update_ncp(NWAM_TOOLTIP_WIDGET(prv->tooltip_widget), NWAMUI_OBJECT(prv->active_ncp)); */

            nwam_menu_start_update_wifi_timer(self);

            nwam_menu_update_wifi_section(self);
        } else {
            /* Delete all NCUs. */
            prv->cached_menuitem_list[SECTION_NCU] = g_list_concat(nwam_menu_section_delete(NWAM_MENU(prv->menu), SECTION_NCU, TRUE), prv->cached_menuitem_list[SECTION_NCU]);
            /* Should not be happened. We should disable all ncu related menu items here. */
            nwam_menu_section_set_sensitive(NWAM_MENU(prv->menu), SECTION_WIFI_CONTROL, FALSE);
            /* Make sure ref'ed, since menu is a container. */
            ADD_MENU_ITEM(NWAM_MENU(prv->menu), g_object_ref(prv->static_menuitems[MENUITEM_NONCU]));
        }
    }
}

static void
switch_loc_manually_changed(GObject *gobject, GParamSpec *arg1, gpointer data)
{
	NwamStatusIcon *self = NWAM_STATUS_ICON(data);
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);

    g_signal_handlers_block_by_func( (GTK_MENU_ITEM(prv->static_menuitems[MENUITEM_SWITCH_LOC_AUTO])),
      (gpointer)location_model_menuitems, self );
    g_signal_handlers_block_by_func( (GTK_MENU_ITEM(prv->static_menuitems[MENUITEM_SWITCH_LOC_MANUALLY])),
      (gpointer)location_model_menuitems, self );

    if (nwamui_daemon_env_selection_is_manual(prv->daemon)) {
        gtk_menu_item_activate(GTK_MENU_ITEM(prv->static_menuitems[MENUITEM_SWITCH_LOC_MANUALLY]));
    } else {
        gtk_menu_item_activate(GTK_MENU_ITEM(prv->static_menuitems[MENUITEM_SWITCH_LOC_AUTO]));
    }

    g_signal_handlers_unblock_by_func( (GTK_MENU_ITEM(prv->static_menuitems[MENUITEM_SWITCH_LOC_AUTO])),
      (gpointer)location_model_menuitems, self );
    g_signal_handlers_unblock_by_func( (GTK_MENU_ITEM(prv->static_menuitems[MENUITEM_SWITCH_LOC_MANUALLY])),
      (gpointer)location_model_menuitems, self );
}

static void
daemon_active_env_changed(NwamuiDaemon *daemon, GParamSpec *arg1, gpointer data)
{
	NwamStatusIcon *self = NWAM_STATUS_ICON(data);
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);
    gchar *sname;
    gchar *summary, *body;
    NwamuiEnv* env = daemon_status_is_good(prv->daemon) ? nwamui_daemon_get_active_env(prv->daemon) : NULL;

    g_message("flag is %d", nwamui_daemon_env_selection_is_manual(prv->daemon));

    g_signal_handlers_block_by_func( (GTK_MENU_ITEM(prv->static_menuitems[MENUITEM_SWITCH_LOC_AUTO])),
      (gpointer)location_model_menuitems, self );
    g_signal_handlers_block_by_func( (GTK_MENU_ITEM(prv->static_menuitems[MENUITEM_SWITCH_LOC_MANUALLY])),
      (gpointer)location_model_menuitems, self );

    if (nwamui_daemon_env_selection_is_manual(prv->daemon)) {
        gtk_menu_item_activate(GTK_MENU_ITEM(prv->static_menuitems[MENUITEM_SWITCH_LOC_MANUALLY]));
    } else {
        gtk_menu_item_activate(GTK_MENU_ITEM(prv->static_menuitems[MENUITEM_SWITCH_LOC_AUTO]));
    }

    g_signal_handlers_unblock_by_func( (GTK_MENU_ITEM(prv->static_menuitems[MENUITEM_SWITCH_LOC_AUTO])),
      (gpointer)location_model_menuitems, self );
    g_signal_handlers_unblock_by_func( (GTK_MENU_ITEM(prv->static_menuitems[MENUITEM_SWITCH_LOC_MANUALLY])),
      (gpointer)location_model_menuitems, self );

    if (env) {
        
        nwam_notification_show_location_changed( env );

        nwam_menu_recreate_wifi_menuitems (self, FALSE);

        nwam_tooltip_widget_update_env(NWAM_TOOLTIP_WIDGET(prv->tooltip_widget), NWAMUI_OBJECT(env));

        nwam_menu_recreate_env_menuitems(self);
    } else {
        /* Delete all LOCs. */
        prv->cached_menuitem_list[SECTION_LOC] = g_list_concat(nwam_menu_section_delete(NWAM_MENU(prv->menu), SECTION_LOC, TRUE), prv->cached_menuitem_list[SECTION_LOC]);
    }
}

static void
connect_nwam_object_signals(GObject *obj, GObject *self)
{
	GType type = G_OBJECT_TYPE(obj);

	if (type == NWAMUI_TYPE_DAEMON) {
        NwamuiDaemon *daemon = NWAMUI_DAEMON(obj);

        g_signal_connect(daemon, "notify::status",
          G_CALLBACK(daemon_status_changed), (gpointer)self);

        g_signal_connect(daemon, "notify::enm-list",
          G_CALLBACK(daemon_enm_list_changed), (gpointer)self);

        g_signal_connect(daemon, "notify::env-list",
          G_CALLBACK(daemon_env_list_changed), (gpointer)self);

        g_signal_connect(daemon, "daemon_info",
          G_CALLBACK(daemon_info), (gpointer)self);

        g_signal_connect(daemon, "notify::active-ncp",
          G_CALLBACK(daemon_active_ncp_changed), (gpointer) self);

        g_signal_connect(daemon, "notify::active-env",
          G_CALLBACK(daemon_active_env_changed), (gpointer) self);

        g_signal_connect(daemon, "wifi_key_needed",
          G_CALLBACK(daemon_wifi_key_needed), (gpointer)self);

        g_signal_connect(daemon, "wifi_selection_needed",
          G_CALLBACK(daemon_wifi_selection_needed), (gpointer)self);

        g_signal_connect(daemon, "wifi_scan_started",
          (GCallback)nwam_menu_scan_started, (gpointer) self);

        g_signal_connect(daemon, "notify::env-selection-mode",
          G_CALLBACK(switch_loc_manually_changed), (gpointer)self);

        g_signal_connect(daemon, "notify::online-enm-num",
          G_CALLBACK(daemon_online_enm_num_notify), (gpointer)self);

        /* We don't have a fav wifi section in pop menu in
         * Phase 1, so comment out these signals to avoid
         * crash when removing fav wifi in fav wireless tab.
         */
/*         g_signal_connect(daemon, "add_wifi_fav", */
/*           G_CALLBACK(daemon_add_wifi_fav), (gpointer) self); */

/*         g_signal_connect(daemon, "remove_wifi_fav", */
/*           G_CALLBACK(daemon_remove_wifi_fav), (gpointer) self); */

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

        g_signal_connect(ncp, "notify::wireless-link-num",
          G_CALLBACK(on_ncp_notify_many_wireless), (gpointer)self);

	} else if (type == NWAMUI_TYPE_NCU) {
        NwamuiNcu* ncu = NWAMUI_NCU(obj);

        g_signal_connect(ncu, "notify::active",
          G_CALLBACK(ncu_notify_active), (gpointer)self);

/* 	} else if (type == NWAMUI_TYPE_ENV) { */
/* 	} else if (type == NWAMUI_TYPE_ENM) { */
	} else {
		g_error("%s unknown nwamui obj", __func__);
	}
}

static void
disconnect_nwam_object_signals(GObject *obj, GObject *self)
{
	GType type = G_OBJECT_TYPE(obj);

	if (type == NWAMUI_TYPE_DAEMON) {
    } else if (type == NWAMUI_TYPE_NCP) {
	} else if (type == NWAMUI_TYPE_NCU) {
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
initial_notify_nwam_object(GObject *obj, GObject *self)
{
	GType type = G_OBJECT_TYPE(obj);

	if (type == NWAMUI_TYPE_DAEMON) {
        NwamuiDaemon *daemon = NWAMUI_DAEMON(obj);
    } else if (type == NWAMUI_TYPE_NCP) {
        NwamuiNcp* ncp = NWAMUI_NCP(obj);
	} else if (type == NWAMUI_TYPE_NCU) {
        NwamuiNcu* ncu = NWAMUI_NCU(obj);
        /* Only initially notify if NCU is active. */
        if (nwamui_object_get_active(NWAMUI_OBJECT(obj))) {
            g_object_notify(G_OBJECT(ncu), "active");
        }
/* 	} else if (type == NWAMUI_TYPE_ENV) { */
/* 	} else if (type == NWAMUI_TYPE_ENM) { */
	} else {
		g_error("%s unknown nwamui obj", __func__);
	}
}

static gint
ncp_find_enabled_wireless_ncu(gconstpointer a, gconstpointer b)
{
	NwamuiNcu *ncu = NWAMUI_NCU(a);

    if (ncu &&
      (nwamui_ncu_get_ncu_type( ncu ) == NWAMUI_NCU_TYPE_WIRELESS) &&
      nwamui_object_get_enabled(NWAMUI_OBJECT(ncu))) {
        return 0;
    }
    return 1;
}

/**
 * nwam_menu_update_wifi_section:
 * Set visible of the wifi section according to if there is a wireless link
 * and if there is an enabled wireless link.
 */
static void
nwam_menu_update_wifi_section(NwamStatusIcon *self)
{
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);

    if (nwamui_ncp_get_wireless_link_num(prv->active_ncp) > 0) {

        /* Clean section anyway. */
        prv->cached_menuitem_list[SECTION_WIFI] = g_list_concat(nwam_menu_section_delete(NWAM_MENU(prv->menu), SECTION_WIFI, TRUE), prv->cached_menuitem_list[SECTION_WIFI]);

        nwam_menu_section_set_visible(NWAM_MENU(prv->menu), SECTION_WIFI, TRUE);

        if (nwamui_ncp_find_ncu_list(prv->active_ncp, NULL, ncp_find_enabled_wireless_ncu)) {
            /* Show wireless control since we have enabled wireless links. */
            nwam_menu_section_set_visible(NWAM_MENU(prv->menu), SECTION_WIFI_CONTROL, TRUE);
            nwam_menu_section_set_sensitive(NWAM_MENU(prv->menu), SECTION_WIFI_CONTROL, TRUE);
            /* This call will clean SECTION_WIFI. Re-init wlan menu items.  */
            nwamui_daemon_dispatch_wifi_scan_events_from_cache(prv->daemon);
        } else {
            /* Hide wireless control due to no enabled wireless links. */
            nwam_menu_section_set_visible(NWAM_MENU(prv->menu), SECTION_WIFI_CONTROL, FALSE);
            /* Make sure ref'ed, since menu is a container. */
            ADD_MENU_ITEM(prv->menu, g_object_ref(prv->static_menuitems[MENUITEM_NO_ENABLED_WIRELESS]));
        }
    } else {
        nwam_menu_section_set_visible(NWAM_MENU(prv->menu), SECTION_WIFI, FALSE);
        nwam_menu_section_set_visible(NWAM_MENU(prv->menu), SECTION_WIFI_CONTROL, FALSE);
    }
}

/**
 * find_wireless_interface:
 *
 * Return a wireless ncu instance.
 */
static gboolean
find_wireless_interface(GtkTreeModel *model,
  GtkTreePath *path,
  GtkTreeIter *iter,
  gpointer data)
{
    NwamuiNcu     **ncu_p = (NwamuiNcu **)data;
    NwamuiNcu      *ncu;

    gtk_tree_model_get(model, iter, 0, &ncu, -1);
    g_assert(NWAMUI_IS_NCU(ncu));

    if (nwamui_ncu_get_ncu_type(ncu) == NWAMUI_NCU_TYPE_WIRELESS) {
        *ncu_p = ncu;
        return TRUE;
    }
    g_object_unref(ncu);
    return FALSE;
}

static void
set_window_urgency( GtkWindow *window, gboolean urgent, gboolean raise )
{
    if ( window != NULL ) {
        gtk_window_set_urgency_hint( GTK_WINDOW(window), urgent );
        if ( raise ) {
            gtk_window_present(window);
        }
    }
}

static NwamPrefIFace*
get_chooser_dialog(  NwamStatusIcon *self )
{
    NwamStatusIconPrivate  *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);

    if ( prv->chooser_dialog == NULL ) {
        prv->chooser_dialog = NWAM_PREF_IFACE(nwam_wireless_chooser_new());
    }

    return( prv->chooser_dialog );
}

static void
show_wireless_chooser( NwamStatusIcon *self )
{
    NwamPrefIFace          *chooser_dialog;
    NwamStatusIconPrivate  *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);
    GtkWindow              *window = NULL;

    chooser_dialog = get_chooser_dialog( self );

    nwamui_daemon_dispatch_wifi_scan_events_from_cache(prv->daemon );

    window = nwam_pref_dialog_get_window(chooser_dialog);

    if ( window != NULL ) {
        set_window_urgency( window, FALSE, FALSE ); /* Reset urgency flag to FALSE */
        gtk_widget_show_all( GTK_WIDGET(window) );
    }
}

static void
set_wireless_chooser_urgency( NwamStatusIcon *self, gboolean urgent )
{
    NwamPrefIFace          *chooser_dialog;
    NwamStatusIconPrivate  *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);
    GtkWindow              *window = NULL;

    chooser_dialog = get_chooser_dialog( self );

    window = nwam_pref_dialog_get_window(chooser_dialog);

    set_window_urgency( window, urgent, FALSE );
}

static NwamPrefIFace*
get_wifi_dialog(  NwamStatusIcon *self )
{
    NwamStatusIconPrivate  *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);

    if ( prv->wifi_dialog == NULL ) {
        prv->wifi_dialog = NWAM_PREF_IFACE(nwam_wireless_dialog_get_instance());
    }

    return( prv->wifi_dialog );
}

static void
join_wireless(NwamStatusIcon* self, NwamuiWifiNet *wifi, gboolean do_connect )
{
    NwamPrefIFace          *wifi_dialog = get_wifi_dialog( self );
    NwamuiDaemon           *daemon = nwamui_daemon_get_instance();
    NwamuiObject           *ncp = nwamui_daemon_get_active_ncp(daemon);
    NwamuiNcu              *ncu = NULL;
    GtkWindow              *window = NULL;

    if (wifi_dialog == NULL) {
        return;
    }

    /* ncu could be NULL due to daemon may not know the active llp */
    if ( wifi != NULL ) {
        ncu = nwamui_wifi_net_get_ncu(wifi);
    }

    if ( ncu == NULL || nwamui_ncu_get_ncu_type(ncu) != NWAMUI_NCU_TYPE_WIRELESS) {
        /* we need find a wireless interface */
        nwamui_ncp_foreach_ncu(NWAMUI_NCP(ncp),
          (GtkTreeModelForeachFunc)find_wireless_interface,
          (gpointer)&ncu);
    }

    g_object_unref(ncp);
    g_object_unref(daemon);

    if (ncu && nwamui_ncu_get_ncu_type(ncu) == NWAMUI_NCU_TYPE_WIRELESS) {
        nwam_wireless_dialog_set_ncu(NWAM_WIRELESS_DIALOG(wifi_dialog), ncu);
    } else {
        if (ncu) {
            g_object_unref(ncu);
        }
        return;
    }

    /* wifi may be NULL -- join a wireless */
    if (wifi) {
        g_debug("%s ## wifi 0x%p %s", __func__, wifi, nwamui_object_get_name(NWAMUI_OBJECT(wifi)));
    }

    nwam_pref_set_purpose(NWAM_PREF_IFACE(wifi_dialog), NWAMUI_DIALOG_PURPOSE_JOIN );
    nwam_wireless_dialog_set_wifi_net(NWAM_WIRELESS_DIALOG(wifi_dialog), wifi);
    nwam_wireless_dialog_set_do_connect(NWAM_WIRELESS_DIALOG(wifi_dialog), do_connect);

    window = nwam_pref_dialog_get_window(wifi_dialog);

    if ( window != NULL ) {
        set_window_urgency( window, TRUE, TRUE ); /* Reset urgency flag to FALSE */
    }
    
    nwam_pref_dialog_run(NWAM_PREF_IFACE(wifi_dialog), NULL);

    g_object_unref(ncu);
}

static void
set_join_wireless_urgency( NwamStatusIcon *self, gboolean urgent )
{
    NwamPrefIFace          *wifi_dialog;
    NwamStatusIconPrivate  *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);
    GtkWindow              *window = NULL;

    wifi_dialog = get_wifi_dialog( self );

    window = nwam_pref_dialog_get_window(wifi_dialog);

    set_window_urgency( window, urgent, TRUE );
}

static gboolean
daemon_status_is_good(NwamuiDaemon *daemon)
{
    switch(nwamui_daemon_get_status(daemon)) {
    case NWAMUI_DAEMON_STATUS_NEEDS_ATTENTION:
    case NWAMUI_DAEMON_STATUS_ALL_OK:
        return TRUE;
    case NWAMUI_DAEMON_STATUS_ERROR:
    case NWAMUI_DAEMON_STATUS_UNINITIALIZED:
    default:
        break;
    }
    return FALSE;
}

static gboolean
animation_panel_icon_timeout (gpointer user_data)
{
    NwamStatusIcon *self = NWAM_STATUS_ICON(user_data);
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);

	gtk_status_icon_set_from_pixbuf(GTK_STATUS_ICON(self),
      nwamui_util_get_env_status_icon(GTK_STATUS_ICON(self),
		(nwamui_daemon_status_t)(++prv->icon_stock_index)%NWAMUI_DAEMON_STATUS_LAST,
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
          nwamui_util_get_env_status_icon(GTK_STATUS_ICON(self), nwamui_daemon_get_status_icon_type(prv->daemon), 0));
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

    g_assert(prv->menu == NULL);

    g_debug("%s: Hide self firstly!", __func__);
    gtk_status_icon_set_visible(GTK_STATUS_ICON(self), FALSE);

    /* Initializing notification, must run first, because others may invoke
     * it.
     */
    nwam_notification_init(GTK_STATUS_ICON(self));

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

    nwam_status_icon_set_activate_callback(self, (GCallback)status_icon_default_activation, NULL );

    /* Enable tooltip. */
    gtk_status_icon_set_has_tooltip(GTK_STATUS_ICON(self), TRUE);

    /* Handle all daemon signals here */
    connect_nwam_object_signals(G_OBJECT(prv->daemon), G_OBJECT(self));

    /* Initial place. Init code must be here. */
    g_object_notify(G_OBJECT(prv->daemon), "status");
    g_object_notify(G_OBJECT(prv->daemon), "active_ncp");
    g_object_notify(G_OBJECT(prv->daemon), "active_env");
    g_object_notify(G_OBJECT(prv->daemon), "env_list");
    g_object_notify(G_OBJECT(prv->daemon), "enm_list");
}

void
nwam_status_icon_set_activate_callback(NwamStatusIcon *self,
  GCallback activate_cb, gpointer user_data)
{
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);
    if (prv->activate_handler_id > 0) {
        g_signal_handler_disconnect(self, prv->activate_handler_id);
        prv->activate_handler_id = 0;
    }

    if (activate_cb) {
        prv->activate_handler_id = g_signal_connect(self, "activate",
          G_CALLBACK(activate_cb), user_data);
    }
}

void
nwam_status_icon_set_status(NwamStatusIcon *self, NwamuiNcu* wireless_ncu )
{
    NwamStatusIconPrivate *prv        = NWAM_STATUS_ICON_GET_PRIVATE(self);
    gint                   env_status = nwamui_daemon_get_status_icon_type( prv->daemon );

    prv->current_status = env_status;

    /* nwam_notification_set_default_icon(nwamui_util_get_env_status_icon(GTK_STATUS_ICON(self), env_status, 48)); */

/*     g_debug("%s: env_status = %d, wireless_ncu = %08X",  __func__, env_status, wireless_ncu ); */

    /* We need overall status icon here instead of a specific ncu status icon. */
    gtk_status_icon_set_from_pixbuf(GTK_STATUS_ICON(self),
      nwamui_util_get_env_status_icon(GTK_STATUS_ICON(self), env_status, 0));

/*     if ( wireless_ncu == NULL || nwamui_ncu_get_ncu_type(wireless_ncu) != NWAMUI_NCU_TYPE_WIRELESS) { */
/*         gtk_status_icon_set_from_pixbuf(GTK_STATUS_ICON(self), */
/*           nwamui_util_get_env_status_icon(GTK_STATUS_ICON(self), env_status, 0)); */
/*     } */
/*     else { */
/*         gtk_status_icon_set_from_pixbuf(GTK_STATUS_ICON(self),  */
/*           nwamui_util_get_ncu_status_icon( wireless_ncu )); */
/*     } */

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
	}
}

static gboolean
status_icon_size_changed(GtkStatusIcon *status_icon, gint size, gpointer user_data)
{
/*     NwamStatusIcon *self = NWAM_STATUS_ICON(status_icon); */
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(status_icon);

    nwam_status_icon_set_status(NWAM_STATUS_ICON(status_icon), NULL);

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

}

static void
ncp_deactivate_ncu (NwamuiNcp *ncp, NwamuiNcu* ncu, gpointer user_data)
{
    NwamStatusIcon *sicon = NWAM_STATUS_ICON(user_data);

}

static void
location_model_menuitems(GtkMenuItem *menuitem, gpointer user_data)
{
    NwamStatusIcon        *self        = NWAM_STATUS_ICON(user_data);
    NwamStatusIconPrivate *prv         = NWAM_STATUS_ICON_GET_PRIVATE(self);
    gint                   menuitem_id = (gint)g_object_get_data(G_OBJECT(menuitem), STATIC_MENUITEM_ID);
    gboolean                active     = gtk_check_menu_item_get_active( GTK_CHECK_MENU_ITEM(menuitem) );

    switch (menuitem_id) {
    case MENUITEM_SWITCH_LOC_MANUALLY:
        if ( active ) {
            NwamuiEnv *env = nwamui_daemon_get_active_env(prv->daemon);
            nwamui_daemon_env_selection_set_manual(prv->daemon, TRUE, env);
            g_object_unref(env);
        }
        break;
    case MENUITEM_SWITCH_LOC_AUTO:
        if ( active ) {
            nwamui_daemon_env_selection_set_manual(prv->daemon, FALSE, NULL);
        }
        break;
    default:
        g_assert_not_reached();
    }
}

static void
on_activate_static_menuitems (GtkMenuItem *menuitem, gpointer user_data)
{
    NwamStatusIcon *self = NWAM_STATUS_ICON(user_data);
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);
    const gchar *argv[3] = { NULL, NULL, NULL };
    gint menuitem_id = (gint)g_object_get_data(G_OBJECT(menuitem), STATIC_MENUITEM_ID);

    switch (menuitem_id) {
    case MENUITEM_ENV_PREF:
		argv[0] = NWAMUI_CAPPLET_OPT_LOCATION_DIALOG_STR;
        break;
    case MENUITEM_CONN_PROF:
		argv[0] = NWAMUI_CAPPLET_OPT_NET_PREF_CONFIG_STR;
        break;
    case MENUITEM_REFRESH_WLAN:
        nwam_menu_recreate_wifi_menuitems(self, TRUE);
        break;
    case MENUITEM_JOIN_WLAN:
		join_wireless( self, NULL, TRUE);
        break;
    case MENUITEM_VPN_PREF:
        argv[0] = NWAMUI_CAPPLET_OPT_VPN_PREF_DIALOG_STR;
        break;
    case MENUITEM_NET_PREF:
		argv[0] = NWAMUI_CAPPLET_OPT_NET_PREF_DIALOG_STR;
        break;
    case MENUITEM_HELP:
        nwamui_util_show_help (HELP_REF_NWSTATUS_ICON);
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
    if (argv[0]) {
        nwam_exec(argv);
    }
}

static void
activate_test_menuitems(GtkMenuItem *menuitem, gpointer user_data)
{
    NwamStatusIcon *self = NWAM_STATUS_ICON(user_data);
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);
    gint menuitem_id = (gint)g_object_get_data(G_OBJECT(menuitem), STATIC_MENUITEM_ID);

    switch (menuitem_id) {
    case MENUITEM_TEST: {
        static gboolean flag = FALSE;
        if (flag = !flag) {
/*             nwam_menu_section_delete(NWAM_MENU(prv->menu), SECTION_LOC); */
/*             nwam_menu_section_set_visible(NWAM_MENU(prv->menu), SECTION_NCU, TRUE); */
/*             daemon_status_changed(prv->daemon, NWAMUI_DAEMON_STATUS_ALL_OK, (gpointer)self); */
        } else {
/*             nwam_menu_recreate_env_menuitems(self); */
/*             nwam_menu_section_set_visible(NWAM_MENU(prv->menu), SECTION_NCU, FALSE); */
/*             daemon_status_changed(prv->daemon, NWAMUI_DAEMON_STATUS_ERROR, (gpointer)self); */
        }
        /* Test - must enable pop up menu. */
        prv->enable_pop_up_menu = TRUE;
    }
        break;
    default:
        break;
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

    /* It is required only is the ncp has at least one active wireless link. */
    if (nwamui_ncp_get_wireless_link_num(prv->active_ncp) > 0) {
        ncu_list = nwamui_ncp_get_ncu_list(prv->active_ncp);

        while (ncu_list) {
            ncu = NWAMUI_NCU(ncu_list->data);
            ncu_list = g_list_delete_link(ncu_list, ncu_list);

            switch(nwamui_ncu_get_ncu_type(ncu)) {
#ifdef TUNNEL_SUPPORT
            case NWAMUI_NCU_TYPE_TUNNEL:
#endif /* TUNNEL_SUPPORT */
            case NWAMUI_NCU_TYPE_WIRED:
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
                    nwam_status_icon_set_status(self, ncu );
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
/*     NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self); */
/*     gboolean wireless_num; */

/*     g_assert(NWAMUI_IS_NCP(gobject)); */

/*     wireless_num = nwamui_ncp_get_wireless_link_num(NWAMUI_NCP(gobject)); */
/*     if (wireless_num > 0) { */
/*         nwam_menu_section_foreach(NWAM_MENU(prv->menu), SECTION_WIFI, */
/*           (GFunc)nwam_obj_proxy_refresh, NULL); */
/*     } else { */
/*         nwam_menu_section_delete(NWAM_MENU(prv->menu), SECTION_WIFI); */
/*     } */
/*     nwam_menu_section_set_sensitive(NWAM_MENU(prv->menu), SECTION_WIFI_CONTROL, wireless_num > 0); */
    nwam_menu_update_wifi_section(self);
}

static void
notifyaction_popup_menus(NotifyNotification *n,
  gchar *action,
  gpointer user_data)
{
    nwam_status_icon_show_menu(NWAM_STATUS_ICON(user_data));
}

static void notifyaction_join_wireless(NotifyNotification *n,
  gchar *action,
  gpointer user_data)
{
    join_wireless(NWAM_STATUS_ICON(user_data), NULL, TRUE);
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
    prv->needs_wifi_selection_seen = FALSE;
    prv->needs_wifi_key = NULL;
    prv->prof = nwamui_prof_get_instance ();
	prv->daemon = nwamui_daemon_get_instance ();

    /* nwamui preference signals */
    g_signal_connect(prv->prof, "notify::join-wifi-not-in-fav",
      G_CALLBACK(join_wifi_not_in_fav), (gpointer) self);
    g_signal_connect(prv->prof, "notify::join-any-fav-wifi",
      G_CALLBACK(join_any_fav_wifi), (gpointer) self);
    g_signal_connect(prv->prof, "notify::add-any-new-wifi-to-fav",
      G_CALLBACK(add_any_new_wifi_to_fav), (gpointer) self);
    g_signal_connect(prv->prof, "notify::action-on-no-fav-networks",
      G_CALLBACK(action_on_no_fav_networks), (gpointer) self);

    g_object_get (prv->prof,
      "action_on_no_fav_networks", &prof_action_if_no_fav_networks,
      "join_wifi_not_in_fav", &prof_ask_join_open_network,
      "join_any_fav_wifi", &prof_ask_join_fav_network,
      "add_any_new_wifi_to_fav", &prof_ask_add_to_fav,
      NULL);
}

static void
nwam_status_icon_finalize (NwamStatusIcon *self)
{
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);

    nwam_menu_stop_update_wifi_timer(self);

    disconnect_nwam_object_signals(G_OBJECT(prv->daemon), G_OBJECT(self));

    nwam_notification_cleanup();

    if (prv->animation_icon_update_timeout_id > 0) {
		g_source_remove (prv->animation_icon_update_timeout_id);
		prv->animation_icon_update_timeout_id = 0;
/* 		/\* reset everything of animation_panel_icon here *\/ */
/* 		prv->icon_stock_index = 0; */
    }

    g_object_unref(prv->menu);
    g_object_unref(prv->tooltip_widget);
	g_object_unref(prv->daemon);
    g_object_unref(prv->prof);
	G_OBJECT_CLASS(nwam_status_icon_parent_class)->finalize(G_OBJECT(self));
}

static void
nwam_menu_recreate_wifi_menuitems (NwamStatusIcon *self, gboolean force_scan )
{
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);

/*     nwam_menu_section_delete(NWAM_MENU(prv->menu), SECTION_WIFI); */

    if ( force_scan ) {
        nwamui_daemon_wifi_start_scan(prv->daemon);
    }
    else {
        nwamui_daemon_dispatch_wifi_scan_events_from_cache(prv->daemon );
    }
}

static void
nwam_menu_recreate_ncu_menuitems (NwamStatusIcon *self)
{
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);
    GList *ncu_list = nwamui_ncp_get_ncu_list(prv->active_ncp);
    GList *idx = NULL;
	GtkWidget* item = NULL;
    gboolean has_wireless_inf = FALSE;

    prv->cached_menuitem_list[SECTION_NCU] = g_list_concat(nwam_menu_section_delete(NWAM_MENU(prv->menu), SECTION_NCU, TRUE), prv->cached_menuitem_list[SECTION_NCU]);

    if (ncu_list) {
        for (idx = ncu_list; idx; idx = g_list_next(idx)) {
            //nwam_menuitem_proxy_new(action, ncu_group);
            item = nwam_status_icon_create_menu_item(self, NWAMUI_OBJECT(idx->data));
            g_object_unref(idx->data);
        }
        g_list_free(ncu_list);

	} else {
        /* Make sure ref'ed, since menu is a container. */
        ADD_MENU_ITEM(NWAM_MENU(prv->menu), g_object_ref(prv->static_menuitems[MENUITEM_NONCU]));
    }

    nwam_menu_update_wifi_section(self);
}

static void
nwam_menu_recreate_env_menuitems (NwamStatusIcon *self)
{
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(self);
    GList *env_list = nwamui_daemon_get_env_list(prv->daemon);
    GList *idx = NULL;
	GtkWidget* item = NULL;

    prv->cached_menuitem_list[SECTION_LOC] = g_list_concat(nwam_menu_section_delete(NWAM_MENU(prv->menu), SECTION_LOC, TRUE), prv->cached_menuitem_list[SECTION_LOC]);

	for (idx = env_list; idx; idx = g_list_next(idx)) {
        //nwam_menuitem_proxy_new(action, env_group);
        item = nwam_status_icon_create_menu_item(self, NWAMUI_OBJECT(idx->data));
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

    prv->cached_menuitem_list[SECTION_ENM] = g_list_concat(nwam_menu_section_delete(NWAM_MENU(prv->menu), SECTION_ENM, TRUE), prv->cached_menuitem_list[SECTION_ENM]);

	for (idx = enm_list; idx; idx = g_list_next(idx)) {
        //nwam_menuitem_proxy_new(action, vpn_group);
        item = nwam_status_icon_create_menu_item(self, NWAMUI_OBJECT(idx->data));
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
      (gpointer)g_object_ref(self),
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

    {
        GSList *group = NULL;
        menu_append_item(sub_menu,
          GTK_TYPE_RADIO_MENU_ITEM, _("Switch Locations Au_tomatically"),
          location_model_menuitems, self);
        CACHE_STATIC_MENUITEMS(self, MENUITEM_SWITCH_LOC_AUTO);
        gtk_radio_menu_item_set_group(GTK_RADIO_MENU_ITEM(menuitem), group);
        group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(menuitem));
        menu_append_item(sub_menu,
          GTK_TYPE_RADIO_MENU_ITEM, _("Switch Locations _Manually"),
          location_model_menuitems, self);
        CACHE_STATIC_MENUITEMS(self, MENUITEM_SWITCH_LOC_MANUALLY);
        gtk_radio_menu_item_set_group(GTK_RADIO_MENU_ITEM(menuitem), group);
    }

    menu_append_separator(sub_menu);

    menu_append_item(sub_menu,
      GTK_TYPE_MENU_ITEM, _("_Network Locations..."),
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

    START_MENU_SECTION_SEPARATOR(root_menu, SECTION_WIFI_CONTROL, TRUE);
    menu_append_item_with_code(root_menu,
      GTK_TYPE_MENU_ITEM, _("_Refresh Wireless Networks"),
      on_activate_static_menuitems, self,
      CACHE_STATIC_MENUITEMS(self, MENUITEM_REFRESH_WLAN));

    menu_append_item_with_code(root_menu,
      GTK_TYPE_MENU_ITEM, _("_Join Unlisted Wireless Network..."),
      on_activate_static_menuitems, self,
      CACHE_STATIC_MENUITEMS(self, MENUITEM_JOIN_WLAN));

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
    if (nwamui_util_is_debug_mode()) {
        int i;
        for (i = MENUITEM_TEST; i < N_STATIC_MENUITEMS; i++) {
            menu_append_item(root_menu,
              GTK_TYPE_MENU_ITEM, "TeSt MeNu",
              activate_test_menuitems, self);
            CACHE_STATIC_MENUITEMS(self, i);
        }
    }

    /* Other dynamical menuitems */
    menuitem = gtk_menu_item_new_with_label(NONCU);
    gtk_widget_set_sensitive(menuitem, FALSE);
    CACHE_STATIC_MENUITEMS(self, MENUITEM_NONCU);

    menuitem = gtk_menu_item_new_with_label(NO_ENABLED_WIRELESS);
    gtk_widget_set_sensitive(menuitem, FALSE);
    CACHE_STATIC_MENUITEMS(self, MENUITEM_NO_ENABLED_WIRELESS);

#undef ITEM_VARNAME
#undef CACHE_STATIC_MENUITEMS
}

static GtkWidget*
nwam_status_icon_create_menu_item(NwamStatusIcon *self, NwamuiObject *object)
{
    NwamStatusIconPrivate *prv  = NWAM_STATUS_ICON_GET_PRIVATE(self);
    GType                  type = G_OBJECT_TYPE(object);
    GtkWidget             *item = NULL;
    gint                   sec_id;
    GtkWidget *(*nwam_menu_item_new_func)(NwamuiObject *);


/*     if (nwamui_util_is_debug_mode()) { */
/*         gchar *name = nwamui_object_get_name(object); */
/*         g_debug("%s %s", __func__, name); */
/*         g_free(name); */
/*     } */

    if (type == NWAMUI_TYPE_NCU) {
        sec_id = SECTION_NCU;
        nwam_menu_item_new_func = nwam_ncu_item_new;
    } else if (type == NWAMUI_TYPE_WIFI_NET) {
        sec_id = SECTION_WIFI;
        nwam_menu_item_new_func = nwam_wifi_item_new;
	} else if (type == NWAMUI_TYPE_ENV) {
        sec_id = SECTION_LOC;
        nwam_menu_item_new_func = nwam_env_item_new;
	} else if (type == NWAMUI_TYPE_ENM) {
        sec_id = SECTION_ENM;
        nwam_menu_item_new_func = nwam_enm_item_new;
	} else {
        sec_id = SECTION_GLOBAL;
        nwam_menu_item_new_func = NULL;
	}

    if (prv->cached_menuitem_list[sec_id]) {
        /* Reuse cached widget. */
        item = GTK_WIDGET(prv->cached_menuitem_list[sec_id]->data);
        prv->cached_menuitem_list[sec_id] = g_list_delete_link(prv->cached_menuitem_list[sec_id], prv->cached_menuitem_list[sec_id]);
        nwam_menu_item_set_proxy(NWAM_MENU_ITEM(item), G_OBJECT(object));
    } else if (sec_id > SECTION_GLOBAL && sec_id < SECTION_WIFI_CONTROL) {
        item = nwam_menu_item_new_func(NWAMUI_OBJECT(object));
    } else {
        g_error("%s unknown nwamui object", __func__);
    }

    ADD_MENU_ITEM(NWAM_MENU(prv->menu), item);
    return item;
}

static void
nwam_status_icon_delete_menu_item(NwamStatusIcon *self, NwamuiObject *object)
{
    NwamStatusIconPrivate *prv  = NWAM_STATUS_ICON_GET_PRIVATE(self);
    GType                  type = G_OBJECT_TYPE(object);
    GtkWidget             *item = NULL;
    gint                   sec_id;

/*     if (nwamui_util_is_debug_mode()) { */
/*         gchar *name = nwamui_object_get_name(object); */
/*         g_debug("%s %s", __func__, name); */
/*         g_free(name); */
/*     } */

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

    item = nwam_menu_section_get_item_by_proxy(NWAM_MENU(prv->menu), sec_id, G_OBJECT(object));
    if (item) {
        g_object_ref(item);
        REMOVE_MENU_ITEM(NWAM_MENU(prv->menu), item);
        prv->cached_menuitem_list[sec_id] = g_list_prepend(prv->cached_menuitem_list[sec_id], item);
    }
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
    } else if (child == (gpointer)prv->static_menuitems[MENUITEM_NO_ENABLED_WIRELESS]) {
        *index = SECTION_WIFI;
    } else if (child == (gpointer)prv->static_menuitems[MENUITEM_REFRESH_WLAN] ||
      child == (gpointer)prv->static_menuitems[MENUITEM_JOIN_WLAN]) {
        *index = SECTION_WIFI_CONTROL;
    } else if (child == (gpointer)prv->static_menuitems[MENUITEM_NONCU]) {
        *index = SECTION_NCU;
	} else {
        *index = -1;
	}
}

void
nwam_exec (const gchar **nwam_arg)
{
	GError *error = NULL;
	gchar **argv = NULL;
	gint argc = 0;

    argv = g_malloc(sizeof(gchar *) * (g_strv_length((gchar**)nwam_arg) + 2));
	argv[argc++] = g_find_program_in_path (NWAM_MANAGER_PROPERTIES);
	/* Seems to be Solaris specific to specify argvv[0] */
	if (argv[0] == NULL) {
		gchar *base = NULL;
		base = g_path_get_dirname (getexecname());
		argv[0] = g_build_filename (base, NWAM_MANAGER_PROPERTIES, NULL);
		g_free (base);
	}
    for (;nwam_arg[argc - 1]; argc++) {
        argv[argc] = (char*)nwam_arg[argc - 1];
    }
    argv[argc] = NULL;

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
	g_free (argv);
}

static void
join_wifi_not_in_fav(GObject *gobject, GParamSpec *arg1, gpointer data)
{
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(data);
    gchar *body;
    
    g_object_get(gobject, "join_wifi_not_in_fav", &prof_ask_join_open_network, NULL);
    body = g_strdup_printf ("prof_ask_join_open_network = %d", prof_ask_join_open_network);
/*     nwam_notification_show_message("Prof signal", body, NULL, NOTIFY_EXPIRES_DEFAULT); */
    g_free (body);
}

static void
join_any_fav_wifi(GObject *gobject, GParamSpec *arg1, gpointer data)
{
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(data);
    gchar *body;

    g_object_get(gobject, "join_any_fav_wifi", &prof_ask_join_fav_network, NULL);
    body = g_strdup_printf ("prof_ask_join_fav_network = %d", prof_ask_join_fav_network);
/*     nwam_notification_show_message("Prof signal", body, NULL, NOTIFY_EXPIRES_DEFAULT); */
    g_free (body);
}

static void
add_any_new_wifi_to_fav(GObject *gobject, GParamSpec *arg1, gpointer data)
{
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(data);
    gchar *body;

    g_object_get(gobject, "add_any_new_wifi_to_fav", &prof_ask_add_to_fav, NULL);
    body = g_strdup_printf ("prof_ask_add_to_fav = %d", prof_ask_add_to_fav);
/*     nwam_notification_show_message("Prof signal", body, NULL, NOTIFY_EXPIRES_DEFAULT); */
    g_free (body);
}

static void
action_on_no_fav_networks(GObject *gobject, GParamSpec *arg1, gpointer data)
{
    NwamStatusIconPrivate *prv = NWAM_STATUS_ICON_GET_PRIVATE(data);
    gchar *body;

    g_object_get(gobject, "action_on_no_fav_networks", &prof_action_if_no_fav_networks, NULL);
    body = g_strdup_printf ("prof_action_if_no_fav_networks = %d", prof_action_if_no_fav_networks);
/*     nwam_notification_show_message("Prof signal", body, NULL, NOTIFY_EXPIRES_DEFAULT); */
    g_free (body);
}

