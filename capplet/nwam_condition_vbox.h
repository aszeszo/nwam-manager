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

#ifndef _nwam_condition_vbox_H
#define	_nwam_condition_vbox_H

G_BEGIN_DECLS

#define NWAM_TYPE_CONDITION_VBOX               (nwam_condition_vbox_get_type ())
#define NWAM_CONDITION_VBOX(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), NWAM_TYPE_CONDITION_VBOX, NwamConditionVBox))
#define NWAM_CONDITION_VBOX_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), NWAM_TYPE_CONDITION_VBOX, NwamConditionVBoxClass))
#define NWAM_IS_CONDITION_VBOX(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NWAM_TYPE_CONDITION_VBOX))
#define NWAM_IS_CONDITION_VBOX_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), NWAM_TYPE_CONDITION_VBOX))
#define NWAM_CONDITION_VBOX_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), NWAM_TYPE_CONDITION_VBOX, NwamConditionVBoxClass))


typedef struct _NwamConditionVBox		NwamConditionVBox;
typedef struct _NwamConditionVBoxClass          NwamConditionVBoxClass;
typedef struct _NwamConditionVBoxPrivate	NwamConditionVBoxPrivate;

struct _NwamConditionVBox
{
	GtkVBox                      object;

	/*< private >*/
	NwamConditionVBoxPrivate    *prv;
};

struct _NwamConditionVBoxClass
{
	GtkVBoxClass                parent_class;
	void (*condition_add)    (NwamConditionVBox *self, GObject *data, gpointer user_data);
	void (*condition_remove)  (NwamConditionVBox *self, GObject *data, gpointer user_data);
};


extern  GType                   nwam_condition_vbox_get_type (void) G_GNUC_CONST;
extern  NwamConditionVBox*      nwam_condition_vbox_new (void);

G_END_DECLS

#endif	/* _nwam_condition_vbox_H */
