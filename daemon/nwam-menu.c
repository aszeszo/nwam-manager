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
#include "nwam-menuitem.h"
#include "nwam-action.h"
#include "libnwamui.h"

#define DEBUG()	g_debug ("[[ %20s : %-4d ]]", __func__, __LINE__)

/* Pop-up menu */
#define NWAM_MANAGER_PROPERTIES "nwam-manager-properties"
#define NWAMUI_MENUITEM_DATA	"nwam_menuitem_data"
#define	NOWIFI	"No wireless networks detected"

static NwamMenu *instance = NULL;

#define NWAMUI_ACTION_ROOT	"StatusIconMenuActions"
#define NWAMUI_ROOT "StatusIconMenu"
#define NWAMUI_PROOT "/"NWAMUI_ROOT
/* NWAM UI 1.0 */
#define MENUITEM_SECTION_FAV_AP_BEGIN	NWAMUI_PROOT"/section_fav_ap_begin"
#define MENUITEM_SECTION_FAV_AP_END	NWAMUI_PROOT"/section_fav_ap_end"
#define MENUITEM_SECTION_VPN_END	NWAMUI_PROOT"/vpn/section_vpn_end"
#define MENUITEM_NAME_ENV_PREF "envpref"
#define MENUITEM_NAME_NET_PREF "netpref"
#define MENUITEM_NAME_VPN_PREF "vpnpref"
#define MENUITEM_NAME_IF_PRIOR "if_prior"
/* NWAM UI 0.5 */
#define MENUITEM_SECTION_IF_END	NWAMUI_PROOT"/section_if_end"

enum {
	ID_WIFI = 0,
	ID_FAV_WIFI,
	ID_ENV,
	ID_VPN,
    /* 0.5 */
	ID_IF,
	N_ID
};

static const gchar * dynamic_part_menu_path[N_ID] = {
	NWAMUI_PROOT,			/* ID_WIFI */
	NWAMUI_PROOT"/addfavor",	/* ID_FAV_WIFI */
	NWAMUI_PROOT"/swtchenv",	/* ID_ENV */
	NWAMUI_PROOT"/vpn",		/* ID_VPN */
    /* 0.5 */
    NWAMUI_PROOT,			/* ID_IF */
};

struct _NwamMenuPrivate {
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	NwamuiDaemon *daemon;
	guint uid[N_ID];
	GtkActionGroup *group[N_ID];

	gboolean has_wifi;
    gboolean force_wifi_rescan_due_to_env_changed;
    gboolean force_wifi_rescan;
    GSList *wireless_group;	/* Radio groups for menus */
    /* 0.5 */
    GSList *if_group;	/* Radio groups for menus */
};

static void nwam_menu_finalize (NwamMenu *self);
static GtkWidget* nwam_menu_get_menuitem (GObject *obj);
static void nwam_menu_connect (NwamMenu *self);
static guint nwam_menu_create_static_part (NwamMenu *self);
static void nwam_menu_recreate_menuitem_group (NwamMenu *self, int group_id,
  const gchar *group_name)

/* 0.5 */
static void nwam_menu_recreate_if_menuitems (NwamMenu *self);
static void nwam_menu_create_if_menuitems (GObject *daemon, GObject *wifi, gpointer data);
/* 1.0 */
static void nwam_menu_recreate_wifi_menuitems (NwamMenu *self);
static void nwam_menu_create_wifi_menuitems (GObject *daemon, GObject *wifi, gpointer data);
static void nwam_menu_create_env_menuitems (NwamMenu *self);
static void nwam_menu_create_vpn_menuitems (NwamMenu *self);

static void notification_joinwireless_cb (NotifyNotification *n, gchar *action, gpointer data);
static void notification_listwireless_cb (NotifyNotification *n, gchar *action, gpointer data);

/* utils */
static GtkWidget* nwam_display_name_to_menuitem (NwamMenu *self, const gchar *dn, int id);

/* call back */
/* Notification menuitem events */
/* NWAM UI 0.5 */
static void on_activate_if_prior (GtkAction *action, gpointer data);
/* NWAM UI 1.0 */
static void on_activate_about (GtkAction *action, gpointer data);
static void on_activate_help (GtkAction *action, gpointer udata);
static void on_activate_pref (GtkAction *action, gpointer udata);
static void on_activate_joinwireless (GtkAction *action, gpointer udata);
static void on_activate_env (GtkAction *action, gpointer data);
static void on_activate_enm (GtkAction *action, gpointer data);

/* NWAMUI object events */
static void on_nwam_ncu_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);
static void on_nwam_wifi_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);
static void on_nwam_env_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);
static void on_nwam_enm_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);

/* NWAMUI daemon events */
static void event_daemon_info (NwamuiDaemon* daemon, gpointer data, gpointer udata);
static void event_ncu_up (NwamuiDaemon* daemon, NwamuiNcu* ncu, gpointer data);
static void event_ncu_down (NwamuiDaemon* daemon, NwamuiNcu* ncu, gpointer data);
static void event_active_env_changed (NwamuiDaemon* daemon, NwamuiEnv* env, gpointer data);
static void event_add_wifi_fav (NwamuiDaemon* daemon, NwamuiWifiNet* wifi, gpointer data);

G_DEFINE_TYPE (NwamMenu, nwam_menu, G_TYPE_OBJECT)

static GtkActionEntry entries[] = {
    /* NWAM UI 0.5 */
    { MENUITEM_NAME_IF_PRIOR,
      NULL, N_("Edit interface priorities..."), 
      NULL, NULL,
      G_CALLBACK(on_activate_if_prior) },
    /* NWAM UI 1.0 */
    { MENUITEM_NAME_ENV_PREF,
      NULL, N_("Location Preferences"), 
      NULL, NULL,
      G_CALLBACK(on_activate_pref) },
    { "joinwireless",
      NULL, N_("Join Unlisted Wireless Network..."),
      NULL, NULL,
      G_CALLBACK(on_activate_joinwireless) },
    { MENUITEM_NAME_NET_PREF,
      NULL, N_("Network Preferences"),
      NULL, NULL,
      G_CALLBACK(on_activate_pref) },
    { MENUITEM_NAME_VPN_PREF,
      NULL, N_("VPN Preferences"),
      NULL, NULL,
      G_CALLBACK(on_activate_pref) },
    { "swtchenv",
      NULL, N_("Switch Location"),
      NULL, NULL, NULL },
    { "vpn",
      NULL, N_("VPN"),
      NULL, NULL, NULL },
    { "help",
      GTK_STOCK_HELP, N_("Help"),
      NULL, NULL,
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

	self->prv->action_group = gtk_action_group_new (NWAMUI_ACTION_ROOT);
	gtk_action_group_set_translation_domain (self->prv->action_group, NULL);
	self->prv->ui_manager = gtk_ui_manager_new ();
	gtk_ui_manager_insert_action_group (self->prv->ui_manager, self->prv->action_group, 0);

    nwam_menu_create_static_part (self);
	nwam_menu_create_env_menuitems (self);
	nwam_menu_create_vpn_menuitems (self);
    /* test code */
    /* 1.0 */
    nwam_menu_recreate_wifi_menuitems (self);
    /* 0.5 */
    nwam_menu_recreate_if_menuitems (self);

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
nwam_menu_recreate_menuitem_group (NwamMenu *self,
  int group_id,
  const gchar *group_name)
{
	if (self->prv->uid[group_id] > 0) {
		gtk_ui_manager_remove_ui (self->prv->ui_manager, self->prv->uid[group_id]);
		gtk_ui_manager_remove_action_group (self->prv->ui_manager, self->prv->group[group_id]);
	}
	self->prv->uid[group_id] = gtk_ui_manager_new_merge_id (self->prv->ui_manager);
	g_assert (self->prv->uid[group_id] > 0);
	self->prv->group[group_id] = gtk_action_group_new (group_name);
	gtk_ui_manager_insert_action_group (self->prv->ui_manager, self->prv->group[group_id], 0);
    g_object_unref (G_OBJECT(self->prv->group[group_id]));
}

static void
nwam_menu_recreate_wifi_menuitems (NwamMenu *self)
{
    self->prv->wireless_group = NULL;
	self->prv->has_wifi = FALSE;
    nwam_menu_recreate_menuitem_group (self, ID_WIFI, "wifi_action_group");

    /* set force rescan flag so that we can identify daemon scan wifi event */
    self->prv->force_wifi_rescan = TRUE;

	nwamui_daemon_wifi_start_scan (self->prv->daemon);
}

static void
nwam_menu_recreate_if_menuitems (NwamMenu *self)
{
    self->prv->if_group = NULL;
    nwam_menu_recreate_menuitem_group (self, ID_IF, "if_action_group");

    /* TODO */
}

static guint
nwam_menu_create_static_part (NwamMenu *self)
{
    guint mid;
    GError *error;

    if ((mid = gtk_ui_manager_add_ui_from_file (self->prv->ui_manager,
                                                UIDATA, &error)) != 0) {
        gtk_action_group_add_actions (self->prv->action_group,
          entries,
          G_N_ELEMENTS (entries),
          (gpointer) self);
    } else {
        g_error ("Can't find %s: %s", UIDATA, error->message);
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

/**
 * on_activate_if_prior:
 * @action:
 * @udata:
 *
 * NWAM UI 0.5 only
 * Pop up a dialog to change the priority of all interfaces.
 */
static void
on_activate_if_prior (GtkAction *action, gpointer udata)
{
    g_debug("TODO");
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
on_activate_joinwireless (GtkAction *action, gpointer data)
{
	NwamMenu *self = NWAM_MENU (data);
	if (data) {
#if 0
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
#endif
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
    GtkWidget* img = NULL;

    nwamobj = NWAMUI_WIFI_NET(gobject);
    m_name = nwamui_wifi_net_get_essid (nwamobj);
    menuitem = nwam_display_name_to_menuitem (self, m_name, ID_WIFI);
    action = gtk_action_group_get_action (self->prv->group[ID_WIFI], m_name);
    g_free (m_name);

    g_assert (menuitem);
    g_assert (action);

    img = gtk_image_new_from_pixbuf (
            nwamui_util_get_wireless_strength_icon(nwamui_wifi_net_get_signal_strength(nwamobj), TRUE )); 
    nwam_menu_item_set_widget (NWAM_MENU_ITEM(menuitem), img, 0);

    img = gtk_image_new_from_pixbuf (
            nwamui_util_get_network_security_icon(nwamui_wifi_net_get_security (nwamobj), TRUE )); 
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
event_ncu_up (NwamuiDaemon* daemon, NwamuiNcu* ncu, gpointer data)
{
    gchar *sname;
    gchar *ifname;
    gchar *addr;
    gint speed;
    gint signal;
    gchar *summary, *body;

    DEBUG();

	/* generic info */
    ifname = nwamui_ncu_get_device_name (ncu);
    if (!nwamui_ncu_get_ipv6_active (ncu)) {
        addr = nwamui_ncu_get_ipv4_address (ncu);
    } else {
        addr = nwamui_ncu_get_ipv6_address (ncu);
    }
    speed = nwamui_ncu_get_speed (ncu);

    switch (nwamui_ncu_get_ncu_type (ncu)) {
    case NWAMUI_NCU_TYPE_WIRED:
        sname = nwamui_ncu_get_vanity_name (ncu);
        summary = g_strdup_printf (_("'%s' is connected"), sname);
        body = g_strdup_printf (_("Interface: %s\n"
                                  "Address: %s\n"
                                  "Speed: %d Mb/s\n"),
                                ifname, addr, speed);
        break;
    case NWAMUI_NCU_TYPE_WIRELESS: {
        NwamuiWifiNet *wifi = nwamui_ncu_get_wifi_info (ncu);
        sname = nwamui_wifi_net_get_essid (wifi);
        signal = (gint) ((gfloat) (nwamui_wifi_net_get_signal_strength (wifi) /
                                   NWAMUI_WIFI_STRENGTH_LAST) * 100);
        summary = g_strdup_printf (_("Connected to '%s'"), sname);
        body = g_strdup_printf (_("Interface: %s\n"
                                  "Address: %s\n"
                                  "Speed: %d Mb/s\n"
                                  "Signal Strength: %d%%\n"),
                                ifname, addr, speed);
        break;
    }
    default:
        g_assert_not_reached ();
    }
    nwam_notification_show_message (summary, body, NULL);
    g_free (sname);
    g_free (ifname);
    g_free (addr);
    g_free (summary);
    g_free (body);
}

static void
event_ncu_down (NwamuiDaemon* daemon, NwamuiNcu* ncu, gpointer data)
{
	NwamMenu *self = NWAM_MENU (data);
    gchar *sname;
    gchar *summary, *body;

    DEBUG();

    switch (nwamui_ncu_get_ncu_type (ncu)) {
    case NWAMUI_NCU_TYPE_WIRED:
        sname = nwamui_ncu_get_vanity_name (ncu);
        summary = g_strdup_printf (_("'%s' is disconnected"), sname);
        body = g_strdup (_("Cable unplugged"));
        nwam_notification_show_message (summary, body, NULL);
        break;
    case NWAMUI_NCU_TYPE_WIRELESS: {
        NwamuiWifiNet *wifi = nwamui_ncu_get_wifi_info (ncu);
        sname = nwamui_wifi_net_get_essid (wifi);
        summary = g_strdup_printf (_("Disconnected from '%s'"), sname);
        body = g_strdup (_("Wireless signal lost.\nClick here to join another\nwireless network."));
        nwam_notification_show_message_with_action (summary,
          body,
          NULL,
          NULL,
          NULL,
          notification_joinwireless_cb,
          (gpointer) g_object_ref (self),
          (GFreeFunc) g_object_unref);
        break;
    }
    default:
        g_assert_not_reached ();
    }
    g_free (sname);
    g_free (summary);
    g_free (body);
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
    nwam_notification_show_message (summary, body, NULL);

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
    gchar *summary;
    gchar *name;

    /* TODO - we should send notification iff wifi is automatically added */
    /* according to v1.5 P4 */
    name = nwamui_wifi_net_get_essid (wifi);
    summary = g_strdup_printf (_("Wireless network '%s' added to favorites"),
      name);
    
    nwam_notification_show_message_with_action (summary,
      _("Click here to view or edit your list of favorite\nwireless networks."),
      NULL,
      NULL,
      NULL,
      notification_listwireless_cb,
      (gpointer) g_object_ref (self),
      (GFreeFunc) g_object_unref);
}

static void
event_daemon_info (NwamuiDaemon* daemon, gpointer data, gpointer udata)
{
    nwam_notification_show_message ("Information", (gchar *)data, NULL);
}

static void
notification_joinwireless_cb (NotifyNotification *n, gchar *action, gpointer data)
{
    nwam_exec("--add-wireless-dialog=");
}

static void
notification_listwireless_cb (NotifyNotification *n, gchar *action, gpointer data)
{
    nwam_exec("--list-fav-wireless=");
}

/**
 * nwam_menu_create_if_menuitems:
 *
 * 0.5 only, create a menuitem for each network interface.
 */
static void
nwam_menu_create_if_menuitems (GObject *daemon, GObject *wifi, gpointer data)
{
    /* TODO */
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
	GtkAction *action = NULL;
	
    /* TODO - Make this more efficient */

	if (wifi) {
		gchar *name = NULL;
		
		name = nwamui_wifi_net_get_essid(NWAMUI_WIFI_NET(wifi));
		action = GTK_ACTION(nwam_action_new (name, name, NULL, GTK_STOCK_OK));
		g_object_set_data(G_OBJECT(action), NWAMUI_MENUITEM_DATA, wifi);
        g_signal_connect (G_OBJECT (action), "activate",
                          G_CALLBACK(on_nwam_wifi_enable), (gpointer)self);
		g_signal_connect (wifi, "notify",
                          G_CALLBACK(on_nwam_wifi_notify_cb), (gpointer)self);
        /* populate the data */
        on_nwam_wifi_notify_cb(wifi, NULL, (gpointer)self);
		gtk_action_group_add_action_with_accel (self->prv->group[ID_WIFI], action, NULL);
		gtk_radio_action_set_group (GTK_RADIO_ACTION(action),
                                    self->prv->wireless_group);
		self->prv->wireless_group = gtk_radio_action_get_group(GTK_RADIO_ACTION(action));
		g_object_unref (action);

		/* menu */
		gtk_ui_manager_add_ui (self->prv->ui_manager,
          self->prv->uid[ID_WIFI],
          MENUITEM_SECTION_FAV_AP_END,
          name, /* name */
          name, /* action */
          GTK_UI_MANAGER_MENUITEM,
          TRUE);

		g_free(name);
		self->prv->has_wifi = TRUE;
	} else {
        /* scan is over */

        /* we must clear force rescan flag if it is */
        if (self->prv->force_wifi_rescan) {
            self->prv->force_wifi_rescan = FALSE;
        }

        /* no wifi */
        if (!self->prv->has_wifi) {
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

            /* happens iff env is just changed and wifi rescan just happenned */
            if (self->prv->force_wifi_rescan_due_to_env_changed) {
                self->prv->force_wifi_rescan_due_to_env_changed = FALSE;
                nwam_notification_show_message_with_action (_("No wireless networks detected"),
                  _("Click here to join an unlisted\nwireless network."),
                  NULL,
                  NULL,
                  NULL,
                  notification_joinwireless_cb,
                  (gpointer) g_object_ref (self),
                  (GFreeFunc) g_object_unref);
            }
        }
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
        /* populate the data */
        on_nwam_env_notify_cb(idx->data, NULL, (gpointer)self);
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
          MENUITEM_SECTION_VPN_END,
          name, /* name */
          name, /* action */
          GTK_UI_MANAGER_MENUITEM,
          TRUE);
        /* populate the data */
        on_nwam_enm_notify_cb(idx->data, NULL, (gpointer)self);
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
	nwam_menu_recreate_wifi_menuitems (self);
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
