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
 * File:   nwam_wireless_chooser.h
 *
 * Created on May 9, 2007, 11:13 AM
 * 
 */

#ifndef _NWAM_WIRELESS_CHOOSER_H
#define	_NWAM_WIRELESS_CHOOSER_H

G_BEGIN_DECLS


#define NWAM_TYPE_WIRELESS_CHOOSER               (nwam_wireless_chooser_get_type ())
#define NWAM_WIRELESS_CHOOSER(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), NWAM_TYPE_WIRELESS_CHOOSER, NwamWirelessChooser))
#define NWAM_WIRELESS_CHOOSER_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), NWAM_TYPE_WIRELESS_CHOOSER, NwamWirelessChooserClass))
#define NWAM_IS_WIRELESS_CHOOSER(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NWAM_TYPE_WIRELESS_CHOOSER))
#define NWAM_IS_WIRELESS_CHOOSER_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), NWAM_TYPE_WIRELESS_CHOOSER))
#define NWAM_WIRELESS_CHOOSER_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), NWAM_TYPE_WIRELESS_CHOOSER, NwamWirelessChooserClass))


typedef struct _NwamWirelessChooser		NwamWirelessChooser;
typedef struct _NwamWirelessChooserClass	NwamWirelessChooserClass;
typedef struct _NwamWirelessChooserPrivate	NwamWirelessChooserPrivate;

struct _NwamWirelessChooser
{
	GObject                      object;

	/*< private >*/
	NwamWirelessChooserPrivate    *prv;
};

struct _NwamWirelessChooserClass
{
	GObjectClass                parent_class;
};

GType nwam_wireless_chooser_get_type (void) G_GNUC_CONST;

extern NwamWirelessChooser*     nwam_wireless_chooser_new (void);
extern gint                     nwam_wireless_chooser_run (NwamWirelessChooser *self, GtkWindow *parent);

G_END_DECLS

#endif	/* _NWAM_WIRELESS_CHOOSER_H */
