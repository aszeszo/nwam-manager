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
 * File:   nwamui_object.c
 *
 */

#include <glib-object.h>
#include <glib/gi18n.h>

#include "libnwamui.h"

enum {
	PLACEHOLDER,
	LAST_SIGNAL
};

static guint nwamui_object_signals[LAST_SIGNAL] = {0};

typedef struct _NwamuiObjectPrivate	  NwamuiObjectPrivate;

struct _NwamuiObjectPrivate {
	gint placeholder;
};

static GObject* nwamui_object_constructor(GType type,
  guint n_construct_properties,
  GObjectConstructParam *construct_properties);

static void nwamui_object_set_property(GObject         *object,
    guint            prop_id,
    const GValue    *value,
    GParamSpec      *pspec);

static void nwamui_object_get_property(GObject         *object,
    guint            prop_id,
    GValue          *value,
    GParamSpec      *pspec);

static void nwamui_object_finalize(NwamuiObject *self);


/* Callbacks */
static void object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);

#define NWAMUI_OBJECT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE((o), NWAMUI_TYPE_OBJECT, NwamuiObjectPrivate))
G_DEFINE_TYPE(NwamuiObject, nwamui_object, G_TYPE_OBJECT)

static void
nwamui_object_class_init(NwamuiObjectClass *klass)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->constructor = nwamui_object_constructor;
	gobject_class->finalize = (void (*)(GObject*)) nwamui_object_finalize;
	gobject_class->set_property = nwamui_object_set_property;
	gobject_class->get_property = nwamui_object_get_property;

    klass->get_name = NULL;
    klass->set_name = NULL;
    klass->get_conditions = NULL;
    klass->set_conditions = NULL;
    klass->get_activation_mode = NULL;
    klass->set_activation_mode = NULL;

	g_type_class_add_private(klass, sizeof(NwamuiObjectPrivate));
}

static void
nwamui_object_init(NwamuiObject *self)
{
}

static void
nwamui_object_set_property(GObject         *object,
    guint            prop_id,
    const GValue    *value,
    GParamSpec      *pspec)
{
	NwamuiObject *self = NWAMUI_OBJECT(object);

	switch (prop_id) {
        default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nwamui_object_get_property(GObject         *object,
    guint            prop_id,
    GValue          *value,
    GParamSpec      *pspec)
{
	NwamuiObject *self = NWAMUI_OBJECT(object);

	switch (prop_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static GObject*
nwamui_object_constructor(GType type,
  guint n_construct_properties,
  GObjectConstructParam *construct_properties)
{
	NwamuiObject *self;
	GObject *object;
	GError *error = NULL;

	object = G_OBJECT_CLASS(nwamui_object_parent_class)->constructor(type,
	    n_construct_properties,
	    construct_properties);
	self = NWAMUI_OBJECT(object);

	return object;
}

static void
nwamui_object_finalize(NwamuiObject *self)
{
	G_OBJECT_CLASS(nwamui_object_parent_class)->finalize(G_OBJECT (self));
}

/** 
 * nwamui_object_set_name:
 * @nwamui_object: a #NwamuiObject.
 * @name: Value to set name to.
 * 
 **/ 
extern void
nwamui_object_set_name (   NwamuiObject *object,
                             const gchar* name )
{
    g_return_if_fail (NWAMUI_IS_OBJECT (object));
    g_return_if_fail (NWAMUI_OBJECT_GET_CLASS (object)->set_name);
    g_assert (name != NULL );

    NWAMUI_OBJECT_GET_CLASS (object)->set_name(object, name);
}

/**
 * nwamui_object_get_name:
 * @nwamui_object: a #NwamuiObject.
 * @returns: the name.
 *
 **/
extern gchar *
nwamui_object_get_name (NwamuiObject *object)
{
    g_return_val_if_fail (NWAMUI_IS_OBJECT (object), NULL);
    g_return_val_if_fail (NWAMUI_OBJECT_GET_CLASS (object)->get_name, NULL);

    return NWAMUI_OBJECT_GET_CLASS (object)->get_name(object);
}

/** 
 * nwamui_object_set_conditions:
 * @nwamui_object: a #NwamuiObject.
 * @conditions: Value to set conditions to.
 * 
 **/ 
extern void
nwamui_object_set_conditions (   NwamuiObject *object,
                             const GList* conditions )
{
    g_return_if_fail (NWAMUI_IS_OBJECT (object));
    g_return_if_fail (NWAMUI_OBJECT_GET_CLASS (object)->set_conditions);
    g_assert (conditions != NULL );

    NWAMUI_OBJECT_GET_CLASS (object)->set_conditions(object, conditions);
}

/**
 * nwamui_object_get_conditions:
 * @nwamui_object: a #NwamuiObject.
 * @returns: the conditions.
 *
 **/
extern GList*
nwamui_object_get_conditions (NwamuiObject *object)
{
    g_return_val_if_fail (NWAMUI_IS_OBJECT (object), NULL);
    g_return_val_if_fail (NWAMUI_OBJECT_GET_CLASS (object)->get_conditions, NULL);

    return NWAMUI_OBJECT_GET_CLASS (object)->get_conditions(object);
}

extern gint
nwamui_object_get_activation_mode(NwamuiObject *object)
{
    g_return_val_if_fail (NWAMUI_IS_OBJECT (object), NWAMUI_COND_ACTIVATION_MODE_LAST);
    g_return_val_if_fail (NWAMUI_OBJECT_GET_CLASS (object)->get_activation_mode, NWAMUI_COND_ACTIVATION_MODE_LAST);

    return NWAMUI_OBJECT_GET_CLASS (object)->get_activation_mode(object);
}

extern void
nwamui_object_set_activation_mode(NwamuiObject *object, gint activation_mode)
{
    g_return_if_fail (NWAMUI_IS_OBJECT (object));
    g_return_if_fail (NWAMUI_OBJECT_GET_CLASS (object)->set_activation_mode);
    g_assert (activation_mode != NULL );

    NWAMUI_OBJECT_GET_CLASS (object)->set_activation_mode(object, activation_mode);
}

extern gint
nwamui_object_get_active(NwamuiObject *object)
{
    g_return_val_if_fail (NWAMUI_IS_OBJECT (object), FALSE);
    g_return_val_if_fail (NWAMUI_OBJECT_GET_CLASS (object)->get_active, FALSE);

    return NWAMUI_OBJECT_GET_CLASS (object)->get_active(object);
}

extern void
nwamui_object_set_active(NwamuiObject *object, gboolean active)
{
    g_return_if_fail (NWAMUI_IS_OBJECT (object));
    g_return_if_fail (NWAMUI_OBJECT_GET_CLASS (object)->set_active);

    NWAMUI_OBJECT_GET_CLASS (object)->set_active(object, active);
}

extern gboolean
nwamui_object_commit(NwamuiObject *object)
{
    g_return_val_if_fail (NWAMUI_IS_OBJECT (object), FALSE);
    g_return_val_if_fail (NWAMUI_OBJECT_GET_CLASS (object)->commit, FALSE);

    return NWAMUI_OBJECT_GET_CLASS (object)->commit(object);
}

extern void
nwamui_object_reload(NwamuiObject *object)
{
    g_return_if_fail (NWAMUI_IS_OBJECT (object));
    g_return_if_fail (NWAMUI_OBJECT_GET_CLASS (object)->reload);

    NWAMUI_OBJECT_GET_CLASS (object)->reload(object);
}
