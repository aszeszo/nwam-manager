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

#ifndef _LIBNWAM_H
#include <libnwam.h>
#endif /* _LIBNWAM_H */

/* TODO: Figure out a replacement for these Phase 0.5 types */
typedef guint64 libnwam_diag_cause_t;

#ifndef __GTK_H__
#include <gtk/gtk.h>
#endif /* __GTK_H__ */
        
#ifndef __GDK_PIXBUF_H__
#include <gdk/gdkpixbuf.h>
#endif /* __GDK_PIXBUF_H__ */
        
#ifndef _NWAMUI_OBJECT_H
#include "nwamui_object.h"
#endif /*_NWAMUI_OBJECT_H */

#ifndef _NWAMUI_PROF_H
#include "nwamui_prof.h"
#endif /*_NWAMUI_PROF_H */

#ifndef _NWAMUI_COND_H
#include "nwamui_cond.h"
#endif /*_NWAMUI_COND_H */
        
#ifndef _NWAMUI_WIFI_NET_H
#include "nwamui_wifi_net.h"
#endif /* _NWAMUI_WIFI_NET_H */

#ifndef _NWAMUI_SVC_H
#include "nwamui_svc.h"
#endif /* _NWAMUI_SVC_H */

#ifndef _NWAMUI_NCU_H
#include "nwamui_ncu.h"
#endif /* _NWAMUI_NCU_H */

#ifndef _NWAMUI_NCP_H
#include "nwamui_ncp.h"
#endif /* _NWAMUI_NCP_H */

#ifndef _NWAMUI_IP_H
#include "nwamui_ip.h"
#endif /* _NWAMUI_IP_H */

#ifndef _NWAMUI_ENV_H
#include "nwamui_env.h"
#endif /* _NWAMUI_ENV_H */

#ifndef _NWAMUI_ENM_H
#include "nwamui_enm.h"
#endif /* _NWAMUI_ENM_H */

#ifndef _NWAMUI_DAEMON_H
#include "nwamui_daemon.h"
#endif /* _NWAMUI_DAEMON_H */

#define  NWAM_ICON_NETWORK_WIRELESS         "network-wireless"
#define  NWAM_ICON_NETWORK_LOCATION         "network-location"

/* Status icons */
#define  NWAM_ICON_EARTH_ERROR              "network-location-error"
#define  NWAM_ICON_EARTH_WARNING            "network-location-warning"
#define  NWAM_ICON_EARTH_CONNECTED          "network-location-connected"
#define  NWAM_ICON_NETWORK_IDLE             "network-wired-connected"
#define  NWAM_ICON_NETWORK_WARNING          "network-wired-warning"
#define  NWAM_ICON_NETWORK_OFFLINE          "network-wired-error"
#define  NWAM_ICON_WIRELESS_CONNECTED       "wireless-wireless-connected"
#define  NWAM_ICON_WIRELESS_WARNING         "wireless-wireless-warning"
#define  NWAM_ICON_WIRELESS_ERROR           "wireless-wireless-error"

/* Wireless strength icons */
#define  NWAM_ICON_WIRELESS_STRENGTH_NONE   "network-wireless-signal-none"
#define  NWAM_ICON_WIRELESS_STRENGTH_POOR   "network-wireless-signal-poor"
#define  NWAM_ICON_WIRELESS_STRENGTH_FAIR   "network-wireless-signal-fair"
#define  NWAM_ICON_WIRELESS_STRENGTH_GOOD   "network-wireless-signal-good"
#define  NWAM_ICON_WIRELESS_STRENGTH_EXCELLENT   "network-wireless-signal-good"

#define  NWAM_ICON_NETWORK_SECURE           "network-secure"
#define  NWAM_ICON_NETWORK_INSECURE         "network-insecure"

/* TODO test icons */
#define  NWAM_ICON_NCU_STATUS_ENABLED       GTK_STOCK_YES
#define  NWAM_ICON_NCU_STATUS_DISABLED      GTK_STOCK_NO
#define  NWAM_ICON_NCU_STATUS_CONDITIONAL   GTK_STOCK_STOP
#define  NWAM_ICON_NCU_STATUS_PRIORITIZED   GTK_STOCK_GO_FORWARD

/* if no favorite networks are available, what should we do ? */
enum 
{
    PROF_SHOW_AVAILABLE_NETWORK_LIST = 0,
    PROF_JOIN_OPEN_NETWORK,
    PROF_REMAIN_OFFLINE,
};

/* Marshal Functions */
void marshal_VOID__INT_POINTER (GClosure     *closure,
  GValue       *return_value G_GNUC_UNUSED,
  guint         n_param_values,
  const GValue *param_values,
  gpointer      invocation_hint G_GNUC_UNUSED,
  gpointer      marshal_data);

void marshal_VOID__OBJECT_OBJECT(GClosure     *closure,
  GValue       *return_value G_GNUC_UNUSED,
  guint         n_param_values,
  const GValue *param_values,
  gpointer      invocation_hint G_GNUC_UNUSED,
  gpointer      marshal_data);

void marshal_VOID__INT_OBJECT_POINTER (GClosure     *closure,
  GValue       *return_value G_GNUC_UNUSED,
  guint         n_param_values,
  const GValue *param_values,
  gpointer      invocation_hint G_GNUC_UNUSED,
  gpointer      marshal_data);

void marshal_OBJECT__VOID(GClosure     *closure,
  GValue       *return_value,
  guint         n_param_values,
  const GValue *param_values,
  gpointer      invocation_hint G_GNUC_UNUSED,
  gpointer      marshal_data);

/* Utility Functions */
extern GtkWidget*               nwamui_util_glade_get_widget( const gchar* widget_name );

extern gchar*                   nwamui_util_wifi_sec_to_string( nwamui_wifi_security_t wireless_sec );

extern gchar*                   nwamui_util_wifi_sec_to_short_string( nwamui_wifi_security_t wireless_sec );

extern gchar*                   nwamui_util_wifi_wpa_config_to_string( nwamui_wifi_wpa_config_t wpa_config );

extern void                     nwamui_util_obj_ref( gpointer obj, gpointer user_data );

extern void                     nwamui_util_obj_unref( gpointer obj, gpointer user_data );

extern void                     nwamui_util_free_obj_list( GList* obj_list );

extern GList*                   nwamui_util_copy_obj_list( GList* obj_list );

extern GdkPixbuf*               nwamui_util_get_network_status_icon( NwamuiNcu* ncu  );

extern const gchar*             nwamui_util_get_ncu_group_icon( NwamuiNcu* ncu  );

extern const gchar*             nwamui_util_get_active_mode_icon( NwamuiObject *object  );

extern GdkPixbuf*               nwamui_util_get_network_type_icon( nwamui_ncu_type_t ncu_type );

extern GdkPixbuf*               nwamui_util_get_env_status_icon( GtkStatusIcon* status_icon, nwamui_env_status_t env_status );

extern GdkPixbuf*               nwamui_util_get_wireless_strength_icon( nwamui_wifi_signal_strength_t signal_strength,
                                                                        gboolean small);

extern GdkPixbuf*               nwamui_util_get_network_security_icon( nwamui_wifi_security_t sec_type,
                                                                        gboolean small);

extern void                     nwamui_util_show_help( gchar* link_name );

extern gchar*                   nwamui_util_rename_dialog_run(GtkWindow* parent_window, const gchar* title, const gchar* current_name);

extern gboolean                 nwamui_util_ask_yes_no(GtkWindow* parent_window, const gchar* title, const gchar* message);

extern void                     nwamui_util_show_message(GtkWindow* parent_window, GtkMessageType type, const gchar* title, const gchar* message);

extern GList*                   nwamui_util_map_condition_strings_to_object_list( char** conditions );

extern char**                   nwamui_util_map_object_list_to_condition_strings( GList* conditions, guint *len );

extern GList*                   nwamui_util_strv_to_glist( gchar **strv );

extern char**                   nwamui_util_glist_to_strv( GList *list );

extern gchar*                   nwamui_util_convert_prefixlen_to_netmask_str( sa_family_t family, guint prefixlen );

extern gboolean                 nwamui_util_get_interface_address(  const char  *ifname, 
                                                                    sa_family_t  family, 
                                                                    gchar      **address_p, 
                                                                    gint        *prefixlen_p, 
                                                                    gboolean    *is_dhcp_p );

extern gboolean                 nwamui_util_set_entry_smf_fmri_completion( GtkEntry* entry );

G_END_DECLS

#endif	/* _libnwamui_H */

