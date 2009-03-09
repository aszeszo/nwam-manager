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

void capplet_compose_nwamui_obj_combo(GtkComboBox *combo, NwamPrefIFace *iface);

void capplet_update_model_from_daemon(GtkTreeModel *model, NwamuiDaemon *daemon, GType type);

void capplet_compose_nwamui_obj_treeview(GtkTreeView *treeview);

void capplet_compose_combo(GtkComboBox *combo,
    GType type,
    GtkCellLayoutDataFunc layout_func,
    GtkTreeViewRowSeparatorFunc separator_func,
    GCallback changed_func,
    gpointer user_data,
    GDestroyNotify destroy);

void capplet_remove_gtk_dialog_escape_binding(GtkDialogClass *dialog_class);
gint capplet_dialog_run(NwamPrefIFace *iface, GtkWidget *w);

gboolean capplet_model_get_iter(GtkTreeModel *model, NwamuiObject *object, GtkTreeIter *iter);
NwamuiObject *capplet_combo_get_active(GtkComboBox *combo);
void capplet_combo_set_active(GtkComboBox *combo, NwamuiObject *object);

gboolean capplet_tree_view_commit_object(NwamTreeView *self, NwamuiObject *object, gpointer user_data);

void capplet_tree_store_merge_children(GtkTreeStore *model,
    GtkTreeIter *target,
    GtkTreeIter *source,
    GtkTreeModelForeachFunc func,
    gpointer user_data);
void capplet_tree_store_exclude_children(GtkTreeStore *model,
    GtkTreeIter *source,
    GtkTreeModelForeachFunc func,
    gpointer user_data);

gboolean capplet_tree_view_expand_row(GtkTreeView *treeview,
    GtkTreeIter *iter,
    gboolean open_all);
gboolean capplet_tree_view_collapse_row(GtkTreeView *treeview,
    GtkTreeIter *iter);

void capplet_list_foreach_merge_to_model(gpointer data, gpointer user_data);

GList* capplet_model_to_list(GtkTreeModel *model);

void capplet_list_store_add(GtkTreeModel *model, NwamuiObject *object);

void capplet_reset_increasable_name(GObject *object);

gchar* capplet_get_increasable_name(GtkTreeModel *model, const gchar *prefix, GObject *object);

gint capplet_get_max_name_num(GtkTreeModel *model, const gchar *prefix);

gchar* capplet_get_original_name(const gchar *prefix, const gchar *name);


/* Column and renderer */
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

void nwamui_object_active_mode_text_cell (GtkTreeViewColumn *col,
    GtkCellRenderer   *renderer,
    GtkTreeModel      *model,
    GtkTreeIter       *iter,
    gpointer           data);

#endif /* _CAPPLET_UTILS_H */
