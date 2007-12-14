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
 * File:   nwamui_ip.c
 *
 */

#include <glib-object.h>
#include <glib/gi18n.h>
#include <strings.h>

#include "libnwamui.h"
#include "nwamui_ip.h"

static GObjectClass *parent_class = NULL;


struct _NwamuiIpPrivate {
    NwamuiNcu*                ncu_parent;
    gboolean                  is_v6;
    gchar*                    address;
    gchar*                    subnet_prefix;
};

static void nwamui_ip_set_property (    GObject         *object,
                                        guint            prop_id,
                                        const GValue    *value,
                                        GParamSpec      *pspec);

static void nwamui_ip_get_property (    GObject         *object,
                                        guint            prop_id,
                                        GValue          *value,
                                        GParamSpec      *pspec);

static void nwamui_ip_finalize (        NwamuiIp *self);


/* Callbacks */
static void object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);

enum {
    PROP_IS_V6 = 1,
    PROP_ADDRESS,
    PROP_SUBNET_PREFIX,
};

G_DEFINE_TYPE (NwamuiIp, nwamui_ip, G_TYPE_OBJECT)

static void
nwamui_ip_class_init (NwamuiIpClass *klass)
{
    /* Pointer to GObject Part of Class */
    GObjectClass *gobject_class = (GObjectClass*) klass;
        
    /* Initialise Static Parent Class pointer */
    parent_class = g_type_class_peek_parent (klass);

    /* Override Some Function Pointers */
    gobject_class->set_property = nwamui_ip_set_property;
    gobject_class->get_property = nwamui_ip_get_property;
    gobject_class->finalize = (void (*)(GObject*)) nwamui_ip_finalize;

    /* Create some properties */
    g_object_class_install_property (gobject_class,
                                     PROP_IS_V6,
                                     g_param_spec_boolean ("is_v6",
                                                          _("is_v6"),
                                                          _("is_v6"),
                                                          FALSE,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_ADDRESS,
                                     g_param_spec_string ("address",
                                                          _("address"),
                                                          _("address"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_SUBNET_PREFIX,
                                     g_param_spec_string ("subnet_prefix",
                                                          _("subnet_prefix"),
                                                          _("subnet_prefix"),
                                                          "",
                                                          G_PARAM_READWRITE));
}


static void
nwamui_ip_init (NwamuiIp *self)
{
    NwamuiIpPrivate *prv = (NwamuiIpPrivate*)g_new0 (NwamuiIpPrivate, 1);

    prv->ncu_parent = NULL;
    prv->is_v6 = FALSE;
    prv->address = NULL;
    prv->subnet_prefix = NULL;

    self->prv = prv;
    
    g_signal_connect(G_OBJECT(self), "notify", (GCallback)object_notify_cb, (gpointer)self);
}

static void
nwamui_ip_set_property (    GObject         *object,
                            guint            prop_id,
                            const GValue    *value,
                            GParamSpec      *pspec)
{
    NwamuiIp          *self = NWAMUI_IP(object);
    NwamuiIpPrivate   *prv;

    g_return_if_fail( self != NULL );
    
    prv = self->prv;
    g_assert( prv != NULL );
    
    switch (prop_id) {
        case PROP_IS_V6: {
                self->prv->is_v6 = g_value_get_boolean( value );
            }
            break;

        case PROP_ADDRESS: {
                if ( self->prv->address != NULL ) {
                        g_free( self->prv->address );
                }
                self->prv->address = g_strdup( g_value_get_string( value ) );
            }
            break;

        case PROP_SUBNET_PREFIX: {
                if ( self->prv->subnet_prefix != NULL ) {
                        g_free( self->prv->subnet_prefix );
                }
                self->prv->subnet_prefix = g_strdup( g_value_get_string( value ) );
            }
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
nwamui_ip_get_property (GObject         *object,
                                  guint            prop_id,
                                  GValue          *value,
                                  GParamSpec      *pspec)
{
    NwamuiIp *self = NWAMUI_IP(object);
    NwamuiIpPrivate   *prv;

    g_return_if_fail( self != NULL );
    
    prv = self->prv;
    g_assert( prv != NULL );

    switch (prop_id) {
        case PROP_IS_V6: {
                g_value_set_boolean( value, self->prv->is_v6 );
            }
            break;

        case PROP_ADDRESS: {
                g_value_set_string( value, self->prv->address );
            }
            break;

        case PROP_SUBNET_PREFIX: {
                g_value_set_string( value, self->prv->subnet_prefix );
            }
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
nwamui_ip_finalize (NwamuiIp *self)
{
    if ( self->prv ) {
        if (self->prv->address != NULL ) {
            g_free( self->prv->address );
        }

        if (self->prv->subnet_prefix != NULL ) {
            g_free( self->prv->subnet_prefix );
        }
    }
    self->prv = NULL;

    parent_class->finalize (G_OBJECT (self));
}

/* Exported Functions */

/**
 * nwamui_ip_new:
 *
 * @addr: a C string containing the IP address
 * @subnet_prefix: a C string containing the address subnet/prefix
 * @is_v6: TRUE if the addr is an IPv6 Address
 * 
 * @returns: a new #NwamuiIp.
 *
 * Creates a new #NwamuiIp.
 **/
extern  NwamuiIp*
nwamui_ip_new(  NwamuiNcu*      ncu_parent,
                const gchar*    addr, 
                const gchar*    subnet_prefix, 
                gboolean        is_v6 )
{
    NwamuiIp*  self = NULL;
    
    g_assert( NWAMUI_IS_NCU( ncu_parent ) );

    self = NWAMUI_IP(g_object_new (NWAMUI_TYPE_IP, NULL));

    g_object_set (G_OBJECT (self),
                    "address", addr,
                    "subnet_prefix", subnet_prefix,
                    "is_v6", is_v6,
                    NULL);
    
    self->prv->ncu_parent = ncu_parent;
    
    return( self );
}

/** 
 * nwamui_ip_set_v6:
 * @nwamui_ip: a #NwamuiIp.
 * @is_v6: Value to set is_v6 to.
 * 
 **/ 
extern void
nwamui_ip_set_v6 (   NwamuiIp *self,
                     gboolean        is_v6 )
{
    g_return_if_fail (NWAMUI_IS_IP (self));

    g_object_set (G_OBJECT (self),
                  "is_v6", is_v6,
                  NULL);
}

/**
 * nwamui_ip_is_v6:
 * @nwamui_ip: a #NwamuiIp.
 * @returns: the whether the address is a an IPv6 addr.
 *
 **/
extern gboolean
nwamui_ip_is_v6 (NwamuiIp *self)
{
    gboolean  is_v6 = FALSE; 

    g_return_val_if_fail (NWAMUI_IS_IP (self), is_v6);

    g_object_get (G_OBJECT (self),
                  "is_v6", &is_v6,
                  NULL);

    return( is_v6 );
}

/** 
 * nwamui_ip_set_address:
 * @nwamui_ip: a #NwamuiIp.
 * @address: Value to set address to.
 * 
 **/ 
extern void
nwamui_ip_set_address (   NwamuiIp *self,
                              const gchar*  address )
{
    g_return_if_fail (NWAMUI_IS_IP (self));
    g_assert (address != NULL );

    if ( address != NULL ) {
        g_object_set (G_OBJECT (self),
                      "address", address,
                      NULL);
    }
}

/**
 * nwamui_ip_get_address:
 * @nwamui_ip: a #NwamuiIp.
 * @returns: the address.
 *
 **/
extern gchar*
nwamui_ip_get_address (NwamuiIp *self)
{
    gchar*  address = NULL; 

    g_return_val_if_fail (NWAMUI_IS_IP (self), address);

    g_object_get (G_OBJECT (self),
                  "address", &address,
                  NULL);

    return( address );
}

/** 
 * nwamui_ip_set_subnet_prefix:
 * @nwamui_ip: a #NwamuiIp.
 * @subnet_prefix: Value to set subnet_prefix to.
 * 
 **/ 
extern void
nwamui_ip_set_subnet_prefix (   NwamuiIp *self,
                              const gchar*  subnet_prefix )
{
    g_return_if_fail (NWAMUI_IS_IP (self));
    g_assert (subnet_prefix != NULL );

    if ( subnet_prefix != NULL ) {
        g_object_set (G_OBJECT (self),
                      "subnet_prefix", subnet_prefix,
                      NULL);
    }
}

/**
 * nwamui_ip_get_subnet_prefix:
 * @nwamui_ip: a #NwamuiIp.
 * @returns: the subnet_prefix.
 *
 **/
extern gchar*
nwamui_ip_get_subnet_prefix (NwamuiIp *self)
{
    gchar*  subnet_prefix = NULL; 

    g_return_val_if_fail (NWAMUI_IS_IP (self), subnet_prefix);

    g_object_get (G_OBJECT (self),
                  "subnet_prefix", &subnet_prefix,
                  NULL);

    return( subnet_prefix );
}

/* Callbacks */

static void
object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
    NwamuiIp* self = NWAMUI_IP(data);

    g_debug("NwamuiIp: notify %s changed\n", arg1->name);

    if ( self->prv->ncu_parent != NULL ) {
        g_object_notify(G_OBJECT(self->prv->ncu_parent), (gpointer)(self->prv->is_v6?"v6addresses":"v4addresses") );
    }
}

