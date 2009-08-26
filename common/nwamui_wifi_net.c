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
 * File:   nwamui_wifi_net.c
 *
 */

#include <glib-object.h>
#include <glib/gi18n.h>
#include <strings.h>
#include <stdlib.h>
#include <errno.h>

#include "libnwamui.h"
#include "nwamui_wifi_net.h"
#include <sys/types.h>
#include <libdlwlan.h>
#include <libdllink.h>

static GObjectClass *parent_class = NULL;


struct _NwamuiWifiNetPrivate {
        nwam_known_wlan_handle_t        known_wlan_h;
        gboolean                        is_favourite;
        gboolean                        modified;
        gchar                          *essid;            
        NwamuiNcu                      *ncu;
        nwamui_wifi_security_t          security;
        nwamui_wifi_bss_type_t          bss_type;
        nwamui_wifi_signal_strength_t   signal_strength;
        guint                           channel;
        guint                           speed;
        gchar*                          mode;
        nwamui_wifi_wpa_config_t        wpa_config; /* Only valid if security is WPA */
        guint                           wep_key_index;
        gchar                          *wep_password;
        gchar                          *wpa_username;
        gchar                          *wpa_password;
        gchar                          *wpa_cert_file;
        gint                            status;

        /* For non-favourites store prio and bssid_strv in memory only */
        gchar**                         bssid_strv; /* NULL terminated list of strings */
        guint64                         priority;
};

static void nwamui_wifi_net_set_property (  GObject         *object,
                                            guint            prop_id,
                                            const GValue    *value,
                                            GParamSpec      *pspec);

static void nwamui_wifi_net_get_property (  GObject         *object,
                                            guint            prop_id,
                                            GValue          *value,
                                            GParamSpec      *pspec);

static void nwamui_wifi_net_finalize (      NwamuiWifiNet *self);

static gchar*       get_nwam_known_wlan_string_prop( nwam_known_wlan_handle_t known_wlan, 
                                                     const char* prop_name );

static gboolean     set_nwam_known_wlan_string_prop( nwam_known_wlan_handle_t known_wlan, 
                                                     const char* prop_name, const gchar* str );

static gchar**      get_nwam_known_wlan_string_array_prop(
                                    nwam_known_wlan_handle_t known_wlan, const char* prop_name );

static gboolean     set_nwam_known_wlan_string_array_prop( nwam_known_wlan_handle_t known_wlan, 
                                                           const char* prop_name, char** strs, guint len );

static guint64      get_nwam_known_wlan_uint64_prop( nwam_known_wlan_handle_t known_wlan, 
                                                     const char* prop_name );

static gboolean     set_nwam_known_wlan_uint64_prop( nwam_known_wlan_handle_t known_wlan, 
                                                     const char* prop_name, guint64 value );

static guint64*     get_nwam_known_wlan_uint64_array_prop(
                                    nwam_known_wlan_handle_t known_wlan, const char* prop_name , guint* out_num );

static gboolean     set_nwam_known_wlan_uint64_array_prop( nwam_known_wlan_handle_t known_wlan, const char* prop_name, 
                                                           const guint64 *value_array, guint len );

/* Callbacks */
static void object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);

enum {
        PROP_ESSID = 1,
        PROP_NCU,
        PROP_STATUS,
        PROP_SECURITY,
        PROP_SIGNAL_STRENGTH,
        PROP_CHANNEL,
        PROP_BSS_TYPE,
        PROP_SPEED,
        PROP_MODE,
        PROP_WPA_CONFIG,
        PROP_WEP_KEY_INDEX,
        PROP_WEP_PASSWORD,
        PROP_WPA_USERNAME,
        PROP_WPA_PASSWORD,
        PROP_WPA_CERT_FILE,
        PROP_BSSID_LIST,
        PROP_PRIORITY
};

G_DEFINE_TYPE (NwamuiWifiNet, nwamui_wifi_net, NWAMUI_TYPE_OBJECT)

static void
nwamui_wifi_net_class_init (NwamuiWifiNetClass *klass)
{
    /* Pointer to GObject Part of Class */
    GObjectClass *gobject_class = (GObjectClass*) klass;
    NwamuiObjectClass *nwamuiobject_class = NWAMUI_OBJECT_CLASS(klass);
        
    /* Initialise Static Parent Class pointer */
    parent_class = g_type_class_peek_parent (klass);

    /* Override Some Function Pointers */
    gobject_class->set_property = nwamui_wifi_net_set_property;
    gobject_class->get_property = nwamui_wifi_net_get_property;
    gobject_class->finalize = (void (*)(GObject*)) nwamui_wifi_net_finalize;

    nwamuiobject_class->get_name = (nwamui_object_get_name_func_t)nwamui_wifi_net_get_essid;
    nwamuiobject_class->set_name = (nwamui_object_set_name_func_t)nwamui_wifi_net_set_essid;

    nwamuiobject_class->reload = (nwamui_object_reload_func_t)nwamui_wifi_net_reload;

    /* Create some properties */
    g_object_class_install_property (gobject_class,
                                     PROP_NCU,
                                     g_param_spec_object ("ncu",
                                                          _("NCU that AP is seen on"),
                                                          _("NCU that AP is seen on"),
                                                          NWAMUI_TYPE_NCU,
                                                          G_PARAM_READWRITE));
    
    g_object_class_install_property (gobject_class,
                                     PROP_ESSID,
                                     g_param_spec_string ("essid",
                                                          _("Wifi Network ESSID"),
                                                          _("Wifi Network ESSID"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_STATUS,
                                     g_param_spec_int    ("status",
                                                          _("Wifi Network Status"),
                                                          _("Wifi Network Status"),
                                                          NWAMUI_WIFI_STATUS_CONNECTED, /* Min */
                                                          NWAMUI_WIFI_STATUS_LAST-1, /* Max */
                                                          NWAMUI_WIFI_STATUS_DISCONNECTED, /* Default */
                                                          G_PARAM_READWRITE));
    g_object_class_install_property (gobject_class,
                                     PROP_SECURITY,
                                     g_param_spec_int    ("security",
                                                          _("Wifi Network Security"),
                                                          _("Wifi Network Security"),
                                                          NWAMUI_WIFI_SEC_NONE, /* Min */
                                                          NWAMUI_WIFI_SEC_LAST-1, /* Max */
                                                          NWAMUI_WIFI_SEC_NONE, /* Default */
                                                          G_PARAM_READWRITE));
    g_object_class_install_property (gobject_class,
                                     PROP_CHANNEL,
                                     g_param_spec_int    ("channel",
                                                          _("Wifi Network Channel"),
                                                          _("Wifi Network Channel"),
                                                          0, /* Min */
                                                          G_MAXINT-1, /* Max */
                                                          0, /* Default */
                                                          G_PARAM_READWRITE));    

    g_object_class_install_property (gobject_class,
                                     PROP_SIGNAL_STRENGTH,
                                     g_param_spec_int    ("signal_strength",
                                                          _("Wifi Network Signal Strength"),
                                                          _("Wifi Network Signal Strength"),
                                                          NWAMUI_WIFI_STRENGTH_NONE, /* Min */
                                                          NWAMUI_WIFI_STRENGTH_LAST-1, /* Max */
                                                          NWAMUI_WIFI_STRENGTH_NONE, /* Default */
                                                          G_PARAM_READWRITE));    

    g_object_class_install_property (gobject_class,
                                     PROP_BSS_TYPE,
                                     g_param_spec_int    ("bss_type",
                                                          _("Wifi BSS Type"),
                                                          _("Wifi BSS Type (AP or AD-HOC)"),
                                                          NWAMUI_WIFI_BSS_TYPE_AUTO, /* Min */
                                                          NWAMUI_WIFI_BSS_TYPE_LAST-1, /* Max */
                                                          NWAMUI_WIFI_BSS_TYPE_AUTO, /* Default */
                                                          G_PARAM_READWRITE));    

    g_object_class_install_property (gobject_class,
                                     PROP_SPEED,
                                     g_param_spec_uint ("speed",
                                                       _("speed"),
                                                       _("speed"),
                                                       0,
                                                       G_MAXUINT,
                                                       0,
                                                       G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_MODE,
                                     g_param_spec_string ("mode",
                                                          _("mode"),
                                                          _("mode"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_WPA_CONFIG,
                                     g_param_spec_int    ("wpa_config",
                                                          _("Wifi WPA Configuration Type"),
                                                          _("Wifi WPA Configuration Type"),
                                                          NWAMUI_WIFI_WPA_CONFIG_AUTOMATIC, /* Min */
                                                          NWAMUI_WIFI_WPA_CONFIG_LAST-1, /* Max */
                                                          NWAMUI_WIFI_WPA_CONFIG_AUTOMATIC, /* Default */
                                                          G_PARAM_READWRITE));


    g_object_class_install_property (gobject_class,
                                     PROP_WEP_PASSWORD,
                                     g_param_spec_string ("wep_password",
                                                          _("Wifi WEP Password"),
                                                          _("Wifi WEP Password"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_WEP_KEY_INDEX,
                                     g_param_spec_uint64 ("wep_key_index",
                                                          _("Wep Key Index"),
                                                          _("Wep Key Index"),
                                                          1, /* Min */
                                                          4, /* Max */
                                                          1, /* Default */
                                                          G_PARAM_READWRITE));    

    g_object_class_install_property (gobject_class,
                                     PROP_WPA_USERNAME,
                                     g_param_spec_string ("wpa_username",
                                                          _("Wifi WPA Username"),
                                                          _("Wifi WPA Username"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_WPA_PASSWORD,
                                     g_param_spec_string ("wpa_password",
                                                          _("Wifi WPA Password"),
                                                          _("Wifi WPA Password"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_WPA_CERT_FILE,
                                     g_param_spec_string ("wpa_cert_file",
                                                          _("Wifi WPA Certificate File Path"),
                                                          _("Wifi WPA Certificate File Path"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_BSSID_LIST,
                                     g_param_spec_pointer ("bssid_list",
                                                          _("bssid_list"),
                                                          _("bssid_list"),
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_PRIORITY,
                                     g_param_spec_uint64 ("priority",
                                                       _("priority"),
                                                       _("priority"),
                                                       0,
                                                       G_MAXUINT64,
                                                       0,
                                                       G_PARAM_READWRITE));

}


static void
nwamui_wifi_net_init (NwamuiWifiNet *self)
{
    NwamuiWifiNetPrivate *prv = (NwamuiWifiNetPrivate*)g_new0 (NwamuiWifiNetPrivate, 1);

    prv->modified = FALSE;

    prv->essid = NULL;            
    prv->is_favourite = TRUE; /* Assume it's a favorite by default */
    prv->bssid_strv = NULL;
    prv->priority = 0;
    prv->security = NWAMUI_WIFI_SEC_NONE;
    prv->signal_strength = NWAMUI_WIFI_STRENGTH_NONE;
    prv->bss_type = NWAMUI_WIFI_BSS_TYPE_AUTO;
    prv->speed = 0;
    prv->mode = NULL;
    prv->wpa_config = NWAMUI_WIFI_WPA_CONFIG_AUTOMATIC; 
    prv->wep_key_index = 1;
    prv->wep_password = NULL;
    prv->wpa_username = NULL;
    prv->wpa_password = NULL;
    prv->wpa_cert_file = NULL;
    prv->status = NWAMUI_WIFI_STATUS_DISCONNECTED;
    
    self->prv = prv;
    
    g_signal_connect(G_OBJECT(self), "notify", (GCallback)object_notify_cb, (gpointer)self);
}

static void
nwamui_wifi_net_set_property (  GObject         *object,
                                guint            prop_id,
                                const GValue    *value,
                                GParamSpec      *pspec)
{
    NwamuiWifiNet          *self = NWAMUI_WIFI_NET(object);
    NwamuiWifiNetPrivate   *prv;

    g_return_if_fail( self != NULL );
    
    prv = self->prv;
    g_assert( prv != NULL );
    
    switch (prop_id) {
    case PROP_NCU: {
            NwamuiNcu* ncu = NWAMUI_NCU( g_value_dup_object( value ) );

            if ( ncu != NULL ) {
                g_assert( nwamui_ncu_get_ncu_type(ncu) == NWAMUI_NCU_TYPE_WIRELESS );
            }

            if ( self->prv->ncu != NULL ) {
                    g_object_unref( self->prv->ncu );
            }
            self->prv->ncu = ncu;
        }
        break;
	  case PROP_ESSID: {
                nwam_error_t    nerr;
	            const gchar    *essid = g_value_get_string (value);
                if ( prv->is_favourite ) {
                    if ( essid != NULL ) {
                        if ( prv->essid != NULL ) {
                            g_debug("Renaming Favourite WLAN with name : '%s' to '%s'", prv->essid, essid );

                            if ( (nerr = nwam_known_wlan_set_name(prv->known_wlan_h, essid)) != NWAM_SUCCESS) {
                                g_warning("Error renaming favourite wlan: %s", nwam_strerror(nerr));
                            }
                            else {
                                g_free( prv->essid );
                                prv->essid = g_strdup(essid);
                            }
                        }
                        else {
                            prv->essid = g_strdup(essid);
                            g_debug("Creating Favourite WLAN with NWAM : %s", self->prv->essid );

                            if ( (nerr = nwam_known_wlan_create(prv->essid, &prv->known_wlan_h)) != NWAM_SUCCESS) {
                                if ( nerr == NWAM_ENTITY_EXISTS ) {
                                    nerr = nwam_known_wlan_read(prv->essid, 0, &self->prv->known_wlan_h);
                                    if (nerr != NWAM_SUCCESS) {
                                        g_warning("Error reading existing favourite wlan: %s", nwam_strerror(nerr));
                                        prv->known_wlan_h = NULL;
                                        /* Ensure it's marked as a not a
                                         * favourite since we no longer  have
                                         * a valid handle. */
                                        self->prv->is_favourite = FALSE;

                                    }
                                }
                                else {
                                    g_warning("Error creating favourite wlan: %s", nwam_strerror(nerr));
                                    prv->known_wlan_h = NULL;
                                    /* Ensure it's marked as a not a
                                     * favourite since we no longer  have
                                     * a valid handle. */
                                    self->prv->is_favourite = FALSE;
                                }
                            }
                        }
                    }
                    else {
                        prv->essid = NULL;
                    }
                }
                else {
                    /* Not a favourite, so just store data in structure */
                    g_debug("Changing non-favourite WifiNet essid to %s", essid?essid:"NULL");
                    if ( prv->essid ) {
                        g_free( prv->essid );
                    }
                    prv->essid = g_strdup(essid);
                }
            }
            break;
        case PROP_SECURITY: {
                gint tmpint = g_value_get_int (value);

                g_assert( tmpint >= NWAMUI_WIFI_SEC_NONE && tmpint < NWAMUI_WIFI_SEC_LAST );

                prv->security = (nwamui_wifi_security_t)tmpint;

                if ( prv->is_favourite && prv->known_wlan_h != NULL ) {
                    set_nwam_known_wlan_uint64_prop( self->prv->known_wlan_h, NWAM_KNOWN_WLAN_PROP_SECURITY_MODE, 
                                                     nwamui_wifi_net_security_map_to_nwam( prv->security));
                }
            }
            break;
        case PROP_CHANNEL: {
                prv->channel = g_value_get_int (value); 
            }
            break;

        case PROP_SIGNAL_STRENGTH: {
                gint tmpint = g_value_get_int (value);

                g_assert( tmpint >= NWAMUI_WIFI_STRENGTH_NONE && tmpint < NWAMUI_WIFI_STRENGTH_LAST );

                prv->signal_strength = (nwamui_wifi_signal_strength_t)tmpint;
            }
            break;

        case PROP_BSS_TYPE: {
                gint tmpint = g_value_get_int (value);

                g_assert( tmpint >= NWAMUI_WIFI_BSS_TYPE_AUTO && tmpint < NWAMUI_WIFI_BSS_TYPE_LAST );

                prv->bss_type = (nwamui_wifi_bss_type_t)tmpint;
            }
            break;

        case PROP_SPEED: {
                self->prv->speed = g_value_get_uint( value );
            }
            break;

        case PROP_MODE: {
                if ( self->prv->mode != NULL ) {
                        g_free( self->prv->mode );
                }
                self->prv->mode = g_strdup( g_value_get_string( value ) );
            }
            break;

        case PROP_WPA_CONFIG: {
                gint tmpint = g_value_get_int (value);

                g_assert( tmpint >= NWAMUI_WIFI_WPA_CONFIG_AUTOMATIC && tmpint < NWAMUI_WIFI_WPA_CONFIG_LAST );

                prv->wpa_config = (nwamui_wifi_wpa_config_t)tmpint;
            }
            break;

        case PROP_WEP_KEY_INDEX: {
                prv->wep_key_index = g_value_get_int (value); 
                nwamui_debug("Setting WifiNet keyslot to %ul", prv->wep_key_index);
                if ( prv->is_favourite && prv->known_wlan_h != NULL ) {
                    set_nwam_known_wlan_uint64_prop( self->prv->known_wlan_h, NWAM_KNOWN_WLAN_PROP_KEYSLOT, 
                                                     g_value_get_uint64( value ) );
                }
            }
            break;

        case PROP_WEP_PASSWORD: {
                const gchar *passwd = g_value_get_string (value);
                if ( passwd != NULL ) {
                    if ( prv->wep_password != NULL ) {
                        g_free( prv->wep_password );
                    }
                    prv->wep_password = g_strdup(passwd);
                }
                else {
                    prv->wep_password = NULL;
                }

            }
            break;
        case PROP_WPA_USERNAME: {
		const gchar *username = g_value_get_string (value);
                if ( username != NULL ) {
                    if ( prv->wpa_username != NULL ) {
                        g_free( prv->wpa_username );
                    }
                    prv->wpa_username = g_strdup(username);
                }
                else {
                    prv->wpa_username = NULL;
                }
            }
            break;
        case PROP_WPA_PASSWORD: {
		const gchar *passwd = g_value_get_string (value);
                if ( passwd != NULL ) {
                    if ( prv->wpa_password != NULL ) {
                        g_free( prv->wpa_password );
                    }
                    prv->wpa_password = g_strdup(passwd);
                }
                else {
                    prv->wpa_password = NULL;
                }
            }
            break;
        case PROP_WPA_CERT_FILE: {
		const gchar *fname = g_value_get_string (value);
                if ( fname != NULL ) {
                    if ( prv->wpa_cert_file != NULL ) {
                        g_free( prv->wpa_cert_file );
                    }
                    prv->wpa_cert_file = g_strdup(fname);
                }
            }
            break;

        case PROP_STATUS: {
                self->prv->status = g_value_get_int( value );
            }
            break;

        case PROP_BSSID_LIST: {
                if ( prv->is_favourite ) {
                    if ( self->prv->known_wlan_h != NULL ) {
                        GList *bssid_list = g_value_get_pointer( value );
                        gchar **bssid_strs = nwamui_util_glist_to_strv( bssid_list );

                        set_nwam_known_wlan_string_array_prop( self->prv->known_wlan_h, NWAM_KNOWN_WLAN_PROP_BSSIDS, bssid_strs, 0 );
                        if ( bssid_strs != NULL ) {
                            g_strfreev( bssid_strs );
                        }
                    }
                    else {
                        g_warning("Unexpected empty known_wlan handle");
                    }
                }
                else {
                    /* Not favourite, so store in internal structure */
                    GList *bssid_list = g_value_get_pointer( value );
                    gchar **bssid_strv = nwamui_util_glist_to_strv( bssid_list );

                    g_debug("Setting non-favourite WifiNet bssid_strv");

                    if ( prv->bssid_strv ) {
                        g_strfreev( prv->bssid_strv );
                    }
                    prv->bssid_strv = bssid_strv;
                }
            }
            break;

        case PROP_PRIORITY: {
                if ( prv->is_favourite ) {
                    if ( self->prv->known_wlan_h != NULL ) {
                        set_nwam_known_wlan_uint64_prop( self->prv->known_wlan_h, NWAM_KNOWN_WLAN_PROP_PRIORITY, 
                                                         g_value_get_uint64( value ) );
                    }
                    else {
                        g_warning("Unexpected empty known_wlan handle");
                    }
                }
                /* Not favourite, so store in internal structure */
                prv->priority = g_value_get_uint64( value );
                nwamui_debug("Setting WifiNet priority to %ul", prv->priority);
            }
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }

    self->prv->modified = TRUE;
}

static void
nwamui_wifi_net_get_property (GObject         *object,
                                  guint            prop_id,
                                  GValue          *value,
                                  GParamSpec      *pspec)
{
    NwamuiWifiNet *self = NWAMUI_WIFI_NET(object);
    NwamuiWifiNetPrivate   *prv;

    g_return_if_fail( self != NULL );
    
    prv = self->prv;
    g_assert( prv != NULL );

    switch (prop_id) {
	case PROP_NCU: {
                g_value_set_object( value, self->prv->ncu );
            }
            break;
	case PROP_ESSID: {
                g_value_set_string(value, prv->essid);
            }
            break;
        case PROP_SECURITY: {
                g_value_set_int(value, (gint)prv->security );
            }
            break;
        case PROP_CHANNEL: {
                g_value_set_int(value, (gint)prv->channel );
            }
            break;
        case PROP_SIGNAL_STRENGTH: {
                g_value_set_int(value, (gint)prv->signal_strength );
            }
            break;
        case PROP_BSS_TYPE: {
                g_value_set_int(value, (gint)prv->bss_type );
            }
            break;
        case PROP_SPEED: {
                g_value_set_uint( value, self->prv->speed );
            }
            break;

        case PROP_MODE: {
                g_value_set_string( value, self->prv->mode );
            }
            break;
        case PROP_WPA_CONFIG: {
                g_value_set_int(value, (gint)prv->wpa_config );
            }
            break;
        case PROP_WEP_KEY_INDEX: {
                guint64 rval = 0;

                if ( prv->is_favourite && self->prv->known_wlan_h != NULL ) {
                    rval = get_nwam_known_wlan_uint64_prop( self->prv->known_wlan_h, NWAM_KNOWN_WLAN_PROP_KEYSLOT );
                }
                else {
                    rval = prv->wep_key_index;
                }
                g_value_set_uint64( value, rval );
            }
            break;
        case PROP_WEP_PASSWORD: {
                g_value_set_string(value, prv->wep_password);
            }
            break;
        case PROP_WPA_USERNAME: {
                g_value_set_string(value, prv->wpa_username);
            }
            break;
        case PROP_WPA_PASSWORD: {
                g_value_set_string(value, prv->wpa_password);
            }
            break;
        case PROP_WPA_CERT_FILE: {
                g_value_set_string(value, prv->wpa_cert_file);
            }
            break;
        case PROP_STATUS: {
                g_value_set_int(value, self->prv->status);
            }
            break;
        case PROP_BSSID_LIST: {
                GList  *bssid_list = NULL;

                if ( prv->is_favourite ) {
                    if ( self->prv->known_wlan_h != NULL ) {
                        gchar **bssid_strv = get_nwam_known_wlan_string_array_prop( self->prv->known_wlan_h, 
                                                                                    NWAM_KNOWN_WLAN_PROP_BSSIDS );
                        bssid_list = nwamui_util_strv_to_glist( bssid_strv );

                        g_strfreev( bssid_strv );
                    }
                    else {
                        g_warning("Unexpected empty known_wlan handle");
                    }
                }
                else {
                    bssid_list = nwamui_util_strv_to_glist( prv->bssid_strv );
                }
                g_value_set_pointer( value, bssid_list );
            }
            break;
        case PROP_PRIORITY: {
                guint64 rval = 0;

                if ( prv->is_favourite && self->prv->known_wlan_h != NULL ) {
                    rval = get_nwam_known_wlan_uint64_prop( self->prv->known_wlan_h, NWAM_KNOWN_WLAN_PROP_PRIORITY );
                }
                else {
                    rval = prv->priority;
                }
                g_value_set_uint64( value, rval );
            }
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

/**
 * nwamui_wifi_net_reload:   re-load stored configuration
 **/
extern void
nwamui_wifi_net_reload( NwamuiWifiNet* self )
{
    g_return_if_fail( NWAMUI_IS_WIFI_NET(self) );

    /* nwamui_enm_update_with_handle will cause re-read from configuration */
    g_object_freeze_notify(G_OBJECT(self));

    if ( self->prv->known_wlan_h != NULL ) {
        nwamui_wifi_net_update_with_handle( self, self->prv->known_wlan_h );
    }

    g_object_thaw_notify(G_OBJECT(self));
}

static void
nwamui_wifi_net_finalize (NwamuiWifiNet *self)
{
    g_debug("%s wifi 0x%p essid %s", __func__, self, self->prv->essid ? self->prv->essid:"nil");
    if ( self->prv ) {
        if ( self->prv->essid ) {
            g_free( self->prv->essid );
        }
        if (self->prv->mode != NULL ) {
            g_free( self->prv->mode );
        }

        if ( self->prv->known_wlan_h != NULL ) {
            nwam_known_wlan_free( self->prv->known_wlan_h );
        }

        if ( !self->prv->is_favourite ) {
            if ( self->prv->bssid_strv != NULL ) {
                g_strfreev( self->prv->bssid_strv );
            }
        }
        
        g_free (self->prv); 
    }
    self->prv = NULL;

    parent_class->finalize (G_OBJECT (self));
}

/* Exported Functions */

/**
 * nwamui_wifi_net_new:
 *
 * @essid: a C string containing the ESSID of the link
 * @security: a #nwamui_wifi_security_t
 * @bssid_list: a GList with the BSSIDs of the AP.
 * 
 * @returns: a new #NwamuiWifiNet.
 *
 * Creates a new #NwamuiWifiNet.
 **/
extern  NwamuiWifiNet*
nwamui_wifi_net_new(    NwamuiNcu                       *ncu,
				        const gchar                     *essid, 
                        nwamui_wifi_security_t           security,
                        GList                           *bssid_list,
                        nwamui_wifi_bss_type_t           bss_type )
{
    NwamuiWifiNet*  self = NULL;
    
    self = NWAMUI_WIFI_NET(g_object_new (NWAMUI_TYPE_WIFI_NET, 
                "ncu", ncu,
                "essid", essid,
                NULL));
    
    g_object_set (G_OBJECT (self),
                    "security", security,
                    "bssid_list", bssid_list,
                    "bss_type", bss_type,
                    NULL);
    
    self->prv->modified = TRUE;

    return( self );
}

extern gboolean
nwamui_wifi_net_update_with_handle( NwamuiWifiNet* self, nwam_known_wlan_handle_t  handle )
{
    gchar*                      name = NULL;
    gboolean                    essid_changed = TRUE;
    gchar**                     bssid_strv = NULL;
    nwam_error_t                nerr;
    uint32_t                    sec_mode;
    nwamui_wifi_security_t      security;

    /* Accept NULL to allow for simple re-read from system */
    if ( handle != NULL ) {
        char*   _name = NULL;
        if ( (nerr = nwam_known_wlan_get_name(handle, &_name)) != NWAM_SUCCESS) {
            g_warning("Error getting name of known wlan: %s", nwam_strerror(nerr));
            return( FALSE );
        }
        name = g_strdup( _name );
        free(_name);
    }
    else if ( self->prv->essid != NULL ) {
        name = g_strdup( self->prv->essid );
    }
    else {
        g_warning("Request to update known_wlan without valid handle or name");
        return( FALSE );
    }

    if ( self->prv->known_wlan_h != NULL ) {
        nwam_known_wlan_free( self->prv->known_wlan_h  );
        self->prv->known_wlan_h = NULL;
    }
    nerr = nwam_known_wlan_read (name, 0, &self->prv->known_wlan_h);

    if (nerr != NWAM_SUCCESS) {
        g_error ("nwamui_wifi_net_update_with_handle failed to read nwam_known_wlan handle %s", name );
        g_free(name);
        return (FALSE);
    }

    if (self->prv->essid != NULL && name != NULL) {
        if ( strcmp( self->prv->essid, name ) == 0 ) {
            essid_changed = FALSE;
        }
    }
    else if ( name == NULL ) {
        essid_changed = FALSE;
    }

    if ( essid_changed ) {
        if (self->prv->essid != NULL) { 
            g_free(self->prv->essid);
        }
        self->prv->essid = g_strdup( name );
        g_object_notify(G_OBJECT(self), "essid");
    }

    sec_mode = get_nwam_known_wlan_uint64_prop( self->prv->known_wlan_h, 
                                                NWAM_KNOWN_WLAN_PROP_SECURITY_MODE );

    security = nwamui_wifi_net_security_map ( sec_mode );

    if ( self->prv->security != security ) {
        self->prv->security = security;
        g_object_notify(G_OBJECT(self), "security");
    }

    g_free(name);

    /* Ensure it's marked as a favourite since we now have a valid handle. */
    self->prv->is_favourite = TRUE;
    self->prv->modified = FALSE;

    return(TRUE);
}


extern  NwamuiWifiNet*
nwamui_wifi_net_new_with_handle(    NwamuiNcu                       *ncu,
                                    nwam_known_wlan_handle_t         handle )
{
    NwamuiWifiNet*  self = NULL;
    
    self = NWAMUI_WIFI_NET(g_object_new (NWAMUI_TYPE_WIFI_NET, NULL));

    if ( ! nwamui_wifi_net_update_with_handle( self, handle ) ) {
        g_object_unref(self);
        return( NULL );
    }

    if ( ncu != NULL ) {
        self->prv->ncu = NWAMUI_NCU(g_object_ref(ncu));
    }

    return(self);
}

extern gboolean
nwamui_wifi_net_update_from_wlan_t(     NwamuiWifiNet* self, 
                                        nwam_wlan_t               *wlan )
{
    if ( wlan != NULL ) {
        const gchar*                    essid = wlan->essid;
        nwamui_wifi_security_t          security;
        GList                          *bssid_list;
        nwamui_wifi_bss_type_t          bss_type;
        nwamui_wifi_signal_strength_t   signal_strength;
        guint                           channel;
        guint                           speed;
        
        bssid_list = nwamui_wifi_net_get_bssid_list( self );
        if ( wlan->bssid != NULL ) {
            /* Merge list */
            GList *match = g_list_find_custom( bssid_list, 
                                wlan->bssid, (GCompareFunc)strcmp );
            if ( match == NULL ) {
                bssid_list = g_list_append( bssid_list, 
                            g_strdup(wlan->bssid) );
            }
        }
        security = nwamui_wifi_net_security_map (
                        wlan->security_mode);
        channel = wlan->channel;
        speed = wlan->speed / 2; /* dladm_wlan_speed_t needs to be div by 2 */
        bss_type = nwamui_wifi_net_bss_type_map( wlan->bsstype );

        signal_strength = nwamui_wifi_net_strength_map(wlan->signal_strength);

        if ( self != NULL ) {
            g_object_set (G_OBJECT (self),
                            "essid", essid,
                            "security", security,
                            "bssid_list", bssid_list,
                            "bss_type", bss_type,
                            "security", security,
                            "channel", channel,
                            "speed", speed,
                            "signal_strength", signal_strength,
                            NULL);
            
            /* Not modified by user */
            self->prv->modified = FALSE;
        }
        return( TRUE );
    }

    return( FALSE );
}

/**
 * nwamui_wifi_net_new_from_wlan_t:
 *
 * @returns: a new #NwamuiWifiNet.
 *
 * Creates a new #NwamuiWifiNet from a nwam_wlan_t
 **/
extern  NwamuiWifiNet*
nwamui_wifi_net_new_from_wlan_t(    NwamuiNcu                       *ncu,
                                    nwam_wlan_t               *wlan )
{
    NwamuiWifiNet*  self = NULL;
    
    self = NWAMUI_WIFI_NET(g_object_new (NWAMUI_TYPE_WIFI_NET, NULL));

    /* Mark this as not being a Favourite so that it will never be committed,
     * and uses in-memory storage.
     */
    self->prv->is_favourite = FALSE;

    if ( ! nwamui_wifi_net_update_from_wlan_t( self, wlan ) ) {
        g_object_unref(self);
        return( NULL );
    }

    if ( ncu != NULL ) {
        self->prv->ncu = NWAMUI_NCU(g_object_ref(ncu));
    }

    return(self);
}


/** 
 * Compare WifiNet objects, returns values like strcmp().
 */
extern gint
nwamui_wifi_net_compare( NwamuiWifiNet *self, NwamuiWifiNet *other )
{
    gint rval = -1;

    if ( !( NWAMUI_IS_WIFI_NET(self) && NWAMUI_IS_WIFI_NET(other) ) ) {
        return( rval );
    }

    if ( self->prv->essid == NULL && other->prv->essid == NULL ) {
        rval = 0;
    }
    else if ( self->prv->essid == NULL || other->prv->essid == NULL ) {
        rval = self->prv->essid == NULL ? -1:1;
    }
    else if ( ( rval = strcmp(self->prv->essid, other->prv->essid)) == 0 ) {
#ifdef _CARE_FOR_BSSID
        /* Now need to look at BSSID List too if we've no match so far */
        if ( self_bssid_list == NULL && other_bssid_list == NULL ) {
            /* Use rval from previous strcmp */
        }
        else {
            if ( self_bssid_list != NULL ) {
                /* Look for a match between other bssid and an entry in the
                 * own bssid_list */
                GList *match = g_list_find_custom( self_bssid_list, other->prv->bssid, (GCompareFunc)strcmp );
                if ( match != NULL ) {
                    rval = 0;
                }
            }
            if ( rval != 0 && other_bssid_list != NULL ) {
                /* Look for a match between own bssid and an entry in the
                 * other bssid_list */
                GList *match = g_list_find_custom( other_bssid_list, self->prv->bssid, (GCompareFunc)strcmp );
                if ( match != NULL ) {
                    rval = 0;
                }
            }
        }
#else
        /* Use rval from strcmp of essid above */
#endif /* _CARE_FOR_BSSID */
    }

    return( rval );
}

/**
 * Ask NWAM to store the password information
 **/
extern void
nwamui_wifi_net_store_key ( NwamuiWifiNet *self )
{
    if ( self == NULL ) {
        return;
    }

    switch ( self->prv->security ) {
        case NWAMUI_WIFI_SEC_NONE: 
            break;

#ifdef WEP_ASCII_EQ_HEX 
        case NWAMUI_WIFI_SEC_WEP:
#else
        case NWAMUI_WIFI_SEC_WEP_HEX:
        case NWAMUI_WIFI_SEC_WEP_ASCII:
#endif /* WEP_ASCII_EQ_HEX */
        case NWAMUI_WIFI_SEC_WPA_PERSONAL: {
                if ( self->prv->ncu && self->prv->essid ) {
                    gchar          *device = nwamui_ncu_get_device_name(self->prv->ncu);
                    nwam_error_t    nerr;

                    nerr = nwam_wlan_set_key( device,
                                              self->prv->essid,
                                              NULL,
                                              nwamui_wifi_net_security_map_to_nwam( self->prv->security),
                                              self->prv->wep_key_index,
                                              self->prv->wep_password?self->prv->wep_password:"");

                    if ( nerr != NWAM_SUCCESS ) {
                        NwamuiDaemon*   daemon = nwamui_daemon_get_instance();

                        g_warning("Error saving network key NWAM : %s", nwam_strerror(nerr));
                        nwamui_daemon_emit_info_message(daemon, _("Failed to store network key."));

                        g_object_unref(daemon);
                    }
                    
                    g_free(device);
                }
            }
            break;
#if 0
        /* Currently ENTERPRISE is not supported */
        case NWAMUI_WIFI_SEC_WPA_ENTERPRISE:
            nwamui_wifi_wpa_config_t wpa_config_type = nwamui_wifi_net_get_wpa_config(wifi_net);

            /* FIXME: Make this work */
            switch( wpa_config_type ) {
                case NWAMUI_WIFI_WPA_CONFIG_AUTOMATIC:
                    break;
                case NWAMUI_WIFI_WPA_CONFIG_LEAP:
                    break;
                case NWAMUI_WIFI_WPA_CONFIG_PEAP:
                    break;
            }
            break;
#endif
        default:
            break;
    }
}


/**
 * Ask NWAM to connect to this network.
 **/
extern void
nwamui_wifi_net_connect ( NwamuiWifiNet *self, gboolean add_to_favourites  )
{
    gchar          *device = nwamui_ncu_get_device_name(self->prv->ncu);
    nwam_error_t    nerr;
    uint32_t        sec_mode; /* maps to dladm_wlan_secmode_t */
    const gchar*    sec_mode_str;

    sec_mode = nwamui_wifi_net_security_map_to_nwam( self->prv->security);

    switch( sec_mode ) {
        case DLADM_WLAN_SECMODE_NONE:
            sec_mode_str = "None";
            break;
        case DLADM_WLAN_SECMODE_WEP:
            sec_mode_str = "WEP";
            break;
        case DLADM_WLAN_SECMODE_WPA:
            sec_mode_str = "WPA";
            break;
        default:
            sec_mode_str = "???";
    }

    nwamui_warning("nwam_wlan_select( %s, %s, NULL, %s[%d], %s[%d])", 
                    device, self->prv->essid, 
                    sec_mode_str, sec_mode,
                    add_to_favourites?"TRUE":"FALSE",
                    add_to_favourites?B_TRUE:B_FALSE );

    if ( (nerr = nwam_wlan_select( device, self->prv->essid, NULL, sec_mode,
                    add_to_favourites?B_TRUE:B_FALSE )) != NWAM_SUCCESS ) {
        NwamuiDaemon*   daemon = nwamui_daemon_get_instance();

        g_warning("Error selecting network with NWAM : %s", nwam_strerror(nerr));
        nwamui_daemon_emit_info_message(daemon, _("Failed to initiate connection to wireless network."));

        g_object_unref(daemon);
    }
    
    g_free(device);
}

/**
 * Ask NWAM to delete this favourite network.
 **/
extern gboolean
nwamui_wifi_net_delete_favourite ( NwamuiWifiNet *self )
{
    nwam_error_t                nerr;
    nwam_known_wlan_handle_t    wlan_h;

    if ( (nerr = nwam_known_wlan_destroy(self->prv->known_wlan_h, 0)) != NWAM_SUCCESS) {
        g_debug("Destroy error when destroying: %s", nwam_strerror(nerr));
        return(FALSE);;
    }

    self->prv->known_wlan_h = NULL;

    /* Ensure it's marked as a not a favourite since we no longer  have a
     * valid handle. */
    self->prv->is_favourite = FALSE;

    return(TRUE);
}

extern gboolean
nwamui_wifi_net_validate_favourite ( NwamuiWifiNet *self, gchar**error_prop )
{
    nwam_error_t                nerr;
    gboolean                    rval = FALSE;

    g_return_val_if_fail( self != NULL, rval );

    const char* errprop = NULL;
    if ( self->prv->is_favourite && 
         (nerr = nwam_known_wlan_validate(self->prv->known_wlan_h, &errprop)) != NWAM_SUCCESS ) {
        g_debug("wlan has a validation error with prop %s : error: %s", errprop?errprop:"NULL", nwam_strerror(nerr));
        if ( error_prop != NULL ) {
            *error_prop = g_strdup( errprop );
        }
        rval = FALSE;
    }
    else {
        if ( error_prop != NULL ) {
            *error_prop = NULL;
        }
        rval = TRUE;
    }

    return( rval );
}

/**
 * Ask NWAM to connect to this network.
 **/
extern gboolean
nwamui_wifi_net_commit_favourite ( NwamuiWifiNet *self )
{
    nwam_error_t                nerr;
    nwam_known_wlan_handle_t    wlan_h;
    gchar**                     bssid_strs = NULL;

    g_return_val_if_fail( self != NULL, FALSE );

    g_debug("Committing Favourite WLAN with NWAM : %s", self->prv->essid );

    if ( self->prv->is_favourite && 
         (nerr = nwam_known_wlan_commit(self->prv->known_wlan_h, 0) ) != NWAM_SUCCESS ) {
		if (nerr == NWAM_ENTITY_MISSING_MEMBER) {
            const char* errprop = NULL;
			nwam_known_wlan_validate(self->prv->known_wlan_h, &errprop);
            g_warning("Couldn't commit wlan due to a validation error with prop %s\n", errprop?errprop:"NULL");
            return( FALSE );
        }
    }

    return( TRUE );
}

/* Get/Set NCU */
extern void
nwamui_wifi_net_set_ncu ( NwamuiWifiNet *self, NwamuiNcu* ncu )
{
    g_return_if_fail (NWAMUI_IS_WIFI_NET(self));

    if ( ncu != NULL ) {

        g_assert ( NWAMUI_IS_NCU( ncu ));

        g_object_set (G_OBJECT (self),
                      "ncu", ncu,
                      NULL);
    }
}
                                
extern NwamuiNcu*
nwamui_wifi_net_get_ncu ( NwamuiWifiNet *self )
{
    NwamuiNcu   *ncu = NULL;
    
    g_return_val_if_fail (NWAMUI_IS_WIFI_NET (self), ncu);

    g_object_get (G_OBJECT (self),
                  "ncu", &ncu,
                  NULL);
    
    return( ncu );
}

/* Get/Set ESSID */
void                    
nwamui_wifi_net_set_essid ( NwamuiWifiNet  *self,
                            const gchar    *essid )
{
    g_return_if_fail (NWAMUI_IS_WIFI_NET(self));
    g_assert (essid != NULL );

    if ( essid != NULL ) {
        g_object_set (G_OBJECT (self),
                      "essid", essid,
                      NULL);
    }
}
                                
gchar*                  
nwamui_wifi_net_get_essid (NwamuiWifiNet *self )
{
    gchar*  ret_str = NULL;
    
    g_return_val_if_fail (NWAMUI_IS_WIFI_NET (self), ret_str);

    g_object_get (G_OBJECT (self),
                  "essid", &ret_str,
                  NULL);
    
    return( ret_str );
}

gchar*                  
nwamui_wifi_net_get_display_string (NwamuiWifiNet *self, gboolean has_many_wireless )
{
    gchar*  ret_str = NULL;
    gchar*  name = NULL;
    gchar*  dev_name = NULL;
    GString* gstr = g_string_new("");
    
    g_return_val_if_fail (NWAMUI_IS_WIFI_NET (self), ret_str);
    
    if ( self->prv->ncu && has_many_wireless ) {
        dev_name = nwamui_ncu_get_device_name( self->prv->ncu );

        g_string_append_printf(gstr, _("[%s] "), dev_name );
    }

    name = self->prv->essid?self->prv->essid:"";

    g_string_append_printf(gstr, _("%s"), name );

    if ( self->prv->bss_type == NWAMUI_WIFI_BSS_TYPE_ADHOC ) {
        g_string_append_printf(gstr, _(" (Computer-to-Computer)") );
    }

    ret_str = g_string_free(gstr, FALSE);

    return( ret_str );
}

extern gchar*
nwamui_wifi_net_get_unique_name ( NwamuiWifiNet *self )
{
    gchar*  ret_str = NULL;
    gchar*  essid = NULL;
    gchar*  device_name = NULL;
    
    g_return_val_if_fail (NWAMUI_IS_WIFI_NET (self), ret_str);

    device_name = nwamui_ncu_get_device_name(self->prv->ncu);

    g_object_get (G_OBJECT (self),
                  "essid", &essid,
                  NULL);

    /* "%s - %s", so it's different to FALSEWLAN */
    ret_str = g_strconcat(device_name?device_name:"", " - ", essid ? essid : "", NULL);

    g_free(device_name);
    g_free(essid);

    return( ret_str );
}

extern void
nwamui_wifi_net_set_status ( NwamuiWifiNet *self, nwamui_wifi_status_t status )
{
    g_return_if_fail (NWAMUI_IS_WIFI_NET(self));
    g_assert (status >= NWAMUI_WIFI_STATUS_CONNECTED && status < NWAMUI_WIFI_STATUS_LAST );

    g_object_set (G_OBJECT (self),
                  "status", status,
                  NULL);
}
          
extern nwamui_wifi_status_t
nwamui_wifi_net_get_status ( NwamuiWifiNet *self )
{
    gint    retval = NWAMUI_WIFI_STATUS_CONNECTED;
    
    g_return_val_if_fail (NWAMUI_IS_WIFI_NET (self), (nwamui_wifi_status_t)retval);

    g_object_get (G_OBJECT (self),
                  "status", &retval,
                  NULL);
    
    return( (nwamui_wifi_status_t)retval );
}

/* Get/Set Security Type */
extern void                    
nwamui_wifi_net_set_security ( NwamuiWifiNet           *self,
                               nwamui_wifi_security_t   security )
{
    g_return_if_fail (NWAMUI_IS_WIFI_NET(self));
    g_assert (security >= NWAMUI_WIFI_SEC_NONE && security < NWAMUI_WIFI_SEC_LAST );

    g_object_set (G_OBJECT (self),
                  "security", security,
                  NULL);
}
                                
extern nwamui_wifi_security_t           
nwamui_wifi_net_get_security (NwamuiWifiNet *self )
{
    gint    retval = NWAMUI_WIFI_SEC_NONE;
    
    g_return_val_if_fail (NWAMUI_IS_WIFI_NET (self), (nwamui_wifi_security_t)retval);

    g_object_get (G_OBJECT (self),
                  "security", &retval,
                  NULL);
    
    return( (nwamui_wifi_security_t)retval );
}

/* Get/Set Channel */
extern void                    
nwamui_wifi_net_set_channel (   NwamuiWifiNet      *self,
                                gint                channel )
{
    g_return_if_fail (NWAMUI_IS_WIFI_NET(self));

    g_object_set (G_OBJECT (self),
                  "channel", channel,
                  NULL);
}
                                
extern gint
nwamui_wifi_net_get_channel (NwamuiWifiNet *self )
{
    gint    retval = 0;
    
    g_return_val_if_fail (NWAMUI_IS_WIFI_NET (self), retval);

    g_object_get (G_OBJECT (self),
                  "channel", &retval,
                  NULL);
    
    return( retval );
}

/* Get/Set Signal Strength */
extern void                    
nwamui_wifi_net_set_signal_strength (   NwamuiWifiNet                  *self,
                                        nwamui_wifi_signal_strength_t   signal_strength )
{
    g_return_if_fail (NWAMUI_IS_WIFI_NET(self));
    g_assert (signal_strength >= NWAMUI_WIFI_STRENGTH_NONE && signal_strength < NWAMUI_WIFI_STRENGTH_LAST );

    if ( signal_strength != self->prv->signal_strength ) {
        g_object_set (G_OBJECT (self),
                      "signal_strength", signal_strength,
                      NULL);
    }
}
                                
extern nwamui_wifi_signal_strength_t          
nwamui_wifi_net_get_signal_strength (NwamuiWifiNet *self )
{
    gint    retval = NWAMUI_WIFI_SEC_NONE;
    
    g_return_val_if_fail (NWAMUI_IS_WIFI_NET (self), (nwamui_wifi_signal_strength_t)retval);

    g_object_get (G_OBJECT (self),
                  "signal_strength", &retval,
                  NULL);
    
    return( (nwamui_wifi_signal_strength_t)retval );
}

extern const gchar*
nwamui_wifi_net_convert_strength_to_string( nwamui_wifi_signal_strength_t strength ) 
{
    const gchar* signal_str = NULL;

    switch ( strength ) {
        case NWAMUI_WIFI_STRENGTH_VERY_WEAK:
            signal_str = _("Very Weak");
            break;
        case NWAMUI_WIFI_STRENGTH_WEAK:
            signal_str = _("Weak");
            break;
        case NWAMUI_WIFI_STRENGTH_GOOD:
            signal_str = _("Good");
            break;
        case NWAMUI_WIFI_STRENGTH_VERY_GOOD:
            signal_str = _("Very Good");
            break;
        case NWAMUI_WIFI_STRENGTH_EXCELLENT:
            signal_str = _("Excellent");
            break;
        case NWAMUI_WIFI_STRENGTH_NONE:
        default:
            signal_str = _("No Signal");
            break;
    }

    return( signal_str );
}

extern const gchar*
nwamui_wifi_net_get_signal_strength_string(NwamuiWifiNet *self )
{
    gint    signal = NWAMUI_WIFI_SEC_NONE;
    
    if (NWAMUI_IS_WIFI_NET (self)) {
        g_object_get (G_OBJECT (self),
                      "signal_strength", &signal,
                      NULL);
    }
    
    return ( nwamui_wifi_net_convert_strength_to_string( signal ) );
}

extern nwamui_wifi_bss_type_t
nwamui_wifi_net_get_bss_type (NwamuiWifiNet *self )
{
    gint    retval = NWAMUI_WIFI_BSS_TYPE_AUTO;
    
    g_return_val_if_fail (NWAMUI_IS_WIFI_NET (self), (nwamui_wifi_bss_type_t)retval);

    g_object_get (G_OBJECT (self),
                  "bss_type", &retval,
                  NULL);
    
    return( (nwamui_wifi_bss_type_t)retval );
}

/** 
 * nwamui_wifi_net_set_speed:
 * @nwamui_wifi_net: a #NwamuiWifiNet.
 * @speed: Value to set speed to.
 * 
 **/ 
extern void
nwamui_wifi_net_set_speed (   NwamuiWifiNet *self,
                              guint        speed )
{
    g_return_if_fail (NWAMUI_IS_WIFI_NET (self));
    g_assert (speed >= 0 && speed <= G_MAXUINT );

    g_object_set (G_OBJECT (self),
                  "speed", speed,
                  NULL);
}

/**
 * nwamui_wifi_net_get_speed:
 * @nwamui_wifi_net: a #NwamuiWifiNet.
 * @returns: the speed.
 *
 **/
extern guint
nwamui_wifi_net_get_speed (NwamuiWifiNet *self)
{
    guint  speed = 0; 

    g_return_val_if_fail (NWAMUI_IS_WIFI_NET (self), speed);

    g_object_get (G_OBJECT (self),
                  "speed", &speed,
                  NULL);

    return( speed );
}

/** 
 * nwamui_wifi_net_set_mode:
 * @nwamui_wifi_net: a #NwamuiWifiNet.
 * @mode: Value to set mode to.
 * 
 **/ 
extern void
nwamui_wifi_net_set_mode (   NwamuiWifiNet *self,
                              const gchar*  mode )
{
    g_return_if_fail (NWAMUI_IS_WIFI_NET (self));
    g_assert (mode != NULL );

    if ( mode != NULL ) {
        g_object_set (G_OBJECT (self),
                      "mode", mode,
                      NULL);
    }
}

/**
 * nwamui_wifi_net_get_mode:
 * @nwamui_wifi_net: a #NwamuiWifiNet.
 * @returns: the mode.
 *
 **/
extern gchar*
nwamui_wifi_net_get_mode (NwamuiWifiNet *self)
{
    gchar*  mode = NULL; 

    g_return_val_if_fail (NWAMUI_IS_WIFI_NET (self), mode);

    g_object_get (G_OBJECT (self),
                  "mode", &mode,
                  NULL);

    return( mode );
}

/* Get/Set WPA Configuration Type */
extern void                    
nwamui_wifi_net_set_wpa_config (    NwamuiWifiNet           *self,
                                    nwamui_wifi_wpa_config_t wpa_config )
{
    g_return_if_fail (NWAMUI_IS_WIFI_NET(self));
    g_assert (wpa_config >= NWAMUI_WIFI_WPA_CONFIG_AUTOMATIC && wpa_config < NWAMUI_WIFI_WPA_CONFIG_LAST );

    g_object_set (G_OBJECT (self),
                  "wpa_config", wpa_config,
                  NULL);
}
                                
extern nwamui_wifi_wpa_config_t
nwamui_wifi_net_get_wpa_config (NwamuiWifiNet *self )
{
    gint    retval = NWAMUI_WIFI_SEC_NONE;
    
    g_return_val_if_fail (NWAMUI_IS_WIFI_NET (self), (nwamui_wifi_wpa_config_t)retval);

    g_object_get (G_OBJECT (self),
                  "wpa_config", &retval,
                  NULL);
    
    return( (nwamui_wifi_wpa_config_t)retval );
}

/* Get/Set WEP Password/Key */
extern void                    
nwamui_wifi_net_set_wep_password (  NwamuiWifiNet  *self,
                                    const gchar    *wep_password )
{
    g_return_if_fail (NWAMUI_IS_WIFI_NET(self));
    g_assert (wep_password != NULL );

    if ( wep_password != NULL ) {
        g_object_set (G_OBJECT (self),
                      "wep_password", wep_password,
                      NULL);
    }
}
                                
extern gchar*                  
nwamui_wifi_net_get_wep_password (NwamuiWifiNet *self )
{
    gchar*  ret_str = NULL;
    
    g_return_val_if_fail (NWAMUI_IS_WIFI_NET (self), ret_str);

    g_object_get (G_OBJECT (self),
                  "wep_password", &ret_str,
                  NULL);
    
    return( ret_str );
}

/* Get/Set WEP Password/Key Index (1-4)*/
extern void                    
nwamui_wifi_net_set_wep_key_index (  NwamuiWifiNet  *self,
                                    guint           wep_key_index )
{
    g_return_if_fail (NWAMUI_IS_WIFI_NET(self));
    g_return_if_fail ( wep_key_index >= 1 && wep_key_index <= 4 );

    g_object_set (G_OBJECT (self),
                  "wep_key_index", wep_key_index,
                  NULL);
}
                                
extern guint
nwamui_wifi_net_get_wep_key_index (NwamuiWifiNet *self )
{
    guint  rval = 0;
    
    g_return_val_if_fail (NWAMUI_IS_WIFI_NET (self), rval);

    g_object_get (G_OBJECT (self),
                  "wep_key_index", &rval,
                  NULL);
    
    return( rval );
}

/* Get/Set WPA Username */
extern void                    
nwamui_wifi_net_set_wpa_username (  NwamuiWifiNet  *self,
                                    const gchar    *wpa_username )
{
    g_return_if_fail (NWAMUI_IS_WIFI_NET(self));
    g_assert (wpa_username != NULL );

    if ( wpa_username != NULL ) {
        g_object_set (G_OBJECT (self),
                      "wpa_username", wpa_username,
                      NULL);
    }
}
                                
extern gchar*                  
nwamui_wifi_net_get_wpa_username (NwamuiWifiNet *self )
{
    gchar*  ret_str = NULL;
    
    g_return_val_if_fail (NWAMUI_IS_WIFI_NET (self), ret_str);

    g_object_get (G_OBJECT (self),
                  "wpa_username", &ret_str,
                  NULL);
    
    return( ret_str );
}

/* Get/Set WPA Password */
extern void                    
nwamui_wifi_net_set_wpa_password ( NwamuiWifiNet  *self,
                                   const gchar    *wpa_password )
{
    g_return_if_fail (NWAMUI_IS_WIFI_NET(self));
    g_assert (wpa_password != NULL );

    if ( wpa_password != NULL ) {
        g_object_set (G_OBJECT (self),
                      "wpa_password", wpa_password,
                      NULL);
    }
}
                                
extern gchar*                  
nwamui_wifi_net_get_wpa_password (NwamuiWifiNet *self )
{
    gchar*  ret_str = NULL;
    
    g_return_val_if_fail (NWAMUI_IS_WIFI_NET (self), ret_str);

    g_object_get (G_OBJECT (self),
                  "wpa_password", &ret_str,
                  NULL);
    
    return( ret_str );
}

/* Get/Set WPA Certificate File */
extern void                    
nwamui_wifi_net_set_wpa_cert_file ( NwamuiWifiNet  *self,
                                    const gchar    *wpa_cert_file )
{
    g_return_if_fail (NWAMUI_IS_WIFI_NET(self));
    g_assert (wpa_cert_file != NULL );

    if ( wpa_cert_file != NULL ) {
        g_object_set (G_OBJECT (self),
                      "wpa_cert_file", wpa_cert_file,
                      NULL);
    }
}
                                
extern gchar*                  
nwamui_wifi_net_get_wpa_cert_file (NwamuiWifiNet *self )
{
    gchar*  ret_str = NULL;
    
    g_return_val_if_fail (NWAMUI_IS_WIFI_NET (self), ret_str);

    g_object_get (G_OBJECT (self),
                  "wpa_cert_file", &ret_str,
                  NULL);
    
    return( ret_str );
}

/** 
 * nwamui_wifi_net_set_bssid_list:
 * @nwamui_wifi_net: a #NwamuiWifiNet.
 * @bssid_list: Value to set bssid_list to.
 * 
 **/ 
extern void
nwamui_wifi_net_set_bssid_list (   NwamuiWifiNet *self,
                              const GList*    bssid_list )
{
    g_return_if_fail (NWAMUI_IS_WIFI_NET (self));
    g_assert (bssid_list != NULL );

    if ( bssid_list != NULL ) {
        g_object_set (G_OBJECT (self),
                      "bssid_list", bssid_list,
                      NULL);
    }
}

/**
 * nwamui_wifi_net_get_bssid_list:
 * @nwamui_wifi_net: a #NwamuiWifiNet.
 * @returns: the bssid_list.
 *
 **/
extern GList*  
nwamui_wifi_net_get_bssid_list (NwamuiWifiNet *self)
{
    GList*    bssid_list = NULL; 

    g_return_val_if_fail (NWAMUI_IS_WIFI_NET (self), bssid_list);

    g_object_get (G_OBJECT (self),
                  "bssid_list", &bssid_list,
                  NULL);

    return( bssid_list );
}

/** 
 * nwamui_wifi_net_set_priority:
 * @nwamui_wifi_net: a #NwamuiWifiNet.
 * @priority: Value to set priority to.
 * 
 **/ 
extern void
nwamui_wifi_net_set_priority (   NwamuiWifiNet *self,
                              guint64        priority )
{
    g_return_if_fail (NWAMUI_IS_WIFI_NET (self));

    g_object_set (G_OBJECT (self),
                  "priority", priority,
                  NULL);
}

/**
 * nwamui_wifi_net_get_priority:
 * @nwamui_wifi_net: a #NwamuiWifiNet.
 * @returns: the priority.
 *
 **/
extern guint64
nwamui_wifi_net_get_priority (NwamuiWifiNet *self)
{
    guint64  priority = 0; 

    g_return_val_if_fail (NWAMUI_IS_WIFI_NET (self), priority);

    g_object_get (G_OBJECT (self),
                  "priority", &priority,
                  NULL);

    return( priority );
}

extern uint32_t 
nwamui_wifi_net_security_map_to_nwam ( nwamui_wifi_security_t sec_mode )
{
    switch (sec_mode ) {
#ifdef WEP_ASCII_EQ_HEX 
        case NWAMUI_WIFI_SEC_WEP:
#else
        case NWAMUI_WIFI_SEC_WEP_ASCII:
        case NWAMUI_WIFI_SEC_WEP_HEX:
#endif /* WEP_ASCII_EQ_HEX */
            return (uint32_t)DLADM_WLAN_SECMODE_WEP;
        case NWAMUI_WIFI_SEC_WPA_PERSONAL:
            return (uint32_t)DLADM_WLAN_SECMODE_WPA;
        default:
            return (uint32_t)DLADM_WLAN_SECMODE_NONE;
    }
}

extern nwamui_wifi_security_t
nwamui_wifi_net_security_map ( uint32_t _sec_mode )
{
    /* Is really a dladm_wlan_secmode_t */
    dladm_wlan_secmode_t sec_mode = (dladm_wlan_secmode_t)_sec_mode;
    
    switch (sec_mode ) {
        case DLADM_WLAN_SECMODE_WEP:
#ifdef WEP_ASCII_EQ_HEX 
            return NWAMUI_WIFI_SEC_WEP;
#else
            return NWAMUI_WIFI_SEC_WEP_HEX;
#endif /* WEP_ASCII_EQ_HEX */
        case DLADM_WLAN_SECMODE_WPA:
            return NWAMUI_WIFI_SEC_WPA_PERSONAL;
        default:
            return NWAMUI_WIFI_SEC_NONE;
    }
}

extern nwamui_wifi_signal_strength_t
nwamui_wifi_net_strength_map (const gchar *strength)
{
    if ( g_ascii_strcasecmp( strength, "very weak" ) == 0 ) {
        return NWAMUI_WIFI_STRENGTH_VERY_WEAK;
    }
    else if ( g_ascii_strcasecmp( strength, "weak" ) == 0 ) {
        return NWAMUI_WIFI_STRENGTH_WEAK;
    }
    else if ( g_ascii_strcasecmp( strength, "good" ) == 0 ) {
        return NWAMUI_WIFI_STRENGTH_GOOD;
    }
    else if ( g_ascii_strcasecmp( strength, "very good" ) == 0 ) {
        return NWAMUI_WIFI_STRENGTH_VERY_GOOD;
    }
    else if ( g_ascii_strcasecmp( strength, "excellent" ) == 0 ) {
        return NWAMUI_WIFI_STRENGTH_EXCELLENT;
    }
    else {
        return NWAMUI_WIFI_STRENGTH_NONE;
    }
}

extern nwamui_wifi_bss_type_t
nwamui_wifi_net_bss_type_map( uint32_t _bsstype )
{
    /* Is really a dladm_wlan_secmode_t */
    dladm_wlan_bsstype_t bsstype = (dladm_wlan_bsstype_t)_bsstype;
    switch( bsstype ) {
        case DLADM_WLAN_BSSTYPE_BSS:
            return NWAMUI_WIFI_BSS_TYPE_AP;
        case DLADM_WLAN_BSSTYPE_IBSS:
            return NWAMUI_WIFI_BSS_TYPE_ADHOC;
        case DLADM_WLAN_BSSTYPE_ANY:
        default:
            return NWAMUI_WIFI_BSS_TYPE_AUTO;
    }
}

static gchar*
get_nwam_known_wlan_string_prop( nwam_known_wlan_handle_t known_wlan, const char* prop_name )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    gchar*              retval = NULL;
    char*               value = NULL;

    g_return_val_if_fail( prop_name != NULL, retval );

    if ( (nerr = nwam_known_wlan_get_prop_type( prop_name, &nwam_type ) ) != NWAM_SUCCESS 
         || nwam_type != NWAM_VALUE_TYPE_STRING ) {
        g_warning("Unexpected type for known_wlan property %s\n", prop_name );
        return retval;
    }

    if ( (nerr = nwam_known_wlan_get_prop_value (known_wlan, prop_name, &nwam_data)) != NWAM_SUCCESS ) {
        g_debug("No value for known_wlan property %s, error = %s", prop_name, nwam_strerror( nerr ) );
        return retval;
    }

    if ( (nerr = nwam_value_get_string(nwam_data, &value )) != NWAM_SUCCESS ) {
        g_debug("Unable to get string value for known_wlan property %s, error = %s", prop_name, nwam_strerror( nerr ) );
        return retval;
    }

    if ( value != NULL ) {
        retval  = g_strdup ( value );
    }

    nwam_value_free(nwam_data);
    
    return( retval );
}

static gboolean
set_nwam_known_wlan_string_prop( nwam_known_wlan_handle_t known_wlan, const char* prop_name, const gchar* str )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    gboolean            retval = FALSE;
    g_return_val_if_fail( prop_name != NULL, retval );

    if ( known_wlan == NULL ) {
        return( retval );
    }

    if ( (nerr = nwam_known_wlan_get_prop_type( prop_name, &nwam_type ) ) != NWAM_SUCCESS 
         || nwam_type != NWAM_VALUE_TYPE_STRING ) {
        g_warning("Unexpected type for known_wlan property %s - got %d\n", prop_name, nwam_type );
        return retval;
    }

    if ( (nerr = nwam_value_create_string( (char*)str, &nwam_data )) != NWAM_SUCCESS ) {
        g_debug("Error creating a string value for string %s", str?str:"NULL");
        return retval;
    }

    if ( (nerr = nwam_known_wlan_set_prop_value (known_wlan, prop_name, nwam_data)) != NWAM_SUCCESS ) {
        g_debug("Unable to set value for known_wlan property %s, error = %s", prop_name, nwam_strerror( nerr ) );
    }
    else {
        retval = TRUE;
    }

    nwam_value_free(nwam_data);
    
    return( retval );
}

static gchar**
get_nwam_known_wlan_string_array_prop( nwam_known_wlan_handle_t known_wlan, const char* prop_name )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    gchar**             retval = NULL;
    char**              value = NULL;
    uint_t              num = 0;

    g_return_val_if_fail( prop_name != NULL, retval );

    if ( (nerr = nwam_known_wlan_get_prop_type( prop_name, &nwam_type ) ) != NWAM_SUCCESS 
         || nwam_type != NWAM_VALUE_TYPE_STRING ) {
        g_warning("Unexpected type for known_wlan property %s\n", prop_name );
        return retval;
    }

    if ( (nerr = nwam_known_wlan_get_prop_value (known_wlan, prop_name, &nwam_data)) != NWAM_SUCCESS ) {
        g_debug("No value for known_wlan property %s, error = %s", prop_name, nwam_strerror( nerr ) );
        return retval;
    }

    if ( (nerr = nwam_value_get_string_array(nwam_data, &value, &num )) != NWAM_SUCCESS ) {
        g_debug("Unable to get string value for known_wlan property %s, error = %s", prop_name, nwam_strerror( nerr ) );
        return retval;
    }

    if ( value != NULL && num > 0 ) {
        /* Create a NULL terminated list of stirngs, allocate 1 extra place
         * for NULL termination. */
        retval = (gchar**)g_malloc0( sizeof(gchar*) * (num+1) );

        for (int i = 0; i < num; i++ ) {
            retval[i]  = g_strdup ( value[i] );
        }
        retval[num]=NULL;
    }

    nwam_value_free(nwam_data);
    
    return( retval );
}

static gboolean
set_nwam_known_wlan_string_array_prop( nwam_known_wlan_handle_t known_wlan, const char* prop_name, char** strs, guint len )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    gboolean            retval = FALSE;

    g_return_val_if_fail( prop_name != NULL, retval );

    if ( known_wlan == NULL ) {
        return( retval );
    }

    if ( (nerr = nwam_known_wlan_get_prop_type( prop_name, &nwam_type ) ) != NWAM_SUCCESS 
         || nwam_type != NWAM_VALUE_TYPE_STRING ) {
        g_warning("Unexpected type for known_wlan property %s - got %d\n", prop_name, nwam_type );
        return retval;
    }

    if ( strs == NULL ) {
        if ( (nerr = nwam_known_wlan_delete_prop (known_wlan, prop_name)) != NWAM_SUCCESS ) {
            g_debug("Unable to delete known_wlan property %s, error = %s", prop_name, nwam_strerror( nerr ) );
        }
        else {
            retval = TRUE;
        }

        return retval;
    }


    if ( len == 0 ) { /* Assume a strv, i.e. NULL terminated list, otherwise strs would be NULL */
        int i;

        for( i = 0; strs != NULL && strs[i] != NULL; i++ ) {
            /* Do Nothing, just count. */
        }
        len = i;
    }

    if ( (nerr = nwam_value_create_string_array (strs, len, &nwam_data )) != NWAM_SUCCESS ) {
        g_debug("Error creating a value for string array 0x%08X", strs);
        return retval;
    }

    if ( (nerr = nwam_known_wlan_set_prop_value (known_wlan, prop_name, nwam_data)) != NWAM_SUCCESS ) {
        g_debug("Unable to set value for known_wlan property %s, error = %s", prop_name, nwam_strerror( nerr ) );
    }
    else {
        retval = TRUE;
    }

    nwam_value_free(nwam_data);
    
    return( retval );
}

static gboolean
get_nwam_known_wlan_boolean_prop( nwam_known_wlan_handle_t known_wlan, const char* prop_name )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    boolean_t           value = FALSE;

    g_return_val_if_fail( prop_name != NULL, value );

    if ( (nerr = nwam_known_wlan_get_prop_type( prop_name, &nwam_type ) ) != NWAM_SUCCESS 
         || nwam_type != NWAM_VALUE_TYPE_BOOLEAN ) {
        g_warning("Unexpected type for known_wlan property %s\n", prop_name );
        return value;
    }

    if ( (nerr = nwam_known_wlan_get_prop_value (known_wlan, prop_name, &nwam_data)) != NWAM_SUCCESS ) {
        g_debug("No value for known_wlan property %s, error = %s", prop_name, nwam_strerror( nerr ) );
        return value;
    }

    if ( (nerr = nwam_value_get_boolean(nwam_data, &value )) != NWAM_SUCCESS ) {
        g_debug("Unable to get boolean value for known_wlan property %s, error = %s", prop_name, nwam_strerror( nerr ) );
        return value;
    }

    nwam_value_free(nwam_data);

    return( (gboolean)value );
}

static guint64
get_nwam_known_wlan_uint64_prop( nwam_known_wlan_handle_t known_wlan, const char* prop_name )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    uint64_t            value = 0;

    g_return_val_if_fail( prop_name != NULL, value );

    if ( (nerr = nwam_known_wlan_get_prop_type( prop_name, &nwam_type ) ) != NWAM_SUCCESS 
         || nwam_type != NWAM_VALUE_TYPE_UINT64 ) {
        g_warning("Unexpected type for known_wlan property %s\n", prop_name );
        return value;
    }

    if ( (nerr = nwam_known_wlan_get_prop_value (known_wlan, prop_name, &nwam_data)) != NWAM_SUCCESS ) {
        g_debug("No value for known_wlan property %s, error = %s", prop_name, nwam_strerror( nerr ) );
        return value;
    }

    if ( (nerr = nwam_value_get_uint64(nwam_data, &value )) != NWAM_SUCCESS ) {
        g_debug("Unable to get uint64 value for known_wlan property %s, error = %s", prop_name, nwam_strerror( nerr ) );
        return value;
    }

    nwam_value_free(nwam_data);

    return( (guint64)value );
}

static gboolean
set_nwam_known_wlan_uint64_prop( nwam_known_wlan_handle_t known_wlan, const char* prop_name, guint64 value )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    gboolean            retval = FALSE;
    g_return_val_if_fail( prop_name != NULL, retval );

    if ( known_wlan == NULL ) {
        return( retval );
    }

    if ( (nerr = nwam_known_wlan_get_prop_type( prop_name, &nwam_type ) ) != NWAM_SUCCESS 
         || nwam_type != NWAM_VALUE_TYPE_UINT64 ) {
        g_warning("Unexpected type for known_wlan property %s - got %d\n", prop_name, nwam_type );
        return retval;
    }

    if ( (nerr = nwam_value_create_uint64 (value, &nwam_data )) != NWAM_SUCCESS ) {
        g_debug("Error creating a uint64 value" );
        return retval;
    }

    if ( (nerr = nwam_known_wlan_set_prop_value (known_wlan, prop_name, nwam_data)) != NWAM_SUCCESS ) {
        g_debug("Unable to set value for known_wlan property %s, error = %s", prop_name, nwam_strerror( nerr ) );
    }
    else {
        retval = TRUE;
    }

    nwam_value_free(nwam_data);
    
    return( retval );
}

static guint64* 
get_nwam_known_wlan_uint64_array_prop( nwam_known_wlan_handle_t known_wlan, const char* prop_name , guint *out_num )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    uint64_t           *value = NULL;
    uint_t              num = 0;
    guint64            *retval = NULL;

    g_return_val_if_fail( prop_name != NULL && out_num != NULL, retval );

    if ( (nerr = nwam_known_wlan_get_prop_type( prop_name, &nwam_type ) ) != NWAM_SUCCESS 
         || nwam_type != NWAM_VALUE_TYPE_UINT64 ) {
        g_warning("Unexpected type for known_wlan property %s\n", prop_name );
        return value;
    }

    if ( (nerr = nwam_known_wlan_get_prop_value (known_wlan, prop_name, &nwam_data)) != NWAM_SUCCESS ) {
        g_debug("No value for known_wlan property %s, error = %s", prop_name, nwam_strerror( nerr ) );
        return value;
    }

    if ( (nerr = nwam_value_get_uint64_array(nwam_data, &value, &num )) != NWAM_SUCCESS ) {
        g_debug("Unable to get uint64 value for known_wlan property %s, error = %s", prop_name, nwam_strerror( nerr ) );
        return value;
    }

    retval = (guint64*)g_malloc( sizeof(guint64) * num );
    for ( int i = 0; i < num; i++ ) {
        retval[i] = (guint64)value[i];
    }

    nwam_value_free(nwam_data);

    *out_num = num;

    return( retval );
}

static gboolean
set_nwam_known_wlan_uint64_array_prop( nwam_known_wlan_handle_t known_wlan, const char* prop_name, 
                                const guint64 *value_array, guint len )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    gboolean            retval = FALSE;
    g_return_val_if_fail( prop_name != NULL, retval );

    if ( known_wlan == NULL ) {
        return( retval );
    }

    if ( (nerr = nwam_known_wlan_get_prop_type( prop_name, &nwam_type ) ) != NWAM_SUCCESS 
         || nwam_type != NWAM_VALUE_TYPE_UINT64 ) {
        g_warning("Unexpected type for known_wlan property %s - got %d\n", prop_name, nwam_type );
        return retval;
    }

    if ( (nerr = nwam_value_create_uint64_array ((uint64_t*)value_array, len, &nwam_data )) != NWAM_SUCCESS ) {
        g_debug("Error creating a uint64 array value" );
        return retval;
    }

    if ( (nerr = nwam_known_wlan_set_prop_value (known_wlan, prop_name, nwam_data)) != NWAM_SUCCESS ) {
        g_debug("Unable to set value for known_wlan property %s, error = %s", prop_name, nwam_strerror( nerr ) );
    }
    else {
        retval = TRUE;
    }

    nwam_value_free(nwam_data);
    
    return( retval );
}

extern gboolean
nwamui_wifi_net_has_modifications( NwamuiWifiNet* self ) 
{
    if ( self != NULL && NWAMUI_IS_WIFI_NET(self) &&
         self->prv->modified ) {
        return( TRUE );
    }

    return( FALSE );
}

/* Callbacks */

static void
object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
/*     NwamuiWifiNet* self = NWAMUI_WIFI_NET(data); */
}

