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

#include <stdlib.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>

#include "capplet-utils.h"

typedef struct _CappletForeachData {
	GtkTreeModelForeachFunc foreach_func;
	gpointer user_data;
	gpointer ret_data;
} CappletForeachData;

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
        *(GtkTreeIter *)data->ret_data = *iter;
        /* Stop flag. */
        data->ret_data = NULL;
    }
	if (object)
		g_object_unref(object);

	return data->ret_data == NULL;
}

void
capplet_list_foreach_merge_to_list_store(gpointer data, gpointer user_data)
{
    CAPPLET_LIST_STORE_ADD(user_data, data);
	g_object_unref(data);
}

void
capplet_list_foreach_merge_to_tree_store(gpointer data, gpointer user_data)
{
    GtkTreeIter iter;
    CAPPLET_TREE_STORE_ADD(user_data, NULL, data, &iter);
	g_object_unref(data);
}

static gboolean
capplet_model_foreach_find_by_name(GtkTreeModel *model,
    GtkTreePath *path,
    GtkTreeIter *iter,
    gpointer user_data)
{
    CappletForeachData *data = (CappletForeachData *)user_data;
    GObject        *object;
    const gchar    *search_name = (const gchar*)data->user_data;
    gboolean       *found_p = (gboolean*)data->ret_data;

    gtk_tree_model_get( GTK_TREE_MODEL(model), iter, 0, &object, -1);

    if ( object ) {
        gchar   *name;

        name = nwamui_object_get_name(NWAMUI_OBJECT(object));

        if ( name != NULL && strcmp(name, search_name) == 0 ) {
            *found_p = TRUE;
        }
    }

    return( *found_p ); /* Stop search if we found something */
}

static gboolean 
capplet_model_check_exists( GtkTreeModel *model, const gchar *name )
{
    CappletForeachData  data;
    gboolean            exists = FALSE;
    NwamuiDaemon       *daemon;

    g_return_val_if_fail( model != NULL && name != NULL, exists );

	data.user_data = (gpointer)name;
	data.ret_data = (gpointer)&exists;

	gtk_tree_model_foreach(model, capplet_model_foreach_find_by_name,
                           (gpointer)&data);

    return( exists );
}

/*
 * capplet_model_find_object:
 * @object: Could be null
 * @iter: output var
 */
gboolean
capplet_model_find_object(GtkTreeModel *model, GObject *object, GtkTreeIter *iter)
{
	CappletForeachData data;

    g_return_val_if_fail(iter, FALSE);

	data.user_data = (gpointer)object;
	data.ret_data = iter;

	gtk_tree_model_foreach(model, capplet_model_foreach_find_object,
      (gpointer)&data);

	return data.ret_data == NULL;
}

gboolean
capplet_model_find_object_with_parent(GtkTreeModel *model, GtkTreeIter *parent, GObject *object, GtkTreeIter *iter)
{
	CappletForeachData data;
    GtkTreeIter i;
    gboolean valid;

    g_return_val_if_fail(iter, FALSE);

	data.user_data = (gpointer)object;
	data.ret_data = iter;

    for (valid = gtk_tree_model_iter_children(model, &i, parent);
         valid;
         valid = gtk_tree_model_iter_next(model, &i)) {
        if (capplet_model_foreach_find_object(model, NULL, &i, (gpointer)&data))
            return TRUE;
    }
    return FALSE;
}

gboolean
capplet_model_1_level_foreach(GtkTreeModel *model, GtkTreeIter *parent, GtkTreeModelForeachFunc func, gpointer user_data, GtkTreeIter *iter)
{
    gboolean valid;
	GtkTreeIter temp_iter;
	GtkTreePath *path = NULL;

    if (parent) {
        path = gtk_tree_model_get_path(model, parent);
    } else {
        path = gtk_tree_path_new_first();
    }

    if (iter == NULL)
        iter = &temp_iter;

    for (valid = gtk_tree_model_iter_children(model, iter, parent), gtk_tree_path_down(path);
         valid;
         valid = gtk_tree_model_iter_next(model, iter), gtk_tree_path_next(path)) {
        if (func(model, path, iter, user_data))
            break;
    }
    gtk_tree_path_free(path);
    return valid;
}

void
capplet_model_remove_object(GtkTreeModel *model, GObject *object)
{
	GtkTreeIter iter;

	if (capplet_model_find_object(model, object, &iter)) {
		if (GTK_IS_LIST_STORE(model))
			gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
		else
			gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);
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

	g_list_foreach(obj_list, capplet_list_foreach_merge_to_list_store, (gpointer)model);
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

void
nwam_pref_iface_combo_changed_cb(GtkComboBox *combo, gpointer user_data)
{
	GtkTreeIter iter;
    GObject *obj;

    obj = capplet_combo_get_active_object(combo);
    g_assert(G_IS_OBJECT(obj));
    nwam_pref_refresh(NWAM_PREF_IFACE(user_data), obj, FALSE);
    g_object_unref(obj);
}

void
capplet_compose_combo(GtkComboBox *combo,
  GType tree_store_type,
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

        if (tree_store_type == GTK_TYPE_LIST_STORE)
            model = GTK_TREE_MODEL(gtk_list_store_new(1, type));
        else if (tree_store_type == GTK_TYPE_TREE_STORE)
            model = GTK_TREE_MODEL(gtk_tree_store_new(1, type));
        else
            g_assert_not_reached();

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

		if (GTK_WIDGET_TOPLEVEL (w)) {
			parent = GTK_WINDOW(w);
		}
	}
	return nwam_pref_dialog_run(iface, parent);
}

gboolean
capplet_dialog_raise(NwamPrefIFace *iface)
{
    GtkWindow*  window;

    if ( ( window = nwam_pref_dialog_get_window(iface) ) != NULL ) {
        gtk_window_present(window);
        return ( TRUE );
    }

	return FALSE;
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

        if ( !capplet_model_check_exists( model, new_text ) ) {
            nwamui_object_set_name(object, new_text);
        }
        else {
            GtkWindow  *top_level = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(treeview)));
            gchar      *message;

            message = g_strdup_printf(_("The name '%s' is already in use"), new_text );

            nwamui_util_show_message( top_level, GTK_MESSAGE_ERROR, _("Name Already In Use"), message, FALSE);

            g_free(message);
        }

		g_object_unref(object);
	}
	gtk_tree_path_free (path);
}

void
nwamui_object_active_toggle_cell(GtkTreeViewColumn *col,
  GtkCellRenderer   *renderer,
  GtkTreeModel      *model,
  GtkTreeIter       *iter,
  gpointer           data)
{
    NwamuiObject*              object = NULL;
    
    gtk_tree_model_get(model, iter, 0, &object, -1);
    
    if ( object != NULL ) {
        /* We need show the active object instead of enabled object. */
        g_object_set(G_OBJECT(renderer),
          "active", nwamui_object_get_active(NWAMUI_OBJECT(object)),
          NULL); 
        g_object_unref(G_OBJECT(object));
    } else {
        g_object_set(G_OBJECT(renderer),
          "active", FALSE,
          NULL); 
    }
}

void
nwamui_object_active_mode_pixbuf_cell (GtkTreeViewColumn *col,
  GtkCellRenderer   *renderer,
  GtkTreeModel      *model,
  GtkTreeIter       *iter,
  gpointer           data)
{
    NwamuiObject*              object = NULL;
    gchar*                  object_markup;
    
    gtk_tree_model_get(model, iter, 0, &object, -1);
    
    if (object) {
        g_object_set(renderer,
          "icon-name", nwamui_util_get_active_mode_icon(object),
          NULL);

		g_object_unref(object);
    }

    return;
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
		g_object_set_valist(G_OBJECT(col), attribute, args);
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

GObject *
capplet_combo_get_active_object(GtkComboBox *combo)
{
	GtkTreeIter iter;

	if (gtk_combo_box_get_active_iter(combo, &iter)) {
		GObject *object;
		gtk_tree_model_get(gtk_combo_box_get_model(combo), &iter,
		    0, &object, -1);
		return object;
	}
	return NULL;
}

gboolean
capplet_combo_set_active_object(GtkComboBox *combo, GObject *object)
{
	GtkTreeIter iter;

	if (capplet_model_find_object(gtk_combo_box_get_model(combo), object, &iter)) {
		gtk_combo_box_set_active_iter(combo, &iter);
        return TRUE;
    }
    return FALSE;
}

void
capplet_tree_store_move_object(GtkTreeModel *model,
    GtkTreeIter *target,
    GtkTreeIter *source)
{
	NwamuiObject *object;
	gtk_tree_model_get(GTK_TREE_MODEL(model), source, 0, &object, -1);
	gtk_tree_store_set(GTK_TREE_STORE(model), target, 0, object, -1);
    if (object)
        g_object_unref(object);
}

/**
 * capplet_tree_store_merge_children:
 * @func is used to iterate the moved item, its return code is ignored.
 * Move source children to the target.
 */
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
		/* do { */
			/* Take care if there are customer monitoring
			 * row-add/delete signals */
			gtk_tree_store_insert_before(GTK_TREE_STORE(model), &iter, target, NULL);
			capplet_tree_store_move_object(GTK_TREE_MODEL(model),
			    &iter, &s_iter);

			if (func)
				func(GTK_TREE_MODEL(model), NULL, &iter, user_data);

		/* } while (gtk_tree_store_remove(GTK_TREE_STORE(model), &s_iter)); */
	} else {
		gtk_tree_store_insert_before(GTK_TREE_STORE(model), &iter, target, NULL);
		capplet_tree_store_move_object(GTK_TREE_MODEL(model), &iter, source);

		if (func)
			func(GTK_TREE_MODEL(model), NULL, &iter, user_data);

	}
	/* Remove the parent */
	gtk_tree_store_remove(GTK_TREE_STORE(model), source);
}

/**
 * capplet_tree_store_exclude_children:
 * @func is used to iterate the moved item, its return code is ignored.
 * Make the children become their parent's siblings.
 */
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
			capplet_tree_store_move_object(GTK_TREE_MODEL(model), &iter, source);

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
            } else if (*max < 0) {
                *max = 0;
            }
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
	gint inc = -1;

	g_return_val_if_fail(object && G_IS_OBJECT(object), NULL);

	/* Initial flag */
	if (inc < 0) {
		inc = capplet_get_max_name_num(model, prefix, inc);
	}

	if (++inc > 0)
		name = g_strdup_printf("%s %d", prefix, inc);
	else
		name = g_strdup(prefix);

	return name;
}

gint
capplet_get_max_name_num(GtkTreeModel *model, const gchar *prefix, gint base)
{
	CappletForeachData data;
	gint max = base;

	data.user_data = (gpointer)prefix;
	data.ret_data = (gpointer)&max;

	gtk_tree_model_foreach(model,
	    capplet_model_find_max_name_suffix,
	    (gpointer)&data);

	return max;
}

/**
 * capplet_get_original_name:
 * @prefix: a string prefix for duplicating names, e.g. Copy of ...
 * @name: maybe has contained a prefix
 * Avoid Copy of Copy of ... 
 *
 * Return: the composed name including the @prefix but the last number
 * if it is there.
 */
gchar*
capplet_get_original_name(const gchar *prefix, const gchar *name)
{
    gchar *prefix_str = g_strconcat(prefix, " ", NULL);
    gchar *oname = NULL;

	if (g_str_has_prefix(name, prefix_str)) {
        gchar *p_num;
		gint num;
		gchar *endptr;

		oname = g_strdup(name);
        /* Assume a blank is ahead of a number, so delete the number. */
		p_num = g_strrstr(oname, " ");
		if ((size_t)(p_num - oname) > strlen(prefix_str)) {
			num = strtoll(p_num + 1, &endptr, 10);
			if (num > 0) {
				*p_num = '\0';
			}
		}
	} else {
        oname = g_strdup_printf(_("%s %s"), prefix_str, name);
    }
    g_free(prefix_str);
    return g_strstrip(oname);
}

extern void
capplet_util_object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
    GtkTreeModel   *model = GTK_TREE_MODEL(data);
    GtkTreeIter     iter;
    gboolean        valid_iter = FALSE;

    g_debug("capplet-utils: %s's notify '%s' changed", g_type_name(G_TYPE_FROM_INSTANCE(gobject)), arg1->name);

    if (capplet_model_find_object(model, gobject, &iter)) {
        GtkTreePath *path;

        path = gtk_tree_model_get_path(GTK_TREE_MODEL(model),
          &iter);
        gtk_tree_model_row_changed(GTK_TREE_MODEL(model),
          path,
          &iter);
        gtk_tree_path_free(path);
/*     } else { */
/*         g_signal_handlers_disconnect_by_func(gobject, */
/*           G_CALLBACK(capplet_util_object_notify_cb), data); */
    }
}

