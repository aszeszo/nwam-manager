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

#include "libnwamui.h"
#include "nwam_tree_view.h"

#define DEBUG()	g_debug ("[[ %20s : %-4d ]]", __func__, __LINE__)

enum {
    PROP_BUTTON_BEGIN = 0,
    PROP_BUTTON_ADD,
    PROP_BUTTON_REMOVE,
    PROP_BUTTON_EDIT,
    PROP_BUTTON_UP,
    PROP_BUTTON_DOWN,
    PROP_BUTTON_END
};

enum {
    NEW_OBJECT,
    LAST_SIGNAL
};

static guint nwam_tree_view_signals[LAST_SIGNAL] = {0};

typedef struct _NwamTreeViewPrivate NwamTreeViewPrivate;
struct _NwamTreeViewPrivate {
    NwamuiObject *new_obj;
    GtkWidget *widget_list[PROP_BUTTON_END];
};

#define NWAM_TREE_VIEW_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), NWAM_TYPE_TREE_VIEW, NwamTreeViewPrivate))

static void nwam_tree_view_set_property (GObject *object,
  guint prop_id,
  const GValue *value,
  GParamSpec *pspec);
static void nwam_tree_view_get_property (GObject *object,
  guint prop_id,
  GValue *value,
  GParamSpec *pspec);

static GObject* nwam_tree_view_constructor(GType type,
  guint n_construct_properties,
  GObjectConstructParam *construct_properties);
static void nwam_tree_view_finalize (NwamTreeView *self);

/* Call backs */
static void on_button_clicked(GtkButton *button, gpointer user_data);
static NwamuiObject* default_signal_handler_new_object(NwamTreeView *self, gpointer user_data);

G_DEFINE_ABSTRACT_TYPE(NwamTreeView, nwam_tree_view, GTK_TYPE_TREE_VIEW)

static void
nwam_tree_view_class_init (NwamTreeViewClass *klass)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->constructor = nwam_tree_view_constructor;
	gobject_class->finalize = (void (*)(GObject*)) nwam_tree_view_finalize;
	gobject_class->set_property = nwam_tree_view_set_property;
	gobject_class->get_property = nwam_tree_view_get_property;

    {
        int propid;
        for (propid = PROP_BUTTON_BEGIN + 1; propid < PROP_BUTTON_END; propid ++) {
            g_object_class_install_property (gobject_class,
              propid,
              g_param_spec_object("buttons of tree view",
                _("buttons of tree view"),
                _("buttons of tree view"),
                GTK_TYPE_BUTTON,
                G_PARAM_READABLE));
        }
    }
    
    nwam_tree_view_signals[NEW_OBJECT] =   
      g_signal_new ("new_object",
        G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
        G_STRUCT_OFFSET (NwamTreeViewClass, new_object),
        NULL, NULL,
        g_cclosure_marshal_VOID__OBJECT, 
        NWAMUI_TYPE_OBJECT,                  /* Return Type */
        0,                            /* Number of Args */
        G_TYPE_NONE);               /* Types of Args */

    klass->new_object = default_signal_handler_new_object;

    g_type_class_add_private(klass, sizeof(NwamTreeViewPrivate));
}

static GObject*
nwam_tree_view_constructor(GType type,
  guint n_construct_properties,
  GObjectConstructParam *construct_properties)
{
    NwamTreeView *self;
    GObject *object;
	GError *error = NULL;

    object = G_OBJECT_CLASS(nwam_tree_view_parent_class)->constructor(type,
      n_construct_properties,
      construct_properties);
    self = NWAM_TREE_VIEW(object);

    return object;
}

static void
nwam_tree_view_finalize(NwamTreeView *self)
{
    NwamTreeViewPrivate *prv = NWAM_TREE_VIEW_PRIVATE(self);
    int propid;

    for (propid = PROP_BUTTON_BEGIN + 1; propid < PROP_BUTTON_END; propid ++) {
        if (prv->widget_list[propid]) {
            g_object_unref(prv->widget_list[propid]);
        }
    }

	G_OBJECT_CLASS(nwam_tree_view_parent_class)->finalize(G_OBJECT(self));
}

static void
nwam_tree_view_set_property (GObject *object,
  guint prop_id,
  const GValue *value,
  GParamSpec *pspec)
{
	NwamTreeView *self = NWAM_TREE_VIEW(object);
    NwamTreeViewPrivate *prv = NWAM_TREE_VIEW_PRIVATE(self);

	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
nwam_tree_view_get_property (GObject *object,
  guint prop_id,
  GValue *value,
  GParamSpec *pspec)
{
	NwamTreeView *self = NWAM_TREE_VIEW(object);
    NwamTreeViewPrivate *prv = NWAM_TREE_VIEW_PRIVATE(self);

	switch (prop_id)
	{
    case PROP_BUTTON_ADD:
    case PROP_BUTTON_REMOVE:
    case PROP_BUTTON_EDIT:
    case PROP_BUTTON_UP:
    case PROP_BUTTON_DOWN:
        g_value_set_object(value, prv->widget_list[prop_id]);
        break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
nwam_tree_view_init(NwamTreeView *self)
{
    NwamTreeViewPrivate *prv = NWAM_TREE_VIEW_PRIVATE(self);
    int propid;

    for (propid = PROP_BUTTON_BEGIN + 1; propid < PROP_BUTTON_END; propid ++) {
        prv->widget_list[propid] = gtk_button_new();
        g_signal_connect(prv->widget_list[propid], "clicked",
          G_CALLBACK(on_button_clicked), (gpointer)self);
    }
}

void
nwam_tree_view_add(NwamTreeView *self)
{
    NwamTreeViewPrivate *prv = NWAM_TREE_VIEW_PRIVATE(self);

    if (prv->new_obj) {
        
    }
}

void
nwam_tree_view_remove(NwamTreeView *self)
{
}

void
nwam_tree_view_up(NwamTreeView *self, GObject *item, gboolean top)
{
}

void
nwam_tree_view_down(NwamTreeView *self, GObject *item)
{
}

GObject*
nwam_tree_view_edit(NwamTreeView *self, const gchar *name)
{
}

GObject*
nwam_tree_view_get_item_by_proxy(NwamTreeView *self, GObject *proxy)
{
}

static void
on_button_clicked(GtkButton *button, gpointer user_data)
{
    NwamTreeViewPrivate *prv = NWAM_TREE_VIEW_PRIVATE(user_data);
	NwamTreeView *self = NWAM_TREE_VIEW(user_data);
    int prop_id;

    for (prop_id = PROP_BUTTON_BEGIN + 1; prop_id < PROP_BUTTON_END; prop_id ++) {
        if (prv->widget_list[prop_id] == (gpointer)button) {
            break;
        }
    }
    
	switch (prop_id)
	{
    case PROP_BUTTON_ADD: {
        g_assert(prv->new_obj == NULL);

        g_signal_emit(self,
          nwam_tree_view_signals[NEW_OBJECT],
          0, /* details */
          &prv->new_obj);

        
    }
    case PROP_BUTTON_REMOVE:
    case PROP_BUTTON_EDIT:
    case PROP_BUTTON_UP:
    case PROP_BUTTON_DOWN:
        break;
	default:
        g_assert_not_reached();
        break;
    }
}

static NwamuiObject*
default_signal_handler_new_object(NwamTreeView *self, gpointer user_data)
{
    return NULL;
}
