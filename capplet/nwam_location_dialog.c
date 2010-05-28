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
#define LOCATION_DIALOG                 "nwam_location"
#define LOCATION_TREE                   "location_tree"
#define LOCATION_OK_BTN                 "location_ok_btn"
#define LOCATION_ADD_BTN                "location_add_btn"
#define LOCATION_REMOVE_BTN             "location_remove_btn"
#define LOCATION_RENAME_BTN             "location_rename_btn"
#define LOCATION_DUP_BTN                "location_dup_btn"
#define LOCATION_EDIT_BTN               "location_edit_btn"
#define LOCATION_ACTIVATION_COMBO       "location_activation_combo"
#define LOCATION_RULES_BTN              "location_rules_btn"
#define LOCATION_SWITCH_LOC_AUTO_CB     "switch_loc_auto_cb"
#define LOCATION_SWITCH_LOC_MANUALLY_CB "switch_loc_manually_cb"

#define TREEVIEW_COLUMN_NUM "meta:column"

struct _NwamLocationDialogPrivate {
	/* Widget Pointers */
    GtkDialog*          location_dialog;
	GtkTreeView*	    location_tree;
    GtkButton*          location_ok_btn;
    GtkButton*          location_add_btn;
    GtkButton*          location_remove_btn;
    GtkButton*          location_rename_btn;
    GtkButton*          location_dup_btn;
    GtkButton*          location_edit_btn;

    GtkComboBox*        location_activation_combo;
    GtkButton*          location_rules_btn;

    GtkRadioButton*     location_switch_loc_auto_cb;
    GtkRadioButton*     location_switch_loc_manually_cb;
    gboolean            switch_loc_manually_flag;
    NwamuiObject*       toggled_env;

	/* Other Data */
    NwamEnvPrefDialog*  env_pref_dialog;
    NwamuiDaemon       *daemon;
};

#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj),   \
	NWAM_TYPE_LOCATION_DIALOG, NwamLocationDialogPrivate)) 

enum {
    PROP_TOGGLED_ENV = 1,
};

enum {
	LOCVIEW_TOGGLE=0,
	LOCVIEW_MODE,
    LOCVIEW_NAME
};

typedef enum {
    NWAMUI_LOC_ACTIVATION_MANUAL = 0,
    NWAMUI_LOC_ACTIVATION_BY_RULES,
    NWAMUI_LOC_ACTIVATION_BY_SYSTEM,
    NWAMUI_LOC_ACTIVATION_LAST
} nwamui_loc_activation_mode_t;

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
static GtkWindow* dialog_get_window(NwamPrefIFace *iface);

static void nwam_location_dialog_finalize(NwamLocationDialog *self);

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
static void nwam_location_connection_toggled_cell_sensitive_func(GtkTreeViewColumn *col,
  GtkCellRenderer   *renderer,
  GtkTreeModel      *model,
  GtkTreeIter       *iter,
  gpointer           data);
static gint nwam_location_connection_compare_cb (GtkTreeModel *model,
			      GtkTreeIter *a,
			      GtkTreeIter *b,
			      gpointer user_data);

static void vanity_name_editing_started (GtkCellRenderer *cell,
                                         GtkCellEditable *editable,
                                         const gchar     *path,
                                         gpointer         data);
static void vanity_name_editing_canceled (  GtkCellRenderer     *cell,
                                            gpointer             data);

static void vanity_name_edited ( GtkCellRendererText *cell,
                                 const gchar         *path_string,
                                 const gchar         *new_text,
                                 gpointer             data);
static void nwam_location_connection_enabled_toggled_cb(    GtkCellRendererToggle *cell_renderer,
                                                            gchar                 *path,
                                                            gpointer               user_data);

static void nwam_treeview_update_widget_cb(GtkTreeSelection *selection, gpointer user_data);
static void on_button_clicked(GtkButton *button, gpointer user_data);
static void location_switch_loc_cb_toggled(GtkToggleButton *button, gpointer user_data);
static void location_activation_combo_cell_cb(GtkCellLayout *cell_layout,
  GtkCellRenderer   *renderer,
  GtkTreeModel      *model,
  GtkTreeIter       *iter,
  gpointer           data);
static void location_activation_combo_changed_cb(GtkComboBox* combo, gpointer user_data );

static gboolean activation_mode_filter_cb(  GtkTreeModel *model,
                                            GtkTreeIter *iter,
                                            gpointer data);

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
    iface->dialog_get_window = dialog_get_window;
}

static void
nwam_location_dialog_class_init(NwamLocationDialogClass *klass)
{
	/* Pointer to GObject Part of Class */
	GObjectClass *gobject_class = (GObjectClass*) klass;
    gobject_class->set_property = location_set_property;
    gobject_class->get_property = location_get_property;

    g_type_class_add_private(klass, sizeof(NwamLocationDialogPrivate));

    g_object_class_install_property (gobject_class,
      PROP_TOGGLED_ENV,
      g_param_spec_object ("toggled_env",
        _("Toggled env"),
        _("Toggled env"),
        NWAMUI_TYPE_OBJECT,
        G_PARAM_WRITABLE));

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
	NwamLocationDialogPrivate  *prv = GET_PRIVATE(self);

    switch (prop_id) {
    case PROP_TOGGLED_ENV:
        if (prv->toggled_env)
            g_object_unref(prv->toggled_env);

        prv->toggled_env = g_value_dup_object(value);
        break;
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
	cell = capplet_column_append_cell(col, gtk_cell_renderer_toggle_new(),
      FALSE,
      (GtkTreeCellDataFunc)nwam_location_connection_toggled_cell_sensitive_func, (gpointer)self, NULL);

	g_object_set (cell,
      "xalign", 0.5,
      "radio", TRUE,
      NULL );
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
	g_signal_connect (cell, "edited", G_CALLBACK(nwamui_object_name_cell_edited), (gpointer)view);
    g_signal_connect (cell, "edited", G_CALLBACK(vanity_name_edited), (gpointer)self);
    g_signal_connect (cell, "editing-started", G_CALLBACK(vanity_name_editing_started), (gpointer)self);
    g_signal_connect (cell, "editing-canceled", G_CALLBACK(vanity_name_editing_canceled), (gpointer)self);
	g_object_set (cell, "weight", PANGO_WEIGHT_BOLD, NULL);
	g_object_set_data (G_OBJECT (cell), TREEVIEW_COLUMN_NUM, GINT_TO_POINTER (LOCVIEW_NAME));

    /* Model */
    CAPPLET_COMPOSE_NWAMUI_OBJECT_LIST_VIEW(view);
}

static void
nwam_location_dialog_init(NwamLocationDialog *self)
{
	NwamLocationDialogPrivate  *prv = GET_PRIVATE(self);
    self->prv = prv;
	/* Iniialise pointers to important widgets */
    prv->daemon = nwamui_daemon_get_instance();
    prv->location_dialog = GTK_DIALOG(nwamui_util_glade_get_widget(LOCATION_DIALOG));
    capplet_remove_gtk_dialog_escape_binding(GTK_DIALOG_GET_CLASS(prv->location_dialog));

	prv->location_tree = GTK_TREE_VIEW(nwamui_util_glade_get_widget(LOCATION_TREE));
    prv->location_ok_btn = GTK_BUTTON(nwamui_util_glade_get_widget(LOCATION_OK_BTN));
    prv->location_add_btn = GTK_BUTTON(nwamui_util_glade_get_widget(LOCATION_ADD_BTN));
    prv->location_remove_btn = GTK_BUTTON(nwamui_util_glade_get_widget(LOCATION_REMOVE_BTN));
    prv->location_rename_btn = GTK_BUTTON(nwamui_util_glade_get_widget(LOCATION_RENAME_BTN));
    prv->location_dup_btn = GTK_BUTTON(nwamui_util_glade_get_widget(LOCATION_DUP_BTN));
    prv->location_edit_btn = GTK_BUTTON(nwamui_util_glade_get_widget(LOCATION_EDIT_BTN));

    prv->location_rules_btn = GTK_BUTTON(nwamui_util_glade_get_widget(LOCATION_RULES_BTN));

    prv->location_switch_loc_auto_cb = GTK_RADIO_BUTTON(nwamui_util_glade_get_widget(LOCATION_SWITCH_LOC_AUTO_CB));
    prv->location_switch_loc_manually_cb = GTK_RADIO_BUTTON(nwamui_util_glade_get_widget(LOCATION_SWITCH_LOC_MANUALLY_CB));

    /* Set title to include hostname */
    nwamui_util_window_title_append_hostname( prv->location_dialog );

    g_signal_connect(prv->location_rules_btn,
      "clicked", G_CALLBACK(on_button_clicked), (gpointer)self);
    g_signal_connect(prv->location_add_btn,
      "clicked", G_CALLBACK(on_button_clicked), (gpointer)self);
    g_signal_connect(prv->location_remove_btn,
      "clicked", G_CALLBACK(on_button_clicked), (gpointer)self);
    g_signal_connect(prv->location_rename_btn,
      "clicked", G_CALLBACK(on_button_clicked), (gpointer)self);
    g_signal_connect(prv->location_edit_btn,
      "clicked", G_CALLBACK(on_button_clicked), (gpointer)self);
    g_signal_connect(prv->location_dup_btn,
      "clicked", G_CALLBACK(on_button_clicked), (gpointer)self);

    g_signal_connect(prv->location_switch_loc_manually_cb,
      "clicked", G_CALLBACK(location_switch_loc_cb_toggled), (gpointer)self);
    g_signal_connect(prv->location_switch_loc_auto_cb,
      "clicked", G_CALLBACK(location_switch_loc_cb_toggled), (gpointer)self);

    g_signal_connect(prv->location_dialog,
      "response", G_CALLBACK(response_cb), (gpointer)self);

    prv->location_activation_combo = GTK_COMBO_BOX(nwamui_util_glade_get_widget(LOCATION_ACTIVATION_COMBO));
    capplet_compose_combo(prv->location_activation_combo,
      GTK_TYPE_LIST_STORE,
      G_TYPE_INT,
      location_activation_combo_cell_cb,
      NULL,
      G_CALLBACK(location_activation_combo_changed_cb),
      (gpointer)self,
      NULL);

    {
        GtkTreeModel      *filter;
        GtkTreeModel      *model;
        GtkTreeIter   iter;
        int i;

        model = gtk_combo_box_get_model(GTK_COMBO_BOX(prv->location_activation_combo));
        /* Clean all */
        gtk_list_store_clear(GTK_LIST_STORE(model));

        /* Add entries for each selection item */
        for (i = 0; i < NWAMUI_LOC_ACTIVATION_LAST; i++) {
            gtk_list_store_append(GTK_LIST_STORE(model), &iter);
            gtk_list_store_set (GTK_LIST_STORE(model), &iter, 0, i, -1);
        }

        /* Create a filter to hide system if activation mode is sensitive
         * system since it shouldn't be user-selectable.
         */
        filter = gtk_tree_model_filter_new (model, NULL);
        gtk_combo_box_set_model(GTK_COMBO_BOX(prv->location_activation_combo), 
                                GTK_TREE_MODEL (filter));
        gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER(filter),
                                                activation_mode_filter_cb,
                                                (gpointer)prv->location_activation_combo,
                                                NULL);

    }

    nwam_compose_tree_view(self);
    
	g_signal_connect(G_OBJECT(self), "notify", (GCallback)object_notify_cb, NULL);

    /* Initially refresh self */
    nwam_pref_refresh(NWAM_PREF_IFACE(self), NULL, TRUE);
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
        
        /* Ensure is OK is sensitive */
        gtk_widget_set_sensitive( GTK_WIDGET(self->prv->location_ok_btn), TRUE);

        response =  gtk_dialog_run(GTK_DIALOG(self->prv->location_dialog));

    }
    return(response);
}

static GtkWindow* 
dialog_get_window(NwamPrefIFace *iface)
{
    NwamLocationDialog*    self = NWAM_LOCATION_DIALOG( iface );

    if (self->prv->location_dialog) {
        return ( GTK_WINDOW(self->prv->location_dialog) );
    }

    return(NULL);
}

/**
 * refresh:
 *
 * Refresh #NwamLocationDialog with the new connections.
 **/
static gboolean
refresh(NwamPrefIFace *iface, gpointer user_data, gboolean force)
{
    NwamLocationDialog        *self = NWAM_LOCATION_DIALOG( iface );
	NwamLocationDialogPrivate *prv  = GET_PRIVATE(self);
    GtkButton                 *switch_cb;
    gboolean                   flag = nwamui_daemon_env_selection_is_manual(prv->daemon);
    
    g_debug("NwamLocationDialog: Refresh");
    g_assert(NWAM_IS_LOCATION_DIALOG(self));
    
    if (force) {
        gtk_widget_hide(GTK_WIDGET(self->prv->location_tree));
        capplet_update_model_from_daemon(gtk_tree_view_get_model(self->prv->location_tree), prv->daemon, NWAMUI_TYPE_ENV);
        gtk_widget_show(GTK_WIDGET(self->prv->location_tree));
    }

    if (flag) {
        switch_cb = GTK_BUTTON(prv->location_switch_loc_manually_cb);
    } else {
        switch_cb = GTK_BUTTON(prv->location_switch_loc_auto_cb);
    }
    gtk_button_clicked(switch_cb);

    nwam_treeview_update_widget_cb(gtk_tree_view_get_selection(self->prv->location_tree),
      (gpointer)self);

    return( TRUE );
}

static gboolean
cancel(NwamPrefIFace *iface, gpointer user_data)
{
    GtkTreeModel          *model;
    NwamLocationDialog    *self = NWAM_LOCATION_DIALOG( iface );

    /* Clean toggled_env after use it, because the next time show this dialog
     * may not show the correct active env.
     */
    g_object_set(self, "toggled_env", NULL, NULL);

    /* Re-read objects from system 
     */
	model = gtk_tree_view_get_model(self->prv->location_tree);
    gtk_tree_model_foreach(model,
      capplet_tree_model_foreach_nwamui_object_reload,
      NULL);

}

static gboolean
apply(NwamPrefIFace *iface, gpointer user_data)
{
    NwamLocationDialog*        self      = NWAM_LOCATION_DIALOG( iface );
	NwamLocationDialogPrivate *prv       = GET_PRIVATE(self);
	GtkTreeSelection          *selection = NULL;
	GtkTreeIter                iter;
    GtkTreePath               *path      = NULL;
    GtkTreeModel              *model;
    NwamuiObject              *obj;
    gboolean                   retval    = TRUE;

    model = gtk_tree_view_get_model(prv->location_tree);
    selection = gtk_tree_view_get_selection(prv->location_tree);

    ForeachNwamuiObjectCommitData data;
    data.failone = NULL;
    data.prop_name = NULL;
    gtk_tree_model_foreach(model, capplet_tree_model_foreach_nwamui_object_commit, &data);

    if (retval && data.failone) {
        retval = FALSE;
        if (data.prop_name) {
            gchar *msg = g_strdup_printf (_("Validation of %s failed with the property %s"), nwamui_object_get_name(NWAMUI_OBJECT(data.failone)), data.prop_name);

            nwamui_util_show_message (GTK_WINDOW(prv->location_dialog),
              GTK_MESSAGE_ERROR, _("Validation error"), msg, TRUE);

            g_free(msg);

            g_free(data.prop_name);
        } else {
            gchar *msg = g_strdup_printf (_("Committing %s failed..."), nwamui_object_get_name(NWAMUI_OBJECT(data.failone)));

            nwamui_util_show_message (GTK_WINDOW(prv->location_dialog),
              GTK_MESSAGE_ERROR, _("Commit ENM error"), msg, TRUE);

            g_free(msg);
        }
        /* Start highlight relevant ENM */
        gtk_tree_selection_select_iter(selection, &data.iter);

        g_object_unref(data.failone);
    }

    if (prv->switch_loc_manually_flag) {
		nwamui_object_set_active(NWAMUI_OBJECT(prv->toggled_env), TRUE);
    } else {
        /* Get the current active env, disactive and wait for nwamd. */
        NwamuiEnv *env = nwamui_daemon_get_active_env(prv->daemon);
        nwamui_object_set_active(NWAMUI_OBJECT(env), FALSE);
        g_object_unref(env);
    }
    /* Clean toggled_env after use it, because the next time show this dialog
     * may not show the correct active env.
     */
    g_object_set(self, "toggled_env", NULL, NULL);

    return retval;
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
	NwamLocationDialogPrivate *prv       = GET_PRIVATE(self);
    g_object_unref(G_OBJECT(self->prv->location_add_btn));	
    g_object_unref(G_OBJECT(self->prv->location_remove_btn));	
    g_object_unref(G_OBJECT(self->prv->location_rename_btn));	

    if (prv->toggled_env)
        g_object_unref(prv->toggled_env);

    g_object_unref(prv->daemon);

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
	NwamLocationDialogPrivate *prv       = GET_PRIVATE(self);
    GtkTreeIter iter;
    GtkTreeModel *model;
    GtkTreePath  *tpath;

	model = gtk_tree_view_get_model(prv->location_tree);
    tpath = gtk_tree_path_new_from_string(path);
    if (tpath != NULL && gtk_tree_model_get_iter (model, &iter, tpath))
    {
        NwamuiEnv*  env;

        gtk_tree_model_get(model, &iter, 0, &env, -1);

        g_object_set(self, "toggled_env", env, NULL);
        /* Refresh the treeview anyway. */
        gtk_widget_hide(GTK_WIDGET(prv->location_tree));
        gtk_widget_show(GTK_WIDGET(prv->location_tree));

        if (env) {
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

            gtk_tree_model_get(model, &iter, 0, &connection, -1);

            env  = NWAMUI_ENV( connection );

            gtk_entry_set_text( entry, nwamui_object_get_name(NWAMUI_OBJECT(env)));

            g_object_unref(G_OBJECT(connection));

            gtk_tree_path_free(tpath);
        }
        gtk_widget_set_sensitive( GTK_WIDGET(self->prv->location_ok_btn), FALSE);
    }
}

static void 
vanity_name_editing_canceled ( GtkCellRenderer  *cell,
                               gpointer          data)
{
	NwamLocationDialog* self = NWAM_LOCATION_DIALOG(data);

    gtk_widget_set_sensitive( GTK_WIDGET(self->prv->location_ok_btn), TRUE);
}

static void
vanity_name_edited ( GtkCellRendererText *cell,
                     const gchar         *path_string,
                     const gchar         *new_text,
                     gpointer             data)
{
	NwamLocationDialog* self = NWAM_LOCATION_DIALOG(data);

    g_object_set (cell, "editable", FALSE, NULL);

    gtk_widget_set_sensitive( GTK_WIDGET(self->prv->location_ok_btn), TRUE);
}


static void
nwam_treeview_update_widget_cb(GtkTreeSelection *selection, gpointer user_data)
{
    NwamLocationDialog*         self   = NWAM_LOCATION_DIALOG(user_data);
    NwamLocationDialogPrivate*  prv    = GET_PRIVATE(self);
    GtkTreeModel*               model;
    GtkTreeIter                 iter;
    gint                        count_selected_rows;

    count_selected_rows = gtk_tree_selection_count_selected_rows(selection);

    gtk_widget_set_sensitive(GTK_WIDGET(prv->location_edit_btn), count_selected_rows > 0 ? TRUE : FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(prv->location_dup_btn), count_selected_rows > 0 ? TRUE : FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(prv->location_rename_btn), count_selected_rows > 0 ? TRUE : FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(prv->location_remove_btn), count_selected_rows > 0 ? TRUE : FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(prv->location_activation_combo), count_selected_rows > 0 ? TRUE : FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(prv->location_rules_btn), FALSE);

    if ( gtk_tree_selection_get_selected( selection, &model, &iter ) ) {
        nwamui_cond_activation_mode_t   cond;
        NwamuiEnv                      *env;

        gtk_tree_model_get(model, &iter, 0, &env, -1);
        
        g_signal_handlers_block_by_func(G_OBJECT(prv->location_activation_combo), 
                                        (gpointer)location_activation_combo_changed_cb, (gpointer)self);

        cond = nwamui_object_get_activation_mode(NWAMUI_OBJECT(env));

        gtk_widget_set_sensitive(GTK_WIDGET(prv->location_activation_combo), 
                                            cond != NWAMUI_COND_ACTIVATION_MODE_SYSTEM);

        gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER(
                    gtk_combo_box_get_model(GTK_COMBO_BOX(prv->location_activation_combo))));

        switch (cond) {
        case NWAMUI_COND_ACTIVATION_MODE_MANUAL:
            gtk_combo_box_set_active(prv->location_activation_combo, NWAMUI_LOC_ACTIVATION_MANUAL);
            gtk_widget_set_sensitive(GTK_WIDGET(prv->location_remove_btn),
                                     !nwamui_object_get_active(NWAMUI_OBJECT(env)));
            break;
        case NWAMUI_COND_ACTIVATION_MODE_CONDITIONAL_ANY:
        case NWAMUI_COND_ACTIVATION_MODE_CONDITIONAL_ALL:
            gtk_combo_box_set_active(prv->location_activation_combo, NWAMUI_LOC_ACTIVATION_BY_RULES );
            gtk_widget_set_sensitive(GTK_WIDGET(self->prv->location_rules_btn), TRUE);
            gtk_widget_set_sensitive(GTK_WIDGET(prv->location_remove_btn),
                                     !nwamui_object_get_active(NWAMUI_OBJECT(env)));
            break;
        case NWAMUI_COND_ACTIVATION_MODE_SYSTEM:
            /* If Activation Mode is system, then you can't rename or remove
             * the selected location.
             */
            gtk_widget_set_sensitive(GTK_WIDGET(prv->location_rename_btn), FALSE );
            gtk_widget_set_sensitive(GTK_WIDGET(prv->location_remove_btn), FALSE );
            /* Fall-through */
        case NWAMUI_COND_ACTIVATION_MODE_PRIORITIZED: /* Not supported, so just assume default if see it. */
        default:
            gtk_widget_set_sensitive(GTK_WIDGET(prv->location_remove_btn), FALSE);
            gtk_combo_box_set_active(prv->location_activation_combo, NWAMUI_LOC_ACTIVATION_BY_SYSTEM);
            break;
        }


        g_signal_handlers_unblock_by_func(G_OBJECT(prv->location_activation_combo), 
                                        (gpointer)location_activation_combo_changed_cb, (gpointer)self);

        g_object_unref(env);
    }

}

static void
on_button_clicked(GtkButton *button, gpointer user_data)
{
    NwamLocationDialog         *self = NWAM_LOCATION_DIALOG(user_data);
    NwamLocationDialogPrivate  *prv = GET_PRIVATE(self);
    GtkTreeModel*               model;
    GtkTreeIter                 iter;

    
    if (button == (gpointer)prv->location_add_btn) {
        gchar *name;
        NwamuiObject *object;

        name = capplet_get_increasable_name(gtk_tree_view_get_model(GTK_TREE_VIEW(prv->location_tree)), _("Unnamed Location"), G_OBJECT(self));

        g_assert(name);

#ifdef USE_DIALOG_FOR_NAME
        {
            gchar *new_name;

            new_name = nwamui_util_rename_dialog_run(GTK_WINDOW(prv->location_dialog), _("Location Name"), name );
        
            if ( new_name != NULL ) {
                g_free(name);
                name = new_name;
                new_name = NULL;
            }
        }
#endif /* USE_DIALOG_FOR_NAME */

        object = NWAMUI_OBJECT(nwamui_env_new(name) );
        CAPPLET_LIST_STORE_ADD(GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(prv->location_tree))), object);
        g_free(name);
        g_object_unref(object);

        /* Select and scroll to this new object. */
        gtk_tree_view_scroll_to_cell(prv->location_tree,
          nwam_tree_view_get_cached_object_path(NWAM_TREE_VIEW(prv->location_tree)),
          NULL, FALSE, 0.0, 0.0);
        gtk_tree_selection_select_path(gtk_tree_view_get_selection(prv->location_tree),
          nwam_tree_view_get_cached_object_path(NWAM_TREE_VIEW(prv->location_tree)));
#ifndef USE_DIALOG_FOR_NAME
        /* Trigger rename on the new object as spec defines. */
        gtk_button_clicked(prv->location_rename_btn);
#endif /* ! USE_DIALOG_FOR_NAME */
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
            nwam_pref_dialog_run(NWAM_PREF_IFACE(env_pref_dialog), GTK_WIDGET(button));

        } else if (button == (gpointer)prv->location_remove_btn) {
            gchar*  message = g_strdup_printf(_("Remove location '%s'?"),
              nwamui_object_get_name(NWAMUI_OBJECT(env)));
            if (nwamui_util_confirm_removal( GTK_WINDOW(prv->location_dialog), _("Remove Location?"), message )) {
                g_debug("Removing location: '%s'", nwamui_object_get_name(NWAMUI_OBJECT(env)));
            
                if (env == (gpointer)prv->toggled_env) {
                    /* Clear toggled-env, otherwise the toggled flag will be lost. */
                    g_object_set(self, "toggled_env", NULL, NULL);
                }

                gtk_list_store_remove(GTK_LIST_STORE(model), &iter);

                nwamui_object_destroy(NWAMUI_OBJECT(env));
            }
        
            g_free(message);
        } else if (button == (gpointer)prv->location_dup_btn) {
            const gchar *sname = nwamui_object_get_name(NWAMUI_OBJECT(env));
            gchar *prefix;
            gchar *name;
            NwamuiObject *object;

            prefix = capplet_get_original_name(_("Copy of"), sname);

            name = capplet_get_increasable_name(gtk_tree_view_get_model(GTK_TREE_VIEW(prv->location_tree)), prefix, G_OBJECT(env));

            g_assert(name);

            object = nwamui_object_clone(NWAMUI_OBJECT(env), name, NWAMUI_OBJECT(prv->daemon));
            if (object) {
                /* We must add it to view first instead of relying on deamon
                 * property changes, because we have to select the new added
                 * row immediately and trigger a renaming mode.
                 */
                CAPPLET_LIST_STORE_ADD(GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(prv->location_tree))), object);

                /* Select and scroll to this new object. */
                gtk_tree_view_scroll_to_cell(prv->location_tree,
                  nwam_tree_view_get_cached_object_path(NWAM_TREE_VIEW(prv->location_tree)),
                  NULL, FALSE, 0.0, 0.0);
                gtk_tree_selection_select_path(gtk_tree_view_get_selection(prv->location_tree),
                  nwam_tree_view_get_cached_object_path(NWAM_TREE_VIEW(prv->location_tree)));
                /* Trigger rename on the new object as spec defines. */
                gtk_button_clicked(prv->location_rename_btn);

                g_object_unref(object);
            }

            g_free(name);
            g_free(prefix);

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

#endif
            if ( nwamui_object_can_rename(NWAMUI_OBJECT(env)) ) {
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
            }
            else {
                if (nwamui_util_ask_about_dup_obj( GTK_WINDOW(prv->location_dialog), NWAMUI_OBJECT(env) )) {
                    gtk_button_clicked(prv->location_dup_btn);
                }
            }


        } else if (button == (gpointer)prv->location_rules_btn) {
            NwamPrefIFace *rules_dialog = NWAM_PREF_IFACE(nwam_rules_dialog_new());
            nwam_pref_refresh(rules_dialog, NWAMUI_OBJECT(env), TRUE);
            nwam_pref_dialog_run(rules_dialog, GTK_WIDGET(button));
            g_object_unref(rules_dialog);
            /* Update the select row, since the activation may changed. */
            nwam_treeview_update_widget_cb(gtk_tree_view_get_selection(prv->location_tree), (gpointer)self);
        } else {
            g_assert_not_reached();
        }
        if (env) {
            g_object_unref(env);
        }
    }
}

static void
location_switch_loc_cb_toggled(GtkToggleButton *button, gpointer user_data)
{
    NwamLocationDialog*         self   = NWAM_LOCATION_DIALOG(user_data);
    NwamLocationDialogPrivate*  prv    = GET_PRIVATE(self);

    if (button == GTK_TOGGLE_BUTTON(prv->location_switch_loc_manually_cb)) {
        prv->switch_loc_manually_flag = TRUE;
    } else {
        prv->switch_loc_manually_flag = FALSE;
    }

    /* Refresh the treeview anyway. */
    gtk_widget_hide(GTK_WIDGET(prv->location_tree));
    gtk_widget_show(GTK_WIDGET(prv->location_tree));
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
    NwamLocationDialogPrivate*    prv = GET_PRIVATE(self);
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
        NwamuiObject                  *obj;
        nwamui_cond_activation_mode_t  cond;

        gtk_tree_model_get(model, &iter, 0, &obj, -1);
        
        cond = nwamui_object_get_activation_mode(obj);
        switch (gtk_combo_box_get_active(prv->location_activation_combo)) {
        case NWAMUI_LOC_ACTIVATION_MANUAL:
            if (cond != NWAMUI_COND_ACTIVATION_MODE_MANUAL) {
                nwamui_object_set_activation_mode(obj, NWAMUI_COND_ACTIVATION_MODE_MANUAL);
            }
            break;
        case NWAMUI_LOC_ACTIVATION_BY_RULES:
            /* Default set to condition any when changing from others. */
            if (cond != NWAMUI_COND_ACTIVATION_MODE_CONDITIONAL_ANY) {
                nwamui_object_set_activation_mode(obj, NWAMUI_COND_ACTIVATION_MODE_CONDITIONAL_ANY);
            }
            break;
        case NWAMUI_LOC_ACTIVATION_BY_SYSTEM:
            if (cond != NWAMUI_COND_ACTIVATION_MODE_SYSTEM) {
                nwamui_object_set_activation_mode(obj, NWAMUI_COND_ACTIVATION_MODE_SYSTEM);
            }
            break;
        default:
            g_assert_not_reached();
            break;
        }
        g_object_unref(obj);
    }
}

static void
response_cb(GtkWidget* widget, gint responseid, gpointer data)
{
	NwamLocationDialog         *self = NWAM_LOCATION_DIALOG(data);
	NwamLocationDialogPrivate  *prv = GET_PRIVATE(self);
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

static void
nwam_location_connection_toggled_cell_sensitive_func(GtkTreeViewColumn *col,
  GtkCellRenderer   *renderer,
  GtkTreeModel      *model,
  GtkTreeIter       *iter,
  gpointer           data)
{
	NwamLocationDialog         *self = NWAM_LOCATION_DIALOG(data);
	NwamLocationDialogPrivate  *prv = GET_PRIVATE(data);

    NwamuiObject*              object = NULL;
    
    gtk_tree_model_get(model, iter, 0, &object, -1);
    
    if (object) {
        if (!prv->toggled_env) {
            g_object_set(G_OBJECT(renderer),
              "active", nwamui_object_get_active(NWAMUI_OBJECT(object)),
              NULL);
        } else if (object == prv->toggled_env) {
            /* Show active, commit later. */
            g_object_set(G_OBJECT(renderer),
              "active", TRUE,
              NULL);
        } else {
            g_object_set(G_OBJECT(renderer),
              "active", FALSE,
              NULL); 
        }
        g_object_unref(G_OBJECT(object));
    } else {
        g_object_set(G_OBJECT(renderer),
          "active", FALSE,
          NULL); 
    }

    g_object_set(G_OBJECT(renderer),
/*       "sensitive", prv->switch_loc_manually_flag, */
      "activatable", prv->switch_loc_manually_flag,
      NULL); 
}

static gboolean
activation_mode_filter_cb(GtkTreeModel *model,
  GtkTreeIter *iter,
  gpointer data)
{
    GtkWidget       *combo = GTK_WIDGET(data);
    NwamuiObject    *obj = NULL;
    gint             active_index;
    nwamui_loc_activation_mode_t    mode;

    gtk_tree_model_get(model, iter, 0, &mode, -1);

    if ( (mode == NWAMUI_LOC_ACTIVATION_BY_SYSTEM) && 
         GTK_WIDGET_IS_SENSITIVE( combo ) ) {
        return( FALSE );
    }
    else {
        return( TRUE );
    } 
}

