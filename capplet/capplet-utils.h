#ifndef _CAPPLET_UTILS_H
#define _CAPPLET_UTILS_H

#include <gtk/gtk.h>
#include "nwam_pref_iface.h"
#include "libnwamui.h"

gchar* nwamui_obj_get_display_name(GObject *obj);
void daemon_compose_ncu_panel_mixed_combo(GtkComboBox *combo, NwamuiDaemon *daemon);
void daemon_update_ncu_panel_mixed_model(GtkComboBox *combo, NwamuiDaemon *daemon);
void capplet_compose_nwamui_obj_combo(GtkComboBox *combo, NwamPrefIFace *iface);
void capplet_update_nwamui_obj_combo(GtkComboBox *combo, NwamuiDaemon *daemon, GType type);
void capplet_update_nwamui_obj_treeview(GtkTreeView *treeview, NwamuiDaemon *daemon, GType type);
void capplet_compose_combo(GtkComboBox *combo,
    GType type,
    GtkCellLayoutDataFunc layout_func,
    GtkTreeViewRowSeparatorFunc separator_func,
    GCallback changed_func,
    gpointer user_data,
    GDestroyNotify destroy);


#endif /* _CAPPLET_UTILS_H */
