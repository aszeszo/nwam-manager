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
 * File:   nwamui_daemon.c
 *
 */

#include <libnwam.h>
#include <glib-object.h>
#include <glib/gi18n.h>
#include <unistd.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <libscf.h>


#include "libnwamui.h"

static GObjectClass    *parent_class    = NULL;
static NwamuiDaemon*    instance        = NULL;

/* Use above mutex for accessing these variables */
static GStaticMutex nwam_event_mutex = G_STATIC_MUTEX_INIT;
static gboolean nwam_event_thread_terminate = FALSE; /* To tell event thread to terminate set to TRUE */
static gboolean nwam_init_done = FALSE; /* Whether to call nwam_events_fini() or not */
/* End of mutex protected variables */


#ifndef NWAM_FMRI
#define	NWAM_FMRI	"svc:/network/physical:nwam"
#endif /* NWAM_FMRI */

#define WLAN_TIMEOUT_SCAN_RATE_SEC (60)
#define WEP_TIMEOUT_SEC (20)

enum {
    DAEMON_STATUS_CHANGED,
    DAEMON_INFO,
    WIFI_SCAN_RESULT,
    ACTIVE_NCP_CHANGED,
    ACTIVE_ENV_CHANGED,
    S_NCU_UP,
    S_NCU_DOWN,
    WIFI_KEY_NEEDED,
    WIFI_SELECTION_NEEDED,
    S_WIFI_FAV_ADD,
    S_WIFI_FAV_REMOVE,
    LAST_SIGNAL
};

enum {
    PROP_ACTIVE_ENV = 1,
    PROP_ACTIVE_NCP,
    PROP_WIFI_FAV,
    PROP_STATUS,
    PROP_EVENT_CAUSE
};

static guint nwamui_daemon_signals[LAST_SIGNAL] = { 0, 0 };

struct _NwamuiDaemonPrivate {
    NwamuiEnv   *active_env;
    NwamuiNcp   *active_ncp;
    
    GList       *env_list;
    GList       *ncp_list;
    GList       *enm_list;
    GList       *wifi_fav_list;

    /* others */
    gboolean                     connected_to_nwamd;
    nwamui_daemon_status_t       status;
    nwamui_daemon_event_cause_t  event_cause;
    GThread                     *nwam_events_gthread;
    gboolean                     communicate_change_to_daemon;
    guint                        wep_timeout_id;
    gboolean                     emit_wlan_changed_signals;
    GList                       *wifi_list;
};

typedef struct _NwamuiEvent {
    int           e;	/* ui daemon event type */
    nwam_event_t  nwamevent;	/* daemon data */
    NwamuiDaemon* daemon;
} NwamuiEvent;

static gboolean nwamd_event_handler(gpointer data);

static NwamuiEvent* nwamui_event_new(NwamuiDaemon* daemon, int e, nwam_event_t event);

static void nwamui_event_free(NwamuiEvent *e);

static void nwamui_daemon_set_property ( GObject         *object,
                                      guint            prop_id,
                                      const GValue    *value,
                                      GParamSpec      *pspec);

static void nwamui_daemon_get_property ( GObject         *object,
                                      guint            prop_id,
                                      GValue          *value,
                                      GParamSpec      *pspec);

static void nwamui_daemon_finalize (     NwamuiDaemon *self);

static void nwamui_daemon_populate_wifi_fav_list(NwamuiDaemon *self );

static void check_enm_online( gpointer obj, gpointer user_data );

static void nwamui_daemon_update_status( NwamuiDaemon   *daemon );

static gboolean     nwamui_daemon_nwam_connect( gboolean block );
static void         nwamui_daemon_nwam_disconnect( void );

static void         nwamui_daemon_update_status_from_object_state_event( NwamuiDaemon   *daemon, nwam_event_t nwamevent );

/* Callbacks */
static void object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);

/* Default Callback Handlers */
static void default_active_env_changed_signal_handler (NwamuiDaemon *self, GObject* data, gpointer user_data);
static void default_active_ncp_changed_signal_handler (NwamuiDaemon *self, GObject* data, gpointer user_data);
static void default_add_wifi_fav_signal_handler (NwamuiDaemon *self, NwamuiWifiNet* new_wifi, gpointer user_data);
static void default_daemon_info_signal_handler (NwamuiDaemon *self, gint type, GObject *obj, gpointer data, gpointer user_data);
static void default_ncu_down_signal_handler (NwamuiDaemon *self, NwamuiNcu* ncu, gpointer user_data);
static void default_ncu_up_signal_handler (NwamuiDaemon *self, NwamuiNcu* ncu, gpointer user_data);
static void default_remove_wifi_fav_signal_handler (NwamuiDaemon *self, NwamuiWifiNet* new_wifi, gpointer user_data);
static void default_status_changed_signal_handler (NwamuiDaemon *self, nwamui_daemon_status_t status , gpointer user_data);
static void default_wifi_key_needed (NwamuiDaemon *self, NwamuiWifiNet* wifi, gpointer user_data);
static void default_wifi_scan_result_signal_handler (NwamuiDaemon *self, NwamuiWifiNet* wifi_net, gpointer user_data);
static void default_wifi_selection_needed (NwamuiDaemon *self, NwamuiNcu* ncu, gpointer user_data);

static gpointer nwam_events_thread ( gpointer daemon );

/* walkers */
static int nwam_loc_walker_cb (nwam_loc_handle_t env, void *data);
static int nwam_enm_walker_cb (nwam_enm_handle_t enm, void *data);
static int nwam_ncp_walker_cb (nwam_ncp_handle_t ncp, void *data);
static int nwam_known_wlan_walker_cb (nwam_known_wlan_handle_t wlan_h, void *data);

G_DEFINE_TYPE (NwamuiDaemon, nwamui_daemon, NWAMUI_TYPE_OBJECT)

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

    g_object_class_install_property (gobject_class,
                                     PROP_ACTIVE_NCP,
                                     g_param_spec_object ("active_ncp",
                                                          _("active_ncp"),
                                                          _("active_ncp"),
                                                          NWAMUI_TYPE_NCP,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_STATUS,
                                     g_param_spec_int   ("status",
                                                         _("status"),
                                                         _("status"),
                                                          NWAMUI_DAEMON_STATUS_UNINITIALIZED,
                                                          NWAMUI_DAEMON_STATUS_LAST-1,
                                                          NWAMUI_DAEMON_STATUS_UNINITIALIZED,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_EVENT_CAUSE,
                                     g_param_spec_int   ("event_cause",
                                                         _("event_cause"),
                                                         _("event_cause"),
                                                          NWAMUI_DAEMON_EVENT_CAUSE_NONE,
                                                          NWAMUI_DAEMON_EVENT_CAUSE_LAST-1,
                                                          NWAMUI_DAEMON_EVENT_CAUSE_UNKNOWN,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_WIFI_FAV,
                                     g_param_spec_pointer("wifi_fav",
                                                         _("wifi_fav"),
                                                         _("wifi_fav"),
                                                          G_PARAM_READWRITE));

    /* Create some signals */
    nwamui_daemon_signals[WIFI_SCAN_RESULT] =   
            g_signal_new ("wifi_scan_result",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
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

    nwamui_daemon_signals[ACTIVE_NCP_CHANGED] =   
            g_signal_new ("active_ncp_changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (NwamuiDaemonClass, active_ncp_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT, 
                  G_TYPE_NONE,                  /* Return Type */
                  1,                            /* Number of Args */
                  G_TYPE_OBJECT);               /* Types of Args */

    nwamui_daemon_signals[DAEMON_STATUS_CHANGED] =   
            g_signal_new ("status_changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (NwamuiDaemonClass, status_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__INT, 
                  G_TYPE_NONE,                  /* Return Type */
                  1,                            /* Number of Args */
                  G_TYPE_INT);                  /* Types of Args */
    
    
    nwamui_daemon_signals[WIFI_KEY_NEEDED] =   
            g_signal_new ("wifi_key_needed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (NwamuiDaemonClass, wifi_key_needed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT, 
                  G_TYPE_NONE,                  /* Return Type */
                  1,                            /* Number of Args */
                  G_TYPE_OBJECT);               /* Types of Args */
    
    
    nwamui_daemon_signals[WIFI_SELECTION_NEEDED] =   
            g_signal_new ("wifi_selection_needed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (NwamuiDaemonClass, wifi_selection_needed),
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

    nwamui_daemon_signals[DAEMON_INFO] =   
            g_signal_new ("daemon_info",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (NwamuiDaemonClass, daemon_info),
                  NULL, NULL,
                  marshal_VOID__INT_OBJECT_POINTER, 
                  G_TYPE_NONE,                  /* Return Type */
                  3,                            /* Number of Args */
                  G_TYPE_INT,                   /* Types of Args */
                  G_TYPE_OBJECT,                /* Types of Args */
                  G_TYPE_POINTER);              /* Types of Args */
    
    nwamui_daemon_signals[S_WIFI_FAV_ADD] =   
      g_signal_new ("add_wifi_fav",
        G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
        G_STRUCT_OFFSET (NwamuiDaemonClass, add_wifi_fav),
        NULL, NULL,
        g_cclosure_marshal_VOID__OBJECT, 
        G_TYPE_NONE,                  /* Return Type */
        1,                            /* Number of Args */
        NWAMUI_TYPE_WIFI_NET);               /* Types of Args */
    
    nwamui_daemon_signals[S_WIFI_FAV_REMOVE] =   
      g_signal_new ("remove_wifi_fav",
        G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
        G_STRUCT_OFFSET (NwamuiDaemonClass, remove_wifi_fav),
        NULL, NULL,
        g_cclosure_marshal_VOID__OBJECT, 
        G_TYPE_NONE,                  /* Return Type */
        1,                            /* Number of Args */
        NWAMUI_TYPE_WIFI_NET);               /* Types of Args */

    klass->status_changed = default_status_changed_signal_handler;
    klass->wifi_key_needed = default_wifi_key_needed;
    klass->wifi_selection_needed = default_wifi_selection_needed;
    klass->wifi_scan_result = default_wifi_scan_result_signal_handler;
    klass->active_env_changed = default_active_env_changed_signal_handler;
    klass->active_ncp_changed = default_active_ncp_changed_signal_handler;
    klass->ncu_up = default_ncu_up_signal_handler;
    klass->ncu_down = default_ncu_down_signal_handler;
    klass->daemon_info = default_daemon_info_signal_handler;
    klass->add_wifi_fav = default_add_wifi_fav_signal_handler;
    klass->remove_wifi_fav = default_remove_wifi_fav_signal_handler;
}

static void
nwamui_daemon_init (NwamuiDaemon *self)
{
    GError                 *error = NULL;
    NwamuiWifiNet          *wifi_net = NULL;
    NwamuiEnm              *new_enm = NULL;
    nwam_error_t            nerr;
    
    self->prv = g_new0 (NwamuiDaemonPrivate, 1);
    
    self->prv->connected_to_nwamd = FALSE;
    self->prv->status = NWAMUI_DAEMON_STATUS_UNINITIALIZED;
    self->prv->event_cause = NWAMUI_DAEMON_EVENT_CAUSE_NONE;
    self->prv->wep_timeout_id = 0;
    self->prv->communicate_change_to_daemon = TRUE;
    self->prv->active_env = NULL;
    self->prv->active_ncp = NULL;


    if ( nwamui_daemon_nwam_connect( FALSE ) ) {
		/*
		 * according to the logic of nwam_events_thead, we can emit NWAMUI_DAEMON_INFO_ACTIVE
		 * here, so we can populate all the info in nwamd_event_handler
		 */
		g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
		  nwamd_event_handler,
		  (gpointer) nwamui_event_new(self, NWAMUI_DAEMON_INFO_ACTIVE, NULL),
		  (GDestroyNotify) nwamui_event_free);

    }
	
    self->prv->nwam_events_gthread = g_thread_create(nwam_events_thread, g_object_ref(self), TRUE, &error);
    if( self->prv->nwam_events_gthread == NULL ) {
        g_debug("Error creating nwam events thread: %s", (error && error->message)?error->message:"" );
    }

    /* Get list of Ncps from libnwam */
    self->prv->ncp_list = NULL;
    {
        nwam_error_t nerr;
        int cbret;
        nerr = nwam_walk_ncps (nwam_ncp_walker_cb, (void *)self, 0, &cbret);
        if (nerr != NWAM_SUCCESS) {
            g_debug ("[libnwam] nwam_walk_ncps %s", nwam_strerror (nerr));
        }
    }

    self->prv->env_list = NULL;

    {
        nwam_error_t nerr;
        int cbret;
        nerr = nwam_walk_locs (nwam_loc_walker_cb, (void *)self, 0, &cbret);
        if (nerr != NWAM_SUCCESS) {
            g_debug ("[libnwam] nwam_walk_locs %s", nwam_strerror (nerr));
        }
    }
    if ( self->prv->active_env == NULL && self->prv->env_list  != NULL ) {
        GList* first_element = g_list_first( self->prv->env_list );


        if ( first_element != NULL && first_element->data != NULL )  {
            self->prv->active_env = NWAMUI_ENV(g_object_ref(G_OBJECT(first_element->data)));
        }
        
    }

    
    /* TODO - Get list of ENM from libnwam */
    self->prv->enm_list = NULL;
    {
        nwam_error_t nerr;
        int cbret;
        nerr = nwam_walk_enms (nwam_enm_walker_cb, (void *)self, 0, &cbret);
        if (nerr != NWAM_SUCCESS) {
            g_debug ("[libnwam] nwam_walk_enms %s", nwam_strerror (nerr));
        }
    }

    /* Populate Wifi Favourites List */
    nwamui_daemon_populate_wifi_fav_list( self );

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
            NwamuiEnv *env;
            
                if ( self->prv->active_env != NULL ) {
                    g_object_unref(G_OBJECT(self->prv->active_env));
                }

                env = NWAMUI_ENV(g_value_dup_object( value ));

                /* TODO - I presume that keep the prev active env if failded */
                /* FIXME: For demo, always set active_env! */
                if (nwamui_env_activate (env) || 1) {
                    self->prv->active_env = env;
                    
                    g_signal_emit (self,
                      nwamui_daemon_signals[ACTIVE_ENV_CHANGED],
                      0, /* details */
                      self->prv->active_env );
                } else {
                    /* TODO - We should tell user we are failed */
                }
            }
            break;
        case PROP_ACTIVE_NCP: {
            NwamuiNcp *ncp;
            
                if ( self->prv->active_ncp != NULL ) {
                    g_object_unref(G_OBJECT(self->prv->active_ncp));
                }

                ncp = NWAMUI_NCP(g_value_dup_object( value ));

                /* Assume that activating, will cause signal to say other was
                 * deactivated too.
                 */
                nwamui_ncp_set_active(ncp, TRUE);
            }
            break;
        case PROP_STATUS: {
                self->prv->status = g_value_get_int( value );

                g_signal_emit (self,
                  nwamui_daemon_signals[DAEMON_STATUS_CHANGED],
                  0, /* details */
                  self->prv->status);
            }
            break;
        case PROP_EVENT_CAUSE: {
                self->prv->event_cause = g_value_get_int( value );
            }
            break;
        case PROP_WIFI_FAV: {
                GList *wifi_fav = (GList*)g_value_get_pointer( value );
                nwamui_daemon_set_fav_wifi_networks( self, wifi_fav );
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
        case PROP_ACTIVE_NCP: {
                g_value_set_object( value, (gpointer)self->prv->active_ncp);
            }
            break;
        case PROP_STATUS: {
                g_value_set_int(value, self->prv->status);
            }
            break;
        case PROP_EVENT_CAUSE: {
                g_value_set_int(value, self->prv->event_cause);
            }
            break;

        case PROP_WIFI_FAV: {
                GList *wifi_fav = NULL;

                wifi_fav = nwamui_daemon_get_fav_wifi_networks( self );

                g_value_set_pointer( value, wifi_fav );
            }
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
nwamui_daemon_terminate_event_thread(NwamuiDaemon *self)
{
    g_return_if_fail( self->prv->nwam_events_gthread != NULL );

    g_static_mutex_lock (&nwam_event_mutex);
    nwam_event_thread_terminate = TRUE;
    g_static_mutex_unlock (&nwam_event_mutex);

    (void)g_thread_join(self->prv->nwam_events_gthread);
    self->prv->nwam_events_gthread = NULL;
}

static gboolean
event_thread_running( void )
{
    gboolean rval = FALSE;

    g_static_mutex_lock (&nwam_event_mutex);
    rval = !nwam_event_thread_terminate;
    g_static_mutex_unlock (&nwam_event_mutex);

    return( rval );
}

static void
nwamui_daemon_finalize (NwamuiDaemon *self)
{
    int             retval;
    int             err;
    nwam_error_t    nerr;
    
    /* TODO - kill tid before destruct self */
    nwamui_daemon_nwam_disconnect();

    if ( self->prv->nwam_events_gthread != NULL ) {
        nwamui_daemon_terminate_event_thread( self );
    }
    
    if (self->prv->active_env != NULL ) {
        g_object_unref( G_OBJECT(self->prv->active_env) );
    }

    if ( self->prv->wifi_fav_list != NULL ) {
        g_list_foreach(self->prv->wifi_fav_list, nwamui_util_obj_unref, NULL );
        g_list_free( self->prv->wifi_fav_list );
    }
    
    g_list_foreach( self->prv->env_list, nwamui_util_obj_unref, NULL ); /* Unref all objects in list */
    g_list_free(self->prv->env_list);
    
    g_list_foreach( self->prv->enm_list, nwamui_util_obj_unref, NULL ); /* Unref all objects in list */
    g_list_free(self->prv->enm_list);
    
    g_list_foreach( self->prv->ncp_list, nwamui_util_obj_unref, NULL ); /* Unref all objects in list */
    g_list_free(self->prv->ncp_list);
    
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
 * nwamui_daemon_get_status:
 *
 * @returns: nwamui_daemon_status_t
 *
 * Determine the current status of the daemon
 *
 **/
extern nwamui_daemon_status_t
nwamui_daemon_get_status( NwamuiDaemon* self ) {
    nwamui_daemon_status_t status = NWAMUI_DAEMON_STATUS_UNINITIALIZED;

    g_return_val_if_fail (NWAMUI_IS_DAEMON (self), status );

    g_object_get (G_OBJECT (self),
                  "status", &status,
                  NULL);

    return( status );
}

/**
 * nwamui_daemon_set_status:
 * @status: nwamui_daemon_status_t
 *
 * Sets the status of the daemon to be <i>status</i>
 *
 **/
extern void
nwamui_daemon_set_status( NwamuiDaemon* self, nwamui_daemon_status_t status ) {
    g_return_if_fail (NWAMUI_IS_DAEMON (self));
    
    g_assert( status >= NWAMUI_DAEMON_STATUS_UNINITIALIZED && status < NWAMUI_DAEMON_STATUS_LAST );

    g_object_set (G_OBJECT (self),
                  "status", status,
                  NULL); 
}

static gboolean
trigger_scan_if_wireless(  GtkTreeModel *model,
                           GtkTreePath *path,
                           GtkTreeIter *iter,
                           gpointer data)
{
	NwamuiNcu *ncu = NULL;
	gchar *name = NULL;
	
  	gtk_tree_model_get(model, iter, 0, &ncu, -1);

    if ( ncu == NULL || !NWAMUI_IS_NCU(ncu) ) {
        return( FALSE );
    }
    g_assert( NWAMUI_IS_NCU(ncu) );
    
	name = nwamui_ncu_get_device_name (ncu);

    g_debug("i/f %s  = %s (%d)", name,  nwamui_ncu_get_ncu_type (ncu) == NWAMUI_NCU_TYPE_WIRELESS?"Wireless":"Wired", nwamui_ncu_get_ncu_type (ncu));
	if (nwamui_ncu_get_ncu_type (ncu) != NWAMUI_NCU_TYPE_WIRELESS ) {
        g_object_unref( ncu );
        g_free (name);
        return( FALSE );
	}


    if ( name != NULL ) {
        g_debug("calling nwam_wlan_scan( %s )", name );
        nwam_wlan_scan( name );
    }
    g_free (name);
    
    g_object_unref( ncu );
    
    return( FALSE );
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
    NwamuiNcp              *ncp;
    
    g_debug("Wireless Scan initiated");
 
    ncp = nwamui_daemon_get_active_ncp (self);

    if ( ncp != NULL ) {
        self->prv->emit_wlan_changed_signals = TRUE;
        nwamui_ncp_foreach_ncu (ncp, trigger_scan_if_wireless, NULL);
        g_object_unref(ncp);
    }
}

/**
 * nwamui_daemon_get_ncp_by_name:
 * @self: NwamuiDaemon*
 * @name: String
 *
 * @returns: NwamuiNcp reference
 *
 * Looks up an NCP by name
 *
 **/
extern NwamuiNcp*
nwamui_daemon_get_ncp_by_name( NwamuiDaemon *self, const gchar* name )
{
    NwamuiNcp*  ncp = NULL;

    g_assert( NWAMUI_IS_DAEMON( self ) );

    g_return_val_if_fail( NWAMUI_IS_DAEMON(self) && name != NULL, ncp );

    for( GList* item = g_list_first( self->prv->ncp_list ); 
         item != NULL;
         item = g_list_next( item ) ) {
        if ( item->data != NULL && NWAMUI_IS_NCP(item->data) ) {
            NwamuiNcp* tmp_ncp = NWAMUI_NCP(item->data);
            gchar*     ncp_name = nwamui_ncp_get_name( tmp_ncp );

            if ( strncmp( name, ncp_name, strlen(name) ) == 0 ) {
                ncp = NWAMUI_NCP(g_object_ref(G_OBJECT(tmp_ncp)));
                break;
            }
        }
    }
    return(ncp);
}

/**
 * nwamui_daemon_get_env_by_name:
 * @self: NwamuiDaemon*
 * @name: String
 *
 * @returns: NwamuiEnv reference
 *
 * Looks up an ENV by name
 *
 **/
extern NwamuiEnv*
nwamui_daemon_get_env_by_name( NwamuiDaemon *self, const gchar* name )
{
    NwamuiEnv*  env = NULL;

    g_assert( NWAMUI_IS_DAEMON( self ) );

    g_return_val_if_fail( NWAMUI_IS_DAEMON(self) && name != NULL, env );

    for( GList* item = g_list_first( self->prv->env_list ); 
         item != NULL;
         item = g_list_next( item ) ) {
        if ( item->data != NULL && NWAMUI_IS_ENV(item->data) ) {
            NwamuiEnv* tmp_env = NWAMUI_ENV(item->data);
            gchar*     env_name = nwamui_env_get_name( tmp_env );

            if ( strncmp( name, env_name, strlen(name) ) == 0 ) {
                env = NWAMUI_ENV(g_object_ref(G_OBJECT(tmp_env)));
                break;
            }
        }
    }
    return(env);
}

/**
 * nwamui_daemon_get_enm_by_name:
 * @self: NwamuiDaemon*
 * @name: String
 *
 * @returns: NwamuiEnm reference
 *
 * Looks up an ENM by name
 *
 **/
extern NwamuiEnm*
nwamui_daemon_get_enm_by_name( NwamuiDaemon *self, const gchar* name )
{
    NwamuiEnm*  enm = NULL;

    g_assert( NWAMUI_IS_DAEMON( self ) );

    g_return_val_if_fail( NWAMUI_IS_DAEMON(self) && name != NULL, enm );

    for( GList* item = g_list_first( self->prv->enm_list ); 
         item != NULL;
         item = g_list_next( item ) ) {
        if ( item->data != NULL && NWAMUI_IS_ENM(item->data) ) {
            NwamuiEnm* tmp_enm = NWAMUI_ENM(item->data);
            gchar*     enm_name = nwamui_enm_get_name( tmp_enm );

            if ( strncmp( name, enm_name, strlen(name) ) == 0 ) {
                enm = NWAMUI_ENM(g_object_ref(G_OBJECT(tmp_enm)));
                break;
            }
        }
    }
    return(enm);
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
    NwamuiNcp*  active_ncp = NULL;
    gchar*      name = NULL;
    
    g_assert( NWAMUI_IS_DAEMON( self ) );

    g_return_val_if_fail( NWAMUI_IS_DAEMON(self), name );

    g_object_get (G_OBJECT (self),
              "active_ncp", &active_ncp,
              NULL);

    name = nwamui_ncp_get_name( active_ncp );

    g_object_unref( G_OBJECT(active_ncp) );

    return(name);
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
    NwamuiNcp*  active_ncp = NULL;
    
    g_assert( NWAMUI_IS_DAEMON( self ) );

    g_return_val_if_fail( NWAMUI_IS_DAEMON(self), active_ncp );

    g_object_get (G_OBJECT (self),
              "active_ncp", &active_ncp,
              NULL);

    return( active_ncp );
}

/**
 * nwamui_daemon_set_active_ncp
 * @self: NwamuiDaemon*
 *
 * Sets the active Environment
 *
 **/
extern void
nwamui_daemon_set_active_ncp( NwamuiDaemon* self, NwamuiNcp* ncp )
{
    g_assert( NWAMUI_IS_DAEMON(self) );

    if ( ncp != NULL ) {
        g_object_set (G_OBJECT (self),
              "active_ncp", ncp,
              NULL);
    }

}

/**
 * nwamui_daemon_is_active_ncp
 * @self: NwamuiDaemon*
 * @ncp: NwamuiNcp*
 *
 * @returns: TRUE if the ncp passed is the active one.
 *
 * Checks if the passed ncp is the active one.
 *
 **/
extern gboolean
nwamui_daemon_is_active_ncp(NwamuiDaemon *self, NwamuiNcp* ncp ) 
{
    g_assert( NWAMUI_IS_DAEMON( self ) );

    g_return_val_if_fail( NWAMUI_IS_DAEMON(self) && NWAMUI_IS_NCP(ncp), FALSE );

    return(self->prv->active_ncp == ncp);
}

/**
 * nwamui_daemon_get_ncp_list:
 * @self: NwamuiDaemon*
 *
 * @returns: a GList* of NwamuiNcp*
 *
 * Gets a list of the known Ncpironments.
 *
 **/
extern GList*
nwamui_daemon_get_ncp_list(NwamuiDaemon *self) 
{
    GList*      new_list = NULL;
    
    g_assert( NWAMUI_IS_DAEMON( self ) );

    g_return_val_if_fail( NWAMUI_IS_DAEMON(self), new_list );

    new_list = g_list_copy(self->prv->ncp_list);
    
    /* increase the refcounr for each element if copied list*/
    g_list_foreach(new_list, nwamui_util_obj_ref, NULL );
    
    return(new_list);
}

/**
 * nwamui_daemon_ncp_append:
 * @self: NwamuiDaemon*
 * @new_ncp: New #NwamuiNcp to add
 *
 **/
extern void
nwamui_daemon_ncp_append(NwamuiDaemon *self, NwamuiNcp* new_ncp ) 
{
    g_assert( NWAMUI_IS_DAEMON( self ) );
    g_assert( NWAMUI_IS_NCP(new_ncp));

    g_return_if_fail( NWAMUI_IS_DAEMON(self));

    self->prv->ncp_list = g_list_append(self->prv->ncp_list, (gpointer)g_object_ref(new_ncp));
}

/**
 * nwamui_daemon_ncp_remove:
 * @self: NwamuiDaemon*
 * @ncp: #NwamuiNcp to remove
 *
 * @returns: TRUE if successfully removed.
 *
 **/
extern gboolean
nwamui_daemon_ncp_remove(NwamuiDaemon *self, NwamuiNcp* ncp ) 
{
    gboolean rval   = FALSE;
    
    g_assert( NWAMUI_IS_DAEMON( self ) );
    g_assert( NWAMUI_IS_NCP(ncp));

    g_return_val_if_fail( NWAMUI_IS_DAEMON(self), rval );

    g_assert( nwamui_ncp_is_modifiable( ncp ) );
    
    if ( nwamui_ncp_is_modifiable( ncp ) ) {
        self->prv->ncp_list = g_list_remove(self->prv->ncp_list, (gpointer)ncp);
        rval = TRUE;
    }
    else {
        g_debug("Ncp is not removeable");
    }

    return(rval);
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
 * Gets the active Environment Name. If NWAM doesn't support creating
 * "auto/default" envs, the active env may be NULL.
 *
 **/
extern gchar*
nwamui_daemon_get_active_env_name(NwamuiDaemon *self) 
{
    NwamuiEnv*  active = nwamui_daemon_get_active_env( self );
    gchar*      name = NULL;
    
    if (active == NULL)
        return NULL;
    
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

    self->prv->env_list = g_list_remove(self->prv->env_list, (gpointer)env);
    rval = TRUE;

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

/*
 * Compare a WifiNet (a) with a string (b).
 */
static gint
find_compare_wifi_net_with_name( gconstpointer a,
                                 gconstpointer b)
{
    NwamuiWifiNet  *wifi_net = NWAMUI_WIFI_NET(a);
    const gchar    *name = (const gchar*)b;
    gchar          *wifi_name = NULL;
    gint            rval = -1;

    if ( wifi_net != NULL ) {
        wifi_name = nwamui_wifi_net_get_essid( wifi_net );

        if ( wifi_name == NULL && name == NULL ) {
            rval = 0;
            g_debug("%s: wifi_name == NULL && name == NULL : returning %d", __func__, rval );
        }
        else if (wifi_name == NULL || name == NULL ) {
            rval = (wifi_name == NULL)?1:-1;
            g_debug("%s: wifi_name == NULL || name == NULL : returning %d", __func__, rval );
        }
        else {
            int len = strnlen(name, NWAM_MAX_NAME_LEN);
            rval = strncmp( wifi_name, name, len );
            g_debug("%s: strcmp( %s, %s, %d ) returning %d", __func__, wifi_name, name, len, rval);
        }
    }

    return (rval);

}

/*
 * Compare two wifi_net objects by priority.
 */
static gint
find_compare_wifi_net_by_prio( gconstpointer a,
                               gconstpointer b)
{
    NwamuiWifiNet  *wifi_net_a = NWAMUI_WIFI_NET(a);
    NwamuiWifiNet  *wifi_net_b = NWAMUI_WIFI_NET(b);
    guint64         prio_a = 0;
    guint64         prio_b = 0;
    gint            rval = -1;

    if ( wifi_net_a != NULL ) {
        prio_a = nwamui_wifi_net_get_priority( wifi_net_a );
    }

    if ( wifi_net_b != NULL ) {
        prio_b = nwamui_wifi_net_get_priority( wifi_net_b );
    }

    rval = (prio_a - prio_b);

    return (rval);
}

/**
 * nwamui_daemon_find_fav_wifi_net_by_name
 * @self: NwamuiDaemon*
 * @returns: a NwamuiWifiNet*, or NULL if not found.
 *
 * Searches the list of the Favourite (Preferred) Wifi Networks by name
 * (ESSID).
 *
 **/
extern NwamuiWifiNet*
nwamui_daemon_find_fav_wifi_net_by_name(NwamuiDaemon *self, const gchar* name ) 
{
    NwamuiWifiNet  *found_wifi_net = NULL;
    GList          *found_elem = NULL;

    if ( self == NULL || name == NULL || self->prv->wifi_fav_list == NULL ) {
        g_debug("%s: Searching for wifi_net '%s', returning NULL", __func__, name );
        return( found_wifi_net );
    }

    if ( (found_elem = g_list_find_custom( self->prv->wifi_fav_list, name, 
                                           (GCompareFunc)find_compare_wifi_net_with_name)) != NULL ) {
        if ( found_elem != NULL && found_elem->data != NULL && NWAMUI_IS_WIFI_NET(found_elem->data)) {
            found_wifi_net = NWAMUI_WIFI_NET(g_object_ref(G_OBJECT(found_elem->data)));
        }
    }
    
    g_debug("%s: Searching for wifi_net '%s', returning 0x%08x", __func__, name, found_wifi_net );
    return( found_wifi_net );
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

    if ( !self->prv->communicate_change_to_daemon
         || nwamui_wifi_net_commit_favourite( new_wifi ) ) {
        self->prv->wifi_fav_list = g_list_insert_sorted(self->prv->wifi_fav_list, (gpointer)g_object_ref(new_wifi),
                                                        find_compare_wifi_net_by_prio);

        g_signal_emit (self,
          nwamui_daemon_signals[S_WIFI_FAV_ADD],
          0, /* details */
          new_wifi );
        g_object_notify(G_OBJECT(self), "wifi_fav");
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

    if( self->prv->wifi_fav_list != NULL ) {
        if ( !self->prv->communicate_change_to_daemon
             || nwamui_wifi_net_delete_favourite( wifi ) ) {
            self->prv->wifi_fav_list = g_list_remove(self->prv->wifi_fav_list, (gpointer)wifi);

            g_signal_emit (self,
              nwamui_daemon_signals[S_WIFI_FAV_REMOVE],
              0, /* details */
              wifi );
            g_object_notify(G_OBJECT(self), "wifi_fav");
        }
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
    if ( self->prv->wifi_fav_list != NULL && new_list != NULL ) {
        /* 
         * Most complex case, need to merge and update libnwam as appropriate.
         *
         * Compare lists and look for networks added/removed 
         * Given Old and New, and copy of Old OldCopy:
         *
         *  foreach New[n] in New 
         *    if ( New[n] exists in OldCopy )
         *      SAME (remove from OldCopy)
         *    else if ( New[n] not found on OldCopy )
         *      NEW 
         *  
         *  Elements remaining in OldCopy have been deleted.
         *    
         */
        GList* merged_list = self->prv->wifi_fav_list; /* Work on original list */
        GList *old_copy = nwamui_util_copy_obj_list(self->prv->wifi_fav_list); /* Get a copy to work with */
        GList *new_items = NULL;

        for( GList* item = g_list_first( new_list ); 
             item != NULL; 
             item = g_list_next( item ) ) {
            GList* found_item = NULL;
            
            if ( (found_item = g_list_find_custom( old_copy, item->data, (GCompareFunc)nwamui_wifi_net_compare )) != NULL ) {
                /* Same */
                gboolean committed = TRUE;

                if ( nwamui_wifi_net_has_modifications( NWAMUI_WIFI_NET( found_item->data ) ) ) {
                    /* Commit first */
                    committed = nwamui_wifi_net_commit_favourite( NWAMUI_WIFI_NET( found_item->data ) );
                }

                if ( committed ) {
                    /* Can't re-read if was never committed - at least this is
                     * the case if it exists only in memory.
                     */
                    nwamui_wifi_net_update_with_handle( NWAMUI_WIFI_NET( found_item->data ), NULL ); /* Re-read configuration */
                }
                g_object_unref(G_OBJECT(found_item->data));
                old_copy = g_list_delete_link( old_copy, found_item );
            }
            else {
                new_items = g_list_append( new_items, g_object_ref(G_OBJECT(item->data)));
            }
        }

        if ( old_copy != NULL ) { /* Removed items */
            /* Remove items from merged list */
            for( GList* item = g_list_first( old_copy ); 
                 item != NULL; 
                 item = g_list_next( item ) ) {
                NwamuiWifiNet* wifi = NWAMUI_WIFI_NET(item->data);

                if (!self->prv->communicate_change_to_daemon 
                    || nwamui_wifi_net_delete_favourite( wifi ) ) {
                    /* Deleted, so remove from merged list */
                    merged_list = g_list_remove( merged_list, item->data );
                    g_object_unref(G_OBJECT( item->data));
                }

                /* Unref wifi now rather than looping again, will free
                 * old_copy when finished looping. */
                g_object_unref(G_OBJECT( item->data));
            }
            g_list_free(old_copy);
        }
        if ( new_items != NULL ) { /* New items */
            for( GList* item = g_list_first( new_items ); 
                 item != NULL; 
                 item = g_list_next( item ) ) {
                NwamuiWifiNet* wifi = NWAMUI_WIFI_NET(item->data);

                if (!self->prv->communicate_change_to_daemon 
                    || nwamui_wifi_net_commit_favourite( wifi ) ) {
                    /* Added, so add to merged list */
                    merged_list = g_list_append( merged_list, item->data );
                    /* Tell users of API */
                    g_signal_emit (self,
                      nwamui_daemon_signals[S_WIFI_FAV_ADD],
                      0, /* details */
                      g_object_ref(wifi) );
                    g_object_notify(G_OBJECT(self), "wifi_fav");
                    /* Don't ref, transfer ownership, since will be freeing new_items list anyway */
                }
            }
            g_list_free( new_items );

        }

        self->prv->wifi_fav_list = merged_list;
    }
    else if ( new_list == NULL ) {
        /* Delete existing list if any */
        if( self->prv->wifi_fav_list != NULL ) {
            for( GList* item = g_list_first( self->prv->wifi_fav_list ); 
                 item != NULL; 
                 item = g_list_next( item ) ) {
                NwamuiWifiNet* wifi = NWAMUI_WIFI_NET(item->data);

                if (!self->prv->communicate_change_to_daemon 
                    || nwamui_wifi_net_delete_favourite( wifi ) ) {
                    /* Deleted */

                    /* Tell users of API */
                    g_signal_emit (self,
                      nwamui_daemon_signals[S_WIFI_FAV_REMOVE],
                      0, /* details */
                      g_object_ref(wifi) );

                    g_object_notify(G_OBJECT(self), "wifi_fav");
                }

                /* Unref wifi now rather than looping again, will free
                 * old_copy when finished looping. */
                g_object_unref(G_OBJECT( item->data));
            }

            g_list_free(self->prv->wifi_fav_list);
        }
        self->prv->wifi_fav_list = NULL;
    }
    else {
        for( GList* item = g_list_first(new_list); 
             item != NULL; 
             item = g_list_next( item ) ) {
            NwamuiWifiNet* wifi = NWAMUI_WIFI_NET(item->data);

            if (!self->prv->communicate_change_to_daemon 
                || nwamui_wifi_net_commit_favourite( wifi ) ) {
                /* Copy on succeeded */
                self->prv->wifi_fav_list = g_list_prepend(self->prv->wifi_fav_list, g_object_ref(item->data));

                g_signal_emit (self,
                  nwamui_daemon_signals[S_WIFI_FAV_ADD],
                  0, /* details */
                  g_object_ref(wifi) );
                g_object_notify(G_OBJECT(self), "wifi_fav");
            }
        }
        
    }
    /* Ensure is sorted by prio */
    self->prv->wifi_fav_list = g_list_sort(self->prv->wifi_fav_list, find_compare_wifi_net_by_prio);

    g_object_notify(G_OBJECT(self), "wifi_fav");

    return( TRUE );
}

extern gboolean
nwamui_daemon_commit_changed_objects( NwamuiDaemon *daemon ) 
{
    gboolean    rval = TRUE;

    for( GList* ncp_item = g_list_first(daemon->prv->ncp_list); 
         rval && ncp_item != NULL; 
         ncp_item = g_list_next( ncp_item ) ) {
        NwamuiNcp*  ncp = NWAMUI_NCP(ncp_item->data);
        gchar*      ncp_name = nwamui_ncp_get_name( ncp );

        if ( nwamui_ncp_is_modifiable( ncp ) ) {
            g_warning("NCP : %s modifiable", ncp_name );
            for( GList* ncu_item = g_list_first(nwamui_ncp_get_ncu_list( NWAMUI_NCP(ncp) ) ); 
                 rval && ncu_item != NULL; 
                 ncu_item = g_list_next( ncu_item ) ) {
                NwamuiNcu*  ncu = NWAMUI_NCU(ncu_item->data);
                gchar*      ncu_name = nwamui_ncu_get_display_name( ncu );

                if ( nwamui_ncu_has_modifications( ncu ) ) {
                    g_warning("Going to commit changes for %s : %s", ncp_name, ncu_name );
                    if ( !nwamui_ncu_commit( ncu ) ) {
                        g_warning("Commit FAILED for %s : %s", ncp_name, ncu_name );
                        rval = FALSE;
                        break;
                    }
                }
                else {
                    g_warning("No changes for %s : %s", ncp_name, ncu_name );
                }
            }
        }
        else {
            g_warning("NCP : %s read-only", ncp_name );
        }

        g_free( ncp_name );
    }
    return( rval );
}

/* Callbacks */

static void
object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
    NwamuiDaemon* self = NWAMUI_DAEMON(data);

    g_debug("NwamuiDaemon: notify %s changed", arg1->name);
}

/**
 * get_ncu_by_device_name:
 *
 * Unref is needed
 */
static NwamuiNcu* 
get_ncu_by_device_name( NwamuiDaemon *daemon, NwamuiNcp*  _ncp, const char*device_name )
{
    NwamuiNcp*  ncp;
    NwamuiNcu*  ncu = NULL;

    if ( _ncp == NULL ) {
        ncp = nwamui_daemon_get_active_ncp( daemon );
    }
    else {
        ncp = _ncp;
    }

    if ( ncp ) {
        ncu = nwamui_ncp_get_ncu_by_device_name( ncp, device_name );
    }

    if ( _ncp == NULL ) {
        /* Only unref if we got ref in first place */
        g_object_unref(ncp);
    }
    return( ncu );
}

static gboolean
dispatch_scan_results_if_wireless(  GtkTreeModel *model,
                                    GtkTreePath *path,
                                    GtkTreeIter *iter,
                                    gpointer data)
{
    NwamuiDaemon   *daemon = NWAMUI_DAEMON(data);
	NwamuiNcu      *ncu = NULL;
	gchar          *name = NULL;
	
  	gtk_tree_model_get(model, iter, 0, &ncu, -1);

    if ( ncu == NULL || !NWAMUI_IS_NCU(ncu) ) {
        return( FALSE );
    }
    g_assert( NWAMUI_IS_NCU(ncu) );
    
	name = nwamui_ncu_get_device_name (ncu);

    g_debug("Dispatch scan events for i/f %s  = %s (%d)", name,  nwamui_ncu_get_ncu_type (ncu) == NWAMUI_NCU_TYPE_WIRELESS?"Wireless":"Wired", nwamui_ncu_get_ncu_type (ncu));
	if (nwamui_ncu_get_ncu_type (ncu) != NWAMUI_NCU_TYPE_WIRELESS ) {
        g_object_unref( ncu );
        g_free (name);
        return( FALSE );
	}


    if ( name != NULL ) {
        uint_t          nwlan = 0;
        nwam_wlan_t    *wlans = NULL;
        nwam_error_t    nerr;
        NwamuiWifiNet  *wifi_net = NULL;
        NwamuiWifiNet  *fav_net = NULL;

        if ( (nerr = nwam_wlan_get_scan_results( name, &nwlan, &wlans )) == NWAM_SUCCESS ) {
            g_debug("%d WLANs in cache.", nwlan);
            for (int i = 0; i < nwlan; i++) {
                g_debug( "%d: ESSID %s BSSID %s", i + 1,
                    wlans[i].essid, wlans[i].bssid);

                /* Skipping empty ESSID seems wrong here, what if we actually connect
                 * to this... Will it still appear in menu??
                 */
                if ( strlen(wlans[i].essid) > 0  && ncu != NULL ) {
                    /* Ensure it's being cached in the ncu, and if already
                     * theere will update it with new information.
                     */
                    wifi_net = nwamui_ncu_wifi_hash_insert_or_update_from_wlan_t( ncu, &(wlans[i]));

                    if ( (fav_net = nwamui_daemon_find_fav_wifi_net_by_name( daemon, 
                                                                wlans[i].essid ) ) != NULL ) {
                        /* Exists as a favourite, so update it's information */
                        nwamui_wifi_net_update_from_wlan_t( fav_net, &(wlans[i]));
                        g_object_unref(fav_net);
                    }

                    if ( wlans[i].connected || wlans[i].selected ) {
                        NwamuiWifiNet* ncu_wifi = nwamui_ncu_get_wifi_info( ncu );

                        if ( wlans[i].connected ) {
                            nwamui_wifi_net_set_status(wifi_net, NWAMUI_WIFI_STATUS_CONNECTED);
                        }

                        if ( ncu_wifi != wifi_net ) {
                            /* Update NCU with connected Wifi object */
                            nwamui_ncu_set_wifi_info(ncu, wifi_net);
                        }
                    }
                    else {
                        nwamui_wifi_net_set_status(wifi_net, NWAMUI_WIFI_STATUS_DISCONNECTED);
                    }

                    if (daemon->prv->emit_wlan_changed_signals) {
                        /* trigger event */
                        g_signal_emit (daemon,
                          nwamui_daemon_signals[WIFI_SCAN_RESULT],
                          0, /* details */
                          wifi_net); /* Hand off ownership of wifi_net */
                    }
                }
            }
            free(wlans);
        }
        else {
            g_debug("Error getting scan results for %s: %s", name, nwam_strerror(nerr) );
        }
    }
    g_free (name);
    
    g_object_unref( ncu );
    
    return( FALSE );
}


extern void
nwamui_daemon_dispatch_wifi_scan_events_from_cache(NwamuiDaemon* daemon )
{
    NwamuiNcp              *ncp;
    
    g_debug("Dispatch wifi scan events from cache called");
 
    ncp = nwamui_daemon_get_active_ncp (daemon);

    if ( ncp != NULL ) {
        daemon->prv->emit_wlan_changed_signals = TRUE;
        nwamui_ncp_foreach_ncu (ncp, dispatch_scan_results_if_wireless, (gpointer)daemon);
        g_object_unref(ncp);

        if (daemon->prv->emit_wlan_changed_signals) {
            /* Signal End List */
            g_signal_emit (daemon,
              nwamui_daemon_signals[WIFI_SCAN_RESULT],
              0, /* details */
              NULL);

            daemon->prv->emit_wlan_changed_signals = FALSE;
        }
    }
}

static void
dispatch_wifi_scan_events_from_event(NwamuiDaemon* daemon, nwam_event_t event )
{
    NwamuiWifiNet          *wifi_net = NULL;
    NwamuiNcu              *ncu = NULL;
    uint_t                  nwlan;
    const char*             prev_iface_name = NULL;
    const gchar*            name = event->data.wlan_info.name;

	if ( event == NULL || event->type != NWAM_EVENT_TYPE_WLAN_SCAN_REPORT ) {
        return;
    }

    nwlan = event->data.wlan_info.num_wlans;

    g_debug("%d WLANs detected.", nwlan);
    for (int i = 0; i < nwlan; i++) {
        /* Try avoid potentially costly look up ncu object every time */
        if ( prev_iface_name == NULL || strcmp(prev_iface_name, name ) != 0 ) {
            if ( ncu ) {
                g_object_unref(ncu);
            }
            ncu = get_ncu_by_device_name( daemon, NULL, name );
        }
        g_debug( "%d: ESSID %s BSSID %s", i + 1,
            event->data.wlan_info.wlans[i].essid,
            event->data.wlan_info.wlans[i].bssid);

        /* Skipping empty ESSID seems wrong here, what if we actually connect
         * to this... Will it still appear in menu??
         */
        if ( strlen(event->data.wlan_info.wlans[i].essid) > 0  && ncu != NULL ) {

            if ( (wifi_net = nwamui_daemon_find_fav_wifi_net_by_name( daemon, 
                                                        event->data.wlan_info.wlans[i].essid ) ) != NULL ) {
                /* Exists as a favourite, so re-use */
                nwamui_wifi_net_update_from_wlan_t( wifi_net, 
                                            &(event->data.wlan_info.wlans[i]));
            }
            else {
                wifi_net = nwamui_wifi_net_new_from_wlan_t( ncu, 
                                            &(event->data.wlan_info.wlans[i]));
            }

            if (daemon->prv->emit_wlan_changed_signals) {
                /* trigger event */
                g_signal_emit (daemon,
                  nwamui_daemon_signals[WIFI_SCAN_RESULT],
                  0, /* details */
                  wifi_net);
            }
        }
    }
    g_object_unref(ncu);
 
    if (daemon->prv->emit_wlan_changed_signals) {
        /* Signal End List */
        g_signal_emit (daemon,
          nwamui_daemon_signals[WIFI_SCAN_RESULT],
          0, /* details */
          NULL);

        daemon->prv->emit_wlan_changed_signals = FALSE;
    }
}

static NwamuiEvent*
nwamui_event_new(NwamuiDaemon* daemon, int e, nwam_event_t nwamevent)
{
    NwamuiEvent *event = g_new(NwamuiEvent, 1);
    event->e = e;
    event->nwamevent = nwamevent;
    event->daemon = g_object_ref(daemon);
    return event;
}

static void
nwamui_event_free(NwamuiEvent *event)
{
    g_object_unref(event->daemon);

    if (event->nwamevent) {
        nwam_event_free(event->nwamevent);
    }
    g_free(event);
}

static nwamui_daemon_event_cause_t
convert_cause(libnwam_diag_cause_t cause)
{
    nwamui_daemon_event_cause_t rval;

    switch (cause) {
#if 0
    /* TODO: Find way to discover cause of event */
    case dcNone:      rval = NWAMUI_DAEMON_EVENT_CAUSE_NONE; break;
    case dcDHCP:      rval = NWAMUI_DAEMON_EVENT_CAUSE_DHCP_DOWN; break;
    case dcTimer:     rval = NWAMUI_DAEMON_EVENT_CAUSE_DHCP_TIMER; break;
    case dcUnplugged: rval = NWAMUI_DAEMON_EVENT_CAUSE_UNPLUGGED; break;
    case dcUser:      rval = NWAMUI_DAEMON_EVENT_CAUSE_USER_PRIO; break;
    case dcBetter:    rval = NWAMUI_DAEMON_EVENT_CAUSE_HIGHER_PRIO_AVAIL; break;
    case dcNewAP:     rval = NWAMUI_DAEMON_EVENT_CAUSE_NEW_AP; break;
    case dcGone:      rval = NWAMUI_DAEMON_EVENT_CAUSE_AP_GONE; break;
    case dcFaded:     rval = NWAMUI_DAEMON_EVENT_CAUSE_AP_FADED; break;
    case dcAllDown:   rval = NWAMUI_DAEMON_EVENT_CAUSE_ALL_BUT_ONE_DOWN; break;
    case dcUnwanted:  rval = NWAMUI_DAEMON_EVENT_CAUSE_NOT_WANTED; break;
    case dcShutdown:  rval = NWAMUI_DAEMON_EVENT_CAUSE_DAEMON_SHUTTING_DOWN; break;
    case dcSelect:    rval = NWAMUI_DAEMON_EVENT_CAUSE_DIFFERENT_AP_SELECTED; break;
    case dcRemoved:   rval = NWAMUI_DAEMON_EVENT_CAUSE_HW_REMOVED; break;
#endif        
    default:
        rval = NWAMUI_DAEMON_EVENT_CAUSE_UNKNOWN;
        break;
    }
    return (rval);
}

const char *
nwamui_daemon_get_event_cause_string(NwamuiDaemon* self )
{
    g_return_val_if_fail (NWAMUI_IS_DAEMON (self), _("unknown") );

    switch (self->prv->event_cause) {
    case NWAMUI_DAEMON_EVENT_CAUSE_DHCP_DOWN: return _("DHCP down");
    case NWAMUI_DAEMON_EVENT_CAUSE_DHCP_TIMER: return _("DHCP timed out");
    case NWAMUI_DAEMON_EVENT_CAUSE_UNPLUGGED: return _("Cable disconnected");
    case NWAMUI_DAEMON_EVENT_CAUSE_USER_PRIO: return _("User changed priority");
    case NWAMUI_DAEMON_EVENT_CAUSE_HIGHER_PRIO_AVAIL: return _("Higher priority, cable connected");
    case NWAMUI_DAEMON_EVENT_CAUSE_NEW_AP: return _("Scan done on higher-priority wireless network interface");
    case NWAMUI_DAEMON_EVENT_CAUSE_AP_GONE: return _("Periodic scan shows wireless network gone.");
    case NWAMUI_DAEMON_EVENT_CAUSE_AP_FADED: return _("Periodic scan shows very weak signal on wireless network");
    case NWAMUI_DAEMON_EVENT_CAUSE_ALL_BUT_ONE_DOWN: return _("All network interfaces but one going down");
    case NWAMUI_DAEMON_EVENT_CAUSE_NOT_WANTED: return _("Another network interface is already active");
    case NWAMUI_DAEMON_EVENT_CAUSE_DAEMON_SHUTTING_DOWN: return _("NWAM Daemon is shutting down");
    case NWAMUI_DAEMON_EVENT_CAUSE_DIFFERENT_AP_SELECTED: return _("Different AP selected");
    case NWAMUI_DAEMON_EVENT_CAUSE_HW_REMOVED: return _("Hardware removed");
    case NWAMUI_DAEMON_EVENT_CAUSE_NONE: return _("none");
    default:
        return _("unknown");
    }
}

nwamui_daemon_event_cause_t
nwamui_daemon_get_event_cause(NwamuiDaemon* self )
{
    nwamui_daemon_event_cause_t cause = NWAMUI_DAEMON_EVENT_CAUSE_NONE;

    g_return_val_if_fail (NWAMUI_IS_DAEMON (self), cause );

    g_object_get (G_OBJECT (self),
                  "event_cause", &cause,
                  NULL);

    return( cause );
}

static void 
nwamui_daemon_set_event_cause(NwamuiDaemon* self,  nwamui_daemon_event_cause_t event_cause )
{
    g_object_set (G_OBJECT (self),
                  "event_cause", event_cause,
                  NULL); 
}

static void
nwamui_daemon_populate_wifi_fav_list(NwamuiDaemon *self )
{
    NwamuiWifiNet*  wifi = NULL;
    GList*          new_list = NULL;
    nwam_error_t    nerr;
    int             cbret;


    nerr = nwam_walk_known_wlans(nwam_known_wlan_walker_cb, &new_list,
                          NWAM_FLAG_KNOWN_WLAN_WALK_PRIORITY_ORDER, &cbret);

    if (nerr != NWAM_SUCCESS) {
        g_debug ("[libnwam] nwam_walk_known_wlans %s", nwam_strerror (nerr));
    }

    /* Use the set method to merge if necessary, but temporarily suspend
     * communication of changes to daemon */
    self->prv->communicate_change_to_daemon = FALSE;
    nwamui_daemon_set_fav_wifi_networks(self, new_list);
    nwamui_daemon_dispatch_wifi_scan_events_from_cache( self );
    self->prv->communicate_change_to_daemon = TRUE;

    g_object_notify(G_OBJECT(self), "wifi_fav");
}

/* Handle case where using WEP, and open auth, trigger an event to request a
 * new WEP key.
 */
static gboolean
wep_key_timeout_handler( gpointer data )
{
    NwamuiDaemon *daemon = nwamui_daemon_get_instance();
    NwamuiNcu* ncu = NWAMUI_NCU(data);
    NwamuiWifiNet *wifi = nwamui_ncu_get_wifi_info( ncu );

    g_debug("*** WEP Key Timeout fired ***");

    /* TODO: Rx should be part of NCU not daemon for Phase 1 */
#if 0    
    if ( wifi != NULL ) {
        gchar* ipv4_address = nwamui_ncu_get_ipv4_address( ncu );
        gint   new_rx = 0;

        /* We're a wireless interface, using WEP, and DHCP at this point.
         * If we've not received any response and the IPv4 address is empty,
         * then look for a key.
         */
        new_rx =  nwamui_daemon_get_ncu_received_packets( daemon, ncu ) - daemon->prv->old_rx_packets;

        g_debug("new_rx = %d ; ipv4_address = %s", new_rx, ipv4_address?ipv4_address:"NULL");
        if ( new_rx < 10 && (ipv4_address == NULL 
                            || strlen(ipv4_address) == 0 
                            || g_ascii_strcasecmp(ipv4_address, "0.0.0.0") == 0) ) {

            g_signal_emit (daemon,
              nwamui_daemon_signals[WIFI_KEY_NEEDED],
              0, /* details */
              wifi );
        }

        g_free(ipv4_address);
    }
#endif    
    g_object_unref(ncu);

    /* Update timer id so that app knows state */
    daemon->prv->wep_timeout_id = 0;
    g_object_unref(daemon);
    return( FALSE );
}


static void
nwamui_daemon_setup_dhcp_or_wep_key_timeout( NwamuiDaemon* self, NwamuiNcu* ncu )
{
    NwamuiWifiNet *wifi = NULL;

    g_return_if_fail (NWAMUI_IS_DAEMON (self) && NWAMUI_IS_NCU (ncu));
    
    /* If existing timeout in place, leave it */
    if ( self->prv->wep_timeout_id != 0 )  {
        g_debug("nwamui_daemon_setup_dhcp_or_wep_key_timeout: Found existing timer set");
        return;
    }

    if ( nwamui_ncu_get_ncu_type(ncu) != NWAMUI_NCU_TYPE_WIRELESS
         || ! nwamui_ncu_get_ipv4_auto_conf(ncu) ) {
        g_debug("nwamui_daemon_setup_dhcp_or_wep_key_timeout: Not wireless or DHCP");
        return;
    }

    wifi = nwamui_ncu_get_wifi_info( ncu );
    if ( wifi != NULL ) {
        switch (nwamui_wifi_net_get_security(wifi)) {
            case NWAMUI_WIFI_SEC_WEP_HEX:
            case NWAMUI_WIFI_SEC_WEP_ASCII: {
                        g_debug("nwamui_daemon_setup_dhcp_or_wep_key_timeout: Setting timer");
                         /* Store num packets before setting timer */
    /* TODO: Rx should be part of NCU not daemon for Phase 1 */
#if 0    
                         self->prv->old_rx_packets = nwamui_daemon_get_ncu_received_packets( self, ncu );
#endif
                         self->prv->wep_timeout_id = 
                            g_timeout_add_seconds( WEP_TIMEOUT_SEC, wep_key_timeout_handler, ncu );
                }
                break;
            default: {
                    break;
                }
        }
        g_object_unref(wifi);
    }
}

static void
nwamui_daemon_disable_dhcp_or_wep_key_timeout( NwamuiDaemon* self, NwamuiNcu* ncu )
{
    g_return_if_fail (NWAMUI_IS_DAEMON (self) && NWAMUI_IS_NCU (ncu));

    if ( self->prv->wep_timeout_id == 0 ) 
        return;

    g_source_remove(self->prv->wep_timeout_id);

    self->prv->wep_timeout_id = 0;
}

static gboolean
nwamd_event_handler(gpointer data)
{
    NwamuiEvent             *event = (NwamuiEvent *)data;
    NwamuiDaemon            *daemon = event->daemon;
	nwam_event_t             nwamevent = event->nwamevent;
    nwam_error_t             err;

    daemon->prv->event_cause = NWAMUI_DAEMON_EVENT_CAUSE_NONE;

    g_debug("nwamd_event_handler : %d", event->e );

    switch (event->e) {
    case NWAMUI_DAEMON_INFO_UNKNOWN:
    case NWAMUI_DAEMON_INFO_ERROR:
    case NWAMUI_DAEMON_INFO_INACTIVE:
        daemon->prv->connected_to_nwamd = FALSE;
        nwamui_daemon_set_status(daemon, NWAMUI_DAEMON_STATUS_ERROR);
        break;
    case NWAMUI_DAEMON_INFO_ACTIVE:
        /* Set to UNINITIALIZED first, status will then be got later when icon is to be shown
         */
        daemon->prv->connected_to_nwamd = TRUE;

        nwamui_daemon_set_status(daemon, NWAMUI_DAEMON_STATUS_UNINITIALIZED);

		/* should repopulate data here */
        nwamui_ncp_populate_ncu_list(daemon->prv->active_ncp, G_OBJECT(daemon));
        nwamui_daemon_populate_wifi_fav_list(daemon);
        nwamui_daemon_dispatch_wifi_scan_events_from_cache(daemon);

        /* Will generate an event if status changes */
        nwamui_daemon_update_status( daemon );

        break;
    case NWAMUI_DAEMON_INFO_RAW:
    {
        g_debug("NWAMUI_DAEMON_INFO_RAW event type %d (%s)", 
                nwamevent->type, nwam_event_type_to_string( nwamevent->type ));
        switch (nwamevent->type) {
        case NWAM_EVENT_TYPE_INIT: {
                /* should repopulate data here */
                
                g_debug ("NWAM daemon started.");
                /* Redispatch as INFO_ACTIVE */
                g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                  nwamd_event_handler,
                  (gpointer) nwamui_event_new(daemon, NWAMUI_DAEMON_INFO_ACTIVE, NULL),
                  (GDestroyNotify) nwamui_event_free);
            }
            break;
        case NWAM_EVENT_TYPE_SHUTDOWN: {
                g_debug ("NWAM daemon stopped.");
                /* Redispatch as INFO_INACTIVE */
                g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                  nwamd_event_handler,
                  (gpointer) nwamui_event_new(daemon, NWAMUI_DAEMON_INFO_INACTIVE, NULL),
                  (GDestroyNotify) nwamui_event_free);
            }
            break;
        case NWAM_EVENT_TYPE_PRIORITY_GROUP: {
                    g_debug("Priority group changed to %d",
                             nwamevent->data.priority_group_info.priority);

                    /* Re-evaluate status since a change in the priority group
                     * means that the status could be different.
                     */
                    nwamui_daemon_update_status( daemon );
            }
            break;
        case NWAM_EVENT_TYPE_OBJECT_STATE: {
                g_debug( "%s Object %s's state changed to %s, %s",
		                 nwam_object_type_to_string(nwamevent->data.object_state.object_type),
                         nwamevent->data.object_state.name,
                         nwam_state_to_string(nwamevent->data.object_state.state),
                         nwam_aux_state_to_string(nwamevent->data.object_state.aux_state));
                nwamui_daemon_update_status_from_object_state_event( daemon, nwamevent );
            }
            break;
        case NWAM_EVENT_TYPE_LINK_STATE: {
                NwamuiNcu*      ncu;
                const gchar*    name = nwamevent->data.link_state.name;

                g_debug("Link %s state changed to %s",
                         nwamevent->data.link_state.name,
                         nwamevent->data.link_state.link_state == 0 ? "down" : "up" );

                if ( nwamevent->data.link_state.link_state == 0 ) {
                    /* Interface Down */

                    /* Directly deliver to upper consumers */
                    ncu = get_ncu_by_device_name( daemon, NULL, name );

                    if ( ncu ) {
                        g_signal_emit (daemon,
                          nwamui_daemon_signals[S_NCU_DOWN],
                          0, /* details */
                          ncu );

                        g_object_unref(ncu);
                    }
                    else {
                        g_debug("%s: Got link state event for device %s, but didn't find an NCU object", __func__, name );
                    }
                }
            }
            break;
        case NWAM_EVENT_TYPE_IF_STATE: {
                NwamuiNcu*      ncu;
                const gchar*    name = nwamevent->data.if_state.name;
                const gchar*    address = NULL;

                ncu = get_ncu_by_device_name( daemon, NULL, name );

                if (nwamevent->data.if_state.addr_valid) {
                    struct sockaddr_in *sin;

                    sin = (struct sockaddr_in *)
                        &(nwamevent->data.if_state.addr);
                    address = inet_ntoa(sin->sin_addr);
                    g_debug ("Interface %s is up with address %s", name, address );

                    /* Directly deliver to upper consumers */
                    if ( ncu ) {
                        g_signal_emit (daemon,
                          nwamui_daemon_signals[S_NCU_UP],
                          0, /* details */
                          ncu );

                        g_object_unref(ncu);
                    }
                    else {
                        g_debug("%s: Got i/f state event for device %s, but didn't find an NCU object", __func__, name );
                    }
                } 
            }
            break;

		case NWAM_EVENT_TYPE_LINK_ACTION: {
                NwamuiNcp*      ncp = nwamui_daemon_get_active_ncp( daemon );
                nwam_action_t   action = nwamevent->data.link_action.action;
                const gchar*    name = nwamevent->data.link_action.name;

                if ( action == NWAM_ACTION_ADD ) {
                    nwamui_ncp_populate_ncu_list( ncp, G_OBJECT(daemon) );

                    g_debug("Interface %s added", name );
                }
                else if ( action == NWAM_ACTION_REMOVE ) {
                    NwamuiNcu *ncu = get_ncu_by_device_name( daemon, ncp, name );
                    if ( ncu ) {
                        nwamui_ncp_remove_ncu( ncp, ncu );
                        g_object_unref(ncu);
                    }
                    g_debug("Interface %s removed", name );
                }
                else {
                    g_error("LINK Action : %d recieved and unhandled", 
                                nwamevent->data.object_action.action );
                }
                if ( ncp ) {
                    g_object_unref(G_OBJECT(ncp));
                }
            }
            break;

		case NWAM_EVENT_TYPE_OBJECT_ACTION: {

                switch ( nwamevent->data.object_action.action ) {
                case NWAM_ACTION_ADD:
                    g_debug("Got NWAM_ACTION_ADD for object %s, doing nothing...", 
                            nwamevent->data.object_action.name );
                    break;
                case NWAM_ACTION_REMOVE:
                    g_debug("Got NWAM_ACTION_REMOVE for object %s, doing nothing...", 
                            nwamevent->data.object_action.name );
                    break;
                case NWAM_ACTION_RENAME:
                    g_debug("Got NWAM_ACTION_RENAME for object %s, doing nothing...", 
                            nwamevent->data.object_action.name );
                    break;
                case NWAM_ACTION_DESTROY:
                    if ( nwamevent->data.object_action.object_type == NWAM_OBJECT_TYPE_KNOWN_WLAN ) {
                        NwamuiWifiNet   *wifi;

                        /* Need to refresh to catch update to priorities */
                        if ( (wifi = nwamui_daemon_find_fav_wifi_net_by_name( daemon, 
                                        nwamevent->data.object_action.name ) ) != NULL ) {
                            daemon->prv->communicate_change_to_daemon = FALSE;
                            nwamui_daemon_remove_wifi_fav(daemon, wifi );
                            daemon->prv->communicate_change_to_daemon = TRUE;
                            g_object_unref(wifi);
                        }
                    }
                    else {
                        g_debug("Got NWAM_ACTION_DESTROY for object %s, doing nothing...", 
                                nwamevent->data.object_action.name );
                    }
                    break;
                case NWAM_ACTION_REFRESH:
                    if ( nwamevent->data.object_action.object_type == NWAM_OBJECT_TYPE_KNOWN_WLAN ) {
                        NwamuiWifiNet   *wifi;

                        /* Need to refresh to catch update to priorities */
                        if ( (wifi = nwamui_daemon_find_fav_wifi_net_by_name( daemon, 
                                        nwamevent->data.object_action.name ) ) != NULL ) {
                            nwamui_object_reload( NWAMUI_OBJECT(wifi) );
                            g_object_unref(wifi);
                        }
                    }
                    else {
                        g_debug("Got NWAM_ACTION_REFRESH for object %s, doing nothing...", 
                                nwamevent->data.object_action.name );
                    }
                    break;
                case NWAM_ACTION_ENABLE:
                    if ( nwamevent->data.object_action.object_type == NWAM_OBJECT_TYPE_NCP ) {
                        NwamuiNcp *ncp;

                        g_debug("NCP %s Enabled",
                                nwamevent->data.object_action.name );
            
                        ncp = nwamui_daemon_get_ncp_by_name( daemon,
                                nwamevent->data.object_action.name );

                        if ( ncp != NULL ) {
                            /* Don't use nwamui_ncp_set_active() since it will
                             * actually cause a switch again...
                             */
                            if ( daemon->prv->active_ncp != NULL ) {
                                g_object_unref(G_OBJECT(daemon->prv->active_ncp));
                            }

                            daemon->prv->active_ncp = NWAMUI_NCP(g_object_ref( ncp ));

                            g_signal_emit (daemon,
                              nwamui_daemon_signals[ACTIVE_NCP_CHANGED],
                              0, /* details */
                              ncp );

                            g_object_unref(ncp);
                        }
                        else {
                            g_error("Couldn't find NCP object for '%s'", nwamevent->data.object_action.name );
                        }
                    }
                    else if ( nwamevent->data.object_action.object_type == NWAM_OBJECT_TYPE_NCU ) {
                        NwamuiNcu *ncu;

                        g_debug("NCU %s Enabled",
                                nwamevent->data.object_action.name );
            
                        ncu = get_ncu_by_device_name( daemon, NULL,
                                                      nwamevent->data.object_action.name );

                        if ( ncu != NULL ) {
                            if ( !nwamui_ncu_has_modifications( ncu ) ) {
                                nwamui_ncu_reload( ncu );
                            }

                            g_signal_emit (daemon,
                              nwamui_daemon_signals[DAEMON_INFO],
                              0, /* details */
                              NWAMUI_DAEMON_INFO_NCU_SELECTED,
                              ncu,
                              g_strdup_printf(_("%s network interface enabled."), 
                                  nwamui_ncu_get_vanity_name( ncu )));

                            g_object_unref(ncu);
                        }
                        else {
                            g_error("Couldn't find NCU object for '%s'", nwamevent->data.object_action.name );
                        }
                    }
                    else if ( nwamevent->data.object_action.object_type == NWAM_OBJECT_TYPE_ENM ) {
                        g_debug("ENM %s enabled", 
                                nwamevent->data.object_action.name );
                        g_signal_emit (daemon,
                          nwamui_daemon_signals[DAEMON_INFO],
                          0, /* details */
                          NWAMUI_DAEMON_INFO_GENERIC,
                          NULL,
                          g_strdup_printf(_("VPN '%s' enabled"),
                                           nwamevent->data.object_action.name) );
                    }
                    break;
                case NWAM_ACTION_DISABLE:
                    if ( nwamevent->data.object_action.object_type == NWAM_OBJECT_TYPE_NCU ) {
                        NwamuiNcu *ncu;

                        g_debug("NCU %s disabled",
                                nwamevent->data.object_action.name );
            
                        ncu = get_ncu_by_device_name( daemon, NULL,
                                                      nwamevent->data.object_action.name );

                        if ( ncu != NULL ) { 
                            if ( !nwamui_ncu_has_modifications( ncu ) ) {
                                nwamui_ncu_reload( ncu );
                            }
                            g_object_unref(ncu);
                        }
                        else {
                            g_error("Couldn't find NCU object for '%s'", nwamevent->data.object_action.name );
                        }
                    }
                    else if ( nwamevent->data.object_action.object_type == NWAM_OBJECT_TYPE_ENM ) {
                        g_debug("ENM %s disabled", 
                                nwamevent->data.object_action.name );

                        g_signal_emit (daemon,
                          nwamui_daemon_signals[DAEMON_INFO],
                          0, /* details */
                          NWAMUI_DAEMON_INFO_GENERIC,
                          NULL,
                          g_strdup_printf(_("VPN '%s' disabled"),
                                           nwamevent->data.object_action.name) );
                    }
                    break;
                }
            }
            break;

		case NWAM_EVENT_TYPE_WLAN_SCAN_REPORT: {
                const gchar*    name = nwamevent->data.wlan_info.name;
                NwamuiNcu * ncu = get_ncu_by_device_name( daemon, NULL, name );
                g_debug("Wireless networks found.", name );

                daemon->prv->emit_wlan_changed_signals = TRUE;

                g_signal_emit (daemon,
                  nwamui_daemon_signals[DAEMON_INFO],
                  0, /* details */
                  NWAMUI_DAEMON_INFO_WLAN_CHANGED,
                  ncu,
                  g_strdup_printf(_("New wireless networks found.")));

                dispatch_wifi_scan_events_from_event( daemon, nwamevent );
                g_object_unref(ncu);
            }
            break;
        case NWAM_EVENT_TYPE_WLAN_CONNECTION_REPORT: {
                const gchar*    device = nwamevent->data.wlan_info.name;
                NwamuiNcu *     ncu = get_ncu_by_device_name( daemon, NULL, device );
                NwamuiWifiNet*  wifi = NULL;
                gchar*          name = NULL;
                gchar*          wifi_name = NULL;


                g_assert(ncu);
                if ( ncu !=  NULL ) {
                    wifi = nwamui_ncu_get_wifi_info(ncu);
                    if ( wifi ) {
                        wifi_name = nwamui_wifi_net_get_essid( wifi );
                    }
                    name = nwamui_ncu_get_vanity_name( ncu );
                }

                if ( nwamevent->data.wlan_info.connected ) {

                    g_debug("Wireless network '%s' connected", 
                            nwamevent->data.wlan_info.wlans[0].essid );

                    if ( wifi && wifi_name && 
                         strncmp( wifi_name, nwamevent->data.wlan_info.wlans[0].essid, NWAM_MAX_NAME_LEN) == 0 ) {
                        nwamui_wifi_net_set_status(wifi, NWAMUI_WIFI_STATUS_CONNECTED);
                        nwamui_wifi_net_update_from_wlan_t( wifi, 
                                                &(nwamevent->data.wlan_info.wlans[0]));
                    }
                    else {
                        if ( wifi ) {
                            /* Different wifi_net */
                            nwamui_wifi_net_set_status(wifi, NWAMUI_WIFI_STATUS_DISCONNECTED);
                            g_object_unref(wifi); 
                        }
                        if ( wifi_name ) {
                            g_free(wifi_name);
                        }

                        if ( (wifi = nwamui_daemon_find_fav_wifi_net_by_name( daemon, 
                                        nwamevent->data.wlan_info.wlans[0].essid ) ) != NULL ) {
                            /* Exists as a favourite, so re-use */
                            nwamui_wifi_net_set_status(wifi, NWAMUI_WIFI_STATUS_CONNECTED);
                            nwamui_wifi_net_update_from_wlan_t( wifi, 
                                                    &(nwamevent->data.wlan_info.wlans[0]));
                        }
                        else {
                            wifi = nwamui_wifi_net_new_from_wlan_t( ncu, 
                                                    &(nwamevent->data.wlan_info.wlans[0]));
                        }

                        nwamui_ncu_set_wifi_info(ncu, wifi);
                    }

                    /* Is this needed for Phase 1? */
                    /* nwamui_daemon_setup_dhcp_or_wep_key_timeout( daemon, ncu ); */

                    /* Retrieve fav wifi list */
                    nwamui_daemon_populate_wifi_fav_list(daemon);

                    g_signal_emit (daemon,
                      nwamui_daemon_signals[DAEMON_INFO],
                      0, /* details */
                      NWAMUI_DAEMON_INFO_WLAN_CONNECTED,
                      wifi,
                      g_strdup_printf("%s", nwamevent->data.wlan_info.wlans[0].essid));
                }
                else {
                    g_debug("Wireless connection on '%s' failed", device );

                    if ( wifi ) {
                        nwamui_wifi_net_set_status(wifi, NWAMUI_WIFI_STATUS_FAILED);
                    }

                    g_signal_emit (daemon,
                      nwamui_daemon_signals[DAEMON_INFO],
                      0, /* details */
                      NWAMUI_DAEMON_INFO_WLAN_CONNECT_FAILED,
                      wifi,
                      g_strdup_printf(_("%s connection to wireless lan failed"), 
                                        name?name:device));


                }
                g_free(name);
                if ( wifi )
                    g_object_unref(wifi);

                if ( ncu )
                    g_object_unref(ncu);

            }
            break;
        case NWAM_EVENT_TYPE_WLAN_NEED_CHOICE: {
                const gchar*    device = nwamevent->data.wlan_info.name;
                NwamuiNcu *     ncu = get_ncu_by_device_name( daemon, NULL, device );

                g_debug("No suitable wireless networks found, selection needed.");

                if (daemon->prv->emit_wlan_changed_signals) {
                    g_signal_emit (daemon,
                      nwamui_daemon_signals[DAEMON_INFO],
                      0, /* details */
                      NWAMUI_DAEMON_INFO_WLAN_CHANGED,
                      NULL,
                      g_strdup_printf(_("New wireless networks found.")));

                    dispatch_wifi_scan_events_from_event( daemon, nwamevent );
                }

                g_signal_emit (daemon,
                  nwamui_daemon_signals[WIFI_SELECTION_NEEDED],
                  0, /* details */
                  ncu );

                g_object_unref(ncu);
            }
            break;

        case NWAM_EVENT_TYPE_WLAN_NEED_KEY: {
                const gchar*    device = nwamevent->data.wlan_info.name;
                NwamuiNcu*      ncu = get_ncu_by_device_name( daemon, NULL, device );
                NwamuiWifiNet*  wifi = nwamui_ncu_get_wifi_info( ncu );


                if ( wifi == NULL ) {
                    if ( (wifi = nwamui_daemon_find_fav_wifi_net_by_name( daemon, 
                                    nwamevent->data.wlan_info.wlans[0].essid ) ) != NULL ) {
                        /* Exists as a favourite, so re-use */
                        nwamui_wifi_net_update_from_wlan_t( wifi, 
                                                &(nwamevent->data.wlan_info.wlans[0]));
                    } else {
                        wifi = nwamui_wifi_net_new_from_wlan_t( ncu, 
                                                    &(nwamevent->data.wlan_info.wlans[0]));
                    }
                    nwamui_ncu_set_wifi_info( ncu, wifi );
                }
                else {
                    char* essid = nwamui_wifi_net_get_essid( wifi );

                    if( essid == NULL ||
                        strncmp( essid, nwamevent->data.wlan_info.wlans[0].essid, strlen(essid) ) != 0) {
                        /* ESSIDs are different, so we should update the ncu */
                        NwamuiWifiNet* new_wifi = NULL;

                        if ( (new_wifi = nwamui_daemon_find_fav_wifi_net_by_name( daemon, 
                                                    nwamevent->data.wlan_info.wlans[0].essid ) ) != NULL ) {
                            /* Exists as a favourite, so re-use */
                            nwamui_wifi_net_update_from_wlan_t( new_wifi, 
                                                    &(nwamevent->data.wlan_info.wlans[0]));
                        } else {
                            new_wifi = nwamui_wifi_net_new_from_wlan_t( ncu, 
                                                    &(nwamevent->data.wlan_info.wlans[0]));
                        }

                        g_debug("deWlanKeyNeeded: WifiNets differ, replacing");
                        nwamui_ncu_set_wifi_info( ncu, new_wifi );
                        g_object_unref(wifi);
                        wifi = new_wifi;
                    }

                    g_free(essid);
                }

                g_assert(wifi);

                g_signal_emit (daemon,
                  nwamui_daemon_signals[WIFI_KEY_NEEDED],
                  0, /* details */
                  wifi );
                g_object_unref(ncu);
            }
            break;

#ifdef _NEVER
        /* FIXME: Don't see an equivalent event in Phase 1... */
        case deWlanDisconnect: {
            NwamuiNcu * ncu = get_ncu_by_device_name( daemon, NULL, nwamevent->led_interface );
            NwamuiWifiNet* wifi = nwamui_ncu_get_wifi_info(ncu);


            g_debug("Wireless network '%s' disconnected", nwamevent->led_wlan.wla_essid );
            g_assert(ncu);

            if ( wifi ) {
                nwamui_wifi_net_set_status(wifi, NWAMUI_WIFI_STATUS_DISCONNECTED);
                /* Do not reuse wifi net instance which will mess up menus */
/*                 nwamui_wifi_net_set_from_wlan_attrs( wifi, &nwamevent->led_wlan  ); */
                nwamui_ncu_set_wifi_info(ncu, NULL);
            }
            else {
                wifi = nwamui_wifi_net_new_from_wlan_attrs( ncu, &nwamevent->led_wlan  );
            }

            g_signal_emit (daemon,
              nwamui_daemon_signals[DAEMON_INFO],
              0, /* details */
              NWAMUI_DAEMON_INFO_WLAN_DISCONNECTED,
              wifi,
              g_strdup_printf("%s", nwamevent->led_wlan.wla_essid ));

            g_object_unref(wifi);
            g_object_unref(ncu);
        }
            break;
#endif /* _NEVER */

#ifdef TODO_EVENTS
        case deLLPSelected: {
            NwamuiNcp * ncp = nwamui_daemon_get_active_ncp( daemon );
            NwamuiNcu * ncu = get_ncu_by_device_name( daemon, ncp, nwamevent->led_interface );
            gchar*      name = NULL;

            g_debug("Interface  '%s' selected.", nwamevent->led_interface);

            g_assert(ncp && ncu);

            nwamui_ncp_set_active_ncu( ncp, ncu );

            name = nwamui_ncu_get_vanity_name(ncu);

            nwamui_daemon_setup_dhcp_or_wep_key_timeout( daemon, ncu );

            if ( nwamui_ncu_get_ncu_type(ncu) == NWAMUI_NCU_TYPE_WIRELESS ) {
                nwamui_daemon_populate_wifi_fav_list(daemon);
            }

            g_signal_emit (daemon,
              nwamui_daemon_signals[DAEMON_INFO],
              0, /* details */
              NWAMUI_DAEMON_INFO_NCU_SELECTED,
              ncu,
              g_strdup_printf(_("%s network interface is up."), name ));

            g_free(name);

            g_object_unref(ncu);
            g_object_unref(ncp);
        }
            break;
        case deLLPUnselected: {
            NwamuiNcp * ncp = nwamui_daemon_get_active_ncp( daemon );
            NwamuiNcu * ncu = get_ncu_by_device_name( daemon, ncp, nwamevent->led_interface );
            NwamuiNcu * current_ncu = NULL;
            const char* reason = NULL;
            gchar*      name = NULL;
            nwamui_daemon_set_event_cause( daemon, convert_cause( nwamevent->led_cause ));
            reason = nwamui_daemon_get_event_cause_string( daemon );

            g_debug("'%s' network interface unselected, Reason: '%s'", nwamevent->led_interface, reason);

            g_assert(ncp && ncu);

            current_ncu = nwamui_ncp_get_active_ncu(ncp);

            if (current_ncu) {
                if (current_ncu == ncu) {
                    nwamui_ncp_set_active_ncu( ncp, NULL );
                    nwamui_daemon_disable_dhcp_or_wep_key_timeout( daemon, ncu );
                }
                g_object_unref(current_ncu);
            }

            name = nwamui_ncu_get_vanity_name(ncu);

            g_signal_emit (daemon,
              nwamui_daemon_signals[DAEMON_INFO],
              0, /* details */
              NWAMUI_DAEMON_INFO_NCU_UNSELECTED,
              ncu,
              g_strdup_printf(_("%s network interface brought down\nReason: %s"), name, reason ));

            g_free(name);

            g_object_unref(ncu);
            g_object_unref(ncp);
        }
            break;

#endif /* TODO _EVENTS */
        default:
            /* Directly deliver to upper consumers */
            g_signal_emit (daemon,
              nwamui_daemon_signals[DAEMON_INFO],
              0, /* details */
              NWAMUI_DAEMON_INFO_UNKNOWN,
              NULL,
              g_strdup_printf ("Unknown NWAM event type %d.", nwamevent->type));
            break;
        }
    }
    break;
    case NWAMUI_DAEMON_INFO_WLAN_CHANGED:
        /* fall through */
    default:
        g_assert_not_reached();
    }
    
    return FALSE;
}


/**
 * nwam_events_callback:
 *
 * This callback is needed to be MT safe.
 */
static int
nwam_events_callback (nwam_event_t *msg, int size, int nouse)
{
#if 0
    /* This is not back to more like phase 0.5, so need to refactor again */
    nwam_event_t *idx;
    NwamuiDaemon *self = nwamui_daemon_get_instance ();
    
    g_debug ("nwam_events_callback");
    
    for (idx = msg; idx; idx = idx->next) {
        
        switch (idx->type) {
        case NWAM_EVENT_TYPE_NOOP:
        case NWAM_EVENT_TYPE_SOURCE_DEAD:
            g_debug ("NWAM daemon died.");
            break;
        case NWAM_EVENT_TYPE_SOURCE_BACK:
            g_debug ("NWAM events synthesized.");
            break;
        case NWAM_EVENT_TYPE_NO_MAGIC:
            g_debug ("NWAM events require user's interaction.");
            break;
        case NWAM_EVENT_TYPE_INFO:
            /* Directly deliver to upper consumers */
            g_signal_emit (self,
              nwamui_daemon_signals[DAEMON_INFO],
              0, /* details */
              g_strdup (idx->data.info.message));
            break;
        case NWAM_EVENT_TYPE_IF_STATE:
            /* TODO - */
            if (idx->data.if_state.addr_valid) {
                char address[INET6_ADDRSTRLEN];
                struct sockaddr_in *sin;
                sin = (struct sockaddr_in *)&idx->data.if_state.addr;
                printf("address %s", inet_ntoa(sin->sin_addr));
                g_signal_emit (self,
                  nwamui_daemon_signals[DAEMON_INFO],
                  0, /* details */
                  g_strdup_printf ("interface %s (%d) flags %x\naddress %s",
                    idx->data.if_state.name, 
                    idx->data.if_state.index,
                    idx->data.if_state.flags,
                    inet_ntoa(sin->sin_addr)));
            } else {
                g_debug ("interface %s (%d) flags %x",
                  idx->data.if_state.name, 
                  idx->data.if_state.index,
                  idx->data.if_state.flags);
            }
            break;
        case NWAM_EVENT_TYPE_IF_REMOVED:
            /* TODO - */
            g_signal_emit (self,
              nwamui_daemon_signals[DAEMON_INFO],
              0, /* details */
              g_strdup_printf ("interface %s is removed.", idx->data.removed.name));
            break;
        case NWAM_EVENT_TYPE_LINK_STATE:
            /* TODO - map to NCU up/down */
            switch (idx->data.link_state.link_state) {
            }
            
            g_signal_emit (self,
              nwamui_daemon_signals[DAEMON_INFO],
              0, /* details */
              g_strdup_printf ("link %s is removed.", idx->data.removed.name));
            break;
        case NWAM_EVENT_TYPE_LINK_REMOVED:
            /* TODO - map to NCU destroy */
            g_signal_emit (self,
              nwamui_daemon_signals[DAEMON_INFO],
              0, /* details */
              g_strdup_printf ("link %s is removed.", idx->data.removed.name));
            break;
        case NWAM_EVENT_TYPE_WLAN_SCAN_REPORT: {
            nwam_events_wlan_t *wlans;
            int i;
            NwamuiWifiNet          *wifi_net = NULL;
            
            wlans = idx->data.wlan_scan.wlans;
            /* TODO - We should list all the wlans from all interfaces */
            g_signal_emit (self,
              nwamui_daemon_signals[DAEMON_INFO],
              0, /* details */
              g_strdup_printf ("got some essids from interface %s.",
                idx->data.wlan_scan.name));
             
            for (i = 0; i < idx->data.wlan_scan.num_wlans; i++) {
                GList *bssid_list = g_list_append( NULL, g_strdup(wlans->bssid) );
                wifi_net = nwamui_wifi_net_new (NULL,
                  wlans->essid,
                  nwamui_wifi_net_security_map (wlans->security_mode),
                  bssid_list,
                  NWAMUI_WIFI_BSS_TYPE_AUTO);
                  
                /* trigger event */
                g_signal_emit (self,
                  nwamui_daemon_signals[WIFI_SCAN_RESULT],
                  0, /* details */
                  wifi_net);
                
                wlans++;
            }
             
            /* Signal End List */
            g_signal_emit (self,
              nwamui_daemon_signals[WIFI_SCAN_RESULT],
              0, /* details */
              NULL);
        }
            break;
        default:
            /* Directly deliver to upper consumers */
            g_signal_emit (self,
              nwamui_daemon_signals[DAEMON_INFO],
              0, /* details */
              g_strdup_printf ("Unknown NWAM event type %d.", idx->type));
            break;
        }
    }
    
    g_object_unref (self);
    
#endif
    return 0;
}

extern void
nwamui_daemon_emit_info_message( NwamuiDaemon* self, const gchar* message )
{
    g_return_if_fail( (NWAMUI_IS_DAEMON(self) && message != NULL ) );

    g_signal_emit (self,
      nwamui_daemon_signals[DAEMON_INFO],
      0, /* details */
      NWAMUI_DAEMON_INFO_UNKNOWN,
      NULL,
      g_strdup(message?message:""));
}

extern void
nwamui_daemon_emit_signals_from_event_msg( NwamuiDaemon* self, NwamuiNcu* ncu, nwam_event_t event )
{
    gchar *vanity_name = NULL;

    g_return_if_fail( self != NULL && ncu != NULL && event != NULL );

    if ( event->type != NWAM_EVENT_TYPE_WLAN_SCAN_REPORT ) {
        g_debug("Tried to emit wifi info from event != NWAM_EVENT_TYPE_WLAN_SCAN_REPORT");
        return;
    }
    
    /* TODO - Implement emit signals from event message */
#if 0
    vanity_name = nwamui_ncu_get_vanity_name( ncu );

    if ( nwamui_ncu_get_ncu_type(ncu) == NWAMUI_NCU_TYPE_WIRELESS ) {
        /* We need to first send events about what Wifi Nets are available,
         * also fills in the wifi_info.  */
        self->prv->emit_wlan_changed_signals = TRUE;
        dispatch_wifi_scan_events_from_cache( self );
    }


    g_assert( vanity_name != NULL );

    g_debug("nwamui_daemon_emit_signals_from_event_msg called");

    if ( llp->llp_link_up  ) {
        g_debug("\tllp_link_up set");
        /* TODO - Not the same as InterfaceUp event, so not sure how to handle
         */
    }

    if ( llp->llp_link_failed  ) {
        g_debug("\tllp_link_failed set");
        g_signal_emit (self,
              nwamui_daemon_signals[DAEMON_INFO],
              0, /* details */
              NWAMUI_DAEMON_INFO_GENERIC,
              NULL,
              g_strdup_printf(_("%s network interface failed to start DHCP request."), vanity_name ));
    }

    if ( llp->llp_dhcp_failed  ) {
        g_debug("\tllp_dhcp_failed set");
        g_signal_emit (self,
              nwamui_daemon_signals[DAEMON_INFO],
              0, /* details */
              NWAMUI_DAEMON_INFO_GENERIC,
              NULL,
              g_strdup_printf(_("%s network interface failed to get response from DHCP request."), vanity_name ));
    }

    if ( llp->llp_need_wlan  ) {
        g_debug("\tllp_need_wlan set");
        g_signal_emit (self,
          nwamui_daemon_signals[WIFI_SELECTION_NEEDED],
          0, /* details */
          ncu );
    }

    if ( llp->llp_need_key  ) {
        g_debug("\tllp_need_key set");
        NwamuiWifiNet *wifi = NULL;

        wifi = nwamui_ncu_get_wifi_info( ncu );

        if ( wifi != NULL ) {
            g_signal_emit (self,
              nwamui_daemon_signals[WIFI_KEY_NEEDED],
              0, /* details */
              (gpointer)wifi );
            g_object_unref(wifi);
        }

        g_object_unref(ncu);
    }

    g_free(vanity_name);

    g_debug("nwamui_daemon_emit_signals_from_event_msg returning");
#endif /* 0 */
}

/*
 * The daemon's state is determined by the following criteria:
 *
 *  if nwamd is not running
 *      set status to ERROR
 *  else
 *      foreach object [ncus in active ncp, env, enm]
 *          if object is an NCU 
 *              if ncu is MANUAL
 *                  if ncu is enabled 
 *                      if state is not ONLINE OR aux_state is not UP
 *                          set status to NEEDS_ATTENTION
 *                      endif
 *                  endif
 *              else if ncu in active priority group
 *                  if ncu is enabled 
 *                      if state is not ONLINE OR aux_state is not UP
 *                          set status to NEEDS_ATTENTION
 *                      endif
 *                  endif
 *              endif
 *          else 
 *              if object is enabled
 *                  if object is ENV
 *                      found_active_env = TRUE
 *                  endif
 *                  if state is not ONLINE OR aux_state is not ACTIVE
 *                      set status to NEEDS_ATTENTION
 *                  endif
 *              endif
 *          endif
 *      end
 *      if not found_active_env
 *          set status to NEEDS_ATTENTION
 *      endif
 *  endif
 */
static void
nwamui_daemon_update_status( NwamuiDaemon   *daemon )
{
    nwamui_daemon_status_t  old_status;
    nwamui_daemon_status_t  new_status = NWAMUI_DAEMON_STATUS_UNINITIALIZED;
    NwamuiDaemonPrivate    *prv;

    /* 
     * Determine status from objects 
     */
    
    if ( daemon == NULL || !NWAMUI_IS_DAEMON(daemon) ) {
        return;
    }

    old_status = daemon->prv->status;
    prv = NWAMUI_DAEMON(daemon)->prv;

    /* First check that the daemon is connected to nwamd */
    if ( !prv->connected_to_nwamd ) {
        new_status = NWAMUI_DAEMON_STATUS_ERROR;
    }
    else {
        /* Now we need to check all objects in the system */

        /* Look at Active NCP and it's NCUs */
        if ( prv->active_ncp == NULL ) {
            new_status = NWAMUI_DAEMON_STATUS_NEEDS_ATTENTION;
        }
        else {
            if ( !nwamui_ncp_all_ncus_online( prv->active_ncp ) ) {
                new_status = NWAMUI_DAEMON_STATUS_NEEDS_ATTENTION;
            }
        }

        /* Assuming we've no found an error yet, check the Locations */
        if ( new_status == NWAMUI_DAEMON_STATUS_UNINITIALIZED ) {
            if ( prv->active_env == NULL ) {
                new_status = NWAMUI_DAEMON_STATUS_NEEDS_ATTENTION;
            }
            else {
                nwam_state_t        state;
                nwam_aux_state_t    aux_state;
                state = nwamui_object_get_nwam_state( NWAMUI_OBJECT(prv->active_env), &aux_state, NULL);

                if ( state != NWAM_STATE_ONLINE || aux_state != NWAM_AUX_STATE_ACTIVE ) {
                    new_status = NWAMUI_DAEMON_STATUS_NEEDS_ATTENTION;
                }
            }
        }

        /* Assuming we've no found an error yet, check the ENMs */
        if ( new_status == NWAMUI_DAEMON_STATUS_UNINITIALIZED ) {
            g_list_foreach( daemon->prv->enm_list, check_enm_online, &new_status ); 
        }
    }

    if ( new_status == NWAMUI_DAEMON_STATUS_UNINITIALIZED ) {
        /* If we got this far with UNINITIALIZED, then we can assume all is OK
         */
        new_status = NWAMUI_DAEMON_STATUS_ALL_OK;
    }

    /* If status has changed, set it, and this will generate an event */
    if ( new_status != old_status ) {
        nwamui_daemon_set_status(daemon, new_status );
    }
}

static void
nwamui_daemon_update_status_from_object_state_event( NwamuiDaemon   *daemon, nwam_event_t nwamevent )
{
    nwamui_daemon_status_t  old_status;
    nwamui_daemon_status_t  new_status = NWAMUI_DAEMON_STATUS_UNINITIALIZED;
    NwamuiDaemonPrivate    *prv;
    nwam_object_type_t      object_type = nwamevent->data.object_state.object_type;
    const char*             object_name = nwamevent->data.object_state.name;
    nwam_state_t            object_state = nwamevent->data.object_state.state;
    nwam_aux_state_t        object_aux_state = nwamevent->data.object_state.aux_state;

    /* 
     * Determine status from objects 
     */
    
    if ( daemon == NULL || !NWAMUI_IS_DAEMON(daemon) ) {
        return;
    }

    old_status = daemon->prv->status;
    prv = NWAMUI_DAEMON(daemon)->prv;

    /* First check that the daemon is connected to nwamd */
    if ( !prv->connected_to_nwamd ) {
        new_status = NWAMUI_DAEMON_STATUS_ERROR;
    }
    else {
        /* Now we need to check all objects in the system */

        switch ( object_type ) {
            case NWAM_OBJECT_TYPE_NCU:
                if ( prv->active_ncp == NULL ) {
                    new_status = NWAMUI_DAEMON_STATUS_NEEDS_ATTENTION;
                }
                else {
                    if ( !nwamui_ncp_all_ncus_online( prv->active_ncp ) ) {
                        new_status = NWAMUI_DAEMON_STATUS_NEEDS_ATTENTION;
                    }
                }
                break;
            case NWAM_OBJECT_TYPE_NCP:
                /* Look at Active NCP and it's NCUs, need to evaluate a change
                 * in NCP or NCU as part of the whole NCP */
                if ( object_state == NWAM_STATE_ONLINE &&
                     object_aux_state == NWAM_AUX_STATE_ACTIVE ) {
                    /* Check active_ncp against this changed object */
                    if ( prv->active_ncp == NULL ) {
                        /* New active NCP */
                        NwamuiNcp* new_ncp;
                        
                        new_ncp = nwamui_daemon_get_ncp_by_name( daemon, object_name );
                        if ( new_ncp ) {
                            nwamui_daemon_set_active_ncp( daemon, new_ncp );
                            g_object_unref(new_ncp);
                        }
                    }
                    else {
                        gchar*  active_name = nwamui_ncp_get_name(prv->active_ncp);
                        if ( strncmp( active_name, object_name, strlen(object_name)) == 0 ) {
                            /* Same, so don't change anything */
                        }
                        else {
                            /* Different, so change active_ncp */
                            NwamuiNcp* new_ncp;
                            
                            new_ncp = nwamui_daemon_get_ncp_by_name( daemon, object_name );
                            if ( new_ncp ) {
                                nwamui_daemon_set_active_ncp( daemon, new_ncp );
                                g_object_unref(new_ncp);
                            }
                        }
                    }
                }

                if ( prv->active_ncp == NULL ) {
                    new_status = NWAMUI_DAEMON_STATUS_NEEDS_ATTENTION;
                }
                else {
                    if ( !nwamui_ncp_all_ncus_online( prv->active_ncp ) ) {
                        new_status = NWAMUI_DAEMON_STATUS_NEEDS_ATTENTION;
                    }
                }
                break;
            case NWAM_OBJECT_TYPE_LOC:

                if ( object_state == NWAM_STATE_ONLINE &&
                     object_aux_state == NWAM_AUX_STATE_ACTIVE ) {

                    /* Ensure that correct active env pointer */
                    if ( prv->active_env == NULL ) {
                        /* New active ENV */
                        NwamuiEnv* new_env;
                        
                        new_env = nwamui_daemon_get_env_by_name( daemon, object_name );
                        if ( new_env ) {
                            nwamui_daemon_set_active_env( daemon, new_env );
                            g_object_unref(new_env);
                        }
                    }
                    else {
                        gchar*  active_name = nwamui_env_get_name(prv->active_env);
                        if ( strncmp( active_name, object_name, strlen(object_name)) == 0 ) {
                            /* Same, so don't change anything */
                        }
                        else {
                            /* Different, so change active_env */
                            NwamuiEnv* new_env;
                            
                            new_env = nwamui_daemon_get_env_by_name( daemon, object_name );
                            if ( new_env ) {
                                nwamui_daemon_set_active_env( daemon, new_env );
                                g_object_unref(new_env);
                            }
                        }
                    }
                }

                if ( prv->active_env == NULL ) {
                    new_status = NWAMUI_DAEMON_STATUS_NEEDS_ATTENTION;
                }
                else {
                    if ( object_state != NWAM_STATE_ONLINE || object_aux_state != NWAM_AUX_STATE_ACTIVE ) {
                        new_status = NWAMUI_DAEMON_STATUS_NEEDS_ATTENTION;
                    }
                }
                break;

            case NWAM_OBJECT_TYPE_ENM: {
                    NwamuiEnm* enm;

                    enm = nwamui_daemon_get_enm_by_name( daemon, object_name );
                    if ( enm ) {
                        if ( nwamui_enm_get_enabled( NWAMUI_ENM(enm) ) ) {
                            if ( object_state != NWAM_STATE_ONLINE || object_aux_state != NWAM_AUX_STATE_ACTIVE ) {
                                new_status = NWAMUI_DAEMON_STATUS_NEEDS_ATTENTION;
                            }
                        }
                    }
                }
                break;
            case NWAM_OBJECT_TYPE_KNOWN_WLAN:
                /* fall-through */
            case NWAM_OBJECT_TYPE_NONE:
                /* fall-through */
            default:
                /* SKip */
                break;
        }

    }

    if ( new_status == NWAMUI_DAEMON_STATUS_UNINITIALIZED ) {
        /* If we got this far with UNINITIALIZED, then we can assume all is OK
         */
        new_status = NWAMUI_DAEMON_STATUS_ALL_OK;
    }

    /* If status has changed, set it, and this will generate an event */
    if ( new_status != old_status ) {
        nwamui_daemon_set_status(daemon, new_status );
    }
}

static void
check_enm_online( gpointer obj, gpointer user_data )
{
    NwamuiObject            *nobj = NWAMUI_OBJECT(obj);
    nwamui_daemon_status_t  *new_status_p = (nwamui_daemon_status_t*)user_data; 
    nwam_state_t             state;
    nwam_aux_state_t         aux_state;

    if ( nwamui_enm_get_enabled( NWAMUI_ENM(nobj) ) ) {
        state = nwamui_object_get_nwam_state( nobj, &aux_state, NULL);

        if ( state != NWAM_STATE_ONLINE || aux_state != NWAM_AUX_STATE_ACTIVE ) {
            *new_status_p = NWAMUI_DAEMON_STATUS_NEEDS_ATTENTION;
        }
    }
}


extern nwamui_env_status_t 
nwamui_daemon_get_status_icon_type( NwamuiDaemon *daemon )
{
    nwamui_env_status_t env_status = NWAMUI_ENV_STATUS_ERROR;

    if ( daemon != NULL ) {
        if ( daemon->prv->status == NWAMUI_DAEMON_STATUS_UNINITIALIZED ) {
            /* Early call, so try to update it's value now */
            nwamui_daemon_update_status( daemon );
        }

        if ( daemon->prv->status == NWAMUI_DAEMON_STATUS_ALL_OK ) {
            env_status = NWAMUI_ENV_STATUS_CONNECTED;
        }
        else if ( daemon->prv->status == NWAMUI_DAEMON_STATUS_NEEDS_ATTENTION ) {
            env_status = NWAMUI_ENV_STATUS_WARNING;
        }
        else {
            /* Fall through to return, with error icon */
        }
    }

    return( env_status );
}

static gboolean
nwamui_daemon_nwam_connect( gboolean block )
{
    gboolean        rval = FALSE;
    gboolean        not_done;
    nwam_error_t    nerr;

    g_static_mutex_lock (&nwam_event_mutex);
    nwam_init_done = rval;
    g_static_mutex_unlock (&nwam_event_mutex);

    do {
        if (strcmp(smf_get_state(NWAM_FMRI), SCF_STATE_STRING_ONLINE) != 0) {
            g_debug("%s: NWAM service appears to be off-line", __func__);
        }
        else if ( (nerr = nwam_events_init()) == NWAM_SUCCESS) {
            g_debug("%s: Connected to nwam daemon", __func__);
            rval = TRUE;
        }
        else {
            g_debug("%s: nwam_events_init() returned %d (%s)", 
                    __func__, nerr, nwam_strerror(nerr) );
        }

        not_done = (block && !rval );
        if ( not_done ) {
            g_debug("%s: sleeping...", __func__);
            sleep( 5 );
        }
    } while ( not_done );

    g_static_mutex_lock (&nwam_event_mutex);
    nwam_init_done = rval;
    g_static_mutex_unlock (&nwam_event_mutex);

    return( rval );
}

static void
nwamui_daemon_nwam_disconnect( void )
{
    gboolean _init_done;

    g_static_mutex_lock (&nwam_event_mutex);
    _init_done = nwam_init_done;
    g_static_mutex_unlock (&nwam_event_mutex);

    if ( _init_done ) {
        g_debug( "%s: Closing connection to nwam daemon", __func__ );
        (void) nwam_events_fini();
    }
}


/**
 * nwam_events_thread:
 *
 * This callback is needed to be MT safe.
 */
static gpointer
nwam_events_thread ( gpointer data )
{
    NwamuiDaemon           *daemon = NWAMUI_DAEMON( data );
	nwam_event_t            nwamevent = NULL;
    nwam_error_t            err;

    g_debug ("nwam_events_thread");
    
	while (event_thread_running()) {
        if ( (err = nwam_event_wait( &nwamevent)) != NWAM_SUCCESS ) {
			g_debug("Event wait error: %s", nwam_strerror(err));

            /* Send event to tell UI there was an error */
            g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
              nwamd_event_handler,
              (gpointer) nwamui_event_new(daemon, NWAMUI_DAEMON_INFO_ERROR, NULL),
              (GDestroyNotify) nwamui_event_free);
              
            if ( ! event_thread_running() ) {
                /* If we were waiting for an event and we got an error, make
                 * sure it wasn't intentional, to cause this thread to exit
                 */
                continue;
            }
			if (err != NWAM_SUCCESS ) {
				g_debug("Attempting to reopen connection to daemon");
                nwamui_daemon_nwam_disconnect();
                if ( nwamui_daemon_nwam_connect( TRUE ) ) {
                    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                      nwamd_event_handler,
                      (gpointer) nwamui_event_new(daemon, NWAMUI_DAEMON_INFO_ACTIVE, NULL),
                      (GDestroyNotify) nwamui_event_free);

					continue;
                }
                else {
                    g_debug("nwam_event_wait() error: %s", nwam_strerror(err));
                }
			}
			/* Rather than just breaking out here, it is better if we sleep
             * and then retry things again so long as event_thread_running()
             * is TRUE.
             */
            sleep(1);
            continue;
		}
        
        g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
          nwamd_event_handler,
          (gpointer) nwamui_event_new(daemon, NWAMUI_DAEMON_INFO_RAW, nwamevent),
          (GDestroyNotify) nwamui_event_free);
    }
    
    g_object_unref (daemon);

    g_debug("Exiting event thread");

    return NULL;
}


/* Default Signal Handlers */
static void
default_status_changed_signal_handler(NwamuiDaemon *self, nwamui_daemon_status_t status, gpointer user_data)
{
    const char* status_str;

    switch( status) {
        case NWAMUI_DAEMON_STATUS_UNINITIALIZED:
            status_str="UNINITIALIZED";
            break;
        case NWAMUI_DAEMON_STATUS_ALL_OK:
            status_str="ALL_OK";
            break;
        case NWAMUI_DAEMON_STATUS_NEEDS_ATTENTION:
            status_str="NEEDS_ATTENTION";
            break;
        case NWAMUI_DAEMON_STATUS_ERROR:
            status_str="ERROR";
        break;
        default:
            status_str="Unexpected";
            break;
    }

    g_debug("Daemon status changed to %s", status_str );
}

static void
default_wifi_selection_needed (NwamuiDaemon *self, NwamuiNcu* ncu, gpointer user_data)
{
    gchar *dispname = NULL;
    g_debug("Wireless selection needed for network interface '%s'", (dispname = nwamui_ncu_get_display_name(ncu)) );
    g_free(dispname);
}

static void
default_wifi_key_needed (NwamuiDaemon *self, NwamuiWifiNet* wifi, gpointer user_data)
{
    if ( wifi ) {
        char *essid = nwamui_wifi_net_get_essid( wifi );

        g_debug("Wireless key needed for network '%s'", essid?essid:"???UNKNOWN ESSID???" );
    }
}


static void
default_wifi_scan_result_signal_handler (NwamuiDaemon *self, NwamuiWifiNet* wifi_net, gpointer user_data)
{
    if ( wifi_net == NULL ) {
        /* End of Scan */
        g_debug("Scan Result : End Of Scan");
    }
    else {
        gchar *name;

        name = nwamui_wifi_net_get_essid( wifi_net );
        g_debug("Scan Result : Got ESSID\t\"%s\"", name);
        g_free (name);
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
default_active_ncp_changed_signal_handler (NwamuiDaemon *self, GObject* data, gpointer user_data)
{
    NwamuiNcp*  ncp = NWAMUI_NCP(data);
    gchar*      name = nwamui_ncp_get_name(ncp);
    
    g_debug("Active Ncp Changed to %s", name );
    
    g_free(name);
}


static void 
default_ncu_up_signal_handler (NwamuiDaemon *self, NwamuiNcu* ncu, gpointer user_data)
{
	g_debug("NCU %s up", nwamui_ncu_get_display_name(ncu));
	
    /* TODO - Send this message to UI, and also NCU  */
}

static void 
default_ncu_down_signal_handler (NwamuiDaemon *self, NwamuiNcu* ncu, gpointer user_data)
{
	g_debug("NCU %s down", nwamui_ncu_get_display_name(ncu));
    /* TODO - Send this message to UI, and also NCU  */
}

static void
default_daemon_info_signal_handler(NwamuiDaemon *self, gint type, GObject *obj, gpointer data, gpointer user_data)
{
    gchar *msg = (gchar *) data;
    
	g_debug ("Daemon got information type %4d msg %s.", type, msg);
	
	g_free (msg);
}

static void
default_add_wifi_fav_signal_handler (NwamuiDaemon *self, NwamuiWifiNet* new_wifi, gpointer user_data)
{
}

static void
default_remove_wifi_fav_signal_handler (NwamuiDaemon *self, NwamuiWifiNet* new_wifi, gpointer user_data)
{
}

/* walkers */
static int
nwam_loc_walker_cb (nwam_loc_handle_t env, void *data)
{
    char *name;
    nwam_error_t nerr;
    NwamuiEnv* new_env;
        
    NwamuiDaemonPrivate *prv = NWAMUI_DAEMON(data)->prv;

    g_debug ("nwam_loc_walker_cb 0x%p", env);
    
    new_env = nwamui_env_new_with_handle (env);
        
    if ( nwamui_env_get_active( new_env ) ) {
        prv->active_env = NWAMUI_ENV(g_object_ref(new_env));
    }

    prv->env_list = g_list_append(prv->env_list, (gpointer)new_env);

    return(0);
}

static int
nwam_enm_walker_cb (nwam_enm_handle_t enm, void *data)
{
    char *name;
    nwam_error_t nerr;
    NwamuiEnm* new_enm;
    
    NwamuiDaemonPrivate *prv = NWAMUI_DAEMON(data)->prv;

    g_debug ("nwam_enm_walker_cb 0x%p", enm);
    
    new_enm = nwamui_enm_new_with_handle (enm);
        
    prv->enm_list = g_list_append(prv->enm_list, (gpointer)new_enm);

    return(0);
}

static int
nwam_ncp_walker_cb (nwam_ncp_handle_t ncp, void *data)
{
    char           *name;
    nwam_error_t    nerr;
    NwamuiNcp      *new_ncp;
    
    NwamuiDaemonPrivate *prv = NWAMUI_DAEMON(data)->prv;

    g_debug ("nwam_ncp_walker_cb 0x%p", ncp);
    
    new_ncp = nwamui_ncp_new_with_handle (ncp);

    if ( nwamui_ncp_is_active( new_ncp ) ) {
        prv->active_ncp = NWAMUI_NCP(g_object_ref( new_ncp ));
    }
        
    prv->ncp_list = g_list_append(prv->ncp_list, (gpointer)new_ncp);


    return(0);
}

static int
nwam_known_wlan_walker_cb (nwam_known_wlan_handle_t wlan_h, void *data)
{
    nwam_error_t nerr;
    GList**        glist_p = (GList**)data;
    NwamuiWifiNet* wifi = NULL;
        
    g_debug ("nwam_known_wlan_walker_cb 0x%p", wlan_h );
    
    wifi = nwamui_wifi_net_new_with_handle( NULL, wlan_h );
        
    (*glist_p) = g_list_append((*glist_p), (gpointer)wifi );

    return(0);
}

