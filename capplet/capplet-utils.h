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

#ifndef _CAPPLET_UTILS_H
#define _CAPPLET_UTILS_H

#include <gtk/gtk.h>
#include "nwam_pref_iface.h"
#include "libnwamui.h"

#include "nwam_tree_view.h"

/* Misc. */
void capplet_remove_gtk_dialog_escape_binding(GtkDialogClass *dialog_class);
gint capplet_dialog_run(NwamPrefIFace *iface, GtkWidget *w);

#define CAPPLET_COMPOSE_NWAMUI_OBJECT_COMBO(combo, iface)   \
    {                                                       \
        capplet_compose_combo(GTK_COMBO_BOX(combo),         \
          GTK_TYPE_LIST_STORE,                              \
          G_TYPE_OBJECT,                                    \
          (GtkCellLayoutDataFunc)nwamui_object_name_cell,   \
          NULL,                                             \
          (GCallback)nwam_pref_iface_combo_changed_cb,      \
          (gpointer)NWAM_PREF_IFACE(iface),                 \
          NULL);                                            \
    }
#define CAPPLET_COMPOSE_NWAMUI_OBJECT_LIST_VIEW(treeview)               \
    {                                                                   \
        GtkTreeModel *model;                                            \
                                                                        \
        model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));       \
        if (model == NULL) {                                            \
            model = (GtkTreeModel *)gtk_list_store_new(1, NWAMUI_TYPE_OBJECT); \
            gtk_tree_view_set_model(treeview, model);                   \
            g_object_unref(model);                                      \
        }                                                               \
    }
#define CAPPLET_COMPOSE_NWAMUI_OBJECT_TREE_VIEW(treeview)               \
    {                                                                   \
        GtkTreeModel *model;                                            \
                                                                        \
        model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));       \
        if (model == NULL) {                                            \
            model = (GtkTreeModel *)gtk_tree_store_new(1, NWAMUI_TYPE_OBJECT); \
            gtk_tree_view_set_model(treeview, model);                   \
            g_object_unref(model);                                      \
        }                                                               \
    }
#define CAPPLET_LIST_STORE_ADD(model, object)                           \
    {                                                                   \
        GtkTreeIter iter;                                               \
        gtk_list_store_append(GTK_LIST_STORE(model), &iter);            \
        gtk_list_store_set(GTK_LIST_STORE(model), &iter, 0, object, -1); \
    }
#define CAPPLET_TREE_STORE_ADD(model, parent, object, iter)             \
    {                                                                   \
        gtk_tree_store_append(GTK_TREE_STORE(model), iter, parent);     \
        gtk_tree_store_set(GTK_TREE_STORE(model), iter, 0, object, -1); \
    }
#define TREE_STORE_INSET_BEFORE(model, parent, sibling, object, iter)   \
    {                                                                   \
        gtk_tree_store_insert_before(GTK_TREE_STORE(model), iter, parent, sibling); \
        gtk_tree_store_set(GTK_TREE_STORE(model), iter, 0, object, -1); \
    }
#define TREE_STORE_INSET_AFTER(model, parent, sibling, object, iter)    \
    {                                                                   \
        gtk_tree_store_insert_after(GTK_TREE_STORE(model), iter, parent, sibling); \
        gtk_tree_store_set(GTK_TREE_STORE(model), iter, 0, object, -1); \
    }
#define TREE_STORE_CP_OBJECT(model, target, source)                     \
    {                                                                   \
        NwamuiObject *object;                                           \
        gtk_tree_model_get(GTK_TREE_MODEL(model), source, 0, &object, -1); \
        gtk_tree_store_set(GTK_TREE_STORE(model), target, 0, object, -1); \
        if (object)                                                     \
            g_object_unref(object);                                     \
    }
#define TREE_STORE_MOVE_OBJECT(model, target, source)                   \
    {                                                                   \
        TREE_STORE_CP_OBJECT(model, target, source);                    \
        gtk_tree_store_remove(GTK_TREE_STORE(model), source);           \
    }
#define TREE_STORE_CP_OBJECT_AFTER(model, source, parent, sibling, iter) \
    {                                                                   \
        gtk_tree_store_insert_after(GTK_TREE_STORE(model), iter, parent, sibling); \
        TREE_STORE_CP_OBJECT(model, iter, source);                      \
    }

#define TREE_VIEW_EXPAND_ROW(treeview, iter)                            \
    {                                                                   \
        GtkTreePath *path;                                              \
        path = gtk_tree_model_get_path(gtk_tree_view_get_model(GTK_TREE_VIEW(treeview)), \
          &parent);                                                     \
        gtk_tree_view_expand_row(GTK_TREE_VIEW(treeview), path, TRUE);  \
        gtk_tree_path_free(path);                                       \
    }

/* Combo utils. */
void capplet_compose_combo(GtkComboBox *combo,
  GType tree_store_type,
  GType type,
  GtkCellLayoutDataFunc layout_func,
  GtkTreeViewRowSeparatorFunc separator_func,
  GCallback changed_func,
  gpointer user_data,
  GDestroyNotify destroy);

void nwam_pref_iface_combo_changed_cb(GtkComboBox* combo, gpointer user_data);

GObject *capplet_combo_get_active_object(GtkComboBox *combo);
gboolean capplet_combo_set_active_object(GtkComboBox *combo, GObject *object);

gboolean capplet_tree_view_commit_object(NwamTreeView *self, NwamuiObject *object, gpointer user_data);

/* Tree model */
void capplet_update_model_from_daemon(GtkTreeModel *model, NwamuiDaemon *daemon, GType type);

void capplet_tree_store_merge_children(GtkTreeStore *model,
    GtkTreeIter *target,
    GtkTreeIter *source,
    GtkTreeModelForeachFunc func,
    gpointer user_data);
void capplet_tree_store_exclude_children(GtkTreeStore *model,
    GtkTreeIter *source,
    GtkTreeModelForeachFunc func,
    gpointer user_data);

typedef gboolean (*CappletTreeModelForeachFunc)(GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data);

gboolean capplet_model_find_object(GtkTreeModel *model, GObject *object, GtkTreeIter *iter);

gboolean capplet_model_find_object_with_parent(GtkTreeModel *model, GtkTreeIter *parent, GObject *object, GtkTreeIter *iter);

gboolean capplet_model_1_level_foreach(GtkTreeModel *model, GtkTreeIter *parent, CappletTreeModelForeachFunc func, gpointer user_data, GtkTreeIter *iter);

void capplet_list_foreach_merge_to_list_store(gpointer data, gpointer store);
void capplet_list_foreach_merge_to_tree_store(gpointer data, gpointer store);

GList* capplet_model_to_list(GtkTreeModel *model);

/* Tree view, column and renderer */
gboolean capplet_tree_view_expand_row(GtkTreeView *treeview,
    GtkTreeIter *iter,
    gboolean open_all);

gboolean capplet_tree_view_collapse_row(GtkTreeView *treeview,
    GtkTreeIter *iter);

GtkTreeViewColumn *capplet_column_new(GtkTreeView *treeview, ...);

GtkCellRenderer *capplet_column_append_cell(GtkTreeViewColumn *col,
    GtkCellRenderer *cell, gboolean expand,
    GtkTreeCellDataFunc func, gpointer user_data, GDestroyNotify destroy);

void nwamui_object_name_cell (GtkCellLayout *cell_layout,
    GtkCellRenderer   *renderer,
    GtkTreeModel      *model,
    GtkTreeIter       *iter,
    gpointer           data);

void nwamui_object_name_cell_edited ( GtkCellRendererText *cell,
    const gchar         *path_string,
    const gchar         *new_text,
    gpointer             data);

void nwamui_object_active_mode_pixbuf_cell (GtkTreeViewColumn *col,
    GtkCellRenderer   *renderer,
    GtkTreeModel      *model,
    GtkTreeIter       *iter,
    gpointer           data);

void capplet_tree_store_move_object(GtkTreeModel *model,
    GtkTreeIter *target,
    GtkTreeIter *source);

/* Increasable name. */
void capplet_reset_increasable_name(GObject *object);

gchar* capplet_get_increasable_name(GtkTreeModel *model, const gchar *prefix, GObject *object);

gint capplet_get_max_name_num(GtkTreeModel *model, const gchar *prefix);

gchar* capplet_get_original_name(const gchar *prefix, const gchar *name);

#endif /* _CAPPLET_UTILS_H */
