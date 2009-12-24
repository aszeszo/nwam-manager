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
 * File:   nwam_wireless_dialog.c
 *
 * Created on May 9, 2007, 11:01 AM
 *
 */

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <libnwamui.h>

#include "capplet-utils.h"
#include "nwam_pref_iface.h"
#include "nwam_pref_dialog.h"
#include "nwam_conn_conf_ip_panel.h"
#include "nwam_conn_stat_panel.h"
#include "nwam_profile_panel.h"

/* Names of Widgets in Glade file */
#define CAPPLET_DIALOG_NAME           "nwam_capplet"
#define CAPPLET_DIALOG_SHOW_COMBO     "show_combo"
#define CAPPLET_DIALOG_MAIN_NOTEBOOK  "mainview_notebook"

#define NWAM_CAPPLET_DIALOG_GET_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), NWAM_TYPE_CAPPLET_DIALOG, NwamCappletDialogPrivate))

struct _NwamCappletDialogPrivate {
	/* Widget Pointers */
	GtkDialog*                  capplet_dialog;
	GtkComboBox*                show_combo;
	GtkNotebook*                main_nb;

    /* Panel Objects */
	NwamPrefIFace* panel[N_PANELS];
                
    /* Other Data */
    NwamuiNcp*                  active_ncp; /* currently active NCP */
    GList*                      ncp_list; /* currently active NCP */

    NwamuiNcu*                  selected_ncu;
    NwamuiNcu*                  prev_selected_ncu;
};

static void nwam_pref_init (gpointer g_iface, gpointer iface_data);
static gboolean refresh(NwamPrefIFace *iface, gpointer user_data, gboolean force);
static gboolean apply(NwamPrefIFace *iface, gpointer user_data);
static gboolean cancel(NwamPrefIFace *iface, gpointer user_data);
static gboolean help(NwamPrefIFace *iface, gpointer user_data);
static gint dialog_run(NwamPrefIFace *iface, GtkWindow *parent);
static GtkWindow* dialog_get_window(NwamPrefIFace *iface);

static void nwam_capplet_dialog_finalize(NwamCappletDialog *self);

/* Callbacks */
static void show_combo_cell_cb (GtkCellLayout *cell_layout,
  GtkCellRenderer   *renderer,
  GtkTreeModel      *model,
  GtkTreeIter       *iter,
  gpointer           data);
static gboolean show_combo_separator_cb (GtkTreeModel *model,
  GtkTreeIter *iter,
  gpointer data);

static void object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);
static void response_cb( GtkWidget* widget, gint repsonseid, gpointer data );
static void show_changed_cb( GtkWidget* widget, gpointer data );

/* Utility Functions */
static void show_combo_add (GtkComboBox* combo, GObject*  obj);
static void show_combo_remove (GtkComboBox* combo, GObject*  obj);

G_DEFINE_TYPE_EXTENDED (NwamCappletDialog,
                        nwam_capplet_dialog,
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
nwam_capplet_dialog_class_init(NwamCappletDialogClass *klass)
{
	/* Pointer to GObject Part of Class */
	GObjectClass *gobject_class = (GObjectClass*) klass;
	
	/* Override Some Function Pointers */
	gobject_class->finalize = (void (*)(GObject*)) nwam_capplet_dialog_finalize;

	g_type_class_add_private(klass, sizeof(NwamCappletDialogPrivate));
}

static void
nwam_capplet_dialog_init(NwamCappletDialog *self)
{
    NwamCappletDialogPrivate *prv = NWAM_CAPPLET_DIALOG_GET_PRIVATE(self);
    GtkButton      *btn = NULL;
    NwamuiDaemon   *daemon = NULL;

    self->prv = prv;
    
    /* Iniialise pointers to important widgets */
    prv->capplet_dialog = GTK_DIALOG(nwamui_util_glade_get_widget(CAPPLET_DIALOG_NAME));
    prv->show_combo = GTK_COMBO_BOX(nwamui_util_glade_get_widget(CAPPLET_DIALOG_SHOW_COMBO));
    prv->main_nb = GTK_NOTEBOOK(nwamui_util_glade_get_widget(CAPPLET_DIALOG_MAIN_NOTEBOOK));
        
    /* Get NCPs and Current NCP */
    daemon = nwamui_daemon_get_instance();
    prv->active_ncp = nwamui_daemon_get_active_ncp( daemon );
    prv->ncp_list = nwamui_daemon_get_ncp_list( daemon );

    prv->selected_ncu = NULL;
    prv->prev_selected_ncu = NULL;

    if ( prv->active_ncp == NULL && prv->ncp_list  != NULL ) {
        GList* first_element = g_list_first( prv->ncp_list );


        if ( first_element != NULL && first_element->data != NULL )  {
            prv->active_ncp = NWAMUI_NCP(g_object_ref(G_OBJECT(first_element->data)));
        }
        
    }

    g_object_unref( daemon );
    daemon = NULL;

    /* Set title to include hostname */
    nwamui_util_window_title_append_hostname( prv->capplet_dialog );

    /* Construct the Notebook Panels Handling objects. */
    prv->panel[PANEL_CONN_STATUS] = NWAM_PREF_IFACE(nwam_conn_status_panel_new( self ));
    prv->panel[PANEL_PROF_PREF] = NWAM_PREF_IFACE(nwam_profile_panel_new(self));
    prv->panel[PANEL_CONF_IP] = NWAM_PREF_IFACE(nwam_conf_ip_panel_new(self));

    /* Change Model */
	capplet_compose_combo(prv->show_combo,
      GTK_TYPE_TREE_STORE,
      G_TYPE_OBJECT,
      show_combo_cell_cb,
      show_combo_separator_cb,
      (GCallback)show_changed_cb,
      (gpointer)self,
      NULL);

	show_combo_add (prv->show_combo, G_OBJECT(prv->panel[PANEL_CONN_STATUS]));
	show_combo_add (prv->show_combo, G_OBJECT(prv->panel[PANEL_PROF_PREF]));

    g_signal_connect(self, "notify", (GCallback)object_notify_cb, NULL);
    g_signal_connect(GTK_DIALOG(prv->capplet_dialog), "response", (GCallback)response_cb, (gpointer)self);
    
    /* Sync nwam_capplet_dialog_set_ok_sensitive_by_voting count. */
    gtk_dialog_set_response_sensitive(prv->capplet_dialog, GTK_RESPONSE_OK, TRUE);

    /* default select "Connection Status" */
    gtk_combo_box_set_active (GTK_COMBO_BOX(prv->show_combo), 0);

    /* Initial */
    nwam_pref_refresh(NWAM_PREF_IFACE(self), NULL, TRUE);
}


/**
 * nwam_capplet_dialog_new:
 * @returns: a new #NwamCappletDialog.
 *
 * Creates a new #NwamCappletDialog with an empty ncu
 **/
NwamCappletDialog*
nwam_capplet_dialog_new(void)
{
	/* capplet dialog should be single */
	static NwamCappletDialog *capplet_dialog = NULL;
	if (capplet_dialog) {
		return NWAM_CAPPLET_DIALOG (g_object_ref (G_OBJECT (capplet_dialog)));
	}
	return capplet_dialog = NWAM_CAPPLET_DIALOG(g_object_new(NWAM_TYPE_CAPPLET_DIALOG, NULL));
}

/**
 * nwam_capplet_dialog_run:
 * @nwam_capplet_dialog: a #NwamCappletDialog.
 * @returns: a GTK_DIALOG Response ID
 *
 *
 * Blocks in a recursive main loop until the dialog either emits the response signal, or is destroyed.
 **/
static gint
dialog_run(NwamPrefIFace *iface, GtkWindow *parent)
{
	NwamCappletDialog* self = NWAM_CAPPLET_DIALOG(iface);
    gint response = GTK_RESPONSE_NONE;
    
    g_assert(NWAM_IS_CAPPLET_DIALOG(self));

    if ( self->prv->capplet_dialog != NULL ) {
            response = gtk_dialog_run(GTK_DIALOG(self->prv->capplet_dialog));
            
            gtk_widget_hide(GTK_WIDGET(self->prv->capplet_dialog));
    }
    return response;
}

static GtkWindow* 
dialog_get_window(NwamPrefIFace *iface)
{
	NwamCappletDialog* self = NWAM_CAPPLET_DIALOG(iface);

    if ( self->prv->capplet_dialog ) {
        return ( GTK_WINDOW(self->prv->capplet_dialog) );
    }

    return(NULL);
}

static void
nwam_capplet_dialog_finalize(NwamCappletDialog *self)
{
    NwamCappletDialogPrivate *prv = NWAM_CAPPLET_DIALOG_GET_PRIVATE(self);

    g_object_unref(G_OBJECT(prv->capplet_dialog));
    g_object_unref(G_OBJECT(prv->show_combo));
    g_object_unref(G_OBJECT(prv->main_nb));

	for (int i = 0; i < N_PANELS; i ++) {
		if (prv->panel[i]) {
			g_object_unref(G_OBJECT(prv->panel[i]));
		}
	}
	if ( prv->active_ncp != NULL )
		g_object_unref(G_OBJECT(prv->active_ncp));
	
    g_list_foreach(prv->ncp_list, nwamui_util_obj_unref, NULL );

	G_OBJECT_CLASS(nwam_capplet_dialog_parent_class)->finalize(G_OBJECT(self));
}

/* Callbacks */

/**
 * refresh:
 * @user_data: Must be NULL.
 * @force: TRUE: refresh the main dialog, actually update the show_combo.
 *
 * Refresh #NwamCappletDialog with the new connections.
 * Including two static enties "Connection Status" and "Network Profile"
 * And dynamic adding connection enties. Then refresh the current selected
 * object in the show_combo.
 **/
static gboolean
refresh(NwamPrefIFace *iface, gpointer user_data, gboolean force)
{
    NwamCappletDialogPrivate *prv      = NWAM_CAPPLET_DIALOG_GET_PRIVATE(iface);
	NwamCappletDialog*        self     = NWAM_CAPPLET_DIALOG(iface);
    GObject                  *obj      = capplet_combo_get_active_object(GTK_COMBO_BOX(prv->show_combo));
/*     gint                      panel_id = (gint)user_data; */

/*     switch(panel_id) { */
/*     case -1: */
/*         /\* if -1, show the first page. *\/ */
/*         panel_id = 0; */
/*     case PANEL_CONN_STATUS: */
/*     case PANEL_PROF_PREF: */
/*         obj = g_object_ref(prv->panel[panel_id]); */
/*         break; */
/*     default: */
/*         /\* Remember the active object and select it later. *\/ */
/*         obj = capplet_combo_get_active_object(GTK_COMBO_BOX(prv->show_combo)); */
/*         break; */
/*     } */
    
    if (force) {
        NwamuiDaemon *daemon = nwamui_daemon_get_instance();
        GList*        ncp_list;

        /* Refresh show-combo */
        ncp_list = nwamui_daemon_get_ncp_list(daemon);
        g_object_unref(daemon);

        gtk_widget_hide(GTK_WIDGET(prv->show_combo));
        for (; ncp_list;) {
            /* TODO need be more efficient. */
            show_combo_remove(prv->show_combo, G_OBJECT(ncp_list->data));
            show_combo_add(prv->show_combo, G_OBJECT(ncp_list->data));
            ncp_list = g_list_delete_link(ncp_list, ncp_list);
        }
        gtk_widget_show(GTK_WIDGET(prv->show_combo));
    }

    if (NWAM_IS_PREF_IFACE(obj)) {
        nwam_pref_refresh(NWAM_PREF_IFACE(obj), NULL, TRUE);
    } else if (NWAMUI_IS_NCU(obj)) {
        nwam_pref_refresh(NWAM_PREF_IFACE(prv->panel[PANEL_CONF_IP]), (gpointer)obj, TRUE);
    } else {
        g_assert_not_reached();
    }

    /* Refresh children */
    capplet_combo_set_active_object(GTK_COMBO_BOX(prv->show_combo), obj);

    g_object_unref(obj);
}

static gboolean
cancel(NwamPrefIFace *iface, gpointer user_data)
{
}

static gboolean
apply(NwamPrefIFace *iface, gpointer user_data)
{
	NwamCappletDialog  *self = NWAM_CAPPLET_DIALOG(iface);
    gboolean            rval = TRUE;
    NwamuiDaemon       *daemon = NULL;
    gint                cur_idx = gtk_notebook_get_current_page (NWAM_CAPPLET_DIALOG(self)->prv->main_nb);

    daemon = nwamui_daemon_get_instance();

    /* Ensure we don't have unsaved data */
    if ( !nwam_pref_apply (NWAM_PREF_IFACE(NWAM_CAPPLET_DIALOG(self)->prv->panel[cur_idx]), NULL) ) {
        rval = FALSE;
    }

    if ( self->prv->selected_ncu ) {
        gchar      *prop_name = NULL;

        /* Validate NCU */
        if ( !nwamui_ncu_validate( NWAMUI_NCU(self->prv->selected_ncu), &prop_name ) ) {
            gchar* message = g_strdup_printf(_("An error occurred validating the current NCU.\nThe property '%s' caused this failure"), prop_name );
            nwamui_util_show_message (GTK_WINDOW(self->prv->capplet_dialog), 
                                      GTK_MESSAGE_ERROR, _("Validation Error"), message, TRUE );
            g_free(prop_name);
            rval = FALSE;
        }
    }

    if (rval && !nwamui_object_commit(NWAMUI_OBJECT(daemon)) ) {
        rval = FALSE;
    }

    return( rval );
}

static gboolean
help(NwamPrefIFace *iface, gpointer user_data)
{
	NwamCappletDialog* self = NWAM_CAPPLET_DIALOG(iface);
    gint idx = gtk_notebook_get_current_page (NWAM_CAPPLET_DIALOG(self)->prv->main_nb);
    nwam_pref_help (NWAM_PREF_IFACE(NWAM_CAPPLET_DIALOG(self)->prv->panel[idx]), NULL);
}

/*
 * According to nwam_pref_iface defined apply, we should do
 * foreach instance apply it, if error, poping up error dialog and keep living
 * otherwise hiding myself.
 */
static void
response_cb( GtkWidget* widget, gint responseid, gpointer data )
{
	NwamCappletDialog* self = NWAM_CAPPLET_DIALOG(data);
    gboolean           stop_emission = FALSE;
	
	switch (responseid) {
		case GTK_RESPONSE_NONE:
			g_debug("GTK_RESPONSE_NONE");
            stop_emission = TRUE;
			break;
		case GTK_RESPONSE_DELETE_EVENT:
			g_debug("GTK_RESPONSE_DELETE_EVENT");
			break;
		case GTK_RESPONSE_OK:
			g_debug("GTK_RESPONSE_OK");
            if (nwam_pref_apply (NWAM_PREF_IFACE(self), NULL)) {
                gtk_widget_hide(GTK_WIDGET(self->prv->capplet_dialog));
            }
            else {
                /* TODO - report error to user */
                stop_emission = TRUE;
            }
			break;
		case GTK_RESPONSE_REJECT: /* Generated by Referesh Button */
			g_debug("GTK_RESPONSE_REJECT");
            nwam_pref_refresh(NWAM_PREF_IFACE(self), NULL, TRUE);
            stop_emission = TRUE;
			break;
		case GTK_RESPONSE_CANCEL:
			g_debug("GTK_RESPONSE_CANCEL");
            if (nwam_pref_cancel (NWAM_PREF_IFACE(self), NULL)) {
                gtk_widget_hide(GTK_WIDGET(self->prv->capplet_dialog));
            }
			break;
		case GTK_RESPONSE_HELP:
            nwam_pref_help(NWAM_PREF_IFACE(self), NULL);
            stop_emission = TRUE;
			break;
	}
    if ( stop_emission ) {
        g_signal_stop_emission_by_name(widget, "response" );
    }
}

static void
show_changed_cb( GtkWidget* widget, gpointer data )
{
	NwamCappletDialog* self = NWAM_CAPPLET_DIALOG(data);
    GType type;
    GtkTreeModel   *model = NULL;
	gint idx;
    GObject *obj;
    gpointer user_data = NULL;
    gboolean    valid = TRUE;

    idx = PANEL_CONF_IP;
        
    model = gtk_combo_box_get_model(GTK_COMBO_BOX(widget));

    /* Get selected NCU and update IP Panel with this information */
    if ((obj = capplet_combo_get_active_object(GTK_COMBO_BOX(widget))) == NULL) {
        /* Caused by updating the content of show combo. */
        return;
    }

    /* Track the previously selected ncu so that we can know when to validate
     * the ncu, specifically when changing from one ncu to another.
     */
    if ( self->prv->prev_selected_ncu != NULL ) {
        g_object_unref(self->prv->prev_selected_ncu);
    }
    self->prv->prev_selected_ncu = self->prv->selected_ncu;
    self->prv->selected_ncu = NULL;

    type = G_OBJECT_TYPE(obj);
    if (type == NWAM_TYPE_CONN_STATUS_PANEL) {
        idx = 0;
    } else if (type == NWAM_TYPE_PROFILE_PANEL) {
        user_data = g_object_ref(self->prv->active_ncp);
        idx = 1;
    } else if (type == NWAMUI_TYPE_NCU) {
        self->prv->selected_ncu = NWAMUI_NCU(g_object_ref(obj));
        user_data = obj;
        obj = G_OBJECT(self->prv->panel[PANEL_CONF_IP]);
        idx = 2;
    } else {
        g_assert_not_reached();
    }

    if ( self->prv->prev_selected_ncu != NULL &&
         self->prv->selected_ncu != self->prv->prev_selected_ncu ) {
        /* We are switching from an NCU to something else, so we should apply
         * any changes, and attempt to validate. If validation changes, then
         * we need to re-selected the previous item.
         */
        gint        cur_idx = gtk_notebook_get_current_page (NWAM_CAPPLET_DIALOG(self)->prv->main_nb);
        gchar      *prop_name = NULL;

        if ( !nwam_pref_apply (NWAM_PREF_IFACE(NWAM_CAPPLET_DIALOG(self)->prv->panel[cur_idx]), NULL) ) {
            /* Don't change selection */
            valid = FALSE;
        }

        /* Validate NCU */
        if ( valid && !nwamui_ncu_validate( NWAMUI_NCU(self->prv->prev_selected_ncu), &prop_name ) ) {
            gchar* message = g_strdup_printf(_("An error occurred validating the current NCU.\nThe property '%s' caused this failure"), prop_name );
            nwamui_util_show_message (GTK_WINDOW(self->prv->capplet_dialog), 
                                      GTK_MESSAGE_ERROR, _("Validation Error"), message, TRUE );
            g_free(prop_name);
            /* Don't change selection */
            valid = FALSE;
        }
        if ( ! valid ) {
            /* Reset selected_ncu, otherwise we will enter a validation loop */
            self->prv->selected_ncu = NWAMUI_NCU(g_object_ref(self->prv->prev_selected_ncu));
            nwam_capplet_dialog_select_ncu(self, self->prv->prev_selected_ncu );
        }
    }

	/* update the notetab according to the selected entry */
    if ( valid ) {
        gtk_notebook_set_current_page(self->prv->main_nb, idx);
        /* Cancel all changes what user has changed in the current page. */
        nwam_pref_refresh(NWAM_PREF_IFACE(obj), user_data, FALSE);
    }

    if (user_data) {
        g_object_unref(user_data);
    }
}

static gboolean
show_combo_separator_cb (GtkTreeModel *model,
                         GtkTreeIter *iter,
                         gpointer data)
{
	gpointer item = NULL;
	gtk_tree_model_get(model, iter, 0, &item, -1);
	if (item != NULL) {
        g_object_unref(item);
        return FALSE;
    } else {
        return TRUE; /* separator */
    }
}

static void
show_combo_cell_cb (GtkCellLayout *cell_layout,
			GtkCellRenderer   *renderer,
			GtkTreeModel      *model,
			GtkTreeIter       *iter,
			gpointer           data)
{
	gpointer row_data = NULL;
    gchar*   text     = NULL;
	
	gtk_tree_model_get(model, iter, 0, &row_data, -1);
	
	if (row_data == NULL) {
		return;
	}
        
	g_assert (G_IS_OBJECT (row_data));
	
	if (NWAMUI_IS_NCU (row_data)) {
		g_object_set(renderer,
          "text", nwamui_ncu_get_display_name(NWAMUI_NCU(row_data)),
          "sensitive", TRUE,
          NULL);
	} else if (NWAMUI_IS_OBJECT (row_data)) {
        gchar *markup;
        markup = g_strdup_printf("<b><i>%s</i></b>", nwamui_object_get_name(NWAMUI_OBJECT(row_data)));
		g_object_set(renderer,
          "markup", markup,
          "sensitive", FALSE,
          NULL);
		g_free (markup);
    } else if (NWAM_IS_CONN_STATUS_PANEL (row_data)) {
		text = _("Connection Status");
		g_object_set(renderer,
          "text", text,
          "sensitive", TRUE,
          NULL);
	} else if (NWAM_IS_PROFILE_PANEL(row_data)) {
		text = _("Network Profile");
		g_object_set(renderer,
          "text", text,
          "sensitive", TRUE,
          NULL);
	} else {
		g_assert_not_reached();
	}
    g_object_unref(row_data);
}

static void
object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
	g_debug("NwamCappletDialog: notify %s changed", arg1->name);
}

/*
 * Utility functions.
 */
static void
show_combo_add(GtkComboBox* combo, GObject*  obj)
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_combo_box_get_model(combo);

    gtk_tree_store_append (GTK_TREE_STORE(model), &iter, NULL);

    if (NWAMUI_IS_NCP(obj)) {
        GList *ncu_list;
        /* Make the previous line become a separator. */
        gtk_tree_store_append (GTK_TREE_STORE(model), &iter, NULL);
        gtk_tree_store_set (GTK_TREE_STORE(model), &iter, 0, obj, -1);
        ncu_list = nwamui_ncp_get_ncu_list(NWAMUI_NCP(obj));
        for (; ncu_list;) {
            gtk_tree_store_append(GTK_TREE_STORE(model), &iter, NULL);
            gtk_tree_store_set(GTK_TREE_STORE(model), &iter, 0, ncu_list->data, -1);
            ncu_list = g_list_delete_link(ncu_list, ncu_list);
        }
    } else {
        gtk_tree_store_set (GTK_TREE_STORE(model), &iter, 0, obj, -1);
    }
}

static void
show_combo_remove(GtkComboBox* combo, GObject*  obj)
{
	GtkTreeModel *model = gtk_combo_box_get_model(combo);
	GtkTreeIter parent;
    GObject *data;

    if (capplet_model_find_object(model, obj, &parent)) {
        /* If it is NCP, remove child NCUs. */
        if (NWAMUI_IS_NCP(obj)) {
            GtkTreeIter iter;
            GtkTreePath *path;

            /* Delete the separator. */
            path = gtk_tree_model_get_path(model, &parent);
            if (gtk_tree_path_prev(path)) {
                gtk_tree_model_get_iter(model, &iter, path);
                gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);
            }
            gtk_tree_path_free(path);

            /* Delete NCUs. */
            if(gtk_tree_store_remove(GTK_TREE_STORE(model), &parent))
                do {
                    gtk_tree_model_get(model, &parent, 0, &data, -1);
                    if (data)
                        g_object_unref(G_OBJECT(data));
                    else
                        break;
                } while (gtk_tree_store_remove(GTK_TREE_STORE(model), &parent));


        } else {
            /* Remove whatever it is. */
            gtk_tree_store_remove(GTK_TREE_STORE(model), &parent);
        }
    }
}

/*
 * Select the given NCU in the show_combo box.
 */
extern void
nwam_capplet_dialog_select_ncu(NwamCappletDialog  *self, NwamuiNcu*  ncu )
{
    NwamCappletDialogPrivate *prv = NWAM_CAPPLET_DIALOG_GET_PRIVATE(self);

    capplet_combo_set_active_object(prv->show_combo, G_OBJECT(ncu));
}

extern void
nwam_capplet_dialog_set_ok_sensitive_by_voting(NwamCappletDialog *self, gboolean setting)
{
    NwamCappletDialogPrivate *prv = NWAM_CAPPLET_DIALOG_GET_PRIVATE(self);
    static gint insensitive_voting_count = 0;

    g_assert(insensitive_voting_count >= 0);

    if (setting) {
        insensitive_voting_count--;
    } else {
        insensitive_voting_count++;
    }
    
    gtk_dialog_set_response_sensitive(prv->capplet_dialog, GTK_RESPONSE_OK, insensitive_voting_count == 0 ? TRUE : FALSE);
}
