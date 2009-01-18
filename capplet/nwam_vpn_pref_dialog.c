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
 */
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n.h>

#include "libnwamui.h"
#include "capplet-utils.h"
#include "nwam_pref_iface.h"
#include "nwam_vpn_pref_dialog.h"
#include "nwam_rules_dialog.h"

#define VPN_APP_VIEW_COL_ID	"nwam_vpn_app_column_id"

/* Names of Widgets in Glade file */
#define	VPN_PREF_DIALOG_NAME	"vpn_config"
#define	VPN_PREF_TREE_VIEW	"vpn_apps_list"
#define	VPN_PREF_ADD_BTN	"vpn_add_btn"
#define	VPN_PREF_REMOVE_BTN	"vpn_remove_btn"
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

enum {
	APP_NAME=0,
	APP_STATE,
};

#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
	NWAM_TYPE_VPN_PREF_DIALOG, NwamVPNPrefDialogPrivate)) 
	
struct _NwamVPNPrefDialogPrivate {
	/* Widget Pointers */
	GtkDialog	*vpn_pref_dialog;
	GtkTreeView	*view;
	GtkButton	*add_btn;
	GtkButton	*remove_btn;
	GtkButton	*start_btn;
	GtkButton	*stop_btn;
	GtkCheckButton	*vpn_conditional_cb;
	GtkButton	*vpn_rules_btn;
	GtkButton	*browse_start_cmd_btn;
	GtkButton	*browse_stop_cmd_btn;
	GtkEntry	*start_cmd_entry;
	GtkEntry	*stop_cmd_entry;
	GtkEntry	*process_entry;
	GtkLabel	*start_cmd_lbl;
	GtkLabel	*stop_cmd_lbl;
	GtkLabel	*process_lbl;
	GtkLabel	*desc_lbl;
	
	/* nwam data related */
	NwamuiDaemon	*daemon;
	//GList	*enm_list;
	GObject	*cur_obj;	/* current selection of tree */
};

static void nwam_pref_init (gpointer g_iface, gpointer iface_data);
static gboolean refresh(NwamPrefIFace *iface, gpointer user_data, gboolean force);
static gboolean apply(NwamPrefIFace *iface, gpointer user_data);
static gboolean help(NwamPrefIFace *iface, gpointer user_data);
extern gint dialog_run(NwamPrefIFace *iface, GtkWindow *parent);

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
static void nwam_vpn_cell_cb(GtkTreeViewColumn *col,
	GtkCellRenderer   *renderer,
	GtkTreeModel      *model,
	GtkTreeIter       *iter,
	gpointer           data);
static void nwam_vpn_cell_edited_cb(GtkCellRendererText *renderer,
	gchar               *path,
	gchar               *new_text,
	gpointer             user_data);
static gint nwam_vpn_cell_comp_cb(GtkTreeModel *model,
	GtkTreeIter *a,
	GtkTreeIter *b,
	gpointer user_data);
static void nwam_vpn_selection_changed(GtkTreeSelection *selection,
	gpointer          data);
static void on_nwam_enm_notify_cb(GObject *gobject, GParamSpec *arg1, gpointer data);
static void on_button_clicked(GtkButton *button, gpointer user_data);

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
    iface->help = help;
    iface->dialog_run = dialog_run;
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
	prv->start_btn = GTK_BUTTON(nwamui_util_glade_get_widget(VPN_PREF_START_BTN));
	prv->stop_btn = GTK_BUTTON(nwamui_util_glade_get_widget(VPN_PREF_STOP_BTN));

	prv->vpn_rules_btn = GTK_BUTTON(nwamui_util_glade_get_widget(VPN_RULES_BTN));
	prv->vpn_conditional_cb = GTK_CHECK_BUTTON(nwamui_util_glade_get_widget(VPN_CONDITIONAL_CB));

	prv->browse_start_cmd_btn = GTK_BUTTON(nwamui_util_glade_get_widget(VPN_PREF_BROWSE_START_CMD_BTN));
	prv->browse_stop_cmd_btn = GTK_BUTTON(nwamui_util_glade_get_widget(VPN_PREF_BROWSE_STOP_CMD_BTN));
	prv->start_cmd_entry = GTK_ENTRY(nwamui_util_glade_get_widget(VPN_PREF_START_CMD_ENTRY));
	prv->stop_cmd_entry = GTK_ENTRY(nwamui_util_glade_get_widget(VPN_PREF_STOP_CMD_ENTRY));
	prv->process_entry = GTK_ENTRY(nwamui_util_glade_get_widget(VPN_PREF_PROCESS_ENTRY));
	prv->start_cmd_lbl = GTK_LABEL(nwamui_util_glade_get_widget(VPN_PREF_START_CMD_LBL));
	prv->stop_cmd_lbl = GTK_LABEL(nwamui_util_glade_get_widget(VPN_PREF_STOP_CMD_LBL));
	prv->process_lbl = GTK_LABEL(nwamui_util_glade_get_widget(VPN_PREF_PROCESS_LBL));
	prv->desc_lbl = GTK_LABEL(nwamui_util_glade_get_widget(VPN_PREF_DESC_LBL));

	g_signal_connect(prv->remove_btn,
      "clicked", (GCallback)vpn_pref_clicked_cb, (gpointer)self);
	g_signal_connect(prv->start_btn,
      "clicked", (GCallback)vpn_pref_clicked_cb, (gpointer)self);
	g_signal_connect(prv->stop_btn,
      "clicked", (GCallback)vpn_pref_clicked_cb, (gpointer)self);
	g_signal_connect(prv->browse_start_cmd_btn,
      "clicked", (GCallback)vpn_pref_clicked_cb, (gpointer)self);
	g_signal_connect(prv->browse_stop_cmd_btn,
      "clicked", (GCallback)vpn_pref_clicked_cb, (gpointer)self);
	

	g_signal_connect(self, "notify", (GCallback)object_notify_cb, NULL);
	g_signal_connect(prv->vpn_pref_dialog, "response", (GCallback)response_cb, (gpointer)self);
	
    g_signal_connect(self->prv->vpn_rules_btn,
      "clicked", G_CALLBACK(on_button_clicked),
      (gpointer)self);

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
nwam_compose_tree_view (NwamVPNPrefDialog *self)
{
	GtkTreeViewColumn *col;
	GtkCellRenderer *renderer;
	GtkTreeView *view = self->prv->view;
	
    g_object_set (G_OBJECT(view),
      "headers-clickable", TRUE,
      "headers-visible", TRUE,
      "rules-hint", FALSE,
      "reorderable", FALSE,
      "enable-search", TRUE,
      "show-expanders", TRUE,
      "button-add", self->prv->add_btn,
      NULL);

	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(view),
      GTK_SELECTION_SINGLE);
    g_signal_connect(gtk_tree_view_get_selection(view),
      "changed",
      G_CALLBACK(nwam_vpn_selection_changed),
      (gpointer)self);

    /* Mode column */
    capplet_active_mode_column_new(view, &col, NULL, &renderer);

	/* column APP_NAME */
    capplet_name_column_new(view, &col, _("Application Name"), &renderer);

	g_object_set(G_OBJECT(renderer), "editable", TRUE, NULL);
	g_object_set_data(G_OBJECT(renderer), VPN_APP_VIEW_COL_ID, GUINT_TO_POINTER(APP_NAME));
	g_signal_connect(G_OBJECT(renderer), "edited",
      (GCallback) nwam_vpn_cell_edited_cb, (gpointer) self);

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
      "sort-indicator", TRUE,
      "reorderable", TRUE,
      NULL);
	gtk_tree_view_column_set_sort_column_id (col, APP_STATE);	

    /* First cell */
	renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);

	gtk_tree_view_column_set_cell_data_func (col,
      renderer,
      nwam_vpn_cell_cb,
      (gpointer) self,
      NULL);

    capplet_compose_nwamui_obj_treeview(view);
}

/* FIXME, if update failed, popup dialog and return FALSE */
static gboolean
nwam_update_obj (NwamVPNPrefDialog *self, GObject *obj)
{
	NwamVPNPrefDialogPrivate *prv = GET_PRIVATE(self);
	const gchar *txt = NULL;
	
	txt = gtk_entry_get_text(prv->start_cmd_entry);
	nwamui_enm_set_start_command(NWAMUI_ENM(prv->cur_obj), txt?txt:"");
	txt = gtk_entry_get_text(prv->stop_cmd_entry);
	nwamui_enm_set_stop_command(NWAMUI_ENM(prv->cur_obj), txt?txt:"");
	txt = gtk_entry_get_text(prv->process_entry);
	nwamui_enm_set_smf_fmri(NWAMUI_ENM(prv->cur_obj), txt?txt:"");
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
		response =  gtk_dialog_run(GTK_DIALOG(self->prv->vpn_pref_dialog));
		
		gtk_widget_hide( GTK_WIDGET(self->prv->vpn_pref_dialog) );
	}
	return(response);
}

static gboolean
refresh(NwamPrefIFace *iface, gpointer user_data, gboolean force)
{        
	NwamVPNPrefDialog* self = NWAM_VPN_PREF_DIALOG(iface);
	NwamVPNPrefDialogPrivate *prv = GET_PRIVATE(iface);
	
    NwamuiDaemon *daemon = nwamui_daemon_get_instance();
    capplet_update_nwamui_obj_treeview(prv->view, daemon, NWAMUI_TYPE_ENM);
    g_object_unref(daemon);


/*     g_signal_connect (idx->data, "notify::active", */
/*       G_CALLBACK(on_nwam_enm_notify_cb), (gpointer)self); */

    if (force) {
    }
}

static gboolean
help(NwamPrefIFace *iface, gpointer user_data)
{
    g_debug ("NwamVPNPrefDialog: Help");
    nwamui_util_show_help ("");
}

/* call backs */
static void
object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
	g_debug("NwamVPNPrefDialog: notify %s changed\n", arg1->name);
}

static void
response_cb(GtkWidget* widget, gint responseid, gpointer data)
{
	NwamVPNPrefDialogPrivate *prv = GET_PRIVATE(data);
    gboolean            stop_emission = FALSE;
	GtkTreeSelection *selection = NULL;
	GtkTreeIter iter;
    GtkTreeModel *model;
    GObject *obj;

	switch (responseid) {
		case GTK_RESPONSE_NONE:
			g_debug("GTK_RESPONSE_NONE");
			break;
		case GTK_RESPONSE_DELETE_EVENT:
			g_debug("GTK_RESPONSE_DELETE_EVENT");
			break;
		case GTK_RESPONSE_OK:
			g_debug("GTK_RESPONSE_OK");

            model = gtk_tree_view_get_model (prv->view);
            selection = gtk_tree_view_get_selection (prv->view);
            if (gtk_tree_selection_get_selected(selection,
                  NULL, &iter)) {

                gtk_tree_model_get (model, &iter, 0, &obj, -1);
                /* update the new one before close */
                if (prv->cur_obj && prv->cur_obj == obj) {
                    nwam_update_obj (NWAM_VPN_PREF_DIALOG(data), obj);
                }
            }

			/* FIXME, ok need call into separated panel/instance
			 * apply all changes, if no errors, hide all
			 */
            if (gtk_tree_model_get_iter_first (model, &iter)) {
                do {
                    gtk_tree_model_get (model, &iter, 0, &obj, -1);
                    if (!nwamui_enm_commit (NWAMUI_ENM (obj))) {
                        gchar *name = nwamui_enm_get_name (NWAMUI_ENM (obj));
                        gchar *msg = g_strdup_printf (_("Committing %s faild..."), name);
                        nwamui_util_show_message (prv->vpn_pref_dialog,
                          GTK_MESSAGE_ERROR,
                          _("Commit ENM error"),
                          msg);
                        g_free (msg);
                        g_free (name);
                        return;
                    }
                } while (gtk_tree_model_iter_next (model, &iter));
                gtk_widget_hide (GTK_WIDGET(prv->vpn_pref_dialog));
                stop_emission = TRUE;
            }
			break;
		case GTK_RESPONSE_CANCEL:
			g_debug("GTK_RESPONSE_CANCEL");
			gtk_widget_hide (GTK_WIDGET(prv->vpn_pref_dialog));
            stop_emission = TRUE;
			break;
		case GTK_RESPONSE_HELP:
            nwam_pref_help (NWAM_PREF_IFACE(data), NULL);
            stop_emission = TRUE;
			break;
	}
    if ( stop_emission ) {
        g_signal_stop_emission_by_name(widget, "response" );
    }
}

static void
vpn_pref_clicked_cb (GtkButton *button, gpointer data)
{
	NwamVPNPrefDialog* self = NWAM_VPN_PREF_DIALOG(data);
	GtkTreeSelection *selection = NULL;
	GtkTreeModel *model = NULL;
	NwamuiEnm *obj;
	GtkTreeIter iter;
	
    if (button == self->prv->remove_btn) {
		GtkTreeSelection *selection;
		GList *list, *idx;
		GtkTreeModel *model;

		model = gtk_tree_view_get_model (self->prv->view);
		selection = gtk_tree_view_get_selection (self->prv->view);
		list = gtk_tree_selection_get_selected_rows (selection, NULL);

		for (idx=list; idx; idx = g_list_next(idx)) {
			GtkTreeIter  iter;
			g_assert (idx->data);
			if (gtk_tree_model_get_iter(model, &iter, (GtkTreePath *)idx->data)) {
				gtk_tree_model_get (model, &iter, 0, &obj, -1);
				gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
				if (obj) {
                    nwamui_daemon_enm_remove (self->prv->daemon, obj);
                }
			} else {
				g_assert_not_reached ();
			}
			gtk_tree_path_free ((GtkTreePath *)idx->data);
		}
		g_list_free (list);
		return;
	}
	
	selection = gtk_tree_view_get_selection (self->prv->view);
	model = gtk_tree_view_get_model (self->prv->view);
	gtk_tree_selection_get_selected(selection, NULL, &iter);
	gtk_tree_model_get (model, &iter, 0, &obj, -1);
	
	if (button == self->prv->browse_start_cmd_btn || button == self->prv->browse_stop_cmd_btn) {
		GtkWidget *toplevel;
		GtkWidget *filedlg = NULL;
		GtkEntry *active_entry;
		gchar *title = NULL;
	
		toplevel = gtk_widget_get_toplevel (GTK_WIDGET(button));
		if (!GTK_WIDGET_TOPLEVEL(toplevel)) {
			toplevel = NULL;
		}
		if (button == self->prv->browse_start_cmd_btn) {
			active_entry = self->prv->start_cmd_entry;
			title = _("Select start command");
		} else if (button == self->prv->browse_stop_cmd_btn) {
			active_entry = self->prv->stop_cmd_entry;
			title = _("Select stop command");
		}
		filedlg = gtk_file_chooser_dialog_new(title,
			toplevel,
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

	/* We should not set sensitive of start/stop buttons after
	 * trigger it, we should wait the sigal if there has.
	 */
	if (button == self->prv->start_btn) {
		nwamui_enm_set_active (obj, TRUE);
	} else if (button == self->prv->stop_btn) {
		nwamui_enm_set_active (obj, FALSE);
	}
}

static void
nwam_vpn_cell_cb (GtkTreeViewColumn *col,
		GtkCellRenderer   *renderer,
		GtkTreeModel      *model,
		GtkTreeIter       *iter,
		gpointer           data)
{
	NwamVPNPrefDialog*self = NWAM_VPN_PREF_DIALOG(data);
	GObject *obj;
	
	gtk_tree_model_get(GTK_TREE_MODEL(model), iter, 0, &obj, -1);

	switch (gtk_tree_view_column_get_sort_column_id(col)) {
		case APP_STATE: {
			if (obj) {
				gboolean is_active;
				gchar *status;
				is_active = nwamui_enm_get_active (NWAMUI_ENM(obj));
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
		break;
		default:
			g_assert_not_reached();
	}
}

static void
nwam_vpn_cell_edited_cb (GtkCellRendererText *renderer,
			gchar               *path,
                        gchar               *new_text,
                        gpointer             data)
{
	NwamVPNPrefDialogPrivate *prv = GET_PRIVATE(data);
	guint col_id = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(renderer), VPN_APP_VIEW_COL_ID));
	GtkTreeModel *model = gtk_tree_view_get_model(prv->view);
	GtkTreeIter iter;
	GObject *obj;
	
	gtk_tree_model_get_iter_from_string(model, &iter, path);
	gtk_tree_model_get(model, &iter, 0, &obj, -1);
	
	switch (col_id) {
		case APP_NAME:
			if (new_text && *new_text != '\0') {
				if (obj) {
					nwamui_enm_set_name(NWAMUI_ENM(obj), new_text);
				} else {
					obj = nwamui_enm_new(new_text, FALSE, NULL, NULL, NULL);
					gtk_list_store_set(GTK_TREE_MODEL(model), &iter,
						0,  obj, -1);
					/* assign the new one to the current selection */
					prv->cur_obj = obj;
				}
			}
			break;
		default:
			g_assert_not_reached();
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

static void
nwam_vpn_selection_changed(GtkTreeSelection *selection,
	gpointer          data)
{
	NwamVPNPrefDialogPrivate *prv = GET_PRIVATE(data);
	
	GtkTreeIter iter;
	
	if (gtk_tree_selection_get_selected(selection, NULL, &iter)) {
		GtkTreeModel *model = gtk_tree_view_get_model (prv->view);
		NwamuiEnm *obj;
		gchar *txt = NULL;

		gtk_tree_model_get (model, &iter, 0, &obj, -1);

		/* FIXME handle the old selection first, should assert the currect data */
		if (prv->cur_obj && prv->cur_obj != obj) {
			nwam_update_obj (NWAM_VPN_PREF_DIALOG(data), prv->cur_obj);
		}
		prv->cur_obj = obj;

		gtk_widget_set_sensitive (GTK_WIDGET(prv->remove_btn), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET(prv->browse_start_cmd_btn), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET(prv->browse_stop_cmd_btn), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET(prv->start_cmd_entry), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET(prv->stop_cmd_entry), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET(prv->process_entry), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET(prv->start_cmd_lbl), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET(prv->stop_cmd_lbl), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET(prv->process_lbl), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET(prv->desc_lbl), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET(prv->vpn_conditional_cb), TRUE);
		
		if (obj) {
            gchar *title;
            gchar *name;

			txt = nwamui_enm_get_start_command (obj);
			gtk_entry_set_text (prv->start_cmd_entry, txt?txt:"");
			g_free (txt);
			txt = nwamui_enm_get_stop_command (obj);
			gtk_entry_set_text (prv->stop_cmd_entry, txt?txt:"");
			g_free (txt);
			txt = nwamui_enm_get_smf_fmri (obj);
			gtk_entry_set_text (prv->process_entry, txt?txt:"");
			g_free (txt);
            on_nwam_enm_notify_cb(obj, NULL, data);

            name = nwamui_object_get_name(NWAMUI_OBJECT(obj));
            title = g_strdup_printf(_("Start/stop '%s' according to rules"), name);
            g_object_set(prv->vpn_conditional_cb, "label", title, NULL);

            if (nwamui_enm_get_active (obj)) {
                gtk_widget_set_sensitive (GTK_WIDGET(prv->start_btn), FALSE);
                gtk_widget_set_sensitive (GTK_WIDGET(prv->stop_btn), TRUE);
            } else {
                gtk_widget_set_sensitive (GTK_WIDGET(prv->start_btn), TRUE);
                gtk_widget_set_sensitive (GTK_WIDGET(prv->stop_btn), FALSE);
            }
            if (nwamui_enm_get_activation_mode(obj) == NWAMUI_COND_ACTIVATION_MODE_CONDITIONAL_ANY) {
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prv->vpn_conditional_cb), TRUE);
            } else {
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prv->vpn_conditional_cb), FALSE);
            }

            g_free(name);
            g_free(title);

		} else {
			gtk_entry_set_text (prv->start_cmd_entry, "");
			gtk_entry_set_text (prv->stop_cmd_entry, "");			
			gtk_entry_set_text (prv->process_entry, "");
            gtk_widget_set_sensitive (GTK_WIDGET(prv->start_btn), FALSE);
            gtk_widget_set_sensitive (GTK_WIDGET(prv->stop_btn), FALSE);
		}
		return;
	}
	gtk_widget_set_sensitive (GTK_WIDGET(prv->remove_btn), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET(prv->start_btn), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET(prv->stop_btn), FALSE);
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
}

static void
on_nwam_enm_notify_cb(GObject *gobject, GParamSpec *arg1, gpointer data)
{
	NwamVPNPrefDialogPrivate *prv = GET_PRIVATE(data);
    NwamuiEnm *nwamobj;
	GtkTreeIter iter;
    GtkTreeSelection *selection;
    GtkTreeModel *model;
	
    selection = gtk_tree_view_get_selection (prv->view);
    model = gtk_tree_view_get_model (prv->view);
	if (gtk_tree_selection_get_selected(selection,
                                        NULL, &iter)) {
        GtkTreePath* path;
        path = gtk_tree_model_get_path (model, &iter);
        gtk_tree_model_row_changed (model, path, &iter);
        gtk_tree_path_free (path);
    } else {
        g_assert_not_reached();
    }
}

static void
on_button_clicked(GtkButton *button, gpointer user_data)
{
	NwamVPNPrefDialogPrivate *prv = GET_PRIVATE(user_data);
    GtkTreeModel*               model;
    GtkTreeIter                 iter;
    static NwamRulesDialog *rules_dialog = NULL;

    if ( gtk_tree_selection_get_selected(gtk_tree_view_get_selection(prv->view), &model, &iter ) ) {
        NwamuiEnm *enm;

        gtk_tree_model_get(model, &iter, 0, &enm, -1);

        /* TODO */
        if (button == (gpointer)prv->vpn_rules_btn) {
            if (rules_dialog == NULL)
                rules_dialog = nwam_rules_dialog_new();

            nwam_pref_refresh(NWAM_PREF_IFACE(rules_dialog), NWAMUI_OBJECT(enm), TRUE);
            nwam_rules_dialog_run(rules_dialog, GTK_WINDOW(prv->vpn_pref_dialog));
        }

        g_object_unref(enm);
    }
}
