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
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include "notify.h"

static NotifyNotification*   notification           = NULL;
static GtkStatusIcon*        parent_status_icon     = NULL;
static gchararray            default_icon           = GTK_STOCK_NETWORK;

static NotifyNotification *
get_notification( void ) {
    
    g_assert ( parent_status_icon != NULL );

    if ( notification == NULL ) {
        notify_init( PACKAGE );
        
        notification = notify_notification_new_with_status_icon("Empty Text", "Empty Body", "nwam-earth-normal", parent_status_icon  );
    }
    return notification;
}

static void
notification_cleanup( void ) {
    if ( notification != NULL ) {
        g_object_unref( parent_status_icon );
        notify_notification_close(notification, NULL); /* Close notification now! */
        notify_uninit();
    }
}

void
nwam_notification_show_message( const gchararray summary, const gchararray body, const gchararray icon ) {
    GError              *err = NULL;
    NotifyNotification  *n = get_notification();
    
    g_assert ( summary != NULL ); /* Must have a value! */
    
    notify_notification_clear_actions (n);
    notify_notification_update(n, summary, body,
      ((icon!=NULL)?(icon):(default_icon)) );
    if ( !notify_notification_show(n, &err) ) {
        g_warning(err->message);
        g_error_free( err );
    }
}

void
nwam_notification_show_message_with_action (const gchararray summary,
  const gchararray body,
  const gchararray icon,
  const gchararray action,
  const gchararray label,
  NotifyActionCallback callback,
  gpointer user_data,
  GFreeFunc free_func) {
    GError              *err = NULL;
    NotifyNotification  *n = get_notification();
    const gchar* real_action = action;
    const gchar* real_label = label;
    
    g_assert ( summary != NULL ); /* Must have a value! */
    
    notify_notification_clear_actions (n);
    notify_notification_update(n, summary, body,
      ((icon!=NULL)?(icon):(default_icon)) );

    if (action == NULL || *(action) == '\0') {
        real_action = "default";
    }
    if (label == NULL || *(label) == '\0') {
        real_label = "default";
    }
    
    notify_notification_add_action (n, real_action, real_label,
      callback, user_data, free_func);

    if ( !notify_notification_show(n, &err) ) {
        g_warning(err->message);
        g_error_free( err );
    }
}

void
nwam_notification_cleanup( void )
{
    notification_cleanup();
}

void
nwam_notification_init( GtkStatusIcon* status_icon )
{
    g_assert ( status_icon != NULL );
    
    g_object_ref( status_icon );
    parent_status_icon = status_icon;
    
}
