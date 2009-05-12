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
 * File:   nwamui_ncp.c
 *
 */
#include <stdlib.h>

#include <glib-object.h>
#include <glib/gi18n.h>
#include <strings.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtktreestore.h>

#include "libnwamui.h"

#include <errno.h>
#include <sys/dlpi.h>
#include <libdllink.h>

static GObjectClass    *parent_class    = NULL;
static NwamuiNcp       *instance        = NULL;

enum {
    PROP_NWAM_NCP = 1,
    PROP_NAME,
    PROP_ACTIVE,
    PROP_NCU_LIST,
    PROP_NCU_TREE_STORE,
    PROP_NCU_LIST_STORE,
    PROP_ACTIVE_NCU,
    PROP_SELECTION_MODE,
    PROP_WIRELESS_LINK_NUM
};

enum {
    S_ACTIVATE_NCU,
    S_DEACTIVATE_NCU,
    LAST_SIGNAL
};

static guint nwamui_ncp_signals[LAST_SIGNAL] = { 0, 0 };

struct _NwamuiNcpPrivate {
        nwam_ncp_handle_t           nwam_ncp;
        gchar*                      name;
        nwamui_ncp_selection_mode_t selection_mode;
        gint                        wireless_link_num;

        GList*                      ncu_list;
        GtkTreeStore*               ncu_tree_store;
        GtkListStore*               ncu_list_store;
        GList*                      temp_ncu_list;
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

/* Default signal handlers */
static void default_activate_ncu_signal_handler (NwamuiNcp *self, NwamuiNcu* ncu, gpointer user_data);
static void default_deactivate_ncu_signal_handler (NwamuiNcp *self, NwamuiNcu* ncu, gpointer user_data);

/* Callbacks */
static void object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);
static void ncu_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);
static void row_deleted_cb (GtkTreeModel *tree_model, GtkTreePath *path, gpointer user_data);
static void row_inserted_cb (GtkTreeModel *tree_model, GtkTreePath *path, GtkTreeIter *iter, gpointer user_data);
static void rows_reordered_cb(GtkTreeModel *tree_model, GtkTreePath *path, GtkTreeIter *iter, gpointer arg3, gpointer user_data);

G_DEFINE_TYPE (NwamuiNcp, nwamui_ncp, NWAMUI_TYPE_OBJECT)

static void
nwamui_ncp_class_init (NwamuiNcpClass *klass)
{
    /* Pointer to GObject Part of Class */
    GObjectClass *gobject_class = (GObjectClass*) klass;
    NwamuiObjectClass *nwamuiobject_class = NWAMUI_OBJECT_CLASS(klass);
    
    /* Initialise Static Parent Class pointer */
    parent_class = g_type_class_peek_parent (klass);

    /* Override Some Function Pointers */
    gobject_class->set_property = nwamui_ncp_set_property;
    gobject_class->get_property = nwamui_ncp_get_property;
    gobject_class->finalize = (void (*)(GObject*)) nwamui_ncp_finalize;

    klass->activate_ncu = default_activate_ncu_signal_handler;
    klass->deactivate_ncu = default_deactivate_ncu_signal_handler;

    nwamuiobject_class->get_name = (nwamui_object_get_name_func_t)nwamui_ncp_get_name;
    nwamuiobject_class->set_name = (nwamui_object_set_name_func_t)NULL;

    /* Create some properties */
    g_object_class_install_property (gobject_class,
                                     PROP_NWAM_NCP,
                                     g_param_spec_pointer ("nwam_ncp",
                                                           _("Nwam Ncp handle"),
                                                           _("Nwam Ncp handle"),
                                                           G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

    g_object_class_install_property (gobject_class,
                                     PROP_NAME,
                                     g_param_spec_string ("name",
                                                          _("Name of the NCP"),
                                                          _("Name of the NCP"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_ACTIVE,
                                     g_param_spec_boolean("active",
                                                         _("active"),
                                                         _("active"),
                                                          FALSE,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_NCU_LIST,
                                     g_param_spec_pointer ("ncu_list",
                                                          _("GList of NCUs in the NCP"),
                                                          _("GList of NCUs in the NCP"),
                                                           G_PARAM_READABLE));

    g_object_class_install_property (gobject_class,
                                     PROP_NCU_TREE_STORE,
                                     g_param_spec_object ("ncu_tree_store",
                                                          _("TreeStore of NCUs in the NCP"),
                                                          _("TreeStore of NCUs in the NCP"),
                                                          GTK_TYPE_TREE_STORE,
                                                          G_PARAM_READABLE));

    g_object_class_install_property (gobject_class,
                                     PROP_NCU_LIST_STORE,
                                     g_param_spec_object ("ncu_list_store",
                                                          _("ListStore of NCUs in the NCP"),
                                                          _("ListStore of NCUs in the NCP"),
                                                          GTK_TYPE_LIST_STORE,
                                                          G_PARAM_READABLE));


    g_object_class_install_property (gobject_class,
                                     PROP_SELECTION_MODE,
                                     g_param_spec_int   ("selection_mode",
                                                         _("selection_mode"),
                                                         _("selection_mode"),
                                                          NWAMUI_NCP_SELECTION_MODE_AUTOMATIC,
                                                          NWAMUI_NCP_SELECTION_MODE_LAST-1,
                                                          NWAMUI_NCP_SELECTION_MODE_AUTOMATIC,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_WIRELESS_LINK_NUM,
                                     g_param_spec_int("wireless_link_num",
                                                      _("wireless_link_num"),
                                                      _("wireless_link_num"),
                                                      0,
                                                      G_MAXINT,
                                                      0,
                                                      G_PARAM_READABLE));

    /* Create some signals */
    nwamui_ncp_signals[S_ACTIVATE_NCU] =   
      g_signal_new ("activate_ncu",
        G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
        G_STRUCT_OFFSET (NwamuiNcpClass, activate_ncu),
        NULL, NULL,
        g_cclosure_marshal_VOID__OBJECT, 
        G_TYPE_NONE,                  /* Return Type */
        1,                            /* Number of Args */
        G_TYPE_OBJECT);                  /* Types of Args */
    
    nwamui_ncp_signals[S_DEACTIVATE_NCU] =   
      g_signal_new ("deactivate_ncu",
        G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
        G_STRUCT_OFFSET (NwamuiNcpClass, deactivate_ncu),
        NULL, NULL,
        g_cclosure_marshal_VOID__OBJECT, 
        G_TYPE_NONE,                  /* Return Type */
        1,                            /* Number of Args */
        G_TYPE_OBJECT);                  /* Types of Args */
    
}


static void
nwamui_ncp_init ( NwamuiNcp *self)
{
    self->prv = g_new0 (NwamuiNcpPrivate, 1);
    
    self->prv->name = NULL;

    self->prv->ncu_list = NULL;
    self->prv->ncu_tree_store = NULL;
    self->prv->ncu_list_store = NULL;

    self->prv->selection_mode = NWAMUI_NCP_SELECTION_MODE_AUTOMATIC;
    self->prv->wireless_link_num = 0;

    g_signal_connect(G_OBJECT(self), "notify", (GCallback)object_notify_cb, (gpointer)self);
}

static void
nwamui_ncp_set_property (   GObject         *object,
                            guint            prop_id,
                            const GValue    *value,
                            GParamSpec      *pspec)
{
    NwamuiNcp*      self = NWAMUI_NCP(object);
    gchar*          tmpstr = NULL;
    gint            tmpint = 0;
    nwam_error_t    nerr;
    gboolean        read_only = FALSE;

    read_only = !nwamui_ncp_is_modifiable(self);

    if ( read_only ) {
        g_error("Attempting to modify read-only ncp %s", self->prv->name?self->prv->name:"NULL");
        return;
    }

    switch (prop_id) {
        case PROP_NAME: {
                if ( self->prv->name != NULL ) {
                    g_free( self->prv->name );
                }
                self->prv->name = g_strdup( g_value_get_string( value ) );
                /* we may rename here */
                if (self->prv->nwam_ncp == NULL) {
                    nerr = nwam_ncp_read (self->prv->name, 0, &self->prv->nwam_ncp);
                    if (nerr == NWAM_SUCCESS) {
                        g_debug ("nwamui_ncp_set_property found nwam_loc_handle %s", self->prv->name);
                    } else {
                        nerr = nwam_ncp_create (self->prv->name, 0, &self->prv->nwam_ncp);
                        g_assert (nerr == NWAM_SUCCESS);
                    }
                }
                /*
                 * Currently can't set name after creation, but just in case...
                 *
                nerr = nwam_ncp_set_name (self->prv->nwam_ncp, self->prv->name);
                if (nerr != NWAM_SUCCESS) {
                    g_debug ("nwam_ncp_set_name %s error: %s", self->prv->name, nwam_strerror (nerr));
                }
                */
            }
            break;
        case PROP_NWAM_NCP: {
                g_assert (self->prv->nwam_ncp == NULL);
                self->prv->nwam_ncp = g_value_get_pointer (value);
            }
            break;

        case PROP_ACTIVE: {
                /* Activate immediately */
                nwam_state_t    state = NWAM_STATE_OFFLINE;

                nwam_ncp_get_state( self->prv->nwam_ncp, &state );

                gboolean active = g_value_get_boolean( value );
                if ( state != NWAM_STATE_ONLINE && active ) {
                    nwam_error_t nerr;
                    if ( (nerr = nwam_ncp_enable (self->prv->nwam_ncp)) != NWAM_SUCCESS ) {
                        g_warning("Failed to enable ncpation due to error: %s", nwam_strerror(nerr));
                    }
                }
                else {
                    g_warning("Cannot disable an NCP, enable another one to do this");
                }
            }
            break;

        case PROP_SELECTION_MODE: {
                self->prv->selection_mode = g_value_get_int( value );
            }
            break;
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
                g_value_set_string( value, self->prv->name );
            }
            break;
        case PROP_ACTIVE: {
                gboolean active = FALSE;
                if ( self->prv->nwam_ncp ) {
                    nwam_state_t    state = NWAM_STATE_OFFLINE;

                    nwam_ncp_get_state( self->prv->nwam_ncp, &state );
                    if ( state == NWAM_STATE_ONLINE ) {
                        active = TRUE;
                    }
                }
                g_value_set_boolean( value, active );
            }
            break;
        case PROP_NCU_LIST: {
                g_value_set_pointer( value, nwamui_util_copy_obj_list( self->prv->ncu_list ) );
            }
            break;
        case PROP_NCU_TREE_STORE: {
                g_value_set_object( value, self->prv->ncu_tree_store );
            }
            break;
        case PROP_NCU_LIST_STORE: {
                g_value_set_object( value, self->prv->ncu_list_store );
            }
            break;
        case PROP_SELECTION_MODE: {
                g_value_set_int( value, self->prv->selection_mode );
            }
            break;
        case PROP_WIRELESS_LINK_NUM: {
                g_value_set_int( value, self->prv->wireless_link_num );
            }
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
nwamui_ncp_finalize (NwamuiNcp *self)
{

    g_free( self->prv->name);
    
    if ( self->prv->ncu_list != NULL ) {
        nwamui_util_free_obj_list(self->prv->ncu_list);
    }
    
    if ( self->prv->ncu_tree_store != NULL ) {
        gtk_tree_store_clear(self->prv->ncu_tree_store);
        g_object_unref(self->prv->ncu_tree_store);
    }
    
    if ( self->prv->ncu_list_store != NULL ) {
        gtk_list_store_clear(self->prv->ncu_list_store);
        g_object_unref(self->prv->ncu_list_store);
    }
    
    g_free (self->prv); 
    self->prv = NULL;

    parent_class->finalize (G_OBJECT (self));
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
nwamui_ncp_new_with_handle (nwam_ncp_handle_t ncp)
{
    NwamuiNcp*      self = NWAMUI_NCP(g_object_new (NWAMUI_TYPE_NCP,
                                   NULL));
    char*           name = NULL;
    nwam_error_t    nerr;
    nwam_ncp_handle_t nwam_ncp;
    
    if ( (nerr = nwam_ncp_get_name (ncp, &name)) != NWAM_SUCCESS ) {
        g_debug ("Failed to get name for ncp, error: %s", nwam_strerror (nerr));
    }

    if ( ( nerr = nwam_ncp_read (name, 0, &nwam_ncp) ) != NWAM_SUCCESS ) {
        g_debug ("Failed to create private handle for ncp, error: %s", nwam_strerror (nerr));
    }

    self->prv->nwam_ncp = nwam_ncp;

    self->prv->name = name;

    nwamui_ncp_populate_ncu_list( self, NULL );

    return( self );
}

extern nwam_ncp_handle_t
nwamui_ncp_get_nwam_handle( NwamuiNcp* self )
{
    return (self->prv->nwam_ncp);
}

/** 
 * nwamui_ncp_set_name:
 * @nwamui_ncp: a #NwamuiNcp.
 * @name: The name to set.
 *
 **/ 
extern void
nwamui_ncp_set_name (   NwamuiNcp *self,
                        const gchar*  name )
{
    g_return_if_fail (NWAMUI_IS_NCP (self));
    g_assert (name != NULL );

    if ( name != NULL ) {
        g_object_set (G_OBJECT (self),
                      "name", name,
                      NULL);
    }
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
 * nwamui_ncp_is_active:
 * @nwamui_ncp: a #NwamuiNcp.
 * @returns: TRUE if the ncp is online.
 *
 **/
extern gboolean
nwamui_ncp_is_active( NwamuiNcp* self )
{
    gboolean is_active = FALSE;

    g_return_val_if_fail( NWAMUI_IS_NCP(self), is_active );

    g_object_get (G_OBJECT (self),
                  "active", &is_active,
                  NULL);

    return( is_active );
}

/** 
 * nwamui_ncp_set_active:
 * @nwamui_ncp: a #NwamuiEnv.
 * @active: Immediately activates/deactivates the ncp.
 * 
 **/ 
extern void
nwamui_ncp_set_active (   NwamuiNcp *self,
                          gboolean        active )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));

    g_object_set (G_OBJECT (self),
                  "active", active,
                  NULL);
}

/**
 * nwamui_ncp_is_modifiable:
 * @nwamui_ncp: a #NwamuiNcp.
 * @returns: the modifiable.
 *
 **/
extern gboolean
nwamui_ncp_is_modifiable (NwamuiNcp *self)
{
    nwam_error_t    nerr;
    gboolean        modifiable = FALSE; 
    boolean_t       readonly;

    if (!NWAMUI_IS_NCP (self) && self->prv->nwam_ncp == NULL ) {
        return( modifiable );
    }

    /* function doesn't exist, but will be made available, comment out until
     * then */
    /*
    if ( (nerr = nwam_ncp_get_read_only( self->prv->nwam_ncp, &readonly )) == NWAM_SUCCESS ) {
        modifiable = readonly?FALSE:TRUE;
    }
    else {
        g_warning("Error getting ncp read-only status: %s", nwam_strerror( nerr ) );
    }
    */
    if ( self->prv->name && strcmp( self->prv->name, "Automatic" ) == 0 ) {
        modifiable = FALSE;
    }
    else {
        modifiable = TRUE;
    }

    return( modifiable );
}

/**
 * nwamui_ncp_get_ncu_tree_store_store:
 * @returns: GList containing NwamuiNcu elements
 *
 **/
extern GtkTreeStore*
nwamui_ncp_get_ncu_tree_store( NwamuiNcp *self )
{
    GtkTreeStore* ncu_tree_store = NULL;
    
    if ( self == NULL ) {
        return( NULL );
    }

    g_return_val_if_fail (NWAMUI_IS_NCP(self), ncu_tree_store); 
    
    g_object_get (G_OBJECT (self),
                  "ncu_tree_store", &ncu_tree_store,
                  NULL);

    return( ncu_tree_store );
}

/**
 * nwamui_ncp_get_ncu_list:
 * @returns: GList containing NwamuiNcu elements
 *
 **/
extern GList*
nwamui_ncp_get_ncu_list( NwamuiNcp *self )
{
    GList*  ncu_list = NULL;

    if ( self == NULL ) {
        return( NULL );
    }

    g_return_val_if_fail (NWAMUI_IS_NCP(self), ncu_list); 
    
    g_object_get (G_OBJECT (self),
                  "ncu_list", &ncu_list,
                  NULL);

    return( ncu_list );
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
    
    if ( self == NULL ) {
        return( NULL );
    }

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
    g_return_if_fail (NWAMUI_IS_NCP(self) && self->prv->ncu_tree_store != NULL); 
    
    gtk_tree_model_foreach( GTK_TREE_MODEL(self->prv->ncu_tree_store), func, user_data );
}

/**
 * nwamui_ncp_get_ncu_by_device_name
 * 
 * Returns a pointer to an NCU given the device name.
 * Unref is needed
 *
 **/
extern  NwamuiNcu*
nwamui_ncp_get_ncu_by_device_name( NwamuiNcp *self, const gchar* device_name )
{
    NwamuiNcu      *ret_ncu = NULL;
    gchar          *ncu_device_name = NULL;

    g_return_val_if_fail (NWAMUI_IS_NCP(self) && self->prv->ncu_tree_store != NULL, ret_ncu ); 

    if ( device_name == NULL ) {
        return( NULL );
    }

    for (GList *elem = g_list_first(self->prv->ncu_list);
         elem != NULL;
         elem = g_list_next(elem) ) {
        if ( elem->data != NULL && NWAMUI_IS_NCU(elem->data) ) {
            NwamuiNcu* ncu = NWAMUI_NCU(elem->data);
            gchar *ncu_device_name = nwamui_ncu_get_device_name( ncu );

            if ( ncu_device_name != NULL 
                 && g_ascii_strcasecmp( ncu_device_name, device_name ) == 0 ) {
                /* Set ret_ncu, which will cause for loop to exit */
                g_debug("compare ncu: %s to target %s : SAME", ncu_device_name, device_name);
                ret_ncu = NWAMUI_NCU(g_object_ref(ncu));
            } else {
                g_debug("compare ncu: %s to target %s : DIFFERENT", ncu_device_name, device_name);
            }
            g_free(ncu_device_name);
        }
    }

    return( ret_ncu );
}

extern void 
nwamui_ncp_remove_ncu_by_device_name( NwamuiNcp* self, const gchar* device_name ) 
{
    GtkTreeIter     iter;
    gchar          *ncu_device_name = NULL;
    gboolean        valid_iter = FALSE;
    NwamuiNcu      *found_ncu = NULL;

    g_return_if_fail (NWAMUI_IS_NCP(self) && self->prv->ncu_tree_store != NULL); 
    g_return_if_fail (device_name != NULL && strlen(device_name) > 0 );

    found_ncu = nwamui_ncp_get_ncu_by_device_name( self, device_name );

    if ( found_ncu != NULL ) {
        nwamui_ncp_remove_ncu( self, found_ncu );
    }
}

extern void 
nwamui_ncp_remove_ncu( NwamuiNcp* self, NwamuiNcu* ncu ) 
{
    GtkTreeIter     iter;
    gboolean        valid_iter = FALSE;

    g_return_if_fail (NWAMUI_IS_NCP(self) && NWAMUI_IS_NCU(ncu) );


    g_object_freeze_notify(G_OBJECT(self->prv->ncu_tree_store));
    g_object_freeze_notify(G_OBJECT(self->prv->ncu_list_store));

    for (valid_iter = gtk_tree_model_get_iter_first( GTK_TREE_MODEL(self->prv->ncu_tree_store), &iter);
         valid_iter;
         valid_iter = gtk_tree_model_iter_next( GTK_TREE_MODEL(self->prv->ncu_tree_store), &iter)) {
        NwamuiNcu      *_ncu;

        gtk_tree_model_get( GTK_TREE_MODEL(self->prv->ncu_tree_store), &iter, 0, &_ncu, -1);

        if ( _ncu == ncu ) {
            gtk_tree_store_remove(GTK_TREE_STORE(self->prv->ncu_tree_store), &iter);

            if ( nwamui_ncu_get_ncu_type( _ncu ) == NWAMUI_NCU_TYPE_WIRELESS ) {
                self->prv->wireless_link_num--;
                g_object_notify(G_OBJECT(self), "wireless_link_num" );
            }

            g_object_unref(_ncu);
            g_object_notify(G_OBJECT(self), "ncu_tree_store" );
            break;
        }
        g_object_unref(_ncu);
    }

    for (valid_iter = gtk_tree_model_get_iter_first( GTK_TREE_MODEL(self->prv->ncu_list_store), &iter);
         valid_iter;
         valid_iter = gtk_tree_model_iter_next( GTK_TREE_MODEL(self->prv->ncu_list_store), &iter)) {
        NwamuiNcu      *_ncu;

        gtk_tree_model_get( GTK_TREE_MODEL(self->prv->ncu_list_store), &iter, 0, &_ncu, -1);


        if ( _ncu == ncu ) {
            gtk_list_store_remove(GTK_LIST_STORE(self->prv->ncu_list_store), &iter);

            g_object_notify(G_OBJECT(self), "ncu_list_store" );
            g_object_unref(_ncu);
            break;
        }
        g_object_unref(_ncu);
    }

    self->prv->ncu_list = g_list_remove( self->prv->ncu_list, ncu );
    g_object_unref(ncu);

    g_object_thaw_notify(G_OBJECT(self->prv->ncu_tree_store));
    g_object_thaw_notify(G_OBJECT(self->prv->ncu_list_store));
}

/* After discussion with Alan, it makes sense that we only show devices that
 * only have a physical presence on the system - on the basis that to configure
 * anything else would have no effect. 
 *
 * Tunnels are the only possible exception, but until it is implemented by
 * Seb where tunnels have a physical link id, then this will omit them too.
 */
static gboolean
device_exists_on_system( gchar* device_name )
{
    dladm_handle_t              handle;
    datalink_id_t               linkid;
    gboolean                    rval = FALSE;

    if ( device_name != NULL ) {
        if ( dladm_open( &handle ) == DLADM_STATUS_OK ) {
            if ( dladm_name2info( handle, device_name, &linkid, NULL, NULL, NULL ) == DLADM_STATUS_OK ) {
                rval = TRUE;
            }
            else {
                g_debug("Unable to map device '%s' to linkid", device_name );
            }

            dladm_close( handle );
        }
    }

    return( rval );
}

static gint _num_wireless = 0; /* Count wireless i/fs */

static int
nwam_ncu_walker_cb (nwam_ncu_handle_t ncu, void *data)
{
    char*               name;
    nwam_error_t        nerr;
    NwamuiNcu*          new_ncu;
    GtkTreeIter         iter;
    NwamuiNcp*          ncp = NWAMUI_NCP(data);
    NwamuiNcpPrivate*   prv = ncp->prv;

    g_debug ("nwam_ncu_walker_cb 0x%p", ncu);

    /* FIXME: Check if NCU already exists... */
    if ( (nerr = nwam_ncu_get_name (ncu, &name)) != NWAM_SUCCESS ) {
        g_debug ("Failed to get name for ncu, error: %s", nwam_strerror (nerr));
    }

    if ( !device_exists_on_system( name ) ) {
        /* Skip device that don't have a physical equivalent */
        return( 0 );
    }
    
    if ( (new_ncu = nwamui_ncp_get_ncu_by_device_name( ncp, name )) != NULL ) {
        /* Update rather than create a new object */
        g_debug("Updating existing ncu (%s) from handle 0x%08X", name, ncu );
        nwamui_ncu_update_with_handle( new_ncu, ncu);

        /*
         * Remove from temp_ncu_list, which is being used to find NCUs that
         * have been removed from the system.
         */
        ncp->prv->temp_ncu_list = g_list_remove( ncp->prv->temp_ncu_list, new_ncu );

        g_object_unref(new_ncu);
        free(name);
        return( 0 );
    }
    else {

        g_debug("Creating a new ncu for %s from handle 0x%08X", name, ncu );
        new_ncu = nwamui_ncu_new_with_handle( NWAMUI_NCP(data), ncu);

        free(name);
    }

    /* NCU isn't already in the list, so add it */
    if ( new_ncu != NULL ) {
        if ( new_ncu && nwamui_ncu_get_ncu_type( new_ncu ) == NWAMUI_NCU_TYPE_WIRELESS ) {
            _num_wireless++;
        }

        prv->ncu_list = g_list_append( prv->ncu_list, g_object_ref(new_ncu) );

        gtk_list_store_append( prv->ncu_list_store, &iter );
        gtk_list_store_set( prv->ncu_list_store, &iter, 0, new_ncu, -1 );

        gtk_tree_store_append( prv->ncu_tree_store, &iter, NULL );
        gtk_tree_store_set( prv->ncu_tree_store, &iter, 0, new_ncu, -1 );

        g_signal_connect(G_OBJECT(new_ncu), "notify",
                         (GCallback)ncu_notify_cb, (gpointer)ncp);

        return(0);
    }

    g_warning("Failed to create a new NCU");
    return(1);
}

/**
 *  Check for new NCUs - useful after reactivation of daemon or signal for new
 *  NCUs.
 **/
extern void 
nwamui_ncp_populate_ncu_list( NwamuiNcp* self, GObject* _daemon ) 
{
    int cb_ret = 0;
    gboolean connect_list_signals = FALSE;
    gboolean connect_tree_signals = FALSE;

    g_assert(self->prv->nwam_ncp != NULL );

    if ( self->prv->ncu_list_store  == NULL ) {
        self->prv->ncu_list_store = gtk_list_store_new ( 1, NWAMUI_TYPE_NCU);
        connect_list_signals = TRUE;
    }

    if ( self->prv->ncu_tree_store  == NULL ) {
        self->prv->ncu_tree_store = gtk_tree_store_new ( 1, NWAMUI_TYPE_OBJECT);
        connect_tree_signals = TRUE;
    }

    g_object_freeze_notify(G_OBJECT(self->prv->ncu_tree_store));
    g_object_freeze_notify(G_OBJECT(self->prv->ncu_list_store));

    /*
     * Set temp_ncu_list, which is being used to find NCUs that
     * have been removed from the system.
     */
    if ( self->prv->ncu_list != NULL ) {
        self->prv->temp_ncu_list = nwamui_util_copy_obj_list( self->prv->ncu_list );
    }
    else {
        self->prv->temp_ncu_list = NULL;
    }

    nwam_ncp_walk_ncus( self->prv->nwam_ncp, nwam_ncu_walker_cb, (void*)self,
                        NWAM_FLAG_NCU_TYPE_CLASS_ALL, &cb_ret );

    if ( self->prv->temp_ncu_list != NULL ) {
        g_debug("Found some NCUs that have been removed from the system");
        for (GList *elem = g_list_first(self->prv->temp_ncu_list);
             elem != NULL;
             elem = g_list_next(elem) ) {
            if ( elem->data != NULL && NWAMUI_IS_NCU(elem->data) ) {
                nwamui_ncp_remove_ncu( self, NWAMUI_NCU(elem->data) );
            }
        }
        nwamui_util_free_obj_list( self->prv->temp_ncu_list );
        self->prv->temp_ncu_list = NULL;
    }
    g_object_thaw_notify(G_OBJECT(self->prv->ncu_tree_store));
    g_object_thaw_notify(G_OBJECT(self->prv->ncu_list_store));

    if ( connect_list_signals ) {
        g_signal_connect(self->prv->ncu_list_store, "row_deleted", G_CALLBACK(row_deleted_cb), self );
        g_signal_connect(self->prv->ncu_list_store, "row_inserted", G_CALLBACK(row_inserted_cb), self );
        g_signal_connect(self->prv->ncu_list_store, "rows_reordered", G_CALLBACK(rows_reordered_cb), self );
    }

    if ( connect_tree_signals ) {
        g_signal_connect(self->prv->ncu_tree_store, "row_deleted", G_CALLBACK(row_deleted_cb), self );
        g_signal_connect(self->prv->ncu_tree_store, "row_inserted", G_CALLBACK(row_inserted_cb), self );
        g_signal_connect(self->prv->ncu_tree_store, "rows_reordered", G_CALLBACK(rows_reordered_cb), self );
    }

    if ( self->prv->wireless_link_num != _num_wireless ) {
        self->prv->wireless_link_num = _num_wireless;
        g_object_notify(G_OBJECT (self), "wireless_link_num" );
    }

#if 0
    /* TODO: Get list of NCUs */
    libnwam_llp_t      *llp, *origllp;
    uint_t              nllp, orignllp;
    nwamui_ncp_selection_mode_t  selection_mode = NWAMUI_NCP_SELECTION_MODE_AUTOMATIC;
    gint                wireless_link_num = 0; /* Count wireless i/fs */

    if ( self->prv->ncu_tree_store  == NULL ) {
        self->prv->ncu_tree_store = gtk_tree_store_new ( 1, NWAMUI_TYPE_NCU);
    }
    
    /* Generate proper list of NCUs */
    origllp = llp = libnwam_get_llp_list(&nllp);
    orignllp = nllp;
    g_debug("%d LLPs are present.\n", nllp);
	g_debug("%-20s %-8s %-8s %-6s %-5s %-4s %-4s %s\n", 
            "Name", "Priority", "Type", "Source", "Dlink", "Ifup", "DHCP", "Status");

    while (nllp-- > 0) {
        NwamuiNcu          *ncu;
        NwamuiWifiNet      *wifi_info = NULL;
        GtkTreeIter         iter;
        nwamui_ncu_type_t   ncu_type;
        
        const char *tstr = "error";
        const char *sstr = "error";

        switch (llp->llp_type) {
        case IF_UNKNOWN:
            tstr = "unknown";
            /* Skip */
            break;
        case IF_WIRED:
            tstr = "wired";
            ncu_type = NWAMUI_NCU_TYPE_WIRED;
            break;
        case IF_WIRELESS:
            tstr = "wireless";
            ncu_type = NWAMUI_NCU_TYPE_WIRELESS;
            wireless_link_num++;
            break;
        case IF_TUN:
            tstr = "tun";
            /* Skip */
            break;
        }
        switch (llp->llp_ipv4src) {
        case IPV4SRC_STATIC:
            sstr = "static";
            break;
        case IPV4SRC_DHCP:
            sstr = "dhcp";
            break;
        }

        g_debug("%-20s %-8d %-8s %-6s %-5s %-4s %-4s %s %s %s %s",
                llp->llp_interface, llp->llp_pri, tstr,
                sstr, llp->llp_link_up ? "Up" : "Down",
                llp->llp_link_failed ? "Failed" : "OK",
                llp->llp_dhcp_failed ? "Fail" : "OK",
                llp->llp_primary ? "Primary" : "",
                llp->llp_locked ? "Locked" : "",
                llp->llp_need_wlan ? "Need WLAN" : "",
                llp->llp_need_key ? "Need Key" : "");

        if ( ncu_type == NWAMUI_NCU_TYPE_WIRED || ncu_type == NWAMUI_NCU_TYPE_WIRELESS ) {
            /* TODO - Do we need current status here, possibly not for Phase 0.5 */
            ncu = nwamui_ncp_get_ncu_by_device_name( self, llp->llp_interface );

            char * vanity_name = g_strdup_printf(_("%s (%s)"), 
              (ncu_type == NWAMUI_NCU_TYPE_WIRELESS?_("Wireless"):_("Wired")),
              llp->llp_interface);

            if ( ncu == NULL ) {

                ncu = nwamui_ncu_new(   vanity_name,
                                        llp->llp_interface,
                                        "",
                                        ncu_type,
                                        llp->llp_primary,
                                        0,
                                        llp->llp_pri,
                                        llp->llp_ipv4src == IPV4SRC_DHCP,
                                        NULL,
                                        NULL,
                                        NULL,
                                        FALSE,
                                        FALSE,
                                        NULL,
                                        NULL,
                                        NULL );


                gtk_tree_store_append( self->prv->ncu_tree_store, &iter );
                gtk_tree_store_set(self->prv->ncu_tree_store, &iter, 0, ncu, -1 );
                g_signal_connect(G_OBJECT(ncu), "notify",
                  (GCallback)ncu_notify_cb, (gpointer)self);
            } else {
                nwamui_ncu_sync(ncu,
                  vanity_name,
                  llp->llp_interface,
                  ncu_type,
                  llp->llp_primary,
                  llp->llp_pri,
                  llp->llp_ipv4src == IPV4SRC_DHCP);
            }

            g_free(vanity_name);
            
            /* Need to handle current state and send events as needed */
            if ( llp->llp_primary ) {
                nwamui_ncp_set_active_ncu( self, ncu );
            }

            if ( llp->llp_locked ) {
                selection_mode = NWAMUI_NCP_SELECTION_MODE_MANUAL;
            }

            if ( _daemon != NULL ) {
                /* May be NULL during object init phase */
                NwamuiDaemon       *daemon = NWAMUI_DAEMON(_daemon);
                nwamui_daemon_emit_signals_from_llp_info( daemon, ncu, llp );
            }

            g_object_unref(ncu);
        }

        llp++;
    }
    libnwam_free_llp_list(origllp);
    

    g_object_notify(G_OBJECT(self), "ncu_tree_store" );

    if ( self->prv->selection_mode != selection_mode ) {
        g_object_set (G_OBJECT (self),
                      "selection_mode", selection_mode,
                      NULL);
    }

    if ( self->prv->wireless_link_num != wireless_link_num ) {
        self->prv->wireless_link_num = wireless_link_num;
        g_object_notify(G_OBJECT (self), "wireless_link_num" );
    }
#endif
}

extern void
nwamui_ncp_set_manual_ncu_selection( NwamuiNcp *self, NwamuiNcu *ncu)
{
    gchar*  device_name;

    g_return_if_fail (NWAMUI_IS_NCP(self)); 
    g_return_if_fail (NWAMUI_IS_NCU(ncu)); 

    device_name = nwamui_ncu_get_device_name( ncu );

#if 0
    /* TODO: Decide if we are going to support i/f locking in Phase 1 */
    if (libnwam_lock_llp(device_name) != 0) {
        g_debug("Call failed: %s\n", strerror(errno));
    }
    else {
        g_object_set (G_OBJECT (self),
                      "selection_mode", NWAMUI_NCP_SELECTION_MODE_MANUAL,
/*                       "active_ncu", ncu, */
                      NULL);
    }
#endif    
}

extern void
nwamui_ncp_set_automatic_ncu_selection( NwamuiNcp *self )
{
    g_return_if_fail (NWAMUI_IS_NCP(self)); 

#if 0
    /* TODO: Decide if we are going to support i/f locking in Phase 1 */
    if (libnwam_lock_llp("") != 0) {
        g_debug("Call failed: %s\n", strerror(errno));
    }
    else {
        g_object_set (G_OBJECT (self),
                      "selection_mode", NWAMUI_NCP_SELECTION_MODE_AUTOMATIC,
                      NULL);
    }
#endif
}

extern nwamui_ncp_selection_mode_t
nwamui_ncp_get_selection_mode( NwamuiNcp* self )
{
    nwamui_ncp_selection_mode_t mode = NWAMUI_NCP_SELECTION_MODE_AUTOMATIC;

    g_return_val_if_fail( NWAMUI_IS_NCP(self), mode );

    g_object_get (G_OBJECT (self),
                  "selection_mode", &mode,
                  NULL);

    return( mode );
}

extern gint
nwamui_ncp_get_wireless_link_num( NwamuiNcp* self )
{
    gboolean many_wireless = FALSE;

    g_return_val_if_fail( NWAMUI_IS_NCP(self), many_wireless );

    g_object_get (G_OBJECT (self),
                  "wireless_link_num", &many_wireless,
                  NULL);

    return( many_wireless );
}

/* Callbacks */

static void
object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
    NwamuiNcp* self = NWAMUI_NCP(gobject);

    g_debug("NwamuiNcp: notify %s changed\n", arg1->name);
}
static void
ncu_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
    NwamuiNcp* self = NWAMUI_NCP(data);
    GtkTreeIter     iter;
    gboolean        valid_iter = FALSE;

    for (valid_iter = gtk_tree_model_get_iter_first( GTK_TREE_MODEL(self->prv->ncu_tree_store), &iter);
         valid_iter;
         valid_iter = gtk_tree_model_iter_next( GTK_TREE_MODEL(self->prv->ncu_tree_store), &iter)) {
        NwamuiNcu      *ncu;

        gtk_tree_model_get( GTK_TREE_MODEL(self->prv->ncu_tree_store), &iter, 0, &ncu, -1);

        if ( (gpointer)ncu == (gpointer)gobject ) {
            GtkTreePath *path;

            valid_iter = FALSE;

            path = gtk_tree_model_get_path(GTK_TREE_MODEL(self->prv->ncu_tree_store),
              &iter);
            gtk_tree_model_row_changed(GTK_TREE_MODEL(self->prv->ncu_tree_store),
              path,
              &iter);
            gtk_tree_path_free(path);
        }
        g_object_unref(ncu);
    }
}

static void
row_deleted_cb (GtkTreeModel *model, GtkTreePath *path, gpointer user_data) 
{
    NwamuiNcp     *ncp = NWAMUI_NCP(user_data);

    g_debug("NwamuiNcp: NCU List: Row Deleted: %s", gtk_tree_path_to_string(path));

    if ( model == GTK_TREE_MODEL(ncp->prv->ncu_list_store )) {
        g_warning("NCU Removed from List, but not propagated.");
    }
    else if ( model == GTK_TREE_MODEL(ncp->prv->ncu_tree_store )) {
/*         g_warning("NCU Removed from Tree, but not propagated."); */
    }
}

static void
row_inserted_cb (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer user_data)
{
    NwamuiNcp     *ncp = NWAMUI_NCP(user_data);

    g_debug("NwamuiNcp: NCU List: Row Inserted: %s",gtk_tree_path_to_string(path));

    if ( model == GTK_TREE_MODEL(ncp->prv->ncu_list_store )) {
        g_warning("NCU Inserted in List, but not propagated.");
    }
    else if ( model == GTK_TREE_MODEL(ncp->prv->ncu_tree_store )) {
/*         g_warning("NCU Inserted in Tree, but not propagated."); */
    }
}

static void
rows_reordered_cb(GtkTreeModel *tree_model, GtkTreePath *path, GtkTreeIter *iter, gpointer arg3, gpointer user_data)   
{
    NwamuiNcp     *ncp = NWAMUI_NCP(user_data);
    gchar         *path_str = gtk_tree_path_to_string(path);

    g_debug("NwamuiNcp: NCU List: Rows Reordered: %s", path_str?path_str:"NULL");

    if ( tree_model == GTK_TREE_MODEL(ncp->prv->ncu_list_store )) {
        g_warning("NCU Re-ordered in List, but not propagated.");
    }
    else if ( tree_model == GTK_TREE_MODEL(ncp->prv->ncu_tree_store )) {
/*         g_warning("NCU Re-ordered in Tree, but not propagated."); */
    }
}

static void
default_activate_ncu_signal_handler (NwamuiNcp *self, NwamuiNcu* ncu, gpointer user_data)
{
    if ( ncu != NULL ) {
        gchar* device_name = nwamui_ncu_get_device_name( ncu );

        g_debug("NCP: activate ncu %s", device_name);

        g_free( device_name );
    }
    else {
        g_debug("NCP: activate ncu NULL");
    }
}

static void
default_deactivate_ncu_signal_handler (NwamuiNcp *self, NwamuiNcu* ncu, gpointer user_data)
{
    gchar* device_name = nwamui_ncu_get_device_name( ncu );

    g_debug("NCP: deactivate ncu %s", device_name);

    g_free( device_name );
}
