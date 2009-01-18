#ifndef _CAPPLET_UTILS_H
#define _CAPPLET_UTILS_H

#include <gtk/gtk.h>
#include "nwam_pref_iface.h"
#include "libnwamui.h"

void daemon_compose_ncu_panel_mixed_combo(GtkComboBox *combo, NwamuiDaemon *daemon);
void daemon_update_ncu_panel_mixed_model(GtkComboBox *combo, NwamuiDaemon *daemon);
void capplet_compose_nwamui_obj_combo(GtkComboBox *combo, NwamPrefIFace *iface);
void capplet_update_nwamui_obj_combo(GtkComboBox *combo, NwamuiDaemon *daemon, GType type);
void capplet_compose_nwamui_obj_treeview(GtkTreeView *treeview);
void capplet_update_nwamui_obj_treeview(GtkTreeView *treeview, NwamuiDaemon *daemon, GType type);
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


/* Column and renderer */
void capplet_name_column_new(GtkTreeView *treeview,
    GtkTreeViewColumn **col, const gchar *title,
    GtkCellRenderer **cell);

void capplet_active_mode_column_new(GtkTreeView *treeview,
    GtkTreeViewColumn **col, const gchar *title,
    GtkCellRenderer **cell);


#endif /* _CAPPLET_UTILS_H */
