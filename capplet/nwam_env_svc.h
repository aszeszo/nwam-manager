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
 * File:   nwam_env_svc.h
 * Author: malin
 *
 * Created on June 11, 2007, 11:07 AM
 */

#ifndef _NWAM_ENV_SVC_H
#define	_NWAM_ENV_SVC_H

G_BEGIN_DECLS


#define NWAM_TYPE_ENV_SVC               (nwam_env_svc_get_type ())
#define NWAM_ENV_SVC(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), NWAM_TYPE_ENV_SVC, NwamEnvSvc))
#define NWAM_ENV_SVC_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), NWAM_TYPE_ENV_SVC, NwamEnvSvcClass))
#define NWAM_IS_ENV_SVC(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NWAM_TYPE_ENV_SVC))
#define NWAM_IS_ENV_SVC_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), NWAM_TYPE_ENV_SVC))
#define NWAM_ENV_SVC_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), NWAM_TYPE_ENV_SVC, NwamEnvSvcClass))


typedef struct _NwamEnvSvc		NwamEnvSvc;
typedef struct _NwamEnvSvcClass	NwamEnvSvcClass;
typedef struct _NwamEnvSvcPrivate	NwamEnvSvcPrivate;

struct _NwamEnvSvc
{
	GObject                      object;

	/*< private >*/
	NwamEnvSvcPrivate    *prv;
};

struct _NwamEnvSvcClass
{
	GObjectClass                parent_class;
};

GType nwam_env_svc_get_type (void) G_GNUC_CONST;
NwamEnvSvc* nwam_env_svc_new (void);

G_END_DECLS

#endif	/* _NWAM_ENV_SVC_H */
