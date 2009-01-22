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

#include "nwam-menu-group.h"

#define DEBUG()	g_debug ("[[ %20s : %-4d ]]", __func__, __LINE__)

typedef struct _NwamMenuGroupPrivate NwamMenuGroupPrivate;
struct _NwamMenuGroupPrivate {
	const int placeholder;
};

#define NWAM_MENU_GROUP_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), NWAM_TYPE_MENU_GROUP, NwamMenuGroupPrivate))

static void nwam_menu_group_set_property (GObject *object,
  guint prop_id,
  const GValue *value,
  GParamSpec *pspec);
static void nwam_menu_group_get_property (GObject *object,
  guint prop_id,
  GValue *value,
  GParamSpec *pspec);

static GObject* nwam_menu_group_constructor(GType type,
  guint n_construct_properties,
  GObjectConstructParam *construct_properties);
static void nwam_menu_group_finalize (NwamMenuGroup *self);

/* NwamMenuGroup is an virtual class */
G_DEFINE_ABSTRACT_TYPE(NwamMenuGroup, nwam_menu_group, G_TYPE_OBJECT)

static void
nwam_menu_group_class_init (NwamMenuGroupClass *klass)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->constructor = nwam_menu_group_constructor;
	gobject_class->finalize = (void (*)(GObject*)) nwam_menu_group_finalize;
	gobject_class->set_property = nwam_menu_group_set_property;
	gobject_class->get_property = nwam_menu_group_get_property;

    g_type_class_add_private(klass, sizeof(NwamMenuGroupPrivate));
}

static GObject*
nwam_menu_group_constructor(GType type,
  guint n_construct_properties,
  GObjectConstructParam *construct_properties)
{
    NwamMenuGroup *self;
    GObject *object;
	GError *error = NULL;

    object = G_OBJECT_CLASS(nwam_menu_group_parent_class)->constructor(type,
      n_construct_properties,
      construct_properties);
    self = NWAM_MENU_GROUP(object);

    return object;
}

static void
nwam_menu_group_finalize (NwamMenuGroup *self)
{
    NwamMenuGroupPrivate *prv = NWAM_MENU_GROUP_PRIVATE(self);
	
	G_OBJECT_CLASS(nwam_menu_group_parent_class)->finalize(G_OBJECT(self));
}

static void
nwam_menu_group_set_property (GObject *object,
  guint prop_id,
  const GValue *value,
  GParamSpec *pspec)
{
	NwamMenuGroup *self = NWAM_MENU_GROUP(object);
    NwamMenuGroupPrivate *prv = NWAM_MENU_GROUP_PRIVATE(self);

	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
nwam_menu_group_get_property (GObject *object,
  guint prop_id,
  GValue *value,
  GParamSpec *pspec)
{
	NwamMenuGroup *self = NWAM_MENU_GROUP(object);
    NwamMenuGroupPrivate *prv = NWAM_MENU_GROUP_PRIVATE(self);

	switch (prop_id)
	{
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
nwam_menu_group_init(NwamMenuGroup *self)
{
}

/**
 * nwam_menu_group_attach:
 * @self: a #NwamMenuGroup instance.
 *
 * Attach the group of menuitems to the menu (show them).
 */
void
nwam_menu_group_attach(NwamMenuGroup *self)
{
    NWAM_MENU_GROUP_GET_CLASS(self)->attach(self);
}

/**
 * nwam_menu_group_detach:
 * @self: a #NwamMenuGroup instance.
 *
 * Detach the group of menuitems from the menu (do not show them).
 */
void
nwam_menu_group_detach(NwamMenuGroup *self)
{
    NWAM_MENU_GROUP_GET_CLASS(self)->detach(self);
}

/**
 * nwam_menu_group_add_item:
 * @self: a #NwamMenuGroup instance.
 * @item: a menu item.
 * @top: the position to add
 *
 * Add an menu item to the menu item group.
 */
void
nwam_menu_group_add_item(NwamMenuGroup *self, GObject *item, gboolean top)
{
    NWAM_MENU_GROUP_GET_CLASS(self)->add_item(self, item, top);
}

/**
 * nwam_menu_group_remove_item:
 * @self: a #NwamMenuGroup instance.
 * @item: a menu item.
 *
 * Remove an menu item from the menu item group.
 */
void
nwam_menu_group_remove_item(NwamMenuGroup *self, GObject *item)
{
    NWAM_MENU_GROUP_GET_CLASS(self)->remove_item(self, item);
}

/**
 * nwam_menu_group_get_item_by_name:
 * @self: a #NwamMenuGroup instance.
 * @name: the name of the menu item what we are looking for.
 * @return: the menu item if founded, else NULL.
 *
 * Find an menu item in the menu item group by the name of item.
 */
GObject*
nwam_menu_group_get_item_by_name(NwamMenuGroup *self, const gchar *name)
{
    return NWAM_MENU_GROUP_GET_CLASS(self)->get_item_by_name(self, name);
}

/**
 * nwam_menu_group_get_item_by_proxy:
 * @self: a #NwamMenuGroup instance.
 * @proxy: the proxy what the menu item has.
 * @return: the menu item if founded, else NULL.
 *
 * Find an menu item in the menu item group by the proxy what item has.
 */
GObject*
nwam_menu_group_get_item_by_proxy(NwamMenuGroup *self, GObject *proxy)
{
    return NWAM_MENU_GROUP_GET_CLASS(self)->get_item_by_proxy(self, proxy);
}
