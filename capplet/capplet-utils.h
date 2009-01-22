#ifndef _CAPPLET_UTILS_H
#define _CAPPLET_UTILS_H

#include <gtk/gtk.h>
#include "nwam_pref_iface.h"
#include "libnwamui.h"

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


/* Column and renderer */
void capplet_name_column_new(GtkTreeView *treeview,
    GtkTreeViewColumn **col, const gchar *title,
    GtkCellRenderer **cell);

void capplet_active_mode_column_new(GtkTreeView *treeview,
    GtkTreeViewColumn **col, const gchar *title,
    GtkCellRenderer **cell);


#endif /* _CAPPLET_UTILS_H */
