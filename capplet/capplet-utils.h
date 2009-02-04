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

GList* capplet_model_to_list(GtkTreeModel *model);

void capplet_list_store_add(GtkTreeModel *model, NwamuiObject *object);


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
