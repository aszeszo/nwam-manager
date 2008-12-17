#include "capplet-utils.h"
static gboolean ncu_panel_mixed_combo_separator_cb(GtkTreeModel *model,
    GtkTreeIter *iter,
    gpointer data);

static void ncu_panel_mixed_combo_cell_cb (GtkCellLayout *cell_layout,
    GtkCellRenderer   *renderer,
    GtkTreeModel      *model,
    GtkTreeIter       *iter,
    gpointer           data);

static void ncp_combo_cell_cb (GtkCellLayout *cell_layout,
    GtkCellRenderer   *renderer,
    GtkTreeModel      *model,
    GtkTreeIter       *iter,
    gpointer           data);

static void nwam_pref_iface_combo_changed_cb(GtkComboBox* combo, gpointer user_data);

void
daemon_compose_ncu_panel_mixed_combo(GtkComboBox *combo, NwamuiDaemon *daemon)
{
	GtkCellRenderer *renderer;
	GtkTreeModel      *model;
        
	model = GTK_TREE_MODEL(gtk_tree_store_new(1, G_TYPE_OBJECT));
	
	gtk_combo_box_set_model(GTK_COMBO_BOX(combo), model);
	g_object_unref(model);

	gtk_cell_layout_clear(GTK_CELL_LAYOUT(combo));
	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), renderer, TRUE);
	gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(combo),
	    renderer,
	    ncu_panel_mixed_combo_cell_cb,
	    NULL,
	    NULL);
	gtk_combo_box_set_row_separator_func (combo,
	    ncu_panel_mixed_combo_separator_cb,
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
daemon_compose_ncp_combo(GtkComboBox *combo, NwamuiDaemon *daemon,
    NwamPrefIFace *iface)
{
	GtkCellRenderer *renderer;
	GtkTreeModel      *model;
        
	model = GTK_TREE_MODEL(gtk_list_store_new(1, G_TYPE_OBJECT));
	
	gtk_combo_box_set_model(GTK_COMBO_BOX(combo), model);
	g_object_unref(model);

	gtk_cell_layout_clear(GTK_CELL_LAYOUT(combo));
	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), renderer, TRUE);
	gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(combo),
	    renderer,
	    ncp_combo_cell_cb,
	    NULL,
	    NULL);
	g_signal_connect(G_OBJECT(combo), "changed",
	    (GCallback)nwam_pref_iface_combo_changed_cb, (gpointer)iface);
}

void
daemon_update_ncp_combo(GtkComboBox *combo, NwamuiDaemon *daemon)
{
	GtkTreeModel      *model;
        
	model = gtk_combo_box_get_model(GTK_COMBO_BOX(combo));

}

static void
ncp_combo_cell_cb (GtkCellLayout *cell_layout,
    GtkCellRenderer   *renderer,
    GtkTreeModel      *model,
    GtkTreeIter       *iter,
    gpointer           data)
{
	gpointer row_data = NULL;
	gchar *text = NULL;
	
	gtk_tree_model_get(model, iter, 0, &row_data, -1);
	
	g_assert (row_data);
	g_assert (NWAMUI_IS_NCP (row_data));
	
	text = nwamui_ncp_get_name(NWAMUI_NCP(row_data));
	g_object_set (G_OBJECT(renderer), "text", text, NULL);
	g_free (text);
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
	} else if (type == NWAMUI_TYPE_ENM) {
		name = nwamui_enm_get_name(NWAMUI_ENM(obj));
	} else {
		g_assert_not_reached();
		name = g_strdup("unknow display name");
	}
	return name;
}
