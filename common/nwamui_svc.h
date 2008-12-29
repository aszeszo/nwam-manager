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
 * File:   nwamui_svc.h
 *
 */

#ifndef _NWAMUI_SVC_H
#define	_NWAMUI_SVC_H

#ifndef _libnwamui_H
#error "Please include libnwamui.h header instead."
#endif

#include <libnwam.h>

G_BEGIN_DECLS

/* GObject Creation */

#define NWAMUI_TYPE_SVC				  (nwamui_svc_get_type ())
#define NWAMUI_SVC(obj)				  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NWAMUI_TYPE_SVC, NwamuiSvc))
#define NWAMUI_SVC_CLASS(klass)		  (G_TYPE_CHECK_CLASS_CAST ((klass), NWAMUI_TYPE_SVC, NwamuiSvcClass))
#define NWAMUI_IS_SVC(obj)			  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NWAMUI_TYPE_SVC))
#define NWAMUI_IS_SVC_CLASS(klass)	  (G_TYPE_CHECK_CLASS_TYPE ((klass), NWAMUI_TYPE_SVC))
#define NWAMUI_SVC_GET_CLASS(obj)	  (G_TYPE_INSTANCE_GET_CLASS ((obj), NWAMUI_TYPE_SVC, NwamuiSvcClass))


typedef struct _NwamuiSvc		NwamuiSvc;
typedef struct _NwamuiSvcClass	 NwamuiSvcClass;
typedef struct _NwamuiSvcPrivate	NwamuiSvcPrivate;

struct _NwamuiSvc
{
	NwamuiObject					object;

	/*< private >*/
	NwamuiSvcPrivate		*prv;
};

struct _NwamuiSvcClass
{
	NwamuiObjectClass			parent_class;
};

/* Temporary typedef until we figure out what is meant to happen with svc type */

extern GType nwamui_svc_get_type (void) G_GNUC_CONST;

extern NwamuiSvc*			nwamui_svc_new (const gchar* fmri);
extern gchar *              nwamui_svc_get_name (NwamuiSvc *self);
extern gchar*				nwamui_svc_get_description (NwamuiSvc *self);
/*
 * TODO: Decide if we still need these.
 */
#if 0
extern void                 nwamui_svc_set_status (NwamuiSvc *self, gboolean status);
extern gboolean             nwamui_svc_get_status (NwamuiSvc *self);
extern void					nwamui_svc_set_default (NwamuiSvc *self, gboolean is_default);
extern gboolean				nwamui_svc_is_default (NwamuiSvc *self);
#endif /* 0 */

G_END_DECLS
		
#endif	/* _NWAMUI_SVC_H */
