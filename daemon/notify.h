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
 */

#ifndef _notify_H
#define	_notify_H

#include <libnotify/notify.h>
#include "libnwamui.h"

G_BEGIN_DECLS

/* 
 * Should be called before trying to display any messages.
 *
 * Requires a StatusIcon to link the popup text to.
 */
void nwam_notification_init( GtkStatusIcon* status_icon );

/* 
 * Displays a message. Only the summary is needed, the body text and icon can be NULL.
 */
void nwam_notification_show_message( const gchararray summary, const gchararray body, const gchararray icon );

/* 
 * Displays a message. Only the summary is needed, the body text and icon can
 * be NULL. If action is NULL or is empty, the default action "default" will be
 * used.
 */
void nwam_notification_show_message_with_action (const gchararray summary,
  const gchararray body,
  const gchararray icon,
  const gchararray action,
  const gchararray label,
  NotifyActionCallback callback,
  gpointer user_data,
  GFreeFunc free_func);

/* 
 * Should be called prior to exiting, to ensure notificaiton area is cleaned up.
 */
void nwam_notification_cleanup( void );

G_END_DECLS

#endif	/* _notify_H */

