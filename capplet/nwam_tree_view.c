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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <stdlib.h>

#include "libnwamui.h"
#include "capplet-utils.h"
#include "nwam_object_ctrl_iface.h"
#include "nwam_tree_view.h"

enum {
    PROP_CTRL_IFACE = 1,
};

enum {
    PLACE_HOLDER,
    LAST_SIGNAL
};

static guint nwam_tree_view_signals[LAST_SIGNAL] = {0};

typedef struct _NwamTreeViewPrivate NwamTreeViewPrivate;
struct _NwamTreeViewPrivate {
    NwamObjectCtrlIFace *ctrl_iface;
    GtkTreeModel *model;
    GtkTreeRowReference *new_obj_ref;
    gint count_selected_rows;
    GtkTreePath *selected_path;
    GtkWidget *widget_list[NTV_N_BTNS];
    GList *widgets;
    gpointer object_func_user_data;

};

#define NWAM_TREE_VIEW_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), NWAM_TYPE_TREE_VIEW, NwamTreeViewPrivate))

static void nwam_tree_view_set_property (GObject *object,
  guint prop_id,
  const GValue *value,
  GParamSpec *pspec);
static void nwam_tree_view_get_property (GObject *object,
  guint prop_id,
  GValue *value,
  GParamSpec *pspec);

static GObject* nwam_tree_view_constructor(GType type,
  guint n_construct_properties,
  GObjectConstructParam *construct_properties);
static void nwam_tree_view_finalize (NwamTreeView *self);

/* Call backs */
static void on_add_btn_clicked(GtkButton *button, gpointer user_data);
static void on_remove_btn_clicked(GtkButton *button, gpointer user_data);
static void on_rename_btn_clicked(GtkButton *button, gpointer user_data);
static void on_edit_btn_clicked(GtkButton *button, gpointer user_data);
static void on_dup_btn_clicked(GtkButton *button, gpointer user_data);
static void on_up_btn_clicked(GtkButton *button, gpointer user_data);
static void on_down_btn_clicked(GtkButton *button, gpointer user_data);

static gpointer w_cb_pairs[NTV_N_BTNS] = {
    (gpointer)on_add_btn_clicked, /* NTV_ADD_BTN, */
    (gpointer)on_remove_btn_clicked, /* NTV_REM_BTN, */
    (gpointer)on_rename_btn_clicked, /* NTV_REN_BTN, */
    (gpointer)on_edit_btn_clicked, /* NTV_EDIT_BTN, */
    (gpointer)on_dup_btn_clicked, /* NTV_DUP_BTN, */
    (gpointer)on_up_btn_clicked, /* NTV_UP_BTN, */
    (gpointer)on_down_btn_clicked, /* NTV_DOWN_BTN, */
    /* NTV_N_BTNS, */
};

static gboolean nwam_tree_view_key_press(GtkWidget   *widget, GdkEventKey *event);
static gboolean nwam_tree_view_key_release(GtkWidget   *widget, GdkEventKey *event);
static void default_signal_handler_activate_widget(NwamTreeView *self, GtkWidget *widget, gpointer user_data);

static void nwam_tree_view_row_activated_cb(GtkTreeView *tree_view,
  GtkTreePath *path,
  GtkTreeViewColumn *column,
  gpointer user_data);
static void nwam_tree_view_row_selected_cb(GtkTreeView *tree_view, gpointer user_data);
static void nwam_tree_view_selection_changed_cb(GtkTreeSelection *selection, gpointer user_data);
static void nwam_tree_view_update_widget_cb (gpointer data, gpointer user_data);
static void connect_model_signals(GObject *self, GtkTreeModel *model);
static void disconnect_model_signals(GObject *self, GtkTreeModel *model);

static void nwam_tree_view_row_inserted(GtkTreeModel *tree_model,
  GtkTreePath  *path,
  GtkTreeIter  *iter,
  gpointer      user_data);
static void nwam_tree_view_row_changed(GtkTreeModel *tree_model,
  GtkTreePath  *path,
  GtkTreeIter  *iter,
  gpointer      user_data);
static void nwam_tree_view_row_deleted(GtkTreeModel *tree_model,
  GtkTreePath *path,
  gpointer user_data);

static void object_tree_model_notify(GObject *gobject, GParamSpec *arg1, gpointer user_data);

static void update_buttons_status(NwamTreeView *self);

G_DEFINE_TYPE(NwamTreeView, nwam_tree_view, GTK_TYPE_TREE_VIEW)

static const gchar*
nwam_tree_view_property_name(gint prop_id) /* unused */
{
	switch (prop_id) {
    case NTV_ADD_BTN:
        return "add";
    case NTV_REMOVE_BTN:
        return "remove";
    case NTV_EDIT_BTN:
        return "edit";
    case NTV_RENAME_BTN:
        return "rename";
    case NTV_UP_BTN:
        return "up";
    case NTV_DOWN_BTN:
        return "down";
    default:
        g_assert_not_reached();
        break;
    }
}

static void
nwam_tree_view_class_init (NwamTreeViewClass *klass)
{
	GObjectClass *gobject_class;
    GtkWidgetClass *widget_class;

	gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->constructor = nwam_tree_view_constructor;
	gobject_class->finalize = (void (*)(GObject*)) nwam_tree_view_finalize;
	gobject_class->set_property = nwam_tree_view_set_property;
	gobject_class->get_property = nwam_tree_view_get_property;

    widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->key_press_event = nwam_tree_view_key_press;
    widget_class->key_release_event = nwam_tree_view_key_release;

    g_object_class_install_property(gobject_class,
      PROP_CTRL_IFACE,
      g_param_spec_object("nwam_object_ctrl",
        _("NwamObject Contrl Interface"),
        _("NwamObject Contrl Interface"),
        G_TYPE_OBJECT,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT/* _ONLY */));

/*     { */
/*         int propid; */
/*         for (propid = PROP_BUTTON_BEGIN + 1; propid < NTV_N_BTNS; propid ++) { */
/*             g_object_class_install_property (gobject_class, */
/*               propid, */
/*               g_param_spec_object(nwam_tree_view_property_name(propid), */
/*                 _("buttons of tree view"), */
/*                 _("buttons of tree view"), */
/*                 GTK_TYPE_BUTTON, */
/*                 G_PARAM_READWRITE | G_PARAM_CONSTRUCT/\*_ONLY*\/)); */
/*         } */
/*     } */
    
    klass->activate_widget = default_signal_handler_activate_widget;

    g_type_class_add_private(klass, sizeof(NwamTreeViewPrivate));
}

static GObject*
nwam_tree_view_constructor(GType type,
  guint n_construct_properties,
  GObjectConstructParam *construct_properties)
{
    NwamTreeView *self;
    GObject *object;
	GError *error = NULL;

    object = G_OBJECT_CLASS(nwam_tree_view_parent_class)->constructor(type,
      n_construct_properties,
      construct_properties);
    self = NWAM_TREE_VIEW(object);

    return object;
}

static void
nwam_tree_view_finalize(NwamTreeView *self)
{
    NwamTreeViewPrivate *prv = NWAM_TREE_VIEW_PRIVATE(self);
    int id;

    g_list_foreach(prv->widgets, (GFunc)g_object_unref, NULL);

    for (id = NTV_ADD_BTN; id < NTV_N_BTNS; id ++) {
        if (prv->widget_list[id]) {
            g_object_unref(prv->widget_list[id]);
        }
    }

	G_OBJECT_CLASS(nwam_tree_view_parent_class)->finalize(G_OBJECT(self));
}

static void
nwam_tree_view_set_property (GObject *object,
  guint prop_id,
  const GValue *value,
  GParamSpec *pspec)
{
	NwamTreeView *self = NWAM_TREE_VIEW(object);
    NwamTreeViewPrivate *prv = NWAM_TREE_VIEW_PRIVATE(self);

	switch (prop_id) {
    case PROP_CTRL_IFACE:
        prv->ctrl_iface = NWAM_OBJECT_CTRL_IFACE(g_value_dup_object(value));
        break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
nwam_tree_view_get_property (GObject *object,
  guint prop_id,
  GValue *value,
  GParamSpec *pspec)
{
	NwamTreeView *self = NWAM_TREE_VIEW(object);
    NwamTreeViewPrivate *prv = NWAM_TREE_VIEW_PRIVATE(self);

	switch (prop_id) {
    case PROP_CTRL_IFACE:
        g_value_set_object(value, prv->ctrl_iface);
        break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
nwam_tree_view_init(NwamTreeView *self)
{
    NwamTreeViewPrivate *prv = NWAM_TREE_VIEW_PRIVATE(self);
    GtkTreeSelection *selection;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(self));
	g_signal_connect(selection,
      "changed",
      (GCallback)nwam_tree_view_selection_changed_cb,
      (gpointer)self);

	g_signal_connect(GTK_TREE_VIEW(self),
      "cursor-changed",
      (GCallback)nwam_tree_view_row_selected_cb,
      (gpointer)self);

	g_signal_connect(GTK_TREE_VIEW(self),
      "row-activated",
      (GCallback)nwam_tree_view_row_activated_cb,
      (gpointer)self);

	g_signal_connect(G_OBJECT(self),
      "notify::model", (GCallback)object_tree_model_notify, NULL);

}

GtkWidget*
nwam_tree_view_new()
{
    NwamTreeView *self = g_object_new(NWAM_TYPE_TREE_VIEW, NULL);
    return GTK_WIDGET(self);
}

void
nwam_tree_view_register_widget(NwamTreeView *self, NwamTreeViewSupportWidgets id, GtkWidget *w)
{
    NwamTreeViewPrivate *prv = NWAM_TREE_VIEW_PRIVATE(self);

    g_assert(id >= NTV_ADD_BTN && id < NTV_N_BTNS);
    g_assert(w);
    g_assert(prv->widget_list[id] == NULL);

    prv->widget_list[id] = g_object_ref(w);
    g_return_if_fail(g_list_find(prv->widgets, w) == NULL);

    prv->widgets = g_list_prepend(prv->widgets, w);

    switch (id) {
    case NTV_ADD_BTN:
        /* TODO, start the cache function. Or we disable it if add button is
         * removed */
        if (prv->model) {
            connect_model_signals(G_OBJECT(self), prv->model);
        }
        /* Fall through */
    /* case NTV_REMOVE_BTN: */
    /* case NTV_EDIT_BTN: */
    /* case NTV_DUP_BTN: */
    /* case NTV_RENAME_BTN: */
    /* case NTV_UP_BTN: */
    /* case NTV_DOWN_BTN: */
    default:
        g_assert(GTK_IS_BUTTON(w));
        g_signal_connect(w, "clicked", G_CALLBACK(w_cb_pairs[id]), (gpointer)self);
        break;
    }
}

void
nwam_tree_view_unregister_widget(NwamTreeView *self, NwamTreeViewSupportWidgets id, GtkWidget *w)
{
    NwamTreeViewPrivate *prv = NWAM_TREE_VIEW_PRIVATE(self);

    g_assert(id >= NTV_ADD_BTN && id < NTV_N_BTNS);
    g_assert(w);
    g_assert(prv->widget_list[id]);

    g_return_if_fail(prv->widget_list[id] != (gpointer)w);
    g_return_if_fail(g_list_find(prv->widgets, w));

    prv->widgets = g_list_remove(prv->widgets, w);
    g_object_unref(w);
    prv->widget_list[id] = NULL;
}

GObject*
nwam_tree_view_get_item_by_proxy(NwamTreeView *self, GObject *proxy)
{
}

static void
on_add_btn_clicked(GtkButton *button, gpointer user_data)
{
    NwamTreeViewPrivate *prv = NWAM_TREE_VIEW_PRIVATE(user_data);
	NwamTreeView *self = NWAM_TREE_VIEW(user_data);
    GObject *obj;

    g_assert(prv->ctrl_iface);

    obj = nwam_object_ctrl_create(prv->ctrl_iface);

    if (obj) {
        GtkTreeIter iter;
        GtkTreePath *path;
        CAPPLET_LIST_STORE_ADD(GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(self))), obj, &iter);
        path = gtk_tree_model_get_path(gtk_tree_view_get_model(GTK_TREE_VIEW(self)), &iter);

        /* TODO */
        /* gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE); */

        /* Cache the ref to the new created object. */
        /* if (prv->new_obj_ref) { */
        /*     if (gtk_tree_row_reference_valid(prv->new_obj_ref)) { */
        /*         GtkTreePath *oldpath = gtk_tree_row_reference_get_path(prv->new_obj_ref); */
        /*         if (gtk_tree_path_compare(path, oldpath) == 0) { */
        /*             return; */
        /*         } */
        /*     } */
        /*     gtk_tree_row_reference_free(prv->new_obj_ref); */
        /* } */
        /* prv->new_obj_ref = gtk_tree_row_reference_new(tree_model, path); */
        /* g_assert(prv->new_obj_ref); */
        
        /* Append a new row will cause this, we disable add button here. */
        /* gtk_widget_set_sensitive(prv->widget_list[NTV_ADD_BTN], FALSE); */

        /* Select and scroll to this new object. */
        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(self), path, NULL, FALSE, 0.0, 0.0);
        gtk_tree_selection_select_path(gtk_tree_view_get_selection(GTK_TREE_VIEW(self)), path);

        /* If there is rename button then trigger it for the new object. */
        if (prv->widget_list[NTV_RENAME_BTN]) {
            gtk_button_clicked(GTK_BUTTON(prv->widget_list[NTV_RENAME_BTN]));
        }

        gtk_tree_path_free(path);
        g_object_unref(obj);
    } else {
        g_debug("Fail to 'create_object'.");
    }
    update_buttons_status(self);
}

static void
on_remove_btn_clicked(GtkButton *button, gpointer user_data)
{
	NwamTreeView        *self = NWAM_TREE_VIEW(user_data);
    NwamTreeViewPrivate *prv  = NWAM_TREE_VIEW_PRIVATE(self);
    GtkTreeModel        *model;
    GtkTreeIter          iter;

    g_assert(prv->ctrl_iface);

    if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW(self)), &model, &iter)) {
        GObject *obj;

        gtk_tree_model_get(model, &iter, 0, &obj, -1);
        if (nwam_object_ctrl_remove(prv->ctrl_iface, obj)) {
            gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
        }
        g_object_unref(obj);
    }
    update_buttons_status(self);
}

static void
on_rename_btn_clicked(GtkButton *button, gpointer user_data)
{
	NwamTreeView *self = NWAM_TREE_VIEW(user_data);
    NwamTreeViewPrivate *prv = NWAM_TREE_VIEW_PRIVATE(self);
    GtkTreeModel *model;
    GtkTreeIter iter;

    g_assert(prv->ctrl_iface);

    if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW(self)), &model, &iter)) {
        GObject *obj;

        gtk_tree_model_get(model, &iter, 0, &obj, -1);

        if (nwam_object_ctrl_rename(prv->ctrl_iface, obj)) {
            GList *cols      = NULL;
            GList *renderers = NULL;
            /* Find the first editable text renderer. */
            for (cols = gtk_tree_view_get_columns(GTK_TREE_VIEW(self));
                 cols;
                 cols = g_list_delete_link(cols, cols)) {
                for (renderers = gtk_cell_layout_get_cells(GTK_CELL_LAYOUT(cols->data));
                     renderers;
                     renderers = g_list_delete_link(renderers, renderers)) {
                    if (GTK_IS_CELL_RENDERER_TEXT(renderers->data)) {
                        GtkTreeModel        *model = gtk_tree_view_get_model(GTK_TREE_VIEW(self));
                        GtkCellRendererText *txt   = GTK_CELL_RENDERER_TEXT(renderers->data);
                        GtkTreeIter          iter;
                        GtkTreePath         *tpath = NULL;

                        gtk_tree_selection_get_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW(self)), NULL, &iter);
                        tpath = gtk_tree_model_get_path(model, &iter);
                        g_object_set(txt, "editable", TRUE, NULL);
                        gtk_tree_view_set_cursor(GTK_TREE_VIEW(self), tpath, cols->data, TRUE);
                        gtk_tree_path_free(tpath);

                        goto L_exit;
                    }
                }
            }
        L_exit:
            g_list_free(renderers);
            g_list_free(cols);
        }
        g_object_unref(obj);
    }
}

static void
on_edit_btn_clicked(GtkButton *button, gpointer user_data)
{
	NwamTreeView *self = NWAM_TREE_VIEW(user_data);
    NwamTreeViewPrivate *prv = NWAM_TREE_VIEW_PRIVATE(self);
    GtkTreeModel *model;
    GtkTreeIter iter;

    g_assert(prv->ctrl_iface);

    if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW(self)), &model, &iter)) {
        GtkTreePath *path;
        GObject *obj;

        gtk_tree_model_get(model, &iter, 0, &obj, -1);

        nwam_object_ctrl_edit(prv->ctrl_iface, obj);

        g_object_unref(obj);

        /* Refresh the row */
        path = gtk_tree_model_get_path(model, &iter);
        gtk_tree_model_row_changed(model, path, &iter);
        gtk_tree_path_free(path);
    }
}

static void
on_dup_btn_clicked(GtkButton *button, gpointer user_data)
{
	NwamTreeView *self = NWAM_TREE_VIEW(user_data);
    NwamTreeViewPrivate *prv = NWAM_TREE_VIEW_PRIVATE(self);
    GtkTreeModel        *model;
    GtkTreeIter          iter;

    g_assert(prv->ctrl_iface);

    if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW(self)), &model, &iter)) {
        GObject *obj;
        GObject *newobj;

        gtk_tree_model_get(model, &iter, 0, &obj, -1);
        newobj = nwam_object_ctrl_dup(prv->ctrl_iface, obj);
        if (newobj) {
            GtkTreeIter iter;
            GtkTreePath *path;
            /* We must add it to view first instead of relying on deamon
             * property changes, because we have to select the new added
             * row immediately and trigger a renaming mode.
             */
            CAPPLET_LIST_STORE_ADD(GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(self))), newobj, &iter);
            path = gtk_tree_model_get_path(gtk_tree_view_get_model(GTK_TREE_VIEW(self)), &iter);

            /* Select and scroll to this new object. */
            gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(self), path, NULL, FALSE, 0.0, 0.0);
            gtk_tree_selection_select_path(gtk_tree_view_get_selection(GTK_TREE_VIEW(self)), path);

            /* If there is rename button then trigger it for the new object. */
            if (prv->widget_list[NTV_RENAME_BTN]) {
                gtk_button_clicked(GTK_BUTTON(prv->widget_list[NTV_RENAME_BTN]));
            }

            gtk_tree_path_free(path);
            g_object_unref(newobj);
        } else {
            g_debug("Fail to 'dup_object'.");
        }
        g_object_unref(obj);
    }
    update_buttons_status(self);
}

static void
on_up_btn_clicked(GtkButton *button, gpointer user_data)
{
	NwamTreeView *self = NWAM_TREE_VIEW(user_data);
    NwamTreeViewPrivate *prv = NWAM_TREE_VIEW_PRIVATE(self);
    GtkTreeModel*               model;
    GtkTreeIter                 iter;
    GtkTreeSelection*           selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(self));

    if ( gtk_tree_selection_get_selected( selection, &model, &iter ) ) {
        GtkTreePath*    path = gtk_tree_model_get_path(model, &iter);
        /* See if we have somewhere to move up to... */
        if ( gtk_tree_path_prev(path) ) {
            GtkTreeIter prev_iter;
            if ( gtk_tree_model_get_iter(model, &prev_iter, path) ) {
                gtk_list_store_move_before(GTK_LIST_STORE(model), &iter, &prev_iter );
            }
        }
        gtk_tree_path_free(path);
    }

    /* Update the state of buttons */
    /* selection_changed(gtk_tree_view_get_selection(prv->wifi_fav_tv), (gpointer)self); */
    update_buttons_status(self);
}

static void
on_down_btn_clicked(GtkButton *button, gpointer user_data)
{
	NwamTreeView *self = NWAM_TREE_VIEW(user_data);
    NwamTreeViewPrivate *prv = NWAM_TREE_VIEW_PRIVATE(self);
    GtkTreeModel*               model;
    GtkTreeIter                 iter;
    GtkTreeSelection*           selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(self));

    if ( gtk_tree_selection_get_selected( selection, &model, &iter ) ) {
        GtkTreeIter *next_iter = gtk_tree_iter_copy(&iter);
        
        if ( gtk_tree_model_iter_next(GTK_TREE_MODEL(model), next_iter) ) { /* See if we have somewhere to move down to... */
            gtk_list_store_move_after(GTK_LIST_STORE(model), &iter, next_iter );
        }
        
        gtk_tree_iter_free(next_iter);
    }

    /* Update the state of buttons */
    /* selection_changed(gtk_tree_view_get_selection(prv->wifi_fav_tv), (gpointer)self); */
    update_buttons_status(self);
}

static gboolean
nwam_tree_view_key_press(GtkWidget   *widget, GdkEventKey *event)
{
    NwamTreeViewPrivate *prv = NWAM_TREE_VIEW_PRIVATE(widget);
	NwamTreeView *self = NWAM_TREE_VIEW(widget);

    if (GTK_WIDGET_CLASS(nwam_tree_view_parent_class)->key_press_event(widget, event)) {
        return TRUE;
    } else if (event->keyval == GDK_Escape) {
        g_debug("TODO Escape cancel all cell");

/*         if (prv->new_obj) { */

/*             prv->remove_object_func(self, prv->new_obj, prv->object_func_user_data); */
/*             prv->new_obj = NULL; */

/*         } */
        
/*         if (prv->widget_list[PROP_BUTTON_ADD]) */
/*             gtk_widget_grab_focus(GTK_WIDGET(prv->widget_list[PROP_BUTTON_ADD])); */
        
        return TRUE;
    } else if (event->keyval == GDK_Return || event->keyval == GDK_Tab) {
/*         if (prv->new_obj) { */
/*             if (prv->commit_object_func(self, prv->new_obj, prv->object_func_user_data)) { */

/*                 prv->new_obj = NULL; */

/*                 gtk_widget_set_sensitive(prv->widget_list[PROP_BUTTON_ADD], TRUE); */
/*             } */
/*         } */

    }
    return FALSE;
}

static gboolean
nwam_tree_view_key_release(GtkWidget   *widget, GdkEventKey *event)
{
    NwamTreeViewPrivate *prv = NWAM_TREE_VIEW_PRIVATE(widget);
	NwamTreeView *self = NWAM_TREE_VIEW(widget);

#if 0
    if (GTK_WIDGET_CLASS(nwam_tree_view_parent_class)->key_release_event(widget, event)) {
        return TRUE;
    } else if (event->keyval == GDK_Escape) {
        g_debug("TODO Escape cancel all cell");

/*         if (prv->new_obj) { */
/*             prv->remove_object_func(self, prv->new_obj, prv->object_func_user_data); */
/*             prv->new_obj = NULL; */
/*         } */
        g_assert(prv->widget_list[PROP_BUTTON_ADD]);
        gtk_widget_grab_focus(GTK_WIDGET(prv->widget_list[PROP_BUTTON_ADD]));
        
        return TRUE;
    }
    return FALSE;
#endif
    return GTK_WIDGET_CLASS(nwam_tree_view_parent_class)->key_release_event(widget, event);
}

static void
default_signal_handler_activate_widget(NwamTreeView *self, GtkWidget *widget, gpointer user_data)
{
}

static void
nwam_tree_view_row_activated_cb(GtkTreeView *tree_view,
  GtkTreePath *path,
  GtkTreeViewColumn *column,
  gpointer user_data)
{
}

static void
nwam_tree_view_row_selected_cb(GtkTreeView *tree_view, gpointer user_data)
{
    NwamTreeViewPrivate *prv = NWAM_TREE_VIEW_PRIVATE(tree_view);
	NwamTreeView *self = NWAM_TREE_VIEW(tree_view);

    update_buttons_status(self);
}

static void
nwam_tree_view_selection_changed_cb(GtkTreeSelection *selection, gpointer user_data)
{
    NwamTreeViewPrivate *prv = NWAM_TREE_VIEW_PRIVATE(user_data);

    prv->count_selected_rows = gtk_tree_selection_count_selected_rows(selection);
    g_list_foreach(prv->widgets, nwam_tree_view_update_widget_cb, user_data);
}

static void
nwam_tree_view_update_widget_cb(gpointer data, gpointer user_data)
{
	NwamTreeView *self = NWAM_TREE_VIEW(user_data);
}

static void
object_tree_model_notify(GObject *gobject, GParamSpec *arg1, gpointer user_data)
{
	NwamTreeView *self = NWAM_TREE_VIEW(gobject);
    NwamTreeViewPrivate *prv = NWAM_TREE_VIEW_PRIVATE(gobject);
    GtkTreeModel *new_model = gtk_tree_view_get_model(GTK_TREE_VIEW(self));

    /* We do not enable cache function if there is not add button */
    if (!prv->widget_list[NTV_ADD_BTN])
        return;

    if (prv->model != new_model) {

        /* Model changed, so free new obj ref */
        if (prv->new_obj_ref) {
            gtk_tree_row_reference_free(prv->new_obj_ref);
            prv->new_obj_ref = NULL;
        }

        if (prv->model) {
            disconnect_model_signals(G_OBJECT(self), prv->model);
        }

        /* Record the new model */
        if ((prv->model = new_model) != NULL) {
            connect_model_signals(G_OBJECT(self), prv->model);
        }
    }
}

static void
nwam_tree_view_row_inserted(GtkTreeModel *tree_model,
  GtkTreePath  *path,
  GtkTreeIter  *iter,
  gpointer      user_data)
{
	NwamTreeView *self = NWAM_TREE_VIEW(user_data);
    NwamTreeViewPrivate *prv = NWAM_TREE_VIEW_PRIVATE(user_data);
    NwamuiObject *object;

    if (prv->new_obj_ref) {
        if (gtk_tree_row_reference_valid(prv->new_obj_ref)) {
            GtkTreePath *oldpath = gtk_tree_row_reference_get_path(prv->new_obj_ref);
            if (gtk_tree_path_compare(path, oldpath) == 0) {
                return;
            }
        }
        gtk_tree_row_reference_free(prv->new_obj_ref);
    }
        
    /* Append a new row will cause this, we disable add button here. */
    gtk_widget_set_sensitive(prv->widget_list[NTV_ADD_BTN], FALSE);
    prv->new_obj_ref = gtk_tree_row_reference_new(tree_model, path);
    g_assert(prv->new_obj_ref);
}

static void
nwam_tree_view_row_changed(GtkTreeModel *tree_model,
  GtkTreePath  *path,
  GtkTreeIter  *iter,
  gpointer      user_data)
{
	NwamTreeView *self = NWAM_TREE_VIEW(user_data);
    NwamTreeViewPrivate *prv = NWAM_TREE_VIEW_PRIVATE(user_data);
    GtkTreePath  *ref_path;
    NwamuiObject *object;

    if (!prv->new_obj_ref)
        return;

    ref_path = gtk_tree_row_reference_get_path(prv->new_obj_ref);

    if (ref_path != NULL && gtk_tree_path_compare(ref_path, path) == 0) {
        gtk_tree_model_get(tree_model, iter, 0, &object, -1);
        if (object) {
            /* Real object is inserted, we enable add button here */
            gtk_widget_set_sensitive(prv->widget_list[NTV_ADD_BTN], TRUE);

            g_object_unref(object);
/*         } else { */
/*             /\* Append a new row will cause this, we disable add button here. *\/ */
/*             gtk_widget_set_sensitive(prv->widget_list[NTV_ADD_BTN], FALSE); */

/*             g_assert(!prv->new_obj_ref); */
/*             prv->new_obj_ref = gtk_tree_row_reference_new(tree_model, path); */
/*             g_assert(prv->new_obj_ref); */
        }
    }
}

static void
nwam_tree_view_row_deleted(GtkTreeModel *tree_model,
  GtkTreePath *path,
  gpointer user_data)
{
	NwamTreeView *self = NWAM_TREE_VIEW(user_data);
    NwamTreeViewPrivate *prv = NWAM_TREE_VIEW_PRIVATE(user_data);

}

GtkTreePath*
nwam_tree_view_get_cached_object_path(NwamTreeView *self)
{
    NwamTreeViewPrivate *prv = NWAM_TREE_VIEW_PRIVATE(self);
    if (prv->new_obj_ref && gtk_tree_row_reference_valid(prv->new_obj_ref))
        return gtk_tree_row_reference_get_path(prv->new_obj_ref);
    return NULL;
}

NwamuiObject*
nwam_tree_view_get_cached_object(NwamTreeView *self)
{
    NwamTreeViewPrivate *prv = NWAM_TREE_VIEW_PRIVATE(self);
    if (prv->new_obj_ref && gtk_tree_row_reference_valid(prv->new_obj_ref)) {
        GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(self));
        GtkTreePath *path = gtk_tree_row_reference_get_path(prv->new_obj_ref);
        GtkTreeIter iter;
        NwamuiObject *object;

        g_assert(model == (gpointer)gtk_tree_row_reference_get_path(prv->new_obj_ref));
        if (gtk_tree_model_get_iter(model, &iter, path)) {
            gtk_tree_model_get(model, &iter, 0, &object, -1);
            return object;
        }
    }
    return NULL;
}

static void
connect_model_signals(GObject *self, GtkTreeModel *model)
{
    g_signal_connect(model, "row-inserted",
      G_CALLBACK(nwam_tree_view_row_inserted),
      self);
    g_signal_connect(model, "row-changed",
      G_CALLBACK(nwam_tree_view_row_changed),
      self);
    g_signal_connect(model, "row-deleted",
      G_CALLBACK(nwam_tree_view_row_deleted),
      self);
}

static void
disconnect_model_signals(GObject *self, GtkTreeModel *model)
{
    g_signal_handlers_disconnect_by_func(model,
      (gpointer)nwam_tree_view_row_inserted,
      (gpointer)self);
    g_signal_handlers_disconnect_by_func(model,
      (gpointer)nwam_tree_view_row_changed,
      (gpointer)self);
    g_signal_handlers_disconnect_by_func(model,
      (gpointer)nwam_tree_view_row_deleted,
      (gpointer)self);
}

static void
update_buttons_status(NwamTreeView *self)
{
    NwamTreeViewPrivate *prv       = NWAM_TREE_VIEW_PRIVATE(self);
    GtkTreeSelection    *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(self));
    GtkTreeModel        *model;
    GtkTreeIter          iter;
    /* gint                 count_selected_rows; */

    /* count_selected_rows = gtk_tree_selection_count_selected_rows(selection); */

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        GtkTreePath *path;

        if (prv->widget_list[NTV_UP_BTN] && prv->widget_list[NTV_DOWN_BTN]) {
            path = gtk_tree_model_get_path(model, &iter);

            if (gtk_tree_path_prev(path)) {
                gtk_widget_set_sensitive(GTK_WIDGET(prv->widget_list[NTV_UP_BTN]), TRUE);
            } else {
                gtk_widget_set_sensitive(GTK_WIDGET(prv->widget_list[NTV_UP_BTN]), FALSE);
            }
            gtk_tree_path_free(path);

            if (gtk_tree_model_iter_next(model, &iter)) {
                gtk_widget_set_sensitive(GTK_WIDGET(prv->widget_list[NTV_DOWN_BTN]), TRUE);
            } else {
                gtk_widget_set_sensitive(GTK_WIDGET(prv->widget_list[NTV_DOWN_BTN]), FALSE);
            }
        }

        if (prv->widget_list[NTV_REMOVE_BTN]) {
            gtk_widget_set_sensitive(GTK_WIDGET(prv->widget_list[NTV_REMOVE_BTN]), TRUE);
        }
        if (prv->widget_list[NTV_EDIT_BTN]) {
            gtk_widget_set_sensitive(GTK_WIDGET(prv->widget_list[NTV_EDIT_BTN]), TRUE);
        }
    } else {
        if (prv->widget_list[NTV_UP_BTN] && prv->widget_list[NTV_DOWN_BTN]) {
            gtk_widget_set_sensitive(GTK_WIDGET(prv->widget_list[NTV_UP_BTN]), FALSE);
            gtk_widget_set_sensitive(GTK_WIDGET(prv->widget_list[NTV_DOWN_BTN]), FALSE);
        }
        if (prv->widget_list[NTV_REMOVE_BTN]) {
            gtk_widget_set_sensitive(GTK_WIDGET(prv->widget_list[NTV_REMOVE_BTN]), FALSE);
        }
        if (prv->widget_list[NTV_EDIT_BTN]) {
            gtk_widget_set_sensitive(GTK_WIDGET(prv->widget_list[NTV_EDIT_BTN]), FALSE);
        }
    }
}
