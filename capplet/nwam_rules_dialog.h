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

#ifndef _nwam_rules_dialog_H
#define	_nwam_rules_dialog_H

G_BEGIN_DECLS

#define NWAM_TYPE_RULES_DIALOG               (nwam_rules_dialog_get_type ())
#define NWAM_RULES_DIALOG(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), NWAM_TYPE_RULES_DIALOG, NwamRulesDialog))
#define NWAM_RULES_DIALOG_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), NWAM_TYPE_RULES_DIALOG, NwamRulesDialogClass))
#define NWAM_IS_RULES_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NWAM_TYPE_RULES_DIALOG))
#define NWAM_IS_RULES_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), NWAM_TYPE_RULES_DIALOG))
#define NWAM_RULES_DIALOG_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), NWAM_TYPE_RULES_DIALOG, NwamRulesDialogClass))


typedef struct _NwamRulesDialog		NwamRulesDialog;
typedef struct _NwamRulesDialogClass          NwamRulesDialogClass;
typedef struct _NwamRulesDialogPrivate	NwamRulesDialogPrivate;

struct _NwamRulesDialog
{
	GObject                      object;

	/*< private >*/
	NwamRulesDialogPrivate    *prv;
};

struct _NwamRulesDialogClass
{
	GObjectClass                parent_class;
	void (*condition_add)    (NwamRulesDialog *self, GObject *data, gpointer user_data);
	void (*condition_remove)  (NwamRulesDialog *self, GObject *data, gpointer user_data);
};


extern  GType            nwam_rules_dialog_get_type (void) G_GNUC_CONST;
extern  NwamRulesDialog* nwam_rules_dialog_new(void);
extern  gint             nwam_rules_dialog_run(NwamPrefIFace *iface, GtkWindow *parent);

G_END_DECLS

#endif	/* _nwam_rules_dialog_H */
