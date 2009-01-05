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

#include <libnwam.h>
#include <glib-object.h>
#include <glib/gi18n.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

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

