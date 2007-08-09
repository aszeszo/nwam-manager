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
 * File:   libnwamui.h
 *
 * Created on May 23, 2007, 2:15 PM
 * 
 */

#ifndef _libnwamui_H
#define	_libnwamui_H

G_BEGIN_DECLS

#include <config.h>

#ifndef __GTK_H__
#include <gtk/gtk.h>
#endif /* __GTK_H__ */
        
#ifndef __GDK_PIXBUF_H__
#include <gdk/gdkpixbuf.h>
#endif /* __GDK_PIXBUF_H__ */
        
#ifndef _NWAMUI_WIFI_NET_H
#include "nwamui_wifi_net.h"
#endif /* _NWAMUI_WIFI_NET_H */

#ifndef _NWAMUI_NCP_H
#include "nwamui_ncp.h"
#endif /* _NWAMUI_NCP_H */

#ifndef _NWAMUI_NCU_H
#include "nwamui_ncu.h"
#endif /* _NWAMUI_NCU_H */

#ifndef _NWAMUI_ENV_H
#include "nwamui_env.h"
#endif /* _NWAMUI_ENV_H */

#ifndef _NWAMUI_ENM_H
#include "nwamui_enm.h"
#endif /* _NWAMUI_ENM_H */

#ifndef _NWAMUI_DAEMON_H
#include "nwamui_daemon.h"
#endif /* _NWAMUI_DAEMON_H */

#ifndef _NWAM_UI_ICONS_H
#include "nwam_ui_icons.h"
#endif /* _NWAM_UI_ICONS_H */

/* Utility Functions */
extern GtkWidget*               nwamui_util_glade_get_widget( const gchar* widget_name );

extern gchar*                   nwamui_util_wifi_sec_to_string( nwamui_wifi_security_t wireless_sec );

extern gchar*                   nwamui_util_wifi_sec_to_short_string( nwamui_wifi_security_t wireless_sec );

extern gchar*                   nwamui_util_wifi_wpa_config_to_string( nwamui_wifi_wpa_config_t wpa_config );

extern void                     nwamui_util_obj_ref( gpointer obj, gpointer user_data );

extern void                     nwamui_util_obj_unref( gpointer obj, gpointer user_data );

extern void                     nwamui_util_free_obj_list( GList* obj_list );

extern GdkPixbuf*               nwamui_util_get_network_status_icon( NwamuiNcu* ncu  );

extern GdkPixbuf*               nwamui_util_get_network_type_icon( nwamui_ncu_type_t ncu_type );

extern GdkPixbuf*               nwamui_util_get_wireless_strength_icon( NwamuiNcu* ncu  );

extern void                     nwamui_util_show_help( gchar* link_name );

extern gchar*                   nwamui_util_rename_dialog_run(GtkWindow* parent_window, const gchar* title, const gchar* current_name);

extern gboolean                 nwamui_util_ask_yes_no(GtkWindow* parent_window, const gchar* title, const gchar* message);

extern void                     nwamui_util_show_message(GtkWindow* parent_window, GtkMessageType type, const gchar* title, const gchar* message);

G_END_DECLS

#endif	/* _libnwamui_H */

