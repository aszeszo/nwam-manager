/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set expandtab ts=4 shiftwidth=4: */
/*
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
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
 * File:   nwam_wireless_dialog.c
 *
 * Created on May 9, 2007, 11:01 AM
 *
 */

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n.h>

#include "nwam_env_svc.h"
#include "nwam_pref_iface.h"
#include "libnwamui.h"

/* Names of Widgets in Glade file */
#define DEFAULT_SVC_TREE_VIEW	"default_netservices_list"
#define ADDT_SVC_TREE_VIEW	"additional_netservices_list"
#define ADDT_SVC_ADD_BUTTON	"add_netservice_btn"
#define ADDT_SVC_REM_BUTTON	"delete_netservice_btn"

struct _NwamEnvSvcPrivate {
	/* Widget Pointers */
	GtkTreeView*	default_svc_view;
	GtkTreeView*	addt_svc_view;
	GtkButton*	addt_add_btn;
	GtkButton*	addt_rem_btn;

	/* Other Data */
};

enum {
	SVC_SWITCH=0,
	SVC_ICON,
	SVC_INFO
};

static void nwam_env_svc_finalize(NwamEnvSvc *self);
static void nwam_pref_init (gpointer g_iface, gpointer iface_data);

/* Callbacks */
static void nwam_env_svc_view_cb (GtkTreeViewColumn *col,
					     GtkCellRenderer   *renderer,
					     GtkTreeModel      *model,
					     GtkTreeIter       *iter,
					     gpointer           data);
static void object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);
static gint nwam_env_svc_comp_cb (GtkTreeModel *model,
			      GtkTreeIter *a,
			      GtkTreeIter *b,
			      gpointer user_data);
static void env_add_clicked_cb( GtkButton *button, gpointer data );
static void env_rem_clicked_cb( GtkButton *button, gpointer data );
static void svc_toggle_cb (GtkCellRendererToggle *cell_renderer,
               gchar                 *path,
               gpointer               user_data);
static gboolean refresh (NwamPrefIFace *self, gpointer data);
static gboolean apply (NwamPrefIFace *self, gpointer data);

G_DEFINE_TYPE_EXTENDED (NwamEnvSvc, 
                        nwam_env_svc, 
                        G_TYPE_OBJECT,
                        0, 
                        G_IMPLEMENT_INTERFACE (NWAM_TYPE_PREF_IFACE, nwam_pref_init));

static void
nwam_pref_init (gpointer g_iface, gpointer iface_data)
{
	NwamPrefInterface *iface = (NwamPrefInterface *)g_iface;
	iface->refresh = NULL;
	iface->apply = NULL;
}

static void
nwam_env_svc_class_init(NwamEnvSvcClass *klass)
{
	/* Pointer to GObject Part of Class */
	GObjectClass *gobject_class = (GObjectClass*) klass;
	
	/* Override Some Function Pointers */
	gobject_class->finalize = (void (*)(GObject*)) nwam_env_svc_finalize;
}

static void
nwam_compose_tree_view (NwamEnvSvc *self, GtkTreeView *view)
{
	GtkTreeViewColumn *col;
	GtkCellRenderer *renderer;
	GtkTreeModel *model;

	/* FIXME should change the type accordingly, only store smf FMRI */
	model = GTK_TREE_MODEL (gtk_tree_store_new (1, G_TYPE_STRING));
	gtk_tree_view_set_model (view, model);

	g_object_set (G_OBJECT(view),
		      "headers-clickable", FALSE,
		      NULL);
	
	// column switch
	col = gtk_tree_view_column_new();
	renderer = gtk_cell_renderer_toggle_new();
	gtk_tree_view_column_pack_start(col, renderer, TRUE);
	gtk_tree_view_column_set_cell_data_func (col,
						 renderer,
						 nwam_env_svc_view_cb,
						 (gpointer) self,
						 NULL
		);
	gtk_tree_view_column_set_sort_column_id (col, SVC_SWITCH);	
	g_object_set (col,
		      "resizable", TRUE,
		      "clickable", TRUE,
		      "sort-indicator", TRUE,
		      "reorderable", TRUE,
		      NULL);
	gtk_tree_view_append_column (view, col);
	gtk_tree_sortable_set_sort_func	(GTK_TREE_SORTABLE(model),
					 SVC_SWITCH,
					 (GtkTreeIterCompareFunc) nwam_env_svc_comp_cb,
					 GINT_TO_POINTER(SVC_SWITCH),
					 NULL);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE(model),
					      SVC_SWITCH,
					      GTK_SORT_ASCENDING);
	g_signal_connect(renderer, "toggled", (GCallback)svc_toggle_cb, (gpointer)self);

	
	// column icon
	col = gtk_tree_view_column_new();
	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(col, renderer, TRUE);
	gtk_tree_view_column_set_cell_data_func (col,
						 renderer,
						 nwam_env_svc_view_cb,
						 (gpointer) self,
						 NULL
		);
	gtk_tree_view_column_set_sort_column_id (col, SVC_ICON);	
	g_object_set (col,
		      "resizable", TRUE,
		      "clickable", TRUE,
		      "sort-indicator", TRUE,
		      "reorderable", TRUE,
		      NULL);
	gtk_tree_view_append_column (view, col);
	gtk_tree_sortable_set_sort_func	(GTK_TREE_SORTABLE(model),
					 SVC_SWITCH,
					 (GtkTreeIterCompareFunc) nwam_env_svc_comp_cb,
					 GINT_TO_POINTER(SVC_ICON),
					 NULL);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE(model),
					      SVC_ICON,
					      GTK_SORT_ASCENDING);

	// column info
	col = gtk_tree_view_column_new();
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col, renderer, TRUE);
	gtk_tree_view_column_set_cell_data_func (col,
						 renderer,
						 nwam_env_svc_view_cb,
						 (gpointer) self,
						 NULL
		);
	gtk_tree_view_column_set_sort_column_id (col, SVC_INFO);	
	g_object_set (col,
		      "resizable", TRUE,
		      "clickable", TRUE,
		      "sort-indicator", TRUE,
		      "reorderable", TRUE,
		      NULL);
	gtk_tree_view_append_column (view, col);
	gtk_tree_sortable_set_sort_func	(GTK_TREE_SORTABLE(model),
					 SVC_INFO,
					 (GtkTreeIterCompareFunc) nwam_env_svc_comp_cb,
					 GINT_TO_POINTER(SVC_INFO),
					 NULL);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE(model),
					      SVC_INFO,
					      GTK_SORT_ASCENDING);
}

static void
nwam_env_svc_init(NwamEnvSvc *self)
{
	GtkTreeModel *model;
	
	self->prv = g_new0(NwamEnvSvcPrivate, 1);
	
	/* Iniialise pointers to important widgets */
	self->prv->default_svc_view = GTK_TREE_VIEW(nwamui_util_glade_get_widget(DEFAULT_SVC_TREE_VIEW));
	self->prv->addt_svc_view = GTK_TREE_VIEW(nwamui_util_glade_get_widget(ADDT_SVC_TREE_VIEW));
	self->prv->addt_add_btn = GTK_BUTTON(nwamui_util_glade_get_widget(ADDT_SVC_ADD_BUTTON));
	self->prv->addt_rem_btn = GTK_BUTTON(nwamui_util_glade_get_widget(ADDT_SVC_REM_BUTTON));
	
	/* Construct the tree view */
	model = gtk_tree_view_get_model (self->prv->default_svc_view);
	if (model == NULL) {
		nwam_compose_tree_view (self, self->prv->default_svc_view);
		nwam_compose_tree_view (self, self->prv->addt_svc_view);
	}

	g_signal_connect(G_OBJECT(self), "notify", (GCallback)object_notify_cb, NULL);
	g_signal_connect(GTK_BUTTON(self->prv->addt_add_btn), "clicked", (GCallback)env_add_clicked_cb, (gpointer)self);
	g_signal_connect(GTK_BUTTON(self->prv->addt_rem_btn), "clicked", (GCallback)env_rem_clicked_cb, (gpointer)self);
}


/**
 * nwam_env_svc_new:
 * @returns: a new #NwamEnvSvc.
 *
 * Creates a new #NwamEnvSvc with an empty ncu
 **/
NwamEnvSvc*
nwam_env_svc_new(void)
{
	return NWAM_ENV_SVC(g_object_new(NWAM_TYPE_ENV_SVC, NULL));
}

static void
nwam_env_svc_finalize(NwamEnvSvc *self)
{
	g_free(self->prv);
	self->prv = NULL;
	
	G_OBJECT_CLASS(nwam_env_svc_parent_class)->finalize(G_OBJECT(self));
}

/* Callbacks */

/*
 * We don't need a comp here actually due to requirements
 */
static gint
nwam_env_svc_comp_cb (GtkTreeModel *model,
			 GtkTreeIter *a,
			 GtkTreeIter *b,
			 gpointer user_data)
{
	return 0;
}

static void
nwam_env_svc_view_cb (GtkTreeViewColumn *col,
				 GtkCellRenderer   *renderer,
				 GtkTreeModel      *model,
				 GtkTreeIter       *iter,
				 gpointer           data)
{
	NwamEnvSvc* self = NWAM_ENV_SVC(data);
	gchar *stockid = NULL;
	gchar *text;
	GString *infobuf;

	gtk_tree_model_get(model, iter, 0, &text, -1);
	/* FIXME, get the current status of connection then update the
	 * tree view
	 */
	switch (gtk_tree_view_column_get_sort_column_id (col)) {
	case SVC_SWITCH:
		/* FIXME, update the toggle */
		//gtk_cell_renderer_toggle_set_active
		break;
	case SVC_ICON:
		/* FIXME, update the icon */
		g_object_set (G_OBJECT(renderer),
			"stock-id", stockid,
			"stock-size", GTK_ICON_SIZE_LARGE_TOOLBAR,
			NULL);
		break;
	case SVC_INFO:
		/* FIXME, update the information */
		infobuf = g_string_new ("test");
		g_object_set (G_OBJECT(renderer),
			"text", infobuf->str,
			NULL);
		g_string_free (infobuf, TRUE);
		break;
	default:
		g_assert_not_reached ();
	}
}

static void
env_add_clicked_cb( GtkButton *button, gpointer data )
{
	GtkWidget *toplevel;
	
	g_debug ("env_add_clicked_cb");
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET(button));
	if (!GTK_WIDGET_TOPLEVEL(toplevel)) {
		toplevel = NULL;
	}
	//FIXME, pop up a confirmation dialog.
}

static void
env_rem_clicked_cb( GtkButton *button, gpointer data )
{
	NwamEnvSvc* self = NWAM_ENV_SVC(data);
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GList *list, *idx;

	//FIXME, pop up a confirmation dialog?
	g_debug ("env_rem_clicked_cb");
	selection = gtk_tree_view_get_selection (self->prv->addt_svc_view);
	model = gtk_tree_view_get_model (self->prv->addt_svc_view);
	list = gtk_tree_selection_get_selected_rows (selection, NULL);
	
	for (idx=list; idx; idx = g_list_next(idx)) {
		GtkTreeIter  iter;
		g_assert (idx->data);
		if (gtk_tree_model_get_iter(model, &iter, (GtkTreePath *)idx->data)) {
			gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);
		} else {
			g_assert_not_reached ();
		}
		gtk_tree_path_free ((GtkTreePath *)idx->data);
	}
	g_list_free (list);
}

static void
svc_toggle_cb (GtkCellRendererToggle *cell_renderer,
               gchar                 *path,
               gpointer               user_data)
{
	// FIXME toggle enable/disable service
}

static void
object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
	g_debug("NwamEnvSvc: notify %s changed\n", arg1->name);
}
