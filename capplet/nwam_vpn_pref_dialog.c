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
#include "nwam_pref_iface.h"
#include "nwam_vpn_pref_dialog.h"
#include "nwam_rules_dialog.h"
#include "capplet-utils.h"
#include "nwam_tree_view.h"

#define VPN_APP_VIEW_COL_ID	"nwam_vpn_app_column_id"

/* Names of Widgets in Glade file */
#define	VPN_PREF_DIALOG_NAME	"vpn_config"
#define	VPN_PREF_TREE_VIEW	"vpn_apps_list"
#define	VPN_PREF_ADD_BTN	"vpn_add_btn"
#define	VPN_PREF_REMOVE_BTN	"vpn_remove_btn"
#define VPN_PREF_RENAME_BTN "vpn_rename_btn"
#define	VPN_PREF_START_BTN	"vpn_start_btn"
#define	VPN_PREF_STOP_BTN	"vpn_stop_btn"
#define	VPN_RULES_BTN	"vpn_rules_btn"
#define	VPN_CONDITIONAL_CB	"vpn_conditional_cb"
#define	VPN_PREF_BROWSE_START_CMD_BTN	"browse_start_cmd_btn"
#define	VPN_PREF_BROWSE_STOP_CMD_BTN	"browse_stop_cmd_entry"
#define VPN_PREF_START_CMD_ENTRY	"start_cmd_entry"
#define VPN_PREF_STOP_CMD_ENTRY	"stop_cmd_entry"
#define VPN_PREF_PROCESS_ENTRY	"process_entry"
#define VPN_PREF_START_CMD_LBL	"start_cmd_lbl"
#define VPN_PREF_STOP_CMD_LBL	"stop_cmd_lbl"
#define VPN_PREF_PROCESS_LBL	"process_lbl"
#define VPN_PREF_DESC_LBL	"desc_lbl"
#define VPN_CLI_RB "vpn_cli_rb"
#define VPN_SMF_RB "vpn_smf_rb"

enum {
    APP_MODE=0,
	APP_NAME,
	APP_STATE,
};

#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
	NWAM_TYPE_VPN_PREF_DIALOG, NwamVPNPrefDialogPrivate))

struct _NwamVPNPrefDialogPrivate {
	/* Widget Pointers */
	GtkDialog      *vpn_pref_dialog;
	GtkTreeView    *view;
	GtkButton      *add_btn;
	GtkButton      *remove_btn;
	GtkButton      *rename_btn;
	GtkButton      *start_btn;
	GtkButton      *stop_btn;
	GtkCheckButton *vpn_conditional_cb;
	GtkButton      *vpn_rules_btn;
	GtkButton      *browse_start_cmd_btn;
	GtkButton      *browse_stop_cmd_btn;
	GtkEntry       *start_cmd_entry;
	GtkEntry       *stop_cmd_entry;
	GtkEntry       *process_entry;
	GtkLabel       *start_cmd_lbl;
	GtkLabel       *stop_cmd_lbl;
	GtkLabel       *process_lbl;
	GtkLabel       *desc_lbl;
    GtkRadioButton *vpn_cli_rb;
    GtkRadioButton *vpn_smf_rb;

	/* nwam data related */
	NwamuiDaemon *daemon;
	//GList	*enm_list;
	GObject	*cur_obj;           /* current selection of tree */
};

static void nwam_pref_init (gpointer g_iface, gpointer iface_data);
static gboolean refresh(NwamPrefIFace *iface, gpointer user_data, gboolean force);
static gboolean apply(NwamPrefIFace *iface, gpointer user_data);
static gboolean cancel(NwamPrefIFace *iface, gpointer user_data);
static gboolean help(NwamPrefIFace *iface, gpointer user_data);
extern gint dialog_run(NwamPrefIFace *iface, GtkWindow *parent);
static GtkWindow* dialog_get_window(NwamPrefIFace *iface);

static void nwam_vpn_pref_dialog_finalize(NwamVPNPrefDialog *self);
static void nwam_compose_tree_view (NwamVPNPrefDialog *self);
static void set_property (GObject         *object,
                          guint            prop_id,
                          const GValue    *value,
                          GParamSpec      *pspec);
static void get_property (GObject         *object,
                          guint            prop_id,
                          GValue          *value,
                          GParamSpec      *pspec);

/* Callbacks */
static void object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);
static void response_cb( GtkWidget* widget, gint repsonseid, gpointer data );
static void vpn_pref_clicked_cb(GtkButton *button, gpointer data);
static void command_entry_changed(GtkEditable *editable, gpointer data);
static void nwam_vpn_state_cell_cb(GtkTreeViewColumn *col,
	GtkCellRenderer   *renderer,
	GtkTreeModel      *model,
	GtkTreeIter       *iter,
	gpointer           data);
static gint nwam_vpn_cell_comp_cb(GtkTreeModel *model,
	GtkTreeIter *a,
	GtkTreeIter *b,
	gpointer user_data);

static gboolean nwam_vpn_pre_selection_validate(    GtkTreeSelection *selection,
                                                    GtkTreeModel *model,
                                                    GtkTreePath *path,
                                                    gboolean path_currently_selected,
                                                    gpointer data);

static void nwam_vpn_selection_changed(GtkTreeSelection *selection,
	gpointer          data);
static void on_rules_button_clicked(GtkButton *button, gpointer user_data);
static void on_radio_button_toggled(GtkToggleButton *button, gpointer user_data);
static void conditional_toggled_cb(GtkToggleButton *button, gpointer user_data);

G_DEFINE_TYPE_EXTENDED (NwamVPNPrefDialog,
                        nwam_vpn_pref_dialog,
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
nwam_vpn_pref_dialog_class_init(NwamVPNPrefDialogClass *klass)
{
	/* Pointer to GObject Part of Class */
	GObjectClass *gobject_class = (GObjectClass*) klass;

	gobject_class->set_property = set_property;
	gobject_class->get_property = get_property;

	/* Override Some Function Pointers */
	gobject_class->finalize = (void (*)(GObject*)) nwam_vpn_pref_dialog_finalize;

	g_type_class_add_private (klass, sizeof (NwamVPNPrefDialogPrivate));
}


static void
nwam_vpn_pref_dialog_init(NwamVPNPrefDialog *self)
{
	NwamVPNPrefDialogPrivate *prv = GET_PRIVATE(self);
	self->prv = prv;

	/* daemon */
	prv->daemon = nwamui_daemon_get_instance ();

	/* Iniialise pointers to important widgets */
	prv->vpn_pref_dialog = GTK_DIALOG(nwamui_util_glade_get_widget(VPN_PREF_DIALOG_NAME));
	prv->view = GTK_TREE_VIEW(nwamui_util_glade_get_widget(VPN_PREF_TREE_VIEW));
	prv->add_btn = GTK_BUTTON(nwamui_util_glade_get_widget(VPN_PREF_ADD_BTN));
	prv->remove_btn = GTK_BUTTON(nwamui_util_glade_get_widget(VPN_PREF_REMOVE_BTN));
	prv->rename_btn = GTK_BUTTON(nwamui_util_glade_get_widget(VPN_PREF_RENAME_BTN));
	prv->start_btn = GTK_BUTTON(nwamui_util_glade_get_widget(VPN_PREF_START_BTN));
	prv->stop_btn = GTK_BUTTON(nwamui_util_glade_get_widget(VPN_PREF_STOP_BTN));

	prv->vpn_rules_btn = GTK_BUTTON(nwamui_util_glade_get_widget(VPN_RULES_BTN));
	prv->vpn_conditional_cb = GTK_CHECK_BUTTON(nwamui_util_glade_get_widget(VPN_CONDITIONAL_CB));

	prv->vpn_cli_rb = GTK_RADIO_BUTTON(nwamui_util_glade_get_widget(VPN_CLI_RB));
	prv->vpn_smf_rb = GTK_RADIO_BUTTON(nwamui_util_glade_get_widget(VPN_SMF_RB));

	prv->browse_start_cmd_btn = GTK_BUTTON(nwamui_util_glade_get_widget(VPN_PREF_BROWSE_START_CMD_BTN));
	prv->browse_stop_cmd_btn = GTK_BUTTON(nwamui_util_glade_get_widget(VPN_PREF_BROWSE_STOP_CMD_BTN));
	prv->start_cmd_entry = GTK_ENTRY(nwamui_util_glade_get_widget(VPN_PREF_START_CMD_ENTRY));
	prv->stop_cmd_entry = GTK_ENTRY(nwamui_util_glade_get_widget(VPN_PREF_STOP_CMD_ENTRY));
	prv->process_entry = GTK_ENTRY(nwamui_util_glade_get_widget(VPN_PREF_PROCESS_ENTRY));
	prv->start_cmd_lbl = GTK_LABEL(nwamui_util_glade_get_widget(VPN_PREF_START_CMD_LBL));
	prv->stop_cmd_lbl = GTK_LABEL(nwamui_util_glade_get_widget(VPN_PREF_STOP_CMD_LBL));
	prv->process_lbl = GTK_LABEL(nwamui_util_glade_get_widget(VPN_PREF_PROCESS_LBL));
	prv->desc_lbl = GTK_LABEL(nwamui_util_glade_get_widget(VPN_PREF_DESC_LBL));

    nwamui_util_set_entry_smf_fmri_completion( GTK_ENTRY(prv->process_entry) );
    g_signal_connect(G_OBJECT(prv->start_cmd_entry), "changed", 
      (GCallback)command_entry_changed, (gpointer)self);
    g_signal_connect(G_OBJECT(prv->stop_cmd_entry), "changed", 
      (GCallback)command_entry_changed, (gpointer)self);
    g_signal_connect(G_OBJECT(prv->process_entry), "changed", 
      (GCallback)command_entry_changed, (gpointer)self);

    /* Set title to include hostname */
    nwamui_util_window_title_append_hostname( prv->vpn_pref_dialog );

	g_signal_connect(prv->add_btn,
      "clicked", (GCallback)vpn_pref_clicked_cb, (gpointer)self);
	g_signal_connect(prv->remove_btn,
      "clicked", (GCallback)vpn_pref_clicked_cb, (gpointer)self);
	g_signal_connect(prv->rename_btn,
      "clicked", (GCallback)vpn_pref_clicked_cb, (gpointer)self);
	g_signal_connect(prv->start_btn,
      "clicked", (GCallback)vpn_pref_clicked_cb, (gpointer)self);
	g_signal_connect(prv->stop_btn,
      "clicked", (GCallback)vpn_pref_clicked_cb, (gpointer)self);
	g_signal_connect(prv->browse_start_cmd_btn,
      "clicked", (GCallback)vpn_pref_clicked_cb, (gpointer)self);
	g_signal_connect(prv->browse_stop_cmd_btn,
      "clicked", (GCallback)vpn_pref_clicked_cb, (gpointer)self);

    g_signal_connect(prv->vpn_conditional_cb, "toggled",
      (GCallback)conditional_toggled_cb, (gpointer)self);

	g_signal_connect(self, "notify", (GCallback)object_notify_cb, NULL);
	g_signal_connect(prv->vpn_pref_dialog, "response", (GCallback)response_cb, (gpointer)self);

    g_signal_connect(self->prv->vpn_rules_btn, "clicked",
      G_CALLBACK(on_rules_button_clicked), (gpointer)self);
	g_signal_connect(prv->vpn_cli_rb, "toggled",
      (GCallback)on_radio_button_toggled, (gpointer)self);
/* 	g_signal_connect(prv->vpn_smf_rb, "toggled", */
/*       (GCallback)on_radio_button_toggled, (gpointer)self); */

	nwam_compose_tree_view (self);

	/* Initializing */
	nwam_pref_refresh(NWAM_PREF_IFACE(self), NULL, TRUE);
}

static void
nwam_vpn_pref_dialog_finalize(NwamVPNPrefDialog *self)
{
	g_object_unref (G_OBJECT(self->prv->daemon));

	G_OBJECT_CLASS(nwam_vpn_pref_dialog_parent_class)->finalize(G_OBJECT(self));
}

static void set_property (GObject         *object,
                          guint            prop_id,
                          const GValue    *value,
                          GParamSpec      *pspec)
{

}

static void get_property (GObject         *object,
                          guint            prop_id,
                          GValue          *value,
                          GParamSpec      *pspec)
{

}

static void
capplet_tree_model_row_changed_func(GtkTreeModel *tree_model,
  GtkTreePath  *path,
  GtkTreeIter  *iter,
  gpointer      user_data)
{
    NwamVPNPrefDialogPrivate *prv       = GET_PRIVATE(user_data);
    GtkTreeSelection         *selection = gtk_tree_view_get_selection(prv->view);

    if (gtk_tree_selection_path_is_selected(selection, path)) {
        /* Re-select the row to update the panel. */
        nwam_vpn_selection_changed(selection, user_data);
    }
}

static void
nwam_compose_tree_view (NwamVPNPrefDialog *self)
{
	GtkTreeViewColumn *col;
	GtkCellRenderer *renderer;
	GtkTreeView *view = self->prv->view;

    g_object_set(view,
      "headers-clickable", FALSE,
      "headers-visible", TRUE,
      "rules-hint", TRUE,
      "reorderable", FALSE,
      "enable-search", TRUE,
      "show-expanders", TRUE,
      "button-add", self->prv->add_btn,
      NULL);

    nwam_tree_view_set_object_func(NWAM_TREE_VIEW(view),
      NULL,
      NULL,
      capplet_tree_view_commit_object,
      (gpointer)self);

	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(view),
      GTK_SELECTION_SINGLE);
    g_signal_connect(gtk_tree_view_get_selection(view),
      "changed",
      G_CALLBACK(nwam_vpn_selection_changed),
      (gpointer)self);

    gtk_tree_selection_set_select_function(gtk_tree_view_get_selection(view),
      nwam_vpn_pre_selection_validate,
      (gpointer)self, NULL);

    /* Mode column */
	col = capplet_column_new(view, NULL);
	renderer = capplet_column_append_cell(col,
      gtk_cell_renderer_pixbuf_new(), FALSE,
      nwamui_object_active_mode_pixbuf_cell, NULL, NULL);

	/* column APP_NAME */
	col = capplet_column_new(view,
      "title", _("Modifier Name"),
      "expand", FALSE,
      "resizable", TRUE,
      "clickable", FALSE,
      "sort-indicator", FALSE,
      "reorderable", FALSE,
      NULL);
	renderer = capplet_column_append_cell(col,
      gtk_cell_renderer_text_new(), FALSE,
      (GtkTreeCellDataFunc)nwamui_object_name_cell, NULL, NULL);
	g_signal_connect(renderer, "edited",
      G_CALLBACK(nwamui_object_name_cell_edited), (gpointer)view);
	g_object_set(G_OBJECT(renderer), "editable", TRUE, NULL);

#if 0
	gtk_tree_view_column_set_sort_column_id (col, APP_NAME);
	gtk_tree_sortable_set_sort_func	(GTK_TREE_SORTABLE(model),
      APP_NAME,
      (GtkTreeIterCompareFunc) nwam_vpn_cell_comp_cb,
      GINT_TO_POINTER(APP_NAME),
      NULL);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE(model),
      APP_NAME,
      GTK_SORT_ASCENDING);
#endif

	/* column APP_STATE */
	col = gtk_tree_view_column_new();
	gtk_tree_view_append_column (view, col);

    g_object_set(col,
      "title", _("Status"),
      "resizable", TRUE,
      "clickable", FALSE,
      "sort-indicator", FALSE,
      "reorderable", FALSE,
      NULL);

#if 0
	gtk_tree_view_column_set_sort_column_id (col, APP_STATE);
#endif

    /* First cell */
	renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);

	gtk_tree_view_column_set_cell_data_func (col,
      renderer,
      nwam_vpn_state_cell_cb,
      (gpointer) self,
      NULL);

    CAPPLET_COMPOSE_NWAMUI_OBJECT_LIST_VIEW(view);
    g_signal_connect(gtk_tree_view_get_model(view),
      "row-changed", G_CALLBACK(capplet_tree_model_row_changed_func), (gpointer)self);
}

/* If update failed, popup dialog and return FALSE */
static gboolean
nwam_update_obj (NwamVPNPrefDialog *self, GObject *obj)
{
	NwamVPNPrefDialogPrivate       *prv = GET_PRIVATE(self);
	const gchar                    *txt = NULL;
    gchar                          *prev_txt = NULL;
    gboolean                        cli_value;
    gboolean                        manual_mode;
    nwamui_cond_activation_mode_t   current_mode;
    GList                          *conditions_list;

    if (!NWAMUI_IS_ENM(obj)) {
        return( FALSE );
    }

    /* Stop update GUI when commit changes one by one. */
    g_object_freeze_notify(obj);
    cli_value = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prv->vpn_cli_rb));
    if (cli_value) {
        prev_txt = nwamui_enm_get_start_command(NWAMUI_ENM(obj));
        txt = gtk_entry_get_text(prv->start_cmd_entry);
        if ( (prev_txt == NULL || strlen( prev_txt ) == 0 )
              || strcmp( prev_txt?prev_txt:"", txt?txt:"") != 0 ) {
            if ( !nwamui_enm_set_start_command(NWAMUI_ENM(obj), txt?txt:"") ) {
                nwamui_util_show_message (GTK_WINDOW(self->prv->vpn_pref_dialog), GTK_MESSAGE_ERROR, _("Validation Error"),
                  _("Invalid value specified for Start Command"), TRUE );
                g_free(prev_txt);
                return( FALSE );
            }
        }
        g_free(prev_txt);

        prev_txt = nwamui_enm_get_stop_command(NWAMUI_ENM(obj));
        txt = gtk_entry_get_text(prv->stop_cmd_entry);
        if ( (prev_txt == NULL || strlen( prev_txt ) == 0 )
              || strcmp( prev_txt?prev_txt:"", txt?txt:"") != 0 ) {
            if ( !nwamui_enm_set_stop_command(NWAMUI_ENM(obj), txt?txt:"") ) {
                nwamui_util_show_message (GTK_WINDOW(self->prv->vpn_pref_dialog), GTK_MESSAGE_ERROR, _("Validation Error"),
                  _("Invalid value specified for Stop Command"), TRUE );
                g_free(prev_txt);
                return( FALSE );
            }
        }
        g_free(prev_txt);
        if ( !nwamui_enm_set_smf_fmri(NWAMUI_ENM(obj), NULL) ) {
            g_debug("Error deleting smf_fmri value");
        }
    } else {
        prev_txt = nwamui_enm_get_smf_fmri(NWAMUI_ENM(obj));
        txt = gtk_entry_get_text(prv->process_entry);
        if ( strcmp( prev_txt?prev_txt:"", txt?txt:"") != 0 ) {
            if ( !nwamui_enm_set_smf_fmri(NWAMUI_ENM(obj), txt?txt:"") ) {
                nwamui_util_show_message (GTK_WINDOW(self->prv->vpn_pref_dialog), GTK_MESSAGE_ERROR, _("Validation Error"),
                  _("Invalid value specified for SMF FMRI"), TRUE );
                g_free(prev_txt);
                return( FALSE );
            }
        }
        g_free(prev_txt);
        if ( !nwamui_enm_set_start_command(NWAMUI_ENM(obj), NULL) ) {
            g_debug("Error deleting start_command value");
        }
        if ( !nwamui_enm_set_stop_command(NWAMUI_ENM(obj), NULL) ) {
            g_debug("Error deleting stop_command value");
        }
    }

    /* Handle activation mode */
    manual_mode = !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prv->vpn_conditional_cb));
    current_mode = nwamui_object_get_activation_mode( NWAMUI_OBJECT(obj) );
    conditions_list = nwamui_object_get_conditions( NWAMUI_OBJECT(obj) );

    if ( manual_mode ) {
        if ( current_mode != NWAMUI_COND_ACTIVATION_MODE_MANUAL ) {
            nwamui_object_set_activation_mode( NWAMUI_OBJECT(obj), NWAMUI_COND_ACTIVATION_MODE_MANUAL );
            nwamui_object_set_conditions( NWAMUI_OBJECT(obj), NULL ); /* To delete conditions */
        }
    } else {
        /* Check that some conditions exist */
        if ( conditions_list == NULL ) {
            nwamui_util_show_message (GTK_WINDOW(GTK_WINDOW(self->prv->vpn_pref_dialog)), GTK_MESSAGE_ERROR, _("Validation Error"),
              _("Activation mode is set to Conditional,\nbut no rules have been configured."), TRUE );
            return( FALSE );
        }
        /* If the current mode isn't one of the conditional ones, then assume
         * ALL is the desired choice */
        if ( current_mode != NWAMUI_COND_ACTIVATION_MODE_CONDITIONAL_ALL &&
             current_mode != NWAMUI_COND_ACTIVATION_MODE_CONDITIONAL_ANY ) {
            nwamui_object_set_activation_mode( NWAMUI_OBJECT(obj), NWAMUI_COND_ACTIVATION_MODE_CONDITIONAL_ALL );
        }
    }

    nwamui_util_free_obj_list( conditions_list );

    /* Update GUI */
    g_object_thaw_notify(obj);

	return TRUE;
}

/**
 * nwam_vpn_pref_dialog_new:
 * @returns: a new #NwamVPNPrefDialog.
 *
 * Creates a new #NwamVPNPrefDialog.
 **/
NwamVPNPrefDialog*
nwam_vpn_pref_dialog_new(void)
{
	return NWAM_VPN_PREF_DIALOG(g_object_new(NWAM_TYPE_VPN_PREF_DIALOG, NULL));
}

extern gint
dialog_run(NwamPrefIFace *iface, GtkWindow *parent)
{
	NwamVPNPrefDialog* self = NWAM_VPN_PREF_DIALOG(iface);
	NwamVPNPrefDialogPrivate *prv = GET_PRIVATE(iface);
	gint response = GTK_RESPONSE_NONE;

	g_assert(NWAM_IS_VPN_PREF_DIALOG(self));
	if (parent) {
		gtk_window_set_transient_for (GTK_WINDOW(self->prv->vpn_pref_dialog), parent);
		gtk_window_set_modal (GTK_WINDOW(self->prv->vpn_pref_dialog), TRUE);
	} else {
		gtk_window_set_transient_for (GTK_WINDOW(self->prv->vpn_pref_dialog), NULL);
		gtk_window_set_modal (GTK_WINDOW(self->prv->vpn_pref_dialog), FALSE);
	}

	if ( self->prv->vpn_pref_dialog != NULL ) {
        nwam_pref_refresh(NWAM_PREF_IFACE(self), NULL, TRUE);
		response =  gtk_dialog_run(GTK_DIALOG(self->prv->vpn_pref_dialog));
	}
	return(response);
}

static GtkWindow*
dialog_get_window(NwamPrefIFace *iface)
{
	NwamVPNPrefDialog* self = NWAM_VPN_PREF_DIALOG(iface);

    if ( self->prv->vpn_pref_dialog ) {
        return ( GTK_WINDOW(self->prv->vpn_pref_dialog) );
    }

    return(NULL);
}

static gboolean
refresh(NwamPrefIFace *iface, gpointer user_data, gboolean force)
{
	NwamVPNPrefDialogPrivate *prv = GET_PRIVATE(iface);

    gtk_widget_hide(GTK_WIDGET(prv->view));
    capplet_update_model_from_daemon(gtk_tree_view_get_model(prv->view), prv->daemon, NWAMUI_TYPE_ENM);
    gtk_widget_show(GTK_WIDGET(prv->view));

    if (force) {
        /* Unselect the selected object to refresh all widget. */
        /* prv->cur_obj == NULL; */
        /* gtk_tree_selection_unselect_all(gtk_tree_view_get_selection(prv->view)); */
    }
}

static gboolean
cancel(NwamPrefIFace *iface, gpointer user_data)
{
	NwamVPNPrefDialog* self = NWAM_VPN_PREF_DIALOG(iface);
	NwamVPNPrefDialogPrivate *prv = GET_PRIVATE(iface);
	GtkTreeSelection *selection = NULL;
	GtkTreeIter iter;
    GtkTreeModel *model;
    GObject *obj;
    gboolean retval = TRUE;

    /* How to handle the new/selected one?
     */
    if (prv->cur_obj && !nwam_update_obj(self, prv->cur_obj)) {
        return FALSE;
    }

    /* model = gtk_tree_view_get_model (prv->view); */
    /* selection = gtk_tree_view_get_selection (prv->view); */
    /* if (gtk_tree_selection_get_selected(selection, */
    /*     NULL, &iter)) { */

    /*     gtk_tree_model_get (model, &iter, 0, &obj, -1); */
    /*     /\* Revert all before close *\/ */
    /*     nwamui_object_reload(NWAMUI_OBJECT(obj)); */
    /* } */

    return( TRUE );
}

static gboolean
apply(NwamPrefIFace *iface, gpointer user_data)
{
	NwamVPNPrefDialog* self = NWAM_VPN_PREF_DIALOG(iface);
	NwamVPNPrefDialogPrivate *prv = GET_PRIVATE(iface);
	GtkTreeSelection *selection = NULL;
    GtkTreeModel *model;
	GtkTreeIter iter;
    GObject *obj;
    gboolean retval = TRUE;

    model = gtk_tree_view_get_model (prv->view);
    selection = gtk_tree_view_get_selection (prv->view);

    /* update the new one before close */
    if (prv->cur_obj) {
        if (!nwam_update_obj(self, prv->cur_obj)) {
            retval = FALSE;
        }
    }

    /* Call into separated panel/instance
     * apply all changes, if no errors, hide all
     */
    if (retval && gtk_tree_model_get_iter_first (model, &iter)) {
        gchar* prop_name = NULL;
        do {
            gtk_tree_model_get (model, &iter, 0, &obj, -1);

            if (nwamui_enm_validate (NWAMUI_ENM (obj), &prop_name)) {
                if (!nwamui_object_commit (NWAMUI_OBJECT (obj))) {
                    /* Start highlight relevant ENM */
                    gtk_tree_selection_select_iter(selection, &iter);

                    gchar *msg = g_strdup_printf (_("Committing %s failed..."), nwamui_object_get_name (NWAMUI_OBJECT(obj)));
                    nwamui_util_show_message (GTK_WINDOW(prv->vpn_pref_dialog),
                      GTK_MESSAGE_ERROR,
                      _("Commit ENM error"),
                      msg, TRUE);
                    g_free (msg);
                    retval = FALSE;
                    g_object_unref(obj);
                    break;
                }
            } else {
                gchar *msg = g_strdup_printf (_("Validation of %s failed with the property %s"), nwamui_object_get_name (NWAMUI_OBJECT (obj)), prop_name);

                /* Start highligh relevant ENM */
                gtk_tree_selection_select_iter(selection, &iter);

                nwamui_util_show_message (GTK_WINDOW(prv->vpn_pref_dialog),
                  GTK_MESSAGE_ERROR,
                  _("Validation error"),
                  msg, TRUE);
                g_free (msg);
                retval = FALSE;
                g_object_unref(obj);
                break;
            }

            g_object_unref(obj);

        } while (gtk_tree_model_iter_next (model, &iter));
    }

    if (retval) {
        NwamuiDaemon *daemon = prv->daemon;
        GList *nlist = capplet_model_to_list(model);
        GList *olist = NULL;
        GList *i;

        nwamui_daemon_foreach_enm(prv->daemon,
          (GFunc)nwamui_util_foreach_nwam_object_dup_and_append_to_list,
          (gpointer)&olist);

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
                    nwamui_daemon_append_object(daemon, NWAMUI_OBJECT(i->data));
                }
                g_object_unref(i->data);
            }
            g_list_free(nlist);
        }

        for (i = olist; i; i = i->next) {
            nwamui_daemon_remove_object(daemon, NWAMUI_OBJECT(i->data));
            g_object_unref(i->data);
        }

        g_list_free(olist);
    }

    return retval;
}

static gboolean
help(NwamPrefIFace *iface, gpointer user_data)
{
    g_debug ("NwamVPNPrefDialog: Help");
    nwamui_util_show_help (HELP_REF_VPN_CONFIGURING);
}

/* call backs */
static void
object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
	g_debug("NwamVPNPrefDialog: notify %s changed", arg1->name);
}

static void
response_cb(GtkWidget* widget, gint responseid, gpointer data)
{
	NwamVPNPrefDialogPrivate *prv = GET_PRIVATE(data);
    gboolean            stop_emission = FALSE;

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
			break;
		case GTK_RESPONSE_CANCEL:
			g_debug("GTK_RESPONSE_CANCEL");
            stop_emission = !nwam_pref_cancel(NWAM_PREF_IFACE(data), NULL);
			break;
		case GTK_RESPONSE_HELP:
            nwam_pref_help (NWAM_PREF_IFACE(data), NULL);
            stop_emission = TRUE;
			break;
	}
    if ( stop_emission ) {
        g_signal_stop_emission_by_name(widget, "response" );
    } else {
        gtk_widget_hide (GTK_WIDGET(prv->vpn_pref_dialog));
    }
}

static void
command_entry_changed(GtkEditable *editable, gpointer data)
{
	NwamVPNPrefDialogPrivate *prv       = GET_PRIVATE(data);
    const gchar              *text      = NULL;
    gboolean                  visible   = FALSE;
    gboolean                  active    = FALSE;
    gboolean                  has_cond  = FALSE;
    /* guint16                   len; */
    gchar*                    prop_name = NULL;
    GtkWidget                *w         = NULL;
    GtkTreeModel             *model     = gtk_tree_view_get_model(prv->view);

    g_assert(prv->cur_obj);


    /* len = gtk_entry_get_text_length(GTK_ENTRY(editable)); */
    /* g_signal_handlers_block_by_func(G_OBJECT(gtk_tree_view_get_selection(prv->view)), */
    /*   (gpointer)nwam_vpn_selection_changed, data); */
    g_signal_handlers_disconnect_by_func(G_OBJECT(prv->cur_obj), (gpointer)capplet_util_object_notify_cb, (gpointer)model);

    has_cond = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prv->vpn_conditional_cb));
    active = nwamui_object_get_active(NWAMUI_OBJECT(prv->cur_obj));
    text = gtk_entry_get_text(GTK_ENTRY(editable));

    if ( editable == GTK_EDITABLE(prv->start_cmd_entry ) ) {
        active = !active;
        w = GTK_WIDGET(prv->start_btn);
        if (nwamui_enm_set_start_command(NWAMUI_ENM(prv->cur_obj), text)) {
            visible = TRUE;
        } else {
            visible = FALSE;
        }
    } else if ( editable == GTK_EDITABLE(prv->stop_cmd_entry ) ) {
        w = GTK_WIDGET(prv->stop_btn);
        if (nwamui_enm_set_stop_command(NWAMUI_ENM(prv->cur_obj), text)) {
            visible = TRUE;
        } else {
            visible = FALSE;
        }
    } else {
        if (nwamui_enm_set_smf_fmri(NWAMUI_ENM(prv->cur_obj), text)) {
            visible = TRUE;
        } else {
            visible = FALSE;
        }
    }
    /* Condition is not enabled. */
    visible = !has_cond && visible;

    /* if (nwamui_enm_validate(NWAMUI_ENM(prv->cur_obj), NULL)) { */
    /*     visible = TRUE; */
    /* } */
    g_signal_connect(G_OBJECT(prv->cur_obj), "notify", (GCallback)capplet_util_object_notify_cb, (gpointer)model);
    /* g_signal_handlers_unblock_by_func(G_OBJECT(gtk_tree_view_get_selection(prv->view)), */
    /*   (gpointer)nwam_vpn_selection_changed, data); */

    if (w) {
        gtk_widget_set_sensitive(w, (active && visible));
    } else {
        gtk_widget_set_sensitive(GTK_WIDGET(prv->start_btn), visible && !active);
        gtk_widget_set_sensitive (GTK_WIDGET(prv->stop_btn), visible && active);
    }
}

static void
vpn_pref_clicked_cb (GtkButton *button, gpointer data)
{
	NwamVPNPrefDialog* self = NWAM_VPN_PREF_DIALOG(data);
	NwamVPNPrefDialogPrivate *prv = GET_PRIVATE(data);
	GtkTreeSelection *selection = NULL;
    GtkTreeModel *model = NULL;
	NwamuiEnm *obj;
	GtkTreeIter iter;

    if (button == (gpointer)prv->add_btn) {
        gchar *name;
        NwamuiObject *object;

		model = gtk_tree_view_get_model (prv->view);

        /* Fixed 14258 [132] nwam error when adding second VPN */
        if (prv->cur_obj && !nwam_update_obj(self, prv->cur_obj)) {
            return;
        }

        name = capplet_get_increasable_name(model, _("Unnamed Modifier"), G_OBJECT(self));

        g_assert(name);

        object = NWAMUI_OBJECT(nwamui_enm_new(name));

        CAPPLET_LIST_STORE_ADD(GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(prv->view))), object);
        nwamui_daemon_append_object(prv->daemon, NWAMUI_OBJECT(object));
        g_free(name);
        g_object_unref(object);

        /* Select and scroll to this new object. */
        gtk_tree_view_scroll_to_cell(prv->view,
          nwam_tree_view_get_cached_object_path(NWAM_TREE_VIEW(prv->view)),
          NULL, FALSE, 0.0, 0.0);
        gtk_tree_selection_select_path(gtk_tree_view_get_selection(prv->view),
          nwam_tree_view_get_cached_object_path(NWAM_TREE_VIEW(prv->view)));
#ifndef USE_DIALOG_FOR_NAME
        /* Trigger rename on the new object as spec defines. */
        gtk_button_clicked(prv->rename_btn);
#endif /* ! USE_DIALOG_FOR_NAME */
        return;
    }

    if (button == prv->remove_btn) {
		GtkTreeSelection *selection;
		GList *list, *idx;
		GtkTreeModel *model;

		model = gtk_tree_view_get_model (prv->view);
		selection = gtk_tree_view_get_selection (prv->view);
		list = gtk_tree_selection_get_selected_rows (selection, NULL);

		for (idx=list; idx; idx = g_list_next(idx)) {
			GtkTreeIter  iter;
			g_assert (idx->data);
			if (gtk_tree_model_get_iter(model, &iter, (GtkTreePath *)idx->data)) {
				gtk_tree_model_get (model, &iter, 0, &obj, -1);
				gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
				if (obj) {
                    if (prv->cur_obj == (gpointer)obj) {
                        prv->cur_obj = NULL;
                    }
                    nwamui_object_destroy(NWAMUI_OBJECT(obj));
                    g_object_unref(obj);
                }
			}
			gtk_tree_path_free ((GtkTreePath *)idx->data);
		}
		g_list_free (list);
		return;
	}

    if (button == prv->rename_btn) {
		GtkTreeSelection *selection;
		GtkTreeModel *model;

		model = gtk_tree_view_get_model (prv->view);
		selection = gtk_tree_view_get_selection (prv->view);
        if ( gtk_tree_selection_get_selected( selection, &model, &iter ) ) {
            NwamuiObject *object;

            gtk_tree_model_get (model, &iter, 0, &obj, -1);

            if (nwamui_object_can_rename(NWAMUI_OBJECT(obj))) {
                GtkCellRendererText*        txt;
                GtkTreeViewColumn*          info_col = gtk_tree_view_get_column( GTK_TREE_VIEW(prv->view), APP_NAME );
                GList*                      renderers = gtk_tree_view_column_get_cell_renderers( info_col );

                /* Should be only one renderer */
                g_assert( g_list_next( renderers ) == NULL );

                if ((txt = GTK_CELL_RENDERER_TEXT(g_list_first( renderers )->data)) != NULL) {
                    GtkTreePath*    tpath = gtk_tree_model_get_path(model, &iter);
                    g_object_set (txt, "editable", TRUE, NULL);

                    gtk_tree_view_set_cursor (GTK_TREE_VIEW(prv->view), tpath, info_col, TRUE);

                    gtk_tree_path_free(tpath);
                }
            } else {
                gchar *summary = g_strdup_printf(_("Cannot rename '%s'"), nwamui_object_get_name(NWAMUI_OBJECT(obj)));
                nwamui_util_show_message(GTK_WINDOW(prv->vpn_pref_dialog),
                  GTK_MESSAGE_ERROR,
                  summary,
                  _("Network modifiers can only be renamed immediately after\nthey have been created."),
                  FALSE);
                g_free(summary);
            }
            g_object_unref(obj);
        }
		return;
	}

	selection = gtk_tree_view_get_selection (prv->view);
	model = gtk_tree_view_get_model (prv->view);
	gtk_tree_selection_get_selected(selection, NULL, &iter);
	gtk_tree_model_get (model, &iter, 0, &obj, -1);

	if (button == prv->browse_start_cmd_btn || button == prv->browse_stop_cmd_btn) {
		GtkWidget *toplevel;
		GtkWidget *filedlg = NULL;
		GtkEntry *active_entry;
		gchar *title = NULL;

		toplevel = gtk_widget_get_toplevel (GTK_WIDGET(button));
		if (!GTK_WIDGET_TOPLEVEL(toplevel)) {
			toplevel = NULL;
		}
		if (button == prv->browse_start_cmd_btn) {
			active_entry = prv->start_cmd_entry;
			title = _("Select start command");
		} else if (button == prv->browse_stop_cmd_btn) {
			active_entry = prv->stop_cmd_entry;
			title = _("Select stop command");
		}
		filedlg = gtk_file_chooser_dialog_new(title,
			GTK_WINDOW(toplevel),
			GTK_FILE_CHOOSER_ACTION_OPEN,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
			NULL);
		if (gtk_dialog_run(GTK_DIALOG(filedlg)) == GTK_RESPONSE_ACCEPT) {
			char *filename;

			filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(filedlg));
			/* FIXME, also set NwamuiEnm object and verify the existance of the filepath */
			gtk_entry_set_text (active_entry, filename);
			g_free(filename);
		}
		gtk_widget_destroy(filedlg);
		return;
	}

    /* Update object before activate it, because object notify will update cell
     * then refresh all the widgets to lose data.
     */
    g_signal_handlers_block_by_func(G_OBJECT(gtk_tree_view_get_selection(prv->view)),
      (gpointer)nwam_vpn_selection_changed, (gpointer)self);

    if (!nwam_update_obj (self, prv->cur_obj) ||
      !nwamui_object_commit(NWAMUI_OBJECT(prv->cur_obj))) {

        g_signal_handlers_unblock_by_func(G_OBJECT(gtk_tree_view_get_selection(prv->view)),
          (gpointer)nwam_vpn_selection_changed, (gpointer)self);

        return;
    }

    g_signal_handlers_unblock_by_func(G_OBJECT(gtk_tree_view_get_selection(prv->view)),
      (gpointer)nwam_vpn_selection_changed, (gpointer)self);

    /* We should not set sensitive of start/stop buttons after
     * trigger it, we should wait the sigal if there has.
     */
    if (button == prv->start_btn) {
        nwamui_object_set_active(NWAMUI_OBJECT(obj), TRUE);
    } else if (button == prv->stop_btn) {
        nwamui_object_set_active(NWAMUI_OBJECT(obj), FALSE);
    }
}

static void
nwam_vpn_state_cell_cb (GtkTreeViewColumn *col,
		GtkCellRenderer   *renderer,
		GtkTreeModel      *model,
		GtkTreeIter       *iter,
		gpointer           data)
{
	NwamVPNPrefDialog*self = NWAM_VPN_PREF_DIALOG(data);
	GObject *obj;

	gtk_tree_model_get(GTK_TREE_MODEL(model), iter, 0, &obj, -1);

    if (obj) {
        gboolean is_active;
        gchar *status;
        is_active = nwamui_object_get_active(NWAMUI_OBJECT(obj));
        if (is_active) {
            status = _("Running");
        } else {
            status = _("Stopped");
        }
        g_object_set((gpointer)renderer,
          "text", status,
          NULL);
    } else {
        g_object_set((gpointer)renderer,
          "text", "",
          NULL);
    }
}

static gint
nwam_vpn_cell_comp_cb (GtkTreeModel *model,
			      GtkTreeIter *a,
			      GtkTreeIter *b,
			      gpointer user_data)
{
	return 0;
}

static gboolean
nwam_vpn_pre_selection_validate(GtkTreeSelection *selection,
  GtkTreeModel *model,
  GtkTreePath *path,
  gboolean path_currently_selected,
  gpointer data)
{
	NwamVPNPrefDialogPrivate *prv    = GET_PRIVATE(data);
	NwamVPNPrefDialog        *self   = NWAM_VPN_PREF_DIALOG(data);
	GtkTreeIter               iter;
    gboolean                  retval = TRUE;

    /* cur_obj always poits to the old selection. */
    if (prv->cur_obj && path_currently_selected) {
        /* This is the old selection. We need to verify and commit it before we
         * unselected it.
         */
        /* if (!nwamui_object_has_modifications(NWAMUI_OBJECT(prv->cur_obj))) { */
        /*     retval = TRUE; */
        /* } else { */
            gchar* prop_name;
            if (!nwam_update_obj(NWAM_VPN_PREF_DIALOG(data), prv->cur_obj)) {
                /* Don't change selection */
                retval = FALSE;
            } else if (!nwamui_enm_validate(NWAMUI_ENM(prv->cur_obj), &prop_name)) {
                gchar* message = g_strdup_printf(_("An error occurred validating the network modifier configuration.\nThe property '%s' caused this failure"), prop_name );
                nwamui_util_show_message (GTK_WINDOW(prv->vpn_pref_dialog),
                  GTK_MESSAGE_ERROR, _("Validation Error"), message, TRUE );
                g_free(prop_name);
                g_free(message);
                retval = FALSE;
            }
        /* } */
        return retval;
    } else {
        retval = TRUE;
    }

	/* if (gtk_tree_selection_get_selected(selection, NULL, &iter)) { */
	/* 	GtkTreeModel *model = gtk_tree_view_get_model (prv->view); */
	/* 	GObject *obj; */

	/* 	gtk_tree_model_get (model, &iter, 0, &obj, -1); */

		/* Validate the the old selection first, should assert the correct data */
		/* if (prv->cur_obj && prv->cur_obj == obj) { */
		/* if (prv->cur_obj) { */
        /*     gchar* prop_name; */

        /*     g_signal_handlers_block_by_func(G_OBJECT(gtk_tree_view_get_selection(prv->view)), */
        /*       (gpointer)nwam_vpn_selection_changed, (gpointer)self); */

        /*     if ( !nwam_update_obj (NWAM_VPN_PREF_DIALOG(data), prv->cur_obj) ) { */
        /*         /\* Don't change selection *\/ */
        /*         retval = FALSE; */
        /*     } */
        /*     else if ( !nwamui_enm_validate( NWAMUI_ENM(obj), &prop_name ) ) { */
        /*         gchar* message = g_strdup_printf(_("An error occurred validating the network modifier configuration.\nThe property '%s' caused this failure"), prop_name ); */
        /*         nwamui_util_show_message (GTK_WINDOW(prv->vpn_pref_dialog), */
        /*                                   GTK_MESSAGE_ERROR, _("Validation Error"), message, TRUE ); */
        /*         g_free(prop_name); */
        /*         g_free(message); */
        /*         retval = FALSE; */
        /*     } */

        /*     g_signal_handlers_unblock_by_func(G_OBJECT(gtk_tree_view_get_selection(prv->view)), */
        /*       (gpointer)nwam_vpn_selection_changed, (gpointer)self); */


        /* } */
    /*     g_object_unref( obj ); */
    /* } */

    return retval;
}

static void
nwam_vpn_selection_changed(GtkTreeSelection *selection, gpointer data)
{
	NwamVPNPrefDialogPrivate *prv = GET_PRIVATE(data);

	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(selection, NULL, &iter)) {
		GtkTreeModel *model = gtk_tree_view_get_model (prv->view);
		GObject *obj;
		gchar *txt = NULL;

		gtk_tree_model_get (model, &iter, 0, &obj, -1);

        if (obj) {
            gchar    *title;
            gboolean  is_active;

            is_active = nwamui_object_get_active(NWAMUI_OBJECT(obj));

            title = g_strdup_printf(_("Start/stop '%s' according to rules"), nwamui_object_get_name(NWAMUI_OBJECT(obj)));
            g_object_set(prv->vpn_conditional_cb, "label", title, NULL);
            gtk_widget_set_sensitive (GTK_WIDGET(prv->vpn_conditional_cb), TRUE);

            g_free(title);

            /* State update, update at once. */
            if (nwamui_object_get_activation_mode(NWAMUI_OBJECT(obj)) == NWAMUI_COND_ACTIVATION_MODE_CONDITIONAL_ANY
              || nwamui_object_get_activation_mode(NWAMUI_OBJECT(obj)) == NWAMUI_COND_ACTIVATION_MODE_CONDITIONAL_ALL) {
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prv->vpn_conditional_cb), TRUE);
                gtk_widget_set_sensitive (GTK_WIDGET(prv->vpn_rules_btn), TRUE);
                gtk_widget_set_sensitive (GTK_WIDGET(prv->start_btn), FALSE);
                gtk_widget_set_sensitive (GTK_WIDGET(prv->stop_btn), FALSE);
            } else {
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prv->vpn_conditional_cb), FALSE);
                gtk_widget_set_sensitive (GTK_WIDGET(prv->vpn_rules_btn), FALSE);
                if (is_active) {
                    gtk_widget_set_sensitive (GTK_WIDGET(prv->start_btn), FALSE);
                    gtk_widget_set_sensitive (GTK_WIDGET(prv->stop_btn), TRUE);
                } else {
                    gtk_widget_set_sensitive (GTK_WIDGET(prv->start_btn), TRUE);
                    gtk_widget_set_sensitive (GTK_WIDGET(prv->stop_btn), FALSE);
                }
            }

            gtk_widget_set_sensitive (GTK_WIDGET(prv->remove_btn), !is_active );
            gtk_widget_set_sensitive (GTK_WIDGET(prv->rename_btn), TRUE );


            /* Content update, shouldn't disturb when user inputs. 
             *
             * The current object will have been updated by the selection
             * function which attempted to verify the object before changing the
             * selection
             */

            /* if (prv->cur_obj != obj) */
            {
                gboolean    start_value = FALSE;
                gboolean    stop_value = FALSE;
                gboolean    fmri_value = FALSE;
                gboolean    init_editting = FALSE;
                gboolean    cli_value = FALSE;

                prv->cur_obj = obj;

                g_signal_handlers_block_by_func(prv->start_cmd_entry,
                  (gpointer)command_entry_changed, data);
                g_signal_handlers_block_by_func(prv->stop_cmd_entry,
                  (gpointer)command_entry_changed, data);
                g_signal_handlers_block_by_func(prv->process_entry,
                  (gpointer)command_entry_changed, data);

                txt = nwamui_enm_get_start_command (NWAMUI_ENM(obj));

                gtk_entry_set_text(prv->start_cmd_entry, txt?txt:"");

                if ( txt != NULL && strlen(txt) > 0 ) {
                    start_value = TRUE;
                    cli_value = TRUE;
                }
                g_free (txt);
                txt = nwamui_enm_get_stop_command (NWAMUI_ENM(obj));

                gtk_entry_set_text (prv->stop_cmd_entry, txt?txt:"");

                if ( txt != NULL && strlen(txt) > 0 ) {
                    stop_value = TRUE;
                    cli_value = TRUE;
                }
                g_free (txt);
                txt = nwamui_enm_get_smf_fmri (NWAMUI_ENM(obj));

                gtk_entry_set_text (prv->process_entry, txt?txt:"");

                if ( txt != NULL && strlen(txt) > 0 ) {
                    fmri_value = TRUE;
                }
                g_free (txt);

                g_signal_handlers_unblock_by_func(prv->start_cmd_entry,
                  (gpointer)command_entry_changed, data);
                g_signal_handlers_unblock_by_func(prv->stop_cmd_entry,
                  (gpointer)command_entry_changed, data);
                g_signal_handlers_unblock_by_func(prv->process_entry,
                  (gpointer)command_entry_changed, data);

                if (!(start_value || stop_value || fmri_value)) {
                    init_editting = TRUE;
                    /* Fixed the new added obj which is inactive make start btn
                     * sensitive in the above code.
                     */
                    gtk_widget_set_sensitive (GTK_WIDGET(prv->start_btn), FALSE);
                }

                gtk_widget_set_sensitive (GTK_WIDGET(prv->vpn_cli_rb), TRUE);
                gtk_widget_set_sensitive (GTK_WIDGET(prv->vpn_smf_rb), TRUE);

                gtk_widget_set_sensitive (GTK_WIDGET(prv->start_cmd_entry), cli_value || init_editting);
                gtk_widget_set_sensitive (GTK_WIDGET(prv->browse_start_cmd_btn), cli_value || init_editting);
                gtk_widget_set_sensitive (GTK_WIDGET(prv->stop_cmd_entry), cli_value || init_editting);
                gtk_widget_set_sensitive (GTK_WIDGET(prv->browse_stop_cmd_btn), cli_value || init_editting);
                gtk_widget_set_sensitive (GTK_WIDGET(prv->process_entry), fmri_value || init_editting);

                gtk_toggle_button_toggled(GTK_TOGGLE_BUTTON(prv->vpn_cli_rb));
                if (cli_value || init_editting) {
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prv->vpn_cli_rb), TRUE);
                } else {
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prv->vpn_smf_rb), TRUE);
                }

            }
            return;
		}
    }

    g_signal_handlers_block_by_func(prv->start_cmd_entry,
      (gpointer)command_entry_changed, data);
    g_signal_handlers_block_by_func(prv->stop_cmd_entry,
      (gpointer)command_entry_changed, data);
    g_signal_handlers_block_by_func(prv->process_entry,
      (gpointer)command_entry_changed, data);
    gtk_entry_set_text (prv->start_cmd_entry, "");
    gtk_entry_set_text (prv->stop_cmd_entry, "");
    gtk_entry_set_text (prv->process_entry, "");
    g_signal_handlers_unblock_by_func(prv->start_cmd_entry,
      (gpointer)command_entry_changed, data);
    g_signal_handlers_unblock_by_func(prv->stop_cmd_entry,
      (gpointer)command_entry_changed, data);
    g_signal_handlers_unblock_by_func(prv->process_entry,
      (gpointer)command_entry_changed, data);
    gtk_widget_set_sensitive (GTK_WIDGET(prv->start_btn), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET(prv->stop_btn), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET(prv->vpn_rules_btn), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET(prv->remove_btn), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET(prv->rename_btn), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET(prv->browse_start_cmd_btn), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET(prv->browse_stop_cmd_btn), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET(prv->start_cmd_entry), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET(prv->stop_cmd_entry), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET(prv->process_entry), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET(prv->start_cmd_lbl), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET(prv->stop_cmd_lbl), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET(prv->process_lbl), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET(prv->desc_lbl), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET(prv->vpn_conditional_cb), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET(prv->vpn_rules_btn), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET(prv->vpn_cli_rb), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET(prv->vpn_smf_rb), FALSE);
}

static void
on_rules_button_clicked(GtkButton *button, gpointer user_data)
{
	NwamVPNPrefDialogPrivate *prv = GET_PRIVATE(user_data);
    GtkTreeModel*               model;
    GtkTreeIter                 iter;

    if ( gtk_tree_selection_get_selected(gtk_tree_view_get_selection(prv->view), &model, &iter ) ) {
        NwamuiEnm *enm;

        gtk_tree_model_get(model, &iter, 0, &enm, -1);

        if (button == (gpointer)prv->vpn_rules_btn) {
            NwamPrefIFace *rules_dialog = NWAM_PREF_IFACE(nwam_rules_dialog_new());
            nwam_pref_refresh(NWAM_PREF_IFACE(rules_dialog), NWAMUI_OBJECT(enm), TRUE);
            nwam_pref_dialog_run(rules_dialog, GTK_WIDGET(prv->vpn_pref_dialog));
            g_object_unref(rules_dialog);
        }

        g_object_unref(enm);
    }
}

static void
conditional_toggled_cb(GtkToggleButton *button, gpointer user_data)
{
	NwamVPNPrefDialogPrivate *prv = GET_PRIVATE(user_data);
    GtkTreeModel*             model;
    GtkTreeIter               iter;
    gboolean                  toggled;

    toggled = gtk_toggle_button_get_active(button);

    gtk_widget_set_sensitive (GTK_WIDGET(prv->vpn_rules_btn), toggled);
    /* Re-evaluate the status of start/stop buttons. */
    if (toggled) {
        gtk_widget_set_sensitive(GTK_WIDGET(prv->start_btn), !toggled);
        gtk_widget_set_sensitive(GTK_WIDGET(prv->stop_btn), !toggled);
    } else {
        if (nwamui_object_get_active(NWAMUI_OBJECT(prv->cur_obj))) {
            gtk_widget_set_sensitive(GTK_WIDGET(prv->start_btn), FALSE);
            gtk_widget_set_sensitive(GTK_WIDGET(prv->stop_btn), TRUE);
        } else {
            gtk_widget_set_sensitive(GTK_WIDGET(prv->start_btn), TRUE);
            gtk_widget_set_sensitive(GTK_WIDGET(prv->stop_btn), FALSE);
        }
    }

    /* if ( gtk_tree_selection_get_selected(gtk_tree_view_get_selection(prv->view), &model, &iter ) ) { */
    /*     NwamuiObject                  *obj; */

    /*     gtk_tree_model_get(model, &iter, 0, &obj, -1); */
        
    /*     if (toggled) { */
    /*         /\* Default set to condition any when changing from others. *\/ */
    /*         nwamui_object_set_activation_mode(obj, NWAMUI_COND_ACTIVATION_MODE_CONDITIONAL_ANY); */
    /*     } else { */
    /*         nwamui_object_set_activation_mode(obj, NWAMUI_COND_ACTIVATION_MODE_MANUAL); */
    /*     } */
    /*     g_object_unref(obj); */
    /* } */
}

static void
on_radio_button_toggled(GtkToggleButton *button, gpointer user_data)
{
	NwamVPNPrefDialogPrivate *prv = GET_PRIVATE(user_data);
    gboolean toggled;

    toggled = gtk_toggle_button_get_active(button);

    gtk_widget_set_sensitive (GTK_WIDGET(prv->start_cmd_entry), toggled);
    gtk_widget_set_sensitive (GTK_WIDGET(prv->browse_start_cmd_btn), toggled);
    gtk_widget_set_sensitive (GTK_WIDGET(prv->start_cmd_lbl), toggled);

    gtk_widget_set_sensitive (GTK_WIDGET(prv->stop_cmd_entry), toggled);
    gtk_widget_set_sensitive (GTK_WIDGET(prv->browse_stop_cmd_btn), toggled);
    gtk_widget_set_sensitive (GTK_WIDGET(prv->stop_cmd_lbl), toggled);

    gtk_widget_set_sensitive (GTK_WIDGET(prv->process_entry), !toggled);
    gtk_widget_set_sensitive (GTK_WIDGET(prv->process_lbl), !toggled);
    gtk_widget_set_sensitive (GTK_WIDGET(prv->desc_lbl), !toggled);
}
