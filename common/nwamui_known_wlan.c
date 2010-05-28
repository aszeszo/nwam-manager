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
 * File:   nwamui_known_wlan.c
 *
 */

#include <glib-object.h>
#include <glib/gi18n.h>
#include <strings.h>
#include <stdlib.h>
#include <errno.h>

#include "libnwamui.h"
#include "nwamui_known_wlan.h"
#include <sys/types.h>
#include <libdlwlan.h>
#include <libdllink.h>

struct _NwamuiKnownWlanPrivate {
    nwam_known_wlan_handle_t  known_wlan_h;
    gboolean                  modified;
    gchar                    *essid;            
    nwamui_wifi_security_t    security;
    guint                     wep_key_index;
    gchar**                   bssid_strv; /* NULL terminated list of strings */
    guint64                   priority;
};

#define NWAMUI_KNOWN_WLAN_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE((o), NWAMUI_TYPE_KNOWN_WLAN, NwamuiKnownWlanPrivate))

static void nwamui_known_wlan_set_property (  GObject         *object,
                                            guint            prop_id,
                                            const GValue    *value,
                                            GParamSpec      *pspec);

static void nwamui_known_wlan_get_property (  GObject         *object,
                                            guint            prop_id,
                                            GValue          *value,
                                            GParamSpec      *pspec);

static void nwamui_known_wlan_finalize (      NwamuiKnownWlan *self);

static gchar*       get_nwam_known_wlan_string_prop( nwam_known_wlan_handle_t known_wlan, 
                                                     const char* prop_name );

static gboolean     set_nwam_known_wlan_string_prop( nwam_known_wlan_handle_t known_wlan, 
                                                     const char* prop_name, const gchar* str );

static gchar**      get_nwam_known_wlan_string_array_prop(
                                    nwam_known_wlan_handle_t known_wlan, const char* prop_name );

static gboolean     set_nwam_known_wlan_string_array_prop( nwam_known_wlan_handle_t known_wlan, 
                                                           const char* prop_name, char** strs, guint len );

static gboolean     get_nwam_known_wlan_boolean_prop( nwam_known_wlan_handle_t known_wlan, const char* prop_name );

static guint64      get_nwam_known_wlan_uint64_prop( nwam_known_wlan_handle_t known_wlan, 
                                                     const char* prop_name );

static gboolean     set_nwam_known_wlan_uint64_prop( nwam_known_wlan_handle_t known_wlan, 
                                                     const char* prop_name, guint64 value );

static guint64*     get_nwam_known_wlan_uint64_array_prop(
                                    nwam_known_wlan_handle_t known_wlan, const char* prop_name , guint* out_num );

static gboolean     set_nwam_known_wlan_uint64_array_prop( nwam_known_wlan_handle_t known_wlan, const char* prop_name, 
                                                           const guint64 *value_array, guint len );

static void   nwamui_known_wlan_real_set_bssid_list(NwamuiKnownWlan *self, GList *bssid_list);
static GList* nwamui_known_wlan_real_get_bssid_list(NwamuiKnownWlan *self);
static gint         nwamui_object_real_open(NwamuiObject *object, gint flag);
static void         nwamui_object_real_set_handle(NwamuiObject *object, const gpointer handle);
static const gchar* nwamui_known_wlan_get_essid (NwamuiObject *object );
static gboolean     nwamui_object_real_set_name ( NwamuiObject  *object, const gchar    *essid ); /*   Actually set ESSID */
static gboolean     nwamui_object_real_can_rename (NwamuiObject *object);
static gint         nwamui_object_real_sort(NwamuiObject *object, NwamuiObject *other, guint sort_by);
static gboolean     nwamui_object_real_commit(NwamuiObject *object);
static gboolean     nwamui_object_real_destroy(NwamuiObject *object);
static gboolean     nwamui_object_real_validate(NwamuiObject *object, gchar **prop_name_ret);
static void         nwamui_object_real_reload(NwamuiObject* object);
static gboolean     nwamui_object_real_has_modifications(NwamuiObject* object);

enum {
    PROP_PRIORITY = 1,
    PROP_WEP_KEY_INDEX,
    PROP_SECURITY,
    PROP_BSSID_LIST,
};

G_DEFINE_TYPE (NwamuiKnownWlan, nwamui_known_wlan, NWAMUI_TYPE_WIFI_NET)

static void
nwamui_known_wlan_class_init (NwamuiKnownWlanClass *klass)
{
    /* Pointer to GObject Part of Class */
    GObjectClass *gobject_class = (GObjectClass*) klass;
    NwamuiObjectClass *nwamuiobject_class = NWAMUI_OBJECT_CLASS(klass);
        
    /* Override Some Function Pointers */
    gobject_class->set_property = nwamui_known_wlan_set_property;
    gobject_class->get_property = nwamui_known_wlan_get_property;
    gobject_class->finalize = (void (*)(GObject*)) nwamui_known_wlan_finalize;

    nwamuiobject_class->open = nwamui_object_real_open;
    nwamuiobject_class->set_handle = nwamui_object_real_set_handle;
    nwamuiobject_class->get_name = nwamui_known_wlan_get_essid;
    nwamuiobject_class->can_rename = nwamui_object_real_can_rename;
    nwamuiobject_class->set_name = nwamui_object_real_set_name;
    nwamuiobject_class->sort = nwamui_object_real_sort;
    nwamuiobject_class->commit = nwamui_object_real_commit;
    nwamuiobject_class->destroy = nwamui_object_real_destroy;
    nwamuiobject_class->validate = nwamui_object_real_validate;
    nwamuiobject_class->reload = nwamui_object_real_reload;
    nwamuiobject_class->has_modifications = nwamui_object_real_has_modifications;

	g_type_class_add_private(klass, sizeof(NwamuiKnownWlanPrivate));

    /* Create some properties */
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
      PROP_WEP_KEY_INDEX,
      g_param_spec_uint64 ("wep_key_index",
        _("Wep Key Index"),
        _("Wep Key Index"),
        1, /* Min */
        4, /* Max */
        1, /* Default */
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
nwamui_known_wlan_init (NwamuiKnownWlan *self)
{
    NwamuiKnownWlanPrivate *prv = NWAMUI_KNOWN_WLAN_GET_PRIVATE(self);
    self->prv = prv;

    prv->security = NWAMUI_WIFI_SEC_NONE;
    prv->wep_key_index = 1;
}

static void
nwamui_known_wlan_set_property (  GObject         *object,
  guint            prop_id,
  const GValue    *value,
  GParamSpec      *pspec)
{
    NwamuiKnownWlan          *self = NWAMUI_KNOWN_WLAN(object);
    NwamuiKnownWlanPrivate   *prv;

    g_return_if_fail( self != NULL );
    
    prv = self->prv;
    g_assert( prv != NULL );
    
    switch (prop_id) {
    case PROP_SECURITY: {
        prv->security = g_value_get_int(value);
        set_nwam_known_wlan_uint64_prop( self->prv->known_wlan_h, NWAM_KNOWN_WLAN_PROP_SECURITY_MODE, 
          nwamui_wifi_net_security_map_to_nwam( prv->security));
    }
        break;

    case PROP_WEP_KEY_INDEX: {
        prv->wep_key_index = g_value_get_uint64(value);
        set_nwam_known_wlan_uint64_prop( self->prv->known_wlan_h, NWAM_KNOWN_WLAN_PROP_KEYSLOT, 
          g_value_get_uint64( value ) );
    }
        break;

    case PROP_BSSID_LIST:
        nwamui_known_wlan_real_set_bssid_list(self, g_value_get_pointer(value));
        break;

    case PROP_PRIORITY: {
        set_nwam_known_wlan_uint64_prop( self->prv->known_wlan_h, NWAM_KNOWN_WLAN_PROP_PRIORITY, 
          g_value_get_uint64( value ) );
    }
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }

    self->prv->modified = TRUE;
}

static void
nwamui_known_wlan_get_property (GObject         *object,
  guint            prop_id,
  GValue          *value,
  GParamSpec      *pspec)
{
    NwamuiKnownWlan *self = NWAMUI_KNOWN_WLAN(object);
    NwamuiKnownWlanPrivate   *prv;

    g_return_if_fail( self != NULL );
    
    prv = self->prv;
    g_assert( prv != NULL );

    switch (prop_id) {
    case PROP_SECURITY: {
        g_value_set_int(value, (gint)prv->security );
    }
        break;
    case PROP_WEP_KEY_INDEX: {
        guint64 rval = 0;

        if (self->prv->known_wlan_h != NULL) {
            rval = get_nwam_known_wlan_uint64_prop( self->prv->known_wlan_h, NWAM_KNOWN_WLAN_PROP_KEYSLOT );
            /* 0 if the entity isn't existing. */
        }
        else {
            rval = prv->wep_key_index;
        }
        g_value_set_uint64( value, rval == 0 ? 1 : rval );
    }
        break;
    case PROP_BSSID_LIST:
        g_value_set_pointer(value, nwamui_known_wlan_real_get_bssid_list(self));
        break;
    case PROP_PRIORITY: {
        guint64 rval = 0;

        rval = get_nwam_known_wlan_uint64_prop( self->prv->known_wlan_h, NWAM_KNOWN_WLAN_PROP_PRIORITY );
        g_value_set_uint64( value, rval );
    }
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

/**
 * nwamui_object_real_reload:   re-load stored configuration
 **/
static void
nwamui_object_real_reload(NwamuiObject* object)
{
    NwamuiKnownWlanPrivate     *prv        = NWAMUI_KNOWN_WLAN_GET_PRIVATE(object);
    nwam_error_t              nerr;
    uint32_t               sec_mode;
    nwamui_wifi_security_t security;

    g_return_if_fail(NWAMUI_IS_KNOWN_WLAN(object));

    nwamui_object_real_open(object, NWAMUI_OBJECT_OPEN);

    g_object_freeze_notify(G_OBJECT(object));

    sec_mode = get_nwam_known_wlan_uint64_prop(prv->known_wlan_h, NWAM_KNOWN_WLAN_PROP_SECURITY_MODE);
            
    security = nwamui_wifi_net_security_map(sec_mode);

    if ( prv->security != security ) {
        prv->security = security;
        g_object_notify(G_OBJECT(object), "security");
    }

    if (prv->modified) {
        nwamui_debug("Not updating wlan %s from system due to unsaved modifications", prv->essid );
    }

    g_object_thaw_notify(G_OBJECT(object));
}

static void
nwamui_known_wlan_finalize(NwamuiKnownWlan *self)
{
    NwamuiKnownWlanPrivate *prv = NWAMUI_KNOWN_WLAN_GET_PRIVATE(self);

    g_debug("%s wifi 0x%p essid %s", __func__, self, prv->essid ? prv->essid:"nil");

    if ( prv->essid ) {
        g_free( prv->essid );
    }

    if ( prv->known_wlan_h != NULL ) {
        nwam_known_wlan_free( prv->known_wlan_h );
    }

    if ( prv->bssid_strv != NULL ) {
        g_strfreev( prv->bssid_strv );
    }

    prv = NULL;

	G_OBJECT_CLASS(nwamui_known_wlan_parent_class)->finalize(G_OBJECT(self));
}

/* Exported Functions */
/**
 * nwamui_known_wifi_new:
 * @returns: a new #NwamuiKnownWlan.
 *
 **/
extern  NwamuiObject*
nwamui_known_wifi_new(const gchar *name)
{
    NwamuiObject *self = NWAMUI_OBJECT(g_object_new (NWAMUI_TYPE_KNOWN_WLAN, NULL));
    NwamuiKnownWlanPrivate *prv = NWAMUI_KNOWN_WLAN_GET_PRIVATE(self);

    nwamui_object_set_name(NWAMUI_OBJECT(self), name);

    if (nwamui_object_real_open(self, NWAMUI_OBJECT_OPEN) != 0) {
        nwamui_object_real_open(self, NWAMUI_OBJECT_CREATE);
    }

    g_assert(prv->known_wlan_h);

    return NWAMUI_OBJECT( self );
}

extern void
nwamui_known_wlan_update_from_wlan_t(NwamuiKnownWlan* self, nwam_wlan_t *wlan)
{
    nwamui_wifi_security_t          security;
    GList                          *bssid_list;
    nwamui_wifi_bss_type_t          bss_type;
    nwamui_wifi_signal_strength_t   signal_strength;
    guint                           channel;
    guint                           speed;
        
    g_return_if_fail(wlan != NULL);

    nwamui_debug("Update known wlan '%s(0x%p)'.", wlan->nww_essid, self);

    bssid_list = nwamui_known_wlan_real_get_bssid_list(self);
    if (wlan->nww_bssid != NULL) {
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
          /* "security", security, */
          /* "bssid_list", bssid_list, */
          "bss_type", bss_type,
          "channel", channel,
          "speed", speed,
          "signal_strength", signal_strength,
          NULL);
            
        /* Not modified by user */
        /* self->prv->modified = FALSE; */
    }
}

/**
 * nwamui_known_wlan_new_with_handle:
 * @returns: a new #NwamuiKnown_Wlan.
 *
 **/
extern  NwamuiObject*          
nwamui_known_wlan_new_with_handle(nwam_known_wlan_handle_t known_wlan)
{
    NwamuiKnownWlan *self = NWAMUI_KNOWN_WLAN(g_object_new(NWAMUI_TYPE_KNOWN_WLAN, NULL));
    
    nwamui_object_real_set_handle(NWAMUI_OBJECT(self), known_wlan);

    if (self->prv->known_wlan_h == NULL) {
        g_object_unref(self);
        self = NULL;
    }

    return NWAMUI_OBJECT( self );
}

/** 
 * Compare WifiNet objects, returns values like strcmp().
 */
static gint
nwamui_object_real_sort(NwamuiObject *object, NwamuiObject *other, guint sort_by)
{
    NwamuiKnownWlanPrivate     *prv[2];
    gint rval = -1;

    if ( !( NWAMUI_IS_KNOWN_WLAN(object) && NWAMUI_IS_KNOWN_WLAN(other) ) ) {
        return( rval );
    }

    prv[0] = NWAMUI_KNOWN_WLAN_GET_PRIVATE(object);
    prv[1] = NWAMUI_KNOWN_WLAN_GET_PRIVATE(other);

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

/**
 * Ask NWAM to delete this favourite network.
 **/
static gboolean
nwamui_object_real_destroy(NwamuiObject *object)
{
    NwamuiKnownWlan *self = NWAMUI_KNOWN_WLAN(object);
    nwam_error_t                nerr;
    nwam_known_wlan_handle_t    wlan_h;

    if ( (nerr = nwam_known_wlan_destroy(self->prv->known_wlan_h, 0)) != NWAM_SUCCESS) {
        g_debug("Destroy error when destroying: %s", nwam_strerror(nerr));
        return(FALSE);;
    }

    self->prv->known_wlan_h = NULL;

    return(TRUE);
}

static gboolean
nwamui_object_real_validate(NwamuiObject *object, gchar **prop_name_ret)
{
    NwamuiKnownWlanPrivate *prv     = NWAMUI_KNOWN_WLAN_GET_PRIVATE(object);
    nwam_error_t            nerr;
    gboolean                rval    = FALSE;
    const char*             errprop = NULL;


    g_return_val_if_fail(NWAMUI_IS_KNOWN_WLAN(object), rval);

    if ((nerr = nwam_known_wlan_validate(prv->known_wlan_h, &errprop)) != NWAM_SUCCESS ) {
        g_debug("wlan has a validation error with prop %s : error: %s", errprop?errprop:"NULL", nwam_strerror(nerr));
        if ( prop_name_ret != NULL ) {
            *prop_name_ret = g_strdup( errprop );
        }
        rval = FALSE;
    } else {
        if ( prop_name_ret != NULL ) {
            *prop_name_ret = NULL;
        }
        rval = TRUE;
    }

    return( rval );
}

/**
 * Ask NWAM to connect to this network.
 **/
static gboolean
nwamui_object_real_commit( NwamuiObject *object )
{
    NwamuiKnownWlan *self = NWAMUI_KNOWN_WLAN(object);
    nwam_error_t                nerr;
    nwam_known_wlan_handle_t    wlan_h;
    gchar**                     bssid_strs = NULL;

    g_return_val_if_fail( self != NULL, FALSE );

    if ((nerr = nwam_known_wlan_commit(self->prv->known_wlan_h, 
          NWAM_FLAG_KNOWN_WLAN_NO_COLLISION_CHECK) ) != NWAM_SUCCESS ) {
		if (nerr == NWAM_ENTITY_MISSING_MEMBER) {
            const char* errprop = NULL;
			nwam_known_wlan_validate(self->prv->known_wlan_h, &errprop);
            g_warning("Couldn't commit wlan due to a validation error with prop %s\n", errprop?errprop:"NULL");
            return( FALSE );
        }
    }

    /* Not modified by user */
    self->prv->modified = FALSE;

    return( TRUE );
}

/**
 * nwamui_known_wlan_can_rename:
 * @self: a #NwamuiKnownWlan.
 * @returns: TRUE if the name.can be changed.
 *
 **/
static gboolean
nwamui_object_real_can_rename (NwamuiObject *object)
{
    NwamuiKnownWlanPrivate *prv = NWAMUI_KNOWN_WLAN_GET_PRIVATE(object);

    g_return_val_if_fail (NWAMUI_IS_KNOWN_WLAN (object), FALSE);

    if (prv->known_wlan_h != NULL) {
        if (nwam_known_wlan_can_set_name( prv->known_wlan_h )) {
            return( TRUE );
        }
    }
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
    NwamuiKnownWlanPrivate *prv = NWAMUI_KNOWN_WLAN_GET_PRIVATE(object);
    nwam_error_t    nerr;

    g_return_val_if_fail(NWAMUI_IS_KNOWN_WLAN(object), FALSE);
    g_assert (essid != NULL );

    /* Initially set name or rename. */
    if ( prv->essid) {
        /* we may rename here */
        if (prv->known_wlan_h) {
            g_debug("Renaming Favourite WLAN with name : '%s' to '%s'", prv->essid, essid );
            nerr = nwam_known_wlan_set_name(prv->known_wlan_h, essid);
            if (nerr != NWAM_SUCCESS) {
                g_warning("Error renaming favourite wlan: %s", nwam_strerror(nerr));
                return FALSE;
            }
        } else {
            /* Not a favourite, so just store data in structure */
            g_debug("Changing non-favourite WifiNet essid to %s", essid?essid:"NULL");
        }
        g_free(prv->essid);
    }
    prv->essid = g_strdup(essid);

    return TRUE;
}
                                
static gint
nwamui_object_real_open(NwamuiObject *object, gint flag)
{
    NwamuiKnownWlanPrivate *prv = NWAMUI_KNOWN_WLAN_GET_PRIVATE(object);
    nwam_error_t      nerr;

    if (flag == NWAMUI_OBJECT_CREATE) {
        g_assert(prv->essid);
        nerr = nwam_known_wlan_create(prv->essid, &prv->known_wlan_h);
        if (nerr == NWAM_SUCCESS) {
        } else {
            g_warning("nwamui_known_wlan_create error creating nwam_know_wlan_handle %s", prv->essid);
            prv->known_wlan_h = NULL;
        }
    } else if (flag == NWAMUI_OBJECT_OPEN) {
        nwam_known_wlan_handle_t handle;

        g_assert(prv->essid);

        nerr = nwam_known_wlan_read(prv->essid, 0, &handle);
        if (nerr == NWAM_SUCCESS) {
            if (prv->known_wlan_h) {
                nwam_known_wlan_free(prv->known_wlan_h);
            }
            prv->known_wlan_h = handle;
        } else if (nerr == NWAM_ENTITY_NOT_FOUND) {
            /* Most likely only exists in memory right now, so we should use
             * handle passed in as parameter. In clone mode, the new handle
             * gets from nwam_enm_copy can't be read again.
             */
            g_debug("Failed to read enm information for %s error: %s", prv->essid, nwam_strerror(nerr));
        } else {
            g_warning("Failed to read enm information for %s error: %s", prv->essid, nwam_strerror(nerr));
            prv->known_wlan_h = NULL;
        }
    } else {
        g_assert_not_reached();
    }
    return nerr;
}

static void
nwamui_object_real_set_handle(NwamuiObject *object, const gpointer new_handle)
{
    NwamuiKnownWlanPrivate   *prv        = NWAMUI_KNOWN_WLAN_GET_PRIVATE(object);
    nwam_known_wlan_handle_t  handle     = new_handle;
    gchar*                    name       = NULL;
    gchar**                   bssid_strv = NULL;
    nwam_error_t              nerr;

    g_assert(NWAMUI_IS_KNOWN_WLAN(object));

    /* Accept NULL to allow for simple re-read from system */
    if ((nerr = nwam_known_wlan_get_name(handle, &name)) != NWAM_SUCCESS) {
        g_warning("Error getting name of known wlan: %s", nwam_strerror(nerr));
        return;
    }

    nwamui_object_set_name(object, name);

    nwamui_object_real_open(object, NWAMUI_OBJECT_OPEN);

    nwamui_object_real_reload(object);

    g_free(name);
}

static const gchar*
nwamui_known_wlan_get_essid (NwamuiObject *object )
{
    NwamuiKnownWlan *self = NWAMUI_KNOWN_WLAN(object);
    g_return_val_if_fail (NWAMUI_IS_KNOWN_WLAN (self), NULL);
    return(self->prv->essid);
}

/** 
 * nwamui_known_wlan_real_set_bssid_list:
 * @nwamui_known_wlan: a #NwamuiKnownWlan.
 * @bssid_list: Value to set bssid_list to.
 * 
 **/ 
extern void
nwamui_known_wlan_real_set_bssid_list(NwamuiKnownWlan *self, GList *bssid_list)
{
    NwamuiKnownWlanPrivate  *prv            = NWAMUI_KNOWN_WLAN_GET_PRIVATE(self);
    gchar                  **bssid_strv;

    g_return_if_fail(NWAMUI_IS_KNOWN_WLAN(self));

    bssid_strv = nwamui_util_glist_to_strv(bssid_list);

    set_nwam_known_wlan_string_array_prop(prv->known_wlan_h, NWAM_KNOWN_WLAN_PROP_BSSIDS, bssid_strv, 0 );

    /* if ( prv->bssid_strv ) { */
    /*     g_strfreev( prv->bssid_strv ); */
    /* } */
    /* prv->bssid_strv = bssid_strv; */

    prv->modified = TRUE;

    g_object_notify(G_OBJECT(self), "bssid_list");
}

/**
 * nwamui_known_wlan_real_get_bssid_list:
 * @nwamui_known_wlan: a #NwamuiKnownWlan.
 *
 * @returns: the bssid_list.
 *
 **/
extern GList*  
nwamui_known_wlan_real_get_bssid_list(NwamuiKnownWlan *self)
{
    NwamuiKnownWlanPrivate  *prv        = NWAMUI_KNOWN_WLAN_GET_PRIVATE(self);
    gchar                  **bssid_strv = NULL;
    GList                   *bssid_list = NULL;

    g_return_val_if_fail(NWAMUI_IS_KNOWN_WLAN(self), bssid_list);

    bssid_strv = get_nwam_known_wlan_string_array_prop(prv->known_wlan_h, 
      NWAM_KNOWN_WLAN_PROP_BSSIDS);

    bssid_list = nwamui_util_strv_to_glist( bssid_strv );

    g_strfreev( bssid_strv );

    return bssid_list;
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

static gboolean
nwamui_object_real_has_modifications(NwamuiObject* object)
{
    NwamuiKnownWlanPrivate *prv = NWAMUI_KNOWN_WLAN_GET_PRIVATE(object);

    return prv->modified;
}

/* Callbacks */
