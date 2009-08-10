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
 * File:   nwam_pref_iface.h
 *
 * Created on June 7, 2007, 11:13 AM
 * 
 */

#ifndef _NWAM_PREF_IFCACE_H_
#define _NWAM_PREF_IFCACE_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define NWAM_TYPE_PREF_IFACE (nwam_pref_iface_get_type ())
#define NWAM_PREF_IFACE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), NWAM_TYPE_PREF_IFACE, NwamPrefIFace))
#define NWAM_IS_PREF_IFACE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), NWAM_TYPE_PREF_IFACE))
#define NWAM_GET_PREF_INTERFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE((obj), NWAM_TYPE_PREF_IFACE, NwamPrefInterface))

typedef struct _NwamPrefIFace NwamPrefIFace; /* dummy object */
typedef struct _NwamPrefInterface NwamPrefInterface;

struct _NwamPrefInterface {
	GTypeInterface parent;

	gboolean (*refresh) (NwamPrefIFace *iface, gpointer user_data, gboolean force);
	gboolean (*apply) (NwamPrefIFace *iface, gpointer user_data);
	gboolean (*cancel) (NwamPrefIFace *iface, gpointer user_data);
	gboolean (*help) (NwamPrefIFace *iface, gpointer user_data);
    gint (*dialog_run)(NwamPrefIFace *iface, GtkWindow *parent);
    GtkWindow *(*dialog_get_window)(NwamPrefIFace *iface);
};

extern GType nwam_pref_iface_get_type (void) G_GNUC_CONST;

/* for nwam_pref application internal usage */

extern gboolean         nwam_pref_refresh (NwamPrefIFace *iface, gpointer user_data, gboolean force);
extern gboolean         nwam_pref_apply (NwamPrefIFace *iface, gpointer user_data);
extern gboolean         nwam_pref_cancel (NwamPrefIFace *iface, gpointer user_data);
extern gboolean         nwam_pref_help (NwamPrefIFace *iface, gpointer user_data);
extern gint             nwam_pref_dialog_run(NwamPrefIFace *iface, GtkWindow *parent);
extern gboolean         nwam_pref_dialog_raise(NwamPrefIFace *iface);
extern GtkWindow*       nwam_pref_dialog_get_window(NwamPrefIFace *iface);

G_END_DECLS

#endif /* _NWAM_PREF_IFCACE_H_ */

