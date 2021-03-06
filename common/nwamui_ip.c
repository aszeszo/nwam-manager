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
 * File:   nwamui_ip.c
 *
 */

#include <glib-object.h>
#include <glib/gi18n.h>
#include <strings.h>

#include "libnwamui.h"
#include "nwamui_ip.h"

#define ADDRSRC_NONE       ( 0 )
#define ADDRSRC_DHCP       ( 1 )
#define ADDRSRC_AUTOCONF   ( 1 << 1 )
#define ADDRSRC_STATIC     ( 1 << 2 )


struct _NwamuiIpPrivate {
    NwamuiNcu*                ncu_parent;
    gchar*                    ncu_device;
    gboolean                  is_v6;
    gchar*                    address;
    gint                      addr_src;
    gchar*                    subnet_prefix;
};

#define NWAMUI_IP_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE((o), NWAMUI_TYPE_IP, NwamuiIpPrivate))

static void nwamui_ip_set_property (    GObject         *object,
                                        guint            prop_id,
                                        const GValue    *value,
                                        GParamSpec      *pspec);

static void nwamui_ip_get_property (    GObject         *object,
                                        guint            prop_id,
                                        GValue          *value,
                                        GParamSpec      *pspec);

static void nwamui_ip_finalize (        NwamuiIp *self);

static gint         nwamui_object_real_sort(NwamuiObject *object, NwamuiObject *other, guint sort_by);

/* Callbacks */
static void object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);

enum {
    PROP_IS_V6 = 1,
    PROP_ADDRESS,
    PROP_SUBNET_PREFIX,
    PROP_IS_DHCP,
    PROP_IS_AUTOCONF,
};

G_DEFINE_TYPE (NwamuiIp, nwamui_ip, NWAMUI_TYPE_OBJECT)

static void
nwamui_ip_class_init (NwamuiIpClass *klass)
{
    /* Pointer to GObject Part of Class */
    GObjectClass *gobject_class = (GObjectClass*) klass;
    NwamuiObjectClass *nwamuiobject_class = NWAMUI_OBJECT_CLASS(klass);
        
    /* Override Some Function Pointers */
    gobject_class->set_property = nwamui_ip_set_property;
    gobject_class->get_property = nwamui_ip_get_property;
    gobject_class->finalize = (void (*)(GObject*)) nwamui_ip_finalize;

    nwamuiobject_class->sort = nwamui_object_real_sort;

	g_type_class_add_private(klass, sizeof(NwamuiIpPrivate));

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

    g_object_class_install_property (gobject_class,
                                     PROP_IS_DHCP,
                                     g_param_spec_boolean ("is_dhcp",
                                                          _("is_dhcp"),
                                                          _("is_dhcp"),
                                                          FALSE,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_IS_AUTOCONF,
                                     g_param_spec_boolean ("is_autoconf",
                                                          _("is_autoconf"),
                                                          _("is_autoconf"),
                                                          FALSE,
                                                          G_PARAM_READWRITE));

}


static void
nwamui_ip_init (NwamuiIp *self)
{
    NwamuiIpPrivate *prv      = NWAMUI_IP_GET_PRIVATE(self);
    self->prv = prv;

    prv->ncu_parent = NULL;
    prv->ncu_device = NULL;
    prv->is_v6 = FALSE;
    prv->address = NULL;
    prv->subnet_prefix = NULL;
    prv->addr_src = ADDRSRC_NONE;

    g_signal_connect(G_OBJECT(self), "notify", (GCallback)object_notify_cb, (gpointer)self);
}

static void
nwamui_ip_set_property (    GObject         *object,
                            guint            prop_id,
                            const GValue    *value,
                            GParamSpec      *pspec)
{
    NwamuiIp        *self = NWAMUI_IP(object);
    NwamuiIpPrivate *prv  = NWAMUI_IP_GET_PRIVATE(object);
    
    switch (prop_id) {
        case PROP_IS_V6: {
                prv->is_v6 = g_value_get_boolean( value );
            }
            break;

        case PROP_ADDRESS: {
            if ( prv->address != NULL ) {
                g_free( prv->address );
            }
            prv->address = g_strdup( g_value_get_string( value ) );
        }
            break;

        case PROP_SUBNET_PREFIX: {
            if ( prv->subnet_prefix != NULL ) {
                g_free( prv->subnet_prefix );
            }
            prv->subnet_prefix = g_strdup( g_value_get_string( value ) );
        }
            break;

        case PROP_IS_DHCP: {
                if ( g_value_get_boolean( value ) ) {
                    prv->addr_src = ADDRSRC_DHCP; 
                }
                else {
                    prv->addr_src = ADDRSRC_STATIC; 
                }
            }
            break;

        case PROP_IS_AUTOCONF: {
                if ( g_value_get_boolean( value ) ) {
                    prv->addr_src = ADDRSRC_AUTOCONF; 
                }
                else {
                    prv->addr_src &= ~ADDRSRC_AUTOCONF; 
                }
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
    NwamuiIp        *self = NWAMUI_IP(object);
    NwamuiIpPrivate *prv  = NWAMUI_IP_GET_PRIVATE(object);

    switch (prop_id) {
        case PROP_IS_V6: {
                g_value_set_boolean( value, prv->is_v6 );
            }
            break;

        case PROP_ADDRESS:
            g_value_set_string( value, prv->address );
            break;

        case PROP_SUBNET_PREFIX:
            g_value_set_string( value, prv->subnet_prefix );
            break;
        case PROP_IS_DHCP: {
                g_value_set_boolean( value, (prv->addr_src & ADDRSRC_DHCP)?TRUE:FALSE );
            }
            break;

        case PROP_IS_AUTOCONF: {
                g_value_set_boolean( value, (prv->addr_src & ADDRSRC_AUTOCONF)?TRUE:FALSE );
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
        if (self->prv->ncu_device != NULL ) {
            g_free( self->prv->ncu_device );
        }

        if (self->prv->address != NULL ) {
            g_free( self->prv->address );
        }

        if (self->prv->subnet_prefix != NULL ) {
            g_free( self->prv->subnet_prefix );
        }
    }
    self->prv = NULL;

	G_OBJECT_CLASS(nwamui_ip_parent_class)->finalize(G_OBJECT(self));
}

static gint
nwamui_object_real_sort(NwamuiObject *object, NwamuiObject *other, guint sort_by)
{
    NwamuiIpPrivate   *prv[2] = {
        NWAMUI_IP_GET_PRIVATE(object),
        NWAMUI_IP_GET_PRIVATE(other)
    };

    switch (sort_by) {
    case NWAMUI_OBJECT_SORT_BY_NAME:
        return g_strcmp0(prv[0]->address, prv[1]->address);
    default:
        g_warning("NwamuiObject::real_sort id '%d' not implemented for `%s'", sort_by, g_type_name(G_TYPE_FROM_INSTANCE(object)));
        break;
    }
}

/* Exported Functions */

/**
 * nwamui_ip_new:
 *
 * @addr: a C string containing the IP address
 * @subnet_prefix: a C string containing the address subnet/prefix
 * @is_v6: TRUE if the addr is an IPv6 Address
 * @is_dhcp: TRUE if the addr is allocated by DHCP
 * @is_autoconf: TRUE if the addr is a Link Local Address
 * @is_static: TRUE if the addr is an statically configured
 * 
 * @returns: a new #NwamuiIp.
 *
 * Creates a new #NwamuiIp.
 **/
extern  NwamuiIp*
nwamui_ip_new(  NwamuiNcu*      ncu_parent,
                const gchar*    addr, 
                const gchar*    subnet_prefix, 
                gboolean        is_v6,
                gboolean        is_dhcp,
                gboolean        is_autoconf)
{
    NwamuiIp*  self = NULL;
    
    self = g_object_new(NWAMUI_TYPE_IP, NULL);

    g_object_set (G_OBJECT (self),
                    "address", addr,
                    "subnet_prefix", subnet_prefix,
                    "is_v6", is_v6,
                    "is_dhcp", is_dhcp,
                    "is_autoconf", is_autoconf,
                    NULL);
    

    if (ncu_parent) {
        g_assert(NWAMUI_IS_NCU(ncu_parent));

        self->prv->ncu_parent = ncu_parent;
        self->prv->ncu_device = nwamui_ncu_get_device_name( self->prv->ncu_parent );
    }
    
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
    g_return_val_if_fail (NWAMUI_IS_IP (self), FALSE);

    return( self->prv->is_v6 );
}

/** 
 * nwamui_ip_set_dhcp:
 * @nwamui_ip: a #NwamuiIp.
 * @is_dhcp: Value to set is_dhcp to.
 * 
 **/ 
extern void
nwamui_ip_set_dhcp (   NwamuiIp *self,
                     gboolean        is_dhcp )
{
    g_return_if_fail (NWAMUI_IS_IP (self));

    g_object_set (G_OBJECT (self),
                  "is_dhcp", is_dhcp,
                  NULL);
}

/**
 * nwamui_ip_is_dhcp:
 * @nwamui_ip: a #NwamuiIp.
 * @returns: the whether the address is a an IPdhcp addr.
 *
 **/
extern gboolean
nwamui_ip_is_dhcp (NwamuiIp *self)
{
    g_return_val_if_fail (NWAMUI_IS_IP (self), FALSE);

    return( (self->prv->addr_src & ADDRSRC_DHCP)?TRUE:FALSE );
}

/** 
 * nwamui_ip_set_autoconf:
 * @nwamui_ip: a #NwamuiIp.
 * @is_autoconf: Value to set is_autoconf to.
 * 
 **/ 
extern void
nwamui_ip_set_autoconf (   NwamuiIp *self,
                     gboolean        is_autoconf )
{
    g_return_if_fail (NWAMUI_IS_IP (self));

    g_object_set (G_OBJECT (self),
                  "is_autoconf", is_autoconf,
                  NULL);
}

/**
 * nwamui_ip_is_autoconf:
 * @nwamui_ip: a #NwamuiIp.
 * @returns: the whether the address is a an IPautoconf addr.
 *
 **/
extern gboolean
nwamui_ip_is_autoconf (NwamuiIp *self)
{
    g_return_val_if_fail (NWAMUI_IS_IP (self), FALSE);

    return( (self->prv->addr_src & ADDRSRC_AUTOCONF)?TRUE:FALSE );
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
nwamui_ip_set_subnet_prefix(NwamuiIp *self, const gchar*  subnet_prefix)
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

extern gchar*               
nwamui_ip_get_display_name(NwamuiObject *obj)
{
    NwamuiIp        *ip        = NWAMUI_IP(obj);
    NwamuiIpPrivate *prv       = NWAMUI_IP_GET_PRIVATE(obj);
    const gchar     *addr_prefix;
    gchar*           dhcp_str;
	gchar           *string    = NULL;
	gchar           *address   = NULL;
	gchar           *netmask   = NULL;
	int              prefixlen = 0;

    if (prv->is_v6) {
        addr_prefix = _("Address(v6)");
    } else {
        addr_prefix = _("Address");
    }

    address = nwamui_ip_get_address(ip);
    netmask = nwamui_ip_get_subnet_prefix(ip);
    prefixlen = nwamui_util_convert_netmask_str_to_prefixlen(prv->is_v6?AF_INET6:AF_INET, netmask);
    g_free(netmask);

    if (nwamui_ip_is_dhcp(ip)) {
        dhcp_str = _(" (DHCP)");
    } else {
        dhcp_str = "";
    }

    string = g_strdup_printf(_("%s: %s/%d%s"), addr_prefix, address, prefixlen, dhcp_str);

    g_free( address );

    return string;
}

/* Callbacks */

static void
object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
    NwamuiIp* self = NWAMUI_IP(data);

    if ( self->prv->ncu_parent != NULL ) {
        g_object_notify(G_OBJECT(self->prv->ncu_parent), (gpointer)(self->prv->is_v6?"v6addresses":"v4addresses") );
    }
}

