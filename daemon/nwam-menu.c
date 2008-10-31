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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>
#include <stdlib.h>

#include "notify.h"
#include "nwam-menu.h"
#include "nwam-wifi-group.h"
#include "nwam-env-group.h"
#include "nwam-enm-group.h"
#include "libnwamui.h"

#define DEBUG()	g_debug ("[[ %20s : %-4d ]]", __func__, __LINE__)

/* Pop-up menu */
#define NWAM_MANAGER_PROPERTIES "nwam-manager-properties"
#define	NONCU	"No network interfaces detected"
#define FALSENCU	"automatic"

static NwamMenu *instance = NULL;

#define NWAMUI_ACTION_ROOT	"StatusIconMenuActions"
#define NWAMUI_ROOT "StatusIconMenu"
#define NWAMUI_PROOT "/"NWAMUI_ROOT
#define MENUITEM_SECTION_FAV_AP_BEGIN	NWAMUI_PROOT"/section_fav_ap_begin"
#define MENUITEM_SECTION_FAV_AP_END	NWAMUI_PROOT"/section_fav_ap_end"
#define MENUITEM_SECTION_VPN_END	NWAMUI_PROOT"/vpn/section_vpn_end"
#define MENUITEM_NAME_ENV_PREF "0_location_pref"
#define MENUITEM_NAME_JOIN_WIRELESS "1_join_wireless"
#define MENUITEM_NAME_NET_PREF "2_network_pref"
#define MENUITEM_NAME_VPN_PREF "3_vpn_pref"
#define MENUITEM_NAME_HELP "4_help"

enum {
	ID_GLOBAL = 0,
	ID_WIFI,
	ID_FAV_WIFI,
	ID_ENV,
	ID_VPN,
	N_ID
};

static const gchar * dynamic_menuitem_inserting_path[N_ID] = {
    NULL,			/* ID_GLOBAL, shouldn't be used */
    MENUITEM_SECTION_FAV_AP_END,			/* ID_WIFI */
	NWAMUI_PROOT"/addfavor",	/* ID_FAV_WIFI */
};

static const gchar * dynamic_menuitem_path[N_ID] = {
    NWAMUI_ROOT,			/* ID_GLOBAL */
    NWAMUI_ROOT,			/* ID_WIFI */
	NWAMUI_ROOT"/addfavor",	/* ID_FAV_WIFI */
};

struct _NwamMenuPrivate {
	GtkUIManager *ui_manager;
	NwamuiDaemon *daemon;
	guint uid[N_ID];
	GtkActionGroup *group[N_ID];

	gboolean has_wifi;
	gboolean force_wifi_rescan_due_to_env_changed;
	gboolean force_wifi_rescan;
	gboolean change_from_daemon;
};

static void nwam_menu_finalize (NwamMenu *self);
static void nwam_menu_create_static_menuitems (NwamMenu *self);
static void nwam_menu_group_recreate(NwamMenu *self, int group_id,
  const gchar *group_name);

static void nwam_menu_connect (NwamMenu *self);
/* 0.5 */
static void nwam_menu_delete_ncu_menuitems(NwamMenu *self);
static void nwam_menu_recreate_ncu_menuitems (NwamMenu *self);
static void nwam_menu_create_ncu_menuitems (GObject *daemon, GObject *ncp, GObject *ncu, gpointer data);
/* 1.0 */
static void nwam_menu_delete_wifi_menuitems(NwamMenu *self);
static void nwam_menu_recreate_wifi_menuitems (NwamMenu *self);
static void nwam_menu_create_wifi_menuitems (GObject *daemon, GObject *wifi, gpointer data);
static void nwam_menu_create_env_menuitems (NwamMenu *self);
static void nwam_menu_create_vpn_menuitems (NwamMenu *self);

static void notification_listwireless_cb (NotifyNotification *n, gchar *action, gpointer data);

/* utils */
static void menus_set_automatic_label(NwamMenu *self, NwamuiNcu *ncu);

static GtkWidget* menus_get_menuitem(NwamMenu *self,
  int path_id,
  const gchar *menu_name);

static void menus_set_action_visible(NwamMenu *self,
  gint group_id,
  const gchar* action_name,
  gboolean visible);

static void menus_set_action_sensitive(NwamMenu *self,
  gint group_id,
  const gchar* action_name,
  gboolean sensitive);

static void menus_set_toggle_action_active(NwamMenu *self,
  gint group_id,
  const gchar* action_name,
  gboolean active);


/* call back */
/* Notification menuitem events */
static void on_activate_static_menuitems (GtkAction *action, gpointer udata);

/* NWAMUI object events */
static void on_nwam_ncu_toggled (GtkAction *action, gpointer data);
static void on_nwam_ncu_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);

/* nwamui daemon signals */
static void connect_daemon_signals(GObject *self, NwamuiDaemon *daemon);
static void disconnect_daemon_signals(GObject *self, NwamuiDaemon *daemon);
static void event_daemon_status_changed(NwamuiDaemon *daemon, nwamui_daemon_status_t status, gpointer user_data);
static void event_daemon_info (NwamuiDaemon* daemon, gint type, GObject *obj, gpointer data, gpointer user_data);
static void event_ncu_up (NwamuiDaemon* daemon, NwamuiNcu* ncu, gpointer data);
static void event_ncu_down (NwamuiDaemon* daemon, NwamuiNcu* ncu, gpointer data);
static void event_active_env_changed (NwamuiDaemon* daemon, NwamuiEnv* env, gpointer data);
static void event_add_wifi_fav (NwamuiDaemon* daemon, NwamuiWifiNet* wifi, gpointer data);

/* nwamui ncp signals */
static void connect_ncp_signals(GObject *self, NwamuiNcp *ncp);
static void disconnect_ncp_signals(GObject *self, NwamuiNcp *ncp);

G_DEFINE_TYPE (NwamMenu, nwam_menu, G_TYPE_OBJECT)

static GtkActionEntry entries[] = {
    { MENUITEM_NAME_ENV_PREF,
      NULL, N_("Location Preferences"), 
      NULL, NULL,
      G_CALLBACK(on_activate_static_menuitems) },
    { MENUITEM_NAME_JOIN_WIRELESS,
      NULL, N_("_Join Unlisted Wireless Network..."),
      NULL, NULL,
      G_CALLBACK(on_activate_static_menuitems) },
    { MENUITEM_NAME_NET_PREF,
      NULL, N_("Network Preferences"),
      NULL, NULL,
      G_CALLBACK(on_activate_static_menuitems) },
    { MENUITEM_NAME_VPN_PREF,
      NULL, N_("VPN Preferences"),
      NULL, NULL,
      G_CALLBACK(on_activate_static_menuitems) },
    { "swtchenv",
      NULL, N_("Switch Location"),
      NULL, NULL, NULL },
    { "vpn",
      NULL, N_("VPN"),
      NULL, NULL, NULL },
    { "help",
      GTK_STOCK_HELP, N_("Help"),
      NULL, NULL,
      G_CALLBACK(on_activate_static_menuitems) }
};

static void
nwam_menu_class_init (NwamMenuClass *klass)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->finalize = (void (*)(GObject*)) nwam_menu_finalize;
}

static void
nwam_menu_init (NwamMenu *self)
{
	GError *error = NULL;

	self->prv = g_new0 (NwamMenuPrivate, 1);
	
	/* ref daemon instance */
	self->prv->daemon = nwamui_daemon_get_instance ();

    /* handle all daemon signals here */
    g_signal_connect(self->prv->daemon, "daemon_info",
      G_CALLBACK(event_daemon_info), (gpointer) self);
	g_signal_connect((gpointer) self->prv->daemon, "wifi_scan_result",
      (GCallback)nwam_menu_create_wifi_menuitems, (gpointer) self);
    g_signal_connect(self->prv->daemon, "ncu_up",
      G_CALLBACK(event_ncu_up), (gpointer) self);
    g_signal_connect(self->prv->daemon, "ncu_down",
      G_CALLBACK(event_ncu_down), (gpointer) self);
    g_signal_connect(self->prv->daemon, "active_env_changed",
      G_CALLBACK(event_active_env_changed), (gpointer) self);
    g_signal_connect(self->prv->daemon, "add_wifi_fav",
      G_CALLBACK(event_add_wifi_fav), (gpointer) self);

	self->prv->group[ID_GLOBAL] = gtk_action_group_new (NWAMUI_ACTION_ROOT);
	gtk_action_group_set_translation_domain (self->prv->group[ID_GLOBAL], NULL);

    connect_daemon_signals(G_OBJECT(self), self->prv->daemon);

    {
        NwamuiNcp *ncp = nwamui_daemon_get_active_ncp(self->prv->daemon);

        connect_ncp_signals(G_OBJECT(self), ncp);

        g_object_unref(ncp);
    }

	self->prv->ui_manager = gtk_ui_manager_new ();

    self->prv->change_from_daemon = FALSE;

    nwam_menu_create_static_menuitems (self);
	nwam_menu_create_env_menuitems (self);
	nwam_menu_create_vpn_menuitems (self);
    /* test code */
    nwam_menu_recreate_wifi_menuitems (self);

    /* initially populate network info */
    event_daemon_status_changed(self->prv->daemon,
      NWAMUI_DAEMON_STATUS_ACTIVE,
      (gpointer) self);
    nwam_menu_connect (self);
}

static void
nwam_menu_finalize (NwamMenu *self)
{
	g_object_unref (G_OBJECT(self->prv->ui_manager));
	g_object_unref (G_OBJECT(self->prv->daemon));
    {
        NwamuiNcp *ncp = nwamui_daemon_get_active_ncp(self->prv->daemon);

        disconnect_ncp_signals(G_OBJECT(self), ncp);

        g_object_unref(ncp);
    }

	/* TODO, release all the resources */

	g_free(self->prv);
	self->prv = NULL;
	
	G_OBJECT_CLASS(nwam_menu_parent_class)->finalize(G_OBJECT(self));
}

static void
menus_set_automatic_label(NwamMenu *self, NwamuiNcu *ncu)
{
    gchar *automatic_label;
    gchar *type_str;

    if ( ncu != NULL ) {
        char* active_interface = nwamui_ncu_get_device_name( ncu );
		if ( nwamui_ncu_get_ncu_type(NWAMUI_NCU(ncu)) == NWAMUI_NCU_TYPE_WIRELESS ){
            type_str = _("Wireless");
        }
        else {
            type_str = _("Wired");
        }
        automatic_label = g_strdup_printf(_("_Automatic Network Interface Selection - Current: %s (%s)"), type_str, active_interface);
        g_free(active_interface);
    } 
    else {
        automatic_label = g_strdup_printf(_("_Automatic Network Interface Selection "));
    }

#if 0
    {
        GtkAction *action = menu_group_get_item_by_name(self, ID_NCU, FALSENCU);
        g_object_set(action,
          "label", automatic_label,
          NULL);
    }
#endif
    g_free(automatic_label);
}

static GtkWidget*
menus_get_menuitem(NwamMenu *self, int path_id, const gchar *menu_name)
{
    gchar *path = NULL;
    GtkWidget *menuitem = NULL;

    path = g_build_path("/", "/", dynamic_menuitem_path[path_id], menu_name, NULL);

    menuitem = gtk_ui_manager_get_widget(self->prv->ui_manager, path);

    g_free (path);
    return menuitem;
}

static void
menus_set_action_visible(NwamMenu *self, gint group_id,
  const gchar* action_name,
  gboolean visible)
{
    GtkAction *action = menu_group_get_item_by_name(self, group_id, action_name);

    if (action) {
        gtk_action_set_visible(action, visible);
    }
}

static void
menus_set_action_sensitive(NwamMenu *self, gint group_id,
  const gchar* action_name,
  gboolean sensitive)
{
    GtkAction *action = menu_group_get_item_by_name(self, group_id, action_name);

    if (action) {
        gtk_action_set_sensitive(action, sensitive);
    }
}

static void
menus_set_toggle_action_active(NwamMenu *self,
  gint group_id,
  const gchar* action_name,
  gboolean active)
{
    GtkAction *action = menu_group_get_item_by_name(self, group_id, action_name);

    if (action) {
        g_return_if_fail(GTK_IS_TOGGLE_ACTION(action));
        self->prv->change_from_daemon = TRUE;
        gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action), active);
        self->prv->change_from_daemon = FALSE;
    }
}

static void
nwam_menu_group_recreate(NwamMenu *self,
  int group_id,
  const gchar *group_name)
{
    NwamMenuPrivate *prv = self->prv;

	if (prv->uid[group_id] > 0) {
		gtk_ui_manager_remove_ui (prv->ui_manager, prv->uid[group_id]);
		gtk_ui_manager_remove_action_group (prv->ui_manager, prv->group[group_id]);
	}
	prv->uid[group_id] = gtk_ui_manager_new_merge_id (prv->ui_manager);
	g_assert (prv->uid[group_id] > 0);
	prv->group[group_id] = gtk_action_group_new (group_name);
	gtk_action_group_set_translation_domain (prv->group[group_id], NULL);
	gtk_ui_manager_insert_action_group (prv->ui_manager, prv->group[group_id], 0);
    g_object_unref (G_OBJECT(prv->group[group_id]));
}

static void
nwam_menu_delete_wifi_menuitems(NwamMenu *self)
{
	self->prv->has_wifi = FALSE;
    g_object_unref (G_OBJECT(self->prv->group[ID_WIFI]));
    self->prv->group[ID_WIFI] = NULL;
}

static void
nwam_menu_recreate_wifi_menuitems (NwamMenu *self)
{
    GtkAction *action;

    nwam_menu_delete_wifi_menuitems(self);

    DEBUG();	
    self->prv->group[ID_WIFI] = nwam_wifi_group_new(self->prv->ui_manager,
      dynamic_menuitem_inserting_path[ID_WIFI]);

    /* set force rescan flag so that we can identify daemon scan wifi event */
    self->prv->force_wifi_rescan = TRUE;
}

static void
nwam_menu_delete_ncu_menuitems(NwamMenu *self)
{
#if 0
    nwam_menu_group_recreate(self, ID_NCU, "ncu_action_group");
#endif
    /* disable "join wireless" menu item */
    /* disable "manage known ap" menu item */
    menus_set_action_visible(self, ID_GLOBAL, MENUITEM_NAME_JOIN_WIRELESS, FALSE);
}

static void
nwam_menu_recreate_ncu_menuitems (NwamMenu *self)
{
#if 0
    NwamuiNcp* active_ncp = nwamui_daemon_get_active_ncp(self->prv->daemon);
    GtkTreeModel *model = NULL;
    GtkTreeIter iter;
    GtkAction *action;
    nwamui_ncp_selection_mode_t selection_mode = nwamui_ncp_get_selection_mode( active_ncp );

    DEBUG();

    nwam_menu_delete_ncu_menuitems(self);

    {
        /* default "automatic" */
        const gchar *name = FALSENCU;

        action = GTK_ACTION(gtk_radio_action_new (name,
            "",
            NULL,
            NULL,
            (gint)NULL));

/* 		g_object_set_data(G_OBJECT(action), NWAMUI_OBJECT_DATA, NULL); */

        g_object_unref (action);

		gtk_ui_manager_add_ui (self->prv->ui_manager,
          self->prv->uid[ID_NCU],
          dynamic_menuitem_inserting_path[ID_NCU],
          name, /* name */
          name, /* action */
          GTK_UI_MANAGER_MENUITEM,
          TRUE);

        if ( selection_mode == NWAMUI_NCP_SELECTION_MODE_AUTOMATIC ) {
            self->prv->change_from_daemon = TRUE;
            gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action), TRUE);
            self->prv->change_from_daemon = FALSE;
        }

        g_signal_connect (G_OBJECT (action), "activate",
                          G_CALLBACK(on_nwam_ncu_toggled), (gpointer)self);
    }

    model = GTK_TREE_MODEL(nwamui_ncp_get_ncu_list_store (active_ncp));

    if (gtk_tree_model_get_iter_first (model, &iter)) {
        gboolean has_wireless_inf = FALSE;

        do {
            NwamuiNcu *ncu;

            gtk_tree_model_get (model, &iter, 0, &ncu, -1);
            nwam_menu_create_ncu_menuitems(G_OBJECT(self->prv->daemon),
              G_OBJECT(active_ncp),
              G_OBJECT(ncu),
              self);
            switch (nwamui_ncu_get_ncu_type (ncu)) {
            case NWAMUI_NCU_TYPE_WIRELESS: {
                /* enable "join wireless" menu item */
                has_wireless_inf = TRUE;
            }
                break;
            case NWAMUI_NCU_TYPE_WIRED:
                break;
            default:
                g_assert_not_reached ();
            }

            g_object_unref(ncu);
        } while (gtk_tree_model_iter_next (model, &iter));

        /* enable "join wireless" menu item */
        /* enable "manage known ap" menu item */
        if (has_wireless_inf) {
            menus_set_action_visible(self, ID_GLOBAL, MENUITEM_NAME_JOIN_WIRELESS, TRUE);
            menus_set_action_visible(self, ID_GLOBAL, MENUITEM_NAME_MANAGE_KNOWN_AP, TRUE);

        }

        /* initially populate network info */
        {
            NwamuiNcu *ncu = nwamui_ncp_get_active_ncu(active_ncp);
            event_ncu_up(self->prv->daemon, ncu, (gpointer) self);
            
            if (ncu) {
                g_object_unref(ncu);
            }
        }

        g_object_unref(active_ncp);

	} else {
        /* no ncu */
        action = GTK_ACTION(gtk_action_new (NONCU, _(NONCU), NULL, NULL));
        gtk_action_set_sensitive (action, FALSE);
        g_object_unref (action);

        gtk_ui_manager_add_ui (self->prv->ui_manager,
          self->prv->uid[ID_NCU],
          dynamic_menuitem_inserting_path[ID_NCU],
          NONCU, /* name */
          NONCU, /* action */
          GTK_UI_MANAGER_MENUITEM,
          TRUE);
    }

    g_object_unref(model);
#endif
}

static void
nwam_menu_create_static_menuitems (NwamMenu *self)
{
    NwamMenuPrivate *prv = self->prv;
    GError *error;

    nwam_menu_group_recreate(self, ID_GLOBAL, NWAMUI_ACTION_ROOT);

    gtk_ui_manager_remove_ui(prv->ui_manager, prv->uid[ID_GLOBAL]);

    if ((prv->uid[ID_GLOBAL] = gtk_ui_manager_add_ui_from_file(prv->ui_manager,
          UIDATA,
          &error)) != 0) {
        gtk_action_group_add_actions (prv->group[ID_GLOBAL],
          entries,
          G_N_ELEMENTS (entries),
          (gpointer) self);
    } else {
        g_error ("Can't find %s: %s", UIDATA, error->message);
        g_error_free (error);
    }
}

static void
nwam_menu_connect (NwamMenu *self)
{
    NwamuiNcp* active_ncp = nwamui_daemon_get_active_ncp (self->prv->daemon);
    GtkTreeModel *model = GTK_TREE_MODEL(nwamui_ncp_get_ncu_list_store (active_ncp));
    NwamuiNcu *ncu;
    GtkTreeIter iter;

    DEBUG();

    if (gtk_tree_model_get_iter_first (model, &iter)) {
        do {
            gtk_tree_model_get (model, &iter, 0, &ncu, -1);
            g_signal_connect (ncu, "notify",
                              G_CALLBACK(on_nwam_ncu_notify_cb),
                              (gpointer)self);
            switch (nwamui_ncu_get_ncu_type (ncu)) {
            case NWAMUI_NCU_TYPE_WIRELESS: {
                NwamuiWifiNet *wifi;
                GtkWidget *menuitem;
                gchar *m_name;

                wifi = nwamui_ncu_get_wifi_info (ncu);

                m_name = nwamui_wifi_net_get_essid (wifi);
                g_debug ("found ncu-wifi %s", m_name ? m_name : "NULL");
                menuitem = nwam_display_name_to_menuitem (self, m_name, ID_WIFI);
                g_free (m_name);

                if (menuitem) {
                    gtk_check_menu_item_toggled (GTK_CHECK_MENU_ITEM(menuitem));
                }
                break;
            }
            case NWAMUI_NCU_TYPE_WIRED:
                return;
            default:
                g_assert_not_reached ();
            }
        } while (gtk_tree_model_iter_next (model, &iter));
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

static void
on_activate_static_menuitems (GtkAction *action, gpointer data)
{
    gchar first_char = *gtk_action_get_name(action);
    gchar *argv = NULL;

    if (first_char == *MENUITEM_NAME_JOIN_WIRELESS) {
#if 0
        NwamMenu *self = NWAM_MENU (data);
        if (data) {
            gchar *argv = NULL;
            gchar *name = NULL;
            /* add valid wireless */
            NwamuiWifiNet *wifi = NWAMUI_WIFI_NET (data);
            name = nwamui_wifi_net_get_essid (wifi);
            argv = g_strconcat ("--add-wireless-dialog=", name, NULL);
            nwam_exec(argv);
            g_free (name);
            g_free (argv);
        } else {
            /* add other wireless */
            nwam_exec("--add-wireless-dialog=");
        }
#endif
        join_wireless(NULL);
    } else if (first_char == *MENUITEM_NAME_ENV_PREF) {
		argv = "-e";
    } else if (first_char == *MENUITEM_NAME_NET_PREF) {
		argv = "-p";
    } else if (first_char == *MENUITEM_NAME_VPN_PREF) {
        argv = "-n";
    } else if (first_char == *MENUITEM_NAME_HELP) {
        nwamui_util_show_help ("");
    } else {
        g_assert_not_reached ();
        /* About */
        gtk_show_about_dialog (NULL, 
          "name", _("NWAM Manager"),
          "title", _("About NWAM Manager"),
          "copyright", _("2007 Sun Microsystems, Inc  All rights reserved\nUse is subject to license terms."),
          "website", _("http://www.sun.com/"),
          NULL);
    }

    if (argv) {
        nwam_exec(argv);
    }
}

static void
on_nwam_ncu_toggled (GtkAction *action, gpointer data)
{
    NwamMenu *self = NWAM_MENU (data);
    NwamuiNcp* ncp = nwamui_daemon_get_active_ncp (self->prv->daemon);
    NwamuiNcu* ncu;
/*     NwamuiNcu* ncu = NWAMUI_NCU (g_object_get_data(G_OBJECT(action), NWAMUI_OBJECT_DATA)); */
    gpointer ncu_value = (gpointer)gtk_radio_action_get_current_value(GTK_RADIO_ACTION(action));

    g_object_get(action, "value", &ncu, NULL);

    g_debug("action 0x%p ncu 0x%p ncu_value 0x%p", action, ncu, ncu_value);

    /* If the toggle is generated by the daemon we don't want to proceed any
     * more since it will tell the daemon to reconnected unnecessarily
     */
    if (!self->prv->change_from_daemon
        && gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(action))) {
        if (ncu) {
            gchar *name = nwamui_ncu_get_device_name(ncu);
            g_debug("\n\tENABLE\tNcu\t 0x%p\t%s\n", ncu, name);
            g_free(name);
            nwamui_ncp_set_manual_ncu_selection(ncp, ncu);
        } else {
            nwamui_ncp_set_automatic_ncu_selection(ncp);
        }
    }
    g_object_unref(ncp);
}

static void
on_nwam_ncu_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
	NwamMenu *self = NWAM_MENU (data);
    NwamuiNcu *ncu;

    DEBUG ();

    ncu = NWAMUI_NCU(gobject);
    switch (nwamui_ncu_get_ncu_type (ncu)) {
    case NWAMUI_NCU_TYPE_WIRELESS:
/*         if (g_ascii_strcasecmp(arg1->name, "wifi-info") == 0) { */
/*             NwamuiWifiNet *wifi; */
/*             GtkAction *action; */
/*             gchar *m_name; */

/*             wifi = nwamui_ncu_get_wifi_info (ncu); */
/*             if (wifi) { */
/*                 m_name = nwamui_wifi_net_get_bssid (wifi); */
/*                 menus_set_toggle_action_active(self, ID_WIFI, m_name, TRUE); */
/*                 g_free (m_name); */
/*                 g_object_unref(wifi); */
/*             } */
/*         } */
        break;
    case NWAMUI_NCU_TYPE_WIRED:
        break;
    default:
        g_assert_not_reached ();
    }
}

static void
on_ncp_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
	NwamMenu *self = NWAM_MENU (data);
    g_assert(NWAMUI_IS_NCP(gobject));

    if (g_ascii_strcasecmp(arg1->name, "ncu-list-store") == 0) {
        nwam_menu_recreate_ncu_menuitems(self);
    } else if (g_ascii_strcasecmp(arg1->name, "many-wireless") == 0) {
        nwam_menu_recreate_wifi_menuitems(self);
    }
}

static void
event_ncu_down (NwamuiDaemon* daemon, NwamuiNcu* ncu, gpointer data)
{
    NwamuiNcp*  active_ncp = nwamui_daemon_get_active_ncp( daemon );
	NwamMenu *self = NWAM_MENU (data);
    NwamuiNcu*  active_ncu = NULL;

    if ( active_ncp != NULL && NWAMUI_NCP( active_ncp ) ) {
        active_ncu = nwamui_ncp_get_active_ncu( active_ncp );
        g_object_unref(active_ncp);
    }

    /* Only change the automatic message if it's the active ncu */
    if ( ncu == active_ncu ) {
        menus_set_automatic_label(self, NULL);
    }

    if ( active_ncu != NULL ) {
        g_object_unref(active_ncu);
    }
}

static void
event_active_env_changed (NwamuiDaemon* daemon, NwamuiEnv* env, gpointer data)
{
	NwamMenu *self = NWAM_MENU (data);
    gchar *sname;
    gchar *summary, *body;
    NwamuiNcp* ncp;

    DEBUG();
    sname = nwamui_env_get_name (env);
    summary = g_strdup_printf (_("Switched to location '%s'"), sname);
    nwamui_daemon_get_active_ncp (daemon);
    body = g_strdup_printf (_("%s\n"), "Now configuring your network");
    nwam_notification_show_message (summary, body, NULL, NOTIFY_EXPIRES_DEFAULT);

    g_free (sname);
    g_free (summary);
    g_free (body);

    /* TODO - we should receive this signal iff change to env successfully */
    /* according to v1.5 P4 rescan wifi now */
    self->prv->force_wifi_rescan_due_to_env_changed = TRUE;
    nwam_menu_recreate_wifi_menuitems (self);
}

static void
event_add_wifi_fav (NwamuiDaemon* daemon, NwamuiWifiNet* wifi, gpointer data)
{
	NwamMenu *self = NWAM_MENU (data);
    gchar *summary, *body;
    gchar *name;

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
      notification_listwireless_cb,
      (gpointer) g_object_ref (self),
      (GFreeFunc) g_object_unref,
      NOTIFY_EXPIRES_DEFAULT);
    g_free(body);
    g_free(summary);
}

static void
connect_daemon_signals(GObject *self, NwamuiDaemon *daemon)
{
    /* handle all daemon signals here */
    g_signal_connect(daemon, "status_changed",
      G_CALLBACK(event_daemon_status_changed), (gpointer) self);
    g_signal_connect(daemon, "daemon_info",
      G_CALLBACK(event_daemon_info), (gpointer) self);
	g_signal_connect((gpointer) daemon, "wifi_scan_result",
      (GCallback)nwam_menu_create_wifi_menuitems, (gpointer) self);
/*     g_signal_connect(daemon, "ncu_up", */
/*       G_CALLBACK(event_ncu_up), (gpointer) self); */
/*     g_signal_connect(daemon, "ncu_down", */
/*       G_CALLBACK(event_ncu_down), (gpointer) self); */
    g_signal_connect(daemon, "add_wifi_fav",
      G_CALLBACK(event_add_wifi_fav), (gpointer) self);
}

static void
disconnect_daemon_signals(GObject *self, NwamuiDaemon *daemon)
{
    g_signal_handlers_disconnect_matched(daemon,
      G_SIGNAL_MATCH_DATA,
      0,
      NULL,
      NULL,
      NULL,
      (gpointer)self);
}

static void
event_daemon_status_changed(NwamuiDaemon *daemon, nwamui_daemon_status_t status, gpointer user_data)
{
	NwamMenu *self = NWAM_MENU (user_data);

	/* should repopulate data here */
	
    switch( status) {
    case NWAMUI_DAEMON_STATUS_ACTIVE:
        /* must call the following two functions to create menus initially */
/*         nwam_menu_recreate_wifi_menuitems (self); */
/*         nwamui_daemon_wifi_start_scan(self->prv->daemon); */

        /* We don't need rescan wlans, because when daemon changes to active it
           will emulate wlan changed signal */

        nwam_menu_recreate_ncu_menuitems (self);
        nwam_menu_recreate_wifi_menuitems(self);

        break;
    case NWAMUI_DAEMON_STATUS_INACTIVE:
        nwam_menu_delete_wifi_menuitems(self);
        nwam_menu_delete_ncu_menuitems(self);
        break;
    default:
        g_assert_not_reached();
        break;
    }
}

static void
event_daemon_info (NwamuiDaemon* daemon,
  gint type,
  GObject *obj,
  gpointer data,
  gpointer user_data)
{
	NwamMenu *self = NWAM_MENU (user_data);
    GtkAction *action = NULL;

    switch (type) {
    case NWAMUI_DAEMON_INFO_WLAN_CHANGED:
        nwam_menu_recreate_wifi_menuitems(self);
        break;
    case NWAMUI_DAEMON_INFO_WLAN_CONNECTED: {
        gchar *name = nwamui_wifi_net_get_unique_name(NWAMUI_WIFI_NET(obj));

        /* Since nwamui daemon may create another wifi net instance for a same
         * wlan and set to a ncu, so we should try to find the menu item which
         * has the same essid so that we can update its wifi data. */
        GtkAction* action = menu_group_get_item_by_name(self, ID_WIFI, name);

        g_free(name);

        if (action) {
            nwam_action_set_wifi(NWAM_WIFI_ACTION(action), NWAMUI_WIFI_NET(obj));

            self->prv->change_from_daemon = TRUE;
            gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action), TRUE);
            self->prv->change_from_daemon = FALSE;
        }
    }
        break;
    case NWAMUI_DAEMON_INFO_WLAN_DISCONNECTED: {
        gchar *name = nwamui_wifi_net_get_unique_name(NWAMUI_WIFI_NET(obj));
        menus_set_toggle_action_active(self, ID_WIFI, name, FALSE);
        g_free(name);
    }
        /* Fall through */
    case NWAMUI_DAEMON_INFO_WLAN_CONNECT_FAILED:
        /* enable default "false" wlan */
        menus_set_toggle_action_active(self, ID_WIFI, NULL, TRUE);
        break;
    default:
        /* ignore others */
        break;
    }
}

static void
notification_listwireless_cb (NotifyNotification *n, gchar *action, gpointer data)
{
    nwam_exec("--list-fav-wireless=");
}

/**
 * nwam_menu_create_ncu_menuitems:
 *
 * 0.5 only, create a menuitem for each network interface.
 */
static void
nwam_menu_create_ncu_menuitems (GObject *daemon, GObject *ncp, GObject *ncu, gpointer data)
{
#if 0
	NwamMenu *self = NWAM_MENU (data);
	GtkAction *action = NULL;
    nwamui_ncp_selection_mode_t selection_mode = nwamui_ncp_get_selection_mode( NWAMUI_NCP(ncp) );

    /* TODO - Make this more efficient */

	if (ncu) {
		gchar *name = NULL;
        gchar *lbl_name = NULL;
        gchar *type_str = NULL;
		
		name = nwamui_ncu_get_device_name(NWAMUI_NCU(ncu));
		if ( nwamui_ncu_get_ncu_type(NWAMUI_NCU(ncu)) == NWAMUI_NCU_TYPE_WIRELESS ){
            type_str = _("Wireless");
        }
        else {
            type_str = _("Wired");
        }
        lbl_name = g_strdup_printf(_("Always Use %s Network Interface (%s)"), type_str, name);

		action = GTK_ACTION(gtk_radio_action_new (name, lbl_name, NULL, NULL, (gint)ncu));
/* 		g_object_set_data(G_OBJECT(action), NWAMUI_OBJECT_DATA, ncu); */
        g_signal_connect (ncu, "notify",
          G_CALLBACK(on_nwam_ncu_notify_cb), (gpointer)self);

        if ( selection_mode == NWAMUI_NCP_SELECTION_MODE_MANUAL
          && nwamui_ncu_get_active(NWAMUI_NCU(ncu)) ) {
            self->prv->change_from_daemon = TRUE;
            gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action), TRUE);
            self->prv->change_from_daemon = FALSE;
        }

		g_object_unref (action);

		/* menu */
		gtk_ui_manager_add_ui (self->prv->ui_manager,
          self->prv->uid[ID_NCU],
          dynamic_menuitem_inserting_path[ID_NCU],
          name, /* name */
          name, /* action */
          GTK_UI_MANAGER_MENUITEM,
          TRUE);

        g_signal_connect (G_OBJECT (action), "toggled",
                          G_CALLBACK(on_nwam_ncu_toggled), (gpointer)self);
		g_free(name);
		g_free(lbl_name);
    }
#endif
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
	NwamMenu *self = NWAM_MENU (data);
    NwamuiNcp *ncp = NULL;
	GtkAction *action = NULL;
    gboolean   has_many_wireless = FALSE;
	
    /* TODO - Make this more efficient */
    if (self == NULL || !self->prv->force_wifi_rescan) {
        return;
    }

    ncp = nwamui_daemon_get_active_ncp(self->prv->daemon);
    has_many_wireless = nwamui_ncp_has_many_wireless( ncp );

	if (wifi) {
		action = GTK_ACTION(nwam_wifi_action_new(NWAMUI_WIFI_NET(wifi)));

		self->prv->has_wifi = TRUE;
        nwam_menuitem_proxy_new(action, self->prv->group[ID_WIFI]);

        /* If the wifi net is connected, then it should be selected in the
         * menus. Do this here since the active ncu could be NULL, yet still
         * be connected, this will catch that case.
         */

        if (nwamui_wifi_net_get_status(NWAMUI_WIFI_NET(wifi)) == NWAMUI_WIFI_STATUS_CONNECTED) {
            gtk_toggle_action_set_active(action, TRUE);
        }

		g_object_unref (action);
	} else {
        g_debug("----------- menu item creation is  over -------------");

        /* we must clear force rescan flag if it is */
        if (self->prv->force_wifi_rescan) {
            self->prv->force_wifi_rescan = FALSE;
        }

        if (self->prv->has_wifi) {
            NwamuiNcu *ncu = nwamui_ncp_get_active_ncu(ncp);

            if (ncu) {
                if (nwamui_ncu_get_ncu_type(ncu) == NWAMUI_NCU_TYPE_WIRELESS) {
                    NwamuiWifiNet *wifi;
                    wifi = nwamui_ncu_get_wifi_info(ncu);

                    if (wifi) {
                        if (nwamui_wifi_net_get_status(wifi) == NWAMUI_WIFI_STATUS_CONNECTED) {
                            gchar *name = nwamui_wifi_net_get_unique_name(wifi);
                            g_debug("======== enable wifi %s ===========", name);
                            g_free(name);

                            gtk_toggle_action_set_active(nwam_menu_group_get_item_by_proxy(self->prv->group[ID_WIFI],
                                wifi),
                              TRUE);
                        }
                        g_object_unref(wifi);
                    }
                }
                g_object_unref(ncu);
            }
        }

        /* no wifi */
        if (!self->prv->has_wifi) {
            gtk_toggle_action_set_active(nwam_menu_group_get_item_by_proxy(self->prv->group[ID_WIFI],
                NULL), TRUE);
            gtk_action_set_sensitive(nwam_menu_group_get_item_by_proxy(self->prv->group[ID_WIFI],
                NULL), FALSE);
        }
    }
    g_object_unref(ncp);
}

static void
connect_ncp_signals(GObject *self, NwamuiNcp *ncp)
{
    g_signal_connect(ncp, "notify::ncu-list-store",
      G_CALLBACK(on_ncp_notify_cb),
      (gpointer)self);
    g_signal_connect(ncp, "notify::many-wireless",
      G_CALLBACK(on_ncp_notify_cb),
      (gpointer)self);
}

static void
disconnect_ncp_signals(GObject *self, NwamuiNcp *ncp)
{
    g_signal_handlers_disconnect_matched(ncp,
      G_SIGNAL_MATCH_DATA,
      0,
      NULL,
      NULL,
      NULL,
      (gpointer)self);
}

static void
nwam_menu_create_env_menuitems (NwamMenu *self)
{
    self->prv->group[ID_ENV] = nwam_env_group_new(self->prv->ui_manager,
      dynamic_menuitem_inserting_path[ID_ENV]);
}

static void
nwam_menu_create_vpn_menuitems(NwamMenu *self)
{
    self->prv->group[ID_VPN] = nwam_enm_group_new(self->prv->ui_manager,
      dynamic_menuitem_inserting_path[ID_VPN]);
}

/* FIXME, should be singleton. for mutiple status icons should sustain an
 * internal vector which record <icon, menu> pairs.
 * This function should be invoked/destroyed in the main function.
 */
NwamMenu *
get_nwam_menu_instance ()
{
	if (instance)
		return NWAM_MENU(g_object_ref (G_OBJECT(instance)));
	return NWAM_MENU(instance = g_object_new (NWAM_TYPE_MENU, NULL));
}

/* FIXME, need re-create menu magic */
static GtkWidget *
nwam_menu_get_statusicon_menu (NwamMenu *self/*, icon index*/)
{
	/* icon index => menu root */
	gtk_ui_manager_ensure_update (self->prv->ui_manager);
#if 0
	{
		gchar *ui = gtk_ui_manager_get_ui (self->prv->ui_manager);
		g_debug("%s\n", ui);
		g_free (ui);
	}
#endif
	return gtk_ui_manager_get_widget(self->prv->ui_manager, NWAMUI_PROOT);
}

/*
 * Be sure call after get_nwam_menu_instance ()
 */
GtkWidget *
get_status_icon_menu( gint index )
{
	return nwam_menu_get_statusicon_menu (instance);
}
