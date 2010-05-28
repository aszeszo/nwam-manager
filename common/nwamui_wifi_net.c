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

struct _NwamuiWifiNetPrivate {
    gboolean                       modified;
    gchar                         *essid;            
    NwamuiNcu                     *ncu;
    nwamui_wifi_security_t         security;
    nwamui_wifi_bss_type_t         bss_type;
    nwamui_wifi_signal_strength_t  signal_strength;
    guint                          channel;
    guint                          speed;
    gchar*                         mode;
    nwamui_wifi_wpa_config_t       wpa_config; /* Only valid if security is WPA */
    guint                          wep_key_index;
    gchar                         *wep_password;
    gchar                         *wpa_username;
    gchar                         *wpa_password;
    gchar                         *wpa_cert_file;
    gint                           status;
    nwamui_wifi_life_state_t       life_state;
    gboolean                       enabled;

    /* For non-favourites store prio and bssid_strv in memory only */
    gchar** bssid_strv;         /* NULL terminated list of strings */
    guint64 priority;
};

#define NWAMUI_WIFI_NET_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE((o), NWAMUI_TYPE_WIFI_NET, NwamuiWifiNetPrivate))

static void nwamui_wifi_net_set_property (  GObject         *object,
                                            guint            prop_id,
                                            const GValue    *value,
                                            GParamSpec      *pspec);

static void nwamui_wifi_net_get_property (  GObject         *object,
                                            guint            prop_id,
                                            GValue          *value,
                                            GParamSpec      *pspec);

static void nwamui_wifi_net_finalize (      NwamuiWifiNet *self);

static void nwamui_wifi_net_real_set_bssid_list(NwamuiWifiNet *self, GList *bssid_list);
static GList* nwamui_wifi_net_real_get_bssid_list(NwamuiWifiNet *self);

static const gchar* nwamui_wifi_net_get_essid (NwamuiObject *object );
static gboolean     nwamui_object_real_set_name ( NwamuiObject  *object, const gchar    *essid ); /*   Actually set ESSID */
static gboolean     nwamui_object_real_can_rename (NwamuiObject *object);
static gint         nwamui_object_real_sort(NwamuiObject *object, NwamuiObject *other, guint sort_by);
static void         nwamui_object_real_set_enabled(NwamuiObject *object, gboolean enabled);
static gboolean     nwamui_object_real_get_enabled(NwamuiObject *object);
static gboolean     nwamui_object_real_has_modifications(NwamuiObject* object);

/* Callbacks */

enum {
        PROP_NCU = 1,
        PROP_STATUS,
        PROP_SIGNAL_STRENGTH,
        PROP_CHANNEL,
        PROP_BSS_TYPE,
        PROP_SPEED,
        PROP_WPA_CONFIG,
        PROP_WEP_KEY_INDEX,
        PROP_WEP_PASSWORD,
        PROP_WPA_USERNAME,
        PROP_WPA_PASSWORD,
        PROP_WPA_CERT_FILE,
        PROP_SECURITY,
        PROP_BSSID_LIST,
};

G_DEFINE_TYPE (NwamuiWifiNet, nwamui_wifi_net, NWAMUI_TYPE_OBJECT)

static void
nwamui_wifi_net_class_init (NwamuiWifiNetClass *klass)
{
    /* Pointer to GObject Part of Class */
    GObjectClass *gobject_class = (GObjectClass*) klass;
    NwamuiObjectClass *nwamuiobject_class = NWAMUI_OBJECT_CLASS(klass);
        
    /* Override Some Function Pointers */
    gobject_class->set_property = nwamui_wifi_net_set_property;
    gobject_class->get_property = nwamui_wifi_net_get_property;
    gobject_class->finalize = (void (*)(GObject*)) nwamui_wifi_net_finalize;

    nwamuiobject_class->get_name = nwamui_wifi_net_get_essid;
    nwamuiobject_class->can_rename = nwamui_object_real_can_rename;
    nwamuiobject_class->set_name = nwamui_object_real_set_name;
    nwamuiobject_class->sort = nwamui_object_real_sort;
    nwamuiobject_class->get_enabled = nwamui_object_real_get_enabled;
    nwamuiobject_class->set_enabled = nwamui_object_real_set_enabled;
    nwamuiobject_class->has_modifications = nwamui_object_real_has_modifications;

	g_type_class_add_private(klass, sizeof(NwamuiWifiNetPrivate));

    /* Create some properties */
    g_object_class_install_property (gobject_class,
                                     PROP_NCU,
                                     g_param_spec_object ("ncu",
                                                          _("NCU that AP is seen on"),
                                                          _("NCU that AP is seen on"),
                                                          NWAMUI_TYPE_NCU,
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
                                                          G_PARAM_WRITABLE));

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

}


static void
nwamui_wifi_net_init (NwamuiWifiNet *self)
{
    NwamuiWifiNetPrivate *prv = NWAMUI_WIFI_NET_GET_PRIVATE(self);
    self->prv = prv;

    prv->modified = FALSE;

    prv->security = NWAMUI_WIFI_SEC_NONE;
    prv->signal_strength = NWAMUI_WIFI_STRENGTH_NONE;
    prv->bss_type = NWAMUI_WIFI_BSS_TYPE_AUTO;
    prv->wpa_config = NWAMUI_WIFI_WPA_CONFIG_AUTOMATIC; 
    prv->wep_key_index = 1;
    prv->status = NWAMUI_WIFI_STATUS_DISCONNECTED;
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
        case PROP_SECURITY: {
                gint tmpint = g_value_get_int (value);

                g_assert( tmpint >= NWAMUI_WIFI_SEC_NONE && tmpint < NWAMUI_WIFI_SEC_LAST );

                prv->security = (nwamui_wifi_security_t)tmpint;
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

        case PROP_WPA_CONFIG: {
                gint tmpint = g_value_get_int (value);

                g_assert( tmpint >= NWAMUI_WIFI_WPA_CONFIG_AUTOMATIC && tmpint < NWAMUI_WIFI_WPA_CONFIG_LAST );

                prv->wpa_config = (nwamui_wifi_wpa_config_t)tmpint;
            }
            break;

        case PROP_WEP_KEY_INDEX: {
                prv->wep_key_index = g_value_get_uint64(value); 
                nwamui_debug("Setting WifiNet keyslot to %ul", prv->wep_key_index);
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

        case PROP_STATUS:
            nwamui_wifi_net_set_status(self, g_value_get_int(value));
            break;

        case PROP_BSSID_LIST:
            nwamui_wifi_net_real_set_bssid_list(self, g_value_get_pointer(value));
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }

    if ( prop_id != PROP_STATUS ) {
        self->prv->modified = TRUE;
    }
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
        case PROP_WPA_CONFIG: {
                g_value_set_int(value, (gint)prv->wpa_config );
            }
            break;
        case PROP_WEP_KEY_INDEX: {
                guint64 rval = 0;
                rval = prv->wep_key_index;
                g_value_set_uint64( value, rval == 0 ? 1 : rval );
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
        case PROP_BSSID_LIST:
            g_value_set_pointer(value, nwamui_wifi_net_real_get_bssid_list(self));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}


static void
nwamui_wifi_net_finalize(NwamuiWifiNet *self)
{
    NwamuiWifiNetPrivate *prv = NWAMUI_WIFI_NET_GET_PRIVATE(self);

    g_debug("%s wifi 0x%p essid %s", __func__, self, prv->essid ? prv->essid:"nil");

    if ( prv->essid ) {
        g_free( prv->essid );
    }
    if (prv->mode != NULL ) {
        g_free( prv->mode );
    }

    if ( prv->bssid_strv != NULL ) {
        g_strfreev( prv->bssid_strv );
    }

    prv = NULL;

	G_OBJECT_CLASS(nwamui_wifi_net_parent_class)->finalize(G_OBJECT(self));
}

/* Exported Functions */

/**
 * nwamui_wifi_net_new:
 *
 * @essid: a C string containing the ESSID of the link
 * 
 * @returns: a new #NwamuiWifiNet.
 *
 * Creates a new #NwamuiWifiNet.
 **/
extern  NwamuiWifiNet*
nwamui_wifi_net_new(NwamuiNcu *ncu, const gchar *essid)
{
    NwamuiWifiNet*  self = NULL;
    
    self = NWAMUI_WIFI_NET(g_object_new (NWAMUI_TYPE_WIFI_NET, 
                "ncu", ncu,
                NULL));

    nwamui_object_set_name(NWAMUI_OBJECT(self), essid);
    
    self->prv->modified = TRUE;

    return( self );
}

extern gboolean
nwamui_wifi_net_update_from_wlan_t(NwamuiWifiNet* self, nwam_wlan_t *wlan)
{
    if ( wlan != NULL ) {
        const gchar*                    essid = wlan->nww_essid;
        nwamui_wifi_security_t          security;
        GList                          *bssid_list;
        nwamui_wifi_bss_type_t          bss_type;
        nwamui_wifi_signal_strength_t   signal_strength;
        guint                           channel;
        guint                           speed;
        
        bssid_list = nwamui_wifi_net_real_get_bssid_list( self );
        if ( wlan->nww_bssid != NULL ) {
            /* Merge list */
            GList *match = g_list_find_custom(bssid_list, wlan->nww_bssid, (GCompareFunc)g_strcmp0);
            if ( match == NULL ) {
                bssid_list = g_list_append(bssid_list, g_strdup(wlan->nww_bssid));
            }
        }
        security = nwamui_wifi_net_security_map(wlan->nww_security_mode);
        channel = wlan->nww_channel;
        speed = wlan->nww_speed / 2; /* dladm_wlan_speed_t needs to be div by 2 */
        bss_type = nwamui_wifi_net_bss_type_map(wlan->nww_bsstype);
        signal_strength = nwamui_wifi_net_strength_map(wlan->nww_signal_strength);

        if ( self != NULL ) {
            g_object_set (G_OBJECT (self),
              "name", essid,
              "security", security,
              "bssid_list", bssid_list,
              "bss_type", bss_type,
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
nwamui_wifi_net_new_from_wlan_t(NwamuiNcu *ncu, nwam_wlan_t *wlan)
{
    NwamuiWifiNet*  self = NULL;
    
    self = NWAMUI_WIFI_NET(g_object_new (NWAMUI_TYPE_WIFI_NET, NULL));

    if (!nwamui_wifi_net_update_from_wlan_t(self, wlan)) {
        g_object_unref(self);
        return( NULL );
    }

    if (ncu != NULL) {
        self->prv->ncu = NWAMUI_NCU(g_object_ref(ncu));
    }

    return(self);
}


/** 
 * Compare WifiNet objects, returns values like strcmp().
 */
static gint
nwamui_object_real_sort(NwamuiObject *object, NwamuiObject *other, guint sort_by)
{
    NwamuiWifiNetPrivate     *prv[2];
    gint rval = -1;

    if ( !( NWAMUI_IS_WIFI_NET(object) && NWAMUI_IS_WIFI_NET(other) ) ) {
        return( rval );
    }

    prv[0] = NWAMUI_WIFI_NET_GET_PRIVATE(object);
    prv[1] = NWAMUI_WIFI_NET_GET_PRIVATE(other);

    switch (sort_by) {
    case NWAMUI_OBJECT_SORT_BY_NAME: {
        /* Ignore sort_by */
        if ( prv[0]->essid == NULL && prv[1]->essid == NULL ) {
            rval = 0;
        }
        else if ( prv[0]->essid == NULL || prv[1]->essid == NULL ) {
            rval = prv[0]->essid == NULL ? -1:1;
        }
        else if ( ( rval = g_strcmp0(prv[0]->essid, prv[1]->essid)) == 0 ) {
#ifdef _CARE_FOR_BSSID
            /* Now need to look at BSSID List too if we've no match so far */
            if ( self_bssid_list == NULL && other_bssid_list == NULL ) {
                /* Use rval from previous strcmp */
            }
            else {
                if ( self_bssid_list != NULL ) {
                    /* Look for a match between other bssid and an entry in the
                     * own bssid_list */
                    GList *match = g_list_find_custom( self_bssid_list, prv[1]->bssid, (GCompareFunc)g_strcmp0 );
                    if ( match != NULL ) {
                        rval = 0;
                    }
                }
                if ( rval != 0 && other_bssid_list != NULL ) {
                    /* Look for a match between own bssid and an entry in the
                     * other bssid_list */
                    GList *match = g_list_find_custom( other_bssid_list, prv[0]->bssid, (GCompareFunc)g_strcmp0 );
                    if ( match != NULL ) {
                        rval = 0;
                    }
                }
            }
#else
            /* Use rval from strcmp of essid above */
#endif /* _CARE_FOR_BSSID */
        }
    }
        break;
    case NWAMUI_OBJECT_SORT_BY_PRIO: {
        guint64 prio_a = 0;
        guint64 prio_b = 0;

        prio_a = nwamui_wifi_net_get_priority(NWAMUI_WIFI_NET(object));

        prio_b = nwamui_wifi_net_get_priority(NWAMUI_WIFI_NET(other));

        rval = (prio_a - prio_b);
    }
        break;
    default:
        g_warning("NwamuiObject::real_sort id '%d' not implemented for `%s'", sort_by, g_type_name(G_TYPE_FROM_INSTANCE(object)));
        break;
    }
    return( rval );
}

static void
nwamui_object_real_set_enabled(NwamuiObject *object, gboolean enabled)
{
    NwamuiWifiNetPrivate *prv = NWAMUI_WIFI_NET_GET_PRIVATE(object);
    g_return_if_fail(NWAMUI_IS_WIFI_NET(object));

    prv->enabled = enabled;
}

static gboolean
nwamui_object_real_get_enabled(NwamuiObject *object)
{
    NwamuiWifiNetPrivate *prv = NWAMUI_WIFI_NET_GET_PRIVATE(object);
    g_return_val_if_fail(NWAMUI_IS_WIFI_NET(object), FALSE);
    return prv->enabled;
}

/**
 * Ask NWAM to store the password information
 **/
extern void
nwamui_wifi_net_store_key(NwamuiWifiNet *self)
{
    NwamuiWifiNetPrivate   *prv = NWAMUI_WIFI_NET_GET_PRIVATE(self);
    nwamui_wifi_security_t  security;

    g_return_if_fail(NWAMUI_IS_WIFI_NET(self));

    /* Must get, because this prop is overwriten by known wlan. */
    g_object_get(self, "security", &security, NULL);

    switch (security) {
    case NWAMUI_WIFI_SEC_NONE: 
        break;

#ifdef WEP_ASCII_EQ_HEX 
    case NWAMUI_WIFI_SEC_WEP:
#else
    case NWAMUI_WIFI_SEC_WEP_HEX:
    case NWAMUI_WIFI_SEC_WEP_ASCII:
#endif /* WEP_ASCII_EQ_HEX */
    case NWAMUI_WIFI_SEC_WPA_PERSONAL: {
        NwamuiDaemon*  daemon = nwamui_daemon_get_instance();
        gchar         *device = NULL;
        nwam_error_t   nerr;

        if (prv->ncu == NULL) {
            prv->ncu = nwamui_ncp_get_first_wireless_ncu_from_active_ncp(daemon);
        }
        device = nwamui_ncu_get_device_name(prv->ncu);
        /* Make sure we use the correct info of the wifi_net or known_wlan. */
        nerr = nwam_wlan_set_key(device,
          nwamui_object_get_name(NWAMUI_OBJECT(self)),
          NULL,
          nwamui_wifi_net_security_map_to_nwam(security),
          nwamui_wifi_net_get_wep_key_index(self),
          prv->wep_password?prv->wep_password:"");

        if (nerr != NWAM_SUCCESS) {
            g_warning("Error saving network key NWAM : %s", nwam_strerror(nerr));
            nwamui_object_event(NWAMUI_OBJECT(daemon), NWAMUI_DAEMON_INFO_GENERIC, _("Failed to store network key."));
        }
                    
        g_object_unref(daemon);
        g_free(device);
    }
    break;
#if 0
    /* Currently ENTERPRISE is not supported */
    case NWAMUI_WIFI_SEC_WPA_ENTERPRISE:
        nwamui_wifi_wpa_config_t wpa_config_type = nwamui_wifi_net_get_wpa_config(wifi_net);

        /* FIXME: Make this work when ENTERPRISE supported */
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
nwamui_wifi_net_connect(NwamuiWifiNet *self, gboolean add_to_favourites)
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

    nwamui_debug("nwam_wlan_select(%s, %s, NULL, %s[%d], fav(%s))", 
      device, self->prv->essid, 
      sec_mode_str, sec_mode,
      add_to_favourites?"TRUE":"FALSE");

    nerr = nwam_wlan_select(device, self->prv->essid, NULL, sec_mode, add_to_favourites?B_TRUE:B_FALSE);

    if (nerr != NWAM_SUCCESS) {
        NwamuiDaemon*   daemon = nwamui_daemon_get_instance();

        if (nerr == NWAM_ENTITY_INVALID_STATE) {
            nwamui_object_event(NWAMUI_OBJECT(daemon), NWAMUI_DAEMON_INFO_GENERIC, _("Failed to connect to wireless network, please try it later."));
        } else {
            g_warning("Error selecting network with NWAM : %s", nwam_strerror(nerr));
        }

        g_object_unref(daemon);
    }
    
    g_free(device);
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

/**
 * nwamui_wifi_net_can_rename:
 * @self: a #NwamuiWifiNet.
 * @returns: TRUE if the name.can be changed.
 *
 **/
static gboolean
nwamui_object_real_can_rename (NwamuiObject *object)
{
    NwamuiWifiNetPrivate *prv = NWAMUI_WIFI_NET_GET_PRIVATE(object);

    g_return_val_if_fail (NWAMUI_IS_WIFI_NET (object), FALSE);

    return FALSE;
}

/**
 * nwamui_object_real_set_name:
 *
 * Actually set ESSID
 */
static gboolean
nwamui_object_real_set_name(NwamuiObject *object, const gchar *essid)
{
    NwamuiWifiNetPrivate *prv = NWAMUI_WIFI_NET_GET_PRIVATE(object);
    nwam_error_t    nerr;

    g_return_val_if_fail(NWAMUI_IS_WIFI_NET(object), FALSE);
    g_assert (essid != NULL );

    /* Initially set name or rename. */
    if ( prv->essid) {
        g_free(prv->essid);
    }
    prv->essid = g_strdup(essid);

    return TRUE;
}
                                
static const gchar*
nwamui_wifi_net_get_essid (NwamuiObject *object )
{
    NwamuiWifiNet *self = NWAMUI_WIFI_NET(object);
    g_return_val_if_fail (NWAMUI_IS_WIFI_NET (self), NULL);
    return(self->prv->essid);
}

gchar*                  
nwamui_wifi_net_get_display_string (NwamuiWifiNet *self, gboolean has_many_wireless )
{
    gchar*  ret_str = NULL;
    gchar*  name = NULL;
    GString* gstr = g_string_new("");
    
    g_return_val_if_fail (NWAMUI_IS_WIFI_NET (self), ret_str);
    
    if ( self->prv->ncu && has_many_wireless ) {
        gchar*  dev_name = NULL;

        dev_name = nwamui_ncu_get_device_name( self->prv->ncu );

        g_string_append_printf(gstr, _("[%s] "), dev_name );

        g_free(dev_name);
    }

    name = self->prv->essid?self->prv->essid:"";

    g_string_append_printf(gstr, _("%s"), name );

    if ( self->prv->bss_type == NWAMUI_WIFI_BSS_TYPE_ADHOC ) {
        g_string_append_printf(gstr, _(" (Computer-to-Computer)") );
    }

    ret_str = g_string_free(gstr, FALSE);

    return( ret_str );
}

extern void
nwamui_wifi_net_set_status(NwamuiWifiNet *self, nwamui_wifi_status_t status)
{
    NwamuiWifiNetPrivate     *prv        = NWAMUI_WIFI_NET_GET_PRIVATE(self);

    g_return_if_fail (NWAMUI_IS_WIFI_NET(self));
    g_assert (status >= NWAMUI_WIFI_STATUS_CONNECTED && status < NWAMUI_WIFI_STATUS_LAST );

    if (prv->status != status) {
        prv->status = status;
        g_object_notify(G_OBJECT(self), "status");
    }
}
          
extern nwamui_wifi_status_t
nwamui_wifi_net_get_status ( NwamuiWifiNet *self )
{
    NwamuiWifiNetPrivate     *prv        = NWAMUI_WIFI_NET_GET_PRIVATE(self);
    
    g_return_val_if_fail(NWAMUI_IS_WIFI_NET (self), NWAMUI_WIFI_STATUS_DISCONNECTED);

    return prv->status;
}

/* Get/Set Security Type, overwrite by known_wlan. */
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
                                
/* Get/Set WEP Password/Key Index (1-4), overwrite by known_wlan. */
extern void                    
nwamui_wifi_net_set_wep_key_index (  NwamuiWifiNet  *self,
                                    guint           wep_key_index )
{
    g_return_if_fail (NWAMUI_IS_WIFI_NET(self));
    g_return_if_fail ( wep_key_index >= 1 && wep_key_index <= 4 );

    g_object_set (G_OBJECT (self),
      "wep_key_index", (guint64)wep_key_index,
                  NULL);
}
                                
extern guint
nwamui_wifi_net_get_wep_key_index (NwamuiWifiNet *self )
{
    guint64  rval = 0;
    
    g_return_val_if_fail (NWAMUI_IS_WIFI_NET (self), rval);

    g_object_get (G_OBJECT (self),
                  "wep_key_index", &rval,
                  NULL);
    
    return (guint64)rval;
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
 * nwamui_wifi_net_real_set_bssid_list:
 * @nwamui_wifi_net: a #NwamuiWifiNet.
 * @bssid_list: Value to set bssid_list to.
 * 
 **/ 
extern void
nwamui_wifi_net_real_set_bssid_list(NwamuiWifiNet *self, GList *bssid_list)
{
    NwamuiWifiNetPrivate  *prv            = NWAMUI_WIFI_NET_GET_PRIVATE(self);
    GList                 *fav_bssid_list = NULL;
    gchar                **bssid_strv;

    g_return_if_fail(NWAMUI_IS_WIFI_NET(self));

    /* Merge lists */
    if ( fav_bssid_list != NULL ) {
        if ( bssid_list != NULL ) {
            for (GList *elem = g_list_first(fav_bssid_list);
                 elem != NULL;
                 elem = g_list_next(elem) ) {
                if ( elem->data != NULL ) {
                    GList  *found;
                    if ( (found = g_list_find_custom(bssid_list, elem->data, 
                          (GCompareFunc)g_strcmp0)) != NULL ) {
                        /* Already in fav_bssid_list, so remove it */
                        g_free(found->data); /* Free it now */
                        bssid_list = g_list_delete_link(bssid_list, found);
                    }
                }
            }
            g_list_foreach(fav_bssid_list, (GFunc)g_free, NULL);
            g_list_free(fav_bssid_list);
        }
    }
                
    bssid_strv = nwamui_util_glist_to_strv( bssid_list );

    if ( prv->bssid_strv ) {
        g_strfreev( prv->bssid_strv );
    }
    prv->bssid_strv = bssid_strv;

    g_object_notify(G_OBJECT(self), "bssid_list");
}

/**
 * nwamui_wifi_net_real_get_bssid_list:
 * @nwamui_wifi_net: a #NwamuiWifiNet.
 *
 * @returns: the bssid_list.
 *
 **/
extern GList*  
nwamui_wifi_net_real_get_bssid_list(NwamuiWifiNet *self)
{
    NwamuiWifiNetPrivate *prv            = NWAMUI_WIFI_NET_GET_PRIVATE(self);
    GList                *fav_bssid_list = NULL;
    GList*                bssid_list     = NULL; 

    g_return_val_if_fail(NWAMUI_IS_WIFI_NET(self), bssid_list);

    bssid_list = nwamui_util_strv_to_glist( prv->bssid_strv );

    /* Merge lists */
    if ( fav_bssid_list != NULL ) {
        if ( bssid_list == NULL ) {
            /* Just use fav_bssid_list */
            bssid_list = fav_bssid_list;
        }
        else {
            for (GList *elem = g_list_first(fav_bssid_list);
                 elem != NULL;
                 elem = g_list_next(elem) ) {
                if ( elem->data != NULL ) {
                    if (g_list_find_custom( bssid_list, elem->data, (GCompareFunc)g_strcmp0) == NULL) {
                        bssid_list = g_list_prepend(bssid_list, g_strdup(elem->data));
                    }
                }
            }
            g_list_foreach(fav_bssid_list, (GFunc)g_free, NULL);
            g_list_free(fav_bssid_list);
        }
    }

    return( bssid_list );
}

/* overwrite by known_wlan. */
void
nwamui_wifi_net_set_bssid_list(NwamuiWifiNet *self, GList *bssid_list)
{
    g_return_if_fail(NWAMUI_IS_WIFI_NET(self));

    g_object_set(G_OBJECT(self), "bssid_list", bssid_list, NULL);
}

GList*
nwamui_wifi_net_get_bssid_list(NwamuiWifiNet *self)
{
    GList *bssid_list = NULL; 

    g_return_val_if_fail(NWAMUI_IS_WIFI_NET (self), bssid_list);

    g_object_get(G_OBJECT(self), "bssid_list", &bssid_list, NULL);

    return bssid_list;
}

/** 
 * nwamui_wifi_net_set_priority:
 * @nwamui_wifi_net: a #NwamuiWifiNet.
 * @priority: Value to set priority to.
 *
 * overwrite by known_wlan.
 */ 
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
 * overwrite by known_wlan.
 */
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

extern void
nwamui_wifi_net_set_life_state(NwamuiWifiNet *self, nwamui_wifi_life_state_t life_state)
{
    NwamuiWifiNetPrivate *prv = NWAMUI_WIFI_NET_GET_PRIVATE(self);

    prv->life_state = life_state;
    g_debug("ESSID %s life state %d", prv->essid, prv->life_state);
}

extern nwamui_wifi_life_state_t
nwamui_wifi_net_get_life_state(NwamuiWifiNet *self)
{
    NwamuiWifiNetPrivate *prv = NWAMUI_WIFI_NET_GET_PRIVATE(self);

    return prv->life_state;
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

static gboolean
nwamui_object_real_has_modifications(NwamuiObject* object)
{
    NwamuiWifiNetPrivate *prv = NWAMUI_WIFI_NET_GET_PRIVATE(object);

    return prv->modified;
}

extern gchar*
nwamui_wifi_net_get_a11y_description( NwamuiWifiNet* self )
{
    gchar*          ret_str = NULL;
    gchar*          essid_str;
    const gchar*    state_str;
    const gchar*    secure_str;
    const gchar*    signal_str;
    NwamuiDaemon*   daemon;
    NwamuiObject*   ncp;
    gboolean        has_many_wifi;

    g_return_val_if_fail( NWAMUI_IS_WIFI_NET(self), ret_str );

    daemon = nwamui_daemon_get_instance();
    ncp = nwamui_daemon_get_active_ncp( daemon );
    has_many_wifi = nwamui_ncp_get_wireless_link_num(NWAMUI_NCP(ncp)) > 1;
    g_object_unref(G_OBJECT(ncp));
    g_object_unref(G_OBJECT(daemon));

    /* Compose a string like : ESSID, connected, secure, strong */

    essid_str = nwamui_wifi_net_get_display_string(self, has_many_wifi );

    switch (self->prv->status ) {
        case NWAMUI_WIFI_STATUS_CONNECTED:
            state_str = _("Connected");
            break;
        case NWAMUI_WIFI_STATUS_DISCONNECTED:
        case NWAMUI_WIFI_STATUS_FAILED:
            state_str = _("Not Connected");
            ;;
    }

    signal_str = nwamui_wifi_net_get_signal_strength_string(self);

    if ( self->prv->security == NWAMUI_WIFI_SEC_NONE ) {
        secure_str = _("Open");
    }
    else {
        secure_str = _("Secure");
    }

    ret_str = g_strdup_printf(  "%s, %s, %s, %s",
                                essid_str,
                                state_str,
                                secure_str,
                                signal_str );

    g_free( essid_str );

    return( ret_str );
}


/* Callbacks */
