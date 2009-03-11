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
 * File:   nwamui_ncu.c
 *
 */

#include <glib-object.h>
#include <glib/gi18n.h>
#include <gtk/gtkliststore.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <kstat.h>

#include "libnwamui.h"
#include "nwamui_ncu.h"

#include <errno.h>
#include <inetcfg.h>
#include <limits.h>
#include <sys/dlpi.h>
#include <libdllink.h>
#include <libdlwlan.h>

static GObjectClass *parent_class = NULL;

struct _NwamuiNcuPrivate {
        gboolean                        initialisation;

        NwamuiNcp*                      ncp;  /* Parent NCP */
        nwam_ncu_handle_t               nwam_ncu_phys;
        gboolean                        nwam_ncu_phys_modified;
        nwam_ncu_handle_t               nwam_ncu_ip;
        gboolean                        nwam_ncu_ip_modified;
        nwam_ncu_handle_t               nwam_ncu_iptun;
        gboolean                        nwam_ncu_iptun_modified;

        gboolean                        active;

        /* General Properties */
        gchar*                          vanity_name;
        gchar*                          device_name;
        nwamui_ncu_type_t               ncu_type;

        /* IPTun Properties */
        nwam_iptun_type_t               tun_type; 
        gchar*                          tun_tsrc;
        gchar*                          tun_tdst;
        gchar*                          tun_encr;
        gchar*                          tun_encr_auth;
        gchar*                          tun_auth;

        NwamuiIp*                       ipv4_primary_ip;
        gboolean                        ipv6_active;
        NwamuiIp*                       ipv6_primary_ip;
        GtkListStore*                   v4addresses;
        GtkListStore*                   v6addresses;

        /* Wireless Info */
        NwamuiWifiNet*                  wifi_info;
};

enum {
        PROP_NCP = 1,
        PROP_VANITY_NAME,
        PROP_DEVICE_NAME,
        PROP_AUTO_PUSH,
        PROP_PHY_ADDRESS,
        PROP_NCU_TYPE,
        PROP_ACTIVE,
        PROP_READONLY,
        PROP_SPEED,
        PROP_MTU,
        PROP_IPV4_DHCP,
        PROP_IPV4_AUTO_CONF,
        PROP_IPV4_ADDRESS,
        PROP_IPV4_SUBNET,
        PROP_IPV6_ACTIVE,
        PROP_IPV6_DHCP,
        PROP_IPV6_AUTO_CONF,
        PROP_IPV6_ADDRESS,
        PROP_IPV6_PREFIX,
        PROP_V4ADDRESSES,
        PROP_V6ADDRESSES,
        PROP_WIFI_INFO,
        PROP_RULES_ENABLED,
        PROP_ACTIVATION_MODE,
        PROP_ENABLED,
        PROP_PRIORITY_GROUP,
        PROP_PRIORITY_GROUP_MODE
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

static gboolean     get_nwam_ncu_boolean_prop( nwam_ncu_handle_t ncu, const char* prop_name );
static gboolean     set_nwam_ncu_boolean_prop( nwam_ncu_handle_t ncu, const char* prop_name, gboolean bool_value );

static gchar*       get_nwam_ncu_string_prop( nwam_ncu_handle_t ncu, const char* prop_name );
static gboolean     set_nwam_ncu_string_prop( nwam_ncu_handle_t ncu, const char* prop_name, const char* str );

static gchar**      get_nwam_ncu_string_array_prop( nwam_ncu_handle_t ncu, const char* prop_name );
static gboolean     set_nwam_ncu_string_array_prop( nwam_ncu_handle_t ncu, const char* prop_name, char** strs, guint len);

static guint64      get_nwam_ncu_uint64_prop( nwam_ncu_handle_t ncu, const char* prop_name );
static gboolean     set_nwam_ncu_uint64_prop( nwam_ncu_handle_t ncu, const char* prop_name, guint64 value );

static guint64*     get_nwam_ncu_uint64_array_prop( nwam_ncu_handle_t ncu, const char* prop_name , guint* out_num );
static gboolean     set_nwam_ncu_uint64_array_prop( nwam_ncu_handle_t ncu, const char* prop_name , 
                                                    const guint64* value, guint len );

static gboolean     get_kstat_uint64 (const gchar *device, const gchar* stat_name, uint64_t *rval );

static gboolean     interface_has_addresses(const char *ifname, sa_family_t family);

static gchar*       get_interface_address_str(const char *ifname, sa_family_t family);

/* Callbacks */
static void object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);

static void ip_row_inserted_or_changed_cb (GtkTreeModel *tree_model, GtkTreePath *path, GtkTreeIter *iter, gpointer user_data); 

static void ip_row_deleted_cb (GtkTreeModel *tree_model, GtkTreePath *path, gpointer user_data);


G_DEFINE_TYPE (NwamuiNcu, nwamui_ncu, NWAMUI_TYPE_OBJECT)

static void
nwamui_ncu_class_init (NwamuiNcuClass *klass)
{
    /* Pointer to GObject Part of Class */
    GObjectClass *gobject_class = (GObjectClass*) klass;
    NwamuiObjectClass *nwamuiobject_class = NWAMUI_OBJECT_CLASS(klass);
        
    /* Initialise Static Parent Class pointer */
    parent_class = g_type_class_peek_parent (klass);

    /* Override Some Function Pointers */
    gobject_class->set_property = nwamui_ncu_set_property;
    gobject_class->get_property = nwamui_ncu_get_property;
    gobject_class->finalize = (void (*)(GObject*)) nwamui_ncu_finalize;

    nwamuiobject_class->get_name = (nwamui_object_get_name_func_t)nwamui_ncu_get_vanity_name;
    nwamuiobject_class->set_name = (nwamui_object_set_name_func_t)nwamui_ncu_set_vanity_name;
    nwamuiobject_class->get_conditions = (nwamui_object_get_conditions_func_t)nwamui_ncu_get_selection_conditions;
    nwamuiobject_class->set_conditions = (nwamui_object_set_conditions_func_t)nwamui_ncu_set_selection_conditions;
    nwamuiobject_class->get_activation_mode = (nwamui_object_get_activation_mode_func_t)nwamui_ncu_get_activation_mode;
    nwamuiobject_class->set_activation_mode = (nwamui_object_set_activation_mode_func_t)nwamui_ncu_set_activation_mode;
    nwamuiobject_class->get_active = (nwamui_object_get_active_func_t)nwamui_ncu_get_active;
    nwamuiobject_class->set_active = (nwamui_object_set_active_func_t)nwamui_ncu_set_active;
    nwamuiobject_class->commit = (nwamui_object_commit_func_t)nwamui_ncu_commit;
    nwamuiobject_class->reload = (nwamui_object_reload_func_t)nwamui_ncu_reload;

    /* Create some properties */
    g_object_class_install_property (gobject_class,
                                     PROP_NCP,
                                     g_param_spec_object ("ncp",
                                                          _("NCP that NCU child of"),
                                                          _("NCP that NCU child of"),
                                                          NWAMUI_TYPE_NCP,
                                                          G_PARAM_READWRITE));

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
                                     PROP_PHY_ADDRESS,
                                     g_param_spec_string ("phy_address",
                                                          _("phy_address"),
                                                          _("phy_address"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_AUTO_PUSH,
                                     g_param_spec_pointer ("autopush",
                                                          _("autopush"),
                                                          _("autopush"),
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
                                                       G_PARAM_READABLE));

    g_object_class_install_property (gobject_class,
                                     PROP_MTU,
                                     g_param_spec_uint ("mtu",
                                                       _("mtu"),
                                                       _("mtu"),
                                                       0,
                                                       G_MAXUINT,
                                                       0,
                                                       G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_IPV4_DHCP,
                                     g_param_spec_boolean ("ipv4_dhcp",
                                                          _("ipv4_dhcp"),
                                                          _("ipv4_dhcp"),
                                                          FALSE,
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
                                     PROP_IPV6_ACTIVE,
                                     g_param_spec_boolean ("ipv6_active",
                                                           _("ipv6_active"),
                                                           _("ipv6_active"),
                                                          FALSE,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_IPV6_DHCP,
                                     g_param_spec_boolean("ipv6_dhcp",
                                                          _("ipv6_dhcp"),
                                                          _("ipv6_dhcp"),
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
                                     PROP_V4ADDRESSES,
                                     g_param_spec_object ("v4addresses",
                                                          _("v4addresses"),
                                                          _("v4addresses"),
                                                          GTK_TYPE_LIST_STORE,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_V6ADDRESSES,
                                     g_param_spec_object ("v6addresses",
                                                          _("v6addresses"),
                                                          _("v6addresses"),
                                                          GTK_TYPE_LIST_STORE,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_WIFI_INFO,
                                     g_param_spec_object ("wifi_info",
                                                          _("Wi-Fi configuration information."),
                                                          _("Wi-Fi configuration information."),
                                                          NWAMUI_TYPE_WIFI_NET,
                                                          G_PARAM_READWRITE));
    
    g_object_class_install_property (gobject_class,
                                     PROP_ACTIVATION_MODE,
                                     g_param_spec_int ("activation_mode",
                                                       _("activation_mode"),
                                                       _("activation_mode"),
                                                       NWAMUI_COND_ACTIVATION_MODE_MANUAL,
                                                       NWAMUI_COND_ACTIVATION_MODE_LAST,
                                                       NWAMUI_COND_ACTIVATION_MODE_MANUAL,
                                                       G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_ENABLED,
                                     g_param_spec_boolean ("enabled",
                                                          _("enabled"),
                                                          _("enabled"),
                                                          FALSE,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_PRIORITY_GROUP,
                                     g_param_spec_uint ("priority_group",
                                                       _("priority_group"),
                                                       _("priority_group"),
                                                       0,
                                                       G_MAXUINT,
                                                       0,
                                                       G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_PRIORITY_GROUP_MODE,
                                     g_param_spec_int ("priority_group_mode",
                                                       _("priority_group_mode"),
                                                       _("priority_group_mode"),
                                                       NWAMUI_COND_PRIORITY_GROUP_MODE_EXCLUSIVE,
                                                       NWAMUI_COND_PRIORITY_GROUP_MODE_LAST,
                                                       NWAMUI_COND_PRIORITY_GROUP_MODE_EXCLUSIVE,
                                                       G_PARAM_READWRITE));

}


static void
nwamui_ncu_init (NwamuiNcu *self)
{
    self->prv = g_new0 (NwamuiNcuPrivate, 1);

    self->prv->initialisation = TRUE;

    self->prv->nwam_ncu_phys = NULL;
    self->prv->nwam_ncu_ip = NULL;
    self->prv->nwam_ncu_iptun = NULL;

    self->prv->nwam_ncu_phys_modified = FALSE;
    self->prv->nwam_ncu_ip_modified = FALSE;
    self->prv->nwam_ncu_iptun_modified = FALSE;

    self->prv->vanity_name = NULL;
    self->prv->device_name = NULL;
    self->prv->ncu_type = NWAMUI_NCU_TYPE_WIRED;
    self->prv->active = FALSE;
    self->prv->ipv4_primary_ip = NULL;
    self->prv->ipv6_active = FALSE;
    self->prv->ipv6_primary_ip = NULL;
    self->prv->v4addresses = gtk_list_store_new ( 1, NWAMUI_TYPE_IP);
    self->prv->v6addresses = gtk_list_store_new ( 1, NWAMUI_TYPE_IP);
    self->prv->wifi_info = NULL;
    
    g_signal_connect(G_OBJECT(self), "notify", (GCallback)object_notify_cb, (gpointer)self);
    g_signal_connect(G_OBJECT(self->prv->v4addresses), "row-deleted", (GCallback)ip_row_deleted_cb, (gpointer)self);
    g_signal_connect(G_OBJECT(self->prv->v4addresses), "row-changed", (GCallback)ip_row_inserted_or_changed_cb, (gpointer)self);
    g_signal_connect(G_OBJECT(self->prv->v4addresses), "row-inserted", (GCallback)ip_row_inserted_or_changed_cb, (gpointer)self);
    g_signal_connect(G_OBJECT(self->prv->v6addresses), "row-deleted", (GCallback)ip_row_deleted_cb, (gpointer)self);
    g_signal_connect(G_OBJECT(self->prv->v6addresses), "row-changed", (GCallback)ip_row_inserted_or_changed_cb, (gpointer)self);
    g_signal_connect(G_OBJECT(self->prv->v6addresses), "row-inserted", (GCallback)ip_row_inserted_or_changed_cb, (gpointer)self);
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

    gboolean     read_only = FALSE;

    if ( !self->prv->initialisation ) {
        read_only = !nwamui_ncu_is_modifiable(self);

        if ( read_only ) {
            g_error("Attempting to modify read-only ncu %s", self->prv->device_name?self->prv->device_name:"NULL");
            return;
        }
    }

    switch (prop_id) {
    case PROP_NCP: {
            NwamuiNcp* ncp = NWAMUI_NCP( g_value_dup_object( value ) );

            if ( self->prv->ncp != NULL ) {
                    g_object_unref( self->prv->ncp );
            }
            self->prv->ncp = ncp;
        }
        break;

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
        case PROP_PHY_ADDRESS: {
                const gchar* mac_addr = g_strdup( g_value_get_string( value ) );
                set_nwam_ncu_string_prop( self->prv->nwam_ncu_phys, NWAM_NCU_PROP_LINK_MAC_ADDR, mac_addr );
                self->prv->nwam_ncu_phys_modified = TRUE;
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
        case PROP_MTU: {
                guint64 mtu = g_value_get_uint( value );
                set_nwam_ncu_uint64_prop( self->prv->nwam_ncu_phys, NWAM_NCU_PROP_LINK_MTU, mtu );
                self->prv->nwam_ncu_phys_modified = TRUE;
            }
            break;
        case PROP_IPV4_DHCP: {
                gboolean dhcp = g_value_get_boolean( value );

                if ( self->prv->ipv4_primary_ip != NULL ) {
                    nwamui_ip_set_dhcp(self->prv->ipv4_primary_ip, dhcp );
                }
                else {
                    NwamuiIp*   ip = nwamui_ip_new( self, "", "", FALSE,
                                                    dhcp, FALSE, FALSE );
                    GtkTreeIter iter;

                    g_signal_handlers_block_by_func(G_OBJECT(self->prv->v4addresses), (gpointer)ip_row_inserted_or_changed_cb, (gpointer)self);
                    gtk_list_store_insert(self->prv->v4addresses, &iter, 0 );
                    gtk_list_store_set(self->prv->v4addresses, &iter, 0, ip, -1 );
                    g_signal_handlers_unblock_by_func(G_OBJECT(self->prv->v4addresses), (gpointer)ip_row_inserted_or_changed_cb, (gpointer)self);
                    self->prv->ipv4_primary_ip = ip;
                }
                self->prv->nwam_ncu_ip_modified = TRUE;
            }
            break;
        case PROP_IPV4_AUTO_CONF: {
                gboolean auto_conf = g_value_get_boolean( value );

                if ( self->prv->ipv4_primary_ip != NULL ) {
                    nwamui_ip_set_autoconf(self->prv->ipv4_primary_ip, auto_conf );
                }
                else {
                    NwamuiIp*   ip = nwamui_ip_new( self, "", "", FALSE,
                                                    FALSE, auto_conf, FALSE );
                    GtkTreeIter iter;

                    g_signal_handlers_block_by_func(G_OBJECT(self->prv->v4addresses), (gpointer)ip_row_inserted_or_changed_cb, (gpointer)self);
                    gtk_list_store_insert(self->prv->v4addresses, &iter, 0 );
                    gtk_list_store_set(self->prv->v4addresses, &iter, 0, ip, -1 );
                    g_signal_handlers_unblock_by_func(G_OBJECT(self->prv->v4addresses), (gpointer)ip_row_inserted_or_changed_cb, (gpointer)self);
                    self->prv->ipv4_primary_ip = ip;
                }
                self->prv->nwam_ncu_ip_modified = TRUE;
            }
            break;
        case PROP_IPV4_ADDRESS: {
                const gchar* new_addr  = g_value_get_string( value );
                
                if ( new_addr != NULL ) {
                    if ( self->prv->ipv4_primary_ip != NULL ) {
                        nwamui_ip_set_address(self->prv->ipv4_primary_ip, new_addr );
                    }
                    else {
                        NwamuiIp*   ip = nwamui_ip_new( self, new_addr, "", FALSE,
                                                        FALSE, FALSE, TRUE );
                        GtkTreeIter iter;

                        g_signal_handlers_block_by_func(G_OBJECT(self->prv->v4addresses), (gpointer)ip_row_inserted_or_changed_cb, (gpointer)self);
                        gtk_list_store_insert(self->prv->v4addresses, &iter, 0 );
                        gtk_list_store_set(self->prv->v4addresses, &iter, 0, ip, -1 );
                        g_signal_handlers_unblock_by_func(G_OBJECT(self->prv->v4addresses), (gpointer)ip_row_inserted_or_changed_cb, (gpointer)self);
                        self->prv->ipv4_primary_ip = ip;
                    }
                    self->prv->nwam_ncu_ip_modified = TRUE;
                }
            }
            break;
        case PROP_IPV4_SUBNET: {
                const gchar* new_subnet  = g_value_get_string( value );
                
                if ( new_subnet != NULL ) {
                    if ( self->prv->ipv4_primary_ip != NULL ) {
                        nwamui_ip_set_subnet_prefix(self->prv->ipv4_primary_ip, new_subnet );
                    }
                    else {
                        NwamuiIp*   ip = nwamui_ip_new( self, "", new_subnet, FALSE,
                                                        FALSE, FALSE, TRUE );
                        GtkTreeIter iter;

                        g_signal_handlers_block_by_func(G_OBJECT(self->prv->v4addresses), (gpointer)ip_row_inserted_or_changed_cb, (gpointer)self);
                        gtk_list_store_insert(self->prv->v4addresses, &iter, 0 );
                        gtk_list_store_set(self->prv->v4addresses, &iter, 0, ip, -1 );
                        g_signal_handlers_unblock_by_func(G_OBJECT(self->prv->v4addresses), (gpointer)ip_row_inserted_or_changed_cb, (gpointer)self);
                        self->prv->ipv4_primary_ip = ip;
                    }
                    self->prv->nwam_ncu_ip_modified = TRUE;
                }
            }
            break;
        case PROP_IPV6_ACTIVE: {
                self->prv->ipv6_active = g_value_get_boolean( value );
                self->prv->nwam_ncu_ip_modified = TRUE;
            }
            break;
        case PROP_IPV6_DHCP: {
                gboolean dhcp = g_value_get_boolean( value );

                if ( self->prv->ipv6_primary_ip != NULL ) {
                    nwamui_ip_set_dhcp(self->prv->ipv6_primary_ip, dhcp );
                }
                else {
                    NwamuiIp*   ip = nwamui_ip_new( self, "", "", TRUE,
                                                    dhcp, FALSE, FALSE );
                    GtkTreeIter iter;

                    g_signal_handlers_block_by_func(G_OBJECT(self->prv->v4addresses), (gpointer)ip_row_inserted_or_changed_cb, (gpointer)self);
                    gtk_list_store_insert(self->prv->v6addresses, &iter, 0 );
                    gtk_list_store_set(self->prv->v6addresses, &iter, 0, ip, -1 );
                    g_signal_handlers_unblock_by_func(G_OBJECT(self->prv->v4addresses), (gpointer)ip_row_inserted_or_changed_cb, (gpointer)self);
                    self->prv->ipv6_primary_ip = ip;
                }
                self->prv->nwam_ncu_ip_modified = TRUE;
            }
            break;
        case PROP_IPV6_AUTO_CONF: {
                gboolean auto_conf = g_value_get_boolean( value );

                if ( self->prv->ipv6_primary_ip != NULL ) {
                    nwamui_ip_set_autoconf(self->prv->ipv6_primary_ip, auto_conf );
                }
                else {
                    NwamuiIp*   ip = nwamui_ip_new( self, "", "", TRUE,
                                                    FALSE, auto_conf, FALSE );
                    GtkTreeIter iter;

                    g_signal_handlers_block_by_func(G_OBJECT(self->prv->v6addresses), (gpointer)ip_row_inserted_or_changed_cb, (gpointer)self);
                    gtk_list_store_insert(self->prv->v6addresses, &iter, 0 );
                    gtk_list_store_set(self->prv->v6addresses, &iter, 0, ip, -1 );
                    g_signal_handlers_unblock_by_func(G_OBJECT(self->prv->v6addresses), (gpointer)ip_row_inserted_or_changed_cb, (gpointer)self);
                    self->prv->ipv6_primary_ip = ip;
                }
                self->prv->nwam_ncu_ip_modified = TRUE;
            }
            break;
        case PROP_IPV6_ADDRESS: {
                const gchar* new_addr  = g_value_get_string( value );
                
                if ( new_addr != NULL ) {
                    if ( self->prv->ipv6_primary_ip != NULL ) {
                        nwamui_ip_set_address(self->prv->ipv6_primary_ip, new_addr );
                    }
                    else {
                        NwamuiIp*   ip = nwamui_ip_new( self, new_addr, "", TRUE,
                                                        FALSE, FALSE, TRUE );
                        GtkTreeIter iter;

                        g_signal_handlers_block_by_func(G_OBJECT(self->prv->v6addresses), (gpointer)ip_row_inserted_or_changed_cb, (gpointer)self);
                        gtk_list_store_insert(self->prv->v6addresses, &iter, 0 );
                        gtk_list_store_set(self->prv->v6addresses, &iter, 0, ip, -1 );
                        g_signal_handlers_unblock_by_func(G_OBJECT(self->prv->v6addresses), (gpointer)ip_row_inserted_or_changed_cb, (gpointer)self);
                        self->prv->ipv6_primary_ip = ip;
                    }
                }
                self->prv->nwam_ncu_ip_modified = TRUE;
            }
            break;
        case PROP_IPV6_PREFIX: {
                const gchar* new_subnet  = g_value_get_string( value );
                
                if ( new_subnet != NULL ) {
                    if ( self->prv->ipv6_primary_ip != NULL ) {
                        nwamui_ip_set_subnet_prefix(self->prv->ipv6_primary_ip, new_subnet );
                    }
                    else {
                        NwamuiIp*   ip = nwamui_ip_new( self, "", new_subnet, TRUE,
                                                        FALSE, FALSE, TRUE );
                        GtkTreeIter iter;

                        g_signal_handlers_block_by_func(G_OBJECT(self->prv->v6addresses), (gpointer)ip_row_inserted_or_changed_cb, (gpointer)self);
                        gtk_list_store_insert(self->prv->v6addresses, &iter, 0 );
                        gtk_list_store_set(self->prv->v6addresses, &iter, 0, ip, -1 );
                        g_signal_handlers_unblock_by_func(G_OBJECT(self->prv->v6addresses), (gpointer)ip_row_inserted_or_changed_cb, (gpointer)self);
                        self->prv->ipv6_primary_ip = ip;
                    }
                }
                self->prv->nwam_ncu_ip_modified = TRUE;
            }
            break;
        case PROP_V4ADDRESSES: {
                self->prv->v4addresses = GTK_LIST_STORE(g_value_dup_object( value ));
                self->prv->nwam_ncu_ip_modified = TRUE;
            }
            break;

        case PROP_V6ADDRESSES: {
                self->prv->v6addresses = GTK_LIST_STORE(g_value_dup_object( value ));
                self->prv->nwam_ncu_ip_modified = TRUE;
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

        case PROP_ACTIVATION_MODE: {
                nwamui_cond_activation_mode_t activation_mode = 
                    (nwamui_cond_activation_mode_t)g_value_get_int( value );

                set_nwam_ncu_uint64_prop( self->prv->nwam_ncu_phys, NWAM_NCU_PROP_ACTIVATION_MODE, (guint64)activation_mode );
                self->prv->nwam_ncu_phys_modified = TRUE;
            }
            break;

        case PROP_ENABLED: {
                gboolean enabled = g_value_get_boolean( value );
                set_nwam_ncu_boolean_prop( self->prv->nwam_ncu_phys, NWAM_NCU_PROP_ENABLED, enabled );
                self->prv->nwam_ncu_phys_modified = TRUE;
            }
            break;

        case PROP_PRIORITY_GROUP: {
                guint64 priority_group = g_value_get_uint( value );

                set_nwam_ncu_uint64_prop( self->prv->nwam_ncu_phys, NWAM_NCU_PROP_PRIORITY_GROUP, priority_group );
                self->prv->nwam_ncu_phys_modified = TRUE;
            }
            break;

        case PROP_PRIORITY_GROUP_MODE: {
                guint64 priority_mode = g_value_get_int( value );
                set_nwam_ncu_uint64_prop( self->prv->nwam_ncu_phys, NWAM_NCU_PROP_PRIORITY_MODE, priority_mode );
                self->prv->nwam_ncu_phys_modified = TRUE;
            }
            break;

        case PROP_AUTO_PUSH: {
                GList*  autopush = g_value_get_pointer( value );
                gchar** autopush_strs = nwamui_util_glist_to_strv( autopush );
                set_nwam_ncu_string_array_prop( self->prv->nwam_ncu_phys, NWAM_NCU_PROP_LINK_AUTOPUSH, autopush_strs, 0 );
                g_strfreev(autopush_strs);
                self->prv->nwam_ncu_phys_modified = TRUE;
            }

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
        case PROP_NCP: {
                g_value_set_object( value, self->prv->ncp );
            }
            break;
        case PROP_VANITY_NAME: {
                g_value_set_string(value, self->prv->vanity_name);
            }
            break;
        case PROP_DEVICE_NAME: {
                g_value_set_string(value, self->prv->device_name);
            }
            break;
        case PROP_PHY_ADDRESS: {
                gchar* mac_addr = get_nwam_ncu_string_prop( self->prv->nwam_ncu_phys, NWAM_NCU_PROP_LINK_MAC_ADDR );
                g_value_set_string(value, mac_addr );
                g_free(mac_addr);
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
                uint64_t speed;
                if ( get_kstat_uint64 ( self->prv->device_name, "ifspeed", &speed ) ) {
                    guint mbs = (guint) (speed / 1000000ull);
                    g_value_set_uint( value, mbs );
                }
                else {
                    g_value_set_uint( value, 0 );
                }
            }
            break;
        case PROP_MTU: {
                guint64 mtu = get_nwam_ncu_uint64_prop( self->prv->nwam_ncu_phys, NWAM_NCU_PROP_LINK_MTU);
                g_value_set_uint( value, (guint)mtu );
            }
            break;
        case PROP_IPV4_DHCP: {
                gboolean dhcp = FALSE;
                
                if ( self->prv->ipv4_primary_ip != NULL ) {
                    dhcp = nwamui_ip_is_dhcp(self->prv->ipv4_primary_ip); 
                }
                g_value_set_boolean(value, dhcp);
            }
            break;
        case PROP_IPV4_AUTO_CONF: {
                gboolean auto_conf = FALSE;
                
                if ( self->prv->ipv4_primary_ip != NULL ) {
                    auto_conf = nwamui_ip_is_autoconf(self->prv->ipv4_primary_ip); 
                }
                g_value_set_boolean(value, auto_conf);
            }
            break;
        case PROP_IPV4_ADDRESS: {
                gchar *address = NULL;
                if ( self->prv->ipv4_primary_ip != NULL ) {
                    address = nwamui_ip_get_address(self->prv->ipv4_primary_ip);
                }
                g_value_set_string(value, address);
                if ( address != NULL ) {
                    g_free(address);
                }
            }
            break;
        case PROP_IPV4_SUBNET: {
                gchar *subnet = NULL;
                if ( self->prv->ipv4_primary_ip != NULL ) {
                    subnet = nwamui_ip_get_subnet_prefix(self->prv->ipv4_primary_ip);
                }
                g_value_set_string(value, subnet);
                if ( subnet != NULL ) {
                    g_free(subnet);
                }
            }
            break;
        case PROP_IPV6_ACTIVE: {
                g_value_set_boolean(value, self->prv->ipv6_active);
            }
            break;
        case PROP_IPV6_DHCP: {
                gboolean dhcp = FALSE;
                
                if ( self->prv->ipv6_primary_ip != NULL ) {
                    dhcp = nwamui_ip_is_dhcp(self->prv->ipv6_primary_ip); 
                }
                g_value_set_boolean(value, dhcp);
            }
            break;
        case PROP_IPV6_AUTO_CONF: {
                gboolean auto_conf = FALSE;
                
                if ( self->prv->ipv6_primary_ip != NULL ) {
                    auto_conf = nwamui_ip_is_autoconf(self->prv->ipv6_primary_ip); 
                }
                g_value_set_boolean(value, auto_conf);
            }
            break;
        case PROP_IPV6_ADDRESS: {
                gchar *address = NULL;

                if ( self->prv->ipv6_primary_ip != NULL ) {
                    address = nwamui_ip_get_address(self->prv->ipv6_primary_ip);
                }
                g_value_set_string(value, address);
                if ( address != NULL ) {
                    g_free(address);
                }
            }
            break;
        case PROP_IPV6_PREFIX: {
                gchar *prefix = NULL;
                if ( self->prv->ipv6_primary_ip != NULL ) {
                    prefix = nwamui_ip_get_subnet_prefix(self->prv->ipv6_primary_ip);
                }
                g_value_set_string(value, prefix);
                if ( prefix != NULL ) {
                    g_free(prefix);
                }
            }
            break;
        case PROP_V4ADDRESSES: {
                g_value_set_object( value, (gpointer)self->prv->v4addresses);
            }
            break;

        case PROP_V6ADDRESSES: {
                g_value_set_object( value, (gpointer)self->prv->v6addresses);
            }
            break;
        case PROP_WIFI_INFO: {
                /* Should be a Wireless i/f */
                g_assert( self->prv->ncu_type == NWAMUI_NCU_TYPE_WIRELESS );

                g_value_set_object( value, (gpointer)self->prv->wifi_info );
            }
            break;
        case PROP_ACTIVATION_MODE: {
                nwamui_cond_activation_mode_t activation_mode;
                activation_mode = (nwamui_cond_activation_mode_t)
                                    get_nwam_ncu_uint64_prop( self->prv->nwam_ncu_phys, NWAM_NCU_PROP_ACTIVATION_MODE );

                g_value_set_int( value, (gint)activation_mode );

            }
            break;

        case PROP_ENABLED: {
                g_value_set_boolean( value, 
                            get_nwam_ncu_boolean_prop( self->prv->nwam_ncu_phys, NWAM_NCU_PROP_ENABLED ) );
            }
            break;

        case PROP_PRIORITY_GROUP: {
                g_value_set_uint( value, (guint)get_nwam_ncu_uint64_prop( self->prv->nwam_ncu_phys,
                                                      NWAM_NCU_PROP_PRIORITY_GROUP ) );
            }
            break;

        case PROP_PRIORITY_GROUP_MODE: {
                nwamui_cond_priority_group_mode_t priority_group_mode = 
                        (nwamui_cond_priority_group_mode_t)
                        get_nwam_ncu_uint64_prop( self->prv->nwam_ncu_phys, NWAM_NCU_PROP_PRIORITY_MODE );

                g_value_set_int( value, (gint)priority_group_mode );
            }
            break;
        case PROP_AUTO_PUSH: {
                gchar** autopush = get_nwam_ncu_string_array_prop( self->prv->nwam_ncu_phys, NWAM_NCU_PROP_LINK_AUTOPUSH );
                GList*  autopush_list = nwamui_util_strv_to_glist( autopush );
                g_value_set_pointer( value, autopush_list );
                g_strfreev(autopush);
            }
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static nwamui_ncu_type_t
get_if_type( const gchar* device )
{
    dladm_handle_t      handle;
    nwamui_ncu_type_t   type;
    uint32_t            media;
    
    type = NWAMUI_NCU_TYPE_WIRED;

    if ( device == NULL ) {
        g_warning("Unable to get i/f type for a NULL device name, returning WIRED");
        return( type );
    }

    if ( dladm_open( &handle ) != DLADM_STATUS_OK ) {
        g_warning("Error creating dladm handle" );
        return( type );
    }

    if ( dladm_name2info( handle, device, NULL, NULL, NULL, &media ) != DLADM_STATUS_OK ) {
        if (strncmp(device, "ip.tun", 6) == 0 ||
            strncmp(device, "ip6.tun", 7) == 0 ||
            strncmp(device, "ip.6to4tun", 10) == 0)
            /*
             * TODO
             *
             * We'll need to update our tunnel detection once
             * the clearview/tun project is integrated; tunnel
             * names won't necessarily be ip.tunN.
             */
            type = NWAMUI_NCU_TYPE_TUNNEL;
    }
    else if ( media == DL_WIFI ) {
        type = NWAMUI_NCU_TYPE_WIRELESS;
    }

    dladm_close( handle );

    return( type );
}

static void
populate_common_ncu_data( NwamuiNcu *ncu, nwam_ncu_handle_t nwam_ncu )
{
    char*               name = NULL;
    nwam_error_t        nerr;
    nwamui_ncu_type_t   ncu_type;

    if ( (nerr = nwam_ncu_get_name (nwam_ncu, &name)) != NWAM_SUCCESS ) {
        g_debug ("Failed to get name for ncu, error: %s", nwam_strerror (nerr));
    }

    ncu_type = get_if_type( name );

    g_object_set( ncu,
                  "device_name", name,
                  "vanity_name", name,
                  "ncu_type", ncu_type,
                  NULL);

    free(name);
}

static void
populate_iptun_ncu_data( NwamuiNcu *ncu, nwam_ncu_handle_t nwam_ncu )
{
    nwam_iptun_type_t tun_type; 
    gchar* tun_tsrc;
    gchar* tun_tdst;
    gchar* tun_encr;
    gchar* tun_encr_auth;
    gchar* tun_auth;

    tun_type = get_nwam_ncu_uint64_prop( nwam_ncu, NWAM_NCU_PROP_IPTUN_TYPE );
    tun_tsrc = get_nwam_ncu_string_prop( nwam_ncu, NWAM_NCU_PROP_IPTUN_TSRC );
    tun_tdst = get_nwam_ncu_string_prop( nwam_ncu, NWAM_NCU_PROP_IPTUN_TDST );
    tun_encr = get_nwam_ncu_string_prop( nwam_ncu, NWAM_NCU_PROP_IPTUN_ENCR );
    tun_encr_auth = get_nwam_ncu_string_prop( nwam_ncu, NWAM_NCU_PROP_IPTUN_ENCR_AUTH );
    tun_auth = get_nwam_ncu_string_prop( nwam_ncu, NWAM_NCU_PROP_IPTUN_AUTH );

    g_object_set( ncu, 
                  "tun_type", tun_type,
                  "tun_tsrc", tun_tsrc,
                  "tun_tdst", tun_tdst,
                  "tun_encr", tun_encr,
                  "tun_encr_auth", tun_encr_auth,
                  "tun_auth", tun_auth,
                  NULL );

    g_free(tun_tsrc);
    g_free(tun_tdst);
    g_free(tun_encr);
    g_free(tun_encr_auth);
    g_free(tun_auth);
}

static void
populate_ip_ncu_data( NwamuiNcu *ncu, nwam_ncu_handle_t nwam_ncu )
{
    nwam_ip_version_t   ip_version;
    guint               ipv4_addrsrc_num = 0;
    nwam_addrsrc_t*     ipv4_addrsrc = NULL;
    gchar**             ipv4_addr = NULL;
    guint               ipv6_addrsrc_num = 0;
    nwam_addrsrc_t*     ipv6_addrsrc =  NULL;
    gchar**             ipv6_addr = NULL;
    
    ip_version = (nwam_ip_version_t)get_nwam_ncu_uint64_prop(nwam_ncu, NWAM_NCU_PROP_IP_VERSION );

    g_object_freeze_notify(G_OBJECT(ncu->prv->v4addresses));
    g_object_freeze_notify(G_OBJECT(ncu->prv->v6addresses));

    gtk_list_store_clear(ncu->prv->v4addresses);
    if ( (ip_version == NWAM_IP_VERSION_IPV4) || (ip_version == NWAM_IP_VERSION_ALL) )  {
        char**  ptr;
        ipv4_addrsrc = (nwam_addrsrc_t*)get_nwam_ncu_uint64_array_prop( nwam_ncu, 
                                                                        NWAM_NCU_PROP_IPV4_ADDRSRC, 
                                                                        &ipv4_addrsrc_num );

        ipv4_addr = get_nwam_ncu_string_array_prop(nwam_ncu, NWAM_NCU_PROP_IPV4_ADDR );

        /* Populate the v4addresses member */
        g_debug( "ipv4_addrsrc_num = %u", ipv4_addrsrc_num );
        ptr = ipv4_addr;

        for( int i = 0; i < ipv4_addrsrc_num; i++ ) {
            NwamuiIp*   ip = nwamui_ip_new( ncu, 
                                            ((ptr != NULL)?(*ptr):(NULL)), 
                                            "", 
                                            FALSE, 
                                            ipv4_addrsrc[i] == NWAM_ADDRSRC_DHCP,
                                            ipv4_addrsrc[i] == NWAM_ADDRSRC_AUTOCONF,
                                            ipv4_addrsrc[i] == NWAM_ADDRSRC_STATIC);

            GtkTreeIter iter;

            g_signal_handlers_block_by_func(G_OBJECT(ncu->prv->v4addresses), (gpointer)ip_row_inserted_or_changed_cb, (gpointer)ncu);
            gtk_list_store_insert(ncu->prv->v4addresses, &iter, 0 );
            gtk_list_store_set(ncu->prv->v4addresses, &iter, 0, ip, -1 );
            g_signal_handlers_unblock_by_func(G_OBJECT(ncu->prv->v4addresses), (gpointer)ip_row_inserted_or_changed_cb, (gpointer)ncu);

            if ( i == 0 ) {
                ncu->prv->ipv4_primary_ip = NWAMUI_IP(g_object_ref(ip));
            }
        }
    }

    ncu->prv->ipv6_active = FALSE;
    gtk_list_store_clear(ncu->prv->v6addresses);

    if ( (ip_version == NWAM_IP_VERSION_IPV6) || (ip_version == NWAM_IP_VERSION_ALL) )  {
        char**  ptr;

        ipv6_addrsrc = (nwam_addrsrc_t*)get_nwam_ncu_uint64_array_prop( nwam_ncu, 
                                                                        NWAM_NCU_PROP_IPV6_ADDRSRC, 
                                                                        &ipv6_addrsrc_num );
        
        ipv6_addr = get_nwam_ncu_string_array_prop(nwam_ncu, NWAM_NCU_PROP_IPV6_ADDR );

        /* Populate the v6addresses member */
        g_debug( "ipv6_addrsrc_num = %u", ipv6_addrsrc_num );
        ptr = ipv6_addr;

        for( int i = 0; i < ipv6_addrsrc_num; i++ ) {
            NwamuiIp*   ip = nwamui_ip_new( ncu, 
                                            ((ptr != NULL)?(*ptr):(NULL)), 
                                            "", 
                                            TRUE, 
                                            ipv6_addrsrc[i] == NWAM_ADDRSRC_DHCPV6,
                                            ipv6_addrsrc[i] == NWAM_ADDRSRC_AUTOCONF,
                                            ipv6_addrsrc[i] == NWAM_ADDRSRC_STATIC);

            GtkTreeIter iter;

            g_signal_handlers_block_by_func(G_OBJECT(ncu->prv->v6addresses), (gpointer)ip_row_inserted_or_changed_cb, (gpointer)ncu);
            gtk_list_store_insert(ncu->prv->v6addresses, &iter, 0 );
            gtk_list_store_set(ncu->prv->v6addresses, &iter, 0, ip, -1 );
            g_signal_handlers_unblock_by_func(G_OBJECT(ncu->prv->v6addresses), (gpointer)ip_row_inserted_or_changed_cb, (gpointer)ncu);

            if ( i == 0 ) {
                ncu->prv->ipv6_primary_ip = NWAMUI_IP(g_object_ref(ip));
            }
        }

        if ( ipv6_addrsrc_num > 0 ) {
            ncu->prv->ipv6_active = TRUE;
        }
    }

    g_object_thaw_notify(G_OBJECT(ncu->prv->v4addresses));
    g_object_thaw_notify(G_OBJECT(ncu->prv->v6addresses));

}

/* 
 * An NCU name is <type>:<devicename>, extract the devicename part 
 */
static gchar*
get_device_from_ncu_name( const gchar* ncu_name )
{
    gchar*  ptr = NULL;

    if ( ncu_name != NULL ) {
        ptr = strrchr( ncu_name, ':');
        if ( ptr != NULL ) {
            ptr = g_strdup(ptr);
        }
    }

    return( ptr );
}

/* 
 * A nwam_ncu_handle_t is actually a set of nvpairs we need to get our
 * own handle (i.e. snapshot) since the handle we are passed may be freed by
 * the owner of it (e.g. walkprop does this).
 */
static nwam_ncu_handle_t
get_nwam_ncu_handle( NwamuiNcu* self, nwam_ncu_type_t ncu_type )
{
    nwam_ncu_handle_t ncu_handle = NULL;

    if (self != NULL && self->prv != NULL && \
        self->prv->ncp != NULL && self->prv->device_name != NULL ) {

        nwam_ncp_handle_t ncp_handle;
        nwam_error_t      nerr;

        ncp_handle = nwamui_ncp_get_nwam_handle( self->prv->ncp );

        if ( (nerr = nwam_ncu_read( ncp_handle, self->prv->device_name, ncu_type, 0,
                                    &ncu_handle ) ) != NWAM_SUCCESS  ) {
            g_warning("Failed to read ncu information for %s", self->prv->device_name );
            return ( NULL );
        }
    }

    return( ncu_handle );
}

extern void
nwamui_ncu_update_with_handle( NwamuiNcu* self, nwam_ncu_handle_t ncu   )
{
    nwam_ncu_class_t    ncu_class;
    
    g_return_if_fail( NWAMUI_IS_NCU(self) );


    ncu_class = (nwam_ncu_class_t)get_nwam_ncu_uint64_prop(ncu, NWAM_NCU_PROP_CLASS);

    g_object_freeze_notify( G_OBJECT(self) );
    self->prv->initialisation = TRUE;

    populate_common_ncu_data( self, ncu );

    switch ( ncu_class ) {
        case NWAM_NCU_CLASS_PHYS: {
                /* CLASS PHYS is of type LINK, so has LINK props */
                nwam_ncu_handle_t   ncu_handle;
                ncu_handle = get_nwam_ncu_handle( self, NWAM_NCU_TYPE_LINK );

                if ( self->prv->nwam_ncu_phys != NULL ) {
                    nwam_ncu_free( self->prv->nwam_ncu_phys );
                }

                self->prv->nwam_ncu_phys = ncu_handle;
            }
            break;
        case NWAM_NCU_CLASS_IPTUN: {
                nwam_ncu_handle_t   ncu_handle;
                ncu_handle = get_nwam_ncu_handle( self, NWAM_NCU_TYPE_IP );

                if ( self->prv->nwam_ncu_iptun != NULL ) {
                    nwam_ncu_free( self->prv->nwam_ncu_iptun );
                }

                self->prv->nwam_ncu_iptun = ncu_handle;
                populate_iptun_ncu_data( self, ncu_handle );
            }
            break;
        case NWAM_NCU_CLASS_IP: {
                nwam_ncu_handle_t   ncu_handle;

                ncu_handle = get_nwam_ncu_handle( self, NWAM_NCU_TYPE_IP );

                if ( self->prv->nwam_ncu_ip != NULL ) {
                    nwam_ncu_free( self->prv->nwam_ncu_ip );
                }

                self->prv->nwam_ncu_ip = ncu_handle;
                populate_ip_ncu_data( self, ncu_handle );
            }
            break;
        default:
            g_error("Unexpected ncu class %u", (guint)ncu_class);
    }
    
    g_object_thaw_notify( G_OBJECT(self) );

    self->prv->initialisation = FALSE;
}


extern  NwamuiNcu*
nwamui_ncu_new_with_handle( NwamuiNcp* ncp, nwam_ncu_handle_t ncu )
{
    NwamuiNcu*          self = NULL;
    nwam_ncu_class_t    ncu_class;
    
    self = NWAMUI_NCU(g_object_new (NWAMUI_TYPE_NCU,
                                    "ncp", ncp,
                                    NULL));

    nwamui_ncu_update_with_handle( self, ncu );

    self->prv->initialisation = FALSE;

    return( self );
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
                    const gchar*        phy_address,
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
                    "phy_address", phy_address,
                    "ncu_type", ncu_type,
                    "active", active,
                    "ipv4_auto_conf", ipv4_auto_conf,
                    "ipv4_address", ipv4_address,
                    "ipv4_subnet", ipv4_subnet,
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
 * nwamui_ncu_is_modifiable:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the modifiable.
 *
 **/
extern gboolean
nwamui_ncu_is_modifiable (NwamuiNcu *self)
{
    nwam_error_t    nerr;
    gboolean        modifiable = FALSE; 
    boolean_t       readonly;

    if (!NWAMUI_IS_NCU (self) || self->prv->nwam_ncu_phys == NULL ) {
        return( modifiable );
    }

    if ( (nerr = nwam_ncu_get_read_only( self->prv->nwam_ncu_phys, &readonly )) == NWAM_SUCCESS ) {
        modifiable = readonly?FALSE:TRUE;
    }
    else {
        g_warning("Error getting ncu read-only status: %s", nwam_strerror( nerr ) );
    }

    return( modifiable );
}


/**
 * nwamui_ncu_reload:   re-load stored configuration
 **/
extern void
nwamui_ncu_reload( NwamuiNcu* self )
{
    g_return_if_fail( NWAMUI_IS_NCU(self) );

    /* nwamui_ncu_update_with_handle will cause re-read from configuration */
    g_object_freeze_notify(G_OBJECT(self));

    if ( self->prv->nwam_ncu_phys != NULL ) {
        g_debug("Reverting NCU PHYS for %s", self->prv->device_name);
        nwamui_ncu_update_with_handle( self, self->prv->nwam_ncu_phys );
    }

    if ( self->prv->nwam_ncu_ip != NULL ) {
        g_debug("Reverting NCU IP for %s", self->prv->device_name);
        nwamui_ncu_update_with_handle( self, self->prv->nwam_ncu_ip );
    }

    if ( self->prv->nwam_ncu_iptun != NULL ) {
        g_debug("Reverting NCU IPTUN for %s", self->prv->device_name);
        nwamui_ncu_update_with_handle( self, self->prv->nwam_ncu_iptun );
    }

    g_object_thaw_notify(G_OBJECT(self));
}

/**
 * nwamui_ncu_has_modifications:   test if there are un-saved changes
 * @returns: TRUE if unsaved changes exist.
 **/
extern gboolean
nwamui_ncu_has_modifications( NwamuiNcu* self )
{
    if ( NWAMUI_IS_NCU(self) &&
          ( self->prv->nwam_ncu_phys_modified 
         || self->prv->nwam_ncu_ip_modified 
         || self->prv->nwam_ncu_iptun_modified) ) {
        return( TRUE );
    }

    return( FALSE );
}

/**
 * nwamui_ncu_validate:   validate in-memory configuration
 * @prop_name_ret:  If non-NULL, the name of the property that failed will be
 *                  returned, should be freed by caller.
 * @returns: TRUE if valid, FALSE if failed
 **/
extern gboolean
nwamui_ncu_validate( NwamuiNcu* self, gchar **prop_name_ret )
{
    nwam_error_t    nerr;
    const char*     prop_name = NULL;

    g_return_val_if_fail( NWAMUI_IS_NCU(self), FALSE );

    if ( self->prv->nwam_ncu_phys_modified && self->prv->nwam_ncu_phys != NULL ) {
        if ( (nerr = nwam_ncu_validate( self->prv->nwam_ncu_phys, &prop_name ) ) != NWAM_SUCCESS ) {
            g_debug("Failed when validating PHYS NCU for %s : invalid value for %s", 
                    self->prv->device_name, prop_name);
            if ( prop_name_ret != NULL ) {
                *prop_name_ret = g_strdup( prop_name );
            }
            return( FALSE );
        }
    }

    if ( self->prv->nwam_ncu_ip_modified && self->prv->nwam_ncu_ip != NULL ) {
        if ( (nerr = nwam_ncu_validate( self->prv->nwam_ncu_ip, &prop_name )) != NWAM_SUCCESS ) {
            g_debug("Failed when validating IP NCU for %s : invalid value for %s",
                    self->prv->device_name, prop_name);
            if ( prop_name_ret != NULL ) {
                *prop_name_ret = g_strdup( prop_name );
            }
            return( FALSE );
        }
    }

    if ( self->prv->nwam_ncu_iptun_modified && self->prv->nwam_ncu_iptun != NULL ) {
        if ( (nerr = nwam_ncu_validate( self->prv->nwam_ncu_iptun, &prop_name )) != NWAM_SUCCESS ) {
            g_debug("Failed when validating IPTUN NCU for %s : invalid value for %s",
                    self->prv->device_name, prop_name);
            if ( prop_name_ret != NULL ) {
                *prop_name_ret = g_strdup( prop_name );
            }
            return( FALSE );
        }
    }

    return( TRUE );
}

/**
 * nwamui_ncu_commit:   commit in-memory configuration, to persistant storage
 * @returns: TRUE if succeeded, FALSE if failed
 **/
extern gboolean
nwamui_ncu_commit( NwamuiNcu* self )
{
    nwam_error_t    nerr;

    g_return_val_if_fail( NWAMUI_IS_NCU(self), FALSE );

    if ( self->prv->nwam_ncu_phys_modified && self->prv->nwam_ncu_phys != NULL ) {
        if ( (nerr = nwam_ncu_commit( self->prv->nwam_ncu_phys, 0 ) ) != NWAM_SUCCESS ) {
            g_warning("Failed when committing PHYS NCU for %s", self->prv->device_name);
            return( FALSE );
        }
    }

    if ( self->prv->nwam_ncu_ip_modified && self->prv->nwam_ncu_ip != NULL ) {
        if ( (nerr = nwam_ncu_commit( self->prv->nwam_ncu_ip, 0 )) != NWAM_SUCCESS ) {
            g_warning("Failed when committing IP NCU for %s", self->prv->device_name);
            return( FALSE );
        }
    }

    if ( self->prv->nwam_ncu_iptun_modified && self->prv->nwam_ncu_iptun != NULL ) {
        if ( (nerr = nwam_ncu_commit( self->prv->nwam_ncu_iptun, 0 )) != NWAM_SUCCESS ) {
            g_warning("Failed when committing IPTUN NCU for %s", self->prv->device_name);
            return( FALSE );
        }
    }

    return( TRUE );
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
 * nwamui_ncu_get_phy_address:
 * @returns: null-terminated C String with the device name of the the NCU.
 *
 **/
extern gchar*
nwamui_ncu_get_phy_address ( NwamuiNcu *self )
{
    gchar*  name = NULL;
    
    g_return_val_if_fail (NWAMUI_IS_NCU(self), name); 
    
    g_object_get (G_OBJECT (self),
                  "phy_address", &name,
                  NULL);

    return( name );
}

/**
 * nwamui_ncu_set_phy_address:
 * @phy_address: null-terminated C String with the device phy_address of the the NCU.
 *
 **/
extern void
nwamui_ncu_set_phy_address ( NwamuiNcu *self, const gchar* phy_address )
{
    g_return_if_fail (NWAMUI_IS_NCU(self)); 
    
    g_assert (phy_address != NULL );

    if ( phy_address != NULL ) {
        g_object_set (G_OBJECT (self),
                      "phy_address", phy_address,
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
    gchar*  display_name = NULL;
    
    g_return_val_if_fail (NWAMUI_IS_NCU(self), display_name); 
    
    if ( self->prv->vanity_name != NULL ) {
        switch( self->prv->ncu_type ) {
            case NWAMUI_NCU_TYPE_WIRED:
                display_name = g_strdup_printf( _("Wired(%s)"), self->prv->vanity_name );
                break;
            case NWAMUI_NCU_TYPE_WIRELESS:
                display_name = g_strdup_printf( _("Wireless(%s)"), self->prv->vanity_name );
                break;
            case NWAMUI_NCU_TYPE_TUNNEL:
                display_name = g_strdup_printf( _("Tunnel(%s)"), self->prv->vanity_name );
                break;
            default:
                display_name = g_strdup( self->prv->vanity_name );
                break;
        }
    }
    
    return( display_name );
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
 * nwamui_ncu_set_mtu:
 * @nwamui_ncu: a #NwamuiNcu.
 * @mtu: Value to set mtu to.
 * 
 **/ 
extern void
nwamui_ncu_set_mtu (   NwamuiNcu *self,
                              guint        mtu )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));
    g_assert (mtu >= 0 && mtu <= G_MAXUINT );

    g_object_set (G_OBJECT (self),
                  "mtu", mtu,
                  NULL);
}

/**
 * nwamui_ncu_get_mtu:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the mtu.
 *
 **/
extern guint
nwamui_ncu_get_mtu (NwamuiNcu *self)
{
    guint  mtu = 0; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), mtu);

    g_object_get (G_OBJECT (self),
                  "mtu", &mtu,
                  NULL);

    return( mtu );
}

/** 
 * nwamui_ncu_set_ipv4_dhcp:
 * @nwamui_ncu: a #NwamuiNcu.
 * @ipv4_dhcp: Valiue to set ipv4_dhcp to.
 * 
 **/ 
extern void
nwamui_ncu_set_ipv4_dhcp (   NwamuiNcu *self,
                              gboolean        ipv4_dhcp )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));

    g_object_set (G_OBJECT (self),
                  "ipv4_dhcp", ipv4_dhcp,
                  NULL);
}

/**
 * nwamui_ncu_get_ipv4_dhcp:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the ipv4_dhcp.
 *
 **/
extern gboolean
nwamui_ncu_get_ipv4_dhcp (NwamuiNcu *self)
{
    gboolean  ipv4_dhcp = TRUE; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), ipv4_dhcp);

    g_object_get (G_OBJECT (self),
                  "ipv4_dhcp", &ipv4_dhcp,
                  NULL);

    return( ipv4_dhcp );
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
 * nwamui_ncu_set_ipv6_dhcp:
 * @nwamui_ncu: a #NwamuiNcu.
 * @ipv6_dhcp: Valiue to set ipv6_dhcp to.
 * 
 **/ 
extern void
nwamui_ncu_set_ipv6_dhcp (   NwamuiNcu *self,
                              gboolean        ipv6_dhcp )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));

    g_object_set (G_OBJECT (self),
                  "ipv6_dhcp", ipv6_dhcp,
                  NULL);
}

/**
 * nwamui_ncu_get_ipv6_dhcp:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the ipv6_dhcp.
 *
 **/
extern gboolean
nwamui_ncu_get_ipv6_dhcp (NwamuiNcu *self)
{
    gboolean  ipv6_dhcp = TRUE; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), ipv6_dhcp);

    g_object_get (G_OBJECT (self),
                  "ipv6_dhcp", &ipv6_dhcp,
                  NULL);

    return( ipv6_dhcp );
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
 * nwamui_ncu_set_v4addresses:
 * @nwamui_ncu: a #NwamuiNcu.
 * @v4addresses: Value to set v4addresses to.
 * 
 **/ 
extern void
nwamui_ncu_set_v4addresses (   NwamuiNcu *self,
                              GtkListStore*  v4addresses )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));
    g_assert (v4addresses != NULL );

    if ( v4addresses != NULL ) {
        g_object_set (G_OBJECT (self),
                      "v4addresses", v4addresses,
                      NULL);
    }
}

/**
 * nwamui_ncu_get_v4addresses:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the v4addresses.
 *
 **/
extern GtkListStore*
nwamui_ncu_get_v4addresses (NwamuiNcu *self)
{
    GtkListStore*  v4addresses = NULL; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), v4addresses);

    g_object_get (G_OBJECT (self),
                  "v4addresses", &v4addresses,
                  NULL);

    return( v4addresses );
}

/** 
 * nwamui_ncu_set_v6addresses:
 * @nwamui_ncu: a #NwamuiNcu.
 * @v6addresses: Value to set v6addresses to.
 * 
 **/ 
extern void
nwamui_ncu_set_v6addresses (   NwamuiNcu *self,
                              GtkListStore*  v6addresses )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));
    g_assert (v6addresses != NULL );

    if ( v6addresses != NULL ) {
        g_object_set (G_OBJECT (self),
                      "v6addresses", v6addresses,
                      NULL);
    }
}

/**
 * nwamui_ncu_get_v6addresses:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the v6addresses.
 *
 **/
extern GtkListStore*
nwamui_ncu_get_v6addresses (NwamuiNcu *self)
{
    GtkListStore*  v6addresses = NULL; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), v6addresses);

    g_object_get (G_OBJECT (self),
                  "v6addresses", &v6addresses,
                  NULL);

    return( v6addresses );
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
 * nwamui_ncu_set_activation_mode:
 * @nwamui_ncu: a #NwamuiNcu.
 * @activation_mode: Value to set activation_mode to.
 * 
 **/ 
extern void
nwamui_ncu_set_activation_mode (   NwamuiNcu                      *self,
                                    nwamui_cond_activation_mode_t        activation_mode )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));
    g_assert (activation_mode >= NWAMUI_COND_ACTIVATION_MODE_MANUAL && activation_mode <= NWAMUI_COND_ACTIVATION_MODE_LAST );

    g_object_set (G_OBJECT (self),
                  "activation_mode", (gint)activation_mode,
                  NULL);
}

/**
 * nwamui_ncu_get_activation_mode:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the activation_mode.
 *
 **/
extern nwamui_cond_activation_mode_t
nwamui_ncu_get_activation_mode (NwamuiNcu *self)
{
    gint  activation_mode = NWAMUI_COND_ACTIVATION_MODE_MANUAL; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), activation_mode);

    g_object_get (G_OBJECT (self),
                  "activation_mode", &activation_mode,
                  NULL);

    return( (nwamui_cond_activation_mode_t)activation_mode );
}

/** 
 * nwamui_ncu_set_enabled:
 * @nwamui_ncu: a #NwamuiNcu.
 * @enabled: Value to set enabled to.
 * 
 **/ 
extern void
nwamui_ncu_set_enabled (   NwamuiNcu      *self,
                           gboolean        enabled )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));

    g_object_set (G_OBJECT (self),
                  "enabled", enabled,
                  NULL);
}

/**
 * nwamui_ncu_get_enabled:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the enabled.
 *
 **/
extern gboolean
nwamui_ncu_get_enabled (NwamuiNcu *self)
{
    gboolean  enabled = FALSE; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), enabled);

    g_object_get (G_OBJECT (self),
                  "enabled", &enabled,
                  NULL);

    return( enabled );
}

/** 
 * nwamui_ncu_set_priority_group:
 * @nwamui_ncu: a #NwamuiNcu.
 * @priority_group: Value to set priority_group to.
 * 
 **/ 
extern void
nwamui_ncu_set_priority_group (   NwamuiNcu *self,
                                  gint       priority_group )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));
    g_assert (priority_group >= 0 && priority_group <= G_MAXUINT );

    g_object_set (G_OBJECT (self),
                  "priority_group", priority_group,
                  NULL);
}

/**
 * nwamui_ncu_get_priority_group:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the priority_group.
 *
 **/
extern gint
nwamui_ncu_get_priority_group (NwamuiNcu *self)
{
    gint  priority_group = 0; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), priority_group);

    g_object_get (G_OBJECT (self),
                  "priority_group", &priority_group,
                  NULL);

    return( priority_group );
}

/** 
 * nwamui_ncu_set_priority_group_mode:
 * @nwamui_ncu: a #NwamuiNcu.
 * @priority_group_mode: Value to set priority_group_mode to.
 * 
 **/ 
extern void
nwamui_ncu_set_priority_group_mode (   NwamuiNcu                       *self,
                                       nwamui_cond_priority_group_mode_t     priority_group_mode )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));
    g_assert (priority_group_mode >= NWAMUI_COND_PRIORITY_GROUP_MODE_EXCLUSIVE && priority_group_mode <= NWAMUI_COND_PRIORITY_GROUP_MODE_LAST );

    g_object_set (G_OBJECT (self),
                  "priority_group_mode", (gint)priority_group_mode,
                  NULL);
}

/**
 * nwamui_ncu_get_priority_group_mode:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the priority_group_mode.
 *
 **/
extern nwamui_cond_priority_group_mode_t
nwamui_ncu_get_priority_group_mode (NwamuiNcu *self)
{
    gint  priority_group_mode = NWAMUI_COND_PRIORITY_GROUP_MODE_EXCLUSIVE; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), priority_group_mode);

    g_object_get (G_OBJECT (self),
                  "priority_group_mode", &priority_group_mode,
                  NULL);

    return( (nwamui_cond_priority_group_mode_t)priority_group_mode );
}


/** 
 * nwamui_ncu_set_selection_conditions:
 * @nwamui_ncu: a #NwamuiNcu.
 * @selected_ncus: A GList of NCUs that are to be checked.
 * 
 **/ 
extern void
nwamui_ncu_set_selection_conditions( NwamuiNcu*                   self,
                                     GList*                       conditions )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));

    g_object_set (G_OBJECT (self),
              "conditions", conditions,
              NULL);
    
}

/** 
 * nwamui_ncu_get_selection_conditions:
 * @nwamui_ncu: a #NwamuiNcu.
 * returns: A GList of conditions that are to be checked.
 * 
 **/ 
extern GList*
nwamui_ncu_get_selection_conditions( NwamuiNcu* self )
{
    GList*  conditions = NULL;

    g_return_val_if_fail (NWAMUI_IS_NCU (self), conditions );

    g_object_get (G_OBJECT (self),
              "conditions", &conditions,
              NULL);
    
    return( conditions );
}


static void
nwamui_ncu_finalize (NwamuiNcu *self)
{
    if (self->prv->v4addresses != NULL ) {
        g_object_unref( G_OBJECT(self->prv->v4addresses) );
    }

    if (self->prv->v6addresses != NULL ) {
        g_object_unref( G_OBJECT(self->prv->v6addresses) );
    }

    g_free (self->prv); 
    self->prv = NULL;

    parent_class->finalize (G_OBJECT (self));
}

static gchar*
get_nwam_ncu_string_prop( nwam_ncu_handle_t ncu, const char* prop_name )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    gchar*              retval = NULL;
    char*               value = NULL;

    g_return_val_if_fail( prop_name != NULL, retval );

    if ( ncu == NULL ) {
        return( retval );
    }

    if ( (nerr = nwam_ncu_get_prop_type( prop_name, &nwam_type ) ) != NWAM_SUCCESS 
         || nwam_type != NWAM_VALUE_TYPE_STRING ) {
        g_warning("Unexpected type for ncu property %s - got %d\n", prop_name, nwam_type );
        return retval;
    }

    if ( (nerr = nwam_ncu_get_prop_value (ncu, prop_name, &nwam_data)) != NWAM_SUCCESS ) {
        g_debug("No value for ncu property %s, error = %s\n", prop_name, nwam_strerror( nerr ) );
        return retval;
    }

    if ( (nerr = nwam_value_get_string(nwam_data, &value )) != NWAM_SUCCESS ) {
        g_debug("Unable to get string value for ncu property %s, error = %s\n", prop_name, nwam_strerror( nerr ) );
        return retval;
    }

    if ( value != NULL ) {
        retval  = g_strdup ( value );
    }

    nwam_value_free(nwam_data);
    
    return( retval );
}

static gboolean
prop_is_readonly( gchar* prop_name ) 
{
    boolean_t       read_only = B_FALSE;
    nwam_error_t    nerr;

    if ( (nerr = nwam_ncu_prop_read_only( prop_name, &read_only ) ) != NWAM_SUCCESS ) {
        g_warning("Unable to get read-only status for ncu property %s: %s", prop_name, nwam_strerror(nerr) );
        return FALSE;
    }

    return( (read_only == B_TRUE)?TRUE:FALSE );
}

static gboolean
set_nwam_ncu_string_prop( nwam_ncu_handle_t ncu, const char* prop_name, const gchar* str )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    gboolean            retval = FALSE;
    g_return_val_if_fail( prop_name != NULL, retval );

    if ( ncu == NULL ) {
        return( retval );
    }

    if ( (nerr = nwam_ncu_get_prop_type( prop_name, &nwam_type ) ) != NWAM_SUCCESS 
         || nwam_type != NWAM_VALUE_TYPE_STRING ) {
        g_warning("Unexpected type for ncu property %s - got %d\n", prop_name, nwam_type );
        return retval;
    }

    if ( prop_is_readonly( prop_name ) ) {
        g_error("Attempting to set a read-only ncu property %s", prop_name );
        return retval;
    }

    if ( (nerr = nwam_value_create_string( (char*)str, &nwam_data )) != NWAM_SUCCESS ) {
        g_debug("Error creating a string value for string %s", str?str:"NULL");
        return retval;
    }

    if ( (nerr = nwam_ncu_set_prop_value (ncu, prop_name, nwam_data)) != NWAM_SUCCESS ) {
        g_debug("Unable to set value for ncu property %s, error = %s\n", prop_name, nwam_strerror( nerr ) );
    }
    else {
        retval = TRUE;
    }

    nwam_value_free(nwam_data);
    
    return( retval );
}

static gchar**
get_nwam_ncu_string_array_prop( nwam_ncu_handle_t ncu, const char* prop_name )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    gchar**             retval = NULL;
    char**              value = NULL;
    uint_t              num = 0;

    g_return_val_if_fail( prop_name != NULL, retval );

    if ( ncu == NULL ) {
        return( retval );
    }

    if ( (nerr = nwam_ncu_get_prop_type( prop_name, &nwam_type ) ) != NWAM_SUCCESS 
         || nwam_type != NWAM_VALUE_TYPE_STRING ) {
        g_warning("Unexpected type for ncu property %s - got %d\n", prop_name, nwam_type );
        return retval;
    }

    if ( (nerr = nwam_ncu_get_prop_value (ncu, prop_name, &nwam_data)) != NWAM_SUCCESS ) {
        g_debug("No value for ncu property %s, error = %s\n", prop_name, nwam_strerror( nerr ) );
        return retval;
    }

    if ( (nerr = nwam_value_get_string_array(nwam_data, &value, &num )) != NWAM_SUCCESS ) {
        g_debug("Unable to get string value for ncu property %s, error = %s\n", prop_name, nwam_strerror( nerr ) );
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
set_nwam_ncu_string_array_prop( nwam_ncu_handle_t ncu, const char* prop_name, char** strs, guint len )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    gboolean            retval = FALSE;

    g_return_val_if_fail( prop_name != NULL, retval );

    if ( ncu == NULL ) {
        return( retval );
    }

    if ( (nerr = nwam_ncu_get_prop_type( prop_name, &nwam_type ) ) != NWAM_SUCCESS 
         || nwam_type != NWAM_VALUE_TYPE_STRING ) {
        g_warning("Unexpected type for ncu property %s - got %d\n", prop_name, nwam_type );
        return retval;
    }

    if ( strs == NULL ) {
        return retval;
    }


    if ( len == 0 ) { /* Assume a strv, i.e. NULL terminated list, otherwise strs would be NULL */
        int i;

        for( i = 0; strs != NULL && strs[i] != NULL; i++ ) {
            /* Do Nothing, just count. */
        }
        len = i;
    }

    if ( prop_is_readonly( prop_name ) ) {
        g_error("Attempting to set a read-only ncu property %s", prop_name );
        return retval;
    }

    if ( (nerr = nwam_value_create_string_array (strs, len, &nwam_data )) != NWAM_SUCCESS ) {
        g_debug("Error creating a value for string array 0x%08X", strs);
        return retval;
    }

    if ( (nerr = nwam_ncu_set_prop_value (ncu, prop_name, nwam_data)) != NWAM_SUCCESS ) {
        g_debug("Unable to set value for ncu property %s, error = %s\n", prop_name, nwam_strerror( nerr ) );
    }
    else {
        retval = TRUE;
    }

    nwam_value_free(nwam_data);
    
    return( retval );
}

static gboolean
get_nwam_ncu_boolean_prop( nwam_ncu_handle_t ncu, const char* prop_name )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    boolean_t           value = FALSE;

    g_return_val_if_fail( prop_name != NULL, value );

    if ( ncu == NULL ) {
        return( value );
    }

    if ( (nerr = nwam_ncu_get_prop_type( prop_name, &nwam_type ) ) != NWAM_SUCCESS ) {
        g_warning("Unexpected error for ncu property %s - %s\n", prop_name, nwam_strerror( nerr ) );
        return value;
    }

    if ( nwam_type != NWAM_VALUE_TYPE_BOOLEAN ) {
        g_warning("Unexpected type for ncu property %s - got %d\n", prop_name, nwam_type );
        return value;
    }

    if ( (nerr = nwam_ncu_get_prop_value (ncu, prop_name, &nwam_data)) != NWAM_SUCCESS ) {
        g_debug("No value for ncu property %s, error = %s\n", prop_name, nwam_strerror( nerr ) );
        return value;
    }

    if ( (nerr = nwam_value_get_boolean(nwam_data, &value )) != NWAM_SUCCESS ) {
        g_debug("Unable to get boolean value for ncu property %s, error = %s\n", prop_name, nwam_strerror( nerr ) );
        return value;
    }

    nwam_value_free(nwam_data);

    return( (gboolean)value );
}

static gboolean
set_nwam_ncu_boolean_prop( nwam_ncu_handle_t ncu, const char* prop_name, gboolean bool_value )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    gboolean            retval = FALSE;
    g_return_val_if_fail( prop_name != NULL, retval );

    if ( ncu == NULL ) {
        return( retval );
    }

    if ( (nerr = nwam_ncu_get_prop_type( prop_name, &nwam_type ) ) != NWAM_SUCCESS 
         || nwam_type != NWAM_VALUE_TYPE_BOOLEAN ) {
        g_warning("Unexpected type for ncu property %s - got %d\n", prop_name, nwam_type );
        return retval;
    }

    if ( prop_is_readonly( prop_name ) ) {
        g_error("Attempting to set a read-only ncu property %s", prop_name );
        return retval;
    }

    if ( (nerr = nwam_value_create_boolean ((boolean_t)bool_value, &nwam_data )) != NWAM_SUCCESS ) {
        g_debug("Error creating a boolean value" );
        return retval;
    }

    if ( (nerr = nwam_ncu_set_prop_value (ncu, prop_name, nwam_data)) != NWAM_SUCCESS ) {
        g_debug("Unable to set value for ncu property %s, error = %s\n", prop_name, nwam_strerror( nerr ) );
    }
    else {
        retval = TRUE;
    }

    nwam_value_free(nwam_data);
    
    return( retval );
}


static guint64
get_nwam_ncu_uint64_prop( nwam_ncu_handle_t ncu, const char* prop_name )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    uint64_t            value = 0;

    g_return_val_if_fail( prop_name != NULL, value );

    if ( ncu == NULL ) {
        return( value );
    }

    if ( (nerr = nwam_ncu_get_prop_type( prop_name, &nwam_type ) ) != NWAM_SUCCESS 
         || nwam_type != NWAM_VALUE_TYPE_UINT64 ) {
        g_warning("Unexpected type for ncu property %s - got %d\n", prop_name, nwam_type );
        return value;
    }

    if ( (nerr = nwam_ncu_get_prop_value (ncu, prop_name, &nwam_data)) != NWAM_SUCCESS ) {
        g_debug("No value for ncu property %s, error = %s\n", prop_name, nwam_strerror( nerr ) );
        return value;
    }

    if ( (nerr = nwam_value_get_uint64(nwam_data, &value )) != NWAM_SUCCESS ) {
        g_debug("Unable to get uint64 value for ncu property %s, error = %s\n", prop_name, nwam_strerror( nerr ) );
        return value;
    }

    nwam_value_free(nwam_data);

    return( (guint64)value );
}

static gboolean
set_nwam_ncu_uint64_prop( nwam_ncu_handle_t ncu, const char* prop_name, guint64 value )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    gboolean            retval = FALSE;
    g_return_val_if_fail( prop_name != NULL, retval );

    if ( ncu == NULL ) {
        return( retval );
    }

    if ( (nerr = nwam_ncu_get_prop_type( prop_name, &nwam_type ) ) != NWAM_SUCCESS 
         || nwam_type != NWAM_VALUE_TYPE_UINT64 ) {
        g_warning("Unexpected type for ncu property %s - got %d\n", prop_name, nwam_type );
        return retval;
    }

    if ( prop_is_readonly( prop_name ) ) {
        g_error("Attempting to set a read-only ncu property %s", prop_name );
        return retval;
    }

    if ( (nerr = nwam_value_create_uint64 (value, &nwam_data )) != NWAM_SUCCESS ) {
        g_debug("Error creating a uint64 value" );
        return retval;
    }

    if ( (nerr = nwam_ncu_set_prop_value (ncu, prop_name, nwam_data)) != NWAM_SUCCESS ) {
        g_debug("Unable to set value for ncu property %s, error = %s\n", prop_name, nwam_strerror( nerr ) );
    }
    else {
        retval = TRUE;
    }

    nwam_value_free(nwam_data);
    
    return( retval );
}

static guint64* 
get_nwam_ncu_uint64_array_prop( nwam_ncu_handle_t ncu, const char* prop_name , guint *out_num )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    uint64_t           *value = NULL;
    uint_t              num = 0;
    guint64            *retval = NULL;

    g_return_val_if_fail( prop_name != NULL && out_num != NULL, retval );

    if ( ncu == NULL ) {
        return( retval );
    }

    if ( (nerr = nwam_ncu_get_prop_type( prop_name, &nwam_type ) ) != NWAM_SUCCESS 
         || nwam_type != NWAM_VALUE_TYPE_UINT64 ) {
        g_warning("Unexpected type for ncu property %s - got %d\n", prop_name, nwam_type );
        return value;
    }

    if ( (nerr = nwam_ncu_get_prop_value (ncu, prop_name, &nwam_data)) != NWAM_SUCCESS ) {
        g_debug("No value for ncu property %s, error = %s\n", prop_name, nwam_strerror( nerr ) );
        return value;
    }

    if ( (nerr = nwam_value_get_uint64_array(nwam_data, &value, &num )) != NWAM_SUCCESS ) {
        g_debug("Unable to get uint64 value for ncu property %s, error = %s\n", prop_name, nwam_strerror( nerr ) );
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
set_nwam_ncu_uint64_array_prop( nwam_ncu_handle_t ncu, const char* prop_name, 
                                const guint64 *value_array, guint len )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    gboolean            retval = FALSE;
    g_return_val_if_fail( prop_name != NULL, retval );

    if ( ncu == NULL ) {
        return( retval );
    }

    if ( (nerr = nwam_ncu_get_prop_type( prop_name, &nwam_type ) ) != NWAM_SUCCESS 
         || nwam_type != NWAM_VALUE_TYPE_UINT64 ) {
        g_warning("Unexpected type for ncu property %s - got %d\n", prop_name, nwam_type );
        return retval;
    }

    if ( prop_is_readonly( prop_name ) ) {
        g_error("Attempting to set a read-only ncu property %s", prop_name );
        return retval;
    }

    if ( (nerr = nwam_value_create_uint64_array ((uint64_t*)value_array, len, &nwam_data )) != NWAM_SUCCESS ) {
        g_debug("Error creating a uint64 array value" );
        return retval;
    }

    if ( (nerr = nwam_ncu_set_prop_value (ncu, prop_name, nwam_data)) != NWAM_SUCCESS ) {
        g_debug("Unable to set value for ncu property %s, error = %s\n", prop_name, nwam_strerror( nerr ) );
    }
    else {
        retval = TRUE;
    }

    nwam_value_free(nwam_data);
    
    return( retval );
}

/*
 * NCU Status Messages - read directly from system, not from NWAM 
 */

/* Get signal percent from system */
extern const gchar*
nwamui_ncu_get_signal_strength_string( NwamuiNcu* self )
{
    dladm_handle_t          handle;
    datalink_id_t           linkid;
    dladm_wlan_linkattr_t	attr;
    const gchar*            signal_str = _("None");
    
    g_return_val_if_fail( NWAMUI_IS_NCU( self ), signal_str );

    if ( self->prv->ncu_type != NWAMUI_NCU_TYPE_WIRELESS ) {
        return( signal_str );
    }

    if ( dladm_open( &handle ) != DLADM_STATUS_OK ) {
        g_warning("Error creating dladm handle" );
        return( signal_str );
    }
    if ( dladm_name2info( handle, self->prv->device_name, &linkid, NULL, NULL, NULL ) != DLADM_STATUS_OK ) {
        dladm_close( handle );
        g_warning("Unable to map device to linkid");
        return( signal_str );
    }
    else {
        dladm_status_t status = dladm_wlan_get_linkattr(handle, linkid, &attr);
        if (status != DLADM_STATUS_OK ) {
            g_error("cannot get link attributes for %s", self->prv->device_name );
        }
        else {
            if ( attr.la_valid |= DLADM_WLAN_LINKATTR_WLAN &&
                 attr.la_wlan_attr.wa_valid & DLADM_WLAN_ATTR_STRENGTH ) {
                switch ( attr.la_wlan_attr.wa_strength ) {
                    case DLADM_WLAN_STRENGTH_VERY_WEAK:
                        signal_str = nwamui_wifi_net_convert_strength_to_string(NWAMUI_WIFI_STRENGTH_VERY_WEAK);
                        break;
                    case DLADM_WLAN_STRENGTH_WEAK:
                        signal_str = nwamui_wifi_net_convert_strength_to_string(NWAMUI_WIFI_STRENGTH_WEAK);
                        break;
                    case DLADM_WLAN_STRENGTH_GOOD:
                        signal_str = nwamui_wifi_net_convert_strength_to_string(NWAMUI_WIFI_STRENGTH_GOOD);
                        break;
                    case DLADM_WLAN_STRENGTH_VERY_GOOD:
                        signal_str = nwamui_wifi_net_convert_strength_to_string(NWAMUI_WIFI_STRENGTH_VERY_GOOD);
                        break;
                    case DLADM_WLAN_STRENGTH_EXCELLENT:
                        signal_str = nwamui_wifi_net_convert_strength_to_string(NWAMUI_WIFI_STRENGTH_EXCELLENT);
                        break;
                    default:
                        break;
                }
            }
        }
    }

    dladm_close( handle );
    
    return( signal_str );
}

static guint64
get_ifflags(const char *name, sa_family_t family)
{
	icfg_if_t intf;
	icfg_handle_t h;
	uint64_t flags = 0;

	(void) strlcpy(intf.if_name, name, sizeof (intf.if_name));
	intf.if_protocol = family;

	if (icfg_open(&h, &intf) != ICFG_SUCCESS)
		return (0);

	if (icfg_get_flags(h, &flags) != ICFG_SUCCESS) {
		/*
		 * Interfaces can be ripped out from underneath us (for example
		 * by DHCP).  We don't want to spam the console for those.
		 */
		if (errno == ENOENT)
			g_debug("get_ifflags: icfg_get_flags failed for '%s'", name);
		else
			g_debug("get_ifflags: icfg_get_flags %s af %d: %s", name, family, strerror( errno ));

		flags = 0;
	}
	icfg_close(h);

	return (flags);
}

static gboolean
interface_has_addresses(const char *ifname, sa_family_t family)
{
	char msg[128];
	icfg_if_t intf;
	icfg_handle_t h;
	struct sockaddr_in sin;
	socklen_t addrlen = sizeof (struct sockaddr_in);
	int prefixlen = 0;

	(void) strlcpy(intf.if_name, ifname, sizeof (intf.if_name));
	intf.if_protocol = family;
	if (icfg_open(&h, &intf) != ICFG_SUCCESS) {
		g_debug( "icfg_open failed on interface %s", ifname);
		return( FALSE );
	}
	if (icfg_get_addr(h, (struct sockaddr *)&sin, &addrlen, &prefixlen,
	    B_TRUE) != ICFG_SUCCESS) {
		g_debug( "icfg_get_addr failed on interface %s for family %s", ifname, (family == AF_INET6)?"v6":"v4");
		icfg_close(h);
		return( FALSE );
	}
	icfg_close(h);

    return( TRUE );
}

static gchar*
get_interface_address_str(const char *ifname, sa_family_t family)
{
	gchar              *string = NULL;
	gchar              *address = NULL;
	int                 prefixlen = 0;
    gboolean            is_dhcp = FALSE;
    gchar*              dhcp_str;

    if ( nwamui_util_get_interface_address( ifname, family, &address, &prefixlen, &is_dhcp ) ) {
        if ( is_dhcp ) {
            dhcp_str = _(" (DHCP)");
        }
        else {
            dhcp_str = "";
        }

        switch ( family ) {
            case AF_INET:
                string = g_strdup_printf( _("Address: %s/%d%s"), address, prefixlen, dhcp_str);
                break;
            case AF_INET6:
                string = g_strdup_printf( _("Address(v6): %s/%d%s"), address, prefixlen, dhcp_str);
                break;
            default:
                break;
        }

        g_free( address );
    }

    return( string );
}

static const gchar* status_string_fmt[NWAMUI_STATE_LAST] = {
    "Unknown",
    "Not Connected",
    "Connecting",
    "Connected",
    "Connecting to %s",     /* %s = ESSID */
    "Connected to %s",      /* %s = ESSID */
    "Network unavailable",
    "Cable unplugged"
};

extern nwamui_connection_state_t
nwamui_ncu_get_connection_state( NwamuiNcu* self ) 
{
    dladm_handle_t              handle;
    datalink_id_t               linkid;
    nwamui_connection_state_t   state = NWAMUI_STATE_UNKNOWN;
    gboolean                    if_running = FALSE;
    gboolean                    has_addresses = FALSE;
    guint64                     iff_flags = 0;

    g_return_val_if_fail( NWAMUI_IS_NCU( self ), state );

    if ( dladm_open( &handle ) != DLADM_STATUS_OK ) {
        g_warning("Error creating dladm handle" );
        return ( NWAMUI_STATE_UNKNOWN );
    }

    if ( dladm_name2info( handle, self->prv->device_name, &linkid, NULL, NULL, NULL ) != DLADM_STATUS_OK ) {
        dladm_close( handle );
        g_warning("Unable to map device to linkid");
        return ( NWAMUI_STATE_UNKNOWN );
    }

    if ( ( iff_flags = get_ifflags(self->prv->device_name, AF_INET)) & IFF_RUNNING ) {
        if_running = TRUE;
    }
    else if ( ( iff_flags = get_ifflags(self->prv->device_name, AF_INET6)) & IFF_RUNNING ) {
        /* Attempt v6 too, if v4 failed, we only care if one of them is marked running  */
        if_running = TRUE;
    }

    if ( if_running ) {
        gboolean is_dhcp = (iff_flags & IFF_DHCPRUNNING)?TRUE:FALSE;

        if ( interface_has_addresses( self->prv->device_name, AF_INET ) ) {
            has_addresses = TRUE;
        }
        else if ( interface_has_addresses( self->prv->device_name, AF_INET6 ) ) {
            /* Attempt v6 too, if v4 failed, we only care if one of them has addresses*/
            has_addresses = TRUE;
        }

        if ( has_addresses ) {
            state = NWAMUI_STATE_CONNECTED;
        }
        else if ( is_dhcp ) {
            state = NWAMUI_STATE_CONNECTING;
        }
        else {
            state = NWAMUI_STATE_NOT_CONNECTED;
        }
    }
    else if ( self->prv->ncu_type == NWAMUI_NCU_TYPE_WIRED ) {
        state = NWAMUI_STATE_CABLE_UNPLUGGED;
    }
    else {
        state = NWAMUI_STATE_NOT_CONNECTED;
    }

    if ( if_running && self->prv->ncu_type == NWAMUI_NCU_TYPE_WIRELESS ) {
        /* Change connected/connecting to be wireless equivalent */
        switch( state ) {
            case NWAMUI_STATE_CONNECTING:
                state = NWAMUI_STATE_CONNECTING_ESSID;
                break;
            case NWAMUI_STATE_CONNECTED:
                state = NWAMUI_STATE_CONNECTED_ESSID;
                break;
            default:
                break;
        }
    }

    dladm_close(handle);

    return( state );
}

/*
 * Get a string that describes the status of the system, should be freed by
 * caller using gfree().
 */
extern gchar*
nwamui_ncu_get_connection_state_string( NwamuiNcu* self )
{
    nwamui_connection_state_t   state = NWAMUI_STATE_NOT_CONNECTED;
    gchar*                      status_string = NULL;
    gchar*                      essid = NULL;

    g_return_val_if_fail( NWAMUI_IS_NCU( self ), NULL );

    state = nwamui_ncu_get_connection_state( self );

    switch( state ) {
        case NWAMUI_STATE_UNKNOWN:
        case NWAMUI_STATE_NOT_CONNECTED:
        case NWAMUI_STATE_CONNECTING:
        case NWAMUI_STATE_CONNECTED:
        case NWAMUI_STATE_NETWORK_UNAVAILABLE:
        case NWAMUI_STATE_CABLE_UNPLUGGED:
            status_string = g_strdup( _(status_string_fmt[state]) );
            break;

        case NWAMUI_STATE_CONNECTING_ESSID:
        case NWAMUI_STATE_CONNECTED_ESSID: {
                dladm_handle_t              handle;
                datalink_id_t               linkid;
                dladm_wlan_linkattr_t	    attr;

                if ( dladm_open( &handle ) != DLADM_STATUS_OK ) {
                    g_warning("Error creating dladm handle" );
                }
                else if ( dladm_name2info( handle, self->prv->device_name,
                                           &linkid, NULL, NULL, NULL ) != DLADM_STATUS_OK ) {
                    g_warning("Unable to map device to linkid");
                    dladm_close(handle);
                }
                else if ( dladm_wlan_get_linkattr(handle, linkid, &attr) != DLADM_STATUS_OK ) {
                    g_warning("cannot get link attributes for %s", self->prv->device_name );
                    dladm_close(handle);
                }
                else if ( attr.la_status == DLADM_WLAN_LINK_CONNECTED ) {
                    if ( (attr.la_valid | DLADM_WLAN_LINKATTR_WLAN) &&
                         (attr.la_wlan_attr.wa_valid & DLADM_WLAN_ATTR_ESSID) ) {
                        char cur_essid[DLADM_STRSIZE];
                        dladm_wlan_essid2str( &attr.la_wlan_attr.wa_essid, cur_essid );
                        essid =  g_strdup( cur_essid );
                    }
                    dladm_close(handle);
                }

                status_string = g_strdup_printf( _(status_string_fmt[state]), essid?essid:"UNKNOWN" );

            }
            break;

        default:
            g_error("Unexpected value for connection state %d", (int)state );
            break;
    }

    if ( essid != NULL ) {
        g_free( essid );
    }

    return( status_string );
}

/*
 * Get a string that provides most information about the current state of the
 * ncu, should be freed by caller using gfree().
 */
extern gchar*
nwamui_ncu_get_connection_state_detail_string( NwamuiNcu* self )
{
    nwamui_connection_state_t   state = NWAMUI_STATE_NOT_CONNECTED;
    gchar*                      status_string = NULL;
    gchar*                      addr_part = NULL;
    gchar*                      signal_part = NULL;
    gchar*                      speed_part = NULL;

    g_return_val_if_fail( NWAMUI_IS_NCU( self ), NULL );

    state = nwamui_ncu_get_connection_state( self );

    switch( state ) {
        case NWAMUI_STATE_CONNECTED:
        case NWAMUI_STATE_CONNECTED_ESSID: {
            gchar *v4addr = get_interface_address_str( self->prv->device_name, AF_INET );
            gchar *v6addr = get_interface_address_str( self->prv->device_name, AF_INET6 );

            if ( v4addr != NULL ) {
                addr_part = v4addr;
                v4addr = NULL;
            }
            
            if ( v6addr != NULL ) {
                if ( addr_part != NULL ) {
                    gchar* tmp = addr_part;
                    addr_part = g_strdup_printf( _("%s, %s"), addr_part, v6addr );
                    g_free ( tmp );
                }
                else {
                    addr_part = v6addr;
                    v6addr = NULL;
                }
            }

            if ( addr_part == NULL ) {
                addr_part = g_strdup( _("Address: unassigned") );
            }

            if ( state == NWAMUI_STATE_CONNECTED_ESSID ) {
                signal_part = g_strdup_printf( _("Signal: %s"), nwamui_ncu_get_signal_strength_string( self ));
            }
            speed_part = g_strdup_printf( _("Speed: %d Mb/s"), nwamui_ncu_get_speed( self ));

            if ( signal_part == NULL ) {
                status_string = g_strdup_printf( _("%s, %s"), addr_part, speed_part );
            }
            else {
                status_string = g_strdup_printf( _("%s, %s, %s"), addr_part, signal_part, speed_part );
            }
            }
            break;
        default:
            status_string = g_strdup(_("Not connected"));
    }

    return( status_string );
}

static gboolean
get_kstat_uint64 (const gchar *device, const gchar* stat_name, uint64_t *rval )
{
    kstat_ctl_t      *kc = NULL;
    kstat_t          *ksp;
    kstat_named_t    *kn;

    if ( device == NULL || stat_name == NULL || rval == NULL ) {
        return( FALSE );
    }

    if ((kc = kstat_open ()) == NULL) {
        g_warning ("Cannot open /dev/kstat: %s", g_strerror (errno));
        return( FALSE );
    }

    if ((ksp = kstat_lookup (kc, "link", 0, (char*)device)) == NULL) {
        kstat_close (kc);
        g_warning("Cannot find information on interface '%s'", device);
        return( FALSE );
    }

    if (kstat_read (kc, ksp, NULL) < 0) {
        kstat_close (kc);
        g_warning("Cannot read kstat");
        return( FALSE );
    }

    if ((kn = kstat_data_lookup (ksp, (char*)stat_name )) != NULL) {
        *rval = kn->value.ui64;
        return( TRUE );
    }

    kstat_close (kc);

    return( FALSE );
}

/* Callbacks */

static void
object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
    NwamuiNcu* self = NWAMUI_NCU(data);
    gchar*     name = self->prv->device_name;

    g_debug("NwamuiNcu: %s: notify %s changed\n", name?name:"", arg1->name);
}

static void 
ip_row_inserted_or_changed_cb (GtkTreeModel *tree_model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
    NwamuiNcu* self = NWAMUI_NCU(data);
    gboolean is_v6 = (tree_model == GTK_TREE_MODEL(self->prv->v6addresses));

    self->prv->nwam_ncu_ip_modified = TRUE;

    /* Chain events in the tree to the NCU object*/
    g_object_notify(G_OBJECT(self), is_v6?"v6addresses":"v4addresses");
}

static void 
ip_row_deleted_cb (GtkTreeModel *tree_model, GtkTreePath *path, gpointer data)
{
    NwamuiNcu* self = NWAMUI_NCU(data);
    gboolean is_v6 = (tree_model == GTK_TREE_MODEL(self->prv->v6addresses));

    self->prv->nwam_ncu_ip_modified = TRUE;

    /* Chain events in the tree to the NCU object*/
    g_object_notify(G_OBJECT(self), is_v6?"v6addresses":"v4addresses");
}


