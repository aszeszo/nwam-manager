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
 * File:   nwam_obj_proxy_iface.h
 *
 */

#ifndef _NWAM_OBJ_PROXY_IFCACE_H_
#define _NWAM_OBJ_PROXY_IFCACE_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define NWAM_TYPE_OBJ_PROXY_IFACE (nwam_obj_proxy_iface_get_type ())
#define NWAM_OBJ_PROXY_IFACE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), NWAM_TYPE_OBJ_PROXY_IFACE, NwamObjProxyIFace))
#define NWAM_IS_OBJ_PROXY_IFACE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), NWAM_TYPE_OBJ_PROXY_IFACE))
#define NWAM_GET_OBJ_PROXY_INTERFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE((obj), NWAM_TYPE_OBJ_PROXY_IFACE, NwamObjProxyInterface))

typedef struct _NwamObjProxyIFace NwamObjProxyIFace; /* dummy object */
typedef struct _NwamObjProxyInterface NwamObjProxyInterface;

struct _NwamObjProxyInterface {
	GTypeInterface parent;

	GObject* (*get_proxy) (NwamObjProxyIFace *self);
	void (*delete_notify) (NwamObjProxyIFace *self);
};

extern GType nwam_obj_proxy_iface_get_type (void) G_GNUC_CONST;

extern GObject* nwam_obj_proxy_get_proxy(NwamObjProxyIFace *self);
extern void nwam_obj_proxy_delete_notify(NwamObjProxyIFace *self);

G_END_DECLS

#endif /* _NWAM_OBJ_PROXY_IFCACE_H_ */

