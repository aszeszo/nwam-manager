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

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n.h>

#include "libnwamui.h"
#include "nwam_pref_dialog.h"
#include "nwam_env_pref_dialog.h"
#include "nwam_location_dialog.h"
#include "nwam_pref_iface.h"
#include "capplet-utils.h"
#include "nwam_tree_view.h"
#include "nwam_rules_dialog.h"

/* Names of Widgets in Glade file */
#define LOCATION_DIALOG           "nwam_location"
#define LOCATION_TREE               "location_tree"
#define LOCATION_ADD_BTN          "location_add_btn"
#define LOCATION_REMOVE_BTN        "location_remove_btn"
#define LOCATION_RENAME_BTN           "location_rename_btn"
#define LOCATION_DUP_BTN           "location_dup_btn"
#define LOCATION_EDIT_BTN           "location_edit_btn"
#define LOCATION_ACTIVATION_COMBO      "location_activation_combo"
#define LOCATION_RULES_BTN           "location_rules_btn"

#define TREEVIEW_COLUMN_NUM             "meta:column"

struct _NwamLocationDialogPrivate {
	/* Widget Pointers */
    GtkDialog*          location_dialog;
	GtkTreeView*	    location_tree;
    GtkButton*          location_add_btn;
    GtkButton*          location_remove_btn;
    GtkButton*          location_rename_btn;
    GtkButton*          location_dup_btn;
    GtkButton*          location_edit_btn;

    GtkComboBox*        location_activation_combo;
    GtkButton*          location_rules_btn;

	/* Other Data */
    NwamEnvPrefDialog*  env_pref_dialog;
};

enum {
    PROP_PLACEHOLDER = 1,
};

enum {
	LOCVIEW_TOGGLE=0,
	LOCVIEW_MODE,
    LOCVIEW_NAME
};

enum {
    NWAMUI_LOC_ACTIVATION_MANUAL = 0,
    NWAMUI_LOC_ACTIVATION_BY_RULES,
    NWAMUI_LOC_ACTIVATION_BY_SYSTEM,
    NWAMUI_LOC_ACTIVATION_LAST
};

static const gchar *combo_contents[NWAMUI_LOC_ACTIVATION_LAST] = {
    N_("manual activation only"),
    N_("activated by rules"),
    N_("activated by system")
};

static void nwam_pref_init (gpointer g_iface, gpointer iface_data);
static gboolean refresh(NwamPrefIFace *iface, gpointer user_data, gboolean force);
static gboolean apply(NwamPrefIFace *iface, gpointer user_data);
static gboolean cancel(NwamPrefIFace *iface, gpointer user_data);
static gboolean help(NwamPrefIFace *iface, gpointer user_data);
static gint dialog_run(NwamPrefIFace *iface, GtkWindow *parent);

static void nwam_location_dialog_finalize(NwamLocationDialog *self);

static void nwam_location_add (NwamLocationDialog *self, NwamuiNcu* connection);

static void location_set_property (GObject         *object,
  guint            prop_id,
  const GValue    *value,
  GParamSpec      *pspec);

static void location_get_property (GObject         *object,
  guint            prop_id,
  GValue          *value,
  GParamSpec      *pspec);

/* Callbacks */
static void response_cb( GtkWidget* widget, gint repsonseid, gpointer data );
static void object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);
static gint nwam_location_connection_compare_cb (GtkTreeModel *model,
			      GtkTreeIter *a,
			      GtkTreeIter *b,
			      gpointer user_data);
static void nwam_location_connection_view_row_activated_cb (GtkTreeView *tree_view,
  GtkTreePath *path,
  GtkTreeViewColumn *column,
  gpointer data);

static void nwam_location_connection_view_row_selected_cb (GtkTreeView *tree_view,
                                                           gpointer data);

static void vanity_name_editing_started (GtkCellRenderer *cell,
                                         GtkCellEditable *editable,
                                         const gchar     *path,
                                         gpointer         data);
static void vanity_name_edited ( GtkCellRendererText *cell,
                                 const gchar         *path_string,
                                 const gchar         *new_text,
                                 gpointer             data);
static void nwam_location_connection_enabled_toggled_cb(    GtkCellRendererToggle *cell_renderer,
                                                            gchar                 *path,
                                                            gpointer               user_data);

static void nwam_treeview_update_widget_cb(GtkTreeSelection *selection, gpointer user_data);
static void on_button_clicked(GtkButton *button, gpointer user_data);

static void location_activation_combo_cell_cb(GtkCellLayout *cell_layout,
  GtkCellRenderer   *renderer,
  GtkTreeModel      *model,
  GtkTreeIter       *iter,
  gpointer           data);
static void location_activation_combo_changed_cb(GtkComboBox* combo, gpointer user_data );

static gboolean tree_model_foreach_find_enabled_env(GtkTreeModel *model,
  GtkTreePath *path,
  GtkTreeIter *iter,
  gpointer user_data);

G_DEFINE_TYPE_EXTENDED (NwamLocationDialog,
  nwam_location_dialog,
  G_TYPE_OBJECT,
  0,
  G_IMPLEMENT_INTERFACE (NWAM_TYPE_PREF_IFACE, nwam_pref_init))

static void
nwam_pref_init (gpointer g_iface, gpointer iface_data)
{
	NwamPrefInterface *iface = (NwamPrefInterface *)g_iface;
	iface->refresh = refresh;
	iface->apply = apply;
	iface->cancel = cancel;
	iface->help = help;
    iface->dialog_run = dialog_run;
}

static void
nwam_location_dialog_class_init(NwamLocationDialogClass *klass)
{
	/* Pointer to GObject Part of Class */
	GObjectClass *gobject_class = (GObjectClass*) klass;
    gobject_class->set_property = location_set_property;
    gobject_class->get_property = location_get_property;

	/* Override Some Function Pointers */
	gobject_class->finalize = (void (*)(GObject*)) nwam_location_dialog_finalize;

}

static void
location_set_property ( GObject         *object,
  guint            prop_id,
  const GValue    *value,
  GParamSpec      *pspec)
{
    NwamLocationDialog*       self = NWAM_LOCATION_DIALOG(object);

    switch (prop_id) {
    default: 
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
location_get_property ( GObject         *object,
  guint            prop_id,
  GValue          *value,
  GParamSpec      *pspec)
{
    NwamLocationDialog*       self = NWAM_LOCATION_DIALOG(object);

    switch (prop_id) {
    default: 
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
nwam_compose_tree_view (NwamLocationDialog *self)
{
	GtkTreeViewColumn *col;
	GtkCellRenderer *cell;
	GtkTreeView *view = self->prv->location_tree;
        
	g_object_set (G_OBJECT(view),
      "headers-clickable", TRUE,
      "headers-visible", TRUE,
      "rules-hint", TRUE,
      "reorderable", FALSE,
      "enable-search", TRUE,
      "show-expanders", TRUE,
      "button-add", self->prv->location_add_btn,
      NULL);

    nwam_tree_view_set_object_func(NWAM_TREE_VIEW(view),
      NULL,
      NULL,
      capplet_tree_view_commit_object,
      (gpointer)self);

    g_signal_connect(gtk_tree_view_get_selection(view),
      "changed",
      G_CALLBACK(nwam_treeview_update_widget_cb),
      (gpointer)self);

    /* Toggle column */
	col = capplet_column_new(view, NULL);
    g_object_set_data (G_OBJECT (col), TREEVIEW_COLUMN_NUM, GINT_TO_POINTER (LOCVIEW_TOGGLE));

	cell = capplet_column_append_cell(col, gtk_cell_renderer_toggle_new(),
      FALSE,
      (GtkTreeCellDataFunc)nwamui_object_active_toggle_cell, (gpointer) 0, NULL);

	g_object_set (cell,
      "xalign", 0.5,
      "radio", TRUE,
      NULL );
    g_object_set_data (G_OBJECT (cell), TREEVIEW_COLUMN_NUM, GINT_TO_POINTER (LOCVIEW_TOGGLE));

    g_signal_connect(G_OBJECT(cell), "toggled", G_CALLBACK(nwam_location_connection_enabled_toggled_cb), (gpointer)self);

    /* Mode column */
	col = capplet_column_new(view, NULL);

    g_object_set_data (G_OBJECT (col), TREEVIEW_COLUMN_NUM, GINT_TO_POINTER (LOCVIEW_MODE));

	cell = capplet_column_append_cell(col, gtk_cell_renderer_pixbuf_new(), FALSE,
      nwamui_object_active_mode_pixbuf_cell, NULL, NULL);

    g_object_set_data (G_OBJECT (cell), TREEVIEW_COLUMN_NUM, GINT_TO_POINTER (LOCVIEW_MODE));

    /* Name column */
	col = capplet_column_new(view,
      "title", _("Name"),
      "expand", FALSE,
      "resizable", TRUE,
      NULL);
	cell = capplet_column_append_cell(col, gtk_cell_renderer_text_new(), FALSE,
	    (GtkTreeCellDataFunc)nwamui_object_name_cell, NULL, NULL);
	g_signal_connect(cell, "edited",
      G_CALLBACK(nwamui_object_name_cell_edited), (gpointer)view);
    g_signal_connect (cell, "edited", G_CALLBACK(vanity_name_edited), (gpointer)self);
    g_signal_connect (cell, "editing-started", G_CALLBACK(vanity_name_editing_started), (gpointer)self);

    g_object_set_data (G_OBJECT (col), TREEVIEW_COLUMN_NUM, GINT_TO_POINTER(LOCVIEW_NAME));
	g_object_set (cell, "weight", PANGO_WEIGHT_BOLD, NULL);
	g_object_set_data (G_OBJECT (cell), TREEVIEW_COLUMN_NUM, GINT_TO_POINTER (LOCVIEW_NAME));

    /* Model */
    CAPPLET_COMPOSE_NWAMUI_OBJECT_LIST_VIEW(view);
}

static void
nwam_location_dialog_init(NwamLocationDialog *self)
{
    NwamuiDaemon *daemon = nwamui_daemon_get_instance();
	self->prv = g_new0(NwamLocationDialogPrivate, 1);
	/* Iniialise pointers to important widgets */
    self->prv->location_dialog = GTK_DIALOG(nwamui_util_glade_get_widget(LOCATION_DIALOG));
    capplet_remove_gtk_dialog_escape_binding(GTK_DIALOG_GET_CLASS(self->prv->location_dialog));

	self->prv->location_tree = GTK_TREE_VIEW(nwamui_util_glade_get_widget(LOCATION_TREE));
    self->prv->location_add_btn = GTK_BUTTON(nwamui_util_glade_get_widget(LOCATION_ADD_BTN));
    self->prv->location_remove_btn = GTK_BUTTON(nwamui_util_glade_get_widget(LOCATION_REMOVE_BTN));
    self->prv->location_rename_btn = GTK_BUTTON(nwamui_util_glade_get_widget(LOCATION_RENAME_BTN));
    self->prv->location_dup_btn = GTK_BUTTON(nwamui_util_glade_get_widget(LOCATION_DUP_BTN));
    self->prv->location_edit_btn = GTK_BUTTON(nwamui_util_glade_get_widget(LOCATION_EDIT_BTN));

    self->prv->location_rules_btn = GTK_BUTTON(nwamui_util_glade_get_widget(LOCATION_RULES_BTN));

    /* Set title to include hostname */
    nwamui_util_window_title_append_hostname( self->prv->location_dialog );

    g_signal_connect(self->prv->location_rules_btn,
      "clicked", G_CALLBACK(on_button_clicked), (gpointer)self);
    g_signal_connect(self->prv->location_add_btn,
      "clicked", G_CALLBACK(on_button_clicked), (gpointer)self);
    g_signal_connect(self->prv->location_remove_btn,
      "clicked", G_CALLBACK(on_button_clicked), (gpointer)self);
    g_signal_connect(self->prv->location_rename_btn,
      "clicked", G_CALLBACK(on_button_clicked), (gpointer)self);
    g_signal_connect(self->prv->location_edit_btn,
      "clicked", G_CALLBACK(on_button_clicked), (gpointer)self);
    g_signal_connect(self->prv->location_dup_btn,
      "clicked", G_CALLBACK(on_button_clicked), (gpointer)self);

    g_signal_connect(self->prv->location_dialog,
      "response", G_CALLBACK(response_cb), (gpointer)self);

    self->prv->location_activation_combo = GTK_COMBO_BOX(nwamui_util_glade_get_widget(LOCATION_ACTIVATION_COMBO));
    capplet_compose_combo(self->prv->location_activation_combo,
      GTK_TYPE_LIST_STORE,
      G_TYPE_INT,
      location_activation_combo_cell_cb,
      NULL,
      G_CALLBACK(location_activation_combo_changed_cb),
      (gpointer)self,
      NULL);

    {
        GtkTreeModel      *model;
        GtkTreeIter   iter;
        int i;

        model = gtk_combo_box_get_model(GTK_COMBO_BOX(self->prv->location_activation_combo));
        /* Clean all */
        gtk_list_store_clear(GTK_LIST_STORE(model));

        /* Add entries for each selection item */
        for (i = 0; i < NWAMUI_LOC_ACTIVATION_LAST; i++) {
            gtk_list_store_append(GTK_LIST_STORE(model), &iter);
            gtk_list_store_set (GTK_LIST_STORE(model), &iter, 0, i, -1);
        }
    }

    nwam_compose_tree_view(self);
    
	g_signal_connect(G_OBJECT(self), "notify", (GCallback)object_notify_cb, NULL);
	g_signal_connect(GTK_TREE_VIEW(self->prv->location_tree),
      "row-activated",
      (GCallback)nwam_location_connection_view_row_activated_cb,
      (gpointer)self);
	g_signal_connect(GTK_TREE_VIEW(self->prv->location_tree),
      "cursor-changed",
      (GCallback)nwam_location_connection_view_row_selected_cb,
      (gpointer)self);

    /* Initially refresh self */
    nwam_pref_refresh(NWAM_PREF_IFACE(self), NULL, TRUE);

    g_object_unref(daemon);
}

/**
 * nwam_location_dialog_new:
 * @returns: a new #NwamLocationDialog.
 *
 * Creates a new #NwamLocationDialog with an empty ncu
 **/
NwamLocationDialog*
nwam_location_dialog_new(void)
{
	NwamLocationDialog *self =  NWAM_LOCATION_DIALOG(g_object_new(NWAM_TYPE_LOCATION_DIALOG, NULL));

    return( self );
}

/* FIXME, if update failed, popup dialog and return FALSE */
static gboolean
nwam_update_obj (NwamLocationDialog *self, GObject *obj)
{
	NwamLocationDialogPrivate  *prv = self->prv;
    gboolean enabled;
    gint cond;
	const gchar *txt = NULL;
    gchar* prev_txt = NULL;

    if (!NWAMUI_IS_ENV(obj)) {
        return( FALSE );
    }
#if 0
	prev_txt = nwamui_env_get_start_command(NWAMUI_ENV(obj));
	txt = gtk_entry_get_text(prv->start_cmd_entry);
	if ( strcmp( prev_txt?prev_txt:"", txt?txt:"") != 0 ) {
        if ( !nwamui_env_set_start_command(NWAMUI_ENV(obj), txt?txt:"") ) {
            nwamui_util_show_message (self->prv->vpn_pref_dialog, GTK_MESSAGE_ERROR, _("Validation Error"), 
                    _("Invalid value specified for Start Command") );
            return( FALSE );
        }
    }
    g_free(prev_txt);
#endif

	return TRUE;
}

static gint
dialog_run(NwamPrefIFace *iface, GtkWindow *parent)
{
    NwamLocationDialog*    self = NWAM_LOCATION_DIALOG( iface );
    gint response = GTK_RESPONSE_NONE;
    
    g_assert(NWAM_IS_LOCATION_DIALOG (self));
    
    if ( self->prv->location_dialog != NULL ) {
        if (parent) {
            gtk_window_set_transient_for (GTK_WINDOW(self->prv->location_dialog), parent);
            gtk_window_set_modal (GTK_WINDOW(self->prv->location_dialog), TRUE);
        } else {
            gtk_window_set_transient_for (GTK_WINDOW(self->prv->location_dialog), NULL);
            gtk_window_set_modal (GTK_WINDOW(self->prv->location_dialog), FALSE);		
        }
        nwam_pref_refresh(NWAM_PREF_IFACE(self), NULL, FALSE);
        
        response =  gtk_dialog_run(GTK_DIALOG(self->prv->location_dialog));

    }
    return(response);
}

/**
 * refresh:
 *
 * Refresh #NwamLocationDialog with the new connections.
 **/
static gboolean
refresh(NwamPrefIFace *iface, gpointer user_data, gboolean force)
{
    NwamLocationDialog*    self = NWAM_LOCATION_DIALOG( iface );
    
    g_debug("NwamLocationDialog: Refresh");
    g_assert(NWAM_IS_LOCATION_DIALOG(self));
    
    if (force) {
        NwamuiDaemon *daemon = nwamui_daemon_get_instance();

        gtk_widget_hide(GTK_WIDGET(self->prv->location_tree));
        capplet_update_model_from_daemon(gtk_tree_view_get_model(self->prv->location_tree), daemon, NWAMUI_TYPE_ENV);
        gtk_widget_show(GTK_WIDGET(self->prv->location_tree));

        g_object_unref(daemon);

    }

    nwam_treeview_update_widget_cb(gtk_tree_view_get_selection(self->prv->location_tree),
      (gpointer)self);

    capplet_reset_increasable_name(G_OBJECT(self));

    return( TRUE );
}

static gboolean
tree_model_foreach_revert(GtkTreeModel *model,
  GtkTreePath *path,
  GtkTreeIter *iter,
  gpointer user_data)
{
	NwamuiEnv *env;

    gtk_tree_model_get( GTK_TREE_MODEL(model), iter, 0, &env, -1);
    if (env) {
        nwamui_object_reload( NWAMUI_OBJECT(env) );
        g_object_unref(env);
    }
    return FALSE;
}
static gboolean
cancel(NwamPrefIFace *iface, gpointer user_data)
{
    GtkTreeModel          *model;
    NwamLocationDialog    *self = NWAM_LOCATION_DIALOG( iface );

    /* Re-read objects from system 
     */

	model = gtk_tree_view_get_model(self->prv->location_tree);
    gtk_tree_model_foreach(model,
                           tree_model_foreach_revert,
                           NULL);

}

static gboolean
apply(NwamPrefIFace *iface, gpointer user_data)
{
    NwamLocationDialog*    self = NWAM_LOCATION_DIALOG( iface );
	NwamLocationDialogPrivate  *prv = self->prv;
	GtkTreeSelection           *selection = NULL;
	GtkTreeIter                 iter;
    GtkTreePath                *path = NULL;
    GtkTreeModel               *model;
    GObject                    *obj;
    gboolean retval = TRUE;

    model = gtk_tree_view_get_model (prv->location_tree);

    /* FIXME, ok need call into separated panel/instance
     * apply all changes, if no errors, hide all
     */
    if (retval && gtk_tree_model_get_iter_first (model, &iter)) {
        gchar* prop_name = NULL;
        do {
            gtk_tree_model_get (model, &iter, 0, &obj, -1);
            if (!nwam_update_obj (self, obj)) {
                g_object_unref(obj);
                retval = FALSE;
                break;
            }
            if (nwamui_env_validate (NWAMUI_ENV (obj), &prop_name)) {
                if (!nwamui_env_commit (NWAMUI_ENV (obj))) {
                    /* Start highlight relevant ENV */
                    path = gtk_tree_model_get_path (model, &iter);
                    gtk_tree_view_set_cursor(prv->location_tree, path, NULL, TRUE);
                    gtk_tree_path_free (path);

                    gchar *name = nwamui_env_get_name (NWAMUI_ENV (obj));
                    gchar *msg = g_strdup_printf (_("Committing %s failed..."), name);
                    nwamui_util_show_message (GTK_WINDOW(prv->location_dialog),
                      GTK_MESSAGE_ERROR,
                      _("Commit ENV error"),
                      msg);
                    g_free (msg);
                    g_free (name);
                    g_object_unref(obj);
                    retval = FALSE;
                    break;
                }
            }
            else {
                gchar *name = nwamui_env_get_name (NWAMUI_ENV (obj));
                gchar *msg = g_strdup_printf (_("Validation of %s failed with the property %s"), name, prop_name);

                /* Start highligh relevant ENV */
                path = gtk_tree_model_get_path (model, &iter);
                gtk_tree_view_set_cursor(prv->location_tree, path, NULL, TRUE);
                gtk_tree_path_free (path);

                nwamui_util_show_message (GTK_WINDOW(prv->location_dialog),
                  GTK_MESSAGE_ERROR,
                  _("Validation error"),
                  msg);
                g_free (msg);
                g_free (name);
                g_object_unref(obj);
                retval = FALSE;
                break;
            }
            g_object_unref(obj);
        } while (gtk_tree_model_iter_next (model, &iter));
    }

    if (retval) {
        NwamuiDaemon *daemon = nwamui_daemon_get_instance();
        GList *nlist = capplet_model_to_list(model);
        GList *olist = nwamui_daemon_get_env_list(daemon);
        GList *i;
        /* Sync o and b */
        if (nlist) {
            /* We remove the element from olist if it existed in both list. So
             * olist finally have all the private elements which should be
             * deleted. */
            for (i = nlist; i; i = i->next) {
                GList *found;

                if ((found = g_list_find(olist, i->data)) != NULL) {
                    g_object_unref(found->data);
                    olist = g_list_delete_link(olist, found);
                } else {
                    nwamui_daemon_env_append(daemon, NWAMUI_ENV(i->data));
                }
                g_object_unref(i->data);
            }
            g_list_free(nlist);
        }

        for (i = olist; i; i = i->next) {
            nwamui_daemon_env_remove(daemon, NWAMUI_ENV(i->data));
            g_object_unref(i->data);
        }

        g_list_free(olist);

        g_object_unref(daemon);
    }

}

/**
 * help:
 *
 * Help #NwamLocationDialog
 **/
static gboolean
help(NwamPrefIFace *iface, gpointer user_data)
{
    g_debug ("NwamLocationDialog: Help");
    nwamui_util_show_help (HELP_REF_LOCATIONPREFS_DIALOG);
}


static void
nwam_location_dialog_finalize(NwamLocationDialog *self)
{
    g_object_unref(G_OBJECT(self->prv->location_add_btn));	
    g_object_unref(G_OBJECT(self->prv->location_remove_btn));	
    g_object_unref(G_OBJECT(self->prv->location_rename_btn));	

    g_free(self->prv);
	self->prv = NULL;
	
	G_OBJECT_CLASS(nwam_location_dialog_parent_class)->finalize(G_OBJECT(self));
}

/* Callbacks */

/*
 * We don't need a comp here actually due to requirements
 */
static gint
nwam_location_connection_compare_cb (GtkTreeModel *model,
			 GtkTreeIter *a,
			 GtkTreeIter *b,
			 gpointer user_data)
{
	return 0;
}

/*
 * Enabled Toggle Button was toggled
 */
static void
nwam_location_connection_enabled_toggled_cb(GtkCellRendererToggle *cell_renderer,
  gchar                 *path,
  gpointer               user_data) 
{
	NwamLocationDialog *self =  NWAM_LOCATION_DIALOG(user_data);
    GtkTreeIter iter;
    GtkTreeModel *model;
    GtkTreePath  *tpath;

	model = gtk_tree_view_get_model(self->prv->location_tree);
    tpath = gtk_tree_path_new_from_string(path);
    if (tpath != NULL && gtk_tree_model_get_iter (model, &iter, tpath))
    {
        NwamuiEnv*  env;

        gtk_tree_model_get(model, &iter, 0, &env, -1);

        if (env) {
            /* Figer out which one is enabled, change it to disabled */
            gtk_tree_model_foreach(model,
              tree_model_foreach_find_enabled_env,
              NULL);

            if (gtk_cell_renderer_toggle_get_active(cell_renderer)) {
                nwamui_env_set_enabled(env, FALSE);
            } else {
                nwamui_env_set_enabled(env, TRUE);
            }

            g_object_unref(env);
        }
        
    }
    if (tpath) {
        gtk_tree_path_free(tpath);
    }
}

static void
vanity_name_editing_started (GtkCellRenderer *cell,
                             GtkCellEditable *editable,
                             const gchar     *path,
                             gpointer         data)
{
	NwamLocationDialog *self =  NWAM_LOCATION_DIALOG(data);

    g_debug("Editing Started");

    g_object_set (cell, "editable", FALSE, NULL);

    if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT(cell), TREEVIEW_COLUMN_NUM)) != LOCVIEW_NAME) {
        return;
    }
    if (GTK_IS_ENTRY (editable)) {
        GtkEntry *entry = GTK_ENTRY (editable);
        GtkTreeIter iter;
        GtkTreeModel *model = gtk_tree_view_get_model(self->prv->location_tree);
        GtkTreePath  *tpath;

        if ( (tpath = gtk_tree_path_new_from_string(path)) != NULL 
              && gtk_tree_model_get_iter (model, &iter, tpath))
        {
            gpointer    connection;
            NwamuiEnv*  env;
            gchar*      vname;

            gtk_tree_model_get(model, &iter, 0, &connection, -1);

            env  = NWAMUI_ENV( connection );

            vname = nwamui_env_get_name(env);
            
            gtk_entry_set_text( entry, vname?vname:"" );

            g_free( vname );
            
            g_object_unref(G_OBJECT(connection));

            gtk_tree_path_free(tpath);
        }
    }
}

static void
vanity_name_edited ( GtkCellRendererText *cell,
                     const gchar         *path_string,
                     const gchar         *new_text,
                     gpointer             data)
{
    g_object_set (cell, "editable", FALSE, NULL);
}

/*
 * Double-clicking a connection switches the status view to that connection's
 * properties view (p5)
 */
static void
nwam_location_connection_view_row_activated_cb (GtkTreeView *tree_view,
			    GtkTreePath *path,
			    GtkTreeViewColumn *column,
			    gpointer data)
{
	NwamLocationDialog* self = NWAM_LOCATION_DIALOG(data);
    GtkTreeIter iter;
    GtkTreeModel *model = gtk_tree_view_get_model(tree_view);

    gint columnid = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (column), TREEVIEW_COLUMN_NUM));

    /* skip the toggle coolumn */
    if (columnid != LOCVIEW_TOGGLE ) {
        if (gtk_tree_model_get_iter (model, &iter, path)) {
            gpointer    connection;
            NwamuiEnv*  env;

            gtk_tree_model_get(model, &iter, 0, &connection, -1);

            env  = NWAMUI_ENV( connection );

            /* nwam_capplet_dialog_select_env(self->prv->pref_dialog, env ); */
        }
    }
}

/*
 * Selecting (using keys or mouse) a connection needs to have the conditions
 * updated.
 */
static void
nwam_location_connection_view_row_selected_cb (GtkTreeView *tree_view,
                                               gpointer data)
{
	NwamLocationDialog*   self = NWAM_LOCATION_DIALOG(data);
    GtkTreePath*        path = NULL;
    GtkTreeViewColumn*  focus_column = NULL;;
    GtkTreeIter         iter;
    GtkTreeModel*       model = gtk_tree_view_get_model(tree_view);

    gtk_tree_view_get_cursor ( tree_view, &path, &focus_column );

    if (path != NULL && gtk_tree_model_get_iter (model, &iter, path))
    {
        gpointer    connection;
        NwamuiEnv*  env;

        gtk_tree_model_get(model, &iter, 0, &connection, -1);

        env  = NWAMUI_ENV( connection );
        
    }
    gtk_tree_path_free (path);
}

static void
nwam_treeview_update_widget_cb(GtkTreeSelection *selection, gpointer user_data)
{
    NwamLocationDialog*           self = NWAM_LOCATION_DIALOG(user_data);
    NwamLocationDialogPrivate*    prv = self->prv;
    GtkTreeModel*               model;
    GtkTreeIter                 iter;
    gint                        count_selected_rows;

    count_selected_rows = gtk_tree_selection_count_selected_rows(selection);

    gtk_widget_set_sensitive(GTK_WIDGET(prv->location_edit_btn), count_selected_rows > 0 ? TRUE : FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(prv->location_dup_btn), count_selected_rows > 0 ? TRUE : FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(prv->location_rename_btn), count_selected_rows > 0 ? TRUE : FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(prv->location_remove_btn), count_selected_rows > 0 ? TRUE : FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(prv->location_activation_combo), count_selected_rows > 0 ? TRUE : FALSE);

    if ( gtk_tree_selection_get_selected( selection, &model, &iter ) ) {
        NwamuiEnv *env;

        gtk_tree_model_get(model, &iter, 0, &env, -1);
        

        switch (nwamui_env_get_activation_mode(env)) {
        case NWAMUI_COND_ACTIVATION_MODE_MANUAL:
            gtk_widget_set_sensitive(GTK_WIDGET(prv->location_activation_combo), TRUE);
            gtk_combo_box_set_active(prv->location_activation_combo, NWAMUI_LOC_ACTIVATION_MANUAL);
            break;
        case NWAMUI_COND_ACTIVATION_MODE_CONDITIONAL_ANY:
            gtk_widget_set_sensitive(GTK_WIDGET(prv->location_activation_combo), TRUE);
            gtk_combo_box_set_active(prv->location_activation_combo, NWAMUI_LOC_ACTIVATION_BY_RULES );
            break;
        case NWAMUI_COND_ACTIVATION_MODE_SYSTEM:
            /* If Activation Mode is system, then you can't rename or remove
             * the selected location.
             */
            gtk_widget_set_sensitive(GTK_WIDGET(prv->location_rename_btn), FALSE );
            gtk_widget_set_sensitive(GTK_WIDGET(prv->location_remove_btn), FALSE );
            /* Fall-through */
        case NWAMUI_COND_ACTIVATION_MODE_PRIORITIZED: /* ?? TODO */
        default:
            gtk_combo_box_set_active(prv->location_activation_combo, NWAMUI_LOC_ACTIVATION_BY_SYSTEM);
            gtk_widget_set_sensitive(GTK_WIDGET(prv->location_activation_combo), FALSE);
            gtk_widget_set_sensitive(GTK_WIDGET(prv->location_remove_btn), FALSE);
            break;
        }

        g_object_unref(env);
    }
}

static void
on_button_clicked(GtkButton *button, gpointer user_data)
{
    NwamLocationDialog*           self = NWAM_LOCATION_DIALOG(user_data);
    NwamLocationDialogPrivate*    prv = self->prv;
    GtkTreeModel*               model;
    GtkTreeIter                 iter;

    
    if (button == (gpointer)prv->location_add_btn) {
        gchar *name;
        NwamuiObject *object;

        name = capplet_get_increasable_name(gtk_tree_view_get_model(GTK_TREE_VIEW(prv->location_tree)),
                                                                    _("Unnamed Location"), G_OBJECT(self));

        g_assert(name);

        object = NWAMUI_OBJECT(nwamui_env_new(name) );
        CAPPLET_LIST_STORE_ADD(GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(prv->location_tree))), object);
        g_free(name);
        g_object_unref(object);

        nwam_tree_view_select_cached_object(NWAM_TREE_VIEW(prv->location_tree));
        return;
    }

    if ( gtk_tree_selection_get_selected(gtk_tree_view_get_selection(prv->location_tree), &model, &iter ) ) {
        NwamuiEnv *env;

        gtk_tree_model_get(model, &iter, 0, &env, -1);

        if (button == (gpointer)prv->location_edit_btn) {
            static NwamEnvPrefDialog *env_pref_dialog = NULL;
            if (env_pref_dialog == NULL)
                env_pref_dialog = nwam_env_pref_dialog_new();

            nwam_pref_refresh(NWAM_PREF_IFACE(env_pref_dialog), NWAMUI_OBJECT(env), TRUE);
            capplet_dialog_run(NWAM_PREF_IFACE(env_pref_dialog), GTK_WIDGET(button));

        } else if (button == (gpointer)prv->location_remove_btn) {
            gchar*  name = nwamui_env_get_name( env );
            gchar*  message = g_strdup_printf(_("Remove location '%s'?"), name?name:"" );
            if (nwamui_util_ask_yes_no( GTK_WINDOW(prv->location_dialog), _("Remove Location?"), message )) {
                g_debug("Removing location: '%s'", name);
            
                gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
                nwamui_object_destroy(NWAMUI_OBJECT(env));
            }
        
            if (name)
                g_free(name);
        
            g_free(message);
        } else if (button == (gpointer)prv->location_dup_btn) {
            gchar*  sname = nwamui_env_get_name( env );
            gchar *oname;
            gchar *prefix;
            gchar *name;
            NwamuiObject *object;

            /* TODO avoid Copy of Copy of ... */
            /* Try to get the original name if it is something like 'Copy...' */
            if (!(oname = capplet_get_original_name(_("Copy of"), sname))) {
                prefix = g_strdup_printf(_("Copy of %s"), sname);
            } else {
                prefix = g_strdup_printf(_("Copy of %s"), oname);
                g_free(oname);
            }

            name = capplet_get_increasable_name(gtk_tree_view_get_model(GTK_TREE_VIEW(prv->location_tree)), prefix, G_OBJECT(env));

            g_assert(name);

            object = NWAMUI_OBJECT(nwamui_env_clone( env ));
            nwamui_object_set_name(object, name);

            CAPPLET_LIST_STORE_ADD(GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(prv->location_tree))), object);

            g_free(name);
            g_free(prefix);
            g_free(sname);
            g_object_unref(object);


/*             NwamuiDaemon *daemon = nwamui_daemon_get_instance(); */



/*             nwamui_daemon_env_append(daemon, new_env ); */
        
/*             nwamui_daemon_set_active_env(daemon, new_env ); */

/*             g_object_unref( G_OBJECT(new_env) ); */
/*             g_object_unref(daemon); */

        } else if (button == (gpointer)prv->location_rename_btn) {
#if 0
            gchar*  current_name;
            gchar*  new_name;
        
            current_name = nwamui_object_get_name(env);
        
            new_name = nwamui_util_rename_dialog_run(GTK_WINDOW(prv->location_dialog), _("Rename Location"), current_name );
        
            if ( new_name != NULL ) {
                nwamui_object_set_name(env, new_name);
                g_free(new_name);
            }

            g_free(current_name);
#endif
            GtkCellRendererText*        txt;
            GtkTreeViewColumn*  info_col = gtk_tree_view_get_column( GTK_TREE_VIEW(prv->location_tree), LOCVIEW_NAME );
            GList*              renderers = gtk_tree_view_column_get_cell_renderers( info_col );
        
            /* Should be only one renderer */
            g_assert( g_list_next( renderers ) == NULL );
        
            if ((txt = GTK_CELL_RENDERER_TEXT(g_list_first( renderers )->data)) != NULL) {
                GtkTreePath*    tpath = gtk_tree_model_get_path(model, &iter);
                g_object_set (txt, "editable", TRUE, NULL);

                gtk_tree_view_set_cursor (GTK_TREE_VIEW(prv->location_tree), tpath, info_col, TRUE);

                gtk_tree_path_free(tpath);
            }
        } else if (button == (gpointer)prv->location_rules_btn) {
            static NwamPrefIFace *rules_dialog = NULL;

            if (rules_dialog == NULL)
                rules_dialog = NWAM_PREF_IFACE(nwam_rules_dialog_new());

            nwam_pref_refresh(rules_dialog, NWAMUI_OBJECT(env), TRUE);
            capplet_dialog_run(rules_dialog, GTK_WIDGET(button));

        } else {
            g_assert_not_reached();
        }
        if (env) {
            g_object_unref(env);
        }
    }
}

static void
location_activation_combo_cell_cb(GtkCellLayout *cell_layout,
    GtkCellRenderer   *renderer,
    GtkTreeModel      *model,
    GtkTreeIter       *iter,
    gpointer           data)
{
    gint row_data;
	gtk_tree_model_get(model, iter, 0, &row_data, -1);
    g_object_set(G_OBJECT(renderer), "text", combo_contents[row_data], NULL);
}

static void 
location_activation_combo_changed_cb(GtkComboBox* combo, gpointer user_data)
{
    NwamLocationDialog*           self = NWAM_LOCATION_DIALOG(user_data);
    NwamLocationDialogPrivate*    prv = self->prv;
    GtkTreeModel*                 model = NULL;
    GtkTreeIter                   iter;

	if (gtk_combo_box_get_active_iter(combo, &iter)) {
        gint row_data;
		gtk_tree_model_get(gtk_combo_box_get_model(combo), &iter, 0, &row_data, -1);
		switch (row_data) {
        case 1:
            gtk_widget_set_sensitive(GTK_WIDGET(self->prv->location_rules_btn), TRUE);
            break;
        default:
            gtk_widget_set_sensitive(GTK_WIDGET(self->prv->location_rules_btn), FALSE);
            break;
        }
	}

    if ( gtk_tree_selection_get_selected(gtk_tree_view_get_selection(prv->location_tree), &model, &iter ) ) {
        NwamuiEnv                      *env;
        nwamui_cond_activation_mode_t   cond;

        gtk_tree_model_get(model, &iter, 0, &env, -1);
        
        cond = nwamui_object_get_activation_mode(NWAMUI_OBJECT(env));
        switch (gtk_combo_box_get_active(prv->location_activation_combo)) {
        case NWAMUI_LOC_ACTIVATION_MANUAL:
            if (cond != NWAMUI_COND_ACTIVATION_MODE_MANUAL) {
                nwamui_object_set_activation_mode(NWAMUI_OBJECT(env), NWAMUI_COND_ACTIVATION_MODE_MANUAL);
            }
            break;
        case NWAMUI_LOC_ACTIVATION_BY_RULES:
            if (cond != NWAMUI_COND_ACTIVATION_MODE_CONDITIONAL_ANY) {
                nwamui_object_set_activation_mode(NWAMUI_OBJECT(env), NWAMUI_COND_ACTIVATION_MODE_CONDITIONAL_ANY);
            }
            break;
        case NWAMUI_LOC_ACTIVATION_BY_SYSTEM:
            if (cond != NWAMUI_COND_ACTIVATION_MODE_SYSTEM) {
                nwamui_object_set_activation_mode(NWAMUI_OBJECT(env), NWAMUI_COND_ACTIVATION_MODE_SYSTEM);
            }
            break;
        default:
            g_assert_not_reached();
            break;
        }
    }
}

static void
response_cb(GtkWidget* widget, gint responseid, gpointer data)
{
	NwamLocationDialog         *self = NWAM_LOCATION_DIALOG(data);
	NwamLocationDialogPrivate  *prv = self->prv;
    gboolean                    stop_emission = FALSE;

	switch (responseid) {
		case GTK_RESPONSE_NONE:
			g_debug("GTK_RESPONSE_NONE");
			break;
		case GTK_RESPONSE_DELETE_EVENT:
			g_debug("GTK_RESPONSE_DELETE_EVENT");
			break;
		case GTK_RESPONSE_OK:
			g_debug("GTK_RESPONSE_OK");
            stop_emission = !nwam_pref_apply(NWAM_PREF_IFACE(data), NULL);
            if (!stop_emission) {
                gtk_widget_hide (GTK_WIDGET(prv->location_dialog));
            }
			break;
		case GTK_RESPONSE_CANCEL:
			g_debug("GTK_RESPONSE_CANCEL");
            nwam_pref_cancel(NWAM_PREF_IFACE(data), NULL);
			gtk_widget_hide (GTK_WIDGET(prv->location_dialog));
            stop_emission = FALSE;
			break;
		case GTK_RESPONSE_HELP:
			g_debug("GTK_RESPONSE_HELP");
            nwam_pref_help (NWAM_PREF_IFACE(data), NULL);
            stop_emission = TRUE;
			break;
	}
    if ( stop_emission ) {
        g_signal_stop_emission_by_name(widget, "response" );
    }
}

static void
object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
    g_debug("NwamLocationDialog: notify %s changed", arg1->name);
}

static gboolean
tree_model_foreach_find_enabled_env(GtkTreeModel *model,
  GtkTreePath *path,
  GtkTreeIter *iter,
  gpointer user_data)
{
	NwamuiEnv *env;

    gtk_tree_model_get( GTK_TREE_MODEL(model), iter, 0, &env, -1);
    if (env) {
        if (nwamui_env_get_enabled(env)) {
            nwamui_env_set_enabled(env, FALSE);

            g_object_unref(env);
            return TRUE;
        }
        g_object_unref(env);
    }
    return FALSE;
}
