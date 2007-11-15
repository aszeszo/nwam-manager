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
#include <libnotify/notify.h>
#include "notify.h"

#define DEBUG()	g_debug ("[[ %20s : %-4d ]]", __func__, __LINE__)

static NotifyNotification*   notification           = NULL;
static GtkStatusIcon*        parent_status_icon     = NULL;
static gchararray            default_icon           = GTK_STOCK_NETWORK;

static 
NotifyNotification *get_notification( void ) {
    
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
    
    notify_notification_update(n, summary, body, ((icon!=NULL)?(icon):(default_icon)) );
    if ( !notify_notification_show(n, &err) ) {
        g_warning(err->message);
        g_error_free( err );
    }
}

static void
on_ncu_up (NwamuiDaemon* daemon, NwamuiNcu* ncu, gpointer udata)
{
    gchar *sname;
    gchar *ifname;
    gchar *addr;
    gint speed;
    gint signal;
    gchar *summary, *body;

    DEBUG();

	/* generic info */
    ifname = nwamui_ncu_get_device_name (ncu);
    if (!nwamui_ncu_get_ipv6_active (ncu)) {
        addr = nwamui_ncu_get_ipv4_address (ncu);
    } else {
        addr = nwamui_ncu_get_ipv6_address (ncu);
    }
    speed = nwamui_ncu_get_speed (ncu);

    switch (nwamui_ncu_get_ncu_type (ncu)) {
    case NWAMUI_NCU_TYPE_WIRED:
        sname = nwamui_ncu_get_vanity_name (ncu);
        summary = g_strdup_printf (_("'%s' is connected"), sname);
        body = g_strdup_printf (_("Interface: %s\n"
                                  "Address: %s\n"
                                  "Speed: %d\n"),
                                ifname, addr, speed);
        break;
    case NWAMUI_NCU_TYPE_WIRELESS: {
        NwamuiWifiNet *wifi = nwamui_ncu_get_wifi_info (ncu);
        sname = nwamui_wifi_net_get_essid (wifi);
        signal = (gint) ((gfloat) (nwamui_wifi_net_get_signal_strength (wifi) /
                                   NWAMUI_WIFI_STRENGTH_LAST) * 100);
        summary = g_strdup_printf (_("Connected to '%s'"), sname);
        body = g_strdup_printf (_("Interface: %s\n"
                                  "Address: %s\n"
                                  "Speed: %d\n"
                                  "Signal Strength: %d%%\n"),
                                ifname, addr, speed);
        break;
    }
    default:
        g_assert_not_reached ();
    }
    nwam_notification_show_message (summary, body, NULL);
    g_free (sname);
    g_free (ifname);
    g_free (addr);
    g_free (summary);
    g_free (body);
}

static void
on_ncu_down (NwamuiDaemon* daemon, NwamuiNcu* ncu, gpointer udata)
{
    gchar *sname;
    gchar *summary, *body;

    DEBUG();

    switch (nwamui_ncu_get_ncu_type (ncu)) {
    case NWAMUI_NCU_TYPE_WIRED:
        sname = nwamui_ncu_get_vanity_name (ncu);
        summary = g_strdup_printf (_("'%s' has disconnected"), sname);
        body = g_strdup_printf (_("%s\n"), "Wired Unknown");
        break;
    case NWAMUI_NCU_TYPE_WIRELESS: {
        NwamuiWifiNet *wifi = nwamui_ncu_get_wifi_info (ncu);
        sname = nwamui_wifi_net_get_essid (wifi);
        summary = g_strdup_printf (_("Disconnected from '%s'"), sname);
        body = g_strdup_printf (_("Interface: %s\n"), "Wireless Unkown");
        break;
    }
    default:
        g_assert_not_reached ();
    }
    nwam_notification_show_message (summary, body, NULL);
    g_free (sname);
    g_free (summary);
    g_free (body);
}

static void
on_active_env_changed (NwamuiDaemon* daemon, NwamuiEnv* env, gpointer udata)
{
    gchar *sname;
    gchar *summary, *body;
    NwamuiNcp* ncp;

    DEBUG();
    sname = nwamui_env_get_name (env);
    summary = g_strdup_printf (_("Switched to environment '%s'"), sname);
    nwamui_daemon_get_active_ncp (daemon);
    body = g_strdup_printf (_("%s\n"), "Unknown");
    nwam_notification_show_message (summary, body, NULL);

    g_free (sname);
    g_free (summary);
    g_free (body);
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

void
nwam_notification_connect (NwamuiDaemon* daemon)
{
    g_signal_connect(daemon, "ncu_up", G_CALLBACK(on_ncu_up), NULL);
    g_signal_connect(daemon, "ncu_down", G_CALLBACK(on_ncu_down), NULL);
    g_signal_connect(daemon, "active_env_changed",
                     G_CALLBACK(on_active_env_changed), NULL);
}
