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
	GObject                     object;

	/*< private >*/
	NwamuiWifiNetPrivate       *prv;
};

struct _NwamuiWifiNetClass
{
	GObjectClass                parent_class;
};

/* Enumerations */
typedef enum {
    NWAMUI_WIFI_SEC_NONE,
    NWAMUI_WIFI_SEC_WEP_HEX,
    NWAMUI_WIFI_SEC_WEP_ASCII,
    NWAMUI_WIFI_SEC_WPA_PERSONAL,
    NWAMUI_WIFI_SEC_WPA_ENTERPRISE,
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


extern  GType                       nwamui_wifi_net_get_type (void) G_GNUC_CONST;

extern  NwamuiWifiNet*              nwamui_wifi_net_new(    const gchar                     *essid, 
                                                            nwamui_wifi_security_t           security,
                                                            const gchar                     *bssid,
                                                            const gchar                     *mode,
                                                            guint                            speed,
                                                            nwamui_wifi_signal_strength_t    signal_strength,
                                                            nwamui_wifi_wpa_config_t         wpa_config );


extern void                         nwamui_wifi_net_set_essid ( NwamuiWifiNet *self, const gchar *essid );
                                
extern gchar*                       nwamui_wifi_net_get_essid ( NwamuiWifiNet *self );

extern void                         nwamui_wifi_net_set_bssid ( NwamuiWifiNet *self, const gchar *bssid );
                          
extern gchar*                       nwamui_wifi_net_get_bssid ( NwamuiWifiNet *self );

extern void                         nwamui_wifi_net_set_security ( NwamuiWifiNet *self, nwamui_wifi_security_t security );
          
extern nwamui_wifi_security_t       nwamui_wifi_net_get_security ( NwamuiWifiNet *self );

extern void                         nwamui_wifi_net_set_speed ( NwamuiWifiNet *self, guint speed );
extern guint                        nwamui_wifi_net_get_speed ( NwamuiWifiNet *self );


extern void                         nwamui_wifi_net_set_mode ( NwamuiWifiNet *self, const gchar* mode );
extern gchar*                       nwamui_wifi_net_get_mode ( NwamuiWifiNet *self );

extern void                         nwamui_wifi_net_set_signal_strength (   NwamuiWifiNet                  *self,
                                                                            nwamui_wifi_signal_strength_t   signal_strength );
                        
extern nwamui_wifi_signal_strength_t nwamui_wifi_net_get_signal_strength (NwamuiWifiNet *self );

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

G_END_DECLS
        
#endif	/* _NWAMUI_WIFI_NET_H */

