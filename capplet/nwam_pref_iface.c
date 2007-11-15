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
 * File:   nwam_pref_iface.c
 *
 * Created on June 7, 2007, 11:13 AM
 * 
 */

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
 * @self: a #NwamPrefIFace instance.
 * @data: User data
 * 
 * Refresh each @self.
 * Invork by nwam_pref_dialog, currently do not check the refresh is successfully
 * Or not.
 *
 * Returns: TRUE, if refresh @self successfully. Default TRUE.
 **/
gboolean
nwam_pref_refresh (NwamPrefIFace *self, gpointer data)
{
    NwamPrefInterface *iface = NWAM_GET_PREF_INTERFACE (self);

    g_return_val_if_fail( self != NULL, FALSE );

    if (iface->refresh)
        return (iface)->refresh(self, data);

    return TRUE;
}

/**
 * nwam_pref_apply:
 * @self: a #NwamPrefIFace instance.
 * @data: User data
 * 
 * Apply each @self.
 * Invork by nwam_pref_dialog, willcheck the application is successfully or not.
 * If all are successfully, nwam_pref_dialog will hide itself, else maybe popup
 * a error dialog(?) and keep living.
 *
 * Returns: TRUE, if apply @self successfully. Default FALSE.
 **/
gboolean
nwam_pref_apply (NwamPrefIFace *self, gpointer data)
{
	NwamPrefInterface *iface = NWAM_GET_PREF_INTERFACE (self);

	if (iface->apply)
		return (iface)->apply(self, data);
	return FALSE;
}

/**
 * nwam_pref_help:
 * @self: a #NwamPrefIFace instance.
 * @data: User data
 * 
 * Help each @self.
 * Invork by nwam_pref_dialog, willcheck the application is successfully or not.
 * If all are successfully, nwam_pref_dialog will hide itself, else maybe popup
 * a error dialog(?) and keep living.
 *
 * Returns: TRUE, if help @self successfully. Default FALSE.
 **/
gboolean
nwam_pref_help (NwamPrefIFace *self, gpointer data)
{
	NwamPrefInterface *iface = NWAM_GET_PREF_INTERFACE (self);

	if (iface->help)
		return (iface)->help(self, data);
	return FALSE;
}
