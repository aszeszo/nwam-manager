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

#ifndef _HELP_REFS_H 
#include "help_refs.h"
#endif /* _HELP_REFS_H  */

/* Disable handling of HOST_FILE since it was removed from location
 * at the end of PHASE 1 work
 */
#define _DISABLE_HOSTS_FILE  (1)


#if 0
#define  NWAM_ICON_NETWORK_LOCATION         "network-location"
#endif

/* Device icons. */
#define  NWAM_ICON_NETWORK_WIRELESS "nwam-wireless-nosignal"
#define  NWAM_ICON_NETWORK_WIRED    "network-wired"

/* Status icons */
#define  NWAM_ICON_CONNECTED "nwam-connected"
#define  NWAM_ICON_WARNING   "nwam-warning"
#define  NWAM_ICON_ERROR     "nwam-error"

/* Wireless strength icons */

/* For menus - is signal bars style */
#define  NWAM_BAR_ICON_WIRELESS_STRENGTH_NONE      "nwam-wireless-signal-none"
#define  NWAM_BAR_ICON_WIRELESS_STRENGTH_POOR      "nwam-wireless-signal-poor"
#define  NWAM_BAR_ICON_WIRELESS_STRENGTH_FAIR      "nwam-wireless-signal-fair"
#define  NWAM_BAR_ICON_WIRELESS_STRENGTH_GOOD      "nwam-wireless-signal-good"
#define  NWAM_BAR_ICON_WIRELESS_STRENGTH_EXCELLENT "nwam-wireless-signal-good"

/* For panel, is radar style */
#define  NWAM_RADAR_ICON_WIRELESS_STRENGTH_NONE      "nwam-wireless-nosignal"
#define  NWAM_RADAR_ICON_WIRELESS_STRENGTH_POOR      "nwam-wireless-0-24"
#define  NWAM_RADAR_ICON_WIRELESS_STRENGTH_FAIR      "nwam-wireless-25-49"
#define  NWAM_RADAR_ICON_WIRELESS_STRENGTH_GOOD      "nwam-wireless-50-74"
#define  NWAM_RADAR_ICON_WIRELESS_STRENGTH_EXCELLENT "nwam-wireless-75-100"

typedef enum {
    NWAMUI_WIRELESS_ICON_TYPE_RADAR = 0,
    NWAMUI_WIRELESS_ICON_TYPE_BARS,
    NWAMUI_WIRELESS_ICON_TYPE_LAST /* Not to be used directly */
} nwamui_wireless_icon_type_t;

#define  NWAM_ICON_NETWORK_SECURE   "nwam-wireless-secure"
#define  NWAM_ICON_NETWORK_INSECURE "nwam-wireless-insecure"

/* Condition icons */
#define  NWAM_ICON_COND_ACT_MODE_SYSTEM          "nwam-activation-system"
#define  NWAM_ICON_COND_ACT_MODE_MANUAL          "nwam-activation-manual"
#define  NWAM_ICON_COND_ACT_MODE_PRIORITIZED     GTK_STOCK_YES
#define  NWAM_ICON_COND_ACT_MODE_CONDITIONAL_ALL "nwam-activation-conditional"
#define  NWAM_ICON_COND_ACT_MODE_CONDITIONAL_ANY "nwam-activation-conditional"

#define  NWAM_ICON_COND_PRIORITY_GROUP_MODE_EXCLUSIVE "priority-group-1"
#define  NWAM_ICON_COND_PRIORITY_GROUP_MODE_SHARED    "priority-group-2"
#define  NWAM_ICON_COND_PRIORITY_GROUP_MODE_ALL       "priority-group-3"

/* TODO test icons */
#define  NWAM_ICON_NCU_STATUS_ENABLED     GTK_STOCK_YES
#define  NWAM_ICON_NCU_STATUS_DISABLED    GTK_STOCK_NO
#define  NWAM_ICON_NCU_STATUS_CONDITIONAL GTK_STOCK_STOP
#define  NWAM_ICON_NCU_STATUS_PRIORITIZED GTK_STOCK_GO_FORWARD

/* NWAM Versions of g_log functions, to prefix with function names */
#ifdef G_HAVE_ISO_VARARGS
#define nwamui_debug(_fmt, ...)         g_debug("%s: " _fmt, __func__, __VA_ARGS__)
#define nwamui_warning(_fmt, ...)       g_warning("%s: " _fmt, __func__, __VA_ARGS__)
#define nwamui_error(_fmt, ...)         g_error("%s: " _fmt, __func__, __VA_ARGS__)
#elif defined(G_HAVE_GNUC_VARARGS)
#define nwamui_debug(_fmt, args...)     g_debug("%s: " _fmt, __func__, args)
#define nwamui_warning(_fmt, args...)   g_warning("%s: " _fmt, __func__, args)
#define nwamui_error(_fmt, args...)     g_error("%s: " _fmt, __func__, args)
#else
#define nwamui_debug        g_debug
#define nwamui_warning      g_warning
#define nwamui_error        g_error
#endif

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

void marshal_VOID__OBJECT_POINTER(GClosure     *closure,
  GValue       *return_value G_GNUC_UNUSED,
  guint         n_param_values,
  const GValue *param_values,
  gpointer      invocation_hint G_GNUC_UNUSED,
  gpointer      marshal_data);

/* Utility Functions */
extern GtkWidget*               nwamui_util_glade_get_widget( const gchar* widget_name );

extern void                     nwamui_util_default_log_handler_init( void );

extern void                     nwamui_util_set_debug_mode( gboolean enabled );

extern gboolean                 nwamui_util_is_debug_mode( void );

extern gchar*                   nwamui_util_wifi_sec_to_string( nwamui_wifi_security_t wireless_sec );

extern gchar*                   nwamui_util_wifi_sec_to_short_string( nwamui_wifi_security_t wireless_sec );

extern gchar*                   nwamui_util_wifi_wpa_config_to_string( nwamui_wifi_wpa_config_t wpa_config );

extern void                     nwamui_util_obj_ref( gpointer obj, gpointer user_data );

extern void                     nwamui_util_obj_unref( gpointer obj, gpointer user_data );

extern void                     nwamui_util_free_obj_list( GList* obj_list );

extern GList*                   nwamui_util_copy_obj_list( GList* obj_list );

extern GdkPixbuf*               nwamui_util_get_network_status_icon(nwamui_ncu_type_t ncu_type, nwamui_wifi_signal_strength_t strength, nwamui_env_status_t net_status, gint size);

extern GdkPixbuf*               nwamui_util_get_ncu_status_icon( NwamuiNcu* ncu, gint size );

extern const gchar*             nwamui_util_get_ncu_group_icon( NwamuiNcu* ncu  );

extern const gchar*             nwamui_util_get_active_mode_icon( NwamuiObject *object  );

extern GdkPixbuf*               nwamui_util_get_network_type_icon( nwamui_ncu_type_t ncu_type );

extern GdkPixbuf*               nwamui_util_get_env_status_icon( GtkStatusIcon* status_icon, nwamui_env_status_t daemon_status, gint force_size );

extern GdkPixbuf*               nwamui_util_get_wireless_strength_icon_with_size( nwamui_wifi_signal_strength_t signal_strength, 
                                                                                  nwamui_wireless_icon_type_t icon_type,
                                                                                  gint size);

extern GdkPixbuf*               nwamui_util_get_network_security_icon( nwamui_wifi_security_t sec_type,
                                                                        gboolean small);

extern void                     nwamui_util_show_help( const gchar* link_name );

extern gchar*                   nwamui_util_rename_dialog_run(GtkWindow* parent_window, const gchar* title, const gchar* current_name);

extern gboolean                 nwamui_util_ask_yes_no(GtkWindow* parent_window, const gchar* title, const gchar* message);

extern gboolean                 nwamui_util_confirm_removal(GtkWindow* parent_window, const gchar* title, const gchar* message);

extern void                     nwamui_util_show_message(   GtkWindow* parent_window, 
                                                            GtkMessageType type, 
                                                            const gchar* title, 
                                                            const gchar* message, 
                                                            gboolean block );

extern GList*                   nwamui_util_map_condition_strings_to_object_list( char** conditions );

extern char**                   nwamui_util_map_object_list_to_condition_strings( GList* conditions, guint *len );

extern GList*                   nwamui_util_strv_to_glist( gchar **strv );

extern char**                   nwamui_util_glist_to_strv( GList *list );

extern gchar*                   nwamui_util_encode_menu_label( gchar **modified_label );

extern gchar*                   nwamui_util_convert_prefixlen_to_netmask_str( sa_family_t family, guint prefixlen );

extern guint                    nwamui_util_convert_netmask_str_to_prefixlen( sa_family_t family, const gchar* netmask_str );

extern gboolean                 nwamui_util_get_interface_address(  const char  *ifname, 
                                                                    sa_family_t  family, 
                                                                    gchar      **address_p, 
                                                                    gint        *prefixlen_p, 
                                                                    gboolean    *is_dhcp_p );

extern gboolean                 nwamui_util_set_entry_smf_fmri_completion( GtkEntry* entry );

extern void                     nwamui_util_set_entry_ip_address_only( GtkEntry* entry, 
                                                                       gboolean is_v6, 
                                                                       gboolean validate_on_focus_out );

extern void                     nwamui_util_unset_entry_ip_address_only( GtkEntry* entry );

extern gboolean                 nwamui_util_validate_ip_address(    GtkWidget   *widget,
                                                                    const gchar *address_str,
                                                                    gboolean     is_v6,
                                                                    gboolean     show_error_dialog );

extern void                     nwamui_util_window_title_append_hostname( GtkDialog* dialog );

extern GList*                   nwamui_util_parse_string_to_glist( const gchar* str );

extern gchar*                   nwamui_util_glist_to_comma_string( GList* list );

extern gboolean                 nwamui_util_split_address_prefix( gboolean v6, 
                                                                  const gchar* address_prefix, 
                                                                  gchar **address, 
                                                                  gchar **prefix );

extern gchar*                   nwamui_util_join_address_prefix( gboolean v6, 
                                                                 const gchar *address, 
                                                                 const gchar *prefix );

G_END_DECLS

#endif	/* _libnwamui_H */

