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
 */

#ifndef _NWAM_PROFILE_DIALOG_H
#define	_NWAM_PROFILE_DIALOG_H

G_BEGIN_DECLS


#define NWAM_TYPE_PROFILE_DIALOG               (nwam_profile_dialog_get_type ())
#define NWAM_PROFILE_DIALOG(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), NWAM_TYPE_PROFILE_DIALOG, NwamProfileDialog))
#define NWAM_PROFILE_DIALOG_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), NWAM_TYPE_PROFILE_DIALOG, NwamProfileDialogClass))
#define NWAM_IS_PROFILE_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NWAM_TYPE_PROFILE_DIALOG))
#define NWAM_IS_PROFILE_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), NWAM_TYPE_PROFILE_DIALOG))
#define NWAM_PROFILE_DIALOG_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), NWAM_TYPE_PROFILE_DIALOG, NwamProfileDialogClass))


typedef struct _NwamProfileDialog		NwamProfileDialog;
typedef struct _NwamProfileDialogClass	NwamProfileDialogClass;
typedef struct _NwamProfileDialogPrivate	NwamProfileDialogPrivate;

struct _NwamProfileDialog
{
	GObject                      object;

	/*< private >*/
	NwamProfileDialogPrivate    *prv;
};

struct _NwamProfileDialogClass
{
	GObjectClass                parent_class;
};

GType nwam_profile_dialog_get_type (void) G_GNUC_CONST;

NwamProfileDialog* nwam_profile_dialog_new(NwamCappletDialog *pref_dialog);

G_END_DECLS

#endif	/* _NWAM_PROFILE_DIALOG_H */
