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
#include <sys/types.h>
#include <sys/dlpi.h>
#include <libdllink.h>
#include <libdlwlan.h>

struct _NwamuiNcuPrivate {
        gboolean                        initialisation;

        NwamuiNcp*                      ncp;  /* Parent NCP */
        nwam_ncu_handle_t               nwam_ncu_phys;
        gboolean                        nwam_ncu_phys_modified;
        nwam_ncu_handle_t               nwam_ncu_ip;
        gboolean                        nwam_ncu_ip_modified;
#ifdef TUNNEL_SUPPORT
        nwam_ncu_handle_t               nwam_ncu_iptun;
        gboolean                        nwam_ncu_iptun_modified;
#endif /* TUNNEL_SUPPORT */

        gboolean                        active;
        gboolean                        enabled;

        gboolean                        link_state_flag;
        gboolean                        if_state_flag;

        /* General Properties */
        gchar*                          vanity_name;
        gchar*                          device_name;
        gchar*                          display_name;
        nwamui_ncu_type_t               ncu_type;

#ifdef TUNNEL_SUPPORT
        /* IPTun Properties */
        nwam_iptun_type_t               tun_type;
        gchar*                          tun_tsrc;
        gchar*                          tun_tdst;
        gchar*                          tun_encr;
        gchar*                          tun_encr_auth;
        gchar*                          tun_auth;
#endif /* TUNNEL_SUPPORT */

        gboolean                        ipv4_active;
        gboolean                        ipv4_has_dhcp;
        NwamuiIp*                       ipv4_dhcp_ip;
        NwamuiIp*                       ipv4_primary_static_ip;
        GtkListStore*                   v4addresses;

        gboolean                        ipv6_active;
        gboolean                        ipv6_has_dhcp; 
        gboolean                        ipv6_has_auto_conf; 
        NwamuiIp*                       ipv6_primary_static_ip;
        GtkListStore*                   v6addresses;

        /* Wireless Info */
        NwamuiWifiNet*                  wifi_info;
        GHashTable                     *wifi_hash_table;

    /* For caching link state */
    nwam_state_t     link_state;
    nwam_aux_state_t link_aux_state;
    nwam_state_t     iface_state;
    nwam_aux_state_t iface_aux_state;

    /* For caching gui connection state */
    nwamui_connection_state_t state;
};

enum {
        PROP_NCP = 1,
        PROP_DEVICE_NAME,
        PROP_AUTO_PUSH,
        PROP_PHY_ADDRESS,
        PROP_NCU_TYPE,
        PROP_READONLY,
        PROP_SPEED,
        PROP_MTU,
        PROP_IPV4_ACTIVE,
        PROP_IPV4_HAS_DHCP,
        PROP_IPV4_ADDRESS,
        PROP_IPV4_SUBNET,
        PROP_IPV4_DEFAULT_ROUTE,
        PROP_V4ADDRESSES,
        PROP_IPV6_ACTIVE,
        PROP_IPV6_HAS_DHCP,
        PROP_IPV6_HAS_AUTO_CONF,
        PROP_IPV6_ADDRESS,
        PROP_IPV6_PREFIX,
        PROP_IPV6_DEFAULT_ROUTE,
        PROP_V6ADDRESSES,
        PROP_WIFI_INFO,
        PROP_PRIORITY_GROUP,
        PROP_PRIORITY_GROUP_MODE
};

#define NWAMUI_NCU_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE((o), NWAMUI_TYPE_NCU, NwamuiNcuPrivate))

static void nwamui_ncu_set_property ( GObject         *object,
                                      guint            prop_id,
                                      const GValue    *value,
                                      GParamSpec      *pspec);

static void nwamui_ncu_get_property ( GObject         *object,
                                      guint            prop_id,
                                      GValue          *value,
                                      GParamSpec      *pspec);

static void nwamui_ncu_finalize (     NwamuiNcu *self);

static void populate_iptun_ncu_data(NwamuiNcu *ncu, nwam_ncu_handle_t nwam_ncu);
static void populate_ip_ncu_data(NwamuiNcu *ncu, nwam_ncu_handle_t nwam_ncu);
static void populate_phys_ncu_data(NwamuiNcu *ncu, nwam_ncu_handle_t nwam_ncu);

static void nwamui_ncu_set_display_name ( NwamuiNcu *self );
static void set_modified_flag( NwamuiNcu* self, nwam_ncu_class_t ncu_class, gboolean value );

static gboolean     delete_nwam_ncu_prop( nwam_ncu_handle_t ncu, const char* prop_name );

static gboolean     get_nwam_ncu_boolean_prop( nwam_ncu_handle_t ncu, const char* prop_name );
static gboolean     set_nwam_ncu_boolean_prop( nwam_ncu_handle_t ncu, const char* prop_name, gboolean bool_value );

static gchar*       get_nwam_ncu_string_prop( nwam_ncu_handle_t ncu, const char* prop_name );
static gboolean     set_nwam_ncu_string_prop( nwam_ncu_handle_t ncu, const char* prop_name, const char* str );

static gchar**      get_nwam_ncu_string_array_prop( nwam_ncu_handle_t ncu, const char* prop_name, guint *len );
static gboolean     set_nwam_ncu_string_array_prop( nwam_ncu_handle_t ncu, const char* prop_name, char** strs, guint len);

static guint64      get_nwam_ncu_uint64_prop( nwam_ncu_handle_t ncu, const char* prop_name );
static gboolean     set_nwam_ncu_uint64_prop( nwam_ncu_handle_t ncu, const char* prop_name, guint64 value );

static guint64*     get_nwam_ncu_uint64_array_prop( nwam_ncu_handle_t ncu, const char* prop_name , guint* out_num );
static gboolean     set_nwam_ncu_uint64_array_prop( nwam_ncu_handle_t ncu, const char* prop_name , 
                                                    const guint64* value, guint len );

static gboolean     get_kstat_uint64 (const gchar *device, const gchar* stat_name, uint64_t *rval );

static gboolean     interface_has_addresses(const char *ifname, sa_family_t family);

static gchar*       get_interface_address_str( NwamuiNcu *ncu, sa_family_t family);

static void         nwamui_object_real_set_handle(NwamuiObject *object, const gpointer handle);
static const gchar* nwamui_ncu_get_vanity_name ( NwamuiObject *object );
static gboolean     nwamui_ncu_set_vanity_name ( NwamuiObject *object, const gchar* name );
static gint         nwamui_object_real_sort(NwamuiObject *object, NwamuiObject *other, guint sort_by);
static gboolean     nwamui_object_real_commit( NwamuiObject* object );
static void         nwamui_object_real_reload(NwamuiObject* object);
static gboolean     nwamui_object_real_destroy( NwamuiObject* object );
static gboolean     nwamui_object_real_is_modifiable(NwamuiObject *object);
static void         nwamui_object_real_set_active ( NwamuiObject *object, gboolean active );
static gboolean     nwamui_object_real_get_active ( NwamuiObject *object );
static void         nwamui_object_real_set_enabled ( NwamuiObject *object, gboolean enabled );
static gboolean     nwamui_object_real_get_enabled ( NwamuiObject *object );
static void         nwamui_object_real_set_activation_mode ( NwamuiObject *object, gint  activation_mode );
static gint         nwamui_object_real_get_activation_mode ( NwamuiObject *object );
static NwamuiObject* nwamui_object_real_clone(NwamuiObject *object, const gchar *name, NwamuiObject *parent);
static gboolean     nwamui_object_real_has_modifications(NwamuiObject* object);
static void         nwamui_object_set_interface_nwam_state(NwamuiObject *object, nwam_state_t state, nwam_aux_state_t aux_state);

/* Callbacks */
static void ip_row_inserted_or_changed_cb (GtkTreeModel *tree_model, GtkTreePath *path, GtkTreeIter *iter, gpointer user_data); 

static void ip_row_deleted_cb (GtkTreeModel *tree_model, GtkTreePath *path, gpointer user_data);

static void wireless_notify_cb(GObject *gobject, GParamSpec *arg1, gpointer user_data);

G_DEFINE_TYPE (NwamuiNcu, nwamui_ncu, NWAMUI_TYPE_OBJECT)

static void
nwamui_ncu_class_init(NwamuiNcuClass *klass)
{
    /* Pointer to GObject Part of Class */
    GObjectClass *gobject_class = (GObjectClass*) klass;
    NwamuiObjectClass *nwamuiobject_class = NWAMUI_OBJECT_CLASS(klass);
        
    /* Override Some Function Pointers */
    gobject_class->set_property = nwamui_ncu_set_property;
    gobject_class->get_property = nwamui_ncu_get_property;
    gobject_class->finalize = (void (*)(GObject*)) nwamui_ncu_finalize;

    /* object get/set name in NCU are VANITY NAME */
    nwamuiobject_class->set_handle = nwamui_object_real_set_handle;
    nwamuiobject_class->get_name = nwamui_ncu_get_vanity_name;
    nwamuiobject_class->set_name = nwamui_ncu_set_vanity_name;
    nwamuiobject_class->get_activation_mode = nwamui_object_real_get_activation_mode;
    nwamuiobject_class->set_activation_mode = nwamui_object_real_set_activation_mode;
    nwamuiobject_class->get_active = nwamui_object_real_get_active;
    nwamuiobject_class->set_active = nwamui_object_real_set_active;
    nwamuiobject_class->get_enabled = nwamui_object_real_get_enabled;
    nwamuiobject_class->set_enabled = nwamui_object_real_set_enabled;
    nwamuiobject_class->get_nwam_state = nwamui_ncu_get_interface_nwam_state;
    nwamuiobject_class->set_nwam_state = nwamui_object_set_interface_nwam_state;
    nwamuiobject_class->sort = nwamui_object_real_sort;
    nwamuiobject_class->commit = nwamui_object_real_commit;
    nwamuiobject_class->reload = nwamui_object_real_reload;
    nwamuiobject_class->destroy = nwamui_object_real_destroy;
    nwamuiobject_class->is_modifiable = nwamui_object_real_is_modifiable;
    nwamuiobject_class->has_modifications = nwamui_object_real_has_modifications;
    nwamuiobject_class->clone = nwamui_object_real_clone;

	g_type_class_add_private(klass, sizeof(NwamuiNcuPrivate));

    /* Create some properties */
    g_object_class_install_property (gobject_class,
                                     PROP_NCP,
                                     g_param_spec_object ("ncp",
                                                          _("NCP that NCU child of"),
                                                          _("NCP that NCU child of"),
                                                          NWAMUI_TYPE_NCP,
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
                                     PROP_IPV4_ACTIVE,
                                     g_param_spec_boolean ("ipv4_active",
                                                           _("ipv4_active"),
                                                           _("ipv4_active"),
                                                          FALSE,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_IPV4_HAS_DHCP,
                                     g_param_spec_boolean ("ipv4_has_dhcp",
                                                          _("ipv4_has_dhcp"),
                                                          _("ipv4_has_dhcp"),
                                                          FALSE,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_IPV4_ADDRESS,
                                     g_param_spec_string ("ipv4_address",
                                                          _("ipv4_address"),
                                                          _("ipv4_address"),
                                                          "",
                                                          G_PARAM_READABLE));

    g_object_class_install_property (gobject_class,
                                     PROP_IPV4_SUBNET,
                                     g_param_spec_string ("ipv4_subnet",
                                                          _("ipv4_subnet"),
                                                          _("ipv4_subnet"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_IPV4_DEFAULT_ROUTE,
                                     g_param_spec_string ("ipv4_default_route",
                                                          _("ipv4_default_route"),
                                                          _("ipv4_default_route"),
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
                                     PROP_IPV6_HAS_DHCP,
                                     g_param_spec_boolean("ipv6_has_dhcp",
                                                          _("ipv6_has_dhcp"),
                                                          _("ipv6_has_dhcp"),
                                                          FALSE,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_IPV6_HAS_AUTO_CONF,
                                     g_param_spec_boolean("ipv6_has_auto_conf",
                                                          _("ipv6_has_auto_conf"),
                                                          _("ipv6_has_auto_conf"),
                                                          FALSE,
                                                          G_PARAM_READWRITE));


    g_object_class_install_property (gobject_class,
                                     PROP_IPV6_ADDRESS,
                                     g_param_spec_string ("ipv6_address",
                                                          _("ipv6_address"),
                                                          _("ipv6_address"),
                                                          "",
                                                          G_PARAM_READABLE));

    g_object_class_install_property (gobject_class,
                                     PROP_IPV6_PREFIX,
                                     g_param_spec_string ("ipv6_prefix",
                                                          _("ipv6_prefix"),
                                                          _("ipv6_prefix"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_IPV6_DEFAULT_ROUTE,
                                     g_param_spec_string ("ipv6_default_route",
                                                          _("ipv6_default_route"),
                                                          _("ipv6_default_route"),
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
nwamui_ncu_init(NwamuiNcu *self)
{
    NwamuiNcuPrivate *prv      = NWAMUI_NCU_GET_PRIVATE(self);
    self->prv = prv;

    prv->initialisation = TRUE;

    prv->ncu_type = NWAMUI_NCU_TYPE_WIRED;
    prv->v4addresses = gtk_list_store_new ( 1, NWAMUI_TYPE_IP);

    prv->ipv6_has_dhcp = TRUE; /* Always assume present, since it is in Phase 1 */
    prv->ipv6_has_auto_conf = TRUE; /* Always assume present, since it is in Phase 1 */
    prv->v6addresses = gtk_list_store_new ( 1, NWAMUI_TYPE_IP);

    prv->link_state = NWAM_STATE_UNINITIALIZED;
    prv->link_aux_state = NWAM_AUX_STATE_UNINITIALIZED;

    prv->state = NWAMUI_STATE_UNKNOWN;

    /* Create WifiNet cache */
    prv->wifi_hash_table = g_hash_table_new_full(  g_str_hash, g_str_equal,
      (GDestroyNotify)g_free,
      (GDestroyNotify)g_object_unref);
    

/*     g_signal_connect(G_OBJECT(self), "notify", (GCallback)object_notify_cb, (gpointer)self); */
    g_signal_connect(G_OBJECT(prv->v4addresses), "row-deleted", (GCallback)ip_row_deleted_cb, (gpointer)self);
    g_signal_connect(G_OBJECT(prv->v4addresses), "row-changed", (GCallback)ip_row_inserted_or_changed_cb, (gpointer)self);
    g_signal_connect(G_OBJECT(prv->v4addresses), "row-inserted", (GCallback)ip_row_inserted_or_changed_cb, (gpointer)self);
    g_signal_connect(G_OBJECT(prv->v6addresses), "row-deleted", (GCallback)ip_row_deleted_cb, (gpointer)self);
    g_signal_connect(G_OBJECT(prv->v6addresses), "row-changed", (GCallback)ip_row_inserted_or_changed_cb, (gpointer)self);
    g_signal_connect(G_OBJECT(prv->v6addresses), "row-inserted", (GCallback)ip_row_inserted_or_changed_cb, (gpointer)self);
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
        read_only = !nwamui_object_real_is_modifiable(NWAMUI_OBJECT(self));

        if ( read_only && prop_id != PROP_WIFI_INFO ) {
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
                set_modified_flag( self, NWAM_NCU_CLASS_PHYS, TRUE );
            }
            break;
        case PROP_NCU_TYPE: {
                self->prv->ncu_type = g_value_get_int( value );
                /* Need update display name if type changes */
                nwamui_ncu_set_display_name(self);
            }
            break;
        case PROP_MTU: {
                guint64 mtu = g_value_get_uint( value );
                set_nwam_ncu_uint64_prop( self->prv->nwam_ncu_phys, NWAM_NCU_PROP_LINK_MTU, mtu );
                set_modified_flag( self, NWAM_NCU_CLASS_PHYS, TRUE );
            }
            break;
        case PROP_IPV4_ACTIVE: {
                gboolean active = g_value_get_boolean( value );
                if ( active != self->prv->ipv4_active ) {
                    self->prv->ipv4_active = active;
                    set_modified_flag( self, NWAM_NCU_CLASS_IP, TRUE );
                }
            }
            break;
        case PROP_IPV4_HAS_DHCP: {
                gboolean dhcp = g_value_get_boolean( value );

                if ( dhcp != self->prv->ipv4_has_dhcp ) {
                    if ( dhcp ) {
                        if ( self->prv->ipv4_dhcp_ip == NULL ) {
                            NwamuiIp*   ip = nwamui_ip_new( self, "", "", FALSE,
                                                            TRUE, FALSE, FALSE );
                            self->prv->ipv4_dhcp_ip = ip;
                        }
                    }
                    self->prv->ipv4_has_dhcp = dhcp;
                    set_modified_flag( self, NWAM_NCU_CLASS_IP, TRUE );
                }
            }
            break;
        case PROP_IPV4_DEFAULT_ROUTE: {
                const gchar* default_route = g_strdup( g_value_get_string( value ) );

                set_nwam_ncu_string_prop( self->prv->nwam_ncu_ip, NWAM_NCU_PROP_IPV4_DEFAULT_ROUTE, default_route );

                set_modified_flag( self, NWAM_NCU_CLASS_IP, TRUE );
            }
            break;
        case PROP_IPV6_ACTIVE: {
                gboolean active = g_value_get_boolean( value );
                if ( active != self->prv->ipv6_active ) {
                    self->prv->ipv6_active = active;
                    set_modified_flag( self, NWAM_NCU_CLASS_IP, TRUE );
                }
          }
            break;
        case PROP_IPV6_HAS_DHCP: {
                gboolean dhcp = g_value_get_boolean( value );

                self->prv->ipv6_has_dhcp = dhcp;
                set_modified_flag( self, NWAM_NCU_CLASS_IP, TRUE );
            }
            break;
        case PROP_IPV6_HAS_AUTO_CONF: {
                gboolean auto_conf = g_value_get_boolean( value );

                self->prv->ipv6_has_auto_conf = auto_conf;
                set_modified_flag( self, NWAM_NCU_CLASS_IP, TRUE );
            }
            break;
        case PROP_IPV6_DEFAULT_ROUTE: {
                const gchar* default_route = g_strdup( g_value_get_string( value ) );
                
                set_nwam_ncu_string_prop( self->prv->nwam_ncu_ip, NWAM_NCU_PROP_IPV6_DEFAULT_ROUTE, default_route );
                
                set_modified_flag( self, NWAM_NCU_CLASS_IP, TRUE );
            }
            break;
        case PROP_V4ADDRESSES: {
                self->prv->v4addresses = GTK_LIST_STORE(g_value_dup_object( value ));
                set_modified_flag( self, NWAM_NCU_CLASS_IP, TRUE );
            }
            break;

        case PROP_V6ADDRESSES: {
                self->prv->v6addresses = GTK_LIST_STORE(g_value_dup_object( value ));
                set_modified_flag( self, NWAM_NCU_CLASS_IP, TRUE );
            }
            break;
        case PROP_WIFI_INFO:
            nwamui_ncu_set_wifi_info(self, NWAMUI_WIFI_NET(g_value_get_object(value)));
            break;

        case PROP_PRIORITY_GROUP: {
                guint64 priority_group = g_value_get_uint( value );

                set_nwam_ncu_uint64_prop( self->prv->nwam_ncu_phys, NWAM_NCU_PROP_PRIORITY_GROUP, priority_group );
                set_modified_flag( self, NWAM_NCU_CLASS_PHYS, TRUE );
            }
            break;

        case PROP_PRIORITY_GROUP_MODE: {
                guint64 priority_mode = g_value_get_int( value );
                set_nwam_ncu_uint64_prop( self->prv->nwam_ncu_phys, NWAM_NCU_PROP_PRIORITY_MODE, priority_mode );
                set_modified_flag( self, NWAM_NCU_CLASS_PHYS, TRUE );
            }
            break;

        case PROP_AUTO_PUSH: {
                GList*  autopush = g_value_get_pointer( value );
                gchar** autopush_strs = nwamui_util_glist_to_strv( autopush );
                set_nwam_ncu_string_array_prop( self->prv->nwam_ncu_phys, NWAM_NCU_PROP_LINK_AUTOPUSH, autopush_strs, 0 );
                g_strfreev(autopush_strs);
                set_modified_flag( self, NWAM_NCU_CLASS_PHYS, TRUE );
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
        case PROP_IPV4_ACTIVE: {
                g_value_set_boolean(value, self->prv->ipv4_active);
            }
            break;
        case PROP_IPV4_HAS_DHCP: {
                g_value_set_boolean(value, self->prv->ipv4_has_dhcp );
            }
            break;
        case PROP_IPV4_ADDRESS: {
                gchar *address = NULL;
                if ( self->prv->ipv4_dhcp_ip != NULL ) {
                    address = nwamui_ip_get_address(self->prv->ipv4_dhcp_ip);
                }
                else if ( self->prv->ipv4_primary_static_ip != NULL ) {
                    address = nwamui_ip_get_address(self->prv->ipv4_primary_static_ip);
                }
                g_value_set_string(value, address);
                if ( address != NULL ) {
                    g_free(address);
                }
            }
            break;
        case PROP_IPV4_SUBNET: {
                gchar *subnet = NULL;
                if ( self->prv->ipv4_dhcp_ip != NULL ) {
                    subnet = nwamui_ip_get_subnet_prefix(self->prv->ipv4_dhcp_ip);
                }
                else if ( self->prv->ipv4_primary_static_ip != NULL ) {
                    subnet = nwamui_ip_get_subnet_prefix(self->prv->ipv4_primary_static_ip);
                }
                g_value_set_string(value, subnet);
                if ( subnet != NULL ) {
                    g_free(subnet);
                }
            }
            break;
        case PROP_IPV4_DEFAULT_ROUTE: {
                gchar *default_route = NULL;

                default_route = get_nwam_ncu_string_prop( self->prv->nwam_ncu_ip, NWAM_NCU_PROP_IPV4_DEFAULT_ROUTE);

                g_value_set_string( value, default_route );
            }
            break;
        case PROP_IPV6_ACTIVE: {
                g_value_set_boolean(value, self->prv->ipv6_active);
            }
            break;
        case PROP_IPV6_HAS_DHCP: {
                g_value_set_boolean(value, self->prv->ipv6_has_dhcp );
            }
            break;
        case PROP_IPV6_HAS_AUTO_CONF: {
                g_value_set_boolean(value, self->prv->ipv6_has_auto_conf );
            }
            break;
        case PROP_IPV6_ADDRESS: {
                gchar *address = NULL;

                if ( self->prv->ipv6_primary_static_ip != NULL ) {
                    address = nwamui_ip_get_address(self->prv->ipv6_primary_static_ip);
                }
                g_value_set_string(value, address);
                if ( address != NULL ) {
                    g_free(address);
                }
            }
            break;
        case PROP_IPV6_PREFIX: {
                gchar *prefix = NULL;
                if ( self->prv->ipv6_primary_static_ip != NULL ) {
                    prefix = nwamui_ip_get_subnet_prefix(self->prv->ipv6_primary_static_ip);
                }
                g_value_set_string(value, prefix);
                if ( prefix != NULL ) {
                    g_free(prefix);
                }
            }
            break;
        case PROP_IPV6_DEFAULT_ROUTE: {
                gchar *default_route = NULL;

                default_route = get_nwam_ncu_string_prop( self->prv->nwam_ncu_ip, NWAM_NCU_PROP_IPV6_DEFAULT_ROUTE);

                g_value_set_string( value, default_route );
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
                gchar** autopush = get_nwam_ncu_string_array_prop( self->prv->nwam_ncu_phys, NWAM_NCU_PROP_LINK_AUTOPUSH, NULL );
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
#ifdef TUNNEL_SUPPORT
        if (strncmp(device, "ip.tun", 6) == 0 ||
            strncmp(device, "ip6.tun", 7) == 0 ||
            strncmp(device, "ip.6to4tun", 10) == 0)
            /*
             * TODO - Tunnel support
             *
             * We'll need to update our tunnel detection once
             * the clearview/tun project is integrated; tunnel
             * names won't necessarily be ip.tunN.
             */
            type = NWAMUI_NCU_TYPE_TUNNEL;
#endif /* TUNNEL_SUPPORT */
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

    nwamui_object_set_name(NWAMUI_OBJECT(ncu), name);

    g_object_set( ncu,
      "device_name", name,
      "ncu_type", ncu_type,
      NULL);

    free(name);
}

#ifdef TUNNEL_SUPPORT
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
#endif /* TUNNEL_SUPPORT */

/* 
 * Note about addrsrc vs addresses:
 *
 * At this moment in time, in Phase 1, there isn't a 1-1 mapping between
 * addresssrc and address. 
 *
 * The contents of addresssrc is used to determine various sources, but the
 * address list isn't looked at unless there is a single address source that
 * is Static.
 */
static void
populate_ip_ncu_data( NwamuiNcu *ncu, nwam_ncu_handle_t nwam_ncu )
{
    guint64            *ip_version = NULL;
    guint               ip_version_num = NULL;
    guint64            *ipv4_addrsrc = NULL;
    guint               ipv4_addrsrc_num = 0;
    gchar**             ipv4_addr = NULL;
    guint               ipv4_addr_num = 0;
    guint               ipv6_addr_num = 0;
    guint64            *ipv6_addrsrc =  NULL;
    guint               ipv6_addrsrc_num = 0;
    gchar**             ipv6_addr = NULL;
    
    ip_version = get_nwam_ncu_uint64_array_prop( nwam_ncu, 
                                                 NWAM_NCU_PROP_IP_VERSION, 
                                                 &ip_version_num );

    g_object_freeze_notify(G_OBJECT(ncu->prv->v4addresses));
    g_object_freeze_notify(G_OBJECT(ncu->prv->v6addresses));

    ncu->prv->ipv4_active = FALSE;
    if ( ncu->prv->ipv4_primary_static_ip != NULL ) {
        g_object_unref(G_OBJECT( ncu->prv->ipv4_primary_static_ip ));
    }
    ncu->prv->ipv4_primary_static_ip = NULL;
    gtk_list_store_clear(ncu->prv->v4addresses);

    ncu->prv->ipv6_active = FALSE;
    if ( ncu->prv->ipv6_primary_static_ip != NULL ) {
        g_object_unref(G_OBJECT( ncu->prv->ipv6_primary_static_ip ));
    }
    ncu->prv->ipv6_primary_static_ip = NULL;
    gtk_list_store_clear(ncu->prv->v6addresses);

    for ( int ip_n = 0; ip_n < ip_version_num; ip_n++ ) {
        if (ip_version[ip_n] == IPV4_VERSION) {
            char**      ptr;
            gboolean    has_static = FALSE;
            gint        i;

            ipv4_addrsrc = get_nwam_ncu_uint64_array_prop( nwam_ncu, 
                                                           NWAM_NCU_PROP_IPV4_ADDRSRC, 
                                                           &ipv4_addrsrc_num );

            ipv4_addr = get_nwam_ncu_string_array_prop(nwam_ncu, NWAM_NCU_PROP_IPV4_ADDR, &ipv4_addr_num );

            /* Populate the v4addresses member */
            g_debug( "ipv4_addrsrc_num = %d, ipv4_addr_num = %d", ipv4_addrsrc_num, ipv4_addr_num );
            ptr = ipv4_addr;

            for( i = 0; i < ipv4_addrsrc_num; i++ ) {
                /* Handle IPv4 DHCP as spearate entity.
                 */
                if ( ipv4_addrsrc[i] == NWAM_ADDRSRC_DHCP ) {
                    ncu->prv->ipv4_has_dhcp = TRUE;
                    ncu->prv->ipv4_dhcp_ip = nwamui_ip_new( ncu, "", "", FALSE, 
                                                TRUE,    /* DHCP */
                                                FALSE,   /* Autoconf */
                                                FALSE ); /* Static */
                    continue; /* Do no more here */
                }

                has_static = TRUE;
            }
            g_free(ipv4_addrsrc);

            if ( has_static ) {
                for( i = 0; i < ipv4_addr_num; i++ ) {
                    gchar*  address = NULL;
                    gchar*  subnet = NULL;

                    if ( ptr != NULL && *ptr != NULL ) {
                         nwamui_util_split_address_prefix( FALSE, *ptr, &address, &subnet );
                    }

                    NwamuiIp*   ip = nwamui_ip_new( ncu, 
                                                    (address?address:""),
                                                    (subnet?subnet:""),
                                                    FALSE, 
                                                    FALSE,  /* DHCP */
                                                    FALSE,  /* Autoconf */
                                                    TRUE ); /* Static */

                    if ( subnet != NULL ) {
                        g_free(subnet);
                    }

                    GtkTreeIter iter;

                    g_signal_handlers_block_by_func(G_OBJECT(ncu->prv->v4addresses), (gpointer)ip_row_inserted_or_changed_cb, (gpointer)ncu);
                    gtk_list_store_insert(ncu->prv->v4addresses, &iter, 0 );
                    gtk_list_store_set(ncu->prv->v4addresses, &iter, 0, ip, -1 );
                    g_signal_handlers_unblock_by_func(G_OBJECT(ncu->prv->v4addresses), (gpointer)ip_row_inserted_or_changed_cb, (gpointer)ncu);

                    if ( ncu->prv->ipv4_primary_static_ip == NULL ) {
                        ncu->prv->ipv4_primary_static_ip = NWAMUI_IP(g_object_ref(ip));
                    }
                    if ( ptr != NULL && *ptr != NULL ) {
                        /* Free up memory as we go */
                        g_free(*ptr);
                    }
                    if ( ptr != NULL ) {
                        ptr++;
                    }
                }
            }
            if ( ipv4_addr != NULL ) {
                g_free(ipv4_addr);
            }

            if ( ipv4_addrsrc_num > 0 ) {
                ncu->prv->ipv4_active = TRUE;
            }
        }
        else if (ip_version[ip_n] == IPV6_VERSION) {
            char          ** ptr;
            const char     *prefix = NULL;
            char           *delim;
            gboolean        has_static = FALSE;
            gint            i;

            ipv6_addrsrc = get_nwam_ncu_uint64_array_prop(  nwam_ncu, 
                                                            NWAM_NCU_PROP_IPV6_ADDRSRC, 
                                                            &ipv6_addrsrc_num );
            
            ipv6_addr = get_nwam_ncu_string_array_prop(nwam_ncu, NWAM_NCU_PROP_IPV6_ADDR, &ipv6_addr_num );

            /* Populate the v6addresses member */
            g_debug( "ipv6_addrsrc_num = %d, ipv6_addr_num = %d", ipv6_addrsrc_num, ipv6_addr_num );
            ptr = ipv6_addr;

            for( i = 0; i < ipv6_addrsrc_num; i++ ) {
                gchar*  address = NULL;
                gchar*  prefix = NULL;

                /* Handle IPv6 DHCP and Autoconf differently.
                 *
                 * At the moment, it is always on, but it may be turned off,
                 * and depends on defect 10461 being fixed in nwamd.
                 *
                 * The GUI doesn't then have any way to enable/disable
                 * autoconf, so we just remember if we saw one, if so, we will
                 * append it on writing out addresses, but for simplicity we
                 * will not show it anywhere in the GUI other than the 
                 * config summary string.
                 */
                if ( ipv6_addrsrc[i] == NWAM_ADDRSRC_DHCP ) {
                    ncu->prv->ipv6_has_dhcp = TRUE;
                    continue; /* Do no more here */
                }

                if ( ipv6_addrsrc[i] == NWAM_ADDRSRC_AUTOCONF ) {
                    ncu->prv->ipv6_has_auto_conf = TRUE;
                    continue; /* Do no more here */
                }

                has_static = TRUE;
            }
            g_free(ipv6_addrsrc);

            for( i = 0; i < ipv6_addr_num; i++ ) {
                gchar*  address = NULL;
                gchar*  prefix = NULL;


                if ( ptr != NULL && *ptr != NULL ) {
                     nwamui_util_split_address_prefix( TRUE, *ptr, &address, &prefix );
                }

                NwamuiIp*   ip = nwamui_ip_new( ncu, 
                                                (address?address:""),
                                                (prefix?prefix:""), 
                                                TRUE, 
                                                FALSE,  /* DHCP */
                                                FALSE,  /* Autoconf */
                                                TRUE ); /* Static */

                GtkTreeIter iter;

                g_signal_handlers_block_by_func(G_OBJECT(ncu->prv->v6addresses), (gpointer)ip_row_inserted_or_changed_cb, (gpointer)ncu);
                gtk_list_store_insert(ncu->prv->v6addresses, &iter, 0 );
                gtk_list_store_set(ncu->prv->v6addresses, &iter, 0, ip, -1 );
                g_signal_handlers_unblock_by_func(G_OBJECT(ncu->prv->v6addresses), (gpointer)ip_row_inserted_or_changed_cb, (gpointer)ncu);

                if ( ncu->prv->ipv6_primary_static_ip == NULL ) {
                    ncu->prv->ipv6_primary_static_ip = NWAMUI_IP(g_object_ref(ip));
                }
                if ( ptr != NULL && *ptr != NULL ) {
                    /* Free up memory as we go */
                    g_free(*ptr);
                }
                if ( ptr != NULL ) {
                    ptr++;
                }
            }

            if ( ipv6_addrsrc_num > 0 ) {
                ncu->prv->ipv6_active = TRUE;
            }

            if ( ipv6_addr != NULL ) {
                g_free(ipv6_addr);
            }
        }
    }
    g_free(ip_version);

    g_object_thaw_notify(G_OBJECT(ncu->prv->v4addresses));
    g_object_thaw_notify(G_OBJECT(ncu->prv->v6addresses));

}

static void
populate_phys_ncu_data(NwamuiNcu *ncu, nwam_ncu_handle_t nwam_ncu)
{
    NwamuiNcuPrivate *prv = NWAMUI_NCU_GET_PRIVATE(ncu);
    gboolean          enabled;

    enabled = get_nwam_ncu_boolean_prop(nwam_ncu, NWAM_NCU_PROP_ENABLED);
    if ( enabled != prv->enabled ) {
        prv->enabled = enabled;
        g_object_notify(G_OBJECT(ncu), "enabled" );
    }
}

static gboolean 
append_to_glist( GtkTreeModel *model,
                 GtkTreePath *path,
                 GtkTreeIter *iter,
                 gpointer data )
{
    GList     **list_p = (GList**)data;
    GList      *list = *list_p;
	NwamuiIp   *ip;

    gtk_tree_model_get( GTK_TREE_MODEL(model), iter, 0, &ip, -1);
    if ( ip ) {
        list = g_list_append( list, (gpointer)g_object_ref(G_OBJECT(ip)) );
    }

    *list_p = list;

    return(FALSE);
}


static GList*
convert_gtk_list_store_to_g_list( GtkListStore *ls ) 
{
    GList*  new_list = NULL;

    if ( ls != NULL ) {
        gtk_tree_model_foreach( GTK_TREE_MODEL(ls), append_to_glist, &new_list );
    }

    return( new_list );
}
    
static gboolean 
set_address_source( GtkTreeModel *model,
                    GtkTreePath *path,
                    GtkTreeIter *iter,
                    gpointer data )
{
    NwamuiIp   *ip = NULL;
	gboolean    is_dhcp = (gboolean)data;

    gtk_tree_model_get( GTK_TREE_MODEL(model), iter, 0, &ip, -1);
    if ( ip && NWAMUI_IS_IP(ip) ) {
        nwamui_ip_set_dhcp( ip, is_dhcp );
    }

    return(FALSE);
}


#define MAX_ADDRSRC_NUM (NWAM_ADDRSRC_STATIC + 1)     

static void
nwamui_ncu_sync_handle_with_ip_data( NwamuiNcu *self )
{
    uint64_t            ip_version[2] = {0};
    guint               ip_version_num = 0;

    if ( self->prv->ipv4_active ) {
        uint64_t            ipv4_addrsrc[MAX_ADDRSRC_NUM];
        guint               ipv4_addrsrc_num = 0;
        guint               ipv4_addr_num = 0;
        gchar             **ipv4_addr = NULL;
        GList              *ipv4_list;
        guint               addr_index;
        gboolean            dhcp = self->prv->ipv4_has_dhcp;

        ip_version[ip_version_num] = (uint64_t)IPV4_VERSION;
        ip_version_num++;

        ipv4_list = convert_gtk_list_store_to_g_list( self->prv->v4addresses );
        ipv4_addr_num = g_list_length( ipv4_list );

        ipv4_addrsrc_num = 0;
        if ( dhcp ) {
            ipv4_addrsrc[ipv4_addrsrc_num] = (uint64_t)NWAM_ADDRSRC_DHCP;
            ipv4_addrsrc_num++; 
        }
        if ( ipv4_addr_num > 0 ) { /* Has static */
            ipv4_addrsrc[ipv4_addrsrc_num] = (uint64_t)NWAM_ADDRSRC_STATIC;
            ipv4_addrsrc_num++;
        }

        ipv4_addr = (gchar**)g_malloc((ipv4_addr_num + 1) * sizeof(gchar**)); /* +1 for NULL */

        /* How handle all static addresses */
        addr_index = 0;
        for ( GList* elem = g_list_first(ipv4_list);
              elem != NULL;
              elem = g_list_next(elem) ) {
            NwamuiIp*   ip = NWAMUI_IP(elem->data);

            gchar*  addr = nwamui_ip_get_address(ip);
            gchar*  subnet = nwamui_ip_get_subnet_prefix(ip); 

            ipv4_addr[addr_index] = nwamui_util_join_address_prefix( FALSE, addr, subnet );
            addr_index++;
        }
        ipv4_addr[addr_index] = NULL;

        if ( addr_index > 0 ) {
            set_nwam_ncu_string_array_prop(self->prv->nwam_ncu_ip,
                                           NWAM_NCU_PROP_IPV4_ADDR, ipv4_addr, 0 );
        }
        else {
            delete_nwam_ncu_prop(self->prv->nwam_ncu_ip, NWAM_NCU_PROP_IPV4_ADDR);
        }
        if ( ipv4_addrsrc_num > 0 ) {
            set_nwam_ncu_uint64_array_prop( self->prv->nwam_ncu_ip,
                                            NWAM_NCU_PROP_IPV4_ADDRSRC, 
                                            ipv4_addrsrc,
                                            ipv4_addrsrc_num );
        }
        nwamui_util_free_obj_list( ipv4_list );
        if ( ipv4_addr != NULL ) {
            g_strfreev(ipv4_addr);
        }
    }
    else {
        /* Delete properties for IPV4 */
        delete_nwam_ncu_prop(self->prv->nwam_ncu_ip, NWAM_NCU_PROP_IPV4_ADDR);
        delete_nwam_ncu_prop( self->prv->nwam_ncu_ip, NWAM_NCU_PROP_IPV4_ADDRSRC );
    }

    if ( self->prv->ipv6_active ) {
        uint64_t            ipv6_addrsrc[MAX_ADDRSRC_NUM];
        guint               ipv6_addrsrc_num = 0;
        guint               ipv6_addr_num = 0;
        gchar             **ipv6_addr = NULL;
        GList              *ipv6_list;
        guint               addr_index;
        gboolean            autoconf = self->prv->ipv6_has_auto_conf;
        gboolean            dhcp = self->prv->ipv6_has_dhcp;

        ip_version[ip_version_num] = (uint64_t)IPV6_VERSION;
        ip_version_num++;

        ipv6_list = convert_gtk_list_store_to_g_list( self->prv->v6addresses );
        ipv6_addr_num = g_list_length( ipv6_list );

        ipv6_addrsrc_num = 0;
        if ( dhcp ) {
            ipv6_addrsrc[ipv6_addrsrc_num] = (uint64_t)NWAM_ADDRSRC_DHCP;
            ipv6_addrsrc_num++; 
        }
        if ( autoconf ) {
            ipv6_addrsrc[ipv6_addrsrc_num] = (uint64_t)NWAM_ADDRSRC_AUTOCONF;
            ipv6_addrsrc_num++; 
        }
        if ( ipv6_addr_num > 0 ) { /* Has static */
            ipv6_addrsrc[ipv6_addrsrc_num] = (uint64_t)NWAM_ADDRSRC_STATIC;
            ipv6_addrsrc_num++;
        }

        ipv6_addr = (gchar**)g_malloc((ipv6_addr_num + 1 ) * sizeof(gchar**)); /* +1 for NULL */

        addr_index = 0;
        for ( GList* elem = g_list_first(ipv6_list);
              elem != NULL;
              elem = g_list_next(elem) ) {
            NwamuiIp*   ip = NWAMUI_IP(elem->data);

            gchar*  addr = nwamui_ip_get_address(ip);
            gchar*  subnet = nwamui_ip_get_subnet_prefix(ip); 

            ipv6_addr[addr_index] = nwamui_util_join_address_prefix( TRUE, addr, subnet );
            addr_index++;
        }

        ipv6_addr[addr_index] = NULL;

        if ( addr_index > 0 ) {
            set_nwam_ncu_string_array_prop(self->prv->nwam_ncu_ip,
                                           NWAM_NCU_PROP_IPV6_ADDR, ipv6_addr, 0 );
        }
        else {
            delete_nwam_ncu_prop(self->prv->nwam_ncu_ip, NWAM_NCU_PROP_IPV6_ADDR);
        }
        if ( ipv6_addrsrc_num > 0 ) {
            set_nwam_ncu_uint64_array_prop( self->prv->nwam_ncu_ip,
                                            NWAM_NCU_PROP_IPV6_ADDRSRC, 
                                            ipv6_addrsrc,
                                            ipv6_addrsrc_num );
        }
        nwamui_util_free_obj_list( ipv6_list );
        if ( ipv6_addr != NULL ) {
            g_strfreev(ipv6_addr);
        }
    }
    else {
        /* Delete properties for IPV6 */
        delete_nwam_ncu_prop(self->prv->nwam_ncu_ip, NWAM_NCU_PROP_IPV6_ADDR);
        delete_nwam_ncu_prop( self->prv->nwam_ncu_ip, NWAM_NCU_PROP_IPV6_ADDRSRC );
    }

    if ( ip_version_num > 0 ) {
        set_nwam_ncu_uint64_array_prop(  self->prv->nwam_ncu_ip,
                                         NWAM_NCU_PROP_IP_VERSION, 
                                         ip_version,
                                         ip_version_num );
    }
    else {
        /* Delete IP_VERSION property, since we shouldn't store an empty list.
         */
        delete_nwam_ncu_prop( self->prv->nwam_ncu_ip, NWAM_NCU_PROP_IP_VERSION);
    }
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
    NwamuiNcuPrivate *prv      = NWAMUI_NCU_GET_PRIVATE(self);
    nwam_ncu_handle_t ncu_handle = NULL;

    g_return_val_if_fail(NWAMUI_IS_NCU(self), ncu_handle);

    if (prv->ncp != NULL && prv->device_name != NULL) {

        nwam_ncp_handle_t ncp_handle;
        nwam_error_t      nerr;

        ncp_handle = nwamui_ncp_get_nwam_handle(prv->ncp);
        nerr = nwam_ncu_read(ncp_handle, prv->device_name, ncu_type, 0, &ncu_handle);

        if (nerr != NWAM_SUCCESS) {
            if (nerr == NWAM_ENTITY_NOT_FOUND) {
                g_debug("Failed to read ncu information for %s error: %s", prv->device_name, nwam_strerror(nerr));
            } else {
                g_warning("Failed to read ncu information for %s error: %s", prv->device_name, nwam_strerror(nerr));
            }
            return ( NULL );
        }
    }
    return ncu_handle;
}

extern  NwamuiObject*
nwamui_ncu_new_with_handle( NwamuiNcp* ncp, nwam_ncu_handle_t ncu )
{
    NwamuiNcu*          self = NULL;
    nwam_ncu_class_t    ncu_class;
    
    self = NWAMUI_NCU(g_object_new (NWAMUI_TYPE_NCU,
                                    "ncp", ncp,
                                    NULL));

    nwamui_object_real_set_handle(NWAMUI_OBJECT(self), ncu);

    self->prv->initialisation = FALSE;

    return NWAMUI_OBJECT( self );
}

/**
 * nwamui_ncu_clone:
 * @returns: a new #NwamuiNcu.
 *
 * Creates a new #NwamuiNcu with info initialised based on the argumens passed.
 **/
static NwamuiObject*
nwamui_object_real_clone(NwamuiObject *object, const gchar *name, NwamuiObject *parent)
{
    NwamuiNcu         *ncu = NWAMUI_NCU(object);
    NwamuiNcp         *ncp = NWAMUI_NCP(parent);
    NwamuiNcu         *new = NULL;
    nwam_ncp_handle_t  nwam_ncp;
    nwam_ncu_handle_t  nwam_ncu_phys;
    nwam_ncu_handle_t  nwam_ncu_ip;
    nwam_ncu_handle_t  nwam_ncu_iptun;
    nwam_error_t       nerr;

    g_assert(NWAMUI_IS_NCU(object));
    g_assert(NWAMUI_IS_NCP(parent));
    g_assert(name == NULL);

    new = NWAMUI_NCU(g_object_new (NWAMUI_TYPE_NCU,
                                    "ncp", ncp,
                                    NULL));

    nwam_ncp = nwamui_ncp_get_nwam_handle( ncp );

    if ( ncu->prv->nwam_ncu_phys != NULL ) {
        nwam_ncu_type_t     ncu_type;
        nwam_ncu_class_t    ncu_class;
        
        ncu_type = (nwam_ncu_type_t)get_nwam_ncu_uint64_prop(ncu->prv->nwam_ncu_phys, NWAM_NCU_PROP_TYPE);
        ncu_class = (nwam_ncu_class_t)get_nwam_ncu_uint64_prop(ncu->prv->nwam_ncu_phys, NWAM_NCU_PROP_CLASS);

        if ( (nerr = nwam_ncu_read( nwam_ncp, ncu->prv->device_name, ncu_type, 0,
                                    &nwam_ncu_phys ) ) != NWAM_SUCCESS  ) {
            nerr = nwam_ncu_create(nwam_ncp, ncu->prv->device_name, ncu_type, ncu_class, &nwam_ncu_phys);
            if ( nerr != NWAM_SUCCESS) {
                nwamui_warning("Create LINK ncu error: %s", nwam_strerror(nerr));
            }
		    if ( (nerr = nwam_ncu_commit(nwam_ncu_phys, 0 ) ) != NWAM_SUCCESS ) {
			    g_warning("Failed when committing PHYS NCU for %s error: %s", ncu->prv->device_name, nwam_strerror(nerr));
		    }
        }
        if ( nwam_ncu_phys != NULL ) {
            nwamui_object_real_set_handle(NWAMUI_OBJECT(new), nwam_ncu_phys);
            set_modified_flag( new, NWAM_NCU_CLASS_PHYS, TRUE );
        }
    }

    if ( ncu->prv->nwam_ncu_ip != NULL ) {
        nwam_ncu_type_t     ncu_type;
        nwam_ncu_class_t    ncu_class;
        
        ncu_type = (nwam_ncu_type_t)get_nwam_ncu_uint64_prop(ncu->prv->nwam_ncu_ip, NWAM_NCU_PROP_TYPE);
        ncu_class = (nwam_ncu_class_t)get_nwam_ncu_uint64_prop(ncu->prv->nwam_ncu_ip, NWAM_NCU_PROP_CLASS);

        if ( (nerr = nwam_ncu_read( nwam_ncp, ncu->prv->device_name, ncu_type, 0,
                                    &nwam_ncu_ip ) ) != NWAM_SUCCESS  ) {
            nerr = nwam_ncu_create(nwam_ncp, ncu->prv->device_name, ncu_type, ncu_class, &nwam_ncu_ip);
            if ( nerr != NWAM_SUCCESS) {
                nwamui_warning("Create LINK ncu error: %s", nwam_strerror(nerr));
            }
	 	   if ( (nerr = nwam_ncu_commit(nwam_ncu_ip, 0 ) ) != NWAM_SUCCESS ) {
			    g_warning("Failed when committing IP NCU for %s error: %s", ncu->prv->device_name, nwam_strerror(nerr));
	    	}
        }
        if ( nwam_ncu_ip != NULL ) {
            nwamui_object_real_set_handle(NWAMUI_OBJECT(new), nwam_ncu_ip);
            set_modified_flag( new, NWAM_NCU_CLASS_IP, TRUE );
        }
    }

#ifdef TUNNEL_SUPPORT
    if ( ncu->prv->nwam_ncu_iptun != NULL ) {
        nwam_ncu_type_t     ncu_type;
        nwam_ncu_class_t    ncu_class;
        
        ncu_type = (nwam_ncu_type_t)get_nwam_ncu_uint64_prop(ncu->prv->nwam_ncu_iptun, NWAM_NCU_PROP_TYPE);
        ncu_class = (nwam_ncu_class_t)get_nwam_ncu_uint64_prop(ncu->prv->nwam_ncu_iptun, NWAM_NCU_PROP_CLASS);

        if ( (nerr = nwam_ncu_read( nwam_ncp, ncu->prv->device_name, ncu_type, 0,
                                    &nwam_ncu_iptun ) ) != NWAM_SUCCESS  ) {
            nerr = nwam_ncu_create(nwam_ncp, ncu->prv->device_name, ncu_type, ncu_class, &nwam_ncu_iptun);
            if ( nerr != NWAM_SUCCESS) {
                nwamui_warning("Create LINK ncu error: %s", nwam_strerror(nerr));
            }
	    	if ( (nerr = nwam_ncu_commit(nwam_ncu_iptun, 0 ) ) != NWAM_SUCCESS ) {
			    g_warning("Failed when committing IPTUN NCU for %s error: %s", ncu->prv->device_name, nwam_strerror(nerr));
	    	}
        }
        if ( nwam_ncu_iptun != NULL ) {
            nwamui_object_real_set_handle(NWAMUI_OBJECT(new), nwam_ncu_iptun);
            set_modified_flag( new, NWAM_NCU_CLASS_IPTUN, TRUE );
        }
    }
#endif /* TUNNEL_SUPPORT */

    return NWAMUI_OBJECT(new);
}

/**
 * nwamui_ncu_is_modifiable:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the modifiable.
 *
 **/
static gboolean
nwamui_object_real_is_modifiable(NwamuiObject *object)
{
    NwamuiNcu    *self       = NWAMUI_NCU(object);
    nwam_error_t  nerr;
    gboolean      modifiable = FALSE; 
    boolean_t     readonly;

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
static void
nwamui_object_real_reload(NwamuiObject* object)
{
    NwamuiNcuPrivate  *prv  = NWAMUI_NCU_GET_PRIVATE(object);
    NwamuiNcu         *self = NWAMUI_NCU(object);
    nwam_ncu_handle_t  handle;

    g_return_if_fail( NWAMUI_IS_NCU(self) );

    /* nwamui_object_set_handle will cause re-read from configuration */
    g_object_freeze_notify(G_OBJECT(self));

    handle = get_nwam_ncu_handle(NWAMUI_NCU(object), NWAM_NCU_TYPE_LINK);
    if (handle) {
        if (prv->nwam_ncu_phys) {
            nwam_ncu_free(prv->nwam_ncu_phys);
        }
        prv->nwam_ncu_phys = handle;
    }
    if ( prv->nwam_ncu_phys != NULL ) {
        g_debug("Reverting NCU PHYS for %s", prv->device_name);
        populate_phys_ncu_data(self, prv->nwam_ncu_phys);
    }

    handle = get_nwam_ncu_handle(NWAMUI_NCU(object), NWAM_NCU_TYPE_INTERFACE);
    if (handle) {
        if (prv->nwam_ncu_ip) {
            nwam_ncu_free(prv->nwam_ncu_ip);
        }
        prv->nwam_ncu_ip = handle;
    }
    if ( prv->nwam_ncu_ip != NULL ) {
        g_debug("Reverting NCU IP for %s", prv->device_name);
        populate_ip_ncu_data( self, prv->nwam_ncu_ip );
    }

#ifdef TUNNEL_SUPPORT
    if ( prv->nwam_ncu_iptun != NULL ) {
        g_debug("Reverting NCU IPTUN for %s", prv->device_name);
        populate_iptun_ncu_data( self, prv->nwam_ncu_iptun );
    }
#endif /* TUNNEL_SUPPORT */

    /* Tell GUI to refresh */
    g_object_notify(G_OBJECT(self), "activation-mode");

    g_object_thaw_notify(G_OBJECT(self));
}

/**
 * nwamui_ncu_has_modifications:   test if there are un-saved changes
 * @returns: TRUE if unsaved changes exist.
 **/
static gboolean
nwamui_object_real_has_modifications(NwamuiObject* object)
{
    NwamuiNcuPrivate *prv = NWAMUI_NCU_GET_PRIVATE(object);

#ifdef TUNNEL_SUPPORT
    if ( NWAMUI_IS_NCU(object) &&
          ( prv->nwam_ncu_phys_modified 
         || prv->nwam_ncu_ip_modified 
         || prv->nwam_ncu_iptun_modified) ) {
#else
    if ( NWAMUI_IS_NCU(object) &&
          ( prv->nwam_ncu_phys_modified 
         || prv->nwam_ncu_ip_modified) ) {
#endif /* TUNNEL_SUPPORT */
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
        nwamui_ncu_sync_handle_with_ip_data( self );
        if ( (nerr = nwam_ncu_validate( self->prv->nwam_ncu_ip, &prop_name )) != NWAM_SUCCESS ) {
            g_debug("Failed when validating IP NCU for %s : invalid value for %s",
                    self->prv->device_name, prop_name);
            if ( prop_name_ret != NULL ) {
                *prop_name_ret = g_strdup( prop_name );
            }
            return( FALSE );
        }
    }

#ifdef TUNNEL_SUPPORT
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
#endif /* TUNNEL_SUPPORT */

    return( TRUE );
}

static gint
nwamui_object_real_sort(NwamuiObject *object, NwamuiObject *other, guint sort_by)
{
    NwamuiObject *objs[2] = { object, other };
    int rank[2];
    int i, retval = 0;

    g_return_val_if_fail(NWAMUI_IS_NCU(object), 0);
    g_return_val_if_fail(NWAMUI_IS_NCU(other), 0);

    switch (sort_by) {
    case NWAMUI_OBJECT_SORT_BY_GROUP:
        for (i = 0; i < 2; i++) {
            switch (nwamui_object_get_activation_mode(objs[i])) {
            case NWAMUI_COND_ACTIVATION_MODE_MANUAL:
                rank[i] = nwamui_object_get_enabled(objs[i]) ? ALWAYS_ON_GROUP_ID : ALWAYS_OFF_GROUP_ID;
                break;
            case NWAMUI_COND_ACTIVATION_MODE_PRIORITIZED:
                rank[i] = nwamui_ncu_get_priority_group(NWAMUI_NCU(objs[i])) + ALWAYS_ON_GROUP_ID + 1;
                break;
            default:
                g_warning("%s: Not supported activation mode %d", __func__, nwamui_object_get_activation_mode(objs[i]));
                rank[i] = ALWAYS_OFF_GROUP_ID;
                break;
            }
        }
        retval = rank[0] - rank[1];
        /* Fall through */
    case NWAMUI_OBJECT_SORT_BY_NAME:
        if (retval == 0) {
            retval = g_strcmp0(nwamui_ncu_get_display_name(NWAMUI_NCU(objs[0])),
              nwamui_ncu_get_display_name(NWAMUI_NCU(objs[1])));
        }
        break;
    default:
        g_warning("%s, not implemented", __func__);
    }

    g_debug("%s: %s - %s = %d", __func__,
      nwamui_ncu_get_display_name(NWAMUI_NCU(objs[0])), 
      nwamui_ncu_get_display_name(NWAMUI_NCU(objs[1])),
      retval);

    return retval;
}

/**
 * nwamui_ncu_commit:   commit in-memory configuration, to persistant storage
 * @returns: TRUE if succeeded, FALSE if failed
 **/
extern gboolean
nwamui_object_real_commit( NwamuiObject *object )
{
    NwamuiNcu *self = NWAMUI_NCU(object);
    nwam_error_t    nerr;
    gboolean currently_enabled;

    g_return_val_if_fail( NWAMUI_IS_NCU(self), FALSE );

    if ( self->prv->nwam_ncu_phys_modified && self->prv->nwam_ncu_phys != NULL ) {
        if ( (nerr = nwam_ncu_commit( self->prv->nwam_ncu_phys, 0 ) ) != NWAM_SUCCESS ) {
            g_warning("Failed when committing PHYS NCU for %s", self->prv->device_name);
            return( FALSE );
        }
	/* Set enabled flag. */
	currently_enabled = get_nwam_ncu_boolean_prop( self->prv->nwam_ncu_phys, NWAM_NCU_PROP_ENABLED );
	if ( self->prv->enabled != currently_enabled ) {
		nwam_error_t nerr;
		if ( self->prv->enabled ) {
			if ( (nerr = nwam_ncu_enable (self->prv->nwam_ncu_phys)) != NWAM_SUCCESS ) {
				g_warning("Failed to enable ncu_phys due to error: %s", nwam_strerror(nerr));
			}
		}
		else {
			if ( (nerr = nwam_ncu_disable (self->prv->nwam_ncu_phys)) != NWAM_SUCCESS ) {
				g_warning("Failed to disable ncu_phys due to error: %s", nwam_strerror(nerr));
			}
		}
	}
    }

    if ( self->prv->nwam_ncu_ip_modified && self->prv->nwam_ncu_ip != NULL ) {
        nwamui_ncu_sync_handle_with_ip_data( self );
        if ( (nerr = nwam_ncu_commit( self->prv->nwam_ncu_ip, 0 )) != NWAM_SUCCESS ) {
            g_warning("Failed when committing IP NCU for %s", self->prv->device_name);
            return( FALSE );
        }
	/* Set enabled flag. */
	currently_enabled = get_nwam_ncu_boolean_prop( self->prv->nwam_ncu_ip, NWAM_NCU_PROP_ENABLED );
    /* doo 12653, if user disable both ipv4/v6, disable IP NCU simplely. */
    self->prv->enabled = (self->prv->ipv4_active || self->prv->ipv6_active);

	if ( self->prv->enabled != currently_enabled ) {
		nwam_error_t nerr;
		if ( self->prv->enabled) {
			if ( (nerr = nwam_ncu_enable (self->prv->nwam_ncu_ip)) != NWAM_SUCCESS ) {
				g_warning("Failed to enable ncu_ip due to error: %s", nwam_strerror(nerr));
			}
		}
		else {
			if ( (nerr = nwam_ncu_disable (self->prv->nwam_ncu_ip)) != NWAM_SUCCESS ) {
				g_warning("Failed to disable ncu_ip due to error: %s", nwam_strerror(nerr));
			}
		}
	}
    }

#ifdef TUNNEL_SUPPORT
    if ( self->prv->nwam_ncu_iptun_modified && self->prv->nwam_ncu_iptun != NULL ) {
        if ( (nerr = nwam_ncu_commit( self->prv->nwam_ncu_iptun, 0 )) != NWAM_SUCCESS ) {
            g_warning("Failed when committing IPTUN NCU for %s", self->prv->device_name);
            return( FALSE );
        }
	/* Set enabled flag. */
	currently_enabled = get_nwam_ncu_boolean_prop( self->prv->nwam_ncu_iptun, NWAM_NCU_PROP_ENABLED );
	if ( self->prv->enabled != currently_enabled ) {
		nwam_error_t nerr;
		if ( self->prv->enabled ) {
			if ( (nerr = nwam_ncu_enable (self->prv->nwam_ncu_iptun)) != NWAM_SUCCESS ) {
				g_warning("Failed to enable ncu_iptun due to error: %s", nwam_strerror(nerr));
			}
		}
		else {
			if ( (nerr = nwam_ncu_disable (self->prv->nwam_ncu_iptun)) != NWAM_SUCCESS ) {
				g_warning("Failed to disable ncu_iptun due to error: %s", nwam_strerror(nerr));
			}
		}
	}
    }
#endif /* TUNNEL_SUPPORT */


    return( TRUE );
}

/**
 * nwamui_ncu_destroy:   commit in-memory configuration, to persistant storage
 * @returns: TRUE if succeeded, FALSE if failed
 **/
static gboolean
nwamui_object_real_destroy( NwamuiObject *object )
{
    NwamuiNcu *self = NWAMUI_NCU(object);
    nwam_error_t    nerr;

    g_return_val_if_fail( NWAMUI_IS_NCU(self), FALSE );

    if ( self->prv->nwam_ncu_ip != NULL ) {
        if ( (nerr = nwam_ncu_destroy( self->prv->nwam_ncu_ip, 0 )) != NWAM_SUCCESS ) {
            g_warning("Failed when destroying IP NCU for %s", self->prv->device_name);
            return( FALSE );
        }
    }

#ifdef TUNNEL_SUPPORT
    if ( self->prv->nwam_ncu_iptun != NULL ) {
        if ( (nerr = nwam_ncu_destroy( self->prv->nwam_ncu_iptun, 0 )) != NWAM_SUCCESS ) {
            g_warning("Failed when destroying IPTUN NCU for %s", self->prv->device_name);
            return( FALSE );
        }
    }
#endif /* TUNNEL_SUPPORT */
    if ( self->prv->nwam_ncu_phys != NULL ) {
        if ( (nerr = nwam_ncu_destroy( self->prv->nwam_ncu_phys, 0 ) ) != NWAM_SUCCESS ) {
            g_warning("Failed when destroying PHYS NCU for %s", self->prv->device_name);
            return( FALSE );
        }
    }

    return( TRUE );
}

/**
 * nwamui_object_real_set_handle:
 * This function will be called two time by NCP for link/interface object.
 */
static void
nwamui_object_real_set_handle(NwamuiObject *object, const gpointer handle)
{
    NwamuiNcuPrivate        *prv  = NWAMUI_NCU_GET_PRIVATE(object);
    NwamuiNcu               *self = NWAMUI_NCU(object);
    const nwam_ncu_handle_t  ncu  = handle;
    nwam_ncu_class_t         ncu_class;
    
    g_return_if_fail(NWAMUI_IS_NCU(object));

    ncu_class = (nwam_ncu_class_t)get_nwam_ncu_uint64_prop(ncu, NWAM_NCU_PROP_CLASS);

    g_object_freeze_notify( G_OBJECT(object) );
    prv->initialisation = TRUE;

    populate_common_ncu_data( self, ncu );

    switch ( ncu_class ) {
        case NWAM_NCU_CLASS_PHYS: {
                /* CLASS PHYS is of type LINK, so has LINK props */
                nwam_ncu_handle_t   ncu_handle;

                ncu_handle = get_nwam_ncu_handle( self, NWAM_NCU_TYPE_LINK );

                if ( ncu_handle != NULL ) {
                    if ( prv->nwam_ncu_phys != NULL ) {
                        nwam_ncu_free( prv->nwam_ncu_phys );
                    }

                    prv->nwam_ncu_phys = ncu_handle;
                    populate_phys_ncu_data(self, prv->nwam_ncu_phys);
                } else {
                    populate_phys_ncu_data(self, ncu);
                }
            }
            break;
#ifdef TUNNEL_SUPPORT
        case NWAM_NCU_CLASS_IPTUN: {
                nwam_ncu_handle_t   ncu_handle;

                ncu_handle = get_nwam_ncu_handle( self, NWAM_NCU_TYPE_INTERFACE );

                if ( ncu_handle != NULL ) {
                    if ( prv->nwam_ncu_iptun != NULL ) {
                        nwam_ncu_free( prv->nwam_ncu_iptun );
                    }

                    prv->nwam_ncu_iptun = ncu_handle;
                    populate_iptun_ncu_data(self, prv->nwam_ncu_iptun);
                } else {
                    populate_iptun_ncu_data(self, ncu);
                }
            }
            break;
#endif /* TUNNEL_SUPPORT */
        case NWAM_NCU_CLASS_IP: {
                nwam_ncu_handle_t   ncu_handle;

                ncu_handle = get_nwam_ncu_handle( self, NWAM_NCU_TYPE_INTERFACE );

                if ( ncu_handle != NULL ) {
                    if ( prv->nwam_ncu_ip != NULL ) {
                        nwam_ncu_free( prv->nwam_ncu_ip );
                    }

                    prv->nwam_ncu_ip = ncu_handle;
                    populate_ip_ncu_data(self, prv->nwam_ncu_ip);
                } else {
                    populate_ip_ncu_data(self, ncu);
                }
            }
            break;
        default:
            g_error("Unexpected ncu class %u", (guint)ncu_class);
    }

    g_object_thaw_notify( G_OBJECT(object) );

    prv->initialisation = FALSE;
}

/**
 * nwamui_ncu_get_vanity_name:
 * @returns: null-terminated C String with the vanity name of the the NCU.
 *
 **/
static const gchar*
nwamui_ncu_get_vanity_name ( NwamuiObject *object )
{
    NwamuiNcu *self = NWAMUI_NCU(object);
    g_return_val_if_fail (NWAMUI_IS_NCU(self), NULL); 
    
    return self->prv->vanity_name;
}

/**
 * nwamui_ncu_set_vanity_name:
 * @name: null-terminated C String with the vanity name of the the NCU.
 *
 **/
static gboolean
nwamui_ncu_set_vanity_name ( NwamuiObject *object, const gchar* name )
{
    NwamuiNcu *self = NWAMUI_NCU(object);
    g_return_val_if_fail(NWAMUI_IS_NCU(self), FALSE); 
    
    g_assert (name != NULL );

    if (self->prv->vanity_name) {
        g_free(self->prv->vanity_name);
    }
    self->prv->vanity_name = g_strdup(name);
    nwamui_ncu_set_display_name(self);
    return TRUE;
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
extern const gchar*               
nwamui_ncu_get_display_name ( NwamuiNcu *self )
{
    g_return_val_if_fail (NWAMUI_IS_NCU(self), NULL);
    
    return self->prv->display_name;
}

/**
 * nwamui_ncu_get_display_name:
 *
 * Use this to set a string of the format "<Vanity Name> (<device name>)"
 **/
static void
nwamui_ncu_set_display_name ( NwamuiNcu *self )
{
    g_return_if_fail (NWAMUI_IS_NCU(self)); 
    
    if (self->prv->display_name) {
        g_free(self->prv->display_name);
    }

    if ( self->prv->vanity_name != NULL ) {
        switch( self->prv->ncu_type ) {
        case NWAMUI_NCU_TYPE_WIRED:
            self->prv->display_name = g_strdup_printf( _("Wired (%s)"), self->prv->vanity_name );
            break;
        case NWAMUI_NCU_TYPE_WIRELESS:
            self->prv->display_name = g_strdup_printf( _("Wireless (%s)"), self->prv->vanity_name );
            break;
#ifdef TUNNEL_SUPPORT
        case NWAMUI_NCU_TYPE_TUNNEL:
            self->prv->display_name = g_strdup_printf( _("Tunnel (%s)"), self->prv->vanity_name );
            break;
#endif /* TUNNEL_SUPPORT */
        default:
            self->prv->display_name = g_strdup( self->prv->vanity_name );
            break;
        }
    }
}

/**
 * nwamui_ncu_get_nwam_qualified_name:
 * @returns: null-terminated C String with the nwam qualified name of the the NCU.
 *
 * Use this to get a string of the format "<ncu type>:<device name>", e.g.
 * 
 *      interface:e1000g0
 *
 **/
extern gchar*               
nwamui_ncu_get_nwam_qualified_name ( NwamuiNcu *self )
{
    gchar*  nwam_qualified_name = NULL;
    
    g_return_val_if_fail (NWAMUI_IS_NCU(self), nwam_qualified_name); 
    
    if ( self->prv->device_name != NULL ) {
        char    *typed_name = NULL;

        if ( nwam_ncu_name_to_typed_name( self->prv->device_name, 
                                          NWAM_NCU_TYPE_INTERFACE,
                                          &typed_name ) == NWAM_SUCCESS ) {
            if ( typed_name != NULL ) {
                nwam_qualified_name = g_strdup( typed_name );
                free(typed_name);
            }
        }
    }
    
    return( nwam_qualified_name );
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
nwamui_object_real_get_active ( NwamuiObject *object )
{
    NwamuiNcuPrivate              *prv            = NWAMUI_NCU_GET_PRIVATE(object);
    NwamuiNcu                     *self           = NWAMUI_NCU(object);
	gboolean                       active         = FALSE;
    nwam_state_t                   state          = NWAM_STATE_OFFLINE;
    nwam_aux_state_t               aux_state      = NWAM_AUX_STATE_UNINITIALIZED;
    nwam_state_t                   link_state     = NWAM_STATE_OFFLINE;
    nwam_aux_state_t               link_aux_state = NWAM_AUX_STATE_UNINITIALIZED;
    gint64                         active_prio    = nwamui_ncp_get_prio_group( self->prv->ncp );
    gint64                         ncu_prio       = nwamui_ncu_get_priority_group(self);
    nwamui_cond_activation_mode_t  activation_mode;

    activation_mode = nwamui_object_get_activation_mode(object);

    state = nwamui_object_get_nwam_state(object, &aux_state, NULL);
    link_state = nwamui_ncu_get_link_nwam_state(self, &link_aux_state, NULL);

    if ( state == NWAM_STATE_ONLINE ||
      (state == NWAM_STATE_OFFLINE_TO_ONLINE &&
        aux_state == NWAM_AUX_STATE_IF_WAITING_FOR_ADDR) )  {
        active = TRUE;
    }

    else if ( activation_mode == NWAMUI_COND_ACTIVATION_MODE_MANUAL || ncu_prio == active_prio) {

        if (link_state == NWAM_STATE_OFFLINE_TO_ONLINE
          && (link_aux_state == NWAM_AUX_STATE_LINK_WIFI_NEED_KEY
            /* || link_aux_state == NWAM_AUX_STATE_LINK_WIFI_NEED_SELECTION */
            /* || aux_state == NWAM_AUX_STATE_LINK_WIFI_CONNECTING */)) {

            /* Lin, nwamd may hang on wifi connecting state for a very long
             * time. We should not set active to true in this case which
             * will mark the corresponding wifinet menu-item of the GUI.
             *
             * We don't mark wifinet menu-item util the wireless really
             * connect to that wlan.
             */

            /* Special case for Wireless - they are marked as
             * offline* while waiting for a key, but for the UI
             * this means it should be seen as active
             */
            active = TRUE;
        }
    }

    if ( active != prv->active ) {
        prv->active = active;
        if (prv->ncu_type == NWAMUI_NCU_TYPE_WIRELESS && prv->wifi_info) {
            nwamui_wifi_net_set_status(prv->wifi_info,
              active? NWAMUI_WIFI_STATUS_CONNECTED:NWAMUI_WIFI_STATUS_DISCONNECTED);
            g_object_notify(G_OBJECT(prv->wifi_info), "status");
        }
    }

    return active;
}

/** 
 * nwamui_ncu_set_active:
 * @nwamui_ncu: a #NwamuiNcu.
 * @active: Valiue to set active to.
 * 
 **/ 
static void
nwamui_object_real_set_active (NwamuiObject *object, gboolean active)
{
    NwamuiNcuPrivate              *prv  = NWAMUI_NCU_GET_PRIVATE(object);
    NwamuiNcu                     *self = NWAMUI_NCU(object);
    nwamui_cond_activation_mode_t  activation_mode;

    g_return_if_fail (NWAMUI_IS_NCU (self));

    activation_mode = (nwamui_cond_activation_mode_t)
      get_nwam_ncu_uint64_prop( prv->nwam_ncu_phys, NWAM_NCU_PROP_ACTIVATION_MODE );
                
    if ( activation_mode == NWAMUI_COND_ACTIVATION_MODE_MANUAL ) {
        /* Activate immediately */
        if ( active ) {
            nwam_error_t nerr;
            if ( prv->nwam_ncu_phys) {
                if ( (nerr = nwam_ncu_enable (prv->nwam_ncu_phys)) != NWAM_SUCCESS ) {
                    g_warning("Failed to enable ncu_phys due to error: %s", nwam_strerror(nerr));
                }
            }
            if ( prv->nwam_ncu_ip) {
                if ( (nerr = nwam_ncu_enable (prv->nwam_ncu_ip)) != NWAM_SUCCESS ) {
                    g_warning("Failed to enable ncu_ip due to error: %s", nwam_strerror(nerr));
                }
            }
#ifdef TUNNEL_SUPPORT
            if ( prv->nwam_ncu_iptun) {
                if ( (nerr = nwam_ncu_enable (prv->nwam_ncu_iptun)) != NWAM_SUCCESS ) {
                    g_warning("Failed to enable ncu_iptun due to error: %s", nwam_strerror(nerr));
                }
            }
#endif /* TUNNEL_SUPPORT */
        }
        else {
            nwam_error_t nerr;
            if ( prv->nwam_ncu_ip) {
                if ( (nerr = nwam_ncu_disable (prv->nwam_ncu_ip)) != NWAM_SUCCESS ) {
                    g_warning("Failed to disable ncu_ip due to error: %s", nwam_strerror(nerr));
                }
            }
#ifdef TUNNEL_SUPPORT
            if ( prv->nwam_ncu_iptun) {
                if ( (nerr = nwam_ncu_disable (prv->nwam_ncu_iptun)) != NWAM_SUCCESS ) {
                    g_warning("Failed to disable ncu_iptun due to error: %s", nwam_strerror(nerr));
                }
            }
#endif /* TUNNEL_SUPPORT */
            if ( prv->nwam_ncu_phys) {
                if ( (nerr = nwam_ncu_disable (prv->nwam_ncu_phys)) != NWAM_SUCCESS ) {
                    g_warning("Failed to disable ncu_phys due to error: %s", nwam_strerror(nerr));
                }
            }
        }
    }
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
 * nwamui_ncu_set_ipv4_active:
 * @nwamui_ncu: a #NwamuiNcu.
 * @ipv4_active: Valiue to set ipv4_active to.
 * 
 **/ 
extern void
nwamui_ncu_set_ipv4_active (   NwamuiNcu *self,
                              gboolean        ipv4_active )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));

    g_object_set (G_OBJECT (self),
                  "ipv4_active", ipv4_active,
                  NULL);
}

/**
 * nwamui_ncu_get_ipv4_active:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the ipv4_active.
 *
 **/
extern gboolean
nwamui_ncu_get_ipv4_active (NwamuiNcu *self)
{
    gboolean  ipv4_active = FALSE; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), ipv4_active);

    g_object_get (G_OBJECT (self),
                  "ipv4_active", &ipv4_active,
                  NULL);

    return( ipv4_active );
}

/** 
 * nwamui_ncu_set_ipv4_has_dhcp:
 * @nwamui_ncu: a #NwamuiNcu.
 * @ipv4_has_dhcp: Valiue to set ipv4_has_dhcp to.
 * 
 **/ 
extern void
nwamui_ncu_set_ipv4_has_dhcp (   NwamuiNcu *self,
                                 gboolean        ipv4_has_dhcp )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));

    g_object_set (G_OBJECT (self),
                  "ipv4_has_dhcp", ipv4_has_dhcp,
                  NULL);
}

/**
 * nwamui_ncu_get_ipv4_has_dhcp:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the ipv4_has_dhcp.
 *
 **/
extern gboolean
nwamui_ncu_get_ipv4_has_dhcp (NwamuiNcu *self)
{
    gboolean  ipv4_has_dhcp = TRUE; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), ipv4_has_dhcp);

    g_object_get (G_OBJECT (self),
                  "ipv4_has_dhcp", &ipv4_has_dhcp,
                  NULL);

    return( ipv4_has_dhcp );
}

extern gboolean
nwamui_ncu_get_ipv4_has_static (NwamuiNcu *self)
{
    gint      num_static;

    g_return_val_if_fail (NWAMUI_IS_NCU (self), FALSE);

    num_static = gtk_tree_model_iter_n_children( GTK_TREE_MODEL(self->prv->v4addresses), NULL );

    return( num_static > 0 );
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
 * nwamui_ncu_set_ipv4_default_route:
 * @nwamui_ncu: a #NwamuiNcu.
 * @ipv4_default_route: Valiue to set ipv4_default_route to.
 *
 **/
extern void
nwamui_ncu_set_ipv4_default_route (   NwamuiNcu *self,
                              const gchar*  ipv4_default_route )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));

    g_object_set (G_OBJECT (self),
                  "ipv4_default_route", ipv4_default_route,
                  NULL);
}

/**
 * nwamui_ncu_get_ipv4_default_route:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the ipv4_default_route.
 *
 **/
extern gchar*
nwamui_ncu_get_ipv4_default_route (NwamuiNcu *self)
{
    gchar*  ipv4_default_route = NULL;

    g_return_val_if_fail (NWAMUI_IS_NCU (self), ipv4_default_route);

    g_object_get (G_OBJECT (self),
                  "ipv4_default_route", &ipv4_default_route,
                  NULL);

    return( ipv4_default_route );
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
 * nwamui_ncu_set_ipv6_has_dhcp:
 * @nwamui_ncu: a #NwamuiNcu.
 * @ipv6_has_dhcp: Valiue to set ipv6_has_dhcp to.
 * 
 **/ 
extern void
nwamui_ncu_set_ipv6_has_dhcp (   NwamuiNcu *self,
                              gboolean        ipv6_has_dhcp )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));

    g_object_set (G_OBJECT (self),
                  "ipv6_has_dhcp", ipv6_has_dhcp,
                  NULL);
}

/**
 * nwamui_ncu_get_ipv6_has_dhcp:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the ipv6_has_dhcp.
 *
 **/
extern gboolean
nwamui_ncu_get_ipv6_has_dhcp (NwamuiNcu *self)
{
    gboolean  ipv6_has_dhcp = TRUE; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), ipv6_has_dhcp);

    g_object_get (G_OBJECT (self),
                  "ipv6_has_dhcp", &ipv6_has_dhcp,
                  NULL);

    return( ipv6_has_dhcp );
}

/** 
 * nwamui_ncu_set_ipv6_has_auto_conf:
 * @nwamui_ncu: a #NwamuiNcu.
 * @ipv6_has_auto_conf: Valiue to set ipv6_has_auto_conf to.
 * 
 **/ 
extern void
nwamui_ncu_set_ipv6_has_auto_conf (   NwamuiNcu *self,
                              gboolean        ipv6_has_auto_conf )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));

    g_object_set (G_OBJECT (self),
                  "ipv6_has_auto_conf", ipv6_has_auto_conf,
                  NULL);
}

/**
 * nwamui_ncu_get_ipv6_has_auto_conf:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the ipv6_has_auto_conf.
 *
 **/
extern gboolean
nwamui_ncu_get_ipv6_has_auto_conf (NwamuiNcu *self)
{
    gboolean  ipv6_has_auto_conf = TRUE; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), ipv6_has_auto_conf);

    g_object_get (G_OBJECT (self),
                  "ipv6_has_auto_conf", &ipv6_has_auto_conf,
                  NULL);

    return( ipv6_has_auto_conf );
}

extern gboolean
nwamui_ncu_get_ipv6_has_static (NwamuiNcu *self)
{
    gint      num_static;

    g_return_val_if_fail (NWAMUI_IS_NCU (self), FALSE);

    num_static = gtk_tree_model_iter_n_children( GTK_TREE_MODEL(self->prv->v6addresses), NULL );

    return( num_static > 0 );
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
 * nwamui_ncu_set_ipv6_default_route:
 * @nwamui_ncu: a #NwamuiNcu.
 * @ipv6_default_route: Valiue to set ipv6_default_route to.
 *
 **/
extern void
nwamui_ncu_set_ipv6_default_route (   NwamuiNcu *self,
                              const gchar*  ipv6_default_route )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));

    g_object_set (G_OBJECT (self),
                  "ipv6_default_route", ipv6_default_route,
                  NULL);
}

/**
 * nwamui_ncu_get_ipv6_default_route:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the ipv6_default_route.
 *
 **/
extern gchar*
nwamui_ncu_get_ipv6_default_route (NwamuiNcu *self)
{
    gchar*  ipv6_default_route = NULL;

    g_return_val_if_fail (NWAMUI_IS_NCU (self), ipv6_default_route);

    g_object_get (G_OBJECT (self),
                  "ipv6_default_route", &ipv6_default_route,
                  NULL);

    return( ipv6_default_route );
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
 * NULL to unset a wifi info and set the disconnected flag.
 **/
extern void
nwamui_ncu_set_wifi_info ( NwamuiNcu *self, NwamuiWifiNet* wifi_info )
{
    g_return_if_fail (NWAMUI_IS_NCU(self)); 

    g_assert( wifi_info == NULL || NWAMUI_IS_WIFI_NET( wifi_info ) );
    /* Should be a Wireless i/f */
    g_assert( self->prv->ncu_type == NWAMUI_NCU_TYPE_WIRELESS );

    if (self->prv->wifi_info != wifi_info) {
        if ( self->prv->wifi_info != NULL ) {
            g_object_unref( self->prv->wifi_info );
            g_signal_handlers_disconnect_matched(
                G_OBJECT(self->prv->wifi_info),
                  G_SIGNAL_MATCH_DATA,
                  0,
                  NULL,
                  NULL,
                  NULL,
                  (gpointer)self);
            nwamui_wifi_net_set_status(self->prv->wifi_info, NWAMUI_WIFI_STATUS_DISCONNECTED);
        }
        self->prv->wifi_info = g_object_ref(wifi_info);
        g_signal_connect (G_OBJECT(self->prv->wifi_info), "notify",
          G_CALLBACK(wireless_notify_cb), (gpointer)self);

        g_object_notify(G_OBJECT(self), "wifi_info");
    }
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
static void
nwamui_object_real_set_activation_mode (NwamuiObject *object, gint activation_mode)
{
    NwamuiNcu *self = NWAMUI_NCU(object);
    gboolean currently_enabled;
    nwam_error_t    nerr;

    g_return_if_fail (NWAMUI_IS_NCU (self));
    g_assert (activation_mode >= NWAMUI_COND_ACTIVATION_MODE_MANUAL && activation_mode < NWAMUI_COND_ACTIVATION_MODE_LAST );

    switch (activation_mode) {
    case NWAMUI_COND_ACTIVATION_MODE_PRIORITIZED:
        /* Activation mode is going to change to PRIORITIZED, we must enable
         * the phys/ip/iptun before change it, otherwise enabled will become
         * a readonly property.
         */
        currently_enabled = get_nwam_ncu_boolean_prop( self->prv->nwam_ncu_phys, NWAM_NCU_PROP_ENABLED );
        if (!currently_enabled) {
            if ( (nerr = nwam_ncu_enable (self->prv->nwam_ncu_phys)) != NWAM_SUCCESS ) {
                g_warning("Failed to enable ncu_phys due to error: %s", nwam_strerror(nerr));
            }
        }
        currently_enabled = get_nwam_ncu_boolean_prop( self->prv->nwam_ncu_ip, NWAM_NCU_PROP_ENABLED );
        if (!currently_enabled) {
            if ( (nerr = nwam_ncu_enable (self->prv->nwam_ncu_ip)) != NWAM_SUCCESS ) {
                g_warning("Failed to enable ncu_ip due to error: %s", nwam_strerror(nerr));
            }
        }
#ifdef TUNNEL_SUPPORT
        currently_enabled = get_nwam_ncu_boolean_prop( self->prv->nwam_ncu_iptun, NWAM_NCU_PROP_ENABLED );
        if (!currently_enabled) {
            if ( (nerr = nwam_ncu_enable (self->prv->nwam_ncu_iptun)) != NWAM_SUCCESS ) {
                g_warning("Failed to enable ncu_iptun due to error: %s", nwam_strerror(nerr));
            }
        }
#endif /* TUNNEL_SUPPORT */

        /* Must update enabled flag. */
        self->prv->enabled = TRUE;
        break;
    default:
        break;
    }
    set_nwam_ncu_uint64_prop( self->prv->nwam_ncu_phys, NWAM_NCU_PROP_ACTIVATION_MODE, (guint64)activation_mode );
    set_modified_flag( self, NWAM_NCU_CLASS_PHYS, TRUE );
}

/**
 * nwamui_ncu_get_activation_mode:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the activation_mode.
 *
 **/
static gint
nwamui_object_real_get_activation_mode (NwamuiObject *object)
{
    NwamuiNcu *self = NWAMUI_NCU(object);
    nwamui_cond_activation_mode_t activation_mode;

    g_return_val_if_fail (NWAMUI_IS_NCU (self), activation_mode);

    activation_mode = (nwamui_cond_activation_mode_t)
      get_nwam_ncu_uint64_prop( self->prv->nwam_ncu_phys, NWAM_NCU_PROP_ACTIVATION_MODE );

    return( (nwamui_cond_activation_mode_t)activation_mode );
}

/** 
 * nwamui_ncu_set_enabled:
 * @nwamui_ncu: a #NwamuiNcu.
 * @enabled: Value to set enabled to.
 * 
 **/ 
static void
nwamui_object_real_set_enabled(NwamuiObject *object, gboolean enabled)
{
    NwamuiNcu *self = NWAMUI_NCU(object);
    g_return_if_fail (NWAMUI_IS_NCU (self));

    if (self->prv->enabled != enabled) {
        self->prv->enabled = enabled;
        g_object_notify(G_OBJECT(object), "enabled" );
        set_modified_flag( self, NWAM_NCU_CLASS_PHYS, TRUE );
    }
}

/**
 * nwamui_ncu_get_enabled:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the enabled.
 *
 **/
static gboolean
nwamui_object_real_get_enabled (NwamuiObject *object)
{
    NwamuiNcu *self = NWAMUI_NCU(object);

    g_return_val_if_fail (NWAMUI_IS_NCU(self), FALSE);

    return self->prv->enabled;
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

extern gint
nwamui_ncu_get_priority_group_for_view(NwamuiNcu *ncu)
{
    nwamui_cond_activation_mode_t act_mode = nwamui_object_get_activation_mode(NWAMUI_OBJECT(ncu));
    switch (act_mode) {
    case NWAMUI_COND_ACTIVATION_MODE_MANUAL:
        return nwamui_object_get_enabled(NWAMUI_OBJECT(ncu)) ? ALWAYS_ON_GROUP_ID : ALWAYS_OFF_GROUP_ID;
    case NWAMUI_COND_ACTIVATION_MODE_PRIORITIZED:
        return nwamui_ncu_get_priority_group(ncu) + ALWAYS_ON_GROUP_ID + 1;
    default:
        g_warning("%s: Not supported activation mode %d", __func__, act_mode);
        return ALWAYS_OFF_GROUP_ID;
    }
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

static void
foreach_wifi_clean_dead(gpointer key, gpointer value, gpointer user_data)
{
    GList **deads = (GList **)user_data;
    
    if (nwamui_wifi_net_get_life_state(NWAMUI_WIFI_NET(value)) == NWAMUI_WIFI_LIFE_DEAD) {
        /* No ref */
        *deads = g_list_prepend(*deads, value);
    }
}

extern void
nwamui_ncu_wifi_hash_clean_dead_wifi_nets(NwamuiNcu *self)
{
    GList *deads = NULL;

    g_return_if_fail(NWAMUI_IS_NCU(self));

    nwamui_ncu_wifi_hash_foreach(self, foreach_wifi_clean_dead, (gpointer)&deads);

    for (; deads; deads = g_list_delete_link(deads, deads)) {
        nwamui_ncu_wifi_hash_remove_wifi_net(self, NWAMUI_WIFI_NET(deads->data));
    }
}

static void
foreach_wifi_mark(gpointer key, gpointer value, gpointer user_data)
{
    nwamui_wifi_life_state_t state = (nwamui_wifi_life_state_t)user_data;
    
    nwamui_wifi_net_set_life_state(NWAMUI_WIFI_NET(value), state);
}

extern void
nwamui_ncu_wifi_hash_mark_each(NwamuiNcu *self, nwamui_wifi_life_state_t state)
{
    g_return_if_fail(NWAMUI_IS_NCU(self));

    nwamui_ncu_wifi_hash_foreach(self, foreach_wifi_mark, (gpointer)state);
}

/*
 * Functions to handle a hash table of wifi_net objects for the NCU.
 */
extern NwamuiWifiNet*
nwamui_ncu_wifi_hash_lookup_by_essid(NwamuiNcu *self, const gchar *essid)
{
    NwamuiWifiNet  *wifi_net = NULL;
    gpointer        value;

    g_return_val_if_fail(NWAMUI_IS_NCU(self), NULL);
    g_return_val_if_fail(essid, NULL);

    if ( (value = g_hash_table_lookup( self->prv->wifi_hash_table, essid )) != NULL ) {
        wifi_net = NWAMUI_WIFI_NET(value);
    }

    nwamui_debug("search for essid '%s' - returning 0x%p", essid, wifi_net);

    return(wifi_net);
}

extern void
nwamui_ncu_wifi_hash_insert_wifi_net( NwamuiNcu     *self, 
                                      NwamuiWifiNet *wifi_net )
{
    gpointer     value = NULL;
    const gchar* essid;

    g_return_if_fail(NWAMUI_IS_NCU(self));
    g_return_if_fail(NWAMUI_IS_WIFI_NET(wifi_net));

    essid = nwamui_object_get_name(NWAMUI_OBJECT(wifi_net));

    if ( essid != NULL ) {
        if ( (value = g_hash_table_lookup( self->prv->wifi_hash_table, essid )) == NULL ) {
            g_hash_table_insert(self->prv->wifi_hash_table, g_strdup(essid),
              g_object_ref(wifi_net) );

            nwamui_wifi_net_set_life_state(NWAMUI_WIFI_NET(wifi_net), NWAMUI_WIFI_LIFE_NEW);
            /* hash table taken ownership of essid */
        }
        else {
            nwamui_warning("Unexpected existing wifi_net in hash table with essid : %s", essid );
        }
    }
}

extern NwamuiWifiNet*
nwamui_ncu_wifi_hash_insert_or_update_from_wlan_t(NwamuiNcu *self, nwam_wlan_t *wlan)
{
    NwamuiWifiNet   *wifi_net = NULL;

    g_return_val_if_fail(NWAMUI_IS_NCU(self), NULL);
    g_return_val_if_fail(wlan, NULL);

    if ((wifi_net = nwamui_ncu_wifi_hash_lookup_by_essid(self, wlan->nww_essid)) != NULL) {
        g_object_ref(wifi_net);
        nwamui_wifi_net_update_from_wlan_t(wifi_net, wlan);
        nwamui_wifi_net_set_life_state(NWAMUI_WIFI_NET(wifi_net), NWAMUI_WIFI_LIFE_MODIFIED);
    } else {
        wifi_net = nwamui_wifi_net_new_from_wlan_t(self, wlan);
        nwamui_ncu_wifi_hash_insert_wifi_net(self, wifi_net);
    }

    return wifi_net;
}

extern NwamuiWifiNet*
nwamui_ncu_wifi_hash_insert_or_update_from_handle( NwamuiNcu                 *self, 
                                                   nwam_known_wlan_handle_t   wlan_h )
{
    NwamuiWifiNet  *wifi_net = NULL;
    char           *essid = NULL;
    nwam_error_t    nerr;

    g_return_val_if_fail(NWAMUI_IS_NCU(self), NULL);
    g_return_val_if_fail(wlan_h, NULL);

    if ( (nerr = nwam_known_wlan_get_name(wlan_h, &essid)) != NWAM_SUCCESS) {
        g_warning("Error getting name of known wlan: %s", nwam_strerror(nerr));
        return( NULL );
    }

    if ( (wifi_net = nwamui_ncu_wifi_hash_lookup_by_essid( self, essid ) ) != NULL ) {
        nwamui_object_set_handle(NWAMUI_OBJECT(wifi_net), wlan_h);
        nwamui_wifi_net_set_life_state(NWAMUI_WIFI_NET(wifi_net), NWAMUI_WIFI_LIFE_MODIFIED);
    }
    else {
        wifi_net = nwamui_wifi_net_new_with_handle( self, wlan_h );
        nwamui_ncu_wifi_hash_insert_wifi_net( self, wifi_net );
    }

    free(essid);
    return( wifi_net );
}

extern gboolean
nwamui_ncu_wifi_hash_remove_by_essid( NwamuiNcu     *self, 
                                      const gchar   *essid )
{
    g_return_val_if_fail(NWAMUI_IS_NCU(self), FALSE);
    g_return_val_if_fail(essid, FALSE);

    return( g_hash_table_remove(self->prv->wifi_hash_table, essid ) );
}

extern gboolean
nwamui_ncu_wifi_hash_remove_wifi_net( NwamuiNcu     *self, 
                                      NwamuiWifiNet *wifi_net )
{
    const gchar* essid;
    gboolean     rval = FALSE;

    g_return_val_if_fail(NWAMUI_IS_NCU(self), FALSE);
    g_return_val_if_fail(wifi_net, FALSE);

    essid = nwamui_object_get_name(NWAMUI_OBJECT(wifi_net));

    if ( essid != NULL ) {
        rval = nwamui_ncu_wifi_hash_remove_by_essid( self, essid );
    }

    return(rval);
}

extern void
nwamui_ncu_wifi_hash_foreach(NwamuiNcu *self, GHFunc func, gpointer user_data)
{
    NwamuiNcuPrivate              *prv  = NWAMUI_NCU_GET_PRIVATE(self);

    g_return_if_fail(NWAMUI_IS_NCU(self));

    g_hash_table_foreach(prv->wifi_hash_table, func, user_data);
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

static void
nwamui_ncu_finalize (NwamuiNcu *self)
{
    if (self->prv->v4addresses != NULL ) {
        g_object_unref( G_OBJECT(self->prv->v4addresses) );
    }

    if (self->prv->v6addresses != NULL ) {
        g_object_unref( G_OBJECT(self->prv->v6addresses) );
    }

    if ( self->prv->wifi_hash_table != NULL ) {
        g_hash_table_destroy(self->prv->wifi_hash_table);
    }
    if (self->prv->vanity_name) {
        g_free(self->prv->vanity_name);
    }
    if (self->prv->display_name) {
        g_free(self->prv->display_name);
    }
    if (self->prv->device_name) {
        g_free(self->prv->device_name);
    }

    self->prv = NULL;

	G_OBJECT_CLASS(nwamui_ncu_parent_class)->finalize(G_OBJECT(self));
}

/**
 * nwamui_object_real_set_interface_state:
 *
 * Interface event emits after link event, e.g.
LINK_STATE             e1000g0 -> state up                                     
OBJECT_STATE           ncu link:e1000g0 -> state offline*, interface/link is u 
OBJECT_STATE           ncu link:e1000g0 -> state online, interface/link is up  
OBJECT_STATE           ncu interface:e1000g0 -> state offline*, (re)initialize 
OBJECT_STATE           ncu interface:e1000g0 -> state offline*, waiting for IP 
IF_STATE               e1000g0 -> state (17) flags 1000842                     
IF_STATE               e1000g0 -> state (17) flags 1000842                     
IF_STATE               e1000g0 -> state (17) flags 1000843                     
IF_STATE               e1000g0 -> state (17) flags 2000840                     
IF_STATE               e1000g0 -> state (17) flags 2000840                     
IF_STATE               e1000g0 -> state (17) flags 2000841                     
IF_STATE               e1000g0 -> state (17) flags 1004843                     
IF_STATE               e1000g0 -> state (17) flags 1004842                     
IF_STATE               e1000g0 -> state (17) flags 1004843                     
IF_STATE               e1000g0 -> state (17) flags 1004842                     
IF_STATE               e1000g0 -> state index 17 flags 0x0 address 129.158.217 
OBJECT_STATE           ncu interface:e1000g0 -> state offline*, interface/link 
OBJECT_STATE           ncu interface:e1000g0 -> state online, interface/link i 

LINK_STATE             e1000g0 -> state down                                   
OBJECT_STATE           ncu link:e1000g0 -> state online*, interface/link is do 
OBJECT_STATE           ncu link:e1000g0 -> state offline, interface/link is do 
OBJECT_STATE           ncu interface:e1000g0 -> state online*, conditions for  
OBJECT_STATE           ncu interface:e1000g0 -> state offline, conditions for  
 */
static void
nwamui_object_set_interface_nwam_state(NwamuiObject *object, nwam_state_t state, nwam_aux_state_t aux_state)
{
    NwamuiNcuPrivate          *prv             = NWAMUI_NCU_GET_PRIVATE(object);

    if ( prv->iface_state != state ||
         prv->iface_aux_state != aux_state ) {
        prv->iface_state = state;
        prv->iface_aux_state = aux_state;

        g_object_notify(G_OBJECT(object), "nwam_state" );
    }

    nwamui_ncu_update_state(NWAMUI_NCU(object));
	/* NWAMUI_OBJECT_CLASS(nwamui_ncu_parent_class)->set_nwam_state(object, state, aux_state); */
}

extern nwam_state_t
nwamui_ncu_get_interface_nwam_state(NwamuiObject *object, nwam_aux_state_t* aux_state, const gchar**aux_state_string)
{
    NwamuiNcuPrivate *prv    = NWAMUI_NCU_GET_PRIVATE(object);

    g_return_val_if_fail(NWAMUI_IS_NCU( object ), NWAM_STATE_UNINITIALIZED);

    if (prv->iface_state == NWAM_STATE_UNINITIALIZED && prv->iface_aux_state == NWAM_AUX_STATE_UNINITIALIZED) {
        if (prv->nwam_ncu_ip) {
            if (nwam_ncu_get_state(prv->nwam_ncu_ip, &prv->iface_state, &prv->iface_aux_state) == NWAM_SUCCESS) {
            } else {
                prv->iface_state = NWAM_STATE_UNINITIALIZED;
                prv->iface_aux_state = NWAM_AUX_STATE_UNINITIALIZED;
            }
        }
    }

    if ( aux_state ) {
        *aux_state = prv->iface_aux_state;
    }
    if ( aux_state_string ) {
        *aux_state_string = (const gchar*)nwam_aux_state_to_string(prv->iface_aux_state);
    }

    return prv->iface_state;
}

extern nwam_state_t
nwamui_ncu_get_link_nwam_state(NwamuiNcu *self, nwam_aux_state_t* aux_state, const gchar**aux_state_string)
{
    NwamuiNcuPrivate *prv      = NWAMUI_NCU_GET_PRIVATE(self);

    g_return_val_if_fail(NWAMUI_IS_NCU(self), NWAM_STATE_UNINITIALIZED);

    /* Initializing link state, important for initially show panel icon. */
    if (prv->link_state == NWAM_STATE_UNINITIALIZED && prv->link_aux_state == NWAM_AUX_STATE_UNINITIALIZED) {
        if (prv->nwam_ncu_phys) {
            if (nwam_ncu_get_state(prv->nwam_ncu_phys, &prv->link_state, &prv->link_aux_state) == NWAM_SUCCESS) {
            } else {
                prv->link_state = NWAM_STATE_UNINITIALIZED;
                prv->link_aux_state = NWAM_AUX_STATE_UNINITIALIZED;
            }
        }
    }

    if ( aux_state ) {
        *aux_state = prv->link_aux_state;
    }
    if ( aux_state_string ) {
        *aux_state_string = (const gchar*)nwam_aux_state_to_string(prv->link_aux_state);
    }

    return prv->link_state;
}

extern void
nwamui_ncu_set_link_nwam_state(NwamuiNcu *self, nwam_state_t state, nwam_aux_state_t aux_state)
{
    NwamuiNcuPrivate *prv           = NWAMUI_NCU_GET_PRIVATE(self);

    g_return_if_fail (NWAMUI_IS_NCU(self));

    g_object_freeze_notify(G_OBJECT(self));

    if ( prv->link_state != state ||
         prv->link_aux_state != aux_state ) {
        prv->link_state = state;
        prv->link_aux_state = aux_state;

        g_object_notify(G_OBJECT(self), "nwam_state" );
    }

    /* Make sure interface state is updated. */
    nwamui_object_get_nwam_state(NWAMUI_OBJECT(self), NULL, NULL);

    nwamui_ncu_update_state(NWAMUI_NCU(self));

    g_object_thaw_notify(G_OBJECT(self));
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
        g_debug("No value for ncu property %s, error = %s", prop_name, nwam_strerror( nerr ) );
        return retval;
    }

    if ( (nerr = nwam_value_get_string(nwam_data, &value )) != NWAM_SUCCESS ) {
        g_debug("Unable to get string value for ncu property %s, error = %s", prop_name, nwam_strerror( nerr ) );
        return retval;
    }

    if ( value != NULL ) {
        retval  = g_strdup ( value );
    }

    nwam_value_free(nwam_data);
    
    return( retval );
}

static gboolean
prop_is_readonly( const char* prop_name ) 
{
    boolean_t       read_only = B_FALSE;
    nwam_error_t    nerr;

    if ( (nerr = nwam_ncu_prop_read_only( prop_name, &read_only ) ) != NWAM_SUCCESS ) {
        g_warning("Unable to get read-only status for ncu property %s: %s", prop_name, nwam_strerror(nerr) );
        return FALSE;
    }

    return( (read_only == B_TRUE)?TRUE:FALSE );
}

static void 
set_modified_flag( NwamuiNcu* self, nwam_ncu_class_t ncu_class, gboolean value )
{

    if ( self->prv->initialisation ) {
        g_debug("Ignore setting modified flag since in initialisation");
        return;
    }

    g_debug("Setting modified flag for CLASS %d on NCU %s to %s", 
            ncu_class, self->prv->device_name, value?"TRUE":"FALSE");
    switch ( ncu_class ) {
        case NWAM_NCU_CLASS_PHYS: {
                self->prv->nwam_ncu_phys_modified = value;
            }
            break;
#ifdef TUNNEL_SUPPORT
        case NWAM_NCU_CLASS_IPTUN: {
                self->prv->nwam_ncu_iptun_modified = value;
            }
            break;
#endif /* TUNNEL_SUPPORT */
        case NWAM_NCU_CLASS_IP: {
                self->prv->nwam_ncu_ip_modified = value;
            }
            break;
        default:
            g_error("Unexpected ncu class %u", (guint)ncu_class);
    }
}

static gboolean
delete_nwam_ncu_prop( nwam_ncu_handle_t ncu, const char* prop_name )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    gboolean            retval = FALSE;

    g_return_val_if_fail( prop_name != NULL, retval );

    if ( prop_is_readonly( prop_name ) ) {
        g_error("Attempting to delete a read-only ncu property %s", prop_name );
        return retval;
    }

    if ( (nerr = nwam_ncu_delete_prop(ncu, prop_name)) != NWAM_SUCCESS ) {
        g_debug("Unable to delete ncu property %s, error = %s", prop_name, nwam_strerror( nerr ) );
    }
    else {
        retval = TRUE;
    }

    return( retval );
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

    if ( str == NULL ) {
        retval = delete_nwam_ncu_prop( ncu, prop_name );
    }
    else {
        if ( (nerr = nwam_value_create_string( (char*)str, &nwam_data )) != NWAM_SUCCESS ) {
            g_debug("Error creating a string value for string %s", str?str:"NULL");
            return retval;
        }

        if ( (nerr = nwam_ncu_set_prop_value (ncu, prop_name, nwam_data)) != NWAM_SUCCESS ) {
            g_debug("Unable to set value for ncu property %s, error = %s", prop_name, nwam_strerror( nerr ) );
        }
        else {
            retval = TRUE;
        }

        nwam_value_free(nwam_data);
    }

    return( retval );
}

static gchar**
get_nwam_ncu_string_array_prop( nwam_ncu_handle_t ncu, const char* prop_name, guint *len )
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
        g_debug("No value for ncu property %s, error = %s", prop_name, nwam_strerror( nerr ) );
        return retval;
    }

    if ( (nerr = nwam_value_get_string_array(nwam_data, &value, &num )) != NWAM_SUCCESS ) {
        g_debug("Unable to get string value for ncu property %s, error = %s", prop_name, nwam_strerror( nerr ) );
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

    if ( len != NULL ) {
        *len = num;
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
        g_debug("Unable to set value for ncu property %s, error = %s", prop_name, nwam_strerror( nerr ) );
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
        g_debug("No value for ncu property %s, error = %s", prop_name, nwam_strerror( nerr ) );
        return value;
    }

    if ( (nerr = nwam_value_get_boolean(nwam_data, &value )) != NWAM_SUCCESS ) {
        g_debug("Unable to get boolean value for ncu property %s, error = %s", prop_name, nwam_strerror( nerr ) );
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
        g_debug("Unable to set value for ncu property %s, error = %s", prop_name, nwam_strerror( nerr ) );
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
        g_debug("No value for ncu property %s, error = %s", prop_name, nwam_strerror( nerr ) );
        return value;
    }

    if ( (nerr = nwam_value_get_uint64(nwam_data, &value )) != NWAM_SUCCESS ) {
        g_debug("Unable to get uint64 value for ncu property %s, error = %s", prop_name, nwam_strerror( nerr ) );
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
        g_debug("Unable to set value for ncu property %s, error = %s", prop_name, nwam_strerror( nerr ) );
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
        g_debug("No value for ncu property %s, error = %s", prop_name, nwam_strerror( nerr ) );
        return value;
    }

    if ( (nerr = nwam_value_get_uint64_array(nwam_data, &value, &num )) != NWAM_SUCCESS ) {
        g_debug("Unable to get uint64 value for ncu property %s, error = %s", prop_name, nwam_strerror( nerr ) );
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
        g_debug("Unable to set value for ncu property %s, error = %s", prop_name, nwam_strerror( nerr ) );
    }
    else {
        retval = TRUE;
    }

    nwam_value_free(nwam_data);
    
    return( retval );
}

extern nwamui_wifi_signal_strength_t
nwamui_ncu_get_signal_strength_from_dladm( NwamuiNcu* self )
{
    dladm_handle_t          handle;
    datalink_id_t           linkid;
    dladm_wlan_linkattr_t	attr;
    nwamui_wifi_signal_strength_t signal = NWAMUI_WIFI_STRENGTH_NONE;
    
    g_return_val_if_fail( NWAMUI_IS_NCU( self ), signal );

    if ( self->prv->ncu_type != NWAMUI_NCU_TYPE_WIRELESS ) {
        return( signal );
    }

    if ( dladm_open( &handle ) == DLADM_STATUS_OK ) {
        if ( dladm_name2info( handle, self->prv->device_name, &linkid, NULL, NULL, NULL ) == DLADM_STATUS_OK ) {
            dladm_status_t status = dladm_wlan_get_linkattr(handle, linkid, &attr);
            if (status == DLADM_STATUS_OK ) {
                if ( attr.la_valid |= DLADM_WLAN_LINKATTR_WLAN &&
                  attr.la_wlan_attr.wa_valid & DLADM_WLAN_ATTR_STRENGTH ) {
                    switch ( attr.la_wlan_attr.wa_strength ) {
                    case DLADM_WLAN_STRENGTH_VERY_WEAK:
                        signal = NWAMUI_WIFI_STRENGTH_VERY_WEAK;
                        break;
                    case DLADM_WLAN_STRENGTH_WEAK:
                        signal = NWAMUI_WIFI_STRENGTH_WEAK;
                        break;
                    case DLADM_WLAN_STRENGTH_GOOD:
                        signal = NWAMUI_WIFI_STRENGTH_GOOD;
                        break;
                    case DLADM_WLAN_STRENGTH_VERY_GOOD:
                        signal = NWAMUI_WIFI_STRENGTH_VERY_GOOD;
                        break;
                    case DLADM_WLAN_STRENGTH_EXCELLENT:
                        signal = NWAMUI_WIFI_STRENGTH_EXCELLENT;
                        break;
                    default:
                        break;
                    }
                }
            } else {
                g_error("cannot get link attributes for %s", self->prv->device_name );
            }
        } else {
            g_warning("Unable to map device to linkid");
        }
        dladm_close( handle );
    } else {
        g_warning("Error creating dladm handle" );
    }
    
    return( signal );
}

/*
 * NCU Status Messages - read directly from system, not from NWAM 
 */

/* Get signal percent from system */
extern const gchar*
nwamui_ncu_get_signal_strength_string( NwamuiNcu* self )
{
    const gchar*            signal_str = _("None");
    
    g_return_val_if_fail( NWAMUI_IS_NCU( self ), signal_str );

    signal_str = nwamui_wifi_net_convert_strength_to_string(nwamui_ncu_get_signal_strength_from_dladm(self));

    return( signal_str );
}

/* Check for dhcp or autoconf, looking in recently read nwam handle, not any
 * possibly modified nwamui_ip objects.
 */
static void
nwamui_ncu_has_dhcp_configured( NwamuiNcu *ncu, gboolean *ipv4_has_dhcp, gboolean *ipv6_has_dhcp, gboolean *ipv6_autoconf )
{
    guint64            *ip_version = NULL;
    guint               ip_version_num = NULL;
    guint               ipv4_addrsrc_num = 0;
    guint64            *ipv4_addrsrc = NULL;
    gchar**             ipv4_addr = NULL;
    guint               ipv6_addrsrc_num = 0;
    guint64            *ipv6_addrsrc =  NULL;
    gchar**             ipv6_addr = NULL;

    ip_version = get_nwam_ncu_uint64_array_prop( ncu->prv->nwam_ncu_ip, 
                                                 NWAM_NCU_PROP_IP_VERSION, 
                                                 &ip_version_num );

    if ( ipv4_has_dhcp != NULL ) {
        *ipv4_has_dhcp = FALSE;
    }
    if ( ipv6_autoconf != NULL ) {
        *ipv6_autoconf = FALSE;
    }
    if ( ipv6_has_dhcp != NULL ) {
        *ipv6_has_dhcp = FALSE;
    }

    for ( int ip_n = 0; ip_n < ip_version_num; ip_n++ ) {
        if (ip_version[ip_n] == IPV4_VERSION) {
            ipv4_addrsrc = get_nwam_ncu_uint64_array_prop( ncu->prv->nwam_ncu_ip, 
                                                           NWAM_NCU_PROP_IPV4_ADDRSRC, 
                                                           &ipv4_addrsrc_num );

            for( int i = 0; i < ipv4_addrsrc_num; i++ ) {
                if ( ipv4_addrsrc[i] == NWAM_ADDRSRC_DHCP ) {
                    if ( ipv4_has_dhcp != NULL ) {
                        *ipv4_has_dhcp = TRUE;
                    }
                }
            }
            g_free(ipv4_addrsrc);
        }
        else if (ip_version[ip_n] == IPV6_VERSION) {
            ipv6_addrsrc = get_nwam_ncu_uint64_array_prop(  ncu->prv->nwam_ncu_ip, 
                                                            NWAM_NCU_PROP_IPV6_ADDRSRC, 
                                                            &ipv6_addrsrc_num );

            for( int i = 0; i < ipv6_addrsrc_num; i++ ) {
                if ( ipv6_addrsrc[i] == NWAM_ADDRSRC_AUTOCONF ) {
                    if ( ipv6_autoconf != NULL ) {
                        *ipv6_autoconf = TRUE;
                    }
                }
                else if ( ipv6_addrsrc[i] == NWAM_ADDRSRC_DHCP ) {
                    if ( ipv6_has_dhcp != NULL ) {
                        *ipv6_has_dhcp = TRUE;
                    }
                }
            }
            g_free(ipv6_addrsrc);
        }
    }
    g_free(ip_version);
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
	char                msg[128];
	icfg_if_t           intf;
	icfg_handle_t       h;
	struct sockaddr    *sin_p;
	struct sockaddr_in  _sin;
	struct sockaddr_in6 _sin6;
	socklen_t           sin_len = sizeof (struct sockaddr_in);
	socklen_t           sin6_len = sizeof (struct sockaddr_in6);
	socklen_t           addrlen;
	int prefixlen = 0;

    if ( family == AF_INET6 ) {
        sin_p = (struct sockaddr *)&_sin6;
        addrlen = sin6_len;
    }
    else if ( family == AF_INET ) {
        sin_p = (struct sockaddr *)&_sin;
        addrlen = sin_len;
    }

	(void) strlcpy(intf.if_name, ifname, sizeof (intf.if_name));
	intf.if_protocol = family;
	if (icfg_open(&h, &intf) != ICFG_SUCCESS) {
		g_debug( "icfg_open failed on interface %s", ifname);
		return( FALSE );
	}
	if (icfg_get_addr(h, sin_p, &addrlen, &prefixlen,
	    B_TRUE) != ICFG_SUCCESS) {
		g_debug( "icfg_get_addr failed on interface %s for family %s", ifname, (family == AF_INET6)?"v6":"v4");
		icfg_close(h);
		return( FALSE );
	}
	icfg_close(h);

    return( TRUE );
}

static gchar*
get_interface_address_str( NwamuiNcu *ncu, sa_family_t family)
{
	gchar              *string = NULL;
	gchar              *address = NULL;
	int                 prefixlen = 0;
    gboolean            is_dhcp = FALSE;
    gchar*              dhcp_str;
    gboolean            ipv4_has_dhcp;
    gboolean            ipv6_has_dhcp;
    gboolean            ipv6_autoconf;

    if ( nwamui_util_get_interface_address( ncu->prv->device_name, family, &address, &prefixlen, &is_dhcp ) ) {
        /* It is possible for the DHCPRUNNING flag to be true, yet DHCP is not
         * the source of the address.
         *
         * This is because NWAM can use DHCP to gather information like the
         * nameservice to use using DHCP, thus setting the flag.
         *
         * So we double check using the stored nwam configuration, so get that
         * info now.
         */

        if ( is_dhcp ) {
            nwamui_ncu_has_dhcp_configured( ncu,  &ipv4_has_dhcp,  &ipv6_has_dhcp,  NULL );

            if ( family == AF_INET && !ipv4_has_dhcp ) {
                is_dhcp = FALSE; /* DHCP not being used to address source */
            }
            if ( family == AF_INET6 && !ipv6_has_dhcp ) {
                is_dhcp = FALSE; /* DHCP not being used to address source */
            }
        }

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
    N_("Unknown"),
    N_("Disabled"),
    N_("Not Connected"),
    N_("Scanning for Networks"),
    N_("Needs Network Selection"),
    N_("Wireless Network %s Needs a Key"),  /* %s = ESSID */
    N_("Waiting for Address"),
    N_("DHCP Timed Out"),
    N_("Duplicate Address Detected"),
    N_("Connecting"),
    N_("Connected"),
    N_("Connecting to %s"),     /* %s = ESSID */
    N_("Connected to %s"),      /* %s = ESSID */
    N_("Network Unavailable"),
    N_("Cable Unplugged")
};

extern nwamui_connection_state_t
nwamui_ncu_update_state(NwamuiNcu* self) 
{
    NwamuiNcuPrivate          *prv             = NWAMUI_NCU_GET_PRIVATE(self);
    nwam_state_t               link_state      = prv->link_state;
    nwam_aux_state_t           link_aux_state  = prv->link_aux_state;
    nwam_state_t               iface_state     = prv->iface_state;
    nwam_aux_state_t           iface_aux_state = prv->iface_aux_state;
    nwamui_connection_state_t  new_state       = prv->state;

    /* Use cached state */
    /* link_state = nwamui_ncu_get_link_nwam_state(self, &link_aux_state, NULL); */
    /* g_debug("Get  link state %-3X: %-10s - %s", link_state, nwam_state_to_string(link_state), nwam_aux_state_to_string(link_aux_state)); */

    /* iface_state = nwamui_object_get_nwam_state(NWAMUI_OBJECT(self), &iface_aux_state, NULL); */
    /* g_debug("Get iface state %-3X: %-10s - %s", iface_state, nwam_state_to_string(iface_state), nwam_aux_state_to_string(iface_aux_state)); */

    switch ( iface_state ) { 
    case NWAM_STATE_UNINITIALIZED:
        new_state = NWAMUI_STATE_UNKNOWN;
        break;
    case NWAM_STATE_MAINTENANCE:
    case NWAM_STATE_DEGRADED:
    case NWAM_STATE_INITIALIZED:
        new_state = NWAMUI_STATE_NETWORK_UNAVAILABLE;
        break;
    case NWAM_STATE_ONLINE_TO_OFFLINE:
        break;
    case NWAM_STATE_DISABLED:
    case NWAM_STATE_OFFLINE:
        if ( prv->ncu_type == NWAMUI_NCU_TYPE_WIRED) {
            if (iface_aux_state == NWAM_AUX_STATE_DOWN || 
              iface_aux_state == NWAM_AUX_STATE_CONDITIONS_NOT_MET) {
                /* iface state_offline and conditions_not_met also happens when
                 * nwamd try up an inf, e.g. try different NCP groups. So only
                 * change state when it change from active.
                 */
                if (new_state == NWAMUI_STATE_CONNECTED) {
                    new_state = NWAMUI_STATE_CABLE_UNPLUGGED;
                } else {
                    /* new_state = NWAMUI_STATE_NOT_CONNECTED; */
                    new_state = NWAMUI_STATE_NETWORK_UNAVAILABLE;
                }
            }
            break;
        } else if ( prv->ncu_type == NWAMUI_NCU_TYPE_WIRELESS ) {
            if ( link_aux_state == NWAM_AUX_STATE_LINK_WIFI_SCANNING ) {
                new_state = NWAMUI_STATE_SCANNING;
                break;
            }
            else if ( link_aux_state == NWAM_AUX_STATE_LINK_WIFI_NEED_SELECTION ) {
                new_state = NWAMUI_STATE_NEEDS_SELECTION;
                break;
            }
            else if ( link_aux_state == NWAM_AUX_STATE_LINK_WIFI_NEED_KEY ) {
                new_state = NWAMUI_STATE_NEEDS_KEY_ESSID;
                break;
            }
            else if ( link_aux_state == NWAM_AUX_STATE_LINK_WIFI_CONNECTING ) {
                new_state = NWAMUI_STATE_CONNECTING_ESSID;
                break;
            } else {
                new_state = NWAMUI_STATE_NETWORK_UNAVAILABLE;
            }
        } else {
            /* new_state = NWAMUI_STATE_NOT_CONNECTED; */
        }
        break;
    case NWAM_STATE_OFFLINE_TO_ONLINE:
        if ( prv->ncu_type == NWAMUI_NCU_TYPE_WIRELESS ) {
            if ( link_aux_state == NWAM_AUX_STATE_LINK_WIFI_SCANNING ) {
                new_state = NWAMUI_STATE_SCANNING;
                break;
            }
            else if ( link_aux_state == NWAM_AUX_STATE_LINK_WIFI_NEED_SELECTION ) {
                new_state = NWAMUI_STATE_NEEDS_SELECTION;
                break;
            }
            else if ( link_aux_state == NWAM_AUX_STATE_LINK_WIFI_NEED_KEY ) {
                new_state = NWAMUI_STATE_NEEDS_KEY_ESSID;
                break;
            }
            else if ( link_aux_state == NWAM_AUX_STATE_LINK_WIFI_CONNECTING ) {
                new_state = NWAMUI_STATE_CONNECTING_ESSID;
                break;
            }
        }
        if ( iface_aux_state == NWAM_AUX_STATE_IF_WAITING_FOR_ADDR ) {
            new_state = NWAMUI_STATE_WAITING_FOR_ADDRESS;
            break;
        }
        else if ( iface_aux_state == NWAM_AUX_STATE_IF_DHCP_TIMED_OUT ) {
            new_state = NWAMUI_STATE_DHCP_TIMED_OUT;
            break;
        }
        else if ( iface_aux_state == NWAM_AUX_STATE_IF_DUPLICATE_ADDR ) {
            new_state = NWAMUI_STATE_DHCP_DUPLICATE_ADDR;
            break;
        }
        new_state = NWAMUI_STATE_CONNECTING;
        break;
    case NWAM_STATE_ONLINE:
        if ( iface_aux_state == NWAM_AUX_STATE_UP ) {
            if ( prv->ncu_type == NWAMUI_NCU_TYPE_WIRELESS ) {
                new_state = NWAMUI_STATE_CONNECTED_ESSID;
                break;
            }
            new_state = NWAMUI_STATE_CONNECTED;
            break;
        }
        break;
    default:
        break;
    }

    if (prv->state != new_state) {
        prv->state = new_state;
        {
            gchar *str;
            g_debug("NCU %s currently STATE STRING: %s",
              prv->vanity_name,
              (str = nwamui_ncu_get_connection_state_string(self)));
            g_free(str);
        }

        g_object_notify(G_OBJECT(self), "active" );
    }

    return new_state;
}

extern nwamui_connection_state_t
nwamui_ncu_get_connection_state( NwamuiNcu* self ) 
{
    NwamuiNcuPrivate          *prv   = NWAMUI_NCU_GET_PRIVATE(self);

    if (prv->state == NWAMUI_STATE_UNKNOWN) {
        /* Initilaly update state */
        nwamui_object_get_nwam_state(NWAMUI_OBJECT(self), NULL, NULL);
        nwamui_ncu_get_link_nwam_state(self, NULL, NULL);
        prv->state = nwamui_ncu_update_state(self);
    }

    return prv->state;
}

/*
 * Get a string that describes the status of the system, should be freed by
 * caller using gfree().
 */
extern gchar*
nwamui_ncu_get_connection_state_string( NwamuiNcu* self )
{
    NwamuiNcuPrivate          *prv           = NWAMUI_NCU_GET_PRIVATE(self);
    nwamui_connection_state_t  state         = NWAMUI_STATE_UNKNOWN;
    gchar*                     status_string = NULL;
    gchar*                     essid         = NULL;

    g_return_val_if_fail( NWAMUI_IS_NCU( self ), NULL );

    /* state = nwamui_ncu_get_connection_state( self ); */
    state = prv->state;

    switch( state ) {
        case NWAMUI_STATE_UNKNOWN:
        case NWAMUI_STATE_NOT_CONNECTED:
        case NWAMUI_STATE_SCANNING:
        case NWAMUI_STATE_NEEDS_SELECTION:
        case NWAMUI_STATE_WAITING_FOR_ADDRESS:
        case NWAMUI_STATE_DHCP_TIMED_OUT:
        case NWAMUI_STATE_DHCP_DUPLICATE_ADDR:
        case NWAMUI_STATE_CONNECTING:
        case NWAMUI_STATE_CONNECTED:
        case NWAMUI_STATE_NETWORK_UNAVAILABLE:
        case NWAMUI_STATE_CABLE_UNPLUGGED:
            status_string = g_strdup( _(status_string_fmt[state]) );
            break;

        case NWAMUI_STATE_NEEDS_KEY_ESSID:
        case NWAMUI_STATE_CONNECTING_ESSID:
        case NWAMUI_STATE_CONNECTED_ESSID: {
                dladm_handle_t              handle;
                datalink_id_t               linkid;
                dladm_wlan_linkattr_t	    attr;

                if ( dladm_open( &handle ) == DLADM_STATUS_OK ) {
                    if ( dladm_name2info( handle, self->prv->device_name,
                        &linkid, NULL, NULL, NULL ) != DLADM_STATUS_OK ) {
                        g_warning("Unable to map device to linkid");
                    }
                    else if ( dladm_wlan_get_linkattr(handle, linkid, &attr) != DLADM_STATUS_OK ) {
                        g_warning("cannot get link attributes for %s", self->prv->device_name );
                    }
                    else if ( attr.la_status == DLADM_WLAN_LINK_CONNECTED ) {
                        if ( (attr.la_valid | DLADM_WLAN_LINKATTR_WLAN) &&
                          (attr.la_wlan_attr.wa_valid & DLADM_WLAN_ATTR_ESSID) ) {
                            char cur_essid[DLADM_STRSIZE];
                            dladm_wlan_essid2str( &attr.la_wlan_attr.wa_essid, cur_essid );
                            essid =  g_strdup( cur_essid );
                        }
                    }
                    dladm_close(handle);
                } else {
                    g_warning("Error creating dladm handle" );
                }

                if ( essid != NULL && self->prv->wifi_info == NULL ) {
                    /* Seems we don't have a valid wifi_info despite being
                     * connected, try rectify this now.
                     */
                    NwamuiWifiNet* wifi = nwamui_ncu_wifi_hash_lookup_by_essid( self, essid );

                    if ( wifi != NULL ) {
                        nwamui_ncu_set_wifi_info( self, wifi );
                    }
                }
                else if ( essid == NULL && self->prv->wifi_info != NULL ) {
                    /* We can't get an essid from the system, but we do have
                     * one in wifi_info, so use it. 
                     * This can happen if we're in need of a key.
                     */
                    essid = g_strdup(nwamui_object_get_name(NWAMUI_OBJECT(self->prv->wifi_info)));
                }
                status_string = g_strdup_printf( _(status_string_fmt[state]), essid);
                g_free(essid);
            }
            break;

        default:
            g_error("Unexpected value for connection state %d", (int)state );
            break;
    }

    return( status_string );
}

/*
 * Get a string that provides most information about the current state of the
 * ncu, should be freed by caller using gfree().
 */
extern gchar*
nwamui_ncu_get_connection_state_detail_string( NwamuiNcu* self, gboolean use_newline )
{
    nwamui_connection_state_t   state = NWAMUI_STATE_NOT_CONNECTED;
    gchar*                      status_string = NULL;
    gchar*                      addr_part = NULL;
    gchar*                      signal_part = NULL;
    gchar*                      speed_part = NULL;

    g_return_val_if_fail( NWAMUI_IS_NCU( self ), NULL );

    /* state = nwamui_ncu_update_connection_state( self ); */
    state = self->prv->state;

    switch( state ) {
        case NWAMUI_STATE_CONNECTED:
        case NWAMUI_STATE_CONNECTED_ESSID: {
            gchar          *v4addr = get_interface_address_str( self, AF_INET );
            gchar          *v6addr = get_interface_address_str( self, AF_INET6 );
            guint           speed;
            const gchar    *sepr;

            if ( use_newline ) {
                sepr = "\n";
            }
            else {
                sepr = ", ";
            }

            if ( v4addr != NULL ) {
                addr_part = v4addr;
                v4addr = NULL;
            }
            
            if ( v6addr != NULL ) {
                if ( addr_part != NULL ) {
                    gchar* tmp = addr_part;
                    addr_part = g_strdup_printf( _("%s%s%s"), addr_part, sepr, v6addr );
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

            speed = nwamui_ncu_get_speed( self );

            if ( speed > 1000 ) {
                speed_part = g_strdup_printf( _("Speed: %d Gb/s"), speed / 1000 );
            }
            else {
                speed_part = g_strdup_printf( _("Speed: %d Mb/s"), speed );
            }

            if ( signal_part == NULL ) {
                status_string = g_strdup_printf( _("%s%s%s"), addr_part, sepr, speed_part );
            }
            else {
                status_string = g_strdup_printf( _("%s%s%s%s%s"), addr_part, sepr, signal_part, sepr, speed_part );
                g_free(signal_part);
            }
            g_free(speed_part);
            g_free(addr_part);
            g_free(v4addr);
            g_free(v6addr);
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
        kstat_close (kc);
        return( TRUE );
    }

    kstat_close (kc);

    return( FALSE );
}

extern gchar*
nwamui_ncu_get_configuration_summary_string( NwamuiNcu* self )
{
    GString        *status_string = NULL;
    GString        *v4addr_part = NULL;
    GString        *v6addr_part = NULL;

    g_return_val_if_fail( NWAMUI_IS_NCU( self ), NULL );

    if ( self->prv->ipv4_active ) {
        GList  *ipv4_list = NULL;
        guint   count;

        ipv4_list = convert_gtk_list_store_to_g_list( self->prv->v4addresses );

        count = g_list_length( ipv4_list );

        v4addr_part = g_string_new( "" );

        if ( ipv4_list != NULL && ipv4_list->data != NULL ) { 
            NwamuiIp   *ip = NWAMUI_IP(ipv4_list->data);

            if ( count > 1 ) {
                (void)g_string_append(v4addr_part, _("Multiple IP configured"));
            }
            else {
                gchar    *addr;
                gchar    *subnet;

                /* Static */
                addr = nwamui_ip_get_address( ip );
                subnet = nwamui_ip_get_subnet_prefix(ip); 

                if ( addr == NULL || strcmp( addr, "0.0.0.0" ) == 0 ) {
                   (void)g_string_append( v4addr_part, _("No IP Address") );
                }
                else {
                    gchar *str = nwamui_util_join_address_prefix( FALSE, addr, subnet );

                   (void)g_string_append( v4addr_part, str );

                   g_free(str);
                }
                g_free(addr);
                g_free(subnet);
            }
        }
        if ( self->prv->ipv4_has_dhcp ) {
            if ( v4addr_part->len > 0 ) {
                (void)g_string_append(v4addr_part, _("(DHCP)"));
            }
            else {
                (void)g_string_append(v4addr_part, _("DHCP Assigned") );
            }
        }
        nwamui_util_free_obj_list( ipv4_list );
    }

    if ( self->prv->ipv6_active ) {
        GList  *ipv6_list = NULL;
        guint   count;

        ipv6_list = convert_gtk_list_store_to_g_list( self->prv->v6addresses );

        count = g_list_length( ipv6_list );

        v6addr_part = g_string_new("");

        if ( ipv6_list != NULL && ipv6_list->data != NULL ) { 
            NwamuiIp   *ip = NWAMUI_IP(ipv6_list->data);

            if ( count > 1 ) {
                v6addr_part = g_string_new( _("Multiple IP configured"));
            }
            else {
                gchar    *addr;
                gchar    *subnet;

                /* Static */

                addr = nwamui_ip_get_address( ip );
                subnet = nwamui_ip_get_subnet_prefix(ip); 

                if ( addr == NULL ) {
                   (void)g_string_append( v6addr_part, _("No IP Address") );
                }
                else {
                    gchar *str = nwamui_util_join_address_prefix( TRUE, addr, subnet );

                    (void)g_string_append( v6addr_part, str );

                    g_free(str);
                }
                g_free(addr);
                g_free(subnet);
            }
        }

        nwamui_util_free_obj_list( ipv6_list );
        if ( self->prv->ipv6_has_dhcp ) {
            if ( v6addr_part->len > 0 ) {
                (void)g_string_append(v6addr_part, _(", "));
            }
            (void)g_string_append(v6addr_part, _("DHCP Assigned") );
        }
        if ( self->prv->ipv6_has_auto_conf ) {
            if ( v6addr_part->len > 0 ) {
                (void)g_string_append(v6addr_part, _(", "));
            }
            (void)g_string_append(v6addr_part, _("Autoconf") );
        }
    }

    status_string = g_string_new("");

    if ( v4addr_part != NULL && v6addr_part != NULL ) {
        /* Have both, so make appropriate string */

        g_string_append_printf(status_string, _("Addresses: (v4) %s, (v6) %s"), v4addr_part->str, v6addr_part->str );
    }
    else if ( v4addr_part != NULL) {
        g_string_append_printf(status_string, _("Address: %s"), v4addr_part->str );
    }
    else if ( v6addr_part != NULL) {
        g_string_append_printf(status_string, _("Address(v6): %s"), v6addr_part->str );
    }
    else {
        (void)g_string_append( status_string, _("No addresses configured"));
    }

    if ( v4addr_part != NULL ) {
        g_string_free( v4addr_part, TRUE );
    }
    if ( v6addr_part != NULL ) {
        g_string_free( v6addr_part, TRUE );
    }

    return( g_string_free(status_string, FALSE) );
}

/* Callbacks */

static void 
ip_row_inserted_or_changed_cb (GtkTreeModel *tree_model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
    NwamuiNcu* self = NWAMUI_NCU(data);
    gboolean is_v6 = (tree_model == GTK_TREE_MODEL(self->prv->v6addresses));

    set_modified_flag( self, NWAM_NCU_CLASS_IP, TRUE );

    /* Chain events in the tree to the NCU object*/
    g_object_notify(G_OBJECT(self), is_v6?"v6addresses":"v4addresses");
}

static void 
ip_row_deleted_cb (GtkTreeModel *tree_model, GtkTreePath *path, gpointer data)
{
    NwamuiNcu* self = NWAMUI_NCU(data);
    gboolean is_v6 = (tree_model == GTK_TREE_MODEL(self->prv->v6addresses));

    set_modified_flag( self, NWAM_NCU_CLASS_IP, TRUE );

    /* Chain events in the tree to the NCU object*/
    g_object_notify(G_OBJECT(self), is_v6?"v6addresses":"v4addresses");
}

static void
wireless_notify_cb(GObject *gobject, GParamSpec *arg1, gpointer user_data)
{
    NwamuiWifiNet*  wifi = NWAMUI_WIFI_NET(gobject);
    NwamuiNcu*      self = NWAMUI_NCU(user_data);

    /* Chain events in the Wifi Net to the NCU */
    g_object_notify(G_OBJECT(self), "wifi_info");
}



