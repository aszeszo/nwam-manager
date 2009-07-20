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
#include "nwam_tree_view.h"

#define DEBUG()	g_debug ("[[ %20s : %-4d ]]", __func__, __LINE__)

enum {
    PROP_BUTTON_BEGIN = 0,
    PROP_BUTTON_ADD,
    PROP_BUTTON_REMOVE,
    PROP_BUTTON_EDIT,
    PROP_BUTTON_RENAME,
    PROP_BUTTON_UP,
    PROP_BUTTON_DOWN,
    PROP_BUTTON_END,
};

enum {
    PLACE_HOLDER,
    LAST_SIGNAL
};

static guint nwam_tree_view_signals[LAST_SIGNAL] = {0};

typedef struct _NwamTreeViewPrivate NwamTreeViewPrivate;
struct _NwamTreeViewPrivate {
    GtkTreeModel *model;
    GtkTreeRowReference *new_obj_ref;
    gint count_selected_rows;
    GtkTreePath *selected_path;
    GtkWidget *widget_list[PROP_BUTTON_END];
    GList *widgets;
    NwamTreeViewAddObjectFunc add_object_func;
    NwamTreeViewRemoveObjectFunc remove_object_func;
    NwamTreeViewCommitObjectFunc commit_object_func;
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
static void on_button_clicked(GtkButton *button, gpointer user_data);
static gboolean nwam_tree_view_key_press(GtkWidget   *widget, GdkEventKey *event);
static gboolean nwam_tree_view_key_release(GtkWidget   *widget, GdkEventKey *event);
static void default_set_object_func(NwamTreeView *self,
  NwamTreeViewAddObjectFunc add_object_func,
  NwamTreeViewRemoveObjectFunc remove_object_func,
  NwamTreeViewCommitObjectFunc commit_object_func,
  gpointer user_data);
static void default_signal_handler_activate_widget(NwamTreeView *self, GtkWidget *widget, gpointer user_data);
static void delete_selection_cb(GtkTreeModel *model,
  GtkTreePath *path,
  GtkTreeIter *iter,
  gpointer data);
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

G_DEFINE_TYPE(NwamTreeView, nwam_tree_view, GTK_TYPE_TREE_VIEW)

static const gchar*
nwam_tree_view_property_name(gint prop_id)
{
	switch (prop_id) {
    case PROP_BUTTON_ADD:
        return "button_add";
    case PROP_BUTTON_REMOVE:
        return "button_remove";
    case PROP_BUTTON_EDIT:
        return "button_edit";
    case PROP_BUTTON_RENAME:
        return "button_rename";
    case PROP_BUTTON_UP:
        return "button_up";
    case PROP_BUTTON_DOWN:
        return "button_down";
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

    /* Only enable ADD button */
    g_object_class_install_property (gobject_class,
      PROP_BUTTON_ADD,
      g_param_spec_object(nwam_tree_view_property_name(PROP_BUTTON_ADD),
        _("buttons of tree view"),
        _("buttons of tree view"),
        GTK_TYPE_BUTTON,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT/*_ONLY*/));

/*     { */
/*         int propid; */
/*         for (propid = PROP_BUTTON_BEGIN + 1; propid < PROP_BUTTON_END; propid ++) { */
/*             g_object_class_install_property (gobject_class, */
/*               propid, */
/*               g_param_spec_object(nwam_tree_view_property_name(propid), */
/*                 _("buttons of tree view"), */
/*                 _("buttons of tree view"), */
/*                 GTK_TYPE_BUTTON, */
/*                 G_PARAM_READWRITE | G_PARAM_CONSTRUCT/\*_ONLY*\/)); */
/*         } */
/*     } */
    

    klass->set_object_func = default_set_object_func;
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
    int propid;

    g_list_foreach(prv->widgets, G_CALLBACK(g_object_unref), NULL);

    for (propid = PROP_BUTTON_BEGIN + 1; propid < PROP_BUTTON_END; propid ++) {
        if (prv->widget_list[propid]) {
            g_object_unref(prv->widget_list[propid]);
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
    case PROP_BUTTON_ADD:
        /* TODO, start the cache function. Or we disable it if add button is
         * removed */
        if (prv->model) {
            connect_model_signals(G_OBJECT(self), prv->model);
        }

    case PROP_BUTTON_REMOVE:
    case PROP_BUTTON_EDIT:
    case PROP_BUTTON_RENAME:
    case PROP_BUTTON_UP:
    case PROP_BUTTON_DOWN:
/*         if (prv->widget_list[propid]) */
/*             g_object_unref(prv->widget_list[propid]); */
        g_assert(prv->widget_list[prop_id] == NULL);
        prv->widget_list[prop_id] = GTK_WIDGET(g_value_get_object(value));
        if (prv->widget_list[prop_id])
            g_signal_connect(prv->widget_list[prop_id], "clicked",
              G_CALLBACK(on_button_clicked), (gpointer)self);
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

	switch (prop_id)
	{
    case PROP_BUTTON_ADD:
    case PROP_BUTTON_REMOVE:
    case PROP_BUTTON_EDIT:
    case PROP_BUTTON_RENAME:
    case PROP_BUTTON_UP:
    case PROP_BUTTON_DOWN:
        g_value_set_object(value, prv->widget_list[prop_id]);
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
nwam_tree_view_add_widget(NwamTreeView *self, GtkWidget *w)
{
    NwamTreeViewPrivate *prv = NWAM_TREE_VIEW_PRIVATE(self);

    g_return_if_fail(prv->widget_list[PROP_BUTTON_ADD] != (gpointer)w);
    g_return_if_fail(g_list_find(prv->widgets, w) == NULL);

    prv->widgets = g_list_prepend(prv->widgets, g_object_ref(w));

    g_signal_connect(w, "clicked",
      G_CALLBACK(on_button_clicked), (gpointer)self);
}

void
nwam_tree_view_remove_widget(NwamTreeView *self, GtkWidget *w)
{
    NwamTreeViewPrivate *prv = NWAM_TREE_VIEW_PRIVATE(self);

    g_return_if_fail(prv->widget_list[PROP_BUTTON_ADD] != (gpointer)w);
    g_return_if_fail(g_list_find(prv->widgets, w));

    prv->widgets = g_list_remove(prv->widgets, g_object_ref(w));
    g_object_unref(w);
}

void
nwam_tree_view_set_object_func(NwamTreeView *self,
  NwamTreeViewAddObjectFunc add_object_func,
  NwamTreeViewRemoveObjectFunc remove_object_func,
  NwamTreeViewCommitObjectFunc commit_object_func,
  gpointer user_data)
{
    NWAM_TREE_VIEW_GET_CLASS(self)->set_object_func(self,
      add_object_func,
      remove_object_func,
      commit_object_func,
      user_data);
}

void
nwam_tree_view_add(NwamTreeView *self)
{
    NwamTreeViewPrivate *prv = NWAM_TREE_VIEW_PRIVATE(self);
}

void
nwam_tree_view_remove(NwamTreeView *self,
    GtkTreeSelectionForeachFunc func,
    gpointer user_data)
{
    NwamTreeViewPrivate *prv = NWAM_TREE_VIEW_PRIVATE(self);
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(self));

    gtk_tree_selection_selected_foreach(sel, func, user_data);
    g_object_unref(sel);
}

void
nwam_tree_view_up(NwamTreeView *self, GObject *item, gboolean top)
{
}

void
nwam_tree_view_down(NwamTreeView *self, GObject *item)
{
}

GObject*
nwam_tree_view_edit(NwamTreeView *self, const gchar *name)
{
}

GObject*
nwam_tree_view_get_item_by_proxy(NwamTreeView *self, GObject *proxy)
{
}

static void
on_button_clicked(GtkButton *button, gpointer user_data)
{
    NwamTreeViewPrivate *prv = NWAM_TREE_VIEW_PRIVATE(user_data);
	NwamTreeView *self = NWAM_TREE_VIEW(user_data);
    int prop_id;

    for (prop_id = PROP_BUTTON_BEGIN + 1; prop_id < PROP_BUTTON_END; prop_id ++) {
        if (prv->widget_list[prop_id] == (gpointer)button) {
            break;
        }
    }

	switch (prop_id)
	{
    case PROP_BUTTON_ADD:
    case PROP_BUTTON_REMOVE:
    case PROP_BUTTON_EDIT:
    case PROP_BUTTON_RENAME:
    case PROP_BUTTON_UP:
    case PROP_BUTTON_DOWN:
	default:
/*         g_assert_not_reached(); */
        break;
    }
}

static gboolean
nwam_tree_view_key_press(GtkWidget   *widget, GdkEventKey *event)
{
    NwamTreeViewPrivate *prv = NWAM_TREE_VIEW_PRIVATE(widget);
	NwamTreeView *self = NWAM_TREE_VIEW(widget);

    DEBUG();
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

    DEBUG();
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
default_set_object_func(NwamTreeView *self,
  NwamTreeViewAddObjectFunc add_object_func,
  NwamTreeViewRemoveObjectFunc remove_object_func,
  NwamTreeViewCommitObjectFunc commit_object_func,
  gpointer user_data)
{
    NwamTreeViewPrivate *prv = NWAM_TREE_VIEW_PRIVATE(self);

    prv->add_object_func = add_object_func;
    prv->remove_object_func = remove_object_func;
    prv->commit_object_func = commit_object_func;
    prv->object_func_user_data = user_data;
}

static void
default_signal_handler_activate_widget(NwamTreeView *self, GtkWidget *widget, gpointer user_data)
{
}

static void
delete_selection_cb(GtkTreeModel *model,
  GtkTreePath *path,
  GtkTreeIter *iter,
  gpointer data)
{
    
}

static void
nwam_tree_view_row_activated_cb(GtkTreeView *tree_view,
  GtkTreePath *path,
  GtkTreeViewColumn *column,
  gpointer user_data)
{
    DEBUG();
}

static void
nwam_tree_view_row_selected_cb(GtkTreeView *tree_view, gpointer user_data)
{
    NwamTreeViewPrivate *prv = NWAM_TREE_VIEW_PRIVATE(tree_view);
	NwamTreeView *self = NWAM_TREE_VIEW(tree_view);
#if 0
    GtkTreePath*        path = NULL;
    GtkTreeViewColumn*  focus_column = NULL;;
    GtkTreeIter         iter;
    GtkTreeModel*       model = gtk_tree_view_get_model(tree_view);

    gtk_tree_view_get_cursor ( tree_view, &path, &focus_column );

    if (path != NULL && gtk_tree_model_get_iter (model, &iter, path))
    {
        gpointer    connection;
        NwamuiNcu*  ncu;

        gtk_tree_model_get(model, &iter, 0, &connection, -1);

        ncu  = NWAMUI_NCU( connection );
        
        if (self->prv->selected_ncu != ncu) {
            g_object_set (self, "selected_ncu", ncu, NULL);
        }
    }
    gtk_tree_path_free (path);
#endif
}

static void
nwam_tree_view_selection_changed_cb(GtkTreeSelection *selection, gpointer user_data)
{
    NwamTreeViewPrivate *prv = NWAM_TREE_VIEW_PRIVATE(user_data);

    prv->count_selected_rows = gtk_tree_selection_count_selected_rows(selection);
    g_list_foreach(prv->widgets, nwam_tree_view_update_widget_cb, user_data);
}

static void
nwam_tree_view_update_widget_cb (gpointer data, gpointer user_data)
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
    if (!prv->widget_list[PROP_BUTTON_ADD])
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
    gtk_widget_set_sensitive(prv->widget_list[PROP_BUTTON_ADD], FALSE);
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
            gtk_widget_set_sensitive(prv->widget_list[PROP_BUTTON_ADD], TRUE);

            g_object_unref(object);
/*         } else { */
/*             /\* Append a new row will cause this, we disable add button here. *\/ */
/*             gtk_widget_set_sensitive(prv->widget_list[PROP_BUTTON_ADD], FALSE); */

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
      nwam_tree_view_row_inserted,
      self);
    g_signal_handlers_disconnect_by_func(model,
      nwam_tree_view_row_changed,
      self);
    g_signal_handlers_disconnect_by_func(model,
      nwam_tree_view_row_deleted,
      self);
}
