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

#ifndef _notify_H
#define	_notify_H

#include <libnotify/notify.h>
#include <libnotify/notification.h>
#include "libnwamui.h"

G_BEGIN_DECLS

typedef enum {
    NOTIFICATION_STYLE_REUSE,
    NOTIFICATION_STYLE_CREATE_ALWAYS,
    NOTIFICATION_STYLE_CREATE_ALWAYS_NO_STATUS_ICON
} notification_style_t;    

/* 
 * Should be called before trying to display any messages.
 *
 * Requires a StatusIcon to link the popup text to.
 */
void nwam_notification_init( GtkStatusIcon* status_icon );

/*
 * Different notification icon styles
 */
void notify_notification_set_notification_style( notification_style_t style );

/* 
 * Show a message to tell the user that the nwam daemon is unavailable.
 */
void nwam_notification_show_nwam_unavailable( void );

/* 
 * Show a message to tell the user that a connection has been made.
 *
 * For a wireless ncu it will also show the ESSID.
 */
gboolean nwam_notification_show_ncu_connected( NwamuiNcu* ncu );

/* 
 * Show a message to tell the user that a connection has been broken.
 *
 * For a wireless ncu it will also show the ESSID.
 */
void nwam_notification_show_ncu_disconnected( NwamuiNcu*           ncu,
                                              NotifyActionCallback callback,
                                              GObject*             user_object );

/* 
 * Show a message to tell the user that an attempt to connect to a wireless
 * ncu has failed.
 */
void nwam_notification_show_ncu_wifi_connect_failed( NwamuiNcu* ncu );

void nwam_notification_show_ncu_dup_address(NwamuiNcu* ncu);

/* 
 * Show a message to tell the user that wireless key is needed.
 */
void nwam_notification_show_ncu_wifi_key_needed( NwamuiWifiNet* wifi_net, NotifyActionCallback callback );

/* 
 * Show a message to tell the user that wireless selection is needed.
 */
void nwam_notification_show_ncu_wifi_selection_needed(   NwamuiNcu           *ncu, 
                                                         NotifyActionCallback callback,
                                                         GObject*             user_object );

/* 
 * Show a message to tell the user that no wireless networks could be found.
 */
void nwam_notification_show_no_wifi_networks( NotifyActionCallback callback, GObject *user_object );

/* 
 * Show a message to tell the user that the active location has changed.
 */
void nwam_notification_show_location_changed( NwamuiEnv* env );

/* 
 * Show a message to tell the user that the active NCP has changed.
 */
void nwam_notification_show_ncp_changed( NwamuiNcp* ncp );

/* 
 * Should be called prior to exiting, to ensure notificaiton area is cleaned up.
 */
void nwam_notification_cleanup( void );

void nwam_notification_set_default_icon(GdkPixbuf *pixbuf);

G_END_DECLS

#endif	/* _notify_H */

