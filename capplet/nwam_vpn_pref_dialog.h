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
 * File:   nwam_vpn_pref.h
 * Author: malin
 *
 * Created on July 26, 2007, 1:25 PM
 */

#ifndef _NWAM_VPN_PREF_H
#define	_NWAM_VPN_PREF_H

G_BEGIN_DECLS

#define NWAM_TYPE_VPN_PREF_DIALOG               (nwam_vpn_pref_dialog_get_type ())
#define NWAM_VPN_PREF_DIALOG(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), NWAM_TYPE_VPN_PREF_DIALOG, NwamVPNPrefDialog))
#define NWAM_VPN_PREF_DIALOG_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), NWAM_TYPE_VPN_PREF_DIALOG, NwamVPNPrefDialogClass))
#define NWAM_IS_VPN_PREF_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NWAM_TYPE_VPN_PREF_DIALOG))
#define NWAM_IS_VPN_PREF_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), NWAM_TYPE_VPN_PREF_DIALOG))
#define NWAM_VPN_PREF_DIALOG_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), NWAM_TYPE_VPN_PREF_DIALOG, NwamVPNPrefDialogClass))


typedef struct _NwamVPNPrefDialog		NwamVPNPrefDialog;
typedef struct _NwamVPNPrefDialogClass		NwamVPNPrefDialogClass;
typedef struct _NwamVPNPrefDialogPrivate	NwamVPNPrefDialogPrivate;

struct _NwamVPNPrefDialog
{
	GObject                      object;

	/*< private >*/
	NwamVPNPrefDialogPrivate    *prv;
};

struct _NwamVPNPrefDialogClass
{
	GObjectClass                parent_class;
};

GType nwam_vpn_pref_dialog_get_type (void) G_GNUC_CONST;
NwamVPNPrefDialog* nwam_vpn_pref_dialog_new (void);
gint nwam_vpn_pref_dialog_run (NwamVPNPrefDialog  *dialog, GtkWindow *parent);


G_END_DECLS

#endif	/* _NWAM_VPN_PREF_H */

