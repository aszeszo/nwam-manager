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
 * File:   nwam_proxy_password_dialog.h
 *
 * Created on May 9, 2007, 11:13 AM 
 * 
 */

#ifndef _NWAM_PROXY_PASSWORD_DIALOG_H
#define	_NWAM_PROXY_PASSWORD_DIALOG_H

#ifndef _libnwamui_H
#include <libnwamui.h>
#endif

G_BEGIN_DECLS

#define NWAM_TYPE_PROXY_PASSWORD_DIALOG               (nwam_proxy_password_dialog_get_type ())
#define NWAM_PROXY_PASSWORD_DIALOG(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), NWAM_TYPE_PROXY_PASSWORD_DIALOG, NwamProxyPasswordDialog))
#define NWAM_PROXY_PASSWORD_DIALOG_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), NWAM_TYPE_PROXY_PASSWORD_DIALOG, NwamProxyPasswordDialogClass))
#define NWAM_IS_PROXY_PASSWORD_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NWAM_TYPE_PROXY_PASSWORD_DIALOG))
#define NWAM_IS_PROXY_PASSWORD_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), NWAM_TYPE_PROXY_PASSWORD_DIALOG))
#define NWAM_PROXY_PASSWORD_DIALOG_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), NWAM_TYPE_PROXY_PASSWORD_DIALOG, NwamProxyPasswordDialogClass))


typedef struct _NwamProxyPasswordDialog		    NwamProxyPasswordDialog;
typedef struct _NwamProxyPasswordDialogClass    NwamProxyPasswordDialogClass;
typedef struct _NwamProxyPasswordDialogPrivate	NwamProxyPasswordDialogPrivate;

struct _NwamProxyPasswordDialog
{
	GObject                           object;

	/*< private >*/
	NwamProxyPasswordDialogPrivate    *prv;
};

struct _NwamProxyPasswordDialogClass
{
	GObjectClass                parent_class;
};





extern  GType                       nwam_proxy_password_dialog_get_type (void) G_GNUC_CONST;
extern  NwamProxyPasswordDialog*    nwam_proxy_password_dialog_new (void);

extern  gint                        nwam_proxy_password_dialog_run (NwamProxyPasswordDialog  *proxy_password_dialog);

extern  void                        nwam_proxy_password_dialog_set_username (
                                                                NwamProxyPasswordDialog *proxy_password_dialog,
                                                                const gchar             *username );

extern  gchar*                      nwam_proxy_password_dialog_get_username (
                                                                NwamProxyPasswordDialog *proxy_password_dialog );

extern  void                        nwam_proxy_password_dialog_set_password (
                                                                NwamProxyPasswordDialog *proxy_password_dialog,
                                                                const gchar             *password );

extern  gchar*                      nwam_proxy_password_dialog_get_password (
                                                                NwamProxyPasswordDialog *proxy_password_dialog );

G_END_DECLS

#endif	/* _NWAM_PROXY_PASSWORD_DIALOG_H */

