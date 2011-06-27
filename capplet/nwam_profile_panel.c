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
#include "nwam_pref_iface.h"
#include "nwam_profile_panel.h"
#include "nwam_profile_dialog.h"
#include "capplet-utils.h"
#include "nwam_tree_view.h"

/* Names of Widgets in Glade file */
#define OBJECT_VIEW             "profile_tree"
#define PROFILE_ADD_BTN         "profile_add_btn"
#define PROFILE_REMOVE_BTN      "profile_remove_btn"
#define PROFILE_RENAME_BTN      "profile_rename_btn"
#define PROFILE_DUP_BTN         "profile_dup_btn"
#define PROFILE_EDIT_BTN        "profile_edit_btn"
#define SELECTED_PROFILE_LBL    "selected_profile_lbl"
#define ALWAYS_ENABLED_LBL      "always_enabled_lbl"
#define PRIORITY_GROUPS_LBL     "priority_groups_lbl"
#define ALWAYS_DISABLED_LBL     "always_disabled_lbl"
#define CONNECTION_PROFILE_VIEW "connection_profile_view"

#define TREEVIEW_COLUMN_NUM "meta:column"

struct _NwamProfilePanelPrivate {
	/* Widget Pointers */
	GtkTreeView* object_view;
    GtkButton*   profile_add_btn;
    GtkButton*   profile_remove_btn;
    GtkButton*   profile_rename_btn;
    GtkButton*   profile_dup_btn;
    GtkButton*   profile_edit_btn;

    GtkWidget* selected_profile_lbl;
    GtkWidget* priority_groups_lbl;
    GtkWidget* always_enabled_lbl;
    GtkWidget* always_disabled_lbl;

    GtkTreeRowReference *toggled_row;

    /* NCUs info of a NCP */
    gint   always_enabled;
    gint   always_disabled;
    GList *groups;
    gchar *first_enabled_name;
    gchar *first_disabled_name;
    
	/* Other Data */
    NwamCappletDialog *pref_dialog;
    NwamuiDaemon      *daemon;
};

#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj),   \
	NWAM_TYPE_PROFILE_PANEL, NwamProfilePanelPrivate)) 

enum {
    PROP_TOGGLED_OBJECT = 1,
};

enum {
	LOCVIEW_TOGGLE=0,
    LOCVIEW_NAME
};

static void nwam_pref_init (gpointer g_iface, gpointer iface_data);
static void nwam_object_ctrl_init(gpointer g_iface, gpointer iface_data);
static gboolean refresh(NwamPrefIFace *iface, gpointer user_data, gboolean force);
static gboolean apply(NwamPrefIFace *iface, gpointer user_data);
static gboolean cancel(NwamPrefIFace *iface, gpointer user_data);
static gboolean help(NwamPrefIFace *iface, gpointer user_data);

static void nwam_profile_panel_finalize(NwamProfilePanel *self);

static void nwam_profile_panel_set_property (GObject         *object,
  guint            prop_id,
  const GValue    *value,
  GParamSpec      *pspec);

static void nwam_profile_panel_get_property (GObject         *object,
  guint            prop_id,
  GValue          *value,
  GParamSpec      *pspec);

static gboolean nwam_profile_panel_is_toggled_row(NwamProfilePanel *self, GtkTreePath *path);
static void nwam_profile_panel_set_toggled_row(NwamProfilePanel *self, GtkTreePath *path);

/* Callbacks */
static void object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);

static void nwam_object_toggled_cell_sensitive_func(GtkTreeViewColumn *col,
  GtkCellRenderer   *renderer,
  GtkTreeModel      *model,
  GtkTreeIter       *iter,
  gpointer           data);
static gint nwam_object_model_compare_cb(GtkTreeModel *model,
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

static void nwamui_ncp_name_cell_edited( GtkCellRendererText *cell,
  const gchar         *path_string,
  const gchar         *new_text,
  gpointer             data);

static void nwam_treeview_update_widget_cb(GtkTreeSelection *selection, gpointer user_data);
static void nwam_object_activate_toggled_cb(GtkCellRendererToggle *cell_renderer,
  gchar                 *path,
  gpointer               user_data);

/* NCP functions */

/* NCU functions */
static void ncp_foreach_ncu_get_info(gpointer data, gpointer user_data);

G_DEFINE_TYPE_EXTENDED (NwamProfilePanel,
  nwam_profile_panel,
  G_TYPE_OBJECT,
  0,
  G_IMPLEMENT_INTERFACE(NWAM_TYPE_PREF_IFACE, nwam_pref_init)
  G_IMPLEMENT_INTERFACE(NWAM_TYPE_OBJECT_CTRL_IFACE, nwam_object_ctrl_init))

static void
nwam_pref_init (gpointer g_iface, gpointer iface_data)
{
	NwamPrefInterface *iface = (NwamPrefInterface *)g_iface;
	iface->refresh = refresh;
	iface->apply = apply;
	iface->cancel = cancel;
	iface->help = help;
}

static GObject*
create_object(NwamObjectCtrlIFace *iface)
{
    NwamProfilePanel        *self = NWAM_PROFILE_PANEL(iface);
	NwamProfilePanelPrivate *prv  = GET_PRIVATE(self);
    gchar *name;
    NwamuiObject *object;

    name = capplet_get_increasable_name(gtk_tree_view_get_model(GTK_TREE_VIEW(prv->object_view)), _("User"), G_OBJECT(self));

    g_assert(name);

    object = nwamui_ncp_new(name);

    if (nwamui_object_commit(object)) {
        nwamui_object_add(NWAMUI_OBJECT(prv->daemon), object);
    }

    g_free(name);

    return G_OBJECT(object);
}

static gboolean
remove_object(NwamObjectCtrlIFace *iface, GObject *obj)
{
    NwamProfilePanel        *self = NWAM_PROFILE_PANEL(iface);
	NwamProfilePanelPrivate *prv  = GET_PRIVATE(self);
    gboolean                 ret  = FALSE;
    GtkTreeModel            *model;
    GtkTreeIter              iter;
    gchar*  message = g_strdup_printf(_("Remove network profile: '%s'?"), nwamui_object_get_name(NWAMUI_OBJECT(obj)));

    gtk_tree_selection_get_selected(gtk_tree_view_get_selection(prv->object_view), &model, &iter);

    if (nwamui_util_confirm_removal(nwam_pref_dialog_get_window(NWAM_PREF_IFACE(prv->pref_dialog)), _("Remove network profile?"), message )) {
        GtkTreePath *selected_path = gtk_tree_model_get_path(model, &iter);

        if (nwam_profile_panel_is_toggled_row(self, selected_path)) {
            /* Clear toggled-env, otherwise the toggled flag will be lost. */
            /* g_object_set(self, "toggled_object", NULL, NULL); */
            nwam_profile_panel_set_toggled_row(self, NULL);
        }
        gtk_tree_path_free(selected_path);

        nwamui_object_destroy(NWAMUI_OBJECT(obj));
        /* Must remove it, because it currectly may haven't been
         * created. So destroy will not trigger nwamui_object_remove.
         */
        nwamui_object_remove(NWAMUI_OBJECT(prv->daemon), NWAMUI_OBJECT(obj));
        ret = TRUE;
    }
        
    g_free(message);

    return ret;
}

static gboolean
rename_object(NwamObjectCtrlIFace *iface, GObject *obj)
{
    NwamProfilePanel        *self = NWAM_PROFILE_PANEL(iface);
	NwamProfilePanelPrivate *prv  = GET_PRIVATE(self);

    /* We enable rename even NCP can't be renamed. The solution is if
     * the NCP can't be renamed, we can dup it with the new name after
     * user finish editing its name, destroy the old NCP, and commit
     * the dup NCP, and replace the tree row with the new NCP.
     */
    return TRUE;
}

static void
edit_object(NwamObjectCtrlIFace *iface, GObject *obj)
{
    NwamProfilePanel        *self = NWAM_PROFILE_PANEL(iface);
	NwamProfilePanelPrivate *prv  = GET_PRIVATE(self);
    static NwamProfileDialog *profile_dialog = NULL;

    if (profile_dialog == NULL)
        profile_dialog = nwam_profile_dialog_new(prv->pref_dialog);

    if (nwamui_object_can_rename(NWAMUI_OBJECT(obj))) {
        /* NCP is not created yet. Commit(which is actually created)
         * and add to UI daemon to reuse it.
         */
        if (nwamui_object_commit(NWAMUI_OBJECT(obj))) {
            nwamui_object_add(NWAMUI_OBJECT(prv->daemon), NWAMUI_OBJECT(obj));
        }
    }
    /* Reload before edit it. */
    nwamui_object_reload(NWAMUI_OBJECT(obj));

    nwam_pref_set_purpose(NWAM_PREF_IFACE(profile_dialog), NWAMUI_DIALOG_PURPOSE_EDIT);
    nwam_pref_refresh(NWAM_PREF_IFACE(profile_dialog), obj, TRUE);
    nwam_pref_dialog_run(NWAM_PREF_IFACE(profile_dialog), GTK_WIDGET(prv->object_view));
    /* Update info widgets. */
    nwam_treeview_update_widget_cb(gtk_tree_view_get_selection(prv->object_view), (gpointer)self);
}

static GObject*
dup_object(NwamObjectCtrlIFace *iface, GObject *obj)
{
    NwamProfilePanel        *self = NWAM_PROFILE_PANEL(iface);
	NwamProfilePanelPrivate *prv  = GET_PRIVATE(self);
    const gchar *sname = nwamui_object_get_name(NWAMUI_OBJECT(obj));
    gchar *prefix;
    gchar *name;
    NwamuiObject *new_ncp;

    prefix = capplet_get_original_name(_("Copy of"), nwamui_object_get_name(NWAMUI_OBJECT(obj)));

    name = capplet_get_increasable_name(gtk_tree_view_get_model(GTK_TREE_VIEW(prv->object_view)), prefix, G_OBJECT(obj));

    g_assert(name);

    new_ncp = nwamui_object_clone(NWAMUI_OBJECT(obj), name, NULL);

    if (nwamui_object_commit(new_ncp)) {
        nwamui_object_add(NWAMUI_OBJECT(prv->daemon), new_ncp);
    }

    g_free(name);
    g_free(prefix);

    return G_OBJECT(new_ncp);
}

static void
nwam_object_ctrl_init(gpointer g_iface, gpointer iface_data)
{
    NwamObjectCtrlInterface *self = (NwamObjectCtrlInterface *)g_iface;

	self->create = create_object;
	self->remove = remove_object;
	self->rename = rename_object;
	self->edit = edit_object;
	self->dup = dup_object;
}

static void
nwam_profile_panel_class_init(NwamProfilePanelClass *klass)
{
	/* Pointer to GObject Part of Class */
	GObjectClass *gobject_class = (GObjectClass*) klass;
    gobject_class->set_property = nwam_profile_panel_set_property;
    gobject_class->get_property = nwam_profile_panel_get_property;

    g_type_class_add_private(klass, sizeof(NwamProfilePanelPrivate));

    g_object_class_install_property (gobject_class,
      PROP_TOGGLED_OBJECT,
      g_param_spec_object ("toggled_object",
        _("Toggled object"),
        _("Toggled object"),
        NWAMUI_TYPE_OBJECT,
        G_PARAM_WRITABLE));

	/* Override Some Function Pointers */
	gobject_class->finalize = (void (*)(GObject*)) nwam_profile_panel_finalize;

}

static void
nwam_profile_panel_set_property( GObject         *object,
  guint            prop_id,
  const GValue    *value,
  GParamSpec      *pspec)
{
    NwamProfilePanel*       self = NWAM_PROFILE_PANEL(object);
	NwamProfilePanelPrivate  *prv = GET_PRIVATE(self);

    switch (prop_id) {
    case PROP_TOGGLED_OBJECT:
        break;
    default: 
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
nwam_profile_panel_get_property( GObject         *object,
  guint            prop_id,
  GValue          *value,
  GParamSpec      *pspec)
{
    NwamProfilePanel*       self = NWAM_PROFILE_PANEL(object);

    switch (prop_id) {
    default: 
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static gboolean
nwam_profile_panel_is_toggled_row(NwamProfilePanel *self, GtkTreePath *path)
{
    NwamProfilePanelPrivate *prv   = GET_PRIVATE(self);
    GtkTreeModel            *model = gtk_tree_view_get_model(prv->object_view);

    if (gtk_tree_row_reference_valid(prv->toggled_row)) {
        GtkTreePath *cur = gtk_tree_row_reference_get_path(prv->toggled_row);

        if (gtk_tree_path_compare(cur, path) == 0) {
            return TRUE;
        }
    } else {
        /* Invalid? clear it. */
        gtk_tree_row_reference_free(prv->toggled_row);
        prv->toggled_row = NULL;
    }

    return FALSE;
}

static void
nwam_profile_panel_set_toggled_row(NwamProfilePanel *self, GtkTreePath *path)
{
    NwamProfilePanelPrivate *prv      = GET_PRIVATE(self);
    GtkTreeModel            *model    = gtk_tree_view_get_model(prv->object_view);

    if (prv->toggled_row) {
        gtk_tree_row_reference_free(prv->toggled_row);
        prv->toggled_row = NULL;
    }

    if (path) {
        prv->toggled_row = gtk_tree_row_reference_new(model, path);
    }
}

static void
nwam_compose_tree_view (NwamProfilePanel *self)
{
    NwamProfilePanelPrivate *prv      = GET_PRIVATE(self);
	GtkTreeViewColumn *col;
	GtkCellRenderer *cell;
	GtkTreeView *view = prv->object_view;
        
	g_object_set (G_OBJECT(view),
      "nwam_object_ctrl", self,
      "headers-clickable", TRUE,
      "headers-visible", TRUE,
      "rules-hint", TRUE,
      "reorderable", FALSE,
      "enable-search", TRUE,
      "show-expanders", FALSE,
      NULL);

    nwam_tree_view_register_widget(NWAM_TREE_VIEW(view), NTV_ADD_BTN, GTK_WIDGET(prv->profile_add_btn));
    nwam_tree_view_register_widget(NWAM_TREE_VIEW(view), NTV_REMOVE_BTN, GTK_WIDGET(prv->profile_remove_btn));
    nwam_tree_view_register_widget(NWAM_TREE_VIEW(view), NTV_RENAME_BTN, GTK_WIDGET(prv->profile_rename_btn));
    nwam_tree_view_register_widget(NWAM_TREE_VIEW(view), NTV_EDIT_BTN, GTK_WIDGET(prv->profile_edit_btn));
    nwam_tree_view_register_widget(NWAM_TREE_VIEW(view), NTV_DUP_BTN, GTK_WIDGET(prv->profile_dup_btn));

    g_signal_connect(gtk_tree_view_get_selection(view),
      "changed",
      G_CALLBACK(nwam_treeview_update_widget_cb),
      (gpointer)self);

    /* Toggle column */
	col = capplet_column_new(view, NULL);
	cell = capplet_column_append_cell(col, gtk_cell_renderer_toggle_new(),
      FALSE,
      (GtkTreeCellDataFunc)nwam_object_toggled_cell_sensitive_func, (gpointer)self, NULL);

	g_object_set (cell,
      "xalign", 0.5,
      "radio", TRUE,
      NULL );
    g_signal_connect(G_OBJECT(cell), "toggled", G_CALLBACK(nwam_object_activate_toggled_cb), (gpointer)self);

    /* Name column */
	col = capplet_column_new(view,
      "title", _("Name"),
      "expand", FALSE,
      "resizable", TRUE,
      NULL);
	cell = capplet_column_append_cell(col, gtk_cell_renderer_text_new(), FALSE,
      (GtkTreeCellDataFunc)nwamui_object_name_cell, NULL, NULL);
	g_signal_connect (cell, "edited", G_CALLBACK(nwamui_ncp_name_cell_edited), (gpointer)self);
    g_signal_connect (cell, "edited", G_CALLBACK(vanity_name_edited), (gpointer)self);
    g_signal_connect (cell, "editing-started", G_CALLBACK(vanity_name_editing_started), (gpointer)self);
    g_signal_connect (cell, "editing-canceled", G_CALLBACK(vanity_name_editing_canceled), (gpointer)self);
	g_object_set (cell, "weight", PANGO_WEIGHT_BOLD, NULL);

    /* Model */
    CAPPLET_COMPOSE_NWAMUI_OBJECT_LIST_VIEW(view);
}

static void
nwam_profile_panel_init(NwamProfilePanel *self)
{
	NwamProfilePanelPrivate  *prv = GET_PRIVATE(self);
    self->prv = prv;
	/* Iniialise pointers to important widgets */
    prv->daemon = nwamui_daemon_get_instance();
    prv->profile_add_btn = GTK_BUTTON(nwamui_util_glade_get_widget(PROFILE_ADD_BTN));
    prv->profile_remove_btn = GTK_BUTTON(nwamui_util_glade_get_widget(PROFILE_REMOVE_BTN));
    prv->profile_rename_btn = GTK_BUTTON(nwamui_util_glade_get_widget(PROFILE_RENAME_BTN));
    prv->profile_dup_btn = GTK_BUTTON(nwamui_util_glade_get_widget(PROFILE_DUP_BTN));
    prv->profile_edit_btn = GTK_BUTTON(nwamui_util_glade_get_widget(PROFILE_EDIT_BTN));
    prv->selected_profile_lbl = nwamui_util_glade_get_widget(SELECTED_PROFILE_LBL);
    prv->priority_groups_lbl = nwamui_util_glade_get_widget(PRIORITY_GROUPS_LBL);
    prv->always_enabled_lbl = nwamui_util_glade_get_widget(ALWAYS_ENABLED_LBL);
    prv->always_disabled_lbl = nwamui_util_glade_get_widget(ALWAYS_DISABLED_LBL);
    prv->object_view = GTK_TREE_VIEW(nwamui_util_glade_get_widget(CONNECTION_PROFILE_VIEW));

    nwam_compose_tree_view(self);
    
	g_signal_connect(G_OBJECT(self), "notify", (GCallback)object_notify_cb, NULL);

    /* Initially refresh self */
    nwam_pref_refresh(NWAM_PREF_IFACE(self), NULL, TRUE);
}

/**
 * nwam_profile_panel_new:
 * @returns: a new #NwamProfilePanel.
 *
 * Creates a new #NwamProfilePanel with an empty ncu
 **/
NwamProfilePanel*
nwam_profile_panel_new(NwamCappletDialog *pref_dialog)
{
	NwamProfilePanel *self =  NWAM_PROFILE_PANEL(g_object_new(NWAM_TYPE_PROFILE_PANEL, NULL));
	NwamProfilePanelPrivate  *prv = GET_PRIVATE(self);

    prv->pref_dialog = g_object_ref(pref_dialog);

    return( self );
}

/**
 * refresh:
 *
 * Refresh #NwamProfilePanel with the new connections.
 **/
static gboolean
refresh(NwamPrefIFace *iface, gpointer user_data, gboolean force)
{
    NwamProfilePanel        *self = NWAM_PROFILE_PANEL(iface);
	NwamProfilePanelPrivate *prv  = GET_PRIVATE(self);
    
    g_debug("NwamProfilePanel: Refresh");
    g_assert(NWAM_IS_PROFILE_PANEL(self));
    
    if (force) {
        GtkTreeModel *model = gtk_tree_view_get_model(prv->object_view);

        /* Re-read objects from system */
        g_object_set(self, "toggled_object", NULL, NULL);
        nwam_profile_panel_set_toggled_row(self, NULL);

        gtk_widget_hide(GTK_WIDGET(self->prv->object_view));
		gtk_list_store_clear(GTK_LIST_STORE(model));
        nwamui_daemon_foreach_ncp(prv->daemon, capplet_list_foreach_merge_to_list_store, (gpointer)model);
        gtk_widget_show(GTK_WIDGET(self->prv->object_view));
    }

    nwam_treeview_update_widget_cb(gtk_tree_view_get_selection(prv->object_view),
      (gpointer)self);

    return( TRUE );
}

static gboolean
cancel(NwamPrefIFace *iface, gpointer user_data)
{
	NwamProfilePanelPrivate *prv  = GET_PRIVATE(iface);
    NwamProfilePanel    *self = NWAM_PROFILE_PANEL(iface);

    /* Re-read objects from system */
    g_object_set(self, "toggled_object", NULL, NULL);
    nwam_profile_panel_set_toggled_row(self, NULL);

    return TRUE;
}

static gboolean
apply(NwamPrefIFace *iface, gpointer user_data)
{
    NwamProfilePanel*        self      = NWAM_PROFILE_PANEL( iface );
	NwamProfilePanelPrivate *prv       = GET_PRIVATE(self);
	GtkTreeSelection *selection = gtk_tree_view_get_selection (prv->object_view);
    GtkTreeModel *model = gtk_tree_view_get_model (prv->object_view);
    gboolean retval = TRUE;
    ForeachNwamuiObjectCommitData data;

    /* Make sure new added which do not have a handle are committed. */
    /* nwamui_daemon_foreach_ncp(prv->daemon, foreach_commit_new_ncp, (gpointer)self); */
    data.failone = NULL;
    data.prop_name = NULL;
    gtk_tree_model_foreach(model, capplet_tree_model_foreach_nwamui_object_commit, &data);

    if (retval && data.failone) {
        retval = FALSE;
        if (data.prop_name) {
            gchar *msg = g_strdup_printf (_("Validation of %s failed with the property %s"), nwamui_object_get_name(NWAMUI_OBJECT(data.failone)), data.prop_name);

            nwamui_util_show_message(nwam_pref_dialog_get_window(NWAM_PREF_IFACE(prv->pref_dialog)),
              GTK_MESSAGE_ERROR, _("Validation error"), msg, TRUE);

            g_free(msg);

            g_free(data.prop_name);
        } else {
            gchar *msg = g_strdup_printf (_("Committing %s failed..."), nwamui_object_get_name(NWAMUI_OBJECT(data.failone)));

            nwamui_util_show_message(nwam_pref_dialog_get_window(NWAM_PREF_IFACE(prv->pref_dialog)),
              GTK_MESSAGE_ERROR, _("Commit ENM error"), msg, TRUE);

            g_free(msg);
        }
        /* Start highlight relevant object */
        gtk_tree_selection_select_iter(selection, &data.iter);

        g_object_unref(data.failone);
    }

    /* Activate the selected NCP. */
    if (prv->toggled_row && gtk_tree_row_reference_valid(prv->toggled_row)) {
        GtkTreePath *path = gtk_tree_row_reference_get_path(prv->toggled_row);
        GtkTreeIter iter;
        if (gtk_tree_model_get_iter(gtk_tree_view_get_model(prv->object_view), &iter, path)) {
            NwamuiObject *toggled_ncp;

            gtk_tree_model_get(gtk_tree_view_get_model(prv->object_view), &iter, 0, &toggled_ncp, -1);
            nwamui_object_set_active(toggled_ncp, TRUE);
            g_object_unref(toggled_ncp);
        }
    }

    return retval;
}

/**
 * help:
 *
 * Help #NwamProfilePanel
 **/
static gboolean
help(NwamPrefIFace *iface, gpointer user_data)
{
    g_debug ("NwamProfilePanel: Help");
    nwamui_util_show_help (HELP_REF_NWPROFILES_CONFIGURING);
}


static void
nwam_profile_panel_finalize(NwamProfilePanel *self)
{
	NwamProfilePanelPrivate *prv       = GET_PRIVATE(self);

    g_object_unref(prv->pref_dialog);

    g_object_unref(prv->daemon);

	self->prv = NULL;
	
	G_OBJECT_CLASS(nwam_profile_panel_parent_class)->finalize(G_OBJECT(self));
}

/* Callbacks */

/*
 * We don't need a comp here actually due to requirements
 */
static gint
nwam_object_model_compare_cb(GtkTreeModel *model,
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
nwam_object_activate_toggled_cb(GtkCellRendererToggle *cell_renderer,
  gchar                 *path,
  gpointer               user_data) 
{
	NwamProfilePanel        *self = NWAM_PROFILE_PANEL(user_data);
	NwamProfilePanelPrivate *prv  = GET_PRIVATE(self);
    GtkTreeIter              iter;
    GtkTreeModel            *model;
    GtkTreePath             *tpath;

	model = gtk_tree_view_get_model(prv->object_view);
    tpath = gtk_tree_path_new_from_string(path);
    /* I have to hide/show the view since I found the last toggled row can't be
     * updated, even I emit row-changed event on that row.
     */
    gtk_widget_hide(GTK_WIDGET(prv->object_view));
    nwam_profile_panel_set_toggled_row(self, tpath);
    gtk_widget_show(GTK_WIDGET(prv->object_view));

    gtk_tree_path_free(tpath);
}

static void
vanity_name_editing_started (GtkCellRenderer *cell,
  GtkCellEditable *editable,
  const gchar     *path,
  gpointer         data)
{
    NwamProfilePanelPrivate* prv = GET_PRIVATE(data);

    g_object_set (cell, "editable", FALSE, NULL);

    if (GTK_IS_ENTRY (editable)) {
        GtkEntry *entry = GTK_ENTRY (editable);
        GtkTreeIter iter;
        GtkTreeModel *model = gtk_tree_view_get_model(prv->object_view);
        GtkTreePath  *tpath;

        if ( (tpath = gtk_tree_path_new_from_string(path)) != NULL 
          && gtk_tree_model_get_iter (model, &iter, tpath)) {
            NwamuiObject*  object;

            gtk_tree_model_get(model, &iter, 0, &object, -1);

            if (object) {
                gtk_entry_set_text( entry, nwamui_object_get_name(object));
                g_object_unref(object);
            }
            gtk_tree_path_free(tpath);
        }
        nwam_capplet_dialog_set_ok_sensitive_by_voting(prv->pref_dialog, FALSE);
    }
}

static void 
vanity_name_editing_canceled ( GtkCellRenderer  *cell, gpointer data)
{
    NwamProfilePanelPrivate* prv = GET_PRIVATE(data);

    nwam_capplet_dialog_set_ok_sensitive_by_voting(prv->pref_dialog, TRUE);
}

static void
vanity_name_edited ( GtkCellRendererText *cell,
  const gchar         *path_string,
  const gchar         *new_text,
  gpointer             data)
{
    NwamProfilePanelPrivate* prv = GET_PRIVATE(data);

    g_object_set (cell, "editable", FALSE, NULL);

    nwam_capplet_dialog_set_ok_sensitive_by_voting(prv->pref_dialog, TRUE);
    /* Updated widgets due to the name changes. */
    nwam_treeview_update_widget_cb(gtk_tree_view_get_selection(prv->object_view), data);
}

static void
nwamui_ncp_name_cell_edited(GtkCellRendererText *cell,
  const gchar         *path_string,
  const gchar         *new_text,
  gpointer             data)
{
	NwamProfilePanelPrivate *prv  = GET_PRIVATE(data);
	GtkTreeView *treeview = GTK_TREE_VIEW(prv->object_view);
	GtkTreeModel *model = gtk_tree_view_get_model(treeview);
	GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
	GtkTreeIter iter;

	if ( gtk_tree_model_get_iter (model, &iter, path)) {
		NwamuiObject*   object;
        gboolean        editable;

		gtk_tree_model_get(model, &iter, 0, &object, -1);

        if (g_strcmp0(new_text, nwamui_object_get_name(object)) != 0) {
              /* We enable rename even NCP can't be renamed. The solution is if
               * the NCP can't be renamed, we can dup it with the new name after
               * user finish editing its name, destroy the old NCP, and commit
               * the dup NCP, and replace the tree row with the new NCP.
               */
              if (!nwamui_object_can_rename(object)) {
                  NwamuiObject *new_ncp;

                  new_ncp = nwamui_object_clone(object, new_text, NULL);

                  if (nwamui_object_commit(new_ncp)) {
                      /* Replace the old ncp in the treeview. */
                      g_signal_handlers_disconnect_by_func(G_OBJECT(object), (gpointer)capplet_util_object_notify_cb, (gpointer)model);
                      /* Add a ref, since it will be replaced, the tree model
                       * doesn't hold a ref anymore, and when returns, it will
                       * be unref one more time.
                       */
                      g_object_ref(object);
                      /* Reload ncp. */
                      nwamui_object_reload(new_ncp);
                      /* Replace the old one. */
                      gtk_list_store_set(GTK_LIST_STORE(model), &iter, 0, G_OBJECT(new_ncp), -1);

                      g_signal_connect(G_OBJECT(new_ncp), "notify", (GCallback)capplet_util_object_notify_cb, (gpointer)model);

                      /* Update row and related widgets. */
                      gtk_tree_model_row_changed(model, path, &iter);
                      nwam_treeview_update_widget_cb(gtk_tree_view_get_selection(treeview), data);

                      /* Add the new NCP, and destroy the old one. */
                      nwamui_object_add(NWAMUI_OBJECT(prv->daemon), new_ncp);
                      nwamui_object_destroy(object);
                      nwamui_object_remove(NWAMUI_OBJECT(prv->daemon), object);
                  } else {
                      GtkWindow  *top_level = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(treeview)));
                      gchar      *message;

                      message = g_strdup_printf(_("The name '%s' is already in use"), new_text );

                      nwamui_util_show_message( top_level, GTK_MESSAGE_ERROR, _("Name Already In Use"), message, FALSE);

                      g_free(message);
                  }
                  g_object_unref(new_ncp);
              } else {
                  /* NCP is not created yet. Commit(which is actually created)
                   * and add to UI daemon to reuse it.
                   */
                  nwamui_object_set_name(object, new_text);
                  if (nwamui_object_commit(object)) {
                      nwamui_object_add(NWAMUI_OBJECT(prv->daemon), object);
                  }
              }
        } else {
            /* Ignore because the name isnt changed. */
        }

		g_object_unref(object);
	}
	gtk_tree_path_free (path);
}

static void
nwam_treeview_update_widget_cb(GtkTreeSelection *selection, gpointer user_data)
{
    NwamProfilePanel*         self   = NWAM_PROFILE_PANEL(user_data);
    NwamProfilePanelPrivate*  prv    = GET_PRIVATE(self);
    GtkTreeModel*               model;
    GtkTreeIter                 iter;
    gint                        count_selected_rows;

    count_selected_rows = gtk_tree_selection_count_selected_rows(selection);

    gtk_widget_set_sensitive(GTK_WIDGET(prv->profile_edit_btn), count_selected_rows > 0 ? TRUE : FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(prv->profile_dup_btn), count_selected_rows > 0 ? TRUE : FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(prv->profile_rename_btn), count_selected_rows > 0 ? TRUE : FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(prv->profile_remove_btn), count_selected_rows > 0 ? TRUE : FALSE);

    if ( gtk_tree_selection_get_selected( selection, &model, &iter ) ) {
        NwamuiObject *object;
        gchar        *info;
        gboolean      modifiable;

        gtk_tree_model_get(model, &iter, 0, &object, -1);

        modifiable = nwamui_object_is_modifiable(object);
        /* gtk_widget_set_sensitive(GTK_WIDGET(prv->profile_edit_btn), modifiable); */
        /* gtk_widget_set_sensitive(GTK_WIDGET(prv->profile_rename_btn), modifiable && nwamui_object_can_rename(object)); */
        gtk_widget_set_sensitive(GTK_WIDGET(prv->profile_remove_btn), modifiable && !nwamui_object_get_active(object));

        gtk_label_set_text(GTK_LABEL(prv->selected_profile_lbl), nwamui_object_get_name(object));

        /* Clean before foreach */
        prv->always_enabled = 0;
        prv->always_disabled = 0;
        g_list_free(prv->groups);
        prv->groups = NULL;
        prv->first_enabled_name = NULL;
        prv->first_disabled_name = NULL;

        nwamui_ncp_foreach_ncu(NWAMUI_NCP(object), ncp_foreach_ncu_get_info, (gpointer)self);

        info = g_strdup_printf("%d", g_list_length(prv->groups));
        gtk_label_set_text(GTK_LABEL(prv->priority_groups_lbl), info);
        g_free(info);

        if (prv->always_enabled == 0) {
            gtk_label_set_text(GTK_LABEL(prv->always_enabled_lbl), _("None"));
        } else if (prv->always_enabled == 1) {
            gtk_label_set_text(GTK_LABEL(prv->always_enabled_lbl), prv->first_enabled_name);
        } else if (prv->always_enabled > 1) {
            info = g_strdup_printf(_("%s and %d more"), prv->first_enabled_name, prv->always_enabled - 1);
            gtk_label_set_text(GTK_LABEL(prv->always_enabled_lbl), info);
            g_free(info);
        }
        g_free(prv->first_enabled_name);

        if (prv->always_disabled == 0) {
            gtk_label_set_text(GTK_LABEL(prv->always_disabled_lbl), _("None"));
        } else if (prv->always_disabled == 1) {
            gtk_label_set_text(GTK_LABEL(prv->always_disabled_lbl), prv->first_disabled_name);
        } else if (prv->always_disabled > 1) {
            info = g_strdup_printf(_("%s and %d more"), prv->first_disabled_name, prv->always_disabled - 1);
            gtk_label_set_text(GTK_LABEL(prv->always_disabled_lbl), info);
            g_free(info);
        }
        g_free(prv->first_disabled_name);

        g_object_unref(object);
    } else {
        /* No selection */
        gtk_label_set_text(GTK_LABEL(prv->selected_profile_lbl), "");
        gtk_label_set_text(GTK_LABEL(prv->always_enabled_lbl), "");
        gtk_label_set_text(GTK_LABEL(prv->always_disabled_lbl), "");
        gtk_label_set_text(GTK_LABEL(prv->priority_groups_lbl), "");
    }
}

static void
object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
    g_debug("NwamProfilePanel: notify %s changed", arg1->name);
}

static void
nwam_object_toggled_cell_sensitive_func(GtkTreeViewColumn *col,
  GtkCellRenderer   *renderer,
  GtkTreeModel      *model,
  GtkTreeIter       *iter,
  gpointer           data)
{
    NwamProfilePanel        *self   = NWAM_PROFILE_PANEL(data);
	NwamProfilePanelPrivate *prv    = GET_PRIVATE(data);
    NwamuiObject*            object = NULL;
    
    gtk_tree_model_get(model, iter, 0, &object, -1);
    
    if (object) {
        if (!prv->toggled_row) {
            g_object_set(G_OBJECT(renderer),
              "active", nwamui_object_get_active(NWAMUI_OBJECT(object)),
              NULL);
        } else {
            GtkTreePath *cur = gtk_tree_model_get_path(model, iter);

            g_object_set(G_OBJECT(renderer),
              "active", FALSE,
              NULL); 
            if (nwam_profile_panel_is_toggled_row(self, cur)) {
                /* Show active, commit later. */
                g_object_set(G_OBJECT(renderer),
                  "active", TRUE,
                  NULL);
            }
            gtk_tree_path_free(cur);
        }
        g_object_unref(G_OBJECT(object));
    } else {
        g_object_set(G_OBJECT(renderer),
          "active", FALSE,
          NULL); 
    }
}

static void
ncp_foreach_ncu_get_info(gpointer data, gpointer user_data)
{
	NwamProfilePanelPrivate *prv = GET_PRIVATE(user_data);
    NwamuiObject            *ncu = NWAMUI_OBJECT(data);
    gint                     group;

    switch (group = nwamui_ncu_get_priority_group_for_view(NWAMUI_NCU(ncu))) {
    case ALWAYS_ON_GROUP_ID:
        prv->always_enabled ++;
        if (prv->first_enabled_name == NULL) {
            prv->first_enabled_name = g_strdup(nwamui_object_get_name(ncu));
        }
        break;
    case ALWAYS_OFF_GROUP_ID:
        prv->always_disabled ++;
        if (prv->first_disabled_name == NULL) {
            prv->first_disabled_name = g_strdup(nwamui_object_get_name(ncu));
        }
        break;
    default:
        if (g_list_find(prv->groups, (gconstpointer)group) == NULL) {
            prv->groups = g_list_prepend(prv->groups, (gpointer)group);
        }
        break;
    }
}

