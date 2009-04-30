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

#define DEF_CUSTOM_TREEVIEW_TOOLTIP 0

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n.h>
#include <strings.h>
#include <stdlib.h>

#include "libnwamui.h"
#include "status_icon_tooltip.h"

#include "nwam-menuitem.h"
#include "nwam-wifi-item.h"
#include "nwam-enm-item.h"
#include "nwam-env-item.h"
#include "nwam-ncu-item.h"

#define TOOLTIP_WIDGET_DATA "sitw_label"

typedef struct _NwamTooltipWidgetPrivate	NwamTooltipWidgetPrivate;
#define NWAM_TOOLTIP_WIDGET_GET_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), NWAM_TYPE_TOOLTIP_WIDGET, NwamTooltipWidgetPrivate))

struct _NwamTooltipWidgetPrivate {
#if DEF_CUSTOM_TREEVIEW_TOOLTIP
    GtkTreeView *tooltip_treeview;
#else
    GtkWidget *env_widget;
    NwamuiObject *env;
    GtkWidget *ncp_widget;
    NwamuiObject *ncp;
#endif
};


static void nwam_tooltip_widget_finalize (NwamTooltipWidget *self);

#if DEF_CUSTOM_TREEVIEW_TOOLTIP
static void nwam_compose_tree_view(NwamTooltipWidget *self);
#endif

/* call back */
static void tooltip_treeview_cell (GtkTreeViewColumn *col,
  GtkCellRenderer   *renderer,
  GtkTreeModel      *model,
  GtkTreeIter       *iter,
  gpointer           data);

/* nwamui object signals */
static void env_notify_tooltip(GObject *gobject, GParamSpec *arg1, gpointer user_data);
static void ncp_notify_tooltip(GObject *gobject, GParamSpec *arg1, gpointer user_data);
static void ncu_notify_tooltip(GObject *gobject, GParamSpec *arg1, gpointer user_data);

G_DEFINE_TYPE(NwamTooltipWidget, nwam_tooltip_widget, GTK_TYPE_VBOX)

static void
nwam_tooltip_widget_class_init (NwamTooltipWidgetClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass*) klass;
        
    gobject_class->finalize = (void (*)(GObject*)) nwam_tooltip_widget_finalize;

    g_type_class_add_private (klass, sizeof (NwamTooltipWidgetPrivate));
}
        
static void
nwam_tooltip_widget_init (NwamTooltipWidget *self)
{
	NwamTooltipWidgetPrivate *prv = NWAM_TOOLTIP_WIDGET_GET_PRIVATE(self);
#if DEF_CUSTOM_TREEVIEW_TOOLTIP
    GtkTreeModel *model = GTK_TREE_MODEL(gtk_list_store_new(1, NWAMUI_TYPE_OBJECT));
    GtkTreeIter iter;

    prv->tooltip_treeview = gtk_tree_view_new_with_model(model);
    /* Tooltip: env */
    gtk_list_store_prepend(GTK_LIST_STORE(model), &iter);
    /* Tooltip: profile */
    gtk_list_store_prepend(GTK_LIST_STORE(model), &iter);
    g_object_unref(model);
    nwam_compose_tree_view(self);

    gtk_box_pack_start(GTK_BOX(self), GTK_WIDGET(prv->tooltip_treeview),
      TRUE, TRUE, 1);
/*         GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL); */
/*         gtk_window_set_decorated(GTK_WINDOW(win), TRUE); */
/*         gtk_window_set_screen(GTK_WINDOW(win), gtk_status_icon_get_screen(status_icon)); */
/*         gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_MOUSE); */
/*         gtk_widget_set_parent_window(GTK_WIDGET(prv->tooltip_treeview), */
/*           gtk_widget_get_tooltip_window(GTK_WIDGET(user_data))); */
/*         GtkWidget *p = gtk_widget_get_parent(GTK_WIDGET(prv->tooltip_treeview)); */
/*         gtk_widget_set_parent_window(GTK_WIDGET(prv->tooltip_treeview), */
/*           gtk_widget_get_root_window(p)); */
/*         gtk_widget_show(GTK_WIDGET(prv->tooltip_treeview)); */

#else

    prv->env_widget = nwam_menu_item_new();
    gtk_widget_show(prv->env_widget);
    gtk_box_pack_start(GTK_BOX(self), prv->env_widget, TRUE, TRUE, 1);

    prv->ncp_widget = nwam_menu_item_new();
    gtk_widget_show(prv->ncp_widget);
    gtk_box_pack_start(GTK_BOX(self), prv->ncp_widget, TRUE, TRUE, 1);

#endif
/*     gtk_tooltip_widget_set_visible(GTK_TOOLTIP_WIDGET(self), FALSE); */
/*     gtk_widget_show_all(GTK_WIDGET(self)); */
}

static void
nwam_tooltip_widget_finalize (NwamTooltipWidget *self)
{
	NwamTooltipWidgetPrivate *prv = NWAM_TOOLTIP_WIDGET_GET_PRIVATE(self);

    g_debug("%s", __func__);
    g_assert_not_reached();
	G_OBJECT_CLASS(nwam_tooltip_widget_parent_class)->finalize(G_OBJECT(self));
}

#if DEF_CUSTOM_TREEVIEW_TOOLTIP
static void
tooltip_treeview_cell (GtkTreeViewColumn *col,
  GtkCellRenderer   *renderer,
  GtkTreeModel      *model,
  GtkTreeIter       *iter,
  gpointer           data)
{
    gint          cell_num = (gint)data; /* Number of cell in column */
    NwamuiObject *object;
    GType         type;
    GtkWidget    *item;
	GString      *gstr;
    gchar        *name;

	gtk_tree_model_get(model, iter, 0, &object, -1);

    if (!object)
        return;

    gstr = g_string_new("");
    type = G_OBJECT_TYPE(object);

    name = nwamui_object_get_name(object);

	if (type == NWAMUI_TYPE_NCU) {
        if (cell_num == 0) {
        } else {
            gchar *state = nwamui_ncu_get_connection_state_detail_string(NWAMUI_NCU(object));
            switch (nwamui_ncu_get_ncu_type(NWAMUI_NCU(object))) {
            case NWAMUI_NCU_TYPE_WIRELESS:
                g_string_append_printf(gstr, _("<b>Wireless (%s):</b> %s"),
                  name, state);
                break;
            case NWAMUI_NCU_TYPE_WIRED:
            case NWAMUI_NCU_TYPE_TUNNEL:
                g_string_append_printf(gstr, _("<b>Wired (%s):</b> %s"),
                  name, state);
                break;
            default:
                g_assert_not_reached ();
            }
            g_free(state);
            g_object_set(renderer, "markup", gstr->str, NULL);
        }
	} else if (type == NWAMUI_TYPE_ENV) {
        if (cell_num == 0) {
        } else {
            g_string_append_printf(gstr, _("<b>Location:</b> %s"), name);
            g_object_set(renderer, "markup", gstr->str, NULL);
        }
	} else if (type == NWAMUI_TYPE_NCP) {
        if (cell_num == 0) {
        } else {
            g_string_append_printf(gstr, _("<b>Network Profile:</b> %s"), name);
            g_object_set(renderer, "markup", gstr->str, NULL);
        }
/* 	} else if (type == NWAMUI_TYPE_WIFI_NET) { */
/* 	} else if (type == NWAMUI_TYPE_ENM) { */
/* 	} else { */
	}
    g_string_free(gstr, TRUE);
    g_free(name);
    g_object_unref(object);
}

static void
nwam_compose_tree_view(NwamTooltipWidget *self)
{
    NwamTooltipWidgetPrivate *prv = NWAM_TOOLTIP_WIDGET_GET_PRIVATE(self);
	GtkTreeViewColumn *col;
	GtkCellRenderer *cell;
	GtkTreeView *view = prv->tooltip_treeview;

	g_object_set (view,
      "headers-clickable", FALSE,
      "headers-visible", FALSE,
      "reorderable", FALSE,
      "enable-search", FALSE,
      NULL);

    {
        col = gtk_tree_view_column_new();
        gtk_tree_view_append_column (view, col);

        g_object_set(col,
          "title", _("Connection Icon"),
          "resizable", TRUE,
          "clickable", TRUE,
          "sort-indicator", TRUE,
          "reorderable", TRUE,
          NULL);

        /* Add 1st "type" icon cell */
        cell = gtk_cell_renderer_pixbuf_new();
        gtk_tree_view_column_pack_start(col, cell, FALSE);
        gtk_tree_view_column_set_cell_data_func (col,
          cell,
          tooltip_treeview_cell,
          (gpointer) 0,
          NULL);
        /* Add 2nd object name and state */
        cell = gtk_cell_renderer_text_new();
        gtk_tree_view_column_pack_end(col, cell, TRUE);
        gtk_tree_view_column_set_cell_data_func (col,
          cell,
          tooltip_treeview_cell,
          (gpointer) 1, /* Important to know this is wireless cell in 1st col */
          NULL);
    } /* column icon */
}
#endif

void
nwam_tooltip_widget_update_env(NwamTooltipWidget *self, NwamuiObject *object)
{
    NwamTooltipWidgetPrivate *prv = NWAM_TOOLTIP_WIDGET_GET_PRIVATE(self);

    g_assert(NWAMUI_IS_ENV(object));

#if DEF_CUSTOM_TREEVIEW_TOOLTIP
    GtkTreeModel *model = gtk_tree_view_get_model(prv->tooltip_treeview);
    GtkTreeIter iter;

    gtk_tree_model_iter_nth_child(model, &iter, NULL, 0);
    gtk_list_store_set(GTK_LIST_STORE(model), &iter, 0, object, -1);
#else
    if (prv->env) {
        g_signal_handlers_disconnect_matched(prv->env,
          G_SIGNAL_MATCH_DATA,
          0,
          NULL,
          NULL,
          NULL,
          (gpointer)prv->env_widget);
/*         gtk_label_set_markup(GTK_LABEL(prv->env_widget), ""); */
        menu_item_set_markup(GTK_MENU_ITEM(prv->env_widget), "");
        prv->env = NULL;
    }

    if (object) {
        env_notify_tooltip(G_OBJECT(object), NULL, (gpointer)prv->env_widget);
        g_signal_connect(object, "notify",
          G_CALLBACK(env_notify_tooltip), (gpointer)prv->env_widget);
        prv->env = object;
    }
#endif
}

void
nwam_tooltip_widget_update_ncp(NwamTooltipWidget *self, NwamuiObject *object)
{
    NwamTooltipWidgetPrivate *prv = NWAM_TOOLTIP_WIDGET_GET_PRIVATE(self);
    GList *ncu_list = nwamui_ncp_get_ncu_list(NWAMUI_NCP(object));
    GList *idx;
    gint ncu_num;

    g_assert(NWAMUI_IS_NCP(object));

#if DEF_CUSTOM_TREEVIEW_TOOLTIP
    GtkTreeModel *model = gtk_tree_view_get_model(prv->tooltip_treeview);
    GtkTreeIter iter;

    gtk_tree_model_iter_nth_child(model, &iter, NULL, 1);
    gtk_list_store_set(GTK_LIST_STORE(model), &iter, 0, object, -1);

    ncu_num = gtk_tree_model_iter_n_children(model, NULL) - 2;

    for (; ncu_list && ncu_num > 0; ncu_num --) {
        gtk_tree_model_iter_next(model, &iter);
        gtk_list_store_set(GTK_LIST_STORE(model), &iter, 0, ncu_list->data, -1);
/*         g_signal_connect(ncu_list->data, "notify::ncu-list-store", */
/*           G_CALLBACK(ncu_notify_tooltip), (gpointer)self); */
        ncu_list = g_list_delete_link(ncu_list, ncu_list);
    }
    for (; ncu_list;) {
        gtk_list_store_append(GTK_LIST_STORE(model), &iter);
        gtk_list_store_set(GTK_LIST_STORE(model), &iter, 0, ncu_list->data, -1);
        ncu_list = g_list_delete_link(ncu_list, ncu_list);
    }
    if (gtk_tree_model_iter_next(model, &iter)) {
        while (gtk_list_store_remove(GTK_LIST_STORE(model), &iter));
    }
#else
    GList *w_list;

    if (prv->ncp) {
        g_signal_handlers_disconnect_matched(prv->ncp,
          G_SIGNAL_MATCH_DATA,
          0,
          NULL,
          NULL,
          NULL,
          (gpointer)prv->ncp_widget);
        menu_item_set_markup(GTK_MENU_ITEM(prv->ncp_widget), "");
        prv->ncp = NULL;
    }

    if (object) {
        ncp_notify_tooltip(G_OBJECT(object), NULL, (gpointer)prv->ncp_widget);
        g_signal_connect(object, "notify",
          G_CALLBACK(ncp_notify_tooltip), (gpointer)prv->ncp_widget);
        prv->ncp = object;
    }

    w_list = gtk_container_get_children(GTK_CONTAINER(self));
    g_assert(w_list);
    w_list = g_list_remove(w_list, prv->env_widget);
    g_assert(w_list);
    w_list = g_list_remove(w_list, prv->ncp_widget);
    ncu_num = g_list_length(w_list);

    for (; ncu_list && w_list; ) {
        NwamuiObject *object = (NwamuiObject*)g_object_get_data(G_OBJECT(w_list->data), TOOLTIP_WIDGET_DATA);
        
        g_signal_handlers_disconnect_matched(object,
          G_SIGNAL_MATCH_DATA,
          0,
          NULL,
          NULL,
          NULL,
          w_list->data);

        g_object_set_data(G_OBJECT(ncu_list->data), TOOLTIP_WIDGET_DATA, w_list->data);
        ncu_notify_tooltip(G_OBJECT(ncu_list->data), NULL, (gpointer)w_list->data);
        g_signal_connect(NWAMUI_NCU(ncu_list->data), "notify",
          G_CALLBACK(ncu_notify_tooltip), w_list->data);

        ncu_list = g_list_delete_link(ncu_list, ncu_list);
        w_list = g_list_delete_link(w_list, w_list);
    }
    for (; ncu_list;) {
        nwam_tooltip_widget_add_ncu(self, NWAMUI_OBJECT(ncu_list->data));
        ncu_list = g_list_delete_link(ncu_list, ncu_list);
    }
    for (; w_list; ) {
        NwamuiObject *object = (NwamuiObject*)g_object_get_data(G_OBJECT(w_list->data), TOOLTIP_WIDGET_DATA);
        
        g_signal_handlers_disconnect_matched(object,
          G_SIGNAL_MATCH_DATA,
          0,
          NULL,
          NULL,
          NULL,
          w_list->data);

        g_assert(w_list->data != prv->env_widget && w_list->data != prv->ncp_widget);
        gtk_container_remove(GTK_CONTAINER(self), GTK_WIDGET(w_list->data));
        w_list = g_list_delete_link(w_list, w_list);
    }
#endif
}

void
nwam_tooltip_widget_add_ncu(NwamTooltipWidget *self, NwamuiObject *object)
{
    NwamTooltipWidgetPrivate *prv = NWAM_TOOLTIP_WIDGET_GET_PRIVATE(self);

    g_assert(NWAMUI_IS_NCU(object));

#if DEF_CUSTOM_TREEVIEW_TOOLTIP
    GtkTreeModel *model = gtk_tree_view_get_model(prv->tooltip_treeview);
    GtkTreeIter iter;

    gtk_list_store_append(GTK_LIST_STORE(model), &iter);
    gtk_list_store_set(GTK_LIST_STORE(model), &iter, 0, object, -1);
#else
    if (object) {
        GtkWidget *label = nwam_menu_item_new_with_object(G_OBJECT(object));
        gtk_widget_show(label);
        gtk_box_pack_start(GTK_BOX(self), label, TRUE, TRUE, 1);
        g_object_set_data(G_OBJECT(label), TOOLTIP_WIDGET_DATA, (gpointer)object);
        ncu_notify_tooltip(G_OBJECT(object), NULL, (gpointer)label);
        g_signal_connect(object, "notify",
          G_CALLBACK(ncu_notify_tooltip), (gpointer)label);
    }
#endif
}

void
nwam_tooltip_widget_remove_ncu(NwamTooltipWidget *self, NwamuiObject *object)
{
    NwamTooltipWidgetPrivate *prv = NWAM_TOOLTIP_WIDGET_GET_PRIVATE(self);

    g_assert(NWAMUI_IS_NCU(object));

#if DEF_CUSTOM_TREEVIEW_TOOLTIP
    GtkTreeModel *model = gtk_tree_view_get_model(prv->tooltip_treeview);
    GtkTreeIter iter;
    NwamuiObject *ncu;

    if (gtk_tree_model_iter_nth_child(model, &iter, NULL, 2)) {
        do {
            gtk_tree_model_get(model, 0, &ncu, -1);
            if (ncu == (gpointer)object) {
                gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
                g_object_unref(ncu);
                break;
            }
            g_object_unref(ncu);
        } while (gtk_tree_model_iter_next(model, &iter));
    }
#else
    GtkWidget *label;
    GList *w_list;

    w_list = gtk_container_get_children(GTK_CONTAINER(self));

    for (; w_list;) {
        NwamuiObject *old = (NwamuiObject*)g_object_get_data(G_OBJECT(w_list->data), TOOLTIP_WIDGET_DATA);
        
        if (old == (gpointer)object) {
            g_signal_handlers_disconnect_matched(object,
              G_SIGNAL_MATCH_DATA,
              0,
              NULL,
              NULL,
              NULL,
              w_list->data);

            g_assert(w_list->data != prv->env_widget && w_list->data != prv->ncp_widget);
            gtk_container_remove(GTK_CONTAINER(self), GTK_WIDGET(w_list->data));

            break;
        }
        w_list = g_list_delete_link(w_list, w_list);
    }
    g_list_free(w_list);
#endif
}

static void
env_notify_tooltip(GObject *gobject, GParamSpec *arg1, gpointer user_data)
{
    NwamuiObject *object = NWAMUI_OBJECT(gobject);
    GtkWidget    *label = GTK_WIDGET(user_data);
	gchar        *str;
    gchar        *name;

    name = nwamui_object_get_name(object);

    str = g_strdup_printf(_("<b>Location:</b> %s"), name);
    menu_item_set_markup(GTK_MENU_ITEM(label), str);

    g_free(str);
    g_free(name);
}

static void
ncp_notify_tooltip(GObject *gobject, GParamSpec *arg1, gpointer user_data)
{
    NwamuiObject *object = NWAMUI_OBJECT(gobject);
    GtkWidget    *label = GTK_WIDGET(user_data);
	gchar        *str;
    gchar        *name;

    name = nwamui_object_get_name(object);

    str = g_strdup_printf(_("<b>Network Profile:</b> %s"), name);
    menu_item_set_markup(GTK_MENU_ITEM(label), str);

    g_free(str);
    g_free(name);
}

static void
ncu_notify_tooltip(GObject *gobject, GParamSpec *arg1, gpointer user_data)
{
    NwamuiObject *object = NWAMUI_OBJECT(gobject);
    GtkWidget    *label  = GTK_WIDGET(user_data);
	gchar        *str;
    gchar        *name;
    gchar        *state;

    name = nwamui_object_get_name(object);
    state = nwamui_ncu_get_connection_state_detail_string(NWAMUI_NCU(object));

    switch (nwamui_ncu_get_ncu_type(NWAMUI_NCU(object))) {
    case NWAMUI_NCU_TYPE_WIRELESS:
        str = g_strdup_printf(_("<b>Wireless (%s):</b> %s"), name, state);
        break;
    case NWAMUI_NCU_TYPE_WIRED:
    case NWAMUI_NCU_TYPE_TUNNEL:
        str = g_strdup_printf(_("<b>Wired (%s):</b> %s"), name, state);
        break;
    default:
        g_assert_not_reached ();
    }
    menu_item_set_markup(GTK_MENU_ITEM(label), str);

    g_free(str);
    g_free(state);
    g_free(name);
}

GtkWidget*
nwam_tooltip_widget_new(void)
{
    return GTK_WIDGET(g_object_new(NWAM_TYPE_TOOLTIP_WIDGET,
        "homogeneous", TRUE,
        NULL));
}

