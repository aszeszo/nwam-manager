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
 * File:   nwam_object_ctrl_iface.c
 *
 * Created on June 7, 2007, 11:13 AM
 * 
 */

#include <gtk/gtk.h>
#include "nwam_object_ctrl_iface.h"

static void
nwam_object_ctrl_iface_base_init (gpointer gclass)
{
	static gboolean initialized = FALSE;

	if (!initialized) {
		initialized = TRUE;
	}
}

static void
nwam_object_ctrl_iface_class_init (gpointer g_class, gpointer g_class_data)
{
	NwamObjectCtrlInterface *self = (NwamObjectCtrlInterface *)g_class;

	self->create = NULL;
	self->remove = NULL;
    /* self->help = NULL; */
    /* self->dialog_run = NULL; */
    /* self->dialog_get_window = NULL; */
}

GType
nwam_object_ctrl_iface_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			/* You fill this structure. */
			sizeof (NwamObjectCtrlInterface),
			nwam_object_ctrl_iface_base_init,   /* base_init */
			NULL,   /* base_finalize */
			nwam_object_ctrl_iface_class_init,   /* class_init */
			NULL,   /* class_finalize */
			NULL,   /* class_data */
			0,
			0,      /* n_preallocs */
			NULL    /* instance_init */
		};
		type = g_type_register_static (G_TYPE_INTERFACE,
					       g_intern_static_string ("NwamObjectCtrlInterfaceType"),
					       &info, 0);
	}
	return type;
}

/**
 * nwam_object_ctrl_create:
 * @iface: a #NwamObjectCtrlIFace instance.
 * 
 * Returns: An object.
 **/
GObject*
nwam_object_ctrl_create(NwamObjectCtrlIFace *iface)
{
    NwamObjectCtrlInterface *interface = NWAM_GET_OBJECT_CTRL_INTERFACE(iface);

    g_return_val_if_fail(interface != NULL, FALSE);
    g_return_val_if_fail(interface->create, FALSE);

    return interface->create(iface);
}

/**
 * nwam_object_ctrl_remove:
 * @iface: a #NwamObjectCtrlIFace instance.
 * 
 * Remove an object.
 **/
gboolean
nwam_object_ctrl_remove(NwamObjectCtrlIFace *iface, GObject *obj)
{
	NwamObjectCtrlInterface *interface = NWAM_GET_OBJECT_CTRL_INTERFACE(iface);

    g_return_val_if_fail(interface != NULL, FALSE);
    g_return_val_if_fail(interface->remove, FALSE);

    return interface->remove(iface, obj);
}

gboolean
nwam_object_ctrl_rename(NwamObjectCtrlIFace *iface, GObject *obj)
{
	NwamObjectCtrlInterface *interface = NWAM_GET_OBJECT_CTRL_INTERFACE(iface);

    g_return_val_if_fail(interface != NULL, FALSE);
    g_return_val_if_fail(interface->rename, FALSE);

    return interface->rename(iface, obj);
}

void
nwam_object_ctrl_edit(NwamObjectCtrlIFace *iface, GObject *obj)
{
	NwamObjectCtrlInterface *interface = NWAM_GET_OBJECT_CTRL_INTERFACE(iface);

    g_return_if_fail(interface != NULL);
    g_return_if_fail(interface->rename);

    interface->edit(iface, obj);
}

GObject*
nwam_object_ctrl_dup(NwamObjectCtrlIFace *iface, GObject *obj)
{
    NwamObjectCtrlInterface *interface = NWAM_GET_OBJECT_CTRL_INTERFACE(iface);

    g_return_val_if_fail(interface != NULL, FALSE);
    g_return_val_if_fail(interface->dup, FALSE);

    return interface->dup(iface, obj);
}
