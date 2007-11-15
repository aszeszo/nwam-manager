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
 * File:   nwamui_ncu.c
 *
 */

#include <glib-object.h>
#include <glib/gi18n.h>
#include <strings.h>

#include "libnwamui.h"
#include "nwamui_ncu.h"

static GObjectClass *parent_class = NULL;

struct _NwamuiNcuPrivate {
        gchar*                      vanity_name;
        gchar*                      device_name;
        nwamui_ncu_type_t           ncu_type;
        gboolean                    active;
        guint                       speed;
        gboolean                    ipv4_auto_conf;
        gchar*                      ipv4_address;
        gchar*                      ipv4_subnet;
        gchar*                      ipv4_gateway;
        gboolean                    ipv6_active;
        gboolean                    ipv6_auto_conf;
        gchar*                      ipv6_address;
        gchar*                      ipv6_prefix;
        NwamuiWifiNet*              wifi_info;
        gboolean                    rules_enabled;
        gpointer                    rules_ncus;
        nwamui_ncu_rule_state_t     rules_ncu_state;
        nwamui_ncu_rule_action_t    rules_action;
};

enum {
        PROP_VANITY_NAME = 1,
        PROP_DEVICE_NAME,
        PROP_NCU_TYPE,
        PROP_ACTIVE,
        PROP_SPEED,
        PROP_IPV4_AUTO_CONF,
        PROP_IPV4_ADDRESS,
        PROP_IPV4_SUBNET,
        PROP_IPV4_GATEWAY,
        PROP_IPV6_ACTIVE,
        PROP_IPV6_AUTO_CONF,
        PROP_IPV6_ADDRESS,
        PROP_IPV6_PREFIX,
        PROP_WIFI_INFO,
        PROP_RULES_ENABLED,
        PROP_RULES_NCUS,
        PROP_RULES_NCU_STATE,
        PROP_RULES_ACTION
};

static void nwamui_ncu_set_property ( GObject         *object,
                                      guint            prop_id,
                                      const GValue    *value,
                                      GParamSpec      *pspec);

static void nwamui_ncu_get_property ( GObject         *object,
                                      guint            prop_id,
                                      GValue          *value,
                                      GParamSpec      *pspec);

static void nwamui_ncu_finalize (     NwamuiNcu *self);


/* Callbacks */
static void object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);

G_DEFINE_TYPE (NwamuiNcu, nwamui_ncu, G_TYPE_OBJECT)

static void
nwamui_ncu_class_init (NwamuiNcuClass *klass)
{
    /* Pointer to GObject Part of Class */
    GObjectClass *gobject_class = (GObjectClass*) klass;
        
    /* Initialise Static Parent Class pointer */
    parent_class = g_type_class_peek_parent (klass);

    /* Override Some Function Pointers */
    gobject_class->set_property = nwamui_ncu_set_property;
    gobject_class->get_property = nwamui_ncu_get_property;
    gobject_class->finalize = (void (*)(GObject*)) nwamui_ncu_finalize;

    /* Create some properties */
    g_object_class_install_property (gobject_class,
                                     PROP_VANITY_NAME,
                                     g_param_spec_string ("vanity_name",
                                                          _("vanity_name"),
                                                          _("vanity_name"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_DEVICE_NAME,
                                     g_param_spec_string ("device_name",
                                                          _("device_name"),
                                                          _("device_name"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_NCU_TYPE,
                                     g_param_spec_int   ("ncu_type",
                                                         _("ncu_type"),
                                                         _("ncu_type"),
                                                          NWAMUI_NCU_TYPE_WIRED,
                                                          NWAMUI_NCU_TYPE_LAST-1,
                                                          NWAMUI_NCU_TYPE_WIRED,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_ACTIVE,
                                     g_param_spec_boolean ("active",
                                                          _("active"),
                                                          _("active"),
                                                          FALSE,
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
                                     PROP_IPV4_AUTO_CONF,
                                     g_param_spec_boolean ("ipv4_auto_conf",
                                                          _("ipv4_auto_conf"),
                                                          _("ipv4_auto_conf"),
                                                          FALSE,
                                                          G_PARAM_READWRITE));


    g_object_class_install_property (gobject_class,
                                     PROP_IPV4_ADDRESS,
                                     g_param_spec_string ("ipv4_address",
                                                          _("ipv4_address"),
                                                          _("ipv4_address"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_IPV4_SUBNET,
                                     g_param_spec_string ("ipv4_subnet",
                                                          _("ipv4_subnet"),
                                                          _("ipv4_subnet"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_IPV4_GATEWAY,
                                     g_param_spec_string ("ipv4_gateway",
                                                          _("ipv4_gateway"),
                                                          _("ipv4_gateway"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_IPV6_ACTIVE,
                                     g_param_spec_boolean ("ipv6_active",
                                                           _("ipv6_active"),
                                                           _("ipv6_active"),
                                                          FALSE,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_IPV6_AUTO_CONF,
                                     g_param_spec_boolean("ipv6_auto_conf",
                                                          _("ipv6_auto_conf"),
                                                          _("ipv6_auto_conf"),
                                                          FALSE,
                                                          G_PARAM_READWRITE));


    g_object_class_install_property (gobject_class,
                                     PROP_IPV6_ADDRESS,
                                     g_param_spec_string ("ipv6_address",
                                                          _("ipv6_address"),
                                                          _("ipv6_address"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_IPV6_PREFIX,
                                     g_param_spec_string ("ipv6_prefix",
                                                          _("ipv6_prefix"),
                                                          _("ipv6_prefix"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_WIFI_INFO,
                                     g_param_spec_object ("wifi_info",
                                                          _("Wi-Fi configuration information."),
                                                          _("Wi-Fi configuration information."),
                                                          NWAMUI_TYPE_WIFI_NET,
                                                          G_PARAM_READWRITE));
    
    g_object_class_install_property (gobject_class,
                                     PROP_RULES_ENABLED,
                                     g_param_spec_boolean ("rules_enabled",
                                                          _("rules_enabled"),
                                                          _("rules_enabled"),
                                                          FALSE,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_RULES_NCUS,
                                     g_param_spec_pointer ("rules_ncus",
                                                          _("rules_ncus"),
                                                          _("rules_ncus"),
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_RULES_NCU_STATE,
                                     g_param_spec_int ("rules_ncu_state",
                                                       _("rules_ncu_state"),
                                                       _("rules_ncu_state"),
                                                       NWAMUI_NCU_RULE_STATE_IS_CONNECTED,
                                                       NWAMUI_NCU_RULE_STATE_LAST,
                                                       NWAMUI_NCU_RULE_STATE_IS_CONNECTED,
                                                       G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_RULES_ACTION,
                                     g_param_spec_int ("rules_action",
                                                       _("rules_action"),
                                                       _("rules_action"),
                                                       NWAMUI_NCU_RULE_ACTION_ENABLE,
                                                       NWAMUI_NCU_RULE_ACTION_LAST,
                                                       NWAMUI_NCU_RULE_ACTION_ENABLE,
                                                       G_PARAM_READWRITE));


}


static void
nwamui_ncu_init (NwamuiNcu *self)
{
    self->prv = g_new0 (NwamuiNcuPrivate, 1);
    
    self->prv->vanity_name = NULL;
    self->prv->device_name = NULL;
    self->prv->ncu_type = NWAMUI_NCU_TYPE_WIRED;
    self->prv->active = FALSE;
    self->prv->speed = 0;
    self->prv->ipv4_auto_conf = TRUE;
    self->prv->ipv4_address = NULL;
    self->prv->ipv4_subnet = NULL;
    self->prv->ipv4_gateway = NULL;
    self->prv->ipv6_active = FALSE;
    self->prv->ipv6_auto_conf = TRUE;
    self->prv->ipv6_address = NULL;
    self->prv->ipv6_prefix = NULL;    
    self->prv->wifi_info = NULL;
    self->prv->rules_enabled = FALSE;
    self->prv->rules_ncus = NULL;
    self->prv->rules_ncu_state = NWAMUI_NCU_RULE_STATE_IS_CONNECTED;
    self->prv->rules_action = NWAMUI_NCU_RULE_ACTION_ENABLE;

    
    g_signal_connect(G_OBJECT(self), "notify", (GCallback)object_notify_cb, (gpointer)self);
}

static void
nwamui_ncu_set_property ( GObject         *object,
                                    guint            prop_id,
                                    const GValue    *value,
                                    GParamSpec      *pspec)
{
    NwamuiNcu *self = NWAMUI_NCU(object);
    gchar*      tmpstr = NULL;
    gint        tmpint = 0;

    switch (prop_id) {
        case PROP_VANITY_NAME: {
                if ( self->prv->vanity_name != NULL ) {
                        g_free( self->prv->vanity_name );
                }
                self->prv->vanity_name = g_strdup( g_value_get_string( value ) );
            }
            break;
        case PROP_DEVICE_NAME: {
                if ( self->prv->device_name != NULL ) {
                        g_free( self->prv->device_name );
                }
                self->prv->device_name = g_strdup( g_value_get_string( value ) );
            }
            break;
        case PROP_NCU_TYPE: {
                self->prv->ncu_type = g_value_get_int( value );
            }
            break;
        case PROP_ACTIVE: {
                self->prv->active = g_value_get_boolean( value );
            }
            break;
        case PROP_SPEED: {
                self->prv->speed = g_value_get_uint( value );
            }
            break;
        case PROP_IPV4_AUTO_CONF: {
                self->prv->ipv4_auto_conf = g_value_get_boolean( value );
            }
            break;
        case PROP_IPV4_ADDRESS: {
                if ( self->prv->ipv4_address != NULL ) {
                        g_free( self->prv->ipv4_address );
                }
                self->prv->ipv4_address = g_strdup( g_value_get_string( value ) );
            }
            break;
        case PROP_IPV4_SUBNET: {
                if ( self->prv->ipv4_subnet != NULL ) {
                        g_free( self->prv->ipv4_subnet );
                }
                self->prv->ipv4_subnet = g_strdup( g_value_get_string( value ) );
            }
            break;
        case PROP_IPV4_GATEWAY: {
                if ( self->prv->ipv4_gateway != NULL ) {
                        g_free( self->prv->ipv4_gateway );
                }
                self->prv->ipv4_gateway = g_strdup( g_value_get_string( value ) );
            }
            break;
        case PROP_IPV6_ACTIVE: {
                self->prv->ipv6_active = g_value_get_boolean( value );
            }
            break;
        case PROP_IPV6_AUTO_CONF: {
                self->prv->ipv6_auto_conf = g_value_get_boolean( value );
            }
            break;
        case PROP_IPV6_ADDRESS: {
                if ( self->prv->ipv6_address != NULL ) {
                        g_free( self->prv->ipv6_address );
                }
                self->prv->ipv6_address = g_strdup( g_value_get_string( value ) );
            }
            break;
        case PROP_IPV6_PREFIX: {
                if ( self->prv->ipv6_prefix != NULL ) {
                        g_free( self->prv->ipv6_prefix );
                }
                self->prv->ipv6_prefix = g_strdup( g_value_get_string( value ) );
            }
            break;
        case PROP_WIFI_INFO: {
                /* Should be a Wireless i/f */
                g_assert( self->prv->ncu_type == NWAMUI_NCU_TYPE_WIRELESS );
                if ( self->prv->wifi_info != NULL ) {
                        g_object_unref( self->prv->wifi_info );
                }
                self->prv->wifi_info = NWAMUI_WIFI_NET( g_value_dup_object( value ) );
            }
            break;
        case PROP_RULES_ENABLED: {
                self->prv->rules_enabled = g_value_get_boolean( value );
            }
            break;

        case PROP_RULES_NCUS: {
                if ( self->prv->rules_ncus != NULL ) {
                        g_free( self->prv->rules_ncus );
                }
                self->prv->rules_ncus = g_value_get_pointer( value );
            }
            break;

        case PROP_RULES_NCU_STATE: {
                self->prv->rules_ncu_state = (nwamui_ncu_rule_state_t)g_value_get_int( value );
            }
            break;

        case PROP_RULES_ACTION: {
                self->prv->rules_action = (nwamui_ncu_rule_action_t)g_value_get_int( value );
            }
            break;
        default: 
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
nwamui_ncu_get_property (GObject         *object,
                                  guint            prop_id,
                                  GValue          *value,
                                  GParamSpec      *pspec)
{
    NwamuiNcu *self = NWAMUI_NCU(object);

    switch (prop_id) {
        case PROP_VANITY_NAME: {
                g_value_set_string(value, self->prv->vanity_name);
            }
            break;
        case PROP_DEVICE_NAME: {
                g_value_set_string(value, self->prv->device_name);
            }
            break;
        case PROP_NCU_TYPE: {
                g_value_set_int(value, self->prv->ncu_type);
            }
            break;
        case PROP_ACTIVE: {
                g_value_set_boolean(value, self->prv->active);
            }
            break;
        case PROP_SPEED: {
                g_value_set_uint( value, self->prv->speed );
            }
            break;
        case PROP_IPV4_AUTO_CONF: {
                g_value_set_boolean(value, self->prv->ipv4_auto_conf);
            }
            break;
        case PROP_IPV4_ADDRESS: {
                g_value_set_string(value, self->prv->ipv4_address);
            }
            break;
        case PROP_IPV4_SUBNET: {
                g_value_set_string(value, self->prv->ipv4_subnet);
            }
            break;
        case PROP_IPV4_GATEWAY: {
                g_value_set_string(value, self->prv->ipv4_gateway);
            }
            break;
        case PROP_IPV6_ACTIVE: {
                g_value_set_boolean(value, self->prv->ipv6_active);
            }
            break;
        case PROP_IPV6_AUTO_CONF: {
                g_value_set_boolean(value, self->prv->ipv6_auto_conf);
            }
            break;
        case PROP_IPV6_ADDRESS: {
                g_value_set_string(value, self->prv->ipv6_address);
            }
            break;
        case PROP_IPV6_PREFIX: {
                g_value_set_string(value, self->prv->ipv6_prefix);
            }
            break;
        case PROP_WIFI_INFO: {
                /* Should be a Wireless i/f */
                g_assert( self->prv->ncu_type == NWAMUI_NCU_TYPE_WIRELESS );

                g_value_set_object( value, (gpointer)self->prv->wifi_info );
            }
            break;
        case PROP_RULES_ENABLED: {
                g_value_set_boolean( value, self->prv->rules_enabled );
            }
            break;

        case PROP_RULES_NCUS: {
                g_value_set_pointer( value, self->prv->rules_ncus );
            }
            break;

        case PROP_RULES_NCU_STATE: {
                g_value_set_int( value, (gint)self->prv->rules_ncu_state );
            }
            break;

        case PROP_RULES_ACTION: {
                g_value_set_int( value, (gint)self->prv->rules_action );
            }
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}


/**
 * nwamui_ncu_new:
 * @returns: a new #NwamuiNcu.
 *
 * Creates a new #NwamuiNcu with info initialised based on the argumens passed.
 **/
extern  NwamuiNcu*          
nwamui_ncu_new (    const gchar*        vanity_name,
                    const gchar*        device_name,
                    nwamui_ncu_type_t   ncu_type,
                    gboolean            active,
                    guint               speed,
                    gboolean            ipv4_auto_conf,
                    const gchar*        ipv4_address,
                    const gchar*        ipv4_subnet,
                    const gchar*        ipv4_gateway,
                    gboolean            ipv6_active,
                    gboolean            ipv6_auto_conf,
                    const gchar*        ipv6_address,
                    const gchar*        ipv6_prefix,
                    NwamuiWifiNet*      wifi_info )
{
    NwamuiNcu   *self = NWAMUI_NCU(g_object_new (NWAMUI_TYPE_NCU, NULL));

    g_object_set (  G_OBJECT (self),
                    "vanity_name", vanity_name,
                    "device_name", device_name,
                    "ncu_type", ncu_type,
                    "active", active,
                    "speed", speed,
                    "ipv4_auto_conf", ipv4_auto_conf,
                    "ipv4_address", ipv4_address,
                    "ipv4_subnet", ipv4_subnet,
                    "ipv4_gateway", ipv4_gateway,
                    "ipv6_active", ipv6_active,
                    "ipv6_auto_conf", ipv6_auto_conf,
                    "ipv6_address", ipv6_address,
                    "ipv6_prefix", ipv6_prefix,
                    NULL);

    if ( ncu_type == NWAMUI_NCU_TYPE_WIRELESS ) {
        g_object_set (  G_OBJECT (self),
                        "wifi_info", wifi_info,
                        NULL);
    }
    
    return( self );
}

/**
 * nwamui_ncu_get_vanity_name:
 * @returns: null-terminated C String with the vanity name of the the NCU.
 *
 **/
extern gchar*
nwamui_ncu_get_vanity_name ( NwamuiNcu *self )
{
    gchar*  name = NULL;
    
    g_return_val_if_fail (NWAMUI_IS_NCU(self), name); 
    
    g_object_get (G_OBJECT (self),
                  "vanity_name", &name,
                  NULL);

    return( name );
}

/**
 * nwamui_ncu_set_vanity_name:
 * @name: null-terminated C String with the vanity name of the the NCU.
 *
 **/
extern void
nwamui_ncu_set_vanity_name ( NwamuiNcu *self, const gchar* name )
{
    g_return_if_fail (NWAMUI_IS_NCU(self)); 
    
    g_assert (name != NULL );

    if ( name != NULL ) {
        g_object_set (G_OBJECT (self),
                      "vanity_name", name,
                      NULL);
    }
}

/**
 * nwamui_ncu_get_device_name:
 * @returns: null-terminated C String with the device name of the the NCU.
 *
 **/
extern gchar*
nwamui_ncu_get_device_name ( NwamuiNcu *self )
{
    gchar*  name = NULL;
    
    g_return_val_if_fail (NWAMUI_IS_NCU(self), name); 
    
    g_object_get (G_OBJECT (self),
                  "device_name", &name,
                  NULL);

    return( name );
}

/**
 * nwamui_ncu_set_device_name:
 * @name: null-terminated C String with the device name of the the NCU.
 *
 **/
extern void
nwamui_ncu_set_device_name ( NwamuiNcu *self, const gchar* name )
{
    g_return_if_fail (NWAMUI_IS_NCU(self)); 
    
    g_assert (name != NULL );

    if ( name != NULL ) {
        g_object_set (G_OBJECT (self),
                      "device_name", name,
                      NULL);
    }
}

/**
 * nwamui_ncu_get_display_name:
 * @returns: null-terminated C String with the device name of the the NCU.
 *
 * Use this to get a string of the format "<Vanity Name> (<device name>)"
 **/
extern gchar*               
nwamui_ncu_get_display_name ( NwamuiNcu *self )
{
    gchar*  dname = NULL;
    gchar*  vname = NULL;
    gchar*  disp_name = NULL;
    
    g_return_val_if_fail (NWAMUI_IS_NCU(self), dname); 
    
    g_object_get (G_OBJECT (self),
                  "vanity_name", &vname,
                  "device_name", &dname,
                  NULL);

    if ( vname != NULL & dname != NULL ) {
        disp_name = g_strdup_printf(_("%s (%s)"), vname, dname );
    }
    g_free( vname );
    g_free( dname );
    
    return( disp_name );
}

/** 
 * nwamui_ncu_set_ncu_type:
 * @nwamui_ncu: a #NwamuiNcu.
 * @ncu_type: Sets the NCU's type.
 * 
 **/ 
extern void                 
nwamui_ncu_set_ncu_type ( NwamuiNcu *self, nwamui_ncu_type_t ncu_type )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));
    
    g_assert( ncu_type >= NWAMUI_NCU_TYPE_WIRED && ncu_type < NWAMUI_NCU_TYPE_LAST );

    g_object_set (G_OBJECT (self),
                  "ncu_type", ncu_type,
                  NULL); 
}

/**
 * nwamui_ncu_get_ncu_type
 * @returns: the type of the NCU.
 *
 **/
extern nwamui_ncu_type_t    
nwamui_ncu_get_ncu_type ( NwamuiNcu *self )
{
    nwamui_ncu_type_t    ncu_type = FALSE; 
    
    g_return_val_if_fail (NWAMUI_IS_NCU(self), ncu_type); 
    
    g_object_get (G_OBJECT (self),
                  "ncu_type", &ncu_type,
                  NULL);

    return( ncu_type );
}

/**
 * nwamui_ncu_get_active:
 * @returns: active state.
 *
 **/
extern gboolean
nwamui_ncu_get_active ( NwamuiNcu *self )
{
    gboolean    active = FALSE; 
    
    g_return_val_if_fail (NWAMUI_IS_NCU(self), active); 
    
    g_object_get (G_OBJECT (self),
                  "active", &active,
                  NULL);

    return( active );
}

/** 
 * nwamui_ncu_set_active:
 * @nwamui_ncu: a #NwamuiNcu.
 * @active: Valiue to set active to.
 * 
 **/ 
extern void
nwamui_ncu_set_active (   NwamuiNcu *self,
                              gboolean        active )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));

    g_object_set (G_OBJECT (self),
                  "active", active,
                  NULL);
}

/** 
 * nwamui_ncu_set_speed:
 * @nwamui_ncu: a #NwamuiNcu.
 * @speed: Value to set speed to.
 * 
 **/ 
extern void
nwamui_ncu_set_speed (   NwamuiNcu *self,
                              guint        speed )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));
    g_assert (speed >= 0 && speed <= G_MAXUINT );

    g_object_set (G_OBJECT (self),
                  "speed", speed,
                  NULL);
}

/**
 * nwamui_ncu_get_speed:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the speed.
 *
 **/
extern guint
nwamui_ncu_get_speed (NwamuiNcu *self)
{
    guint  speed = 0; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), speed);

    g_object_get (G_OBJECT (self),
                  "speed", &speed,
                  NULL);

    return( speed );
}

/** 
 * nwamui_ncu_set_ipv4_auto_conf:
 * @nwamui_ncu: a #NwamuiNcu.
 * @ipv4_auto_conf: Valiue to set ipv4_auto_conf to.
 * 
 **/ 
extern void
nwamui_ncu_set_ipv4_auto_conf (   NwamuiNcu *self,
                              gboolean        ipv4_auto_conf )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));

    g_object_set (G_OBJECT (self),
                  "ipv4_auto_conf", ipv4_auto_conf,
                  NULL);
}

/**
 * nwamui_ncu_get_ipv4_auto_conf:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the ipv4_auto_conf.
 *
 **/
extern gboolean
nwamui_ncu_get_ipv4_auto_conf (NwamuiNcu *self)
{
    gboolean  ipv4_auto_conf = TRUE; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), ipv4_auto_conf);

    g_object_get (G_OBJECT (self),
                  "ipv4_auto_conf", &ipv4_auto_conf,
                  NULL);

    return( ipv4_auto_conf );
}

/** 
 * nwamui_ncu_set_ipv4_address:
 * @nwamui_ncu: a #NwamuiNcu.
 * @ipv4_address: Valiue to set ipv4_address to.
 * 
 **/ 
extern void
nwamui_ncu_set_ipv4_address (   NwamuiNcu *self,
                              const gchar*  ipv4_address )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));
    g_assert (ipv4_address != NULL );

    if ( ipv4_address != NULL ) {
        g_object_set (G_OBJECT (self),
                      "ipv4_address", ipv4_address,
                      NULL);
    }
}

/**
 * nwamui_ncu_get_ipv4_address:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the ipv4_address.
 *
 **/
extern gchar*
nwamui_ncu_get_ipv4_address (NwamuiNcu *self)
{
    gchar*  ipv4_address = NULL; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), ipv4_address);

    g_object_get (G_OBJECT (self),
                  "ipv4_address", &ipv4_address,
                  NULL);

    return( ipv4_address );
}

/** 
 * nwamui_ncu_set_ipv4_subnet:
 * @nwamui_ncu: a #NwamuiNcu.
 * @ipv4_subnet: Valiue to set ipv4_subnet to.
 * 
 **/ 
extern void
nwamui_ncu_set_ipv4_subnet (   NwamuiNcu *self,
                              const gchar*  ipv4_subnet )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));
    g_assert (ipv4_subnet != NULL );

    if ( ipv4_subnet != NULL ) {
        g_object_set (G_OBJECT (self),
                      "ipv4_subnet", ipv4_subnet,
                      NULL);
    }
}

/**
 * nwamui_ncu_get_ipv4_subnet:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the ipv4_subnet.
 *
 **/
extern gchar*
nwamui_ncu_get_ipv4_subnet (NwamuiNcu *self)
{
    gchar*  ipv4_subnet = NULL; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), ipv4_subnet);

    g_object_get (G_OBJECT (self),
                  "ipv4_subnet", &ipv4_subnet,
                  NULL);

    return( ipv4_subnet );
}

/** 
 * nwamui_ncu_set_ipv4_gateway:
 * @nwamui_ncu: a #NwamuiNcu.
 * @ipv4_gateway: Valiue to set ipv4_gateway to.
 * 
 **/ 
extern void
nwamui_ncu_set_ipv4_gateway (   NwamuiNcu *self,
                              const gchar*  ipv4_gateway )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));
    g_assert (ipv4_gateway != NULL );

    if ( ipv4_gateway != NULL ) {
        g_object_set (G_OBJECT (self),
                      "ipv4_gateway", ipv4_gateway,
                      NULL);
    }
}

/**
 * nwamui_ncu_get_ipv4_gateway:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the ipv4_gateway.
 *
 **/
extern gchar*
nwamui_ncu_get_ipv4_gateway (NwamuiNcu *self)
{
    gchar*  ipv4_gateway = NULL; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), ipv4_gateway);

    g_object_get (G_OBJECT (self),
                  "ipv4_gateway", &ipv4_gateway,
                  NULL);

    return( ipv4_gateway );
}

/** 
 * nwamui_ncu_set_ipv6_active:
 * @nwamui_ncu: a #NwamuiNcu.
 * @ipv6_active: Valiue to set ipv6_active to.
 * 
 **/ 
extern void
nwamui_ncu_set_ipv6_active (   NwamuiNcu *self,
                              gboolean        ipv6_active )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));

    g_object_set (G_OBJECT (self),
                  "ipv6_active", ipv6_active,
                  NULL);
}

/**
 * nwamui_ncu_get_ipv6_active:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the ipv6_active.
 *
 **/
extern gboolean
nwamui_ncu_get_ipv6_active (NwamuiNcu *self)
{
    gboolean  ipv6_active = FALSE; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), ipv6_active);

    g_object_get (G_OBJECT (self),
                  "ipv6_active", &ipv6_active,
                  NULL);

    return( ipv6_active );
}

/** 
 * nwamui_ncu_set_ipv6_auto_conf:
 * @nwamui_ncu: a #NwamuiNcu.
 * @ipv6_auto_conf: Valiue to set ipv6_auto_conf to.
 * 
 **/ 
extern void
nwamui_ncu_set_ipv6_auto_conf (   NwamuiNcu *self,
                              gboolean        ipv6_auto_conf )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));

    g_object_set (G_OBJECT (self),
                  "ipv6_auto_conf", ipv6_auto_conf,
                  NULL);
}

/**
 * nwamui_ncu_get_ipv6_auto_conf:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the ipv6_auto_conf.
 *
 **/
extern gboolean
nwamui_ncu_get_ipv6_auto_conf (NwamuiNcu *self)
{
    gboolean  ipv6_auto_conf = TRUE; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), ipv6_auto_conf);

    g_object_get (G_OBJECT (self),
                  "ipv6_auto_conf", &ipv6_auto_conf,
                  NULL);

    return( ipv6_auto_conf );
}

/** 
 * nwamui_ncu_set_ipv6_address:
 * @nwamui_ncu: a #NwamuiNcu.
 * @ipv6_address: Valiue to set ipv6_address to.
 * 
 **/ 
extern void
nwamui_ncu_set_ipv6_address (   NwamuiNcu *self,
                              const gchar*  ipv6_address )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));
    g_assert (ipv6_address != NULL );

    if ( ipv6_address != NULL ) {
        g_object_set (G_OBJECT (self),
                      "ipv6_address", ipv6_address,
                      NULL);
    }
}

/**
 * nwamui_ncu_get_ipv6_address:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the ipv6_address.
 *
 **/
extern gchar*
nwamui_ncu_get_ipv6_address (NwamuiNcu *self)
{
    gchar*  ipv6_address = NULL; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), ipv6_address);

    g_object_get (G_OBJECT (self),
                  "ipv6_address", &ipv6_address,
                  NULL);

    return( ipv6_address );
}

/** 
 * nwamui_ncu_set_ipv6_prefix:
 * @nwamui_ncu: a #NwamuiNcu.
 * @ipv6_prefix: Valiue to set ipv6_prefix to.
 * 
 **/ 
extern void
nwamui_ncu_set_ipv6_prefix (   NwamuiNcu *self,
                              const gchar*  ipv6_prefix )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));
    g_assert (ipv6_prefix != NULL );

    if ( ipv6_prefix != NULL ) {
        g_object_set (G_OBJECT (self),
                      "ipv6_prefix", ipv6_prefix,
                      NULL);
    }
}

/**
 * nwamui_ncu_get_ipv6_prefix:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the ipv6_prefix.
 *
 **/
extern gchar*
nwamui_ncu_get_ipv6_prefix (NwamuiNcu *self)
{
    gchar*  ipv6_prefix = NULL; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), ipv6_prefix);

    g_object_get (G_OBJECT (self),
                  "ipv6_prefix", &ipv6_prefix,
                  NULL);

    return( ipv6_prefix );
}

/**
 * nwamui_ncu_get_wifi_info:
 * @returns: #NwamuiWifiNet object
 *
 * For a wireless interface you can get the NwamuiWifiNet object that describes the
 * current wireless configuration.
 *
 * May return NULL if not configuration exists.
 *
 **/
extern NwamuiWifiNet*
nwamui_ncu_get_wifi_info ( NwamuiNcu *self )
{
    NwamuiWifiNet*  wifi_info = NULL;
    
    g_return_val_if_fail (NWAMUI_IS_NCU(self), wifi_info); 
    
    g_object_get (G_OBJECT (self),
                  "wifi_info", &wifi_info,
                  NULL);

    return( NWAMUI_WIFI_NET(wifi_info) );
}

/**
 * nwamui_ncu_set_wifi_info:
 * @wifi_info: #NwamuiWifiNet object
 *
 * For a wireless interface you can set the NwamuiWifiNet object that describes the
 * current wireless configuration.
 *
 **/
extern void
nwamui_ncu_set_wifi_info ( NwamuiNcu *self, NwamuiWifiNet* wifi_info )
{
    g_return_if_fail (NWAMUI_IS_NCU(self)); 

    g_assert( NWAMUI_IS_WIFI_NET( wifi_info ) );
    
    g_object_set (G_OBJECT (self),
                  "wifi_info", wifi_info,
                  NULL);
}

/**
 * nwamui_ncu_get_wifi_signal_strength:
 * @returns: signal strength
 *
 * For a wireless interface get the signal_strength
 *
 **/
extern nwamui_wifi_signal_strength_t
nwamui_ncu_get_wifi_signal_strength ( NwamuiNcu *self )
{
    nwamui_wifi_signal_strength_t   signal_strength = NWAMUI_WIFI_STRENGTH_NONE;
    
    g_return_val_if_fail (NWAMUI_IS_NCU(self), signal_strength); 
    
    if ( self->prv->wifi_info != NULL ) {
        signal_strength = nwamui_wifi_net_get_signal_strength(self->prv->wifi_info);
    }
    
    return( signal_strength );
}

/** 
 * nwamui_ncu_set_rules_enabled:
 * @nwamui_ncu: a #NwamuiNcu.
 * @rules_enabled: Value to set rules_enabled to.
 * 
 **/ 
extern void
nwamui_ncu_set_selection_rules_enabled (  NwamuiNcu*    self,
                                          gboolean      rules_enabled )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));

    g_object_set (G_OBJECT (self),
                  "rules_enabled", rules_enabled,
                  NULL);
}

/**
 * nwamui_ncu_get_rules_enabled:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the rules_enabled.
 *
 **/
extern gboolean
nwamui_ncu_get_selection_rules_enabled (NwamuiNcu *self)
{
    gboolean  rules_enabled = FALSE; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), rules_enabled);

    g_object_get (G_OBJECT (self),
                  "rules_enabled", &rules_enabled,
                  NULL);

    return( rules_enabled );
}

/** 
 * nwamui_ncu_set_selection_rule_ncus:
 * @nwamui_ncu: a #NwamuiNcu.
 * @selected_ncus: A GList of NCUs that are to be checked.
 * 
 **/ 
extern void
nwamui_ncu_set_selection_rule_ncus( NwamuiNcu*                   self,
                                    GList*                       selected_ncus )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));

    g_object_set (G_OBJECT (self),
              "rules_ncus", selected_ncus,
              NULL);
    
}

/** 
 * nwamui_ncu_set_selection_rule_ncus:
 * @nwamui_ncu: a #NwamuiNcu.
 * @selected_ncus: A GList of NCUs that are to be checked.
 * 
 **/ 
extern void
nwamui_ncu_selection_rule_ncus_add( NwamuiNcu*  self,
                                    NwamuiNcu*  ncu_to_add )
{
    GList*  selected_ncus;
    
    g_return_if_fail (NWAMUI_IS_NCU (self));

    g_object_get (G_OBJECT (self),
              "rules_ncus", &selected_ncus,
              NULL);
    
    if ( g_list_find( selected_ncus, (gpointer)ncu_to_add) == NULL ) {

        selected_ncus = g_list_insert(selected_ncus, (gpointer)g_object_ref(ncu_to_add), 0 );

        g_object_set (G_OBJECT (self),
                  "rules_ncus", selected_ncus,
                  NULL);
    }
}

extern void
nwamui_ncu_selection_rule_ncus_remove( NwamuiNcu*  self,
                                       NwamuiNcu*  ncu_to_remove )
{
    GList*  selected_ncus = NULL;
    GList*  node = NULL ;
    
    g_return_if_fail (NWAMUI_IS_NCU (self));

    g_object_get (G_OBJECT (self),
              "rules_ncus", &selected_ncus,
              NULL);

    if ( (node = g_list_find( selected_ncus, (gpointer)ncu_to_remove)) != NULL ) {

        selected_ncus = g_list_delete_link(selected_ncus, node );

        g_object_unref( ncu_to_remove );

        g_object_set (G_OBJECT (self),
                  "rules_ncus", selected_ncus,
                  NULL);
    }
    
}

extern gboolean
nwamui_ncu_selection_rule_ncus_contains( NwamuiNcu*  self,
                                         NwamuiNcu*  ncu_to_find )
{
    g_return_val_if_fail (NWAMUI_IS_NCU (self), FALSE );

    return( g_list_find( self->prv->rules_ncus, ncu_to_find) != NULL );
}



/** 
 * nwamui_ncu_set_selection_rule_state:
 * @nwamui_ncu: a #NwamuiNcu.
 * @ncu_state: The state to check NCUs for.
 * 
 **/ 
extern void
nwamui_ncu_set_selection_rule_state(  NwamuiNcu*                   self,
                                      nwamui_ncu_rule_state_t      ncu_state )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));

    g_object_set (G_OBJECT (self),
              "rules_ncu_state", ncu_state,
              NULL);
}

/** 
 * nwamui_ncu_set_selection_rule_action:
 * @nwamui_ncu: a #NwamuiNcu.
 * @action: The action to take if the selected_ncus are in given state.
 * 
 **/ 
extern void
nwamui_ncu_set_selection_rule_action(  NwamuiNcu*                   self,
                                       nwamui_ncu_rule_action_t     action )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));

    g_object_set (G_OBJECT (self),
              "rules_action", action,
              NULL);
    
}

/** 
 * nwamui_ncu_get_selection_rule:
 * @nwamui_ncu: a #NwamuiNcu.
 * @selected_ncus: A GList of NCUs that are to be checked.
 * @ncu_state: The state to check NCUs for.
 * @action: The action to take if the selected_ncus are in given state.
 * 
 * @returns: whether the selection rules are enabled or not.
 **/ 
extern gboolean
nwamui_ncu_get_selection_rule(  NwamuiNcu*                   self,
                                GList**                      selected_ncus,
                                nwamui_ncu_rule_state_t*     ncu_state,    
                                nwamui_ncu_rule_action_t*    action )
{
    gboolean  rules_enabled = FALSE; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), rules_enabled);

    g_object_get (G_OBJECT (self),
                  "rules_enabled", &rules_enabled,
                  "rules_ncus", selected_ncus,
                  "rules_ncu_state", ncu_state,
                  "rules_action", action,
                  NULL);

    return( rules_enabled );
}


static void
nwamui_ncu_finalize (NwamuiNcu *self)
{
    if (self->prv->rules_ncus != NULL ) {
        nwamui_util_free_obj_list(self->prv->rules_ncus);
    }

    g_free (self->prv); 
    self->prv = NULL;

    parent_class->finalize (G_OBJECT (self));
}

/* Callbacks */

static void
object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
    NwamuiNcu* self = NWAMUI_NCU(data);
    gchar*     name = self->prv->device_name;

    g_debug("NwamuiNcu: %s: notify %s changed\n", name?name:"", arg1->name);
}

