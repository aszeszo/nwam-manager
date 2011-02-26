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
 * File:   nwam_pref_iface.c
 *
 * Created on June 7, 2007, 11:13 AM
 * 
 */

#include <gtk/gtk.h>
#include "nwam_pref_iface.h"

static void
nwam_pref_iface_base_init (gpointer gclass)
{
	static gboolean initialized = FALSE;

	if (!initialized) {
		initialized = TRUE;
	}
}

static void
nwam_pref_iface_class_init (gpointer g_class, gpointer g_class_data)
{
	NwamPrefInterface *self = (NwamPrefInterface *)g_class;

	self->refresh = NULL;
	self->apply = NULL;
    self->help = NULL;
    self->dialog_get_window = NULL;
}

GType
nwam_pref_iface_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			/* You fill this structure. */
			sizeof (NwamPrefInterface),
			nwam_pref_iface_base_init,   /* base_init */
			NULL,   /* base_finalize */
			nwam_pref_iface_class_init,   /* class_init */
			NULL,   /* class_finalize */
			NULL,   /* class_data */
			0,
			0,      /* n_preallocs */
			NULL    /* instance_init */
		};
		type = g_type_register_static (G_TYPE_INTERFACE,
					       g_intern_static_string ("NwamPrefInterfaceType"),
					       &info, 0);
	}
	return type;
}

/**
 * nwam_pref_refresh:
 * @iface: a #NwamPrefIFace instance.
 * @user_data: User data
 * 
 * Refresh each @iface.
 * Invork by nwam_pref_dialog, currently do not check the refresh is successfully
 * Or not.
 *
 * Returns: TRUE, if refresh @iface successfully. Default TRUE.
 **/
gboolean
nwam_pref_refresh (NwamPrefIFace *iface, gpointer user_data, gboolean force)
{
    NwamPrefInterface *interface = NWAM_GET_PREF_INTERFACE (iface);

    g_return_val_if_fail(interface != NULL, FALSE );
    g_return_val_if_fail(interface->refresh, FALSE );

    return interface->refresh(iface, user_data, force);
}

/**
 * nwam_pref_apply:
 * @iface: a #NwamPrefIFace instance.
 * @user_data: User data
 * 
 * Apply @iface.
 * Invork by nwam_pref_dialog, willcheck the application is successfully or not.
 * If all are successfully, nwam_pref_dialog will hide itiface, else maybe popup
 * a error dialog(?) and keep living.
 *
 * Returns: TRUE, if apply @iface successfully. Default FALSE.
 **/
gboolean
nwam_pref_apply (NwamPrefIFace *iface, gpointer user_data)
{
	NwamPrefInterface *interface = NWAM_GET_PREF_INTERFACE (iface);

    g_return_val_if_fail(interface != NULL, FALSE );
    g_return_val_if_fail(interface->apply, FALSE );

    return interface->apply(iface, user_data);
}

/**
 * nwam_pref_cancel:
 * @iface: a #NwamPrefIFace instance.
 * @user_data: User data
 * 
 * Cancel @iface.
 * Invork by nwam_pref_dialog, will check the application is successfully or not.
 * If all are successfully, nwam_pref_dialog will hide itiface, else maybe popup
 * a error dialog(?) and keep living.
 *
 * Returns: TRUE, if cancel @iface successfully. Default FALSE.
 **/
gboolean
nwam_pref_cancel (NwamPrefIFace *iface, gpointer user_data)
{
	NwamPrefInterface *interface = NWAM_GET_PREF_INTERFACE (iface);

    g_return_val_if_fail(interface != NULL, FALSE );
    g_return_val_if_fail(interface->cancel, FALSE );

    return interface->cancel(iface, user_data);
}

/**
 * nwam_pref_help:
 * @iface: a #NwamPrefIFace instance.
 * @user_data: User data
 * 
 * Help each @iface.
 * Invork by nwam_pref_dialog, willcheck the application is successfully or not.
 * If all are successfully, nwam_pref_dialog will hide itiface, else maybe popup
 * a error dialog(?) and keep living.
 *
 * Returns: TRUE, if help @iface successfully. Default FALSE.
 **/
gboolean
nwam_pref_help (NwamPrefIFace *iface, gpointer user_data)
{
	NwamPrefInterface *interface = NWAM_GET_PREF_INTERFACE (iface);

    g_return_val_if_fail(interface != NULL, FALSE );
    g_return_val_if_fail(interface->help, FALSE );

    return interface->help(iface, user_data);
}

extern gint
nwam_pref_dialog_run(NwamPrefIFace *iface, GtkWidget *w)
{
	NwamPrefInterface *interface = NWAM_GET_PREF_INTERFACE (iface);
	GtkWidget         *parent    = NULL;
    GtkWindow         *window    = nwam_pref_dialog_get_window(iface);
    gint               response  = GTK_RESPONSE_NONE;

    g_return_val_if_fail(interface != NULL, FALSE);
    g_return_val_if_fail(window != NULL, FALSE);

	if (w) {
		parent = gtk_widget_get_toplevel(w);

		if (gtk_widget_is_toplevel(parent)) {
            gtk_window_set_transient_for(window, GTK_WINDOW(parent));
		}
	}

    gtk_window_set_modal(window, parent != NULL);

    response = gtk_dialog_run(GTK_DIALOG(window));

    debug_response_id(response);

    return response;
}

extern GtkWindow*
nwam_pref_dialog_get_window(NwamPrefIFace *iface)
{
	NwamPrefInterface *interface = NWAM_GET_PREF_INTERFACE (iface);

    g_return_val_if_fail(interface != NULL, NULL );
    g_return_val_if_fail(interface->dialog_get_window, NULL );

    return interface->dialog_get_window(iface);
}

extern void
nwam_pref_set_purpose(NwamPrefIFace *iface, nwamui_dialog_purpose_t purpose)
{
	NwamPrefInterface *interface = NWAM_GET_PREF_INTERFACE (iface);

    g_return_if_fail(interface);
    g_return_if_fail(interface->set_purpose);

    interface->set_purpose(iface, purpose);
}

