#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include "capplet-utils.h"

typedef struct _CappletForeachData {
	GtkTreeModelForeachFunc foreach_func;
	gpointer user_data;
	gpointer ret_data;
} CappletForeachData;

static void nwam_pref_iface_combo_changed_cb(GtkComboBox* combo, gpointer user_data);

void
capplet_compose_nwamui_obj_combo(GtkComboBox *combo, NwamPrefIFace *iface)
{
	capplet_compose_combo(combo,
	    G_TYPE_OBJECT,
	    nwamui_object_name_cell,
	    NULL,
	    nwam_pref_iface_combo_changed_cb,
	    (gpointer)iface,
	    NULL);
}

static gboolean
capplet_model_foreach_find_object(GtkTreeModel *model,
    GtkTreePath *path,
    GtkTreeIter *iter,
    gpointer user_data)
{
	CappletForeachData *data = (CappletForeachData *)user_data;
	GObject *object;

        gtk_tree_model_get( GTK_TREE_MODEL(model), iter, 0, &object, -1);

        if (object == data->user_data) {
		data->ret_data = (gpointer)gtk_tree_iter_copy(iter);
        }
	if (object)
		g_object_unref(object);

	return data->ret_data != NULL;
}

static void
capplet_list_foreach_merge_to_model(gpointer data, gpointer user_data)
{
	GtkTreeModel *model = (GtkTreeModel *)user_data;
	GtkTreeIter   iter;

	gtk_list_store_append(GTK_LIST_STORE(model), &iter);
	gtk_list_store_set(GTK_LIST_STORE(model), &iter, 0, G_OBJECT(data), -1);

	g_object_unref(data);
}

void
capplet_model_update_from_list(GtkTreeModel *model, GList *obj_list)
{
	GtkTreeIter   iter;
	GList           *idx;

	
	for (idx = obj_list; idx; idx = g_list_next(idx)) {
		gtk_list_store_append(GTK_LIST_STORE(model), &iter);
		gtk_list_store_set (GTK_LIST_STORE(model), &iter,
		    0, idx->data,
                    -1);
		g_object_unref(idx->data);
	}
}

GtkTreeIter *
capplet_model_find_object(GtkTreeModel *model, GObject *object)
{
	CappletForeachData data;

	data.user_data = (gpointer)object;
	data.ret_data = NULL;

	gtk_tree_model_foreach(model,
	    capplet_model_foreach_find_object,
	    (gpointer)&data);

	return data.ret_data;
}

void
capplet_model_remove_object(GtkTreeModel *model, GObject *object)
{
	GtkTreeIter *iter = capplet_model_find_object(model, object);

	if (iter) {
		if (GTK_IS_LIST_STORE(model))
			gtk_list_store_remove(GTK_LIST_STORE(model), iter);
		else
			gtk_tree_store_remove(GTK_TREE_STORE(model), iter);

		gtk_tree_iter_free(iter);
	}
}

void
capplet_update_model_from_daemon(GtkTreeModel *model, NwamuiDaemon *daemon, GType type)
{
	GList *obj_list;

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

	g_list_foreach(obj_list, capplet_list_foreach_merge_to_model, (gpointer)model);
	g_list_free(obj_list);
}

void
nwamui_object_name_cell (GtkCellLayout *cell_layout,
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
	
		text = nwamui_object_get_name(object);

		g_object_unref(object);
	} else {
		text = g_strdup(_("No name"));
	}
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
capplet_list_store_add(GtkTreeModel *model, NwamuiObject *object)
{
	GtkTreeIter iter;
	gtk_list_store_append(GTK_LIST_STORE(model), &iter);
	gtk_list_store_set(GTK_LIST_STORE(model), &iter, 0, object, -1);
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

void
nwamui_object_name_cell_edited ( GtkCellRendererText *cell,
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

void
nwamui_object_active_mode_text_cell (GtkTreeViewColumn *col,
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
    case NWAMUI_COND_ACTIVATION_MODE_CONDITIONAL_ALL:
            object_markup= g_strdup_printf(_("<b>%s</b>"), "A");
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

GtkTreeViewColumn *
capplet_column_new(GtkTreeView *treeview, ...)
{
	GtkTreeViewColumn *col = gtk_tree_view_column_new();
	va_list args;
	gchar *attribute;

	va_start(args, treeview);
	attribute = va_arg(args, gchar *);
	if (attribute)
		g_object_set_valist(col, attribute, args);
	va_end (args);

	gtk_tree_view_append_column (treeview, col);
	return col;
}

GtkCellRenderer *
capplet_column_append_cell(GtkTreeViewColumn *col,
    GtkCellRenderer *cell, gboolean expand,
    GtkTreeCellDataFunc func, gpointer user_data, GDestroyNotify destroy)
{
	gtk_tree_view_column_pack_start(col, cell, expand);
	gtk_tree_view_column_set_cell_data_func (col, cell, func, user_data, destroy);
	return cell;
}

static gboolean
tree_model_find_object_cb(GtkTreeModel *model,
  GtkTreePath *path,
  GtkTreeIter *iter,
  gpointer user_data)
{
	CappletForeachData *data = (CappletForeachData*)user_data;
	NwamuiObject *obj;

	gtk_tree_model_get(model, iter, 0, &obj, -1);
	/* Safely unref */
	g_object_unref(obj);

	if (obj == data->user_data) {
		GtkTreeIter *iter_ret = (GtkTreeIter*)data->ret_data;

		*iter_ret = *iter;

		data->ret_data = NULL;
	}
	return data->ret_data == NULL;
}

gboolean
capplet_model_get_iter(GtkTreeModel *model, NwamuiObject *object, GtkTreeIter *iter)
{
	CappletForeachData data;

	data.user_data = (gpointer)object;
	data.ret_data = (gpointer)iter;

	gtk_tree_model_foreach(model,
	    tree_model_find_object_cb,
	    (gpointer)&data);

	return data.ret_data == NULL;
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

void
capplet_tree_store_move_object(GtkTreeModel *model,
    GtkTreeIter *target,
    GtkTreeIter *source)
{
	NwamuiObject *object;
	gtk_tree_model_get(GTK_TREE_MODEL(model), source, 0, &object, -1);
	gtk_tree_store_set(GTK_TREE_STORE(model), target, 0, object, -1);
	g_object_unref(object);
}

void
capplet_tree_store_merge_children(GtkTreeStore *model,
    GtkTreeIter *target,
    GtkTreeIter *source,
    GtkTreeModelForeachFunc func,
    gpointer user_data)
{
	GtkTreeIter t_iter;
	GtkTreeIter s_iter;
	GtkTreeIter iter;

	if (!gtk_tree_model_iter_children(GTK_TREE_MODEL(model), &t_iter, target)) {
		/* Think about the target position? */
	}

	/* If source doesn't have children, then we move it */
	if (gtk_tree_model_iter_children(GTK_TREE_MODEL(model), &s_iter, source)) {
		do {
			/* Take care if there are customer monitoring
			 * row-add/delete signals */
			gtk_tree_store_insert_before(GTK_TREE_STORE(model), &iter, target, NULL);
			capplet_tree_store_move_object(GTK_TREE_MODEL(model),
			    &iter, &s_iter);

			if (func)
				func(GTK_TREE_MODEL(model), NULL, &iter, user_data);

		} while (gtk_tree_store_remove(GTK_TREE_STORE(model), &s_iter));
	} else {
		gtk_tree_store_insert_before(GTK_TREE_STORE(model), &iter, target, NULL);
		capplet_tree_store_move_object(GTK_TREE_MODEL(model),
		    &iter, source);

		if (func)
			func(GTK_TREE_MODEL(model), NULL, &iter, user_data);

	}
	/* Remove the parent */
	gtk_tree_store_remove(GTK_TREE_STORE(model), source);
}

void
capplet_tree_store_exclude_children(GtkTreeStore *model,
    GtkTreeIter *source,
    GtkTreeModelForeachFunc func,
    gpointer user_data)
{
	GtkTreeIter target;
	GtkTreeIter t_iter;
	GtkTreeIter s_iter;
	GtkTreeIter iter;

	if (gtk_tree_model_iter_children(GTK_TREE_MODEL(model), &s_iter, source)) {
		do {
			/* Take care if there are customer monitoring
			 * row-add/delete signals */
			gtk_tree_store_insert_before(GTK_TREE_STORE(model), &iter, NULL, source);
			capplet_tree_store_move_object(GTK_TREE_MODEL(model),
			    &iter, &s_iter);

			if (func)
				func(GTK_TREE_MODEL(model), NULL, &iter, user_data);

		} while (gtk_tree_store_remove(GTK_TREE_STORE(model), &s_iter));

		/* Remove the parent */
		gtk_tree_store_remove(GTK_TREE_STORE(model), source);
	} else {
		if (gtk_tree_model_iter_parent(GTK_TREE_MODEL(model), &target, source)) {

			gtk_tree_store_insert_before(GTK_TREE_STORE(model), &iter, NULL, &target);
			capplet_tree_store_move_object(GTK_TREE_MODEL(model),
			    &iter, source);

			if (func)
				func(GTK_TREE_MODEL(model), NULL, &iter, user_data);

			if (!gtk_tree_store_remove(GTK_TREE_STORE(model), source)) {
				/* Doesn't have sibling, delete parent */
				gtk_tree_store_remove(GTK_TREE_STORE(model), &target);
			}

		} else {
			/* Nothing to do is no parent? */
			return;
		}
	}
}

gboolean
capplet_tree_view_expand_row(GtkTreeView *treeview,
    GtkTreeIter *iter,
    gboolean open_all)
{
	GtkTreePath *path = gtk_tree_model_get_path(gtk_tree_view_get_model(treeview), iter);
	gboolean ret = gtk_tree_view_expand_row(treeview, path, open_all);
	gtk_tree_path_free(path);
	return ret;
}

gboolean
capplet_tree_view_collapse_row(GtkTreeView *treeview,
    GtkTreeIter *iter)
{
	GtkTreePath *path = gtk_tree_model_get_path(gtk_tree_view_get_model(treeview), iter);
	gboolean ret = gtk_tree_view_collapse_row(treeview, path);
	gtk_tree_path_free(path);
	return ret;
}

gboolean
capplet_tree_view_commit_object(NwamTreeView *self, NwamuiObject *object, gpointer user_data)
{
	return nwamui_object_commit(object);
}

static gboolean
capplet_model_foreach_add_to_list(GtkTreeModel *model,
    GtkTreePath *path,
    GtkTreeIter *iter,
    gpointer user_data)
{
	CappletForeachData *data = (CappletForeachData *)user_data;
	GList *list = (GList*)data->ret_data;
	GObject *object;

        gtk_tree_model_get( GTK_TREE_MODEL(model), iter, 0, &object, -1);
	list = g_list_prepend(list, object);
	data->ret_data = (gpointer)list;

	return FALSE;
}

/* Convert model to a list, each element is ref'ed. */
GList*
capplet_model_to_list(GtkTreeModel *model)
{
	CappletForeachData data;

	data.ret_data = NULL;

	gtk_tree_model_foreach(model,
	    capplet_model_foreach_add_to_list,
	    (gpointer)&data);

	return data.ret_data;
}

static gboolean
capplet_model_find_max_name_suffix(GtkTreeModel *model,
    GtkTreePath *path,
    GtkTreeIter *iter,
    gpointer user_data)
{
	CappletForeachData *data = (CappletForeachData *)user_data;
	const gchar *prefix = data->user_data;
	gint *max = data->ret_data;
	NwamuiObject *object;

        gtk_tree_model_get( GTK_TREE_MODEL(model), iter, 0, &object, -1);
	
	if (object) {
		gchar *name = nwamui_object_get_name(object);
		gchar *endptr;
		gint num;
		if (g_str_has_prefix(name, prefix)) {
			gint prefix_len = strlen(prefix);
			if (*(name + prefix_len) != '\0') {
				num = strtoll(name + prefix_len + 1, &endptr, 10);
				if (num > 0 && *max < num)
					*max = num;
			} else
				*max = 0;
		}
		g_free(name);
		g_object_unref(object);
	}
	return FALSE;
}

gchar*
capplet_get_increasable_name(GtkTreeModel *model, const gchar *prefix, GObject *object)
{
	gchar *name;
	gint inc;

	g_return_val_if_fail(object && G_IS_OBJECT(object), NULL);

	inc = (gint)g_object_get_data(object, "capplet::increasable_name");
	g_assert(inc >= -1);

	/* Initial flag */
	if (inc < 0) {
		capplet_get_max_name_num(model, prefix);
	}

	if (++inc > 0)
		name = g_strdup_printf("%s %d", prefix, inc);
	else
		name = g_strdup(prefix);

	g_object_set_data(object, "capplet::increasable_name", (gpointer)inc);

	return name;
}

void
capplet_reset_increasable_name(GObject *object)
{
	g_object_set_data(object, "capplet::increasable_name", (gpointer)-1);
}

gint
capplet_get_max_name_num(GtkTreeModel *model, const gchar *prefix)
{
	CappletForeachData data;
	gint max;

	data.user_data = prefix;
	data.ret_data = &max;

	gtk_tree_model_foreach(model,
	    capplet_model_find_max_name_suffix,
	    (gpointer)&data);

	return max;
}

gchar*
capplet_get_original_name(const gchar *prefix, const gchar *name)
{
	if (g_str_has_prefix(name, prefix)) {
		gchar *oname = g_strdup(name);
		gchar *p_num = g_strrstr(oname, " ");
		gint num;
		gchar *endptr;
		if (p_num) {
			num = strtoll(p_num + 1, &endptr, 10);
			if (num > 0) {
				*p_num = '\0';
				return oname;
			}
		}
	}
	return NULL;
}

