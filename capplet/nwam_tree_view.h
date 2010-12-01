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
#ifndef NWAM_TREE_VIEW_H
#define NWAM_TREE_VIEW_H

#include <gtk/gtk.h>
#include "nwam_object_ctrl_iface.h"

G_BEGIN_DECLS

#define NWAM_TYPE_TREE_VIEW      (nwam_tree_view_get_type ())
#define NWAM_TREE_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NWAM_TYPE_TREE_VIEW, NwamTreeView))
#define NWAM_TREE_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NWAM_TYPE_TREE_VIEW, NwamTreeViewClass))
#define NWAM_IS_TREE_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NWAM_TYPE_TREE_VIEW))
#define NWAM_IS_TREE_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NWAM_TYPE_TREE_VIEW))
#define NWAM_TREE_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), NWAM_TYPE_TREE_VIEW, NwamTreeViewClass))

typedef struct _NwamTreeView        NwamTreeView;
typedef struct _NwamTreeViewClass   NwamTreeViewClass;
typedef void (*NwamTreeViewAddObjectFunc)(NwamTreeView *self, NwamuiObject **object, gpointer user_data);
typedef void (*NwamTreeViewRemoveObjectFunc)(NwamTreeView *self, NwamuiObject *object, gpointer user_data);
typedef gboolean (*NwamTreeViewCommitObjectFunc)(NwamTreeView *self, NwamuiObject *object, gpointer user_data);

typedef enum _NwamTreeViewSupportWidgets {
    NTV_ADD_BTN = 0,
    NTV_REMOVE_BTN,
    NTV_RENAME_BTN,
    NTV_EDIT_BTN,
    NTV_DUP_BTN,
    NTV_UP_BTN,
    NTV_DOWN_BTN,
    NTV_N_BTNS,
} NwamTreeViewSupportWidgets;

struct _NwamTreeView {
	GtkTreeView parent;
};

struct _NwamTreeViewClass {
	GtkTreeViewClass parent_class;

    void (*activate_widget)(NwamTreeView *self, GtkWidget *widget, gpointer user_data);

/*     GObject *(*get_item_by_proxy)(NwamTreeView *self, GObject *proxy); */
/*     void (*foreach)(NwamTreeView *self, GtkCallback callback, gpointer user_data); */
};

GType nwam_tree_view_get_type(void) G_GNUC_CONST;

GtkWidget* nwam_tree_view_new();
void nwam_tree_view_register_widget(NwamTreeView *self, NwamTreeViewSupportWidgets id, GtkWidget *w);
void nwam_tree_view_unregister_widget(NwamTreeView *self, NwamTreeViewSupportWidgets id, GtkWidget *w);

GObject* nwam_tree_view_get_item_by_proxy(NwamTreeView *self, GObject *proxy);

NwamuiObject* nwam_tree_view_get_cached_object(NwamTreeView *self);
GtkTreePath* nwam_tree_view_get_cached_object_path(NwamTreeView *self);


G_END_DECLS

#endif  /* NWAM_TREE_VIEW_H */
