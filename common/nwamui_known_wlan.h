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
 * File:   nwamui_known_wlan.h
 * Author: darrenk
 *
 * Created on June 29, 2007, 10:20 AM
 */

#ifndef _NWAMUI_KNOWN_WLAN_H
#define	_NWAMUI_KNOWN_WLAN_H

#ifndef _libnwamui_H
#error "Please include libnwamui.h header instead."
#endif

G_BEGIN_DECLS

/* GObject Creation */

#define NWAMUI_TYPE_KNOWN_WLAN               (nwamui_known_wlan_get_type ())
#define NWAMUI_KNOWN_WLAN(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), NWAMUI_TYPE_KNOWN_WLAN, NwamuiKnownWlan))
#define NWAMUI_KNOWN_WLAN_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), NWAMUI_TYPE_KNOWN_WLAN, NwamuiKnownWlanClass))
#define NWAMUI_IS_KNOWN_WLAN(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NWAMUI_TYPE_KNOWN_WLAN))
#define NWAMUI_IS_KNOWN_WLAN_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), NWAMUI_TYPE_KNOWN_WLAN))
#define NWAMUI_KNOWN_WLAN_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), NWAMUI_TYPE_KNOWN_WLAN, NwamuiKnownWlanClass))


typedef struct _NwamuiKnownWlan		NwamuiKnownWlan;
typedef struct _NwamuiKnownWlanClass      NwamuiKnownWlanClass;
typedef struct _NwamuiKnownWlanPrivate	NwamuiKnownWlanPrivate;

struct _NwamuiKnownWlan
{
	NwamuiWifiNet                     object;

	/*< private >*/
	NwamuiKnownWlanPrivate       *prv;
};

struct _NwamuiKnownWlanClass
{
	NwamuiWifiNetClass                parent_class;
};

extern  GType                       nwamui_known_wlan_get_type (void) G_GNUC_CONST;

extern  NwamuiObject*               nwamui_known_wifi_new(const gchar *name);

extern  void                        nwamui_known_wlan_update_from_wlan_t(NwamuiKnownWlan* self, nwam_wlan_t *wlan);

extern  NwamuiObject*               nwamui_known_wlan_new_with_handle(nwam_known_wlan_handle_t known_wlan);


G_END_DECLS
        
#endif	/* _NWAMUI_KNOWN_WLAN_H */

