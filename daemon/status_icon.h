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

#ifndef _status_icon_H
#define	_status_icon_H

G_BEGIN_DECLS

gint nwam_status_icon_create( void );

GtkStatusIcon* nwam_status_icon_get_widget( gint index );

void nwam_status_icon_set_activate_callback( gint index, GCallback activate_cb );

void nwam_status_icon_show( gint index );

void nwam_status_icon_hide( gint index );

void nwam_status_icon_blink( gint index );

void nwam_status_icon_no_blink( gint index );

void nwam_status_icon_set_tooltip (gint index, const gchar *str);

G_END_DECLS

#endif	/* _status_icon_H */

