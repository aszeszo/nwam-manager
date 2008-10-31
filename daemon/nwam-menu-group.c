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

void
nwam_menu_group_attach(NwamMenuGroup *self)
{
    NWAM_MENU_GROUP_GET_CLASS(self)->attach(self);
}

void
nwam_menu_group_detach(NwamMenuGroup *self)
{
    NWAM_MENU_GROUP_GET_CLASS(self)->detach(self);
}

void
nwam_menu_group_add_item(NwamMenuGroup *self, GObject *item, gboolean top)
{
    NWAM_MENU_GROUP_GET_CLASS(self)->add_item(self, item, top);
}

void
nwam_menu_group_remove_item(NwamMenuGroup *self, GObject *item)
{
    NWAM_MENU_GROUP_GET_CLASS(self)->remove_item(self, item);
}

GObject*
nwam_menu_group_get_item_by_name(NwamMenuGroup *self, const gchar *name)
{
    return NWAM_MENU_GROUP_GET_CLASS(self)->get_item_by_name(self, name);
}

GObject*
nwam_menu_group_get_item_by_proxy(NwamMenuGroup *self, GObject *proxy)
{
    return NWAM_MENU_GROUP_GET_CLASS(self)->get_item_by_proxy(self, proxy);
}

