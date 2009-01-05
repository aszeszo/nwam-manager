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
 * File:   nwamui_object.h
 *
 */

#ifndef _NWAMUI_OBJECT_H
#define	_NWAMUI_OBJECT_H

#ifndef _libnwamui_H
#error "Please include libnwamui.h header instead."
#endif

G_BEGIN_DECLS

#define NWAMUI_TYPE_OBJECT               (nwamui_object_get_type ())
#define NWAMUI_OBJECT(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), NWAMUI_TYPE_OBJECT, NwamuiObject))
#define NWAMUI_OBJECT_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), NWAMUI_TYPE_OBJECT, NwamuiObjectClass))
#define NWAMUI_IS_OBJECT(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NWAMUI_TYPE_OBJECT))
#define NWAMUI_IS_OBJECT_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), NWAMUI_TYPE_OBJECT))
#define NWAMUI_OBJECT_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), NWAMUI_TYPE_OBJECT, NwamuiObjectClass))


typedef struct _NwamuiObject		      NwamuiObject;
typedef struct _NwamuiObjectClass        NwamuiObjectClass;

struct _NwamuiObject
{
	GObject                      object;
};

struct _NwamuiObjectClass
{
	GObjectClass                parent_class;
};

extern  GType                       nwamui_object_get_type (void) G_GNUC_CONST;


G_END_DECLS

#endif	/* _NWAMUI_OBJECT_H */

