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
#include <strings.h>

#include "libnwamui.h"
#include "nwam_pref_iface.h"
#include "nwam_condition_vbox.h"
#include "nwam_rules_dialog.h"

#define RULES_DIALOG "rules_dialog"
#define RULES_NAME_LABEL "rules_name_lbl"
#define RULES_VBOX_SW "rules_vbox_sw"
#define RULES_MATCH_ALL_RB "rules_match_all_rb"
#define RULES_MATCH_ANY_RB "rules_match_any_rb"
#define RULES_DIALOG_VBOX "rules_dialog_vbox"

enum {
	PLACE_HOLDER,
	LAST_SIGNAL
};

static guint cond_signals[LAST_SIGNAL] = {0};

#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
	NWAM_TYPE_RULES_DIALOG, NwamRulesDialogPrivate)) 

struct _NwamRulesDialogPrivate {
	GtkDialog         *rules_dialog;
	GtkRadioButton    *rules_match_any_rb;
	GtkRadioButton    *rules_match_all_rb;
	GtkWidget         *rules_vbox_sw;
	GtkWidget         *rules_dialog_vbox;
	NwamConditionVBox *rules_vbox;

	NwamuiObject *selected_object;
};

static void nwam_pref_init (gpointer g_iface, gpointer iface_data);
static gboolean refresh(NwamPrefIFace *iface, gpointer user_data, gboolean force);
static gboolean apply(NwamPrefIFace *iface, gpointer user_data);
static gboolean cancel(NwamPrefIFace *iface, gpointer user_data);
static GtkWindow* dialog_get_window(NwamPrefIFace *iface);

static void nwam_rules_dialog_finalize (NwamRulesDialog *self);
static void response_cb(GtkWidget* widget, gint responseid, gpointer user_data);
static void condition_cb(NwamConditionVBox *self, GObject* data, gpointer user_data);

G_DEFINE_TYPE_EXTENDED (NwamRulesDialog,
    nwam_rules_dialog,
    G_TYPE_OBJECT,
    0,
    G_IMPLEMENT_INTERFACE (NWAM_TYPE_PREF_IFACE, nwam_pref_init))

static gboolean
refresh(NwamPrefIFace *iface, gpointer user_data, gboolean force)
{
	NwamRulesDialogPrivate *prv = GET_PRIVATE(iface);
    gboolean                rval;

	if (prv->selected_object != user_data) {
		if (prv->selected_object) {
			g_object_unref(prv->selected_object);
		}
		if ((prv->selected_object = user_data) != NULL) {
			g_object_ref(prv->selected_object);
		}
		force = TRUE;
	}

	if (force) {
		if (prv->selected_object) {
			gchar *title;
			gchar *name;
            nwamui_cond_activation_mode_t mode;

            mode = nwamui_object_get_activation_mode(prv->selected_object);

            if (mode == NWAMUI_COND_ACTIVATION_MODE_CONDITIONAL_ALL) {
                gtk_button_clicked(GTK_BUTTON(prv->rules_match_all_rb));
            } else if (mode == NWAMUI_COND_ACTIVATION_MODE_CONDITIONAL_ANY) {
                gtk_button_clicked(GTK_BUTTON(prv->rules_match_any_rb));
            } else {
                /* Maybe manual or system, set to default. */
                gtk_button_clicked(GTK_BUTTON(prv->rules_match_any_rb));
            }

			title = g_strdup_printf("Edit Rules : %s", nwamui_object_get_name(prv->selected_object));
			g_object_set(prv->rules_dialog,
			    "title", title,
			    NULL);
			g_free(title);
		}
	}
	rval = nwam_pref_refresh(NWAM_PREF_IFACE(prv->rules_vbox), prv->selected_object, force);
    gtk_widget_set_size_request( GTK_WIDGET(prv->rules_dialog), -1, 200 );
    return(rval);
}

static gboolean
apply(NwamPrefIFace *iface, gpointer user_data)
{
    gboolean                cond_all;
	NwamRulesDialogPrivate *prv = GET_PRIVATE(iface);

    /* Set the activation mode based on the toggle */
    cond_all = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(prv->rules_match_all_rb) );

    if (cond_all) {
        nwamui_object_set_activation_mode(  prv->selected_object, 
                                            NWAMUI_COND_ACTIVATION_MODE_CONDITIONAL_ALL );
    }
    else {
        nwamui_object_set_activation_mode(  prv->selected_object, 
                                            NWAMUI_COND_ACTIVATION_MODE_CONDITIONAL_ANY );
    }


	return nwam_pref_apply(NWAM_PREF_IFACE(prv->rules_vbox), prv->selected_object);
}

static gboolean
cancel(NwamPrefIFace *iface, gpointer user_data)
{
	NwamRulesDialogPrivate *prv = GET_PRIVATE(iface);
	return nwam_pref_cancel(NWAM_PREF_IFACE(prv->rules_vbox), user_data);
}

static GtkWindow*
dialog_get_window(NwamPrefIFace *iface)
{
	NwamRulesDialogPrivate *prv = GET_PRIVATE(iface);

    if (prv->rules_dialog) {
        return GTK_WINDOW(prv->rules_dialog);
    }

    return NULL;
}

static void
nwam_pref_init (gpointer g_iface, gpointer iface_data)
{
	NwamPrefInterface *iface = (NwamPrefInterface *)g_iface;
	iface->refresh = refresh;
	iface->apply = apply;
	iface->cancel = cancel;
	iface->help = NULL;
    iface->dialog_get_window = dialog_get_window;
}

static void
nwam_rules_dialog_class_init (NwamRulesDialogClass *klass)
{
    /* Pointer to GObject Part of Class */
    GObjectClass *gobject_class = (GObjectClass*) klass;
        
    /* Override Some Function Pointers */
    gobject_class->finalize = (void (*)(GObject*)) nwam_rules_dialog_finalize;

    g_type_class_add_private (klass, sizeof (NwamRulesDialogPrivate));
}
        
static void
nwam_rules_dialog_init (NwamRulesDialog *self)
{
	NwamRulesDialogPrivate *prv = GET_PRIVATE(self);
    GtkLabel               *rules_name_lbl;

	self->prv = prv;

	prv->rules_dialog = GTK_DIALOG(nwamui_util_glade_get_widget(RULES_DIALOG));
	g_signal_connect(GTK_DIALOG(self->prv->rules_dialog), "response", (GCallback)response_cb, (gpointer)self);

	prv->rules_dialog_vbox = nwamui_util_glade_get_widget(RULES_DIALOG_VBOX);
	prv->rules_match_all_rb = GTK_RADIO_BUTTON(nwamui_util_glade_get_widget(RULES_MATCH_ALL_RB));
	prv->rules_match_any_rb = GTK_RADIO_BUTTON(nwamui_util_glade_get_widget(RULES_MATCH_ANY_RB));


	prv->rules_vbox = nwam_condition_vbox_new();
	prv->rules_vbox_sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(prv->rules_vbox_sw), GTK_SHADOW_IN);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(prv->rules_vbox_sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(prv->rules_vbox_sw), GTK_WIDGET(prv->rules_vbox));
    g_object_set(gtk_widget_get_parent(GTK_WIDGET(prv->rules_vbox)),
      "resize-mode", GTK_RESIZE_PARENT,
      NULL);
    gtk_box_pack_start(GTK_BOX(prv->rules_dialog_vbox), prv->rules_vbox_sw, TRUE, TRUE, 2);

    g_signal_connect( prv->rules_vbox, "condition_add", (GCallback)condition_cb, (gpointer)self );
    g_signal_connect( prv->rules_vbox, "condition_remove", (GCallback)condition_cb, (gpointer)self );

    rules_name_lbl = GTK_LABEL(nwamui_util_glade_get_widget(RULES_NAME_LABEL));
    nwamui_util_set_a11y_label_for_widget( rules_name_lbl, GTK_WIDGET(prv->rules_vbox_sw) );

    gtk_widget_show(prv->rules_vbox_sw);


	/* Initializing */
	nwam_pref_refresh(NWAM_PREF_IFACE(self), NULL, TRUE);
}

static void
nwam_rules_dialog_finalize (NwamRulesDialog *self)
{
	NwamRulesDialogPrivate *prv = GET_PRIVATE(self);

    g_signal_handlers_disconnect_matched(prv->rules_dialog,
      G_SIGNAL_MATCH_FUNC,
      0,
      NULL,
      NULL,
      (gpointer)response_cb,
      NULL);

    gtk_container_remove(GTK_CONTAINER(prv->rules_dialog_vbox), prv->rules_vbox_sw);

	if (prv->selected_object) {
		g_object_unref(prv->selected_object);
	}

	G_OBJECT_CLASS(nwam_rules_dialog_parent_class)->finalize(G_OBJECT(self));
}

/**
 * nwam_rules_dialog_new:
 * @returns: a new #NwamRulesDialog.
 *
 * Creates a new #NwamRulesDialog.
 **/
NwamRulesDialog*
nwam_rules_dialog_new (void)
{
    return NWAM_RULES_DIALOG(g_object_new(NWAM_TYPE_RULES_DIALOG,
	    NULL));
}

static void
response_cb(GtkWidget* widget, gint responseid, gpointer user_data)
{
	NwamRulesDialog *self = NWAM_RULES_DIALOG(user_data);
	gboolean           stop_emission = FALSE;

	switch (responseid) {
	case GTK_RESPONSE_NONE:
		g_debug("GTK_RESPONSE_NONE");
		stop_emission = TRUE;
		break;
	case GTK_RESPONSE_OK:
		g_debug("GTK_RESPONSE_OK");
		if (!nwam_pref_apply(NWAM_PREF_IFACE(self), NULL)) {
			/* TODO - report error to user */
            g_debug("GTK_RESPONSE failed to apply, emission stopped");
			stop_emission = TRUE;
		}
		break;
	case GTK_RESPONSE_REJECT: /* Generated by Revert Button */
		g_debug("GTK_RESPONSE_REJECT");
		nwam_pref_refresh(NWAM_PREF_IFACE(self), self->prv->selected_object, TRUE);
		stop_emission = TRUE;
		break;
    case GTK_RESPONSE_DELETE_EVENT:
        g_debug("GTK_RESPONSE_DELETE_EVENT");
        /* Fall through */
	case GTK_RESPONSE_CANCEL:
		g_debug("GTK_RESPONSE_CANCEL");
		break;
	case GTK_RESPONSE_HELP:
		break;
	}
	if ( stop_emission ) {
		g_signal_stop_emission_by_name(widget, "response" );
	} else {
		gtk_widget_hide(widget);
	}
}

static void
condition_cb(NwamConditionVBox *vbox, GObject* data, gpointer user_data)
{
	NwamRulesDialog *self = NWAM_RULES_DIALOG(user_data);
    gboolean         sensitivity = FALSE;

    if ( nwam_condition_vbox_get_num_lines (vbox) > 1 ) {
        sensitivity = TRUE;
    }

	gtk_widget_set_sensitive( GTK_WIDGET(self->prv->rules_match_all_rb), sensitivity );
	gtk_widget_set_sensitive( GTK_WIDGET(self->prv->rules_match_any_rb), sensitivity );
}
