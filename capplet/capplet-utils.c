#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include "capplet-utils.h"

static gboolean ncu_panel_mixed_combo_separator_cb(GtkTreeModel *model,
    GtkTreeIter *iter,
    gpointer data);

static void ncu_panel_mixed_combo_cell_cb (GtkCellLayout *cell_layout,
    GtkCellRenderer   *renderer,
    GtkTreeModel      *model,
    GtkTreeIter       *iter,
    gpointer           data);

static void nwamui_obj_name_cell_cb (GtkCellLayout *cell_layout,
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
	    nwamui_obj_name_cell_cb,
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
nwamui_obj_name_cell_cb (GtkCellLayout *cell_layout,
    GtkCellRenderer   *renderer,
    GtkTreeModel      *model,
    GtkTreeIter       *iter,
    gpointer           data)
{
	NwamuiObject *object = NULL;
	gchar *text = NULL;
	
	gtk_tree_model_get(model, iter, 0, &object, -1);
	
	if (object) {
		g_assert(NWAMUI_IS_OBJECT (object));
	
		if (object) {
			text = nwamui_object_get_name(object);
		} else {
			text = g_strdup(_("No name"));
		}

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
		nwam_pref_refresh(NWAM_PREF_IFACE(user_data), obj, FALSE);
		g_object_unref(obj);
	}
}

void
capplet_compose_nwamui_obj_treeview(GtkTreeView *treeview)
{
        GtkTreeModel *model;

        model = gtk_tree_view_get_model(treeview);
        if (model == NULL) {
		model = GTK_TREE_MODEL(gtk_list_store_new(1, NWAMUI_TYPE_OBJECT));
		gtk_tree_view_set_model(treeview, model);
		g_object_unref(model);
        }
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

        gtk_widget_hide(GTK_WIDGET(treeview));

	for (idx = obj_list; idx; idx = g_list_next(idx)) {
		gtk_list_store_append(GTK_LIST_STORE(model), &iter);
		gtk_list_store_set (GTK_LIST_STORE(model), &iter,
		    0, idx->data,
                    -1);
		g_object_unref(idx->data);
	}
        gtk_widget_show(GTK_WIDGET(treeview));

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

gint
capplet_dialog_run(NwamPrefIFace *iface, GtkWidget *w)
{
	GtkWindow *parent = NULL;

	if (w) {
		w = gtk_widget_get_toplevel(w);

		if (!GTK_WIDGET_TOPLEVEL (w)) {
			parent = GTK_WINDOW(w);
		}
	}
	return nwam_pref_dialog_run(iface, parent);
}

static void
nwamui_obj_name_cell_edited_cb ( GtkCellRendererText *cell,
                     const gchar         *path_string,
                     const gchar         *new_text,
                     gpointer             data)
{
	GtkTreeView *treeview = GTK_TREE_VIEW(data);
	GtkTreeModel *model = gtk_tree_view_get_model(treeview);
	GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
	GtkTreeIter iter;

	if ( gtk_tree_model_get_iter (model, &iter, path)) {
		NwamuiObject*  object;
		gtk_tree_model_get(model, &iter, 0, &object, -1);

		nwamui_object_set_name(object, new_text);

		g_object_unref(object);
	}
	gtk_tree_path_free (path);
}

static void
capplet_active_mode_renderer_cell_cb (GtkTreeViewColumn *col,
  GtkCellRenderer   *renderer,
  GtkTreeModel      *model,
  GtkTreeIter       *iter,
  gpointer           data)
{
    NwamuiObject*              object = NULL;
    gchar*                  object_markup;
    
    gtk_tree_model_get(model, iter, 0, &object, -1);
    
    switch (nwamui_object_get_activation_mode(object)) {
    case NWAMUI_COND_ACTIVATION_MODE_MANUAL:
            object_markup= g_strdup_printf(_("<b>%s</b>"), "M");
            break;
    case NWAMUI_COND_ACTIVATION_MODE_SYSTEM:
            object_markup= g_strdup_printf(_("<b>%s</b>"), "S");
            break;
    case NWAMUI_COND_ACTIVATION_MODE_PRIORITIZED:
            object_markup= g_strdup_printf(_("<b>%s</b>"), "P");
            break;
    case NWAMUI_COND_ACTIVATION_MODE_CONDITIONAL_ANY:
            object_markup= g_strdup_printf(_("<b>%s</b>"), "C");
            break;
    default:
            g_assert_not_reached();
            break;
    }
    g_object_set(G_OBJECT(renderer),
	"markup", object_markup,
	NULL);
    g_free( object_markup );
}

void
capplet_name_column_new(GtkTreeView *treeview,
    GtkTreeViewColumn **col, const gchar *title,
    GtkCellRenderer **cell)
{
	*col = gtk_tree_view_column_new();
	gtk_tree_view_append_column (treeview, *col);

	g_object_set(*col,
	    "title", title,
	    "expand", TRUE,
	    "resizable", TRUE,
	    "clickable", TRUE,
	    "sort-indicator", TRUE,
	    "reorderable", TRUE,
	    NULL);

	*cell = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(*col, *cell, FALSE);
	gtk_tree_view_column_set_cell_data_func (*col, *cell,
	    (GtkTreeCellDataFunc)nwamui_obj_name_cell_cb, (gpointer) 0,
	    NULL);
	g_signal_connect(*cell, "edited",
	    G_CALLBACK(nwamui_obj_name_cell_edited_cb), (gpointer)treeview);
}

void
capplet_active_mode_column_new(GtkTreeView *treeview,
    GtkTreeViewColumn **col, const gchar *title,
    GtkCellRenderer **cell)
{
	*col = gtk_tree_view_column_new();
	gtk_tree_view_append_column (treeview, *col);

	g_object_set(*col,
	    "title", title,
	    "expand", FALSE,
	    "resizable", TRUE,
	    "clickable", TRUE,
	    "sort-indicator", FALSE,
	    "reorderable", TRUE,
	    NULL);

	*cell = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(*col, *cell, FALSE);
	gtk_tree_view_column_set_cell_data_func (*col, *cell,
	    capplet_active_mode_renderer_cell_cb, (gpointer) 0,
	    NULL);
}

static gboolean
tree_model_find_object_cb(GtkTreeModel *model,
  GtkTreePath *path,
  GtkTreeIter *iter,
  gpointer data)
{
	NwamuiObject *obj;

	gtk_tree_model_get(model, iter, 0, &obj, -1);
	/* Safely unref */
	g_object_unref(obj);

	if (obj == data) {
		GtkTreeIter *iter_ret = (GtkTreeIter*)g_object_get_data(model, "capplet:get_iter");
		g_object_set_data(model, "capplet:get_iter_valid", (gpointer)TRUE);
		*iter_ret = *iter;
		return TRUE;
	}
	return FALSE;
}

gboolean
capplet_model_get_iter(GtkTreeModel *model, NwamuiObject *object, GtkTreeIter *iter)
{
	g_object_set_data(model, "capplet:get_iter", (gpointer*)iter);
	g_object_set_data(model, "capplet:get_iter_valid", (gpointer)FALSE);
	gtk_tree_model_foreach(model,
	    tree_model_find_object_cb,
	    (gpointer)object);
	return (gboolean)g_object_get_data(model, "capplet:get_iter_valid");
}

NwamuiObject *
capplet_combo_get_active(GtkComboBox *combo)
{
	GtkTreeIter iter;

	if (gtk_combo_box_get_active_iter(combo, &iter)) {
		NwamuiObject *object;
		gtk_tree_model_get(gtk_combo_box_get_model(combo), &iter,
		    0, &object, -1);
		return object;
	}
	return NULL;
}

void
capplet_combo_set_active(GtkComboBox *combo, NwamuiObject *object)
{
	GtkTreeIter iter;

	if (capplet_model_get_iter(gtk_combo_box_get_model(combo), object, &iter))
		gtk_combo_box_set_active_iter(combo, &iter);
}
