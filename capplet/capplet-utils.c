#include <gdk/gdkkeysyms.h>
#include "capplet-utils.h"

static gboolean ncu_panel_mixed_combo_separator_cb(GtkTreeModel *model,
    GtkTreeIter *iter,
    gpointer data);

static void ncu_panel_mixed_combo_cell_cb (GtkCellLayout *cell_layout,
    GtkCellRenderer   *renderer,
    GtkTreeModel      *model,
    GtkTreeIter       *iter,
    gpointer           data);

static void nwamui_obj_combo_cell_cb (GtkCellLayout *cell_layout,
    GtkCellRenderer   *renderer,
    GtkTreeModel      *model,
    GtkTreeIter       *iter,
    gpointer           data);

static void nwam_pref_iface_combo_changed_cb(GtkComboBox* combo, gpointer user_data);

void
daemon_compose_ncu_panel_mixed_combo(GtkComboBox *combo, NwamuiDaemon *daemon)
{
	capplet_compose_combo(combo,
	    G_TYPE_OBJECT,
	    ncu_panel_mixed_combo_cell_cb,
	    ncu_panel_mixed_combo_separator_cb,
	    NULL,
	    NULL,
	    NULL);
}

void
daemon_update_ncu_panel_mixed_model(GtkComboBox *combo, NwamuiDaemon *daemon)
{
	GtkTreeModel      *model;
	model = gtk_combo_box_get_model(GTK_COMBO_BOX(combo));        
}

static void
ncu_panel_mixed_combo_cell_cb (GtkCellLayout *cell_layout,
    GtkCellRenderer   *renderer,
    GtkTreeModel      *model,
    GtkTreeIter       *iter,
    gpointer           data)
{
}

static gboolean
ncu_panel_mixed_combo_separator_cb(GtkTreeModel *model,
    GtkTreeIter *iter,
    gpointer data)
{
	gpointer item = NULL;
	gtk_tree_model_get(model, iter, 0, &item, -1);
	return item == NULL;
}

void
capplet_compose_nwamui_obj_combo(GtkComboBox *combo, NwamPrefIFace *iface)
{
	capplet_compose_combo(combo,
	    G_TYPE_OBJECT,
	    nwamui_obj_combo_cell_cb,
	    NULL,
	    nwam_pref_iface_combo_changed_cb,
	    (gpointer)iface,
	    NULL);
}

void
capplet_update_nwamui_obj_combo(GtkComboBox *combo, NwamuiDaemon *daemon, GType type)
{
	GtkTreeModel      *model;
	GList           *obj_list;
	GList           *idx;
	GtkTreeIter   iter;

	model = gtk_combo_box_get_model(GTK_COMBO_BOX(combo));
	/* TODO clean all */
	gtk_list_store_clear(GTK_LIST_STORE(model));

	if (type == NWAMUI_TYPE_NCP) {
		obj_list = nwamui_daemon_get_ncp_list(daemon);
	/* } else if (type == NWAMUI_TYPE_NCU) { */
	} else if (type == NWAMUI_TYPE_ENV) {
		obj_list = nwamui_daemon_get_env_list(daemon);
	} else if (type == NWAMUI_TYPE_ENM) {
		obj_list = nwamui_daemon_get_enm_list(daemon);
	} else {
		g_error("unknow supported get nwamui obj list");
	}

	for (idx = obj_list; idx; idx = g_list_next(idx)) {
		gtk_list_store_append(GTK_LIST_STORE(model), &iter);
		gtk_list_store_set (GTK_LIST_STORE(model), &iter,
		    0, idx->data,
                    -1);
		g_object_unref(idx->data);
	}
	g_list_free(obj_list);
}

static void
nwamui_obj_combo_cell_cb (GtkCellLayout *cell_layout,
    GtkCellRenderer   *renderer,
    GtkTreeModel      *model,
    GtkTreeIter       *iter,
    gpointer           data)
{
	gpointer row_data = NULL;
	gchar *text = NULL;
	
	gtk_tree_model_get(model, iter, 0, &row_data, -1);
	
	if (row_data) {
		g_assert(G_IS_OBJECT (row_data));
	
		text = nwamui_obj_get_display_name(G_OBJECT(row_data));
		g_object_set (G_OBJECT(renderer), "text", text, NULL);
		g_free (text);
	}
}

static void
nwam_pref_iface_combo_changed_cb(GtkComboBox *combo, gpointer user_data)
{
	GtkTreeIter iter;

	if (gtk_combo_box_get_active_iter(combo, &iter)) {
		GObject *obj;
		gtk_tree_model_get(gtk_combo_box_get_model(combo), &iter, 0, &obj, -1);
		g_assert(G_IS_OBJECT(obj));
		nwam_pref_refresh(NWAM_PREF_IFACE(user_data), obj, TRUE);
		g_object_unref(obj);
	}
}

gchar*
nwamui_obj_get_display_name(GObject *obj)
{
	GType type = G_OBJECT_TYPE(obj);
	gchar *name;

	if (type == NWAMUI_TYPE_NCP) {
		name = nwamui_ncp_get_name(NWAMUI_NCP(obj));
	} else if (type == NWAMUI_TYPE_NCU) {
		name = nwamui_ncu_get_vanity_name(NWAMUI_NCP(obj));
	} else if (type == NWAMUI_TYPE_ENV) {
		name = nwamui_env_get_name(NWAMUI_ENV(obj));
	} else if (type == NWAMUI_TYPE_ENM) {
		name = nwamui_enm_get_name(NWAMUI_ENM(obj));
	} else {
		g_error("unknow get display name");
	}
	return name;
}

void
capplet_update_nwamui_obj_treeview(GtkTreeView *treeview, NwamuiDaemon *daemon, GType type)
{
	GtkTreeModel      *model;
	GList           *obj_list;
	GList           *idx;
	GtkTreeIter   iter;

	model = gtk_tree_view_get_model(treeview);
	/* TODO clean all */
	gtk_list_store_clear(GTK_LIST_STORE(model));

	if (type == NWAMUI_TYPE_NCP) {
		obj_list = nwamui_daemon_get_ncp_list(daemon);
	/* } else if (type == NWAMUI_TYPE_NCU) { */
	} else if (type == NWAMUI_TYPE_ENV) {
		obj_list = nwamui_daemon_get_env_list(daemon);
	} else if (type == NWAMUI_TYPE_ENM) {
		obj_list = nwamui_daemon_get_enm_list(daemon);
	} else {
		g_error("unknow supported get nwamui obj list");
	}

	for (idx = obj_list; idx; idx = g_list_next(idx)) {
		gtk_list_store_append(GTK_LIST_STORE(model), &iter);
		gtk_list_store_set (GTK_LIST_STORE(model), &iter,
		    0, idx->data,
                    -1);
		g_object_unref(idx->data);
	}
	g_list_free(obj_list);
}

void
capplet_compose_combo(GtkComboBox *combo,
    GType type,
    GtkCellLayoutDataFunc layout_func,
    GtkTreeViewRowSeparatorFunc separator_func,
    GCallback changed_func,
    gpointer user_data,
    GDestroyNotify destroy)
{
	GtkCellRenderer *renderer;
	GtkTreeModel      *model;
        
	if (type != G_TYPE_STRING) {
		g_return_if_fail(layout_func);

		model = GTK_TREE_MODEL(gtk_list_store_new(1, type));
		gtk_combo_box_set_model(GTK_COMBO_BOX(combo), model);
		g_object_unref(model);

		gtk_cell_layout_clear(GTK_CELL_LAYOUT(combo));
		renderer = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), renderer, TRUE);
		gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(combo),
		    renderer,
		    layout_func,
		    user_data,
		    destroy);
	}
	if (separator_func) {
		gtk_combo_box_set_row_separator_func (combo,
		    separator_func,
		    user_data,
		    destroy);
	}
	if (changed_func) {
		g_signal_connect(G_OBJECT(combo),
		    "changed",
		    changed_func,
		    user_data);
	}
}

void
capplet_remove_gtk_dialog_escape_binding(GtkDialogClass *dialog_class)
{
	GtkBindingSet *binding_set;

	binding_set = gtk_binding_set_by_class(dialog_class);
  
	gtk_binding_entry_remove(binding_set, GDK_Escape, 0);
}
