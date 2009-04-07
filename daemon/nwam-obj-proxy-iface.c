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
 * File:   nwam_obj_proxy_iface.c
 *
 * Created on June 7, 2007, 11:13 AM
 * 
 */

#include "nwam-obj-proxy-iface.h"

static void
nwam_obj_proxy_iface_base_init (gpointer gclass)
{
	static gboolean initialized = FALSE;

	if (!initialized) {
		initialized = TRUE;
	}
}

static void
nwam_obj_proxy_iface_class_init (gpointer g_class, gpointer g_class_data)
{
	NwamObjProxyInterface *self = (NwamObjProxyInterface *)g_class;

	self->get_proxy = NULL;
}

GType
nwam_obj_proxy_iface_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			/* You fill this structure. */
			sizeof (NwamObjProxyInterface),
			nwam_obj_proxy_iface_base_init,   /* base_init */
			NULL,   /* base_finalize */
			nwam_obj_proxy_iface_class_init,   /* class_init */
			NULL,   /* class_finalize */
			NULL,   /* class_data */
			0,
			0,      /* n_preallocs */
			NULL    /* instance_init */
		};
		type = g_type_register_static (G_TYPE_INTERFACE,
					       g_intern_static_string ("NwamObjProxyInterfaceType"),
					       &info, 0);
	}
	return type;
}

extern GObject*
nwam_obj_proxy_get_proxy(NwamObjProxyIFace *self)
{
    NwamObjProxyInterface *iface = NWAM_GET_OBJ_PROXY_INTERFACE(self);

    g_return_val_if_fail(self != NULL, NULL);

    return iface->get_proxy(self);
}

extern void
nwam_obj_proxy_refresh(NwamObjProxyIFace *self)
{
    NwamObjProxyInterface *iface = NWAM_GET_OBJ_PROXY_INTERFACE(self);

    g_return_if_fail(self != NULL);

    iface->refresh(self);
}

extern void
nwam_obj_proxy_delete_notify(NwamObjProxyIFace *self)
{
    NwamObjProxyInterface *iface = NWAM_GET_OBJ_PROXY_INTERFACE(self);

    g_return_if_fail(self != NULL);

    if (iface->delete_notify) {
        iface->delete_notify(self);
    }
}
