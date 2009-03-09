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
 * File:   nwamui_wifi_net.h
 * Author: darrenk
 *
 * Created on June 29, 2007, 10:20 AM
 */

#ifndef _NWAMUI_WIFI_NET_H
#define	_NWAMUI_WIFI_NET_H

#ifndef _libnwamui_H
#error "Please include libnwamui.h header instead."
#endif

G_BEGIN_DECLS

/* GObject Creation */

#define NWAMUI_TYPE_WIFI_NET               (nwamui_wifi_net_get_type ())
#define NWAMUI_WIFI_NET(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), NWAMUI_TYPE_WIFI_NET, NwamuiWifiNet))
#define NWAMUI_WIFI_NET_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), NWAMUI_TYPE_WIFI_NET, NwamuiWifiNetClass))
#define NWAMUI_IS_WIFI_NET(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NWAMUI_TYPE_WIFI_NET))
#define NWAMUI_IS_WIFI_NET_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), NWAMUI_TYPE_WIFI_NET))
#define NWAMUI_WIFI_NET_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), NWAMUI_TYPE_WIFI_NET, NwamuiWifiNetClass))


typedef struct _NwamuiWifiNet		NwamuiWifiNet;
typedef struct _NwamuiWifiNetClass      NwamuiWifiNetClass;
typedef struct _NwamuiWifiNetPrivate	NwamuiWifiNetPrivate;

struct _NwamuiWifiNet
{
	NwamuiObject                     object;

	/*< private >*/
	NwamuiWifiNetPrivate       *prv;
};

struct _NwamuiWifiNetClass
{
	NwamuiObjectClass                parent_class;
};

/* Enumerations */
typedef enum {
    NWAMUI_WIFI_SEC_NONE,
    NWAMUI_WIFI_SEC_WEP_HEX,
    NWAMUI_WIFI_SEC_WEP_ASCII,
    NWAMUI_WIFI_SEC_WPA_PERSONAL,
    /* NWAMUI_WIFI_SEC_WPA_ENTERPRISE,- Currently not supported  */
    NWAMUI_WIFI_SEC_LAST /* Not to be used directly */
} nwamui_wifi_security_t;

typedef enum {
    NWAMUI_WIFI_WPA_CONFIG_AUTOMATIC, /* TODO - Figure out if this is needed */
    NWAMUI_WIFI_WPA_CONFIG_LEAP,
    NWAMUI_WIFI_WPA_CONFIG_PEAP, /* aka MSCHAPv2 */
    NWAMUI_WIFI_WPA_CONFIG_LAST /* Not to be used directly */	
} nwamui_wifi_wpa_config_t;

typedef enum {
    NWAMUI_WIFI_STRENGTH_NONE,
    NWAMUI_WIFI_STRENGTH_VERY_WEAK,
    NWAMUI_WIFI_STRENGTH_WEAK,
    NWAMUI_WIFI_STRENGTH_GOOD,
    NWAMUI_WIFI_STRENGTH_VERY_GOOD,
    NWAMUI_WIFI_STRENGTH_EXCELLENT,
    NWAMUI_WIFI_STRENGTH_LAST /* Not to be used directly */	
} nwamui_wifi_signal_strength_t;


typedef enum {
    NWAMUI_WIFI_STATUS_CONNECTED,
    NWAMUI_WIFI_STATUS_DISCONNECTED,
    NWAMUI_WIFI_STATUS_FAILED,
    NWAMUI_WIFI_STATUS_LAST /* Not to be used directly */	
} nwamui_wifi_status_t;

typedef enum {
    NWAMUI_WIFI_BSS_TYPE_AUTO,
    NWAMUI_WIFI_BSS_TYPE_AP,
    NWAMUI_WIFI_BSS_TYPE_ADHOC,
    NWAMUI_WIFI_BSS_TYPE_LAST /* Not to be used directly */	
} nwamui_wifi_bss_type_t;

extern struct _NwamuiNcu; /* forwardref */

extern  GType                       nwamui_wifi_net_get_type (void) G_GNUC_CONST;

extern gboolean                     nwamui_wifi_net_has_modifications( NwamuiWifiNet* self );

extern gboolean                     nwamui_wifi_net_commit_favourite ( NwamuiWifiNet *self );

extern  NwamuiWifiNet*              nwamui_wifi_net_new(    struct _NwamuiNcu               *ncu,
                                                            const gchar                     *essid, 
                                                            nwamui_wifi_security_t           security,
                                                            GList                           *bssid_list,
                                                            nwamui_wifi_bss_type_t           bss_type );

extern  NwamuiWifiNet*              nwamui_wifi_net_new_with_handle(    struct _NwamuiNcu               *ncu,
                                                                        nwam_known_wlan_handle_t         handle );

extern gboolean                     nwamui_wifi_net_update_with_handle( NwamuiWifiNet* self, nwam_known_wlan_handle_t  handle );

extern void                         nwamui_wifi_net_connect ( NwamuiWifiNet *self );

extern gint                         nwamui_wifi_net_compare( NwamuiWifiNet *self, NwamuiWifiNet *other );

extern gboolean                     nwamui_wifi_net_create_favourite ( NwamuiWifiNet *self );

extern gboolean                     nwamui_wifi_net_delete_favourite ( NwamuiWifiNet *self );

extern gboolean                     nwamui_wifi_net_validate_favourite( NwamuiWifiNet *self, gchar**error_prop );

extern void                         nwamui_wifi_net_set_ncu ( NwamuiWifiNet *self, struct _NwamuiNcu* ncu );
                                
extern struct _NwamuiNcu*           nwamui_wifi_net_get_ncu ( NwamuiWifiNet *self );

extern void                         nwamui_wifi_net_set_essid ( NwamuiWifiNet *self, const gchar *essid );
                                
extern gchar*                       nwamui_wifi_net_get_essid ( NwamuiWifiNet *self );

extern gchar*                       nwamui_wifi_net_get_unique_name ( NwamuiWifiNet *self );

extern gchar*                       nwamui_wifi_net_get_display_string (NwamuiWifiNet *self, gboolean has_many_wireless );

extern void                         nwamui_wifi_net_set_status ( NwamuiWifiNet *self, nwamui_wifi_status_t status );
          
extern nwamui_wifi_status_t         nwamui_wifi_net_get_status ( NwamuiWifiNet *self );

extern void                         nwamui_wifi_net_set_security ( NwamuiWifiNet *self, nwamui_wifi_security_t security );
          
extern nwamui_wifi_security_t       nwamui_wifi_net_get_security ( NwamuiWifiNet *self );

extern void                         nwamui_wifi_net_set_speed ( NwamuiWifiNet *self, guint speed );
extern guint                        nwamui_wifi_net_get_speed ( NwamuiWifiNet *self );


extern void                         nwamui_wifi_net_set_mode ( NwamuiWifiNet *self, const gchar* mode );
extern gchar*                       nwamui_wifi_net_get_mode ( NwamuiWifiNet *self );

extern void                         nwamui_wifi_net_set_channel (   NwamuiWifiNet      *self,
                                                                    gint                channel );
extern gint                         nwamui_wifi_net_get_channel (NwamuiWifiNet *self );

extern void                         nwamui_wifi_net_set_signal_strength (   NwamuiWifiNet                  *self,
                                                                            nwamui_wifi_signal_strength_t   signal_strength );
                        
extern nwamui_wifi_signal_strength_t nwamui_wifi_net_get_signal_strength (NwamuiWifiNet *self );

extern const gchar*                 nwamui_wifi_net_convert_strength_to_string( nwamui_wifi_signal_strength_t strength );

extern const gchar*                 nwamui_wifi_net_get_signal_strength_string(NwamuiWifiNet *self );

extern nwamui_wifi_bss_type_t        nwamui_wifi_net_get_bss_type (NwamuiWifiNet *self );

extern void                         nwamui_wifi_net_set_wpa_config (    NwamuiWifiNet           *self,
                                                                        nwamui_wifi_wpa_config_t wpa_config );
                           
extern nwamui_wifi_wpa_config_t     nwamui_wifi_net_get_wpa_config (    NwamuiWifiNet *self );

extern void                         nwamui_wifi_net_set_wep_password (  NwamuiWifiNet *self, const gchar *wep_password );
                       
extern gchar*                       nwamui_wifi_net_get_wep_password (  NwamuiWifiNet *self );

extern void                         nwamui_wifi_net_set_wpa_username (  NwamuiWifiNet *self, const gchar *wpa_username );
                     
extern gchar*                       nwamui_wifi_net_get_wpa_username (  NwamuiWifiNet *self );

extern void                         nwamui_wifi_net_set_wpa_password (  NwamuiWifiNet *self, const gchar *wpa_password );
                            
extern gchar*                       nwamui_wifi_net_get_wpa_password (  NwamuiWifiNet *self );

extern void                         nwamui_wifi_net_set_wpa_cert_file ( NwamuiWifiNet *self, const gchar *wpa_cert_file );
                      
extern gchar*                       nwamui_wifi_net_get_wpa_cert_file ( NwamuiWifiNet *self );

extern void                         nwamui_wifi_net_set_bssid_list ( NwamuiWifiNet *self, const GList*   bssid_list );
extern GList*                       nwamui_wifi_net_get_bssid_list ( NwamuiWifiNet *self );
                          
                          
extern void                         nwamui_wifi_net_set_priority ( NwamuiWifiNet *self, guint64 priority );
extern guint64                      nwamui_wifi_net_get_priority ( NwamuiWifiNet *self );


extern nwamui_wifi_security_t       nwamui_wifi_net_security_map ( uint32_t _sec_mode );

extern nwamui_wifi_signal_strength_t nwamui_wifi_net_strength_map (const gchar *strength);

extern nwamui_wifi_bss_type_t        nwamui_wifi_net_bss_type_map (const gchar *bss_type);
G_END_DECLS
        
#endif	/* _NWAMUI_WIFI_NET_H */

