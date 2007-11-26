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
 * File:   nwamui_ncp.c
 *
 */

#include <glib-object.h>
#include <glib/gi18n.h>
#include <strings.h>
#include <gtk/gtkliststore.h>

#include "libnwamui.h"

static GObjectClass    *parent_class    = NULL;
static NwamuiNcp       *instance        = NULL;


struct _NwamuiNcpPrivate {
        gchar*          ncp_name;
        GtkListStore*   ncu_list_store;
};

enum {
    PROP_NAME = 1,
    PROP_ncu_list_store
};


static void nwamui_ncp_set_property ( GObject         *object,
                                      guint            prop_id,
                                      const GValue    *value,
                                      GParamSpec      *pspec);

static void nwamui_ncp_get_property ( GObject         *object,
                                      guint            prop_id,
                                      GValue          *value,
                                      GParamSpec      *pspec);

static void nwamui_ncp_finalize (     NwamuiNcp *self);


/* Callbacks */
static void object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);
static void row_deleted_cb (GtkTreeModel *tree_model, GtkTreePath *path, gpointer user_data);
static void row_inserted_cb (GtkTreeModel *tree_model, GtkTreePath *path, GtkTreeIter *iter, gpointer user_data);
static void rows_reordered_cb(GtkTreeModel *tree_model, GtkTreePath *path, GtkTreeIter *iter, gpointer arg3, gpointer user_data);

G_DEFINE_TYPE (NwamuiNcp, nwamui_ncp, G_TYPE_OBJECT)

static void
nwamui_ncp_class_init (NwamuiNcpClass *klass)
{
    /* Pointer to GObject Part of Class */
    GObjectClass *gobject_class = (GObjectClass*) klass;
        
    /* Initialise Static Parent Class pointer */
    parent_class = g_type_class_peek_parent (klass);

    /* Override Some Function Pointers */
    gobject_class->set_property = nwamui_ncp_set_property;
    gobject_class->get_property = nwamui_ncp_get_property;
    gobject_class->finalize = (void (*)(GObject*)) nwamui_ncp_finalize;

    /* Create some properties */
    g_object_class_install_property (gobject_class,
                                     PROP_NAME,
                                     g_param_spec_string ("name",
                                                          _("Name of the NCP"),
                                                          _("Name of the NCP"),
                                                          NULL,
                                                          G_PARAM_READABLE));

    g_object_class_install_property (gobject_class,
                                     PROP_ncu_list_store,
                                     g_param_spec_object ("ncu_list_store",
                                                          _("List of NCUs in the NCP"),
                                                          _("List of NCUs in the NCP"),
                                                          GTK_TYPE_LIST_STORE,
                                                          G_PARAM_READABLE));

    
}

/* TODO - Remove when using NWAM proper to get NCUs */
struct _ncu_info {
    gchar*              vanity_name;
    gchar*              device_name;
    nwamui_ncu_type_t   type;
    gboolean            enabled;
    guint               speed;
    gboolean            ipv4_auto_conf;
    gchar*              ipv4_address;
    gchar*              ipv4_subnet;
    gchar*              ipv4_gateway;
    gboolean            ipv6_active;
    gboolean            ipv6_auto_conf;
    gchar*              ipv6_address;
    gchar*              ipv6_prefix;
    gchar*              essid;
};

typedef struct _ncu_info    ncu_info_t;

static void
nwamui_ncp_init ( NwamuiNcp *self)
{
    static ncu_info_t   tmp_ncu_list[] = {
        { "MyWireless", "ath0", NWAMUI_NCU_TYPE_WIRELESS, TRUE, 54, FALSE, "192.168.1.20", "255.255.255.0", "192.168.1.1", TRUE, TRUE, "2001:0db8:0:0:0:0:1428:57ab", "2001:0db8:1234::/48", "MyESSID" },
        { "MyWired1", "bge0", NWAMUI_NCU_TYPE_WIRED, FALSE, 100, TRUE, NULL, NULL, NULL, FALSE, TRUE, NULL, NULL, NULL },
        { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL }
    };
    
    self->prv = g_new0 (NwamuiNcpPrivate, 1);
    
    /* TODO - See if there's a better name for the NCP */
    self->prv->ncp_name = g_strdup("CurrentNCP");
    
    self->prv->ncu_list_store = gtk_list_store_new ( 1, NWAMUI_TYPE_NCU);
    
    /* TODO - Generate proper list of NCUs */
    for( ncu_info_t *ptr = tmp_ncu_list; ptr != NULL && ptr->vanity_name != NULL; ptr++) {
        NwamuiNcu       *ncu;
        NwamuiWifiNet   *wifi_info = NULL;
        GtkTreeIter      iter;
        
        if ( ptr->essid != NULL ) {
            wifi_info = nwamui_wifi_net_new( ptr->essid, NWAMUI_WIFI_SEC_NONE, "aa:bb:cc:dd:ee", "g", 54, NWAMUI_WIFI_STRENGTH_GOOD, NWAMUI_WIFI_WPA_CONFIG_AUTOMATIC);
        }
        ncu = nwamui_ncu_new(   ptr->vanity_name,
                                ptr->device_name,
                                ptr->type,
                                ptr->enabled,
                                ptr->speed,
                                ptr->ipv4_auto_conf,
                                ptr->ipv4_address,
                                ptr->ipv4_subnet,
                                ptr->ipv4_gateway,
                                ptr->ipv6_active,
                                ptr->ipv6_auto_conf,
                                ptr->ipv6_address,
                                ptr->ipv6_prefix,
                                wifi_info );
        
        if ( wifi_info != NULL ) {
            g_object_unref( G_OBJECT( wifi_info ) );
        }
        
        gtk_list_store_append( self->prv->ncu_list_store, &iter );
        gtk_list_store_set(self->prv->ncu_list_store, &iter, 0, ncu, -1 );
    }
    
    g_signal_connect(G_OBJECT(self), "notify", (GCallback)object_notify_cb, (gpointer)self);
    g_signal_connect(self->prv->ncu_list_store, "row_deleted", G_CALLBACK(row_deleted_cb), NULL );
    g_signal_connect(self->prv->ncu_list_store, "row_inserted", G_CALLBACK(row_inserted_cb), NULL );
    g_signal_connect(self->prv->ncu_list_store, "rows_reordered", G_CALLBACK(rows_reordered_cb), NULL );

}

static void
nwamui_ncp_set_property (   GObject         *object,
                            guint            prop_id,
                            const GValue    *value,
                            GParamSpec      *pspec)
{
    NwamuiNcp *self = NWAMUI_NCP(object);
    gchar*      tmpstr = NULL;
    gint        tmpint = 0;

    switch (prop_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
nwamui_ncp_get_property (   GObject         *object,
                            guint            prop_id,
                            GValue          *value,
                            GParamSpec      *pspec)
{
    NwamuiNcp *self = NWAMUI_NCP(object);

    switch (prop_id) {
        case PROP_NAME: {
                g_value_set_string( value, self->prv->ncp_name );
            }
            break;
        case PROP_ncu_list_store: {
                g_value_set_object( value, self->prv->ncu_list_store );
            }
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}


/**
 * nwamui_ncp_get_instance:
 * @returns: a #NwamuiNcp.
 *
 * Get a reference to the <b>singleton</b> #NwamuiNcp. When finished using the object,
 * you should g_object_unref() it.
 *
 *  NOTE: For Phase 2 this will be obsoleted since there will be multiple NCPs in NWAM.
 *
 **/
extern NwamuiNcp*
nwamui_ncp_get_instance (void)
{
    if ( instance == NULL ) {
	instance = NWAMUI_NCP(g_object_new (NWAMUI_TYPE_NCP, NULL));
        g_object_ref(G_OBJECT(instance)); /* Ensure we always hold at least one reference */
    }
    g_object_ref(G_OBJECT(instance)); /* update reference pointer. */
    return( instance );
}


/**
 * nwamui_ncp_get_name:
 * @returns: null-terminated C String with name of the the NCP.
 *
 **/
extern gchar*
nwamui_ncp_get_name ( NwamuiNcp *self )
{
    gchar*  name = NULL;
    
    g_return_val_if_fail (NWAMUI_IS_NCP(self), name); 
    
    g_object_get (G_OBJECT (self),
                  "name", &name,
                  NULL);

    return( name );
}

/**
 * nwamui_ncp_get_ncu_list_store_store:
 * @returns: GList containing NwamuiNcu elements
 *
 **/
extern GtkListStore*
nwamui_ncp_get_ncu_list_store( NwamuiNcp *self )
{
    GtkListStore* ncu_list_store = NULL;
    
    g_return_val_if_fail (NWAMUI_IS_NCP(self), ncu_list_store); 
    
    g_object_get (G_OBJECT (self),
                  "ncu_list_store", &ncu_list_store,
                  NULL);

    return( ncu_list_store );
}


/**
 * nwamui_ncp_foreach_ncu
 * 
 * Calls func for each NCU in the NCP
 *
 **/
extern void
nwamui_ncp_foreach_ncu( NwamuiNcp *self, GtkTreeModelForeachFunc func, gpointer user_data )
{
    g_return_if_fail (NWAMUI_IS_NCP(self) && self->prv->ncu_list_store != NULL); 
    
    gtk_tree_model_foreach( GTK_TREE_MODEL(self->prv->ncu_list_store), func, user_data );
}

static void
nwamui_ncp_finalize (NwamuiNcp *self)
{

    g_free( self->prv->ncp_name);
    
    if ( self->prv->ncu_list_store != NULL ) {
        gtk_list_store_clear(self->prv->ncu_list_store);
        g_object_unref(self->prv->ncu_list_store);
    }
    
    g_free (self->prv); 
    self->prv = NULL;

    parent_class->finalize (G_OBJECT (self));
}

/* Callbacks */

static void
object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
    NwamuiNcp* self = NWAMUI_NCP(data);

    g_debug("NwamuiNcp: notify %s changed\n", arg1->name);
}

static void
row_deleted_cb (GtkTreeModel *tree_model, GtkTreePath *path, gpointer user_data) 
{
    g_debug("NwamuiNcp: NCU List: Row Deleted: %s", gtk_tree_path_to_string(path));
}

static void
row_inserted_cb (GtkTreeModel *tree_model, GtkTreePath *path, GtkTreeIter *iter, gpointer user_data)
{
    g_debug("NwamuiNcp: NCU List: Row Inserted: %s", gtk_tree_path_to_string(path));
}

static void
rows_reordered_cb(GtkTreeModel *tree_model, GtkTreePath *path, GtkTreeIter *iter, gpointer arg3, gpointer user_data)   
{
    gchar*  path_str = gtk_tree_path_to_string(path);
    g_debug("NwamuiNcp: NCU List: Rows Reordered: %s", path_str?path_str:"NULL");
}

