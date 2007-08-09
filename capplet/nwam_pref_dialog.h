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
 * File:   nwam_capplet.h
 *
 * Created on May 9, 2007, 11:13 AM
 * 
 */

#ifndef _NWAM_CAPPLET_H
#define	_NWAM_CAPPLET_H

#include "nwam_pref_iface.h"

G_BEGIN_DECLS

#define NWAM_TYPE_CAPPLET_DIALOG               (nwam_capplet_dialog_get_type ())
#define NWAM_CAPPLET_DIALOG(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), NWAM_TYPE_CAPPLET_DIALOG, NwamCappletDialog))
#define NWAM_CAPPLET_DIALOG_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), NWAM_TYPE_CAPPLET_DIALOG, NwamCappletDialogClass))
#define NWAM_IS_CAPPLET_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NWAM_TYPE_CAPPLET_DIALOG))
#define NWAM_IS_CAPPLET_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), NWAM_TYPE_CAPPLET_DIALOG))
#define NWAM_CAPPLET_DIALOG_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), NWAM_TYPE_CAPPLET_DIALOG, NwamCappletDialogClass))


typedef struct _NwamCappletDialog		NwamCappletDialog;
typedef struct _NwamCappletDialogClass		NwamCappletDialogClass;
typedef struct _NwamCappletDialogPrivate	NwamCappletDialogPrivate;

struct _NwamCappletDialog
{
	GObject                      object;

	/*< private >*/
	NwamCappletDialogPrivate    *prv;
};

struct _NwamCappletDialogClass
{
	GObjectClass                parent_class;
};

extern GType                   nwam_capplet_dialog_get_type (void) G_GNUC_CONST;
extern NwamCappletDialog*     nwam_capplet_dialog_new (void);

extern gint                    nwam_capplet_dialog_run (NwamCappletDialog  *capplet_dialog);

extern void			nwam_capplet_dialog_refresh(NwamCappletDialog *self);
/*
 * NwamPrefIFace instance should be added, so it can be refreshed or applied.
 */
extern void nwam_capplet_dialog_add_pref_instance (NwamPrefIFace*, gpointer);

extern void nwam_capplet_dialog_remove_pref_instance (NwamPrefIFace*);

extern void nwam_capplet_dialog_select_ncu(NwamCappletDialog  *self, NwamuiNcu*  ncu );


G_END_DECLS

#endif	/* _NWAM_CAPPLET_H */
