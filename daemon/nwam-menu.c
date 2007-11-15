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

#include "nwam-menu.h"
#include "nwam-menuitem.h"
#include "nwam-action.h"
#include "libnwamui.h"

#define DEBUG()	g_debug ("[[ %20s : %-4d ]]", __func__, __LINE__)

/* Pop-up menu */
#define NWAM_MANAGER_PROPERTIES "nwam-manager-properties"
#define NWAM_UI UIDATA
#define NWAMUI_MENUITEM_DATA	"nwam_menuitem_data"
#define	NOWIFI	"No wireless networks detected"

static NwamMenu *instance = NULL;

enum {
	MI_PREF_ENV = 0,
	MI_PREF_NET,
	MI_PREF_VPN,
};

#define NWAMUI_PROOT "/StatusIconMenu"
#define MENUITEM_NAME_ENV_PREF "envpref"
#define MENUITEM_NAME_NET_PREF "netpref"
#define MENUITEM_NAME_VPN_PREF "vpnpref"

enum {
	ID_WIFI = 0,
	ID_FAV_WIFI,
	ID_ENV,
	ID_VPN,
	N_ID
};

static const gchar * dynamic_part_menu_path[N_ID] = {
	NWAMUI_PROOT,			/* ID_WIFI */
	NWAMUI_PROOT"/addfavor",	/* ID_FAV_WIFI */
	NWAMUI_PROOT"/swtchenv",	/* ID_ENV */
	NWAMUI_PROOT"/vpn",		/* ID_VPN */
};

struct _NwamMenuPrivate {
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	NwamuiDaemon *daemon;
	guint uid[N_ID];
	GtkActionGroup *group[N_ID];
	gboolean has_wifi;
    GSList *wireless_group;
};

static GtkWidget* nwam_menu_get_menuitem (GObject *obj);
static void nwam_menu_finalize (NwamMenu *self);
static guint nwam_menu_create_static_part (NwamMenu *self);
static void nwam_menu_create_wifi_menuitems (GObject *daemon, GObject *wifi, gpointer data);
static void nwam_menu_create_env_menuitems (NwamMenu *self);
static void nwam_menu_create_vpn_menuitems (NwamMenu *self);
static void nwam_menu_recreate_menuitems (NwamMenu *self);
static void nwam_menu_connect (NwamMenu *self);

/* call back */
static void on_activate_about (GtkAction *action, gpointer data);
static void on_activate_help (GtkAction *action, gpointer udata);
static void on_activate_pref (GtkAction *action, gpointer udata);
static void on_activate_addwireless (GtkAction *action, gpointer udata);
static void on_activate_env (GtkAction *action, gpointer data);
static void on_activate_enm (GtkAction *action, gpointer data);
static void on_nwam_ncu_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);
static void on_nwam_wifi_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);
static void on_nwam_env_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);
static void on_nwam_enm_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);
static void on_ncu_up (NwamuiDaemon* daemon, NwamuiNcu* ncu, gpointer data);
static void on_ncu_down (NwamuiDaemon* daemon, NwamuiNcu* ncu, gpointer data);
static void on_active_env_changed (NwamuiDaemon* daemon, NwamuiEnv* env, gpointer data);

/* utils */
static GtkWidget* nwam_display_name_to_menuitem (NwamMenu *self, const gchar *dn, int id);

/* utils */
static GtkWidget* nwam_display_name_to_menuitem (NwamMenu *self, const gchar *dn, int id);

G_DEFINE_TYPE (NwamMenu, nwam_menu, G_TYPE_OBJECT)

static GtkActionEntry entries[] = {
    { MENUITEM_NAME_ENV_PREF,
      NULL, N_("Location Preferences"), 
      NULL, NULL,
      G_CALLBACK(on_activate_pref) },
    { "joinwireless",
      NULL, N_("Join Unlisted Wireless Network..."),
      NULL, NULL,
      G_CALLBACK(on_activate_addwireless) },
    { MENUITEM_NAME_NET_PREF,
      NULL, N_("Network Preferences"),
      NULL, NULL,
      G_CALLBACK(on_activate_pref) },
    { MENUITEM_NAME_VPN_PREF,
      NULL, N_("VPN Preferences"),
      NULL, NULL,
      G_CALLBACK(on_activate_pref) },
    { "swtchenv", NULL, N_("Switch Location"), NULL, NULL, NULL },
    { "vpn", NULL, N_("VPN"), NULL, NULL, NULL },
    { "help", GTK_STOCK_HELP, N_("Help"), NULL, NULL,
      G_CALLBACK (on_activate_help) }
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
	g_signal_connect(self->prv->daemon, "wifi_scan_result",
                     (GCallback)nwam_menu_create_wifi_menuitems, self);
    g_signal_connect(self->prv->daemon, "ncu_up",
                     G_CALLBACK(on_ncu_up), (gpointer) self);
    g_signal_connect(self->prv->daemon, "ncu_down",
                     G_CALLBACK(on_ncu_down), (gpointer) self);
    g_signal_connect(self->prv->daemon, "active_env_changed",
                     G_CALLBACK(on_active_env_changed), (gpointer) self);

	self->prv->action_group = gtk_action_group_new ("StatusIconMenuActions");
	gtk_action_group_set_translation_domain (self->prv->action_group, NULL);
	self->prv->ui_manager = gtk_ui_manager_new ();
	gtk_ui_manager_insert_action_group (self->prv->ui_manager, self->prv->action_group, 0);

    nwam_menu_create_static_part (self);
	nwam_menu_create_env_menuitems (self);
	nwam_menu_create_vpn_menuitems (self);
    /* test code */
    nwam_menu_recreate_menuitems (self);

    nwam_menu_connect (self);
}

static void
nwam_menu_finalize (NwamMenu *self)
{
	g_object_unref (G_OBJECT(self->prv->ui_manager));
	g_object_unref (G_OBJECT(self->prv->action_group));
	g_object_unref (G_OBJECT(self->prv->daemon));

	/* TODO, release all the resources */

	g_free(self->prv);
	self->prv = NULL;
	
	(*G_OBJECT_CLASS(nwam_menu_parent_class)->finalize) (G_OBJECT(self));
}

static GtkWidget*
nwam_display_name_to_menuitem (NwamMenu *self, const gchar *dn, int id)
{
    gchar *path = NULL;
    GtkWidget *menuitem = NULL;

    path = g_build_path ("/", dynamic_part_menu_path[id], dn, NULL);
    menuitem = gtk_ui_manager_get_widget (self->prv->ui_manager, path);

    g_free (path);
    return menuitem;
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

static void
nwam_menu_recreate_menuitems (NwamMenu *self)
{
	if (self->prv->uid[ID_WIFI] > 0) {
		gtk_ui_manager_remove_ui (self->prv->ui_manager, self->prv->uid[ID_WIFI]);
		gtk_ui_manager_remove_action_group (self->prv->ui_manager, self->prv->group[ID_WIFI]);
	}
    self->prv->wireless_group = NULL;
	self->prv->has_wifi = FALSE;
	self->prv->uid[ID_WIFI] = gtk_ui_manager_new_merge_id (self->prv->ui_manager);
	g_assert (self->prv->uid[ID_WIFI] > 0);
	self->prv->group[ID_WIFI] = gtk_action_group_new ("wifi_action_group");
	gtk_ui_manager_insert_action_group (self->prv->ui_manager, self->prv->group[ID_WIFI], 0);
    g_object_unref (G_OBJECT(self->prv->group[ID_WIFI]));
	nwamui_daemon_wifi_start_scan (self->prv->daemon);
}

static guint
nwam_menu_create_static_part (NwamMenu *self)
{
    guint mid;
    GError *error;

    if ((mid = gtk_ui_manager_add_ui_from_file (self->prv->ui_manager,
                                                NWAM_UI, &error)) != 0) {
        gtk_action_group_add_actions (self->prv->action_group,
                                      entries,
                                      G_N_ELEMENTS (entries),
                                      NULL);
    } else {
        g_error ("Can't find %s: %s", NWAM_UI, error->message);
        g_error_free (error);
    }
    return mid;
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
	argv[argc++] = nwam_arg;
	g_debug ("\nnwam-menu-exec: %s\n", nwam_arg);

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
on_activate_pref (GtkAction *action, gpointer udata)
{
    const gchar *name;
	gchar *argv = NULL;

    name = gtk_action_get_name (action);

    if (g_ascii_strcasecmp (name, MENUITEM_NAME_ENV_PREF) == 0) {
		argv = "-e";
    } else if (g_ascii_strcasecmp (name, MENUITEM_NAME_NET_PREF) == 0) {
		argv = "-p";
    } else if (g_ascii_strcasecmp (name, MENUITEM_NAME_VPN_PREF) == 0) {
        argv = "-n";
    } else {
        g_assert_not_reached ();
    }
    nwam_exec(argv);
}

static void
on_activate_about (GtkAction *action, gpointer data)
{
    gtk_show_about_dialog (NULL, 
                       "name", _("NWAM Manager"),
                       "title", _("About NWAM Manager"),
                       "copyright", _("2007 Sun Microsystems, Inc  All rights reserved\nUse is subject to license terms."),
                       "website", _("http://www.sun.com/"),
                       NULL);
}

static void
on_activate_help (GtkAction *action, gpointer udata)
{
    nwamui_util_show_help ("");
}

static void
on_nwam_wifi_enable (GtkAction *action, gpointer data)
{
    NwamMenu *self = NWAM_MENU (data);
    NwamuiNcp* active_ncp = nwamui_daemon_get_active_ncp (self->prv->daemon);
	NwamuiWifiNet *wifi = NWAMUI_WIFI_NET (g_object_get_data(G_OBJECT(action), NWAMUI_MENUITEM_DATA));
    GtkTreeModel *model = GTK_TREE_MODEL(nwamui_ncp_get_ncu_list_store (active_ncp));
    NwamuiNcu *ncu;
    GtkTreeIter iter;

    DEBUG();

    if (gtk_tree_model_get_iter_first (model, &iter)) {
        do {
            gtk_tree_model_get (model, &iter, 0, &ncu, -1);
            if (nwamui_ncu_get_ncu_type (ncu) == NWAMUI_NCU_TYPE_WIRELESS) {
                if (wifi != nwamui_ncu_get_wifi_info (ncu)) {
                    nwamui_ncu_set_wifi_info (ncu, wifi);
                }
                nwamui_ncu_set_active (ncu, TRUE);
                // TODO, need commit it.
                return;
            }
        } while (gtk_tree_model_iter_next (model, &iter));
    }
}

static void
on_activate_addwireless (GtkAction *action, gpointer data)
{
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
}

static void
on_activate_env (GtkAction *action, gpointer data)
{
	NwamMenu *self = NWAM_MENU (data);
	NwamuiEnv *env = NULL;
	env = NWAMUI_ENV(g_object_get_data(G_OBJECT(action), NWAMUI_MENUITEM_DATA));
	if (!nwamui_daemon_is_active_env(self->prv->daemon, env)) {
		nwamui_daemon_set_active_env (self->prv->daemon, env);
	}
}

static void
on_activate_enm (GtkAction *action, gpointer data)
{
	NwamMenu *self = NWAM_MENU (data);
	NwamuiEnm *nwamobj = NULL;
	nwamobj = NWAMUI_ENM(g_object_get_data(G_OBJECT(action), NWAMUI_MENUITEM_DATA));
	if (nwamui_enm_get_active(nwamobj)) {
		nwamui_enm_set_active (nwamobj, FALSE);
	} else {
		nwamui_enm_set_active (nwamobj, TRUE);
    }
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
        if (g_ascii_strcasecmp(arg1->name, "wifi_info") == 0) {
            NwamuiWifiNet *wifi;
            GtkWidget *menuitem;
            gchar *m_name;

            wifi = nwamui_ncu_get_wifi_info (ncu);
            m_name = nwamui_wifi_net_get_essid (wifi); 
            menuitem = nwam_display_name_to_menuitem (self, m_name, ID_WIFI);
            g_free (m_name);

            if (menuitem) {
                //if (! gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM(menuitem)))
                    gtk_check_menu_item_toggled (GTK_CHECK_MENU_ITEM(menuitem));
            }
            break;
        }
    case NWAMUI_NCU_TYPE_WIRED:
        return;
    default:
        g_assert_not_reached ();
    }
}

static void
on_nwam_wifi_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
	NwamMenu *self = NWAM_MENU (data);
    NwamuiWifiNet *nwamobj;
    GtkWidget *menuitem;
    GtkAction *action;
    gchar *m_name;
    gchar *stock_id;
    GtkWidget* img = NULL;
    gdouble strength;

    nwamobj = NWAMUI_WIFI_NET(gobject);
    m_name = nwamui_wifi_net_get_essid (nwamobj);
    menuitem = nwam_display_name_to_menuitem (self, m_name, ID_WIFI);
    action = gtk_action_group_get_action (self->prv->group[ID_WIFI], m_name);
    g_free (m_name);

    g_assert (menuitem);
    g_assert (action);

    switch (nwamui_wifi_net_get_signal_strength (nwamobj)) {
    case NWAMUI_WIFI_STRENGTH_NONE:
    case NWAMUI_WIFI_STRENGTH_VERY_WEAK:
        stock_id = NWAM_ICON_WIRELESS_STRENGTH_0_24;
        break;
    case NWAMUI_WIFI_STRENGTH_WEAK:
        stock_id = NWAM_ICON_WIRELESS_STRENGTH_25_49;
        break;
    case NWAMUI_WIFI_STRENGTH_GOOD:
        stock_id = NWAM_ICON_WIRELESS_STRENGTH_50_74;
        break;
    case NWAMUI_WIFI_STRENGTH_VERY_GOOD:
    case NWAMUI_WIFI_STRENGTH_EXCELLENT:
        stock_id = NWAM_ICON_WIRELESS_STRENGTH_75_100;
        break;
    default:
        g_assert_not_reached ();
    }
    img = gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_MENU); 
    nwam_menu_item_set_widget (NWAM_MENU_ITEM(menuitem), img, 0);

    switch (nwamui_wifi_net_get_security (nwamobj)) {
    case NWAMUI_WIFI_SEC_NONE:
        img = gtk_image_new_from_stock (NWAMUI_WIFI_SEC_NONE_STOCKID, GTK_ICON_SIZE_MENU);
        break;
    default:
        img = gtk_image_new_from_stock (GTK_STOCK_YES, GTK_ICON_SIZE_MENU);
        break;
    }
    nwam_menu_item_set_widget (NWAM_MENU_ITEM(menuitem), img, 1);
}

static void
on_nwam_env_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
	NwamMenu *self = NWAM_MENU (data);
    NwamuiEnv *nwamobj;

    nwamobj = NWAMUI_ENV(g_object_get_data(G_OBJECT(gobject), NWAMUI_MENUITEM_DATA));
    g_debug("menuitem get env notify %s changed\n", arg1->name);
}

static void
on_nwam_enm_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
	NwamMenu *self = NWAM_MENU (data);
    NwamuiEnm *nwamobj;
    gchar *m_name;
    GtkWidget *label, *menuitem;
    gchar *new_text;

    nwamobj = NWAMUI_ENM(gobject);

    m_name = nwamui_enm_get_name (nwamobj);
    menuitem = nwam_display_name_to_menuitem (self, m_name, ID_VPN);

    g_assert (menuitem);

    label = gtk_bin_get_child (GTK_BIN(menuitem));
    if (nwamui_enm_get_active (nwamobj)) {
        new_text = g_strconcat (_("Stop "), m_name, NULL);
        gtk_label_set_text (GTK_LABEL (label), new_text);
    } else {
        new_text = g_strconcat (_("Start "), m_name, NULL);
        gtk_label_set_text (GTK_LABEL (label), new_text);
    }
    g_free (new_text);
    g_free (m_name);
}

static void
on_ncu_up (NwamuiDaemon* daemon, NwamuiNcu* ncu, gpointer data)
{
    DEBUG ();
}

static void
on_ncu_down (NwamuiDaemon* daemon, NwamuiNcu* ncu, gpointer data)
{
    DEBUG ();
}

static void
on_active_env_changed (NwamuiDaemon* daemon, NwamuiEnv* env, gpointer data)
{
    DEBUG ();
}

/*
 * dynamically create a menuitem for wireless/wired and insert in to top
 * of the basic pop up menus. I presume to create toggle menu item.
 * REF UI design P2.
 */

/*
 * constuct a menuitem for a nwam object
 */
static void
nwam_menu_menuitem_postfix (NwamMenu *self, GObject *obj)
{
	GtkWidget *menuitem = NULL;
	gchar *m_name = NULL;

	if (NWAMUI_IS_WIFI_NET (obj)) {
        on_nwam_wifi_notify_cb(obj, NULL, (gpointer)self);
	} else if (NWAMUI_IS_NCU (obj)) {
	} else if (NWAMUI_IS_ENV (obj)) {
	} else if (NWAMUI_IS_ENM (obj)) {
        on_nwam_enm_notify_cb(obj, NULL, (gpointer)self);
	} else {
		g_assert_not_reached ();
	}
}

static void
nwam_menu_create_wifi_menuitems (GObject *daemon, GObject *wifi, gpointer data)
{
	NwamMenu *self = NWAM_MENU (data);
	GtkAction *action = NULL;
	
	if (wifi) {
		gchar *name = NULL;
		
		name = nwamui_wifi_net_get_essid(NWAMUI_WIFI_NET(wifi));
		action = GTK_ACTION(nwam_action_new (name, name, NULL, GTK_STOCK_OK));
		g_object_set_data(G_OBJECT(action), NWAMUI_MENUITEM_DATA, wifi);
        g_signal_connect (G_OBJECT (action), "activate",
                          G_CALLBACK(on_nwam_wifi_enable), (gpointer)self);
		g_signal_connect (wifi, "notify",
                          G_CALLBACK(on_nwam_wifi_notify_cb), (gpointer)self);
		gtk_action_group_add_action_with_accel (self->prv->group[ID_WIFI], action, NULL);
		gtk_radio_action_set_group (GTK_RADIO_ACTION(action),
                                    self->prv->wireless_group);
		self->prv->wireless_group = gtk_radio_action_get_group(GTK_RADIO_ACTION(action));
		g_object_unref (action);

		/* menu */
		gtk_ui_manager_add_ui (self->prv->ui_manager,
				       self->prv->uid[ID_WIFI],
				       NWAMUI_PROOT"/end_fav_wifi",
				       name, /* name */
				       name, /* action */
				       GTK_UI_MANAGER_MENUITEM,
				       TRUE);

		nwam_menu_menuitem_postfix(self, wifi);
		g_free(name);
		self->prv->has_wifi = TRUE;
	} else if (!self->prv->has_wifi) {
		action = GTK_ACTION(gtk_action_new (NOWIFI, _(NOWIFI), NULL, NULL));
		gtk_action_set_sensitive (action, FALSE);
		gtk_action_group_add_action_with_accel (self->prv->group[ID_WIFI], action, NULL);
		g_object_unref (action);

		gtk_ui_manager_add_ui (self->prv->ui_manager,
				       self->prv->uid[ID_WIFI],
				       dynamic_part_menu_path[ID_WIFI],
				       NOWIFI, /* name */
				       NOWIFI, /* action */
				       GTK_UI_MANAGER_MENUITEM,
				       FALSE);
	}
}

static void
nwam_menu_create_env_menuitems (NwamMenu *self)
{
	GtkAction* action = NULL;
	GList *idx = NULL;
	GSList *group = NULL;
	
	self->prv->uid[ID_ENV] = gtk_ui_manager_new_merge_id (self->prv->ui_manager);
	self->prv->group[ID_ENV] = gtk_action_group_new ("fav_wifi_action_group");
	gtk_ui_manager_insert_action_group (self->prv->ui_manager, self->prv->group[ID_ENV], 0);
	g_object_unref(G_OBJECT(self->prv->group[ID_ENV]));
	
	for (idx = nwamui_daemon_get_env_list(self->prv->daemon); idx; idx = g_list_next(idx)) {
		gchar *name = NULL;
		name = nwamui_env_get_name(NWAMUI_ENV(idx->data));
		action = GTK_ACTION(gtk_radio_action_new(name,
			name,
			NULL,
			NULL,
			FALSE));
		g_object_set_data(G_OBJECT(action), NWAMUI_MENUITEM_DATA, idx->data);
		g_signal_connect (G_OBJECT (action), "activate",
                          G_CALLBACK(on_activate_env), (gpointer)self);
		g_signal_connect (idx->data, "notify",
                          G_CALLBACK(on_nwam_env_notify_cb), (gpointer)self);
		gtk_action_group_add_action_with_accel(self->prv->group[ID_ENV], action, NULL);
		gtk_radio_action_set_group(GTK_RADIO_ACTION(action), group);
		group = gtk_radio_action_get_group(GTK_RADIO_ACTION(action));
		g_object_unref(action);
		
		gtk_ui_manager_add_ui(self->prv->ui_manager,
			self->prv->uid[ID_ENV],
			dynamic_part_menu_path[ID_ENV],
			name, /* name */
			name, /* action */
			GTK_UI_MANAGER_MENUITEM,
			FALSE);
		nwam_menu_menuitem_postfix(self, idx->data);
		g_free(name);
	}
}

static void
nwam_menu_create_vpn_menuitems(NwamMenu *self)
{
	GtkAction* action = NULL;
	GList *idx = NULL;
	
	self->prv->uid[ID_VPN] = gtk_ui_manager_new_merge_id(self->prv->ui_manager);
	self->prv->group[ID_VPN] = gtk_action_group_new("vpn_action_group");
	gtk_ui_manager_insert_action_group(self->prv->ui_manager, self->prv->group[ID_VPN], 0);
	g_object_unref(G_OBJECT(self->prv->group[ID_VPN]));
	
	for (idx = nwamui_daemon_get_enm_list(self->prv->daemon); idx; idx = g_list_next(idx)) {
		gchar *name = NULL;
		name = nwamui_enm_get_name(NWAMUI_ENM(idx->data));
		action = GTK_ACTION(gtk_action_new(name,
			name,
			NULL,
			NULL));
		g_object_set_data(G_OBJECT(action), NWAMUI_MENUITEM_DATA, idx->data);
		g_signal_connect (G_OBJECT (action), "activate",
                          G_CALLBACK(on_activate_enm), (gpointer)self);
		g_signal_connect (idx->data, "notify::active",
                          G_CALLBACK(on_nwam_enm_notify_cb), (gpointer)self);
		gtk_action_group_add_action_with_accel(self->prv->group[ID_VPN], action, NULL);
		g_object_unref(action);
		
		gtk_ui_manager_add_ui(self->prv->ui_manager,
			self->prv->uid[ID_VPN],
			NWAMUI_PROOT"/vpn/end_vpn",
			name, /* name */
			name, /* action */
			GTK_UI_MANAGER_MENUITEM,
			TRUE);
		nwam_menu_menuitem_postfix(self, idx->data);
		g_free(name);
	}
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
	nwam_menu_recreate_menuitems (self);
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
