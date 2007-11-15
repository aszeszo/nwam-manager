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
 * File:   nwam_ui_icons.h
 *
 * Created on May 9, 2007, 11:13 AM
 * 
 */

#ifndef _NWAM_UI_ICONS_H
#define	_NWAM_UI_ICONS_H

G_BEGIN_DECLS


#define NWAM_TYPE_UI_ICONS               (nwam_ui_icons_get_type ())
#define NWAM_UI_ICONS(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), NWAM_TYPE_UI_ICONS, NwamUIIcons))
#define NWAM_UI_ICONS_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), NWAM_TYPE_UI_ICONS, NwamUIIconsClass))
#define NWAM_IS_UI_ICONS(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NWAM_TYPE_UI_ICONS))
#define NWAM_IS_UI_ICONS_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), NWAM_TYPE_UI_ICONS))
#define NWAM_UI_ICONS_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), NWAM_TYPE_UI_ICONS, NwamUIIconsClass))


typedef struct _NwamUIIcons		NwamUIIcons;
typedef struct _NwamUIIconsClass	NwamUIIconsClass;
typedef struct _NwamUIIconsPrivate	NwamUIIconsPrivate;

struct _NwamUIIcons
{
	GObject                      object;

	/*< private >*/
	NwamUIIconsPrivate    *prv;
};

struct _NwamUIIconsClass
{
	GObjectClass                parent_class;
};

#define nwam_wireless_
#define nwam_wired_

/* NWAM UI Stock Icons */
#define  NWAM_ICON_EARTH_INFO			        "nwam-earth-info"
#define  NWAM_ICON_EARTH_NORMAL			        "nwam-earth-normal"
#define  NWAM_ICON_NETWORK_IDLE			        "nwam-network-idle"
#define  NWAM_ICON_NETWORK_OFFLINE			    "nwam-network-offline"
#define  NWAM_ICON_NETWORK_WIRELESS			    "nwam-network-wireless"
#define  NWAM_ICON_WIRELESS_STRENGTH_0_24	    "nwam-wireless-strength-0-24"
#define  NWAM_ICON_WIRELESS_STRENGTH_25_49	    "nwam-wireless-strength-25-49"
#define  NWAM_ICON_WIRELESS_STRENGTH_50_74	    "nwam-wireless-strength-50-74"
#define  NWAM_ICON_WIRELESS_STRENGTH_75_100	    "nwam-wireless-strength-75-100"
#define NWAMUI_WIFI_SEC_NONE_STOCKID			"nwam-wireless-fragile"

GType nwam_ui_icons_get_type (void) G_GNUC_CONST;
NwamUIIcons* nwam_ui_icons_get_instance (void);

GdkPixbuf*   nwam_ui_icons_get_pixbuf( NwamUIIcons* self, const gchar* stock_id );

G_END_DECLS

#endif	/* _NWAM_UI_ICONS_H */

