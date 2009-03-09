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

#include "nwam-menu-action-group.h"

#define UID_DATA "uid"
#define DEBUG()	g_debug ("[[ %20s : %-4d ]]", __func__, __LINE__)

enum {
	PROP_ZERO = 0,
	PROP_UI,
    PROP_PLACEHOLDER,
};

typedef struct _NwamMenuActionGroupPrivate NwamMenuActionGroupPrivate;
struct _NwamMenuActionGroupPrivate {
	GtkUIManager *ui_manager;
	gchar *ui_placeholder;
	GtkActionGroup *action_group;
};

#define NWAM_MENU_ACTION_GROUP_GET_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), NWAM_TYPE_MENU_ACTION_GROUP, NwamMenuActionGroupPrivate))

static void nwam_menu_action_group_set_property (GObject *object,
  guint prop_id,
  const GValue *value,
  GParamSpec *pspec);
static void nwam_menu_action_group_get_property (GObject *object,
  guint prop_id,
  GValue *value,
  GParamSpec *pspec);

static void nwam_menu_action_group_finalize(NwamMenuActionGroup *self);

static void nwam_menu_action_group_attach(NwamMenuGroup *menugroup);
static void nwam_menu_action_group_detach(NwamMenuGroup *menugroup);
static void nwam_menu_action_group_add_item(NwamMenuGroup *menugroup,
  GObject *object,
  gboolean top);
static void nwam_menu_action_group_remove_item(NwamMenuGroup *menugroup,
  GObject *object);
static GObject* nwam_menu_action_group_get_item_by_name(NwamMenuGroup *menugroup, const gchar* action_name);
static GList* nwam_menu_action_group_get_items(NwamMenuGroup *menugroup);
static GtkMenu* nwam_menu_action_group_get_menu(NwamMenuGroup *menugroup);

G_DEFINE_TYPE(NwamMenuActionGroup,
  nwam_menu_action_group,
  NWAM_TYPE_MENU_GROUP)

static void
nwam_menu_action_group_class_init(NwamMenuActionGroupClass *klass)
{
	GObjectClass *gobject_class;
	NwamMenuGroupClass *menu_group_class;

	gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->finalize = (void (*)(GObject*)) nwam_menu_action_group_finalize;

    menu_group_class = NWAM_MENU_GROUP_CLASS(klass);
    menu_group_class->attach = nwam_menu_action_group_attach;
    menu_group_class->detach = nwam_menu_action_group_detach;
    menu_group_class->add_item = nwam_menu_action_group_add_item;
    menu_group_class->remove_item = nwam_menu_action_group_remove_item;
    menu_group_class->get_item_by_name = nwam_menu_action_group_get_item_by_name;
    menu_group_class->get_items = nwam_menu_action_group_get_items;
    menu_group_class->get_menu = nwam_menu_action_group_get_menu;

	gobject_class->set_property = nwam_menu_action_group_set_property;
	gobject_class->get_property = nwam_menu_action_group_get_property;

	g_object_class_install_property (gobject_class,
      PROP_UI,
      g_param_spec_object ("ui-manager",
        N_("Gtk UI Manager"),
        N_("Gtk UI Manager"),
        GTK_TYPE_UI_MANAGER,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (gobject_class,
      PROP_PLACEHOLDER,
      g_param_spec_string ("ui-placeholder",
        N_("placeholder where inserting"),
        N_("placeholder where inserting"),
        NULL,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    g_type_class_add_private(klass, sizeof(NwamMenuActionGroupPrivate));
}

static void
nwam_menu_action_group_set_property (GObject *object,
  guint prop_id,
  const GValue *value,
  GParamSpec *pspec)
{
    NwamMenuActionGroupPrivate *prv = NWAM_MENU_ACTION_GROUP_GET_PRIVATE(NWAM_MENU_GROUP(object));

	switch (prop_id) {
	case PROP_UI:
        if (prv->ui_manager) {
            g_object_unref(prv->ui_manager);
            g_object_unref(prv->action_group);
        }
        prv->ui_manager = GTK_UI_MANAGER(g_value_dup_object(value));
        prv->action_group = gtk_action_group_new(__func__);

        nwam_menu_group_attach(NWAM_MENU_GROUP(object));
        break;
	case PROP_PLACEHOLDER:
        g_free(prv->ui_placeholder);
        prv->ui_placeholder = g_value_dup_string(value);
        break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
nwam_menu_action_group_get_property (GObject *object,
  guint prop_id,
  GValue *value,
  GParamSpec *pspec)
{
    NwamMenuActionGroupPrivate *prv = NWAM_MENU_ACTION_GROUP_GET_PRIVATE(NWAM_MENU_GROUP(object));

	switch (prop_id)
	{
	case PROP_UI:
		g_value_set_object(value, (GObject*)prv->ui_manager);
        break;
	case PROP_PLACEHOLDER:
        g_value_set_string(value, prv->ui_placeholder);
        break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
nwam_menu_action_group_init(NwamMenuActionGroup *self)
{
    NwamMenuActionGroupPrivate *prv = NWAM_MENU_ACTION_GROUP_GET_PRIVATE(self);
}

static void
nwam_menu_action_group_finalize(NwamMenuActionGroup *self)
{
    NwamMenuActionGroupPrivate *prv = NWAM_MENU_ACTION_GROUP_GET_PRIVATE(self);
	g_object_unref(G_OBJECT(prv->ui_manager));
    g_object_unref(prv->action_group);
	g_free(prv->ui_placeholder);

	G_OBJECT_CLASS(nwam_menu_action_group_parent_class)->finalize(G_OBJECT(self));
}

static GObject*
nwam_menu_action_group_get_item_by_name(NwamMenuGroup *menugroup, const gchar* action_name)
{
    NwamMenuActionGroupPrivate *prv = NWAM_MENU_ACTION_GROUP_GET_PRIVATE(menugroup);
    GtkAction *action = NULL;

    action = gtk_action_group_get_action(prv->action_group, action_name);

    return (GObject *)action;
}

static void
nwam_menu_action_group_add_item(NwamMenuGroup *menugroup,
  GObject *object,
  gboolean top)
{
    NwamMenuActionGroupPrivate *prv = NWAM_MENU_ACTION_GROUP_GET_PRIVATE(menugroup);
    guint uid;
    GtkAction *action;

    g_assert(GTK_IS_ACTION(object));

    action = GTK_ACTION(object);

/*     GtkUIManager *ui_manager; */
/*     gchar *ui_placeholder; */
/*     g_object_get(menugroup, */
/*       "ui-manager", &ui_manager, */
/*       "ui-placeholder", &ui_placeholder, */
/*       NULL); */

    gtk_action_group_add_action(prv->action_group, action);
/*     gtk_action_group_add_action_with_accel(prv->action_group, action, NULL); */

	uid = gtk_ui_manager_new_merge_id(prv->ui_manager);

    g_object_set_data(G_OBJECT(action), UID_DATA, uid);

    g_debug("Add action '%s' to '%s'", gtk_action_get_name(action), prv->ui_placeholder);

    gtk_ui_manager_add_ui(prv->ui_manager,
      uid,
      prv->ui_placeholder,
      gtk_action_get_name(action), /* name */
      gtk_action_get_name(action), /* action */
      GTK_UI_MANAGER_MENUITEM,
      top);

#if 1
	{
		gchar *ui = gtk_ui_manager_get_ui(prv->ui_manager);
		g_debug("\n%s\n", ui);
		g_free (ui);
	}
#endif

}

static void
nwam_menu_action_group_remove_item(NwamMenuGroup *menugroup,
  GObject *object)
{
    NwamMenuActionGroupPrivate *prv = NWAM_MENU_ACTION_GROUP_GET_PRIVATE(menugroup);
    guint uid;
    GtkAction *action;

    g_assert(GTK_IS_ACTION(object));

    action = GTK_ACTION(object);

/*     GtkUIManager *ui_manager; */
/*     g_object_get(menugroup, */
/*       "ui-manager", &ui_manager, */
/*       NULL); */

    g_debug("Remove action '%s'", gtk_action_get_name(action));

    uid = g_object_get_data(G_OBJECT(action), UID_DATA);

    gtk_ui_manager_remove_ui(prv->ui_manager, uid);

    gtk_action_group_remove_action(prv->action_group, action);
}

static void
nwam_menu_action_group_attach(NwamMenuGroup *menugroup)
{
    NwamMenuActionGroupPrivate *prv = NWAM_MENU_ACTION_GROUP_GET_PRIVATE(menugroup);
/*     GtkUIManager *ui_manager; */
/*     g_object_get(menugroup, "ui-manager", &ui_manager, NULL); */
	gtk_ui_manager_insert_action_group (prv->ui_manager, prv->action_group, 0);
}

static void
nwam_menu_action_group_detach(NwamMenuGroup *menugroup)
{
    NwamMenuActionGroupPrivate *prv = NWAM_MENU_ACTION_GROUP_GET_PRIVATE(menugroup);
/*     GtkUIManager *ui_manager; */
/*     g_object_get(menugroup, "ui-manager", &ui_manager, NULL); */
    gtk_ui_manager_remove_action_group (prv->ui_manager, prv->action_group);
}

static GList*
nwam_menu_action_group_get_items(NwamMenuGroup *menugroup)
{
    NwamMenuActionGroupPrivate *prv = NWAM_MENU_ACTION_GROUP_GET_PRIVATE(menugroup);
    return gtk_action_group_list_actions(prv->action_group);
}

static GtkMenu*
nwam_menu_action_group_get_menu(NwamMenuGroup *menugroup)
{
    NwamMenuActionGroupPrivate *prv = NWAM_MENU_ACTION_GROUP_GET_PRIVATE(menugroup);
    GtkWidget *menu;

    menu = gtk_ui_manager_get_widget(prv->ui_manager, prv->ui_placeholder);

    if (!GTK_IS_MENU(menu)) {
        gchar *parent_name;

        parent_name = prv->ui_placeholder + strlen(prv->ui_placeholder) - 1;

        if (*parent_name == '/')
            parent_name--;

        for (; parent_name != prv->ui_placeholder && *parent_name != '/';
             parent_name--);
        
        if (parent_name != prv->ui_placeholder) {
            gint len = (gint)(prv->ui_placeholder - parent_name);

            parent_name = g_new(gchar, len);
            g_strlcpy(parent_name, prv->ui_placeholder, len);
            *(parent_name + len) = '\0';

            menu = gtk_ui_manager_get_widget(prv->ui_manager, prv->ui_placeholder);
            g_free(parent_name);

        }
    }

    return menu;
}

NwamMenuGroup*
nwam_menu_action_group_new(GtkUIManager *ui_manager,
  const gchar *place_holder)
{
    NwamMenuGroup *self = NWAM_MENU_GROUP(g_object_new(NWAM_TYPE_MENU_ACTION_GROUP,
        "ui-manager", ui_manager,
        "ui-placeholder", place_holder,
        NULL));
    NwamMenuActionGroupPrivate *prv = NWAM_MENU_ACTION_GROUP_GET_PRIVATE(self);

    g_assert(prv->ui_manager);
    if (prv->ui_placeholder == NULL ||
      *prv->ui_placeholder == '\0' ||
      *prv->ui_placeholder == '/') {
        gtk_ui_manager_add_ui(prv->ui_manager,
          gtk_ui_manager_new_merge_id(prv->ui_manager),
          "/",
          "nwam_menu_action_group",
          NULL,
          GTK_UI_MANAGER_POPUP,
          FALSE);

        if (prv->ui_placeholder)
            g_free(prv->ui_placeholder);
        prv->ui_placeholder = g_strdup("/nwam_menu_action_group");
    }

    return self;
}

