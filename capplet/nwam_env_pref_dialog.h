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
 * File:   nwam_env_pref_dialog.h
 *
 * Created on May 29, 2007, 1:11 PM
 * 
 */

#ifndef _nwam_env_pref_dialog_H
#define	_nwam_env_pref_dialog_H

G_BEGIN_DECLS

#define NWAM_TYPE_ENV_PREF_DIALOG               (nwam_env_pref_dialog_get_type ())
#define NWAM_ENV_PREF_DIALOG(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), NWAM_TYPE_ENV_PREF_DIALOG, NwamEnvPrefDialog))
#define NWAM_ENV_PREF_DIALOG_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), NWAM_TYPE_ENV_PREF_DIALOG, NwamEnvPrefDialogClass))
#define NWAM_IS_ENV_PREF_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NWAM_TYPE_ENV_PREF_DIALOG))
#define NWAM_IS_ENV_PREF_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), NWAM_TYPE_ENV_PREF_DIALOG))
#define NWAM_ENV_PREF_DIALOG_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), NWAM_TYPE_ENV_PREF_DIALOG, NwamEnvPrefDialogClass))


typedef struct _NwamEnvPrefDialog		NwamEnvPrefDialog;
typedef struct _NwamEnvPrefDialogClass          NwamEnvPrefDialogClass;
typedef struct _NwamEnvPrefDialogPrivate	NwamEnvPrefDialogPrivate;

struct _NwamEnvPrefDialog
{
	GObject                      object;

	/*< private >*/
	NwamEnvPrefDialogPrivate    *prv;
};

struct _NwamEnvPrefDialogClass
{
	GObjectClass                parent_class;
};


extern  GType                   nwam_env_pref_dialog_get_type (void) G_GNUC_CONST;
extern  NwamEnvPrefDialog*      nwam_env_pref_dialog_new (void);
extern  NwamEnvPrefDialog*      nwam_env_pref_dialog_new_with_label (const gchar *label);

extern  gint                    nwam_env_pref_dialog_run (NwamEnvPrefDialog  *wireless_dialog, GtkWindow* parent);



G_END_DECLS

#endif	/* _nwam_env_pref_dialog_H */

