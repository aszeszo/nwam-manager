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
 * File:   nwamui_daemon.c
 *
 */

#include <glib-object.h>
#include <glib/gi18n.h>
#include <strings.h>

#include "libnwamui.h"

static GObjectClass    *parent_class    = NULL;
static NwamuiDaemon*    instance        = NULL;

enum {
    WIFI_SCAN_RESULT,
    ACTIVE_ENV_CHANGED,
    S_NCU_CREATE,
    S_NCU_DESTROY,
    S_NCU_UP,
    S_NCU_DOWN,
    LAST_SIGNAL
};

enum {
    PROP_ACTIVE_ENV = 1
};

static guint nwamui_daemon_signals[LAST_SIGNAL] = { 0, 0 };

struct _NwamuiDaemonPrivate {
    NwamuiEnv   *active_env;
    
    GList       *env_list;
    GList       *enm_list;
    GList       *wifi_fav_list;
};

static void nwamui_daemon_set_property ( GObject         *object,
                                      guint            prop_id,
                                      const GValue    *value,
                                      GParamSpec      *pspec);

static void nwamui_daemon_get_property ( GObject         *object,
                                      guint            prop_id,
                                      GValue          *value,
                                      GParamSpec      *pspec);

static void nwamui_daemon_finalize (     NwamuiDaemon *self);


/* Callbacks */
static void object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);

/* Default Callback Handlers */
static void default_wifi_scan_result_signal_handler (NwamuiDaemon *self, GObject* data, gpointer user_data);
static void default_active_env_changed_signal_handler (NwamuiDaemon *self, GObject* data, gpointer user_data);
static void default_ncu_create_signal_handler (NwamuiDaemon *self, GObject* data, gpointer user_data);
static void default_ncu_destroy_signal_handler (NwamuiDaemon *self, GObject* data, gpointer user_data);
static void default_ncu_up_signal_handler (NwamuiDaemon *self, GObject* data, gpointer user_data);
static void default_ncu_down_signal_handler (NwamuiDaemon *self, GObject* data, gpointer user_data);


G_DEFINE_TYPE (NwamuiDaemon, nwamui_daemon, G_TYPE_OBJECT)

static void
nwamui_daemon_class_init (NwamuiDaemonClass *klass)
{
    /* Pointer to GObject Part of Class */
    GObjectClass *gobject_class = (GObjectClass*) klass;
        
    /* Initialise Static Parent Class pointer */
    parent_class = g_type_class_peek_parent (klass);

    /* Override Some Function Pointers */
    gobject_class->set_property = nwamui_daemon_set_property;
    gobject_class->get_property = nwamui_daemon_get_property;
    gobject_class->finalize = (void (*)(GObject*)) nwamui_daemon_finalize;

    /* Create some properties */
    g_object_class_install_property (gobject_class,
                                     PROP_ACTIVE_ENV,
                                     g_param_spec_object ("active_env",
                                                          _("active_env"),
                                                          _("active_env"),
                                                          NWAMUI_TYPE_ENV,
                                                          G_PARAM_READWRITE));


    /* Create some signals */
    nwamui_daemon_signals[WIFI_SCAN_RESULT] =   
            g_signal_new ("wifi_scan_result",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (NwamuiDaemonClass, wifi_scan_result),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT, 
                  G_TYPE_NONE,                  /* Return Type */
                  1,                            /* Number of Args */
                  G_TYPE_OBJECT);               /* Types of Args */
    
    nwamui_daemon_signals[ACTIVE_ENV_CHANGED] =   
            g_signal_new ("active_env_changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (NwamuiDaemonClass, active_env_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT, 
                  G_TYPE_NONE,                  /* Return Type */
                  1,                            /* Number of Args */
                  G_TYPE_OBJECT);               /* Types of Args */
    
    nwamui_daemon_signals[S_NCU_CREATE] =   
            g_signal_new ("ncu_create",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (NwamuiDaemonClass, ncu_create),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT, 
                  G_TYPE_NONE,                  /* Return Type */
                  1,                            /* Number of Args */
                  G_TYPE_OBJECT);               /* Types of Args */
    
    nwamui_daemon_signals[S_NCU_DESTROY] =   
            g_signal_new ("ncu_destroy",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (NwamuiDaemonClass, ncu_destroy),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT, 
                  G_TYPE_NONE,                  /* Return Type */
                  1,                            /* Number of Args */
                  G_TYPE_OBJECT);               /* Types of Args */
    
    nwamui_daemon_signals[S_NCU_UP] =   
            g_signal_new ("ncu_up",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (NwamuiDaemonClass, ncu_up),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT, 
                  G_TYPE_NONE,                  /* Return Type */
                  1,                            /* Number of Args */
                  G_TYPE_OBJECT);               /* Types of Args */
    
    nwamui_daemon_signals[S_NCU_DOWN] =   
            g_signal_new ("ncu_down",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (NwamuiDaemonClass, ncu_down),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT, 
                  G_TYPE_NONE,                  /* Return Type */
                  1,                            /* Number of Args */
                  G_TYPE_OBJECT);               /* Types of Args */
    
    klass->wifi_scan_result = default_wifi_scan_result_signal_handler;
    klass->active_env_changed = default_active_env_changed_signal_handler;
    klass->ncu_create = default_ncu_create_signal_handler;
    klass->ncu_destroy = default_ncu_destroy_signal_handler;
    klass->ncu_up = default_ncu_up_signal_handler;
    klass->ncu_down = default_ncu_down_signal_handler;
}
/* TODO - Remove static Environemnts when using libnwam */
static  gchar* environment_names[] = {
    "Automatic",
    "Default",
    NULL
};

/* TODO - Remove when using libnwam */
struct nwamui_wifi_essid {
    gchar*                          essid;
    nwamui_wifi_security_t          security;
    gchar*                          bssid;
    gchar*                          mode;
    guint                           speed;
    nwamui_wifi_signal_strength_t   signal_strength;
    nwamui_wifi_wpa_config_t        wpa_config;
};

typedef struct nwamui_wifi_essid nwamui_wifi_essid_t;


static nwamui_wifi_essid_t demo_pref_essids[] = {
    {"My Absolute Fav Network", NWAMUI_WIFI_SEC_NONE, "11:22:33:44:55:66", "g", 54, NWAMUI_WIFI_STRENGTH_EXCELLENT , NWAMUI_WIFI_WPA_CONFIG_AUTOMATIC },
    {"2nd Favourite", NWAMUI_WIFI_SEC_WEP_HEX, "22:33:44:55:66:77", "b", 11, NWAMUI_WIFI_STRENGTH_GOOD, NWAMUI_WIFI_WPA_CONFIG_AUTOMATIC },
    {"Last Favourite", NWAMUI_WIFI_SEC_WPA_PERSONAL, "33:44:55:66:77:88", "b", 2, NWAMUI_WIFI_STRENGTH_WEAK, NWAMUI_WIFI_WPA_CONFIG_PEAP },
    {NULL, NULL, NULL, NULL, NULL},
};


static void
nwamui_daemon_init (NwamuiDaemon *self)
{
    nwamui_wifi_essid_t    *ptr;
    NwamuiWifiNet          *wifi_net = NULL;
    NwamuiEnm              *new_enm = NULL;
    
    self->prv = g_new0 (NwamuiDaemonPrivate, 1);
    
    self->prv->active_env = NULL;

    /* TODO - Link with libnwam proper, currently a stub */

    for( ptr = demo_pref_essids; ptr != NULL && ptr->essid != NULL; ptr++) {
        wifi_net = nwamui_wifi_net_new(ptr->essid, ptr->security, ptr->bssid, ptr->mode, ptr->speed, ptr->signal_strength, ptr->wpa_config);
        self->prv->wifi_fav_list = g_list_append(self->prv->wifi_fav_list, (gpointer)wifi_net);
    }

    /* TODO - Get list of Env from libnwam */
    self->prv->env_list = NULL;
    for ( int i = 0; environment_names[i] != NULL; i++ ) {
        NwamuiEnv*    new_env = nwamui_env_new(environment_names[i] );
        if ( i == 0 ) {
            nwamui_daemon_set_active_env( self, new_env );
            nwamui_env_set_modifiable(new_env, FALSE);
        }
        self->prv->env_list = g_list_append(self->prv->env_list, (gpointer)new_env);
    }

    /* TODO - Get list of ENM from libnwam */
    self->prv->enm_list = NULL;
    new_enm = nwamui_enm_new("Punchin", FALSE, NULL, "/usr/local/bin/punchctl -x start", "/usr/local/bin/punchctl -x stop");
    self->prv->enm_list = g_list_append(self->prv->enm_list, (gpointer)new_enm);

    g_signal_connect(G_OBJECT(self), "notify", (GCallback)object_notify_cb, (gpointer)self);
}

static void
nwamui_daemon_set_property ( GObject         *object,
                                    guint            prop_id,
                                    const GValue    *value,
                                    GParamSpec      *pspec)
{
    NwamuiDaemon *self = NWAMUI_DAEMON(object);
    gchar*      tmpstr = NULL;
    gint        tmpint = 0;

    g_debug("set property called");
    switch (prop_id) {
        case PROP_ACTIVE_ENV: {
                if ( self->prv->active_env != NULL ) {
                    g_object_unref(G_OBJECT(self->prv->active_env));
                }

                self->prv->active_env = NWAMUI_ENV(g_value_dup_object( value ));

                g_signal_emit (self,
                             nwamui_daemon_signals[ACTIVE_ENV_CHANGED],
                             0, /* details */
                             self->prv->active_env );

            }
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
nwamui_daemon_get_property (GObject         *object,
                                  guint            prop_id,
                                  GValue          *value,
                                  GParamSpec      *pspec)
{
    NwamuiDaemon *self = NWAMUI_DAEMON(object);

    switch (prop_id) {
        case PROP_ACTIVE_ENV: {
                g_value_set_object( value, (gpointer)self->prv->active_env);
            }
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}



static void
nwamui_daemon_finalize (NwamuiDaemon *self)
{
    if (self->prv->active_env != NULL ) {
        g_object_unref( G_OBJECT(self->prv->active_env) );
    }

    g_list_foreach( self->prv->env_list, nwamui_util_obj_unref, NULL ); /* Unref all objects in list */
    g_list_free(self->prv->env_list);
    
    g_free (self->prv); 
    self->prv = NULL;

    parent_class->finalize (G_OBJECT (self));
}

/* Exported Functions */

/**
 * nwamui_daemon_get_instance:
 *
 * @returns: a new #NwamuiDaemon.
 *
 * Get a reference to the <b>singleton</b> #NwamuiDaemon. When finished using the object,
 * you should g_object_unref() it.
 *
 **/
extern NwamuiDaemon*
nwamui_daemon_get_instance (void)
{
    if ( instance == NULL ) {
	instance = NWAMUI_DAEMON(g_object_new (NWAMUI_TYPE_DAEMON, NULL));
        g_object_ref(G_OBJECT(instance)); /* Ensure we always hold at least one reference */
    }
    g_object_ref(G_OBJECT(instance)); /* update reference pointer. */
    return( instance );
}


/**
 * nwamui_daemon_wifi_start_scan:
 * @self: NwamuiDaemon*
 *
 * Initiates an asynchronous wifi scan.
 *
 * You should register a signal handler for wifi_scan_result if you want to catch the results of the scan.
 * At the end of the scan, the NwamuiWifiNet object will be NULL to signify the end of the scan.
 *
 **/
extern void 
nwamui_daemon_wifi_start_scan(NwamuiDaemon *self) 
{
    static nwamui_wifi_essid_t demo_essids[] = {
        {"essid_open", NWAMUI_WIFI_SEC_NONE, "AA:bb:cc:dd:ee:ff", "b", 11, NWAMUI_WIFI_STRENGTH_GOOD, NWAMUI_WIFI_WPA_CONFIG_AUTOMATIC },
        {"essid_wep", NWAMUI_WIFI_SEC_WEP_HEX, "BB:bb:cc:dd:ee:ff", "g", 54, NWAMUI_WIFI_STRENGTH_EXCELLENT, NWAMUI_WIFI_WPA_CONFIG_AUTOMATIC },
        {"essid_wpa", NWAMUI_WIFI_SEC_WPA_PERSONAL, "CC:bb:cc:dd:ee:ff", "g", 36, NWAMUI_WIFI_STRENGTH_WEAK, NWAMUI_WIFI_WPA_CONFIG_PEAP },
        {NULL, NULL, NULL, NULL, NULL},
    };
    nwamui_wifi_essid_t    *ptr;
    NwamuiWifiNet          *wifi_net = NULL;
    
    
    /* TODO - Link with libnwam proper, currently a stub */
    for( ptr = demo_essids; ptr != NULL && ptr->essid != NULL; ptr++) {
        wifi_net = nwamui_wifi_net_new(ptr->essid, ptr->security, ptr->bssid, ptr->mode, ptr->speed, ptr->signal_strength, ptr->wpa_config);
          /* trigger event */
          g_signal_emit (self,
                 nwamui_daemon_signals[WIFI_SCAN_RESULT],
                 0, /* details */
                 wifi_net);
    }
    /* Signal End List */
    g_signal_emit (self,
         nwamui_daemon_signals[WIFI_SCAN_RESULT],
         0, /* details */
         NULL);
}

/**
 * nwamui_daemon_get_current_ncp_name:
 * @self: NwamuiDaemon*
 *
 * @returns: null-terminated C String.
 *
 * Gets the active NCP Name
 *
 * NOTE: For Phase 1 there is only *one* NCP.
 *
 **/
extern gchar*
nwamui_daemon_get_active_ncp_name(NwamuiDaemon *self) 
{
    NwamuiNcp   *ncp = nwamui_ncp_get_instance();
    gchar       *name;
    
    g_assert( NWAMUI_IS_NCP(ncp) );
    
    name = nwamui_ncp_get_name( ncp );
    
    g_object_unref( G_OBJECT(ncp) );
    
    return( name ); 
}

/**
 * nwamui_daemon_get_current_ncp:
 * @self: NwamuiDaemon*
 *
 * @returns: NwamuiNcp*.
 *
 * Gets the active NCP
 *
 * NOTE: For Phase 1 there is only *one* NCP.
 *
 **/
extern NwamuiNcp*
nwamui_daemon_get_active_ncp(NwamuiDaemon *self) 
{
    return( nwamui_ncp_get_instance() ); 
}

/**
 * nwamui_daemon_is_active_env
 * @self: NwamuiDaemon*
 * @env: NwamuiEnv*
 *
 * @returns: TRUE if the env passed is the active one.
 *
 * Checks if the passed env is the active one.
 *
 **/
extern gboolean
nwamui_daemon_is_active_env(NwamuiDaemon *self, NwamuiEnv* env ) 
{
    g_assert( NWAMUI_IS_DAEMON( self ) );

    g_return_val_if_fail( NWAMUI_IS_DAEMON(self) && NWAMUI_IS_ENV(env), FALSE );

    return(self->prv->active_env == env);
}

/**
 * nwamui_daemon_get_active_env
 * @self: NwamuiDaemon*
 *
 * @returns: NwamuiEnv*
 *
 * Gets the active Environment
 *
 **/
extern NwamuiEnv* 
nwamui_daemon_get_active_env(NwamuiDaemon *self) 
{
    NwamuiEnv*  active_env = NULL;
    
    g_assert( NWAMUI_IS_DAEMON( self ) );

    g_return_val_if_fail( NWAMUI_IS_DAEMON(self), active_env );

    g_object_get (G_OBJECT (self),
              "active_env", &active_env,
              NULL);

    return(active_env);
}

/**
 * nwamui_daemon_set_active_env
 * @self: NwamuiDaemon*
 *
 * Sets the active Environment
 *
 **/
extern void
nwamui_daemon_set_active_env( NwamuiDaemon* self, NwamuiEnv* env )
{
    g_assert( NWAMUI_IS_DAEMON(self) );

    if ( env != NULL ) {
        g_object_set (G_OBJECT (self),
              "active_env", env,
              NULL);
    }

}

/**
 * nwamui_daemon_get_active_env_name:
 * @self: NwamuiDaemon*
 *
 * @returns: null-terminated C String
 *
 * Gets the active Environment Name
 *
 **/
extern gchar*
nwamui_daemon_get_active_env_name(NwamuiDaemon *self) 
{
    NwamuiEnv*  active = nwamui_daemon_get_active_env( self );
    gchar*      name = NULL;
    
    g_assert( active != NULL );

    name = nwamui_env_get_name(active);
    
    g_object_unref(G_OBJECT(active));
    
    return( name );
}

/**
 * nwamui_daemon_get_env_list:
 * @self: NwamuiDaemon*
 *
 * @returns: a GList* of NwamuiEnv*
 *
 * Gets a list of the known Environments.
 *
 **/
extern GList*
nwamui_daemon_get_env_list(NwamuiDaemon *self) 
{
    GList*      new_list = NULL;
    
    g_assert( NWAMUI_IS_DAEMON( self ) );

    g_return_val_if_fail( NWAMUI_IS_DAEMON(self), new_list );

    new_list = g_list_copy(self->prv->env_list);
    
    /* increase the refcounr for each element if copied list*/
    g_list_foreach(new_list, nwamui_util_obj_ref, NULL );
    
    return(new_list);
}

/**
 * nwamui_daemon_env_append:
 * @self: NwamuiDaemon*
 * @new_env: New #NwamuiEnv to add
 *
 **/
extern void
nwamui_daemon_env_append(NwamuiDaemon *self, NwamuiEnv* new_env ) 
{
    g_assert( NWAMUI_IS_DAEMON( self ) );
    g_assert( NWAMUI_IS_ENV(new_env));

    g_return_if_fail( NWAMUI_IS_DAEMON(self));

    self->prv->env_list = g_list_append(self->prv->env_list, (gpointer)g_object_ref(new_env));
}

/**
 * nwamui_daemon_env_remove:
 * @self: NwamuiDaemon*
 * @env: #NwamuiEnv to remove
 *
 * @returns: TRUE if successfully removed.
 *
 **/
extern gboolean
nwamui_daemon_env_remove(NwamuiDaemon *self, NwamuiEnv* env ) 
{
    gboolean rval   = FALSE;
    
    g_assert( NWAMUI_IS_DAEMON( self ) );
    g_assert( NWAMUI_IS_ENV(env));

    g_return_val_if_fail( NWAMUI_IS_DAEMON(self), rval );

    g_assert( nwamui_env_is_modifiable( env ) );
    
    if ( nwamui_env_is_modifiable( env ) ) {
        self->prv->env_list = g_list_remove(self->prv->env_list, (gpointer)env);
        rval = TRUE;
    }
    else {
        g_debug("Environment is not removeable");
    }

    return(rval);
}

/**
 * nwamui_daemon_get_enm_list:
 * @self: NwamuiDaemon*
 *
 * @returns: a GList* of NwamuiEnm*
 *
 * Gets a list of the known External Network Modifiers (ENMs).
 *
 **/
extern GList*
nwamui_daemon_get_enm_list(NwamuiDaemon *self) 
{
    GList*      new_list = NULL;
    
    g_assert( NWAMUI_IS_DAEMON( self ) );

    g_return_val_if_fail( NWAMUI_IS_DAEMON(self), new_list );

    new_list = g_list_copy(self->prv->enm_list);
    
    /* increase the refcounr for each element if copied list*/
    g_list_foreach(new_list, nwamui_util_obj_ref, NULL );
    
    return(new_list);
}

/**
 * nwamui_daemon_enm_append:
 * @self: NwamuiDaemon*
 * @new_env: New #NwamuiEnm to add
 *
 **/
extern void
nwamui_daemon_enm_append(NwamuiDaemon *self, NwamuiEnm* new_enm ) 
{
    g_assert( NWAMUI_IS_DAEMON( self ) );
    g_assert( NWAMUI_IS_ENM(new_enm));

    g_return_if_fail( NWAMUI_IS_DAEMON(self));

    self->prv->enm_list = g_list_append(self->prv->enm_list, (gpointer)g_object_ref(new_enm));
}

/**
 * nwamui_daemon_enm_remove:
 * @self: NwamuiDaemon*
 * @env: #NwamuiEnm to remove
 *
 * @returns: TRUE if successfully removed.
 *
 **/
extern gboolean
nwamui_daemon_enm_remove(NwamuiDaemon *self, NwamuiEnm* enm ) 
{
    gboolean rval   = FALSE;
    
    g_assert( NWAMUI_IS_DAEMON( self ) );
    g_assert( NWAMUI_IS_ENM(enm));

    g_return_val_if_fail( NWAMUI_IS_DAEMON(self), rval );

    self->prv->enm_list = g_list_remove(self->prv->enm_list, (gpointer)enm);
    rval = TRUE;

    return(rval);
}

/**
 * nwamui_daemon_get_fav_wifi_networks:
 * @self: NwamuiDaemon*
 * @returns: a GList* of NwamuiWifiNet*
 *
 * Gets a list of the Favourite (Preferred) Wifi Networks
 *
 **/
extern GList*
nwamui_daemon_get_fav_wifi_networks(NwamuiDaemon *self) 
{
    GList*  new_list = NULL;
    
    new_list = g_list_copy(self->prv->wifi_fav_list);
    
    /* increase the refcounr for each element if copied list*/
    g_list_foreach(new_list, nwamui_util_obj_ref, NULL );
    
    return( new_list );
}

/**
 * nwamui_daemon_add_wifi_fav:
 * @self: NwamuiDaemon*
 * @new_wifi: new NwamuiWifiNet* to add
 *
 * Updates the list of Favourite (Preferred) Wifi Networks, appending the specified network.
 *
 **/
extern void
nwamui_daemon_add_wifi_fav(NwamuiDaemon *self, NwamuiWifiNet* new_wifi )
{

    /* TODO - Make this more efficient */
    if( self->prv->wifi_fav_list != NULL ) {
        self->prv->wifi_fav_list = g_list_append(self->prv->wifi_fav_list, (gpointer)g_object_ref(new_wifi) );
    }
}

        
/**
 * nwamui_daemon_remove_wifi_fav:
 * @self: NwamuiDaemon*
 * @wifi: NwamuiWifiNet* to remove
 *
 * Updates the list of Favourite (Preferred) Wifi Networks, removing the specified network.
 *
 **/
extern void
nwamui_daemon_remove_wifi_fav(NwamuiDaemon *self, NwamuiWifiNet* wifi )
{

    /* TODO - Make this more efficient */
    if( self->prv->wifi_fav_list != NULL ) {
        self->prv->wifi_fav_list = g_list_remove(self->prv->wifi_fav_list, (gpointer)wifi);
        g_object_unref(wifi);
    }
}

/**
 * nwamui_daemon_set_fav_wifi_networks:
 * @self: NwamuiDaemon*
 * @new_list: a GList* of NwamuiWifiNet*
 *
 * @returns: TRUE if the list was successfully updated, FALSE otherwise.
 *
 * Updates the list of Favourite (Preferred) Wifi Networks
 *
 **/
extern gboolean
nwamui_daemon_set_fav_wifi_networks(NwamuiDaemon *self, GList *new_list )
{

    /* TODO - Make this more efficient */
    if( self->prv->wifi_fav_list != NULL ) {
        g_list_foreach(self->prv->wifi_fav_list, nwamui_util_obj_unref, NULL );
        g_list_free(self->prv->wifi_fav_list);
    }

    self->prv->wifi_fav_list = g_list_copy(new_list);
    
    /* increase the refcounr for each element if copied list*/
    g_list_foreach(self->prv->wifi_fav_list, nwamui_util_obj_ref, NULL );
    
    return( TRUE );
}

/* Callbacks */

static void
object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
    NwamuiDaemon* self = NWAMUI_DAEMON(data);

    g_debug("NwamuiDaemon: notify %s changed\n", arg1->name);
}

/* Default Signal Handlers */
static void
default_wifi_scan_result_signal_handler (NwamuiDaemon *self, GObject* data, gpointer user_data)
{
    if ( data == NULL ) {
        /* End of Scan */
        g_debug("Scan Result : End Of Scan");
    }
    else {
        NwamuiWifiNet*  wifi_net = NWAMUI_WIFI_NET(data);

        g_debug("Scan Result : Got ESSID %s", nwamui_wifi_net_get_essid( wifi_net ));
    }
}

static void
default_active_env_changed_signal_handler (NwamuiDaemon *self, GObject* data, gpointer user_data)
{
    NwamuiEnv*  env = NWAMUI_ENV(data);
    gchar*      name = nwamui_env_get_name(env);
    
    g_debug("Active Env Changed to %s", name );
    
    g_free(name);
}

static void
default_ncu_create_signal_handler (NwamuiDaemon *self, GObject* data, gpointer user_data)
{
	NwamuiNcu*  ncu = NWAMUI_NCU(data);
	gchar*      name = nwamui_ncu_get_display_name(ncu);
	
	g_debug("Create NCU %s", name );
	
	g_free(name);
}

static void
default_ncu_destroy_signal_handler (NwamuiDaemon *self, GObject* data, gpointer user_data)
{
	NwamuiNcu*  ncu = NWAMUI_NCU(data);
	gchar*      name = nwamui_ncu_get_display_name(ncu);
	
	g_debug("Destroy NCU %s", name );
	
	g_free(name);
}

static void
default_ncu_up_signal_handler (NwamuiDaemon *self, GObject* data, gpointer user_data)
{
	NwamuiNcu*  ncu = NWAMUI_NCU(data);
	gchar*      name = nwamui_ncu_get_display_name(ncu);
	
	g_debug("NCU %s up", name );
	
	g_free(name);
}

static void
default_ncu_down_signal_handler(NwamuiDaemon *self, GObject* data, gpointer user_data)
{
	NwamuiNcu*  ncu = NWAMUI_NCU(data);
	gchar*      name = nwamui_ncu_get_display_name(ncu);
	
	g_debug("NCU %s down", name );
	
	g_free(name);
}
