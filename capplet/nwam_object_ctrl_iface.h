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
 * File:   nwam_object_ctrl_iface.h
 *
 * Created on June 7, 2007, 11:13 AM
 * 
 */

#ifndef _NWAM_OBJECT_CTRL_IFCACE_H_
#define _NWAM_OBJECT_CTRL_IFCACE_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define NWAM_TYPE_OBJECT_CTRL_IFACE (nwam_object_ctrl_iface_get_type ())
#define NWAM_OBJECT_CTRL_IFACE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), NWAM_TYPE_OBJECT_CTRL_IFACE, NwamObjectCtrlIFace))
#define NWAM_IS_OBJECT_CTRL_IFACE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), NWAM_TYPE_OBJECT_CTRL_IFACE))
#define NWAM_GET_OBJECT_CTRL_INTERFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE((obj), NWAM_TYPE_OBJECT_CTRL_IFACE, NwamObjectCtrlInterface))

typedef struct _NwamObjectCtrlIFace NwamObjectCtrlIFace; /* dummy object */
typedef struct _NwamObjectCtrlInterface NwamObjectCtrlInterface;

struct _NwamObjectCtrlInterface {
	GTypeInterface parent;

	GObject* (*create)(NwamObjectCtrlIFace *iface);
	gboolean (*remove)(NwamObjectCtrlIFace *iface, GObject *obj);
	gboolean (*rename)(NwamObjectCtrlIFace *iface, GObject *obj);
	void (*edit)(NwamObjectCtrlIFace *iface, GObject *obj);
	GObject* (*dup)(NwamObjectCtrlIFace *iface, GObject *obj);
};

extern GType nwam_object_ctrl_iface_get_type (void) G_GNUC_CONST;

/* for nwam_object_ctrl application internal usage */

extern GObject* nwam_object_ctrl_create(NwamObjectCtrlIFace *iface);
extern gboolean nwam_object_ctrl_remove(NwamObjectCtrlIFace *iface, GObject *obj);
extern gboolean nwam_object_ctrl_rename(NwamObjectCtrlIFace *iface, GObject *obj);
extern void nwam_object_ctrl_edit(NwamObjectCtrlIFace *iface, GObject *obj);
extern GObject* nwam_object_ctrl_dup(NwamObjectCtrlIFace *iface, GObject *obj);

G_END_DECLS

#endif /* _NWAM_OBJECT_CTRL_IFCACE_H_ */

