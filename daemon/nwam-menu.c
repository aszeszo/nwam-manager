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
#include "libnwamui.h"
#include "nwam-obj-proxy-iface.h"

#include "nwam-menuitem.h"
#include "nwam-wifi-item.h"
#include "nwam-enm-item.h"
#include "nwam-env-item.h"
#include "nwam-ncu-item.h"

#define DEBUG()	g_debug ("[[ %20s : %-4d ]]", __func__, __LINE__)

typedef struct _NwamMenuPrivate NwamMenuPrivate;
#define NWAM_MENU_GET_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), NWAM_TYPE_MENU, NwamMenuPrivate))

enum {
	SECTION_GLOBAL = 0,         /* Not really used */
	SECTION_WIFI,
	SECTION_LOC,
	SECTION_ENM,
	SECTION_NCU,
	N_SECTION
};

typedef struct _ForeachData {
	GtkTreeModelForeachFunc foreach_func;
	gpointer user_data;
	gpointer ret_data;
} ForeachData;

struct _NwamMenuPrivate {
	NwamuiDaemon *daemon;

	gboolean has_wifi;
	gboolean force_wifi_rescan_due_to_env_changed;
	gboolean force_wifi_rescan;

    guint update_wifi_timer;

	/* Each section is began with GtkSeparatorMenuItem, NULL means 0. We'd
     * better record every separators even though we don't really use some of
     * them. Must update it if Menu is re-design.
     */
    GtkWidget *section[N_SECTION];
};

#define STATIC_MENUITEM_ID "static_item_id"

/* Pop-up menu */
#define NWAM_MANAGER_PROPERTIES "nwam-manager-properties"
#define	NONCU	"No network interfaces detected"

enum {
    MENUITEM_ENV_PREF,
    MENUITEM_CONN_PROF,
    MENUITEM_REFRESH_WLAN,
    MENUITEM_JOIN_WLAN,
    MENUITEM_VPN_PREF,
    MENUITEM_NET_PREF,
    MENUITEM_HELP,
    MENUITEM_TEST,
};

/* GtkMenu MACROs, must define ITEM_VARNAME before use them. */
#define MENU_APPEND_ITEM_BASE(menu, type, label, callback, user_data)   \
    g_assert(GTK_IS_MENU_SHELL(menu));                                  \
    ITEM_VARNAME = g_object_new(type, NULL);                            \
    if (!g_type_is_a(type, GTK_TYPE_SEPARATOR_MENU_ITEM)) {             \
        if (label)                                                      \
            menu_item_set_label(GTK_MENU_ITEM(ITEM_VARNAME), label);    \
        if (callback)                                                   \
            g_signal_connect((ITEM_VARNAME),                            \
              "activate", G_CALLBACK(callback), (gpointer)user_data);   \
    }                                                                   \
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), GTK_WIDGET(ITEM_VARNAME));
#define menu_append_item(menu, type, label, callback, user_data)        \
    {                                                                   \
        MENU_APPEND_ITEM_BASE(menu, type, label, callback, user_data);  \
    }
#define menu_append_image_item(menu, label, img, callback, user_data)   \
    {                                                                   \
        MENU_APPEND_ITEM_BASE(menu, GTK_TYPE_IMAGE_MENU_ITEM,           \
          label, callback, user_data);                                  \
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(ITEM_VARNAME), \
          GTK_WIDGET(GTK_IMAGE(img)));                                  \
    }
#define menu_append_stock_item(menu, label, stock, callback, user_data) \
    {                                                                   \
        MENU_APPEND_ITEM_BASE(menu, GTK_TYPE_IMAGE_MENU_ITEM,           \
          label, callback, user_data);                                  \
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(ITEM_VARNAME), \
          gtk_image_new_from_stock(stock, GTK_ICON_SIZE_MENU));         \
    }
#define menu_append_item_with_submenu(menu, type, label, submenu)       \
    {                                                                   \
        g_assert(!g_type_is_a(type, GTK_TYPE_SEPARATOR_MENU_ITEM));     \
        MENU_APPEND_ITEM_BASE(menu, type, label, NULL, NULL);           \
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(ITEM_VARNAME),          \
          GTK_WIDGET(submenu));                                         \
    }
#define menu_append_separator(menu)                                 \
    {                                                               \
        MENU_APPEND_ITEM_BASE(menu, GTK_TYPE_SEPARATOR_MENU_ITEM,   \
          NULL, NULL, NULL);                                        \
    }
#define menu_append_section_separator(self, menu, sec_id, is_first)     \
    {                                                                   \
        NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);             \
        g_assert(GTK_IS_MENU_SHELL(menu));                              \
        if (is_first)                                                   \
            prv->section[sec_id] = GTK_WIDGET(menu);                    \
        else {                                                          \
            MENU_APPEND_ITEM_BASE(menu, GTK_TYPE_SEPARATOR_MENU_ITEM,   \
              NULL, NULL, NULL);                                        \
            prv->section[sec_id] = GTK_WIDGET(ITEM_VARNAME);            \
        }                                                               \
    }
#define menu_remove_item(menu, item)                            \
    {                                                           \
        g_assert(GTK_IS_MENU(menu) && GTK_IS_MENU_ITEM(item));  \
        gtk_container_remove(GTK_CONTAINER(menu), item);        \
    }

static glong update_wifi_timer_interval = 500;

static void nwam_menu_finalize (NwamMenu *self);

static void nwam_menu_connect (NwamMenu *self);

static void nwam_menu_start_update_wifi_timer(NwamMenu *self);
static void nwam_menu_stop_update_wifi_timer(NwamMenu *self);
static void nwam_menu_reset_update_wifi_timer(NwamMenu *self);

static void nwam_menu_recreate_wifi_menuitems (NwamMenu *self);
static void nwam_menu_recreate_ncu_menuitems (NwamMenu *self);
static void nwam_menu_recreate_env_menuitems (NwamMenu *self);
static void nwam_menu_recreate_enm_menuitems (NwamMenu *self);

static void nwam_menu_create_wifi_menuitems (GObject *daemon, GObject *wifi, gpointer data);
static void nwam_menu_create_static_menuitems (NwamMenu *self);
static void nwam_menu_create_dynamic_menuitems (NwamMenu *self);
static GtkWidget* nwam_menu_item_creator(NwamMenu *self, NwamuiObject *object);

static void notification_listwireless_cb (NotifyNotification *n, gchar *action, gpointer data);

/* Utils */
static void menus_set_automatic_label(NwamMenu *self, NwamuiNcu *ncu);
static gint nwam_menu_item_compare(GtkWidget *a, GtkWidget *b);

/* GtkMenu utils */
static void menu_foreach_by_name(GtkWidget *widget, gpointer user_data);
static void menu_foreach_by_proxy(GtkWidget *widget, gpointer user_data);
static GtkMenuItem* menu_get_item_by_name(GtkMenu *menu, const gchar* name);
static GtkMenuItem* menu_get_item_by_proxy(GtkMenu *menu, GObject* proxy);
static void menu_section_get_positions(GtkMenu *menu,
  GtkWidget *start_sep, GtkWidget *end_sep,
  gint *ret_start_pos, gint *ret_end_pos);
static void menu_section_get_children(GtkMenu *menu,
  GtkWidget *start_sep, GtkWidget *end_sep,
  GList **ret_children, gint *ret_start_pos);

/* NwamMenu section utils */
static void nwam_menu_section_get_section_info(NwamMenu *self, gint sec_id,
  GtkWidget **menu, GtkWidget **start_sep, GtkWidget **end_sep);
static void nwam_menu_section_sort(NwamMenu *self, gint sec_id);
static void nwam_menu_get_section_positions(NwamMenu *self, gint sec_id,
  gint *ret_start_pos, gint *ret_end_pos);
static void nwam_menu_section_insert_sort(NwamMenu *self, gint sec_id, GtkWidget *item);
static void nwam_menu_section_append(NwamMenu *self, gint sec_id, GtkWidget *item);
static void nwam_menu_section_prepend(NwamMenu *self, gint sec_id, GtkWidget *item);
static void nwam_menu_section_delete(NwamMenu *self, gint sec_id);

/* call back */
static void on_activate_static_menuitems (GtkMenuItem *menuitem, gpointer user_data);

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

G_DEFINE_TYPE (NwamMenu, nwam_menu, GTK_TYPE_MENU)

static void
nwam_menu_class_init (NwamMenuClass *klass)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->finalize = (void (*)(GObject*)) nwam_menu_finalize;

	g_type_class_add_private (klass, sizeof (NwamMenuPrivate));
}

static void
nwam_menu_init (NwamMenu *self)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);

	/* ref daemon instance */
	prv->daemon = nwamui_daemon_get_instance ();

    /* Must create static menus before connect to any signals. */
    nwam_menu_create_static_menuitems (self);
	nwam_menu_create_dynamic_menuitems (self);

    /* handle all daemon signals here */
    connect_daemon_signals(G_OBJECT(self), prv->daemon);

    {
        NwamuiNcp *ncp = nwamui_daemon_get_active_ncp(prv->daemon);

        connect_ncp_signals(G_OBJECT(self), ncp);

        g_object_unref(ncp);
    }

    /* initially populate network info */
    event_daemon_status_changed(prv->daemon,
      NWAMUI_DAEMON_STATUS_ACTIVE,
      (gpointer) self);
#if PHASE_1_0
    nwam_menu_connect (self);
#endif
}

static void
nwam_menu_finalize (NwamMenu *self)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);
	g_object_unref (G_OBJECT(prv->daemon));

#if PHASE_1_0
    {
        NwamuiNcp *ncp = nwamui_daemon_get_active_ncp(prv->daemon);

        disconnect_ncp_signals(G_OBJECT(self), ncp);

        g_object_unref(ncp);
    }
#endif

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

    g_free(automatic_label);
}

static gint
nwam_menu_item_compare(GtkWidget *a, GtkWidget *b)
{
    return 0;
}

static void
nwam_menu_recreate_wifi_menuitems (NwamMenu *self)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);

    nwam_menu_section_delete(self, SECTION_WIFI);

    /* set force rescan flag so that we can identify daemon scan wifi event */
    prv->force_wifi_rescan = TRUE;
}

static void
nwam_menu_recreate_ncu_menuitems (NwamMenu *self)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);
    NwamuiNcp* active_ncp = nwamui_daemon_get_active_ncp(prv->daemon);
    GtkTreeModel *model = NULL;
    GtkTreeIter iter;
    nwamui_ncp_selection_mode_t selection_mode = nwamui_ncp_get_selection_mode( active_ncp );

    DEBUG();

    nwam_menu_section_delete(self, SECTION_NCU);
    /* disable "join wireless" menu item */
    /* disable "manage known ap" menu item */
/*     gtk_widget_hide(MENUITEM_NAME_JOIN_WIRELESS); */

    model = GTK_TREE_MODEL(nwamui_ncp_get_ncu_tree_store (active_ncp));

    if (gtk_tree_model_get_iter_first (model, &iter)) {
        gboolean has_wireless_inf = FALSE;

        do {
            NwamuiNcu *ncu;
            GtkWidget *item;

            gtk_tree_model_get (model, &iter, 0, &ncu, -1);

            item = nwam_menu_item_creator(self, NWAMUI_OBJECT(ncu));

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
/*             gtk_widget_show(MENUITEM_NAME_JOIN_WIRELESS); */
/*             gtk_widget_show(MENUITEM_NAME_MANAGE_KNOWN_AP); */
        }

        /* initially populate network info */
        {
            NwamuiNcu *ncu = nwamui_ncp_get_active_ncu(active_ncp);
/*             event_ncu_up(prv->daemon, ncu, (gpointer) self); */
            
            if (ncu) {
                g_object_unref(ncu);
            }
        }

        g_object_unref(active_ncp);

	} else {
        GtkAction *action;
        /* no ncu */
        /* TODO add to group */
        action = GTK_ACTION(gtk_action_new (NONCU, _(NONCU), NULL, NULL));
        gtk_action_set_sensitive (action, FALSE);
        g_object_unref (action);
    }

    g_object_unref(model);
}

static void
nwam_menu_recreate_env_menuitems (NwamMenu *self)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);
    GList *env_list = nwamui_daemon_get_env_list(prv->daemon);
    GList *idx = NULL;
	GtkWidget* item = NULL;

	for (idx = env_list; idx; idx = g_list_next(idx)) {
        //nwam_menuitem_proxy_new(action, env_group);
        item = nwam_menu_item_creator(self, NWAMUI_OBJECT(idx->data));
	}
    g_list_free(env_list);
}

static void
nwam_menu_recreate_enm_menuitems (NwamMenu *self)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);
    GList *enm_list = nwamui_daemon_get_enm_list(prv->daemon);
	GList *idx = NULL;
	GtkWidget* item = NULL;

	for (idx = enm_list; idx; idx = g_list_next(idx)) {
        //nwam_menuitem_proxy_new(action, vpn_group);
        item = nwam_menu_item_creator(self, NWAMUI_OBJECT(idx->data));
	}
    g_list_free(enm_list);
}

static void
nwam_menu_create_static_menuitems (NwamMenu *self)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);
    GtkWidget *root_menu;
    GtkWidget *sub_menu;
    GtkWidget *menuitem;

#define ITEM_VARNAME menuitem

    /* Root menu. */
    root_menu = GTK_WIDGET(self);
    menu_append_section_separator(self, root_menu, SECTION_GLOBAL, TRUE);

    sub_menu = gtk_menu_new();
    menu_append_section_separator(self, sub_menu, SECTION_LOC, TRUE);
    menu_append_separator(sub_menu);

    menu_append_item(sub_menu,
      GTK_TYPE_MENU_ITEM, _("_Location Preferences..."),
      on_activate_static_menuitems, self);
    g_object_set_data(menuitem, STATIC_MENUITEM_ID, (gpointer)MENUITEM_ENV_PREF);
    menu_append_item_with_submenu(root_menu,
      GTK_TYPE_MENU_ITEM, _("_Location"), sub_menu);

    sub_menu = gtk_menu_new();
    menu_append_section_separator(self, sub_menu, SECTION_NCU, TRUE);
    menu_append_separator(sub_menu);

    menu_append_item(sub_menu,
      GTK_TYPE_MENU_ITEM, _("_Connection Profile..."),
      on_activate_static_menuitems, self);
    g_object_set_data(menuitem, STATIC_MENUITEM_ID, (gpointer)MENUITEM_CONN_PROF);

    menu_append_item_with_submenu(root_menu,
      GTK_TYPE_MENU_ITEM, _("_Connections"), sub_menu);

    menu_append_section_separator(self, root_menu, SECTION_WIFI, FALSE);

    menu_append_separator(root_menu);

    menu_append_item(root_menu,
      GTK_TYPE_MENU_ITEM, _("_Refresh Wireless Networks"),
      on_activate_static_menuitems, self);
    g_object_set_data(menuitem, STATIC_MENUITEM_ID, (gpointer)MENUITEM_REFRESH_WLAN);

    menu_append_item(root_menu,
      GTK_TYPE_MENU_ITEM, _("_Join Unlisted Wireless Network..."),
      on_activate_static_menuitems, self);
    g_object_set_data(menuitem, STATIC_MENUITEM_ID, (gpointer)MENUITEM_JOIN_WLAN);

    menu_append_separator(root_menu);

    /* Sub_Menu */
    sub_menu = gtk_menu_new();
    menu_append_section_separator(self, sub_menu, SECTION_ENM, TRUE);

    menu_append_separator(sub_menu);

    menu_append_item(sub_menu,
      GTK_TYPE_MENU_ITEM, _("_VPN Preferences..."),
      on_activate_static_menuitems, self);
    g_object_set_data(menuitem, STATIC_MENUITEM_ID, (gpointer)MENUITEM_VPN_PREF);
    menu_append_item_with_submenu(root_menu,
      GTK_TYPE_MENU_ITEM,_("_VPN Applications"), sub_menu);

    menu_append_separator(root_menu);

    menu_append_item(root_menu,
      GTK_TYPE_MENU_ITEM, _("_Network Preferences..."),
      on_activate_static_menuitems, self);
    g_object_set_data(menuitem, STATIC_MENUITEM_ID, (gpointer)MENUITEM_NET_PREF);

    menu_append_stock_item(root_menu,
      _("_Help"), GTK_STOCK_HELP,
      on_activate_static_menuitems, self);
    g_object_set_data(menuitem, STATIC_MENUITEM_ID, (gpointer)MENUITEM_HELP);

    menu_append_item(root_menu,
      GTK_TYPE_MENU_ITEM, _("TeSt MeNu"),
      on_activate_static_menuitems, self);
    g_object_set_data(menuitem, STATIC_MENUITEM_ID, (gpointer)MENUITEM_TEST);
}

static void
nwam_menu_connect (NwamMenu *self)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);
    NwamuiNcp* active_ncp = nwamui_daemon_get_active_ncp (prv->daemon);
    GtkTreeModel *model = GTK_TREE_MODEL(nwamui_ncp_get_ncu_tree_store (active_ncp));
    NwamuiNcu *ncu;
    GtkTreeIter iter;

    DEBUG();

    if (gtk_tree_model_get_iter_first (model, &iter)) {
        do {
            gtk_tree_model_get (model, &iter, 0, &ncu, -1);
/*             g_signal_connect (ncu, "notify", */
/*                               G_CALLBACK(on_nwam_ncu_notify_cb), */
/*                               (gpointer)self); */
            switch (nwamui_ncu_get_ncu_type (ncu)) {
            case NWAMUI_NCU_TYPE_WIRELESS: {
                NwamuiWifiNet *wifi;
                GtkWidget *menuitem = NULL;
                gchar *m_name;

                wifi = nwamui_ncu_get_wifi_info (ncu);

                m_name = nwamui_wifi_net_get_essid (wifi);
                g_debug ("found ncu-wifi %s", m_name ? m_name : "NULL");
                /* menuitem = nwam_display_name_to_menuitem (self, m_name, ID_WIFI); */
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
on_activate_static_menuitems (GtkMenuItem *menuitem, gpointer user_data)
{
    NwamMenu *self = NWAM_MENU(user_data);
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);
    gchar *argv = NULL;
    gint menuitem_id = (gint)g_object_get_data(menuitem, STATIC_MENUITEM_ID);

    switch (menuitem_id) {
    case MENUITEM_ENV_PREF:
		argv = "-l";
        break;
    case MENUITEM_CONN_PROF:
    case MENUITEM_REFRESH_WLAN:
        nwam_menu_recreate_wifi_menuitems(self);
        break;
    case MENUITEM_JOIN_WLAN:
#if 0
        NwamMenu *self = NWAM_MENU(data);
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
            nwam_menu_section_delete(self, SECTION_LOC);
        } else {
            nwam_menu_recreate_env_menuitems(self);
        }
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
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);
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
    prv->force_wifi_rescan_due_to_env_changed = TRUE;
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
    g_signal_connect(daemon, "active_env_changed",
      G_CALLBACK(event_active_env_changed), (gpointer) self);
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
/*         nwamui_daemon_wifi_start_scan(prv->daemon); */

        /* We don't need rescan wlans, because when daemon changes to active it
           will emulate wlan changed signal */

        nwam_menu_recreate_ncu_menuitems (self);
        nwam_menu_recreate_wifi_menuitems(self);

        break;
    case NWAMUI_DAEMON_STATUS_INACTIVE:
        nwam_menu_section_delete(self, SECTION_WIFI);
        nwam_menu_section_delete(self, SECTION_NCU);
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

    switch (type) {
    case NWAMUI_DAEMON_INFO_WLAN_CHANGED:
        nwam_menu_stop_update_wifi_timer(self);
        nwam_menu_start_update_wifi_timer(self);
        break;
    case NWAMUI_DAEMON_INFO_WLAN_CONNECTED:
    case NWAMUI_DAEMON_INFO_WLAN_DISCONNECTED:
    case NWAMUI_DAEMON_INFO_WLAN_CONNECT_FAILED:
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
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);
    NwamuiNcp *ncp = NULL;
	GtkWidget *item = NULL;
    gboolean   has_many_wireless = FALSE;
	
    /* TODO - Make this more efficient */
    if (self == NULL || !prv->force_wifi_rescan) {
        return;
    }

    ncp = nwamui_daemon_get_active_ncp(prv->daemon);
    has_many_wireless = nwamui_ncp_has_many_wireless( ncp );

	if (wifi) {
		item = nwam_menu_item_creator(self, NWAMUI_OBJECT(wifi));

		prv->has_wifi = TRUE;
	} else {
        g_debug("----------- menu item creation is  over -------------");

        /* we must clear force rescan flag if it is */
        if (prv->force_wifi_rescan) {
            prv->force_wifi_rescan = FALSE;
        }

        if (prv->has_wifi) {
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
                        }
                        g_object_unref(wifi);
                    }
                }
                g_object_unref(ncu);
            }
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
nwam_menu_create_dynamic_menuitems(NwamMenu *self)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);

    nwam_menu_recreate_ncu_menuitems (self);
    nwam_menu_recreate_env_menuitems (self);
    nwam_menu_recreate_enm_menuitems (self);
    nwam_menu_recreate_wifi_menuitems (self);
}

static GtkWidget*
nwam_menu_item_creator(NwamMenu *self, NwamuiObject *object)
{
    GType type = G_OBJECT_TYPE(object);
    GtkWidget *item;
    gint sec_id;

    {
        gchar *name = nwamui_object_get_name(object);
        g_debug("%s %s", __func__, name);
        g_free(name);
    }

    if (type == NWAMUI_TYPE_NCU) {
        sec_id = SECTION_NCU;
        item = nwam_ncu_item_new(NWAMUI_NCU(object));
    } else if (type == NWAMUI_TYPE_WIFI_NET) {
        sec_id = SECTION_WIFI;
        item = nwam_wifi_item_new(NWAMUI_WIFI_NET(object));
	} else if (type == NWAMUI_TYPE_ENV) {
        sec_id = SECTION_LOC;
        item = nwam_env_item_new(NWAMUI_ENV(object));
	} else if (type == NWAMUI_TYPE_ENM) {
        sec_id = SECTION_ENM;
        item = nwam_enm_item_new(NWAMUI_ENM(object));
	} else {
        sec_id = SECTION_GLOBAL;
        item = NULL;
		g_error("unknow supported get nwamui obj list");
	}
    nwam_menu_section_insert_sort(self, sec_id, item);
    return item;
}

GtkMenu*
nwam_menu_new ()
{
	return g_object_new (NWAM_TYPE_MENU, NULL);
}

static gboolean
update_wifi_timer_func(gpointer user_data)
{
	NwamMenu *self = NWAM_MENU (user_data);
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);

    /* TODO, scan wifi */
    nwam_menu_recreate_wifi_menuitems(self);

    return TRUE;
}

static void
nwam_menu_start_update_wifi_timer(NwamMenu *self)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);

    if (prv->update_wifi_timer > 0) {
        g_source_remove(prv->update_wifi_timer);
        prv->update_wifi_timer = 0;
    }

    prv->update_wifi_timer = g_timeout_add_full(G_PRIORITY_DEFAULT,
      update_wifi_timer_interval,
      update_wifi_timer_func,
      (gpointer)self,
      (GDestroyNotify)g_object_unref);
}

static void
nwam_menu_stop_update_wifi_timer(NwamMenu *self)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);

    if (prv->update_wifi_timer > 0) {
        g_source_remove(prv->update_wifi_timer);
        prv->update_wifi_timer = 0;
    }
}

static void
nwam_menu_reset_update_wifi_timer(NwamMenu *self)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);

}

static void
nwam_menu_section_get_section_info(NwamMenu *self, gint sec_id,
  GtkWidget **menu, GtkWidget **start_sep, GtkWidget **end_sep)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);

    g_assert(menu);
    g_assert(start_sep);

    *start_sep = prv->section[sec_id];

    g_assert(*start_sep);

    if (GTK_IS_SEPARATOR_MENU_ITEM(*start_sep)) {
        *menu = gtk_widget_get_parent(*start_sep);
    } else if (GTK_IS_MENU(*start_sep)) {
        *menu = *start_sep;
        *start_sep = NULL;
    } else
        g_assert_not_reached();

    if (end_sep)
        *end_sep = NULL;
}


static void
nwam_menu_section_prepend(NwamMenu *self, gint sec_id, GtkWidget *item)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);
    GtkWidget *menu, *start_sep;
    gint pos;

    g_assert(GTK_IS_MENU_ITEM(item) && !GTK_IS_SEPARATOR_MENU_ITEM(item));

    nwam_menu_section_get_section_info(self, sec_id, &menu, &start_sep, NULL);

    menu_section_get_positions(GTK_MENU(menu),
      start_sep, NULL,
      &pos, NULL);

    gtk_menu_shell_insert(GTK_MENU_SHELL(menu), item, pos);
}

static void
nwam_menu_section_append(NwamMenu *self, gint sec_id, GtkWidget *item)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);
    GtkWidget *menu, *start_sep;
    gint pos;

    g_assert(GTK_IS_MENU_ITEM(item) && !GTK_IS_SEPARATOR_MENU_ITEM(item));

    nwam_menu_section_get_section_info(self, sec_id, &menu, &start_sep, NULL);

    menu_section_get_positions(GTK_MENU(menu),
      start_sep, NULL,
      NULL, &pos);

    gtk_menu_shell_insert(GTK_MENU_SHELL(menu), item, pos + 1);
}

static void
nwam_menu_section_insert_sort(NwamMenu *self, gint sec_id, GtkWidget *item)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);
    GtkWidget *menu, *start_sep;
    GList *sorted, *i;
    gint start_pos;

    g_assert(GTK_IS_MENU_ITEM(item) && !GTK_IS_SEPARATOR_MENU_ITEM(item));

    nwam_menu_section_get_section_info(self, sec_id, &menu, &start_sep, NULL);

    /* Sorting */
    menu_section_get_children(GTK_MENU(menu),
      start_sep, NULL, &sorted, &start_pos);

    if (sorted) {
        sorted = g_list_sort(sorted, (GCompareFunc)nwam_menu_item_compare);

        for (i = sorted; i; i = g_list_next(i)) {
            if (item && nwam_menu_item_compare(item, GTK_WIDGET(i->data)) < 0) {
                gtk_menu_shell_insert(GTK_MENU_SHELL(menu), item, start_pos++);
                item = NULL;
            }
            gtk_menu_reorder_child(GTK_MENU(menu), GTK_WIDGET(i->data), start_pos++);
        }
        g_list_free(sorted);

        if (item)
            gtk_menu_shell_insert(GTK_MENU_SHELL(menu), item, start_pos++);
    } else {
        g_assert(start_pos == 0);
        gtk_menu_shell_insert(GTK_MENU_SHELL(menu), item, start_pos);
    }
}

static void
nwam_menu_section_sort(NwamMenu *self, gint sec_id)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);
    GtkWidget *menu, *start_sep;
    GList *sorted, *i;
    gint start_pos;

    nwam_menu_section_get_section_info(self, sec_id, &menu, &start_sep, NULL);

    /* Sorting */
    menu_section_get_children(GTK_MENU(menu),
      start_sep, NULL, &sorted, &start_pos);

    if (sorted) {
        sorted = g_list_sort(sorted, (GCompareFunc)nwam_menu_item_compare);

        for (i = sorted; i; i = g_list_next(i)) {
            gtk_menu_reorder_child(GTK_MENU(menu), GTK_WIDGET(i->data), start_pos++);
        }
        g_list_free(sorted);
    }
}

static void
nwam_menu_get_section_positions(NwamMenu *self, gint sec_id,
  gint *ret_start_pos, gint *ret_end_pos)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);
    GtkWidget *menu, *start_sep;

    nwam_menu_section_get_section_info(self, sec_id, &menu, &start_sep, NULL);

    menu_section_get_positions(GTK_MENU(menu),
      start_sep, NULL,
      ret_start_pos, ret_end_pos);
}

static void
nwam_menu_section_delete(NwamMenu *self, gint sec_id)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);
    GtkWidget *menu, *start_sep;
    GList *children;

    nwam_menu_section_get_section_info(self, sec_id, &menu, &start_sep, NULL);

    /* Sorting */
    menu_section_get_children(GTK_MENU(menu),
      start_sep, NULL, &children, NULL);

    while (children) {
        menu_remove_item(menu, children->data);
        children = g_list_delete_link(children, children);
    }

/*     gtk_widget_hide(start_sep); */
}

/* GtkMenu related */
static void
menu_section_get_children(GtkMenu *menu,
  GtkWidget *start_sep, GtkWidget *end_sep,
  GList **ret_children, gint *ret_start_pos)
{
    GList *children = gtk_container_get_children(GTK_CONTAINER(menu));
    GList *i;
    gint pos = 0;

    g_assert(ret_children);

    /* Delete left part. */
    if (start_sep) {

        for (i = NULL; children && i != children; pos++) {
            if (children->data == (gpointer)start_sep) {
                /* Point to the next to the start separator. */
                i = g_list_next(children);
            }
            children = g_list_delete_link(children, children);
        }
    }

    /* Return the start pos. */
    if (ret_start_pos)
        *ret_start_pos = pos;

    /* Find the end separator. */
    if (children) {

        /* Find the right separator if it isn't passed. */
        if (end_sep) {
            for (i = children; i && i->data != (gpointer)end_sep; i = g_list_next(i)) {}
        } else {
            for (i = children; i && !GTK_IS_SEPARATOR_MENU_ITEM(i->data); i = g_list_next(i)) {}
        }
        /* Delete right part, i point to the right one. */
        if (i) {
            if (i == children) {
                children = NULL;
                g_debug("No sortable elements.");
            } else {
                g_list_previous(i)->next = NULL;
            }
            g_list_free(i);
        }
    } else
        g_message("Sort menu out of range.");

    /* Return found children, or NULL. */
    *ret_children = children;
}

static void
menu_section_get_positions(GtkMenu *menu,
  GtkWidget *start_sep, GtkWidget *end_sep,
  gint *ret_start_pos, gint *ret_end_pos)
{
    GList *children = gtk_container_get_children(GTK_CONTAINER(menu));

    if (ret_start_pos) {
        if (start_sep)
            *ret_start_pos = g_list_index(children, start_sep);
        else
            *ret_start_pos = 0;
    }

    if (ret_end_pos) {
        if (end_sep)
            *ret_end_pos = g_list_index(children, end_sep);
        else
            *ret_end_pos = g_list_length(children) - 1;
    }

    g_list_free(children);
}

static void
menu_foreach_by_name(GtkWidget *widget, gpointer user_data)
{
    ForeachData *data = (ForeachData *)user_data;
    GtkWidget *label;

    label = gtk_bin_get_child(GTK_BIN(widget));

    g_assert(GTK_IS_LABEL(label));

    if (g_ascii_strcasecmp(gtk_label_get_text(GTK_LABEL(label)),
        (gchar*)data->user_data) == 0) {

        g_assert(!data->ret_data);

        data->ret_data = (gpointer)g_object_ref(widget);
    }
}

static void
menu_foreach_by_proxy(GtkWidget *widget, gpointer user_data)
{
    ForeachData *data = (ForeachData *)user_data;

    g_assert(NWAM_IS_OBJ_PROXY_IFACE(widget));

    if (nwam_obj_proxy_get_proxy(NWAM_OBJ_PROXY_IFACE(widget)) == (gpointer)data->user_data) {
        
        g_assert(!data->ret_data);

        data->ret_data = (gpointer)g_object_ref(widget);
    }
}

static GtkMenuItem*
menu_get_item_by_name(GtkMenu *menu, const gchar* name)
{
    ForeachData data;

    data.ret_data = NULL;
    data.user_data = (gpointer)name;

    gtk_container_foreach(GTK_CONTAINER(menu), menu_foreach_by_name, (gpointer)&data);

    return (GtkMenuItem *)data.ret_data;
}

static GtkMenuItem*
menu_get_item_by_proxy(GtkMenu *menu, GObject* proxy)
{
    ForeachData data;

    data.ret_data = NULL;
    data.user_data = (gpointer)proxy;

    gtk_container_foreach(GTK_CONTAINER(menu), menu_foreach_by_proxy, (gpointer)&data);

    return (GtkMenuItem *)data.ret_data;
}


