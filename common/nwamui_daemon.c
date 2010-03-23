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

#define DEBUG_STATUS( name, state, aux_state, status_flag )             \
    nwamui_debug("line: %d : name = %s : state = %d (%s) : aux_state = %d (%s) status_flag: %02x", \
      __LINE__, name,                                                   \
      (state), nwam_state_to_string(state),                             \
      (aux_state), nwam_aux_state_to_string(aux_state),                 \
      status_flag)

typedef struct _to_emit {
    guint       signal_id;
    GQuark      detaiil;
    gpointer    param1;
} to_emit_t;

enum {
    DAEMON_INFO,
    WIFI_KEY_NEEDED,
    WIFI_SELECTION_NEEDED,
    S_WIFI_FAV_ADD,
    S_WIFI_FAV_REMOVE,
    LAST_SIGNAL
};

enum {
    PROP_ACTIVE_ENV = 1,
    PROP_ACTIVE_NCP,
    PROP_NCP_LIST,
    PROP_ENV_LIST,
    PROP_ENM_LIST,
    PROP_WIFI_FAV,
    PROP_STATUS,
    PROP_NUM_SCANNED_WIFI,
    PROP_ONLINE_ENM_NUM,
    PROP_ENV_SELECTION_MODE
};

/* Flags to track various reasons for a state not being ALL_OK
 *
 * The purpose of this is to allow us know if we're still in a broken state after
 * an object_state event might cause something to seem ALL_OK.
 */
enum {
    STATUS_REASON_DAEMON = 0x01,
    STATUS_REASON_NCP    = 0x02, /* Includes NCUs */
    STATUS_REASON_LOC    = 0x04,
    STATUS_REASON_ENM    = 0x08,
};

enum {
	MANAGED_NCP,
	MANAGED_LOC,
	MANAGED_ENM,
	MANAGED_FAV_WIFI,
	N_MANAGED
};

static guint nwamui_daemon_signals[LAST_SIGNAL] = { 0, 0 };

struct _NwamuiDaemonPrivate {
    NwamuiObject *active_env;
    NwamuiObject *active_ncp;
    NwamuiNcp    *auto_ncp;     /* For quick access */
    
    GList       *managed_list[N_MANAGED];

    GList       *temp_list; /* Used to temporarily track not found objects in walkers */

    /* others */
    gboolean                connected_to_nwamd;
    nwamui_daemon_status_t  status;
    guint                   status_flags; 
    GThread                *nwam_events_gthread;
    gboolean                communicate_change_to_daemon;
    guint                   wep_timeout_id;
    GQueue                 *wlan_scan_queue;
    gint                    num_scanned_wifi;
    gint                    online_enm_num;
};

#define NWAMUI_DAEMON_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE((o), NWAMUI_TYPE_DAEMON, NwamuiDaemonPrivate))

typedef struct _fav_wlan_walker_data {
    NwamuiNcu  *wireless_ncu;
    GList      *fav_list;
} fav_wlan_walker_data_t;

typedef struct _NwamuiEvent {
    nwamui_daemon_info_t e;     /* ui daemon event type */
    nwam_event_t         nwamevent; /* daemon data */
    NwamuiDaemon*        daemon;
} NwamuiEvent;

static void nwamui_daemon_init_lists (NwamuiDaemon *self);

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

static void nwamui_daemon_set_num_scanned_wifi(NwamuiDaemon* self,  gint num_scanned_wifi);

static void nwamui_daemon_finalize (     NwamuiDaemon *self);

static void nwamui_daemon_populate_wifi_fav_list(NwamuiDaemon *self);

static void check_nwamui_object_online( gpointer obj, gpointer user_data );
static void nwamui_daemon_update_online_enm_num(NwamuiDaemon *self);
static void check_nwamui_object_online_num( gpointer obj, gpointer user_data );

static void nwamui_daemon_update_status( NwamuiDaemon   *daemon );

static gboolean nwamui_daemon_nwam_connect( gboolean block );
static void     nwamui_daemon_nwam_disconnect( void );

static void nwamui_daemon_handle_object_action_event( NwamuiDaemon   *daemon, nwam_event_t nwamevent );
static void nwamui_daemon_handle_object_state_event( NwamuiDaemon   *daemon, nwam_event_t nwamevent );
static void nwamui_daemon_set_status( NwamuiDaemon* self, nwamui_daemon_status_t status );
static gboolean nwamui_object_real_commit(NwamuiObject *object);

/* Callbacks */

/* Default Callback Handlers */
static void default_add_wifi_fav_signal_handler (NwamuiDaemon *self, NwamuiWifiNet* new_wifi, gpointer user_data);
static void default_daemon_info_signal_handler (NwamuiDaemon *self, gint type, GObject *obj, gpointer data, gpointer user_data);
static void default_remove_wifi_fav_signal_handler (NwamuiDaemon *self, NwamuiWifiNet* new_wifi, gpointer user_data);
static void default_wifi_key_needed (NwamuiDaemon *self, NwamuiWifiNet* wifi, gpointer user_data);
static void default_wifi_scan_started_signal_handler (NwamuiDaemon *self, gpointer user_data);
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
    NwamuiObjectClass *nwamuiobject_class = NWAMUI_OBJECT_CLASS(klass);
        
    /* Override Some Function Pointers */
    gobject_class->set_property = nwamui_daemon_set_property;
    gobject_class->get_property = nwamui_daemon_get_property;
    gobject_class->finalize = (void (*)(GObject*)) nwamui_daemon_finalize;

    nwamuiobject_class->commit = nwamui_object_real_commit;

	g_type_class_add_private(klass, sizeof(NwamuiDaemonPrivate));

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
        G_PARAM_READABLE));

    g_object_class_install_property (gobject_class,
                                     PROP_NCP_LIST,
                                     g_param_spec_pointer ("ncp_list",
                                                          _("GList of NCPs"),
                                                          _("GList of NCPs"),
                                                           G_PARAM_READABLE));

    g_object_class_install_property (gobject_class,
                                     PROP_ENV_LIST,
                                     g_param_spec_pointer ("env_list",
                                                          _("GList of ENVs"),
                                                          _("GList of ENVs"),
                                                           G_PARAM_READABLE));

    g_object_class_install_property (gobject_class,
                                     PROP_ENM_LIST,
                                     g_param_spec_pointer ("enm_list",
                                                          _("GList of ENMs"),
                                                          _("GList of ENMs"),
                                                           G_PARAM_READABLE));

    g_object_class_install_property (gobject_class,
                                     PROP_STATUS,
                                     g_param_spec_int   ("status",
                                                         _("status"),
                                                         _("status"),
                                                          NWAMUI_DAEMON_STATUS_UNINITIALIZED,
                                                          NWAMUI_DAEMON_STATUS_LAST-1,
                                                          NWAMUI_DAEMON_STATUS_UNINITIALIZED,
                                                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    g_object_class_install_property (gobject_class,
                                     PROP_NUM_SCANNED_WIFI,
                                     g_param_spec_int   ("num_scanned_wifi",
                                                         _("num_scanned_wifi"),
                                                         _("num_scanned_wifi"),
                                                          0,
                                                          G_MAXINT,
                                                          0,
                                                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    g_object_class_install_property (gobject_class,
                                     PROP_WIFI_FAV,
                                     g_param_spec_pointer("wifi_fav",
                                                         _("wifi_fav"),
                                                         _("wifi_fav"),
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_ENV_SELECTION_MODE,
                                     g_param_spec_boolean("env_selection_mode",
                                                         _("env_selection_mode"),
                                                         _("env_selection_mode"),
                                                          FALSE,
                                                          G_PARAM_READABLE));

    g_object_class_install_property (gobject_class,
      PROP_ONLINE_ENM_NUM,
      g_param_spec_int("online_enm_num",
        _("online_enm_num"),
        _("online_enm_num"),
        0,
        G_MAXINT,
        0,
        G_PARAM_READABLE));

    /* Create some signals */
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
                  
    nwamui_daemon_signals[DAEMON_INFO] =
      g_signal_new ("daemon_info",
        G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_CLEANUP | G_SIGNAL_ACTION,
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

    klass->wifi_key_needed = default_wifi_key_needed;
    klass->wifi_selection_needed = default_wifi_selection_needed;
    klass->wifi_scan_started = default_wifi_scan_started_signal_handler;
    klass->wifi_scan_result = default_wifi_scan_result_signal_handler;
    klass->daemon_info = default_daemon_info_signal_handler;
    klass->add_wifi_fav = default_add_wifi_fav_signal_handler;
    klass->remove_wifi_fav = default_remove_wifi_fav_signal_handler;
}

static void
nwamui_daemon_init (NwamuiDaemon *self)
{
    NwamuiDaemonPrivate *prv      = NWAMUI_DAEMON_GET_PRIVATE(self);
    GError              *error    = NULL;
    NwamuiWifiNet       *wifi_net = NULL;
    NwamuiEnm           *new_enm  = NULL;
    nwam_error_t         nerr;
    NwamuiNcp*           user_ncp;
    
    self->prv = prv;
    
    prv->communicate_change_to_daemon = TRUE;

    prv->nwam_events_gthread = g_thread_create(nwam_events_thread, g_object_ref(self), TRUE, &error);
    if( prv->nwam_events_gthread == NULL ) {
        g_debug("Error creating nwam events thread: %s", (error && error->message)?error->message:"" );
    }

    /* nwam_events_thread include the initial function call, so this is
     * dup. Note capplet require this dup call because capplet doesn't handle
     * daemon related info changes, e.g. NCP list changes, so it may lose info
     * with this dup call.
     */
    nwamui_daemon_init_lists ( self );
}

/**
 * Do not populate fav/ wlan in this function call, because this function will
 * be invoked two times at initilly time.
 */
static void
nwamui_daemon_init_lists(NwamuiDaemon *self)
{
    GError                 *error = NULL;
    NwamuiWifiNet          *wifi_net = NULL;
    NwamuiEnm              *new_enm = NULL;
    nwam_error_t            nerr;
    NwamuiNcp*              user_ncp;
    
    /* NCPs */

    /* Get list of Ncps from libnwam */
    self->prv->temp_list = g_list_copy( self->prv->managed_list[MANAGED_NCP] ); /* not worried about refs here */
    {
        nwam_error_t nerr;
        int cbret;
        nerr = nwam_walk_ncps (nwam_ncp_walker_cb, (void *)self, 0, &cbret);
        if (nerr != NWAM_SUCCESS) {
            g_debug ("[libnwam] nwam_walk_ncps %s", nwam_strerror (nerr));
        }
    }
    if ( self->prv->temp_list != NULL ) {
        for( GList* item = g_list_first( self->prv->temp_list ); 
             item != NULL;
             item = g_list_next( item ) ) {
            nwamui_daemon_remove_object( self, NWAMUI_OBJECT(item->data));
        }
    }

    /* Env / Locations */
    self->prv->temp_list = g_list_copy( self->prv->managed_list[MANAGED_LOC] ); /* not worried about refs here */
    {
        nwam_error_t nerr;
        int cbret;

        nerr = nwam_walk_locs (nwam_loc_walker_cb, (void *)self, 0, &cbret);
        if (nerr != NWAM_SUCCESS) {
            g_debug ("[libnwam] nwam_walk_locs %s", nwam_strerror (nerr));
        }
    }
    if ( self->prv->active_env == NULL && self->prv->managed_list[MANAGED_LOC]  != NULL ) {
        GList* first_element = g_list_first( self->prv->managed_list[MANAGED_LOC] );


        if ( first_element != NULL && first_element->data != NULL )  {
            self->prv->active_env = NWAMUI_OBJECT(g_object_ref(G_OBJECT(first_element->data)));
        }
        
    }
    if ( self->prv->temp_list != NULL ) {
        for( GList* item = g_list_first( self->prv->temp_list ); 
             item != NULL;
             item = g_list_next( item ) ) {
            nwamui_daemon_remove_object( self, NWAMUI_OBJECT(item->data));
        }
    }

    /* ENMs */
    self->prv->temp_list = g_list_copy( self->prv->managed_list[MANAGED_ENM] ); /* not worried about refs here */
    {
        nwam_error_t nerr;
        int cbret;
        nerr = nwam_walk_enms (nwam_enm_walker_cb, (void *)self, 0, &cbret);
        if (nerr != NWAM_SUCCESS) {
            g_debug ("[libnwam] nwam_walk_enms %s", nwam_strerror (nerr));
        }
    }
    if ( self->prv->temp_list != NULL ) {
        for( GList* item = g_list_first( self->prv->temp_list ); 
             item != NULL;
             item = g_list_next( item ) ) {
            nwamui_daemon_remove_object( self, NWAMUI_OBJECT(item->data));
        }
    }

    /* Will generate an event if status changes */
    nwamui_daemon_update_status(self);

    nwamui_daemon_update_online_enm_num(self);

    /* Populate Wifi Favourites List */
    nwamui_daemon_populate_wifi_fav_list(self);
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

    switch (prop_id) {
        case PROP_ACTIVE_ENV: {
            NwamuiObject *env;
            
            if ( self->prv->active_env != NULL ) {
                g_object_unref(G_OBJECT(self->prv->active_env));
            }

            env = NWAMUI_OBJECT(g_value_dup_object( value ));
            g_assert(NWAMUI_IS_ENV(env));

            /* Always send out this signal. To set active env should
             * call nwamui_env_activate (env) directly, then if it
             * is successful event handler will update this prop.
             * This should be a private function instead of a public prop.
             */
            self->prv->active_env = env;
            g_object_notify(G_OBJECT(self), "active_env");
            /* Comment out this line because when a loc is enabled, we will get
             * an event first, when handling the event, we will notify this
             * signal.
             */
            /* g_object_notify(G_OBJECT(self), "env_selection_mode"); */
        }
            break;
        case PROP_STATUS:
            nwamui_daemon_set_status(self, g_value_get_int( value ));
            break;
        case PROP_NUM_SCANNED_WIFI: {
                self->prv->num_scanned_wifi = g_value_get_int(value);
            }
            break;
        case PROP_WIFI_FAV:
            nwamui_daemon_set_fav_wifi_networks( self, (GList*)g_value_get_pointer(value));
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
        case PROP_NCP_LIST: {
                g_value_set_pointer( value, nwamui_util_copy_obj_list( self->prv->managed_list[MANAGED_NCP] ) );
            }
            break;
        case PROP_STATUS:
            g_value_set_int(value, nwamui_daemon_get_status(self));
            break;
        case PROP_WIFI_FAV:
            g_value_set_pointer( value, nwamui_util_copy_obj_list(self->prv->managed_list[MANAGED_FAV_WIFI]));
            break;
        case PROP_NUM_SCANNED_WIFI: {
                g_value_set_int(value, self->prv->num_scanned_wifi);
            }
            break;
        case PROP_ONLINE_ENM_NUM:
            g_value_set_int(value, nwamui_daemon_get_online_enm_num(self));
            break;
        case PROP_ENV_SELECTION_MODE: {
                gboolean is_manual;

                is_manual = nwamui_daemon_env_selection_is_manual( self );

                g_value_set_boolean(value, is_manual );
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
    
    nwamui_daemon_nwam_disconnect();

    if ( self->prv->nwam_events_gthread != NULL ) {
        nwamui_daemon_terminate_event_thread( self );
    }
    
    if (self->prv->active_env != NULL ) {
        g_object_unref( G_OBJECT(self->prv->active_env) );
    }

    if ( self->prv->managed_list[MANAGED_FAV_WIFI] != NULL ) {
        g_list_foreach(self->prv->managed_list[MANAGED_FAV_WIFI], nwamui_util_obj_unref, NULL );
        g_list_free( self->prv->managed_list[MANAGED_FAV_WIFI] );
    }
    
    g_list_foreach( self->prv->managed_list[MANAGED_LOC], nwamui_util_obj_unref, NULL ); /* Unref all objects in list */
    g_list_free(self->prv->managed_list[MANAGED_LOC]);
    
    g_list_foreach( self->prv->managed_list[MANAGED_ENM], nwamui_util_obj_unref, NULL ); /* Unref all objects in list */
    g_list_free(self->prv->managed_list[MANAGED_ENM]);
    
    g_list_foreach( self->prv->managed_list[MANAGED_NCP], nwamui_util_obj_unref, NULL ); /* Unref all objects in list */
    g_list_free(self->prv->managed_list[MANAGED_NCP]);
    
    self->prv = NULL;

	G_OBJECT_CLASS(nwamui_daemon_parent_class)->finalize(G_OBJECT(self));
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
nwamui_daemon_get_status( NwamuiDaemon* self )
{
    g_return_val_if_fail(NWAMUI_IS_DAEMON(self), NWAMUI_DAEMON_STATUS_UNINITIALIZED);

    return self->prv->status;
}

/**
 * nwamui_daemon_set_status:
 * @status: nwamui_daemon_status_t
 *
 * Sets the status of the daemon to be <i>status</i>
 *
 **/
static void
nwamui_daemon_set_status( NwamuiDaemon* self, nwamui_daemon_status_t status )
{
    g_return_if_fail (NWAMUI_IS_DAEMON (self));

    if ( self->prv->status != status ) {
        self->prv->status = status;
        nwamui_debug("Daemon status changed to %s", 
          nwamui_deamon_status_to_string(self->prv->status));
        g_object_notify(G_OBJECT(self), "status");
    }
}

static void
foreach_wireless_trigger_scan(gpointer data, gpointer user_data)
{
	NwamuiNcu   *ncu = data;

    g_return_if_fail(NWAMUI_IS_NCU(ncu));

	if (nwamui_ncu_get_ncu_type (ncu) == NWAMUI_NCU_TYPE_WIRELESS) {
        const gchar *name = nwamui_object_get_name(NWAMUI_OBJECT(ncu));
        nwamui_debug("calling nwam_wlan_scan (%s)", name);
        nwam_wlan_scan(name);
	}
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
    NwamuiDaemonPrivate *prv      = NWAMUI_DAEMON_GET_PRIVATE(self);

    g_debug("Wireless Scan initiated");

    if (prv->active_ncp != NULL) {
        nwamui_ncp_foreach_ncu(NWAMUI_NCP(prv->active_ncp), foreach_wireless_trigger_scan, NULL);
    }
}

typedef struct {
    const gchar    *essid;
    NwamuiWifiNet  *wifi_net;
} find_wifi_net_info_t;

static gint
find_wifi_net(gconstpointer data, gconstpointer user_data)
{
	NwamuiNcu              *ncu = (NwamuiNcu *) data;
    NwamuiWifiNet          *wifi = NULL;
    find_wifi_net_info_t   *find_info_p = (find_wifi_net_info_t*)user_data;
	
    if ( ncu == NULL || !NWAMUI_IS_NCU(ncu) || find_info_p == NULL ) {
        return 1;
    }
    
	if (nwamui_ncu_get_ncu_type (ncu) != NWAMUI_NCU_TYPE_WIRELESS ) {
        return 1;
	}
    
    wifi = nwamui_ncu_wifi_hash_lookup_by_essid( ncu, find_info_p->essid );

    if ( wifi != NULL ) {
        find_info_p->wifi_net = wifi;
        return 0;
    }
    return 1;
}

/**
 * nwamui_daemon_find_wifi_net_in_ncus:
 *
 * Searches for a wireless network by essid in the ncus on the system.
 *
 **/
static NwamuiWifiNet*
nwamui_daemon_find_wifi_net_in_ncus( NwamuiDaemon *self, const gchar* essid )
{
    NwamuiDaemonPrivate  *prv = NWAMUI_DAEMON_GET_PRIVATE(self);
    find_wifi_net_info_t  find_info;
    
    if (prv->active_ncp != NULL) {
        find_info.essid = essid;
        find_info.wifi_net = NULL;

        nwamui_ncp_find_ncu(NWAMUI_NCP(prv->active_ncp), find_wifi_net, (gpointer)&find_info );
    }

    return( find_info.wifi_net );
}

/**
 * nwamui_daemon_get_ncp_by_name:
 * @self: NwamuiDaemon*
 * @name: String
 *
 * @returns: NwamuiObject reference
 *
 * Looks up an NCP by name
 *
 **/
extern NwamuiObject*
nwamui_daemon_get_ncp_by_name( NwamuiDaemon *self, const gchar* name )
{
    GString       *have_names = NULL;
    NwamuiObject  *ncp        = NULL;

    g_return_val_if_fail( NWAMUI_IS_DAEMON(self) && name != NULL, ncp );

    have_names = g_string_new("");
    g_string_append_printf(have_names, "daemon -> find ncp %s in [ ", name);

    for( GList* item = g_list_first( self->prv->managed_list[MANAGED_NCP] ); 
         item != NULL;
         item = g_list_next( item ) ) {
        if ( item->data != NULL && NWAMUI_IS_NCP(item->data) ) {
            NwamuiObject* tmp_ncp  = NWAMUI_OBJECT(item->data);
            const gchar*  ncp_name = nwamui_object_get_name(tmp_ncp);

            g_string_append_printf(have_names, "%s(0x%p) ", ncp_name, tmp_ncp);

            if ( strcmp( name, ncp_name) == 0 ) {
                ncp = NWAMUI_OBJECT(g_object_ref(tmp_ncp));
                break;
            }
        }
    }
    g_string_append_printf(have_names, "] %s", ncp ? "OK" : "FAILD");
    g_debug("%s", have_names->str);
    g_string_free(have_names, TRUE);

    return(ncp);
}

/**
 * nwamui_daemon_get_env_by_name:
 * @self: NwamuiDaemon*
 * @name: String
 *
 * @returns: NwamuiObject reference
 *
 * Looks up an ENV by name
 *
 **/
extern NwamuiObject*
nwamui_daemon_get_env_by_name( NwamuiDaemon *self, const gchar* name )
{
    NwamuiObject*  env = NULL;

    g_assert( NWAMUI_IS_DAEMON( self ) );

    g_return_val_if_fail( NWAMUI_IS_DAEMON(self) && name != NULL, env );

    for( GList* item = g_list_first( self->prv->managed_list[MANAGED_LOC] ); 
         item != NULL;
         item = g_list_next( item ) ) {
        if ( item->data != NULL && NWAMUI_IS_ENV(item->data) ) {
            NwamuiObject* tmp_env = NWAMUI_OBJECT(item->data);
            const gchar*     env_name = nwamui_object_get_name(tmp_env);

            if ( strcmp( name, env_name) == 0 ) {
                env = NWAMUI_OBJECT(g_object_ref(G_OBJECT(tmp_env)));
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
 * @returns: NwamuiObject reference
 *
 * Looks up an ENM by name
 *
 **/
extern NwamuiObject*
nwamui_daemon_get_enm_by_name( NwamuiDaemon *self, const gchar* name )
{
    NwamuiObject*  enm = NULL;

    g_assert( NWAMUI_IS_DAEMON( self ) );

    g_return_val_if_fail( NWAMUI_IS_DAEMON(self) && name != NULL, enm );

    for( GList* item = g_list_first( self->prv->managed_list[MANAGED_ENM] ); 
         item != NULL;
         item = g_list_next( item ) ) {
        if ( item->data != NULL && NWAMUI_IS_ENM(item->data) ) {
            NwamuiObject* tmp_enm  = NWAMUI_OBJECT(item->data);
            const gchar*  enm_name = nwamui_object_get_name(tmp_enm);

            if ( strcmp( name, enm_name) == 0 ) {
                enm = NWAMUI_OBJECT(g_object_ref(tmp_enm));
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

    name = g_strdup(nwamui_object_get_name(NWAMUI_OBJECT(active_ncp)));

    g_object_unref( G_OBJECT(active_ncp) );

    return(name);
}

/**
 * nwamui_daemon_get_current_ncp:
 * @self: NwamuiDaemon*
 *
 * @returns: NwamuiObject*.
 *
 * Gets the active NCP
 *
 * NOTE: For Phase 1 there is only *one* NCP.
 *
 **/
extern NwamuiObject*
nwamui_daemon_get_active_ncp(NwamuiDaemon *self) 
{
    NwamuiObject*  active_ncp = NULL;
    
    g_assert( NWAMUI_IS_DAEMON( self ) );

    g_return_val_if_fail( NWAMUI_IS_DAEMON(self), active_ncp );

    g_object_get (G_OBJECT (self),
              "active_ncp", &active_ncp,
              NULL);

    return( active_ncp );
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
nwamui_daemon_is_active_ncp(NwamuiDaemon *self, NwamuiObject* ncp ) 
{
    g_assert( NWAMUI_IS_DAEMON( self ) );
    g_assert( NWAMUI_IS_NCP(ncp) );

    return(self->prv->active_ncp == ncp);
}

/**
 * nwamui_daemon_object_append:
 * @self: NwamuiDaemon*
 * @new_ncp: New #NwamuiObject to add
 *
 **/
extern void
nwamui_daemon_append_object(NwamuiDaemon *self, NwamuiObject* object) 
{
    gint         idx;
    const gchar *notify_prop;
    g_assert( NWAMUI_IS_DAEMON( self ) );

    if ( NWAMUI_IS_NCP(object)) {
        idx = MANAGED_NCP;
        notify_prop = "ncp_list";
    } else if ( NWAMUI_IS_ENV(object)) {
        idx = MANAGED_LOC;
        notify_prop = "env_list";
    } else if ( NWAMUI_IS_ENM(object)) {
        idx = MANAGED_ENM;
        notify_prop = "enm_list";
    } else {
        g_error("Unsupported object %s", g_type_name(G_TYPE_FROM_INSTANCE(object)));
    }

    if (!g_list_find(self->prv->managed_list[idx], object)) {
        self->prv->managed_list[idx] = g_list_append(self->prv->managed_list[idx], (gpointer)g_object_ref(object));
        g_object_notify(G_OBJECT(self), notify_prop);

        g_signal_emit(self,
          nwamui_daemon_signals[DAEMON_INFO],
          0, /* details */
          NWAMUI_DAEMON_INFO_OBJECT_ADDED,
          object,
          NULL);
    }
}

/**
 * nwamui_daemon_remove_object:
 * @self: NwamuiDaemon*
 * @ncp: #NwamuiObject to remove
 *
 * @returns: TRUE if successfully removed.
 *
 **/
extern gboolean
nwamui_daemon_remove_object(NwamuiDaemon *self, NwamuiObject* object)
{
    gboolean     rval = FALSE;
    gint         idx;
    const gchar *notify_prop;
    
    g_assert( NWAMUI_IS_DAEMON( self ) );
    g_assert( nwamui_object_is_modifiable(object) );
    
    if ( NWAMUI_IS_NCP(object)) {
        idx = MANAGED_NCP;
        notify_prop = "ncp_list";
    } else if ( NWAMUI_IS_ENV(object)) {
        idx = MANAGED_LOC;
        notify_prop = "env_list";
    } else if ( NWAMUI_IS_ENM(object)) {
        idx = MANAGED_ENM;
        notify_prop = "enm_list";
    } else {
        g_error("Unsupported object %s", g_type_name(G_TYPE_FROM_INSTANCE(object)));
    }

    if (nwamui_object_is_modifiable(object)) {
        self->prv->managed_list[idx] = g_list_remove(self->prv->managed_list[idx], (gpointer)object);
        g_object_notify(G_OBJECT(self), notify_prop);

        g_signal_emit(self,
          nwamui_daemon_signals[DAEMON_INFO],
          0, /* details */
          NWAMUI_DAEMON_INFO_OBJECT_REMOVED,
          object,
          NULL);

        g_object_unref(object);
        rval = TRUE;
    } else {
        g_debug("%s %s is not removeable", g_type_name(G_TYPE_FROM_INSTANCE(object)), nwamui_object_get_name(object));
    }

    return(rval);
}


static gint
find_enabled_env(gconstpointer a, gconstpointer b)
{
    NwamuiObject *obj = NWAMUI_OBJECT(a);

    /* Only check if we've not already found that something's enabled */
    if (nwamui_object_get_enabled(NWAMUI_OBJECT(obj))) {
        return 0;
    }
    return 1;
}

/**
 * nwamui_daemon_env_selection_is_manual
 * @self: NwamuiDaemon*
 *
 * When in auto mode, the "enabled" property of every loc is "false".
 * @returns: TRUE if the env selection is manual.
 *
 **/
extern gboolean
nwamui_daemon_env_selection_is_manual(NwamuiDaemon *self)
{
    g_return_val_if_fail(NWAMUI_IS_DAEMON(self), FALSE);

    /* This is determined by looking at the enabled property of env objects */
    if (g_list_find_custom(self->prv->managed_list[MANAGED_LOC], NULL, find_enabled_env)) {
        return TRUE;
    }

    return FALSE;
}

static void
set_env_disabled( gpointer obj, gpointer user_data )
{
    NwamuiObject           *nobj = NWAMUI_OBJECT(obj);
    nwam_state_t            state = NWAM_STATE_UNINITIALIZED;
    nwam_aux_state_t        aux_state = NWAM_AUX_STATE_UNINITIALIZED;

    /* Only disable an env if it's not marked as offline or disabled already,
     * best to make sure we pick the correct one here.
     */
    /* state = nwamui_object_get_nwam_state(nobj, &aux_state, NULL); */
    /* if ( state != NWAM_STATE_OFFLINE && state != NWAM_STATE_DISABLED ) { */
    /*     nwamui_object_set_active(nobj, FALSE); */
    /* } */

    /* We only check the enabled flag, and commit immediately. */
    if (nwamui_object_get_enabled(nobj)) {
        nwamui_object_set_active(nobj, FALSE);
    }
}

/**
 * nwamui_daemon_is_active_env
 * @self: NwamuiDaemon*
 * @env: NwamuiObject*
 *
 * @returns: TRUE if the env passed is the active one.
 *
 * Checks if the passed env is the active one.
 *
 **/
extern gboolean
nwamui_daemon_is_active_env(NwamuiDaemon *self, NwamuiObject* env ) 
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
 * To set active env should call nwamui_env_activate (env) directly, then if it
 * is successful event handler will update this prop.
 * This should be a private function instead of a public prop.
 *
 **/
static void
nwamui_daemon_set_active_env( NwamuiDaemon* self, NwamuiObject* env )
{
    g_assert( NWAMUI_IS_DAEMON(self) );
    g_assert( NWAMUI_IS_ENV(env) );

    if ( env != NULL && env != self->prv->active_env ) {
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
    
    name = g_strdup(nwamui_object_get_name(NWAMUI_OBJECT(active)));
    
    g_object_unref(G_OBJECT(active));
    
    return( name );
}

extern gint
nwamui_daemon_get_online_enm_num(NwamuiDaemon *self)
{
    g_return_val_if_fail(NWAMUI_IS_DAEMON(self), 0);
    return self->prv->online_enm_num;
}

/*
 * Compare a WifiNet (a) with a string (b).
 */
static gint
find_compare_wifi_net_with_name(gconstpointer a, gconstpointer b)
{
    NwamuiWifiNet *wifi_net  = NWAMUI_WIFI_NET(a);
    const gchar   *name      = (const gchar*)b;
    gint           rval      = -1;

    if ( wifi_net != NULL ) {
        const gchar   *wifi_name = NULL;

        wifi_name = nwamui_object_get_name(NWAMUI_OBJECT(wifi_net));

        if (wifi_name != NULL && name != NULL) {
            int len = NWAM_MAX_NAME_LEN;
            rval = strncmp(wifi_name, name, len);
        } else {
            rval = (gint)(wifi_name - name);
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
nwamui_daemon_find_fav_wifi_net_by_name(NwamuiDaemon *self, const gchar* name) 
{
    NwamuiWifiNet  *found_wifi_net = NULL;
    GList          *found_elem = NULL;

    g_return_val_if_fail(NWAMUI_IS_DAEMON(self), NULL);
    g_return_val_if_fail(name, NULL);

    if ((found_elem = g_list_find_custom(self->prv->managed_list[MANAGED_FAV_WIFI], name, 
          (GCompareFunc)find_compare_wifi_net_with_name)) != NULL) {
        if (NWAMUI_IS_WIFI_NET(found_elem->data)) {
            found_wifi_net = NWAMUI_WIFI_NET(g_object_ref(G_OBJECT(found_elem->data)));
        }
    }
    
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
nwamui_daemon_add_wifi_fav(NwamuiDaemon *self, NwamuiWifiNet* new_wifi)
{
    g_return_if_fail(NWAMUI_IS_DAEMON(self));
    g_return_if_fail(new_wifi && NWAMUI_IS_WIFI_NET(new_wifi));

    if ( !self->prv->communicate_change_to_daemon
      || nwamui_object_commit(NWAMUI_OBJECT(new_wifi)) ) {
        self->prv->managed_list[MANAGED_FAV_WIFI] = g_list_insert_sorted(self->prv->managed_list[MANAGED_FAV_WIFI],
          (gpointer)g_object_ref(new_wifi),
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
nwamui_daemon_remove_wifi_fav(NwamuiDaemon *self, NwamuiWifiNet* wifi)
{
    g_return_if_fail(NWAMUI_IS_DAEMON(self));
    g_return_if_fail(wifi && NWAMUI_IS_WIFI_NET(wifi));

    if(self->prv->managed_list[MANAGED_FAV_WIFI] != NULL ) {
        if ( !self->prv->communicate_change_to_daemon
          || nwamui_object_destroy(NWAMUI_OBJECT(wifi)) ) {
            self->prv->managed_list[MANAGED_FAV_WIFI] = g_list_remove(self->prv->managed_list[MANAGED_FAV_WIFI], (gpointer)wifi);

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
    if ( self->prv->managed_list[MANAGED_FAV_WIFI] != NULL && new_list != NULL ) {
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
        GList* merged_list = self->prv->managed_list[MANAGED_FAV_WIFI]; /* Work on original list */
        GList *old_copy = nwamui_util_copy_obj_list(self->prv->managed_list[MANAGED_FAV_WIFI]); /* Get a copy to work with */
        GList *new_items = NULL;

        for(; new_list != NULL; 
            new_list = g_list_next(new_list) ) {
            GList* found_item = NULL;
            
            if ( (found_item = g_list_find_custom( old_copy, new_list->data, (GCompareFunc)nwamui_object_sort_by_name)) != NULL ) {
                /* Same */
                gboolean committed = TRUE;

                if ( nwamui_object_has_modifications( NWAMUI_OBJECT( found_item->data ) ) ) {
                    /* Commit first */
                    committed = nwamui_object_commit( NWAMUI_OBJECT( found_item->data ) );
                }

                if ( committed ) {
                    /* Can't re-read if was never committed - at least this is
                     * the case if it exists only in memory.
                     */
                    nwamui_object_set_handle(NWAMUI_OBJECT( found_item->data ), NULL); /* Re-read configuration */
                }
                g_object_unref(G_OBJECT(found_item->data));
                old_copy = g_list_delete_link( old_copy, found_item );
            }
            else {
                new_items = g_list_append( new_items, g_object_ref(G_OBJECT(new_list->data)));
            }
            /* g_object_unref(G_OBJECT(new_list->data)); */
        }

        if ( old_copy != NULL ) { /* Removed items */
            /* Remove items from merged list */
            for( GList* item = g_list_first( old_copy ); 
                 item != NULL; 
                 item = g_list_next( item ) ) {
                NwamuiWifiNet* wifi = NWAMUI_WIFI_NET(item->data);

                if (!self->prv->communicate_change_to_daemon 
                  || nwamui_object_destroy(NWAMUI_OBJECT(wifi)) ) {
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
                  || nwamui_object_commit(NWAMUI_OBJECT(wifi)) ) {
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

        self->prv->managed_list[MANAGED_FAV_WIFI] = merged_list;
    }
    else if ( new_list == NULL ) {
        /* Delete existing list if any */
        if( self->prv->managed_list[MANAGED_FAV_WIFI] != NULL ) {
            for( GList* item = g_list_first( self->prv->managed_list[MANAGED_FAV_WIFI] ); 
                 item != NULL; 
                 item = g_list_next( item ) ) {
                NwamuiWifiNet* wifi = NWAMUI_WIFI_NET(item->data);

                if (!self->prv->communicate_change_to_daemon 
                  || nwamui_object_destroy(NWAMUI_OBJECT(wifi)) ) {
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

            g_list_free(self->prv->managed_list[MANAGED_FAV_WIFI]);
        }
        self->prv->managed_list[MANAGED_FAV_WIFI] = NULL;
    }
    else {
        for( GList* item = g_list_first(new_list); 
             item != NULL; 
             item = g_list_next(item) ) {
            NwamuiWifiNet* wifi = NWAMUI_WIFI_NET(item->data);

            if (!self->prv->communicate_change_to_daemon 
              || nwamui_object_commit(NWAMUI_OBJECT(wifi)) ) {
                /* Copy on succeeded */
                self->prv->managed_list[MANAGED_FAV_WIFI] = g_list_prepend(self->prv->managed_list[MANAGED_FAV_WIFI], g_object_ref(item->data));

                g_signal_emit (self,
                  nwamui_daemon_signals[S_WIFI_FAV_ADD],
                  0, /* details */
                  g_object_ref(wifi) );
                g_object_notify(G_OBJECT(self), "wifi_fav");
            }
            /* g_object_unref(item->data); */
        }
    }
    /* Ensure is sorted by prio */
    self->prv->managed_list[MANAGED_FAV_WIFI] = g_list_sort(self->prv->managed_list[MANAGED_FAV_WIFI], find_compare_wifi_net_by_prio);

    g_object_notify(G_OBJECT(self), "wifi_fav");

    return( TRUE );
}

static gboolean
nwamui_object_real_commit(NwamuiObject *object)
{
    NwamuiDaemon *daemon = NWAMUI_DAEMON(object);
    gboolean      rval   = TRUE;

    g_return_val_if_fail (NWAMUI_IS_DAEMON(object), FALSE);

    /* Commit changed objects. */
    for( GList* ncp_item = g_list_first(daemon->prv->managed_list[MANAGED_NCP]); 
         ncp_item;
         ncp_item = g_list_next( ncp_item ) ) {

        NwamuiNcp*   ncp      = NWAMUI_NCP(ncp_item->data);

        if (!nwamui_object_commit(NWAMUI_OBJECT(ncp))) {
            rval = FALSE;
            break;
        }
    }
    return( rval );
}

/* Callbacks */
static int
compare_strength( const void *p1, const void *p2 )
{
    nwam_wlan_t*                    wlan1_p = *(nwam_wlan_t**)p1;
    nwam_wlan_t*                    wlan2_p = *(nwam_wlan_t**)p2;
    nwamui_wifi_signal_strength_t   signal_strength1;
    nwamui_wifi_signal_strength_t   signal_strength2;
    
    signal_strength1 = nwamui_wifi_net_strength_map(wlan1_p->nww_signal_strength);
    signal_strength2 = nwamui_wifi_net_strength_map(wlan2_p->nww_signal_strength);

    if ( signal_strength1 > signal_strength2 ) {
        return( 1 );
    }
    else if ( signal_strength1 < signal_strength2 ) {
        return( -1 );
    }
    else {
        return( 0 );
    }
}

/* Sorts wlans by signal strength, uses a pointer to the original elements to
 * avoid extra copy overhead, etc.
 */
static nwam_wlan_t**
sort_wlan_array_by_strength( uint_t nwlan, nwam_wlan_t *wlans )
{
    nwam_wlan_t **sorted_wlans = g_malloc0( sizeof(nwam_wlan_t*) * nwlan );

    g_return_val_if_fail( sorted_wlans != NULL, NULL );

    for ( int i = 0; i < nwlan; i++ ) {
        sorted_wlans[i] = &wlans[i];
    }
    qsort((void*)sorted_wlans, nwlan, sizeof(nwam_wlan_t*), compare_strength);

    return( sorted_wlans );
}

static void
foreach_wifi_in_ncu_emit(gpointer key, gpointer value, gpointer user_data)
{
    NwamuiDaemonPrivate *prv      = NWAMUI_DAEMON_GET_PRIVATE(user_data);
    NwamuiDaemon        *self     = NWAMUI_DAEMON(user_data);
    NwamuiWifiNet       *wifi_net = NWAMUI_WIFI_NET(value);
    
    switch (nwamui_wifi_net_get_life_state(NWAMUI_WIFI_NET(value))) {
    case NWAMUI_WIFI_LIFE_NEW:
        g_signal_emit(self,
          nwamui_daemon_signals[DAEMON_INFO],
          0, /* details */
          NWAMUI_DAEMON_INFO_WLAN_ADDED,
          wifi_net,
          NULL);
        break;

    case NWAMUI_WIFI_LIFE_MODIFIED:
        /* Ignore updated signal since data is updated will trigger its
         * notify.
         */
        /* g_signal_emit(self, */
        /*   nwamui_daemon_signals[DAEMON_INFO], */
        /*   0, /\* details *\/ */
        /*   NWAMUI_DAEMON_INFO_WLAN_UPDATED, */
        /*   wifi_net, */
        /*   NULL); */
        break;

    case NWAMUI_WIFI_LIFE_DEAD:
        g_signal_emit(self,
          nwamui_daemon_signals[DAEMON_INFO],
          0, /* details */
          NWAMUI_DAEMON_INFO_WLAN_REMOVED,
          wifi_net,
          NULL);
        break;

    default:
        break;
    }
}

static gboolean
dispatch_scan_results_from_wlan_array( NwamuiDaemon *daemon, NwamuiNcu* ncu,  uint_t nwlan, nwam_wlan_t *wlans )
{
    g_return_val_if_fail(NWAMUI_IS_DAEMON(daemon), FALSE);

    if ( nwlan != daemon->prv->num_scanned_wifi ) {
        nwamui_daemon_set_num_scanned_wifi( daemon, nwlan );
    }

    g_return_val_if_fail(ncu && NWAMUI_IS_NCU(ncu), FALSE);
    
    nwamui_debug("Dispatch scan events for i/f %s  = %s (%d), %d WLANs in cache.",
      nwamui_object_get_name(NWAMUI_OBJECT(ncu)),
      nwamui_ncu_get_ncu_type(ncu) == NWAMUI_NCU_TYPE_WIRELESS ? "Wireless":"Wired",
      nwamui_ncu_get_ncu_type(ncu),
      nwlan);

    g_return_val_if_fail(nwamui_ncu_get_ncu_type(ncu) == NWAMUI_NCU_TYPE_WIRELESS, FALSE);

    /* Clean the deads first, then mark the left as dead. */
    nwamui_ncu_wifi_hash_clean_dead_wifi_nets(ncu);
    nwamui_ncu_wifi_hash_mark_each(ncu, NWAMUI_WIFI_LIFE_DEAD);

    if (nwlan > 0 && wlans != NULL) {
        nwam_wlan_t   **sorted_wlans = NULL;
        NwamuiWifiNet  *wifi_net     = NULL;
        NwamuiWifiNet  *fav_net      = NULL;

        sorted_wlans = sort_wlan_array_by_strength( nwlan, wlans );

        for (int i = 0; i < nwlan; i++) {
            nwam_wlan_t* wlan_p = sorted_wlans[i];

            g_debug("- %3d: %s%s ESSID %s BSSID %s", i + 1,
              wlan_p->nww_selected?"S":"-",
              wlan_p->nww_connected?"C":"-",
              wlan_p->nww_essid, wlan_p->nww_bssid);

            /* Skipping empty ESSID seems wrong here, what if we actually connect
             * to this... Will it still appear in menu??
             */
            if ( strlen(wlan_p->nww_essid) > 0  && ncu != NULL ) {
                /* Ensure it's being cached in the ncu, and if already
                 * theere will update it with new information.
                 */
                wifi_net = nwamui_ncu_wifi_hash_insert_or_update_from_wlan_t(ncu, wlan_p);

                fav_net = nwamui_daemon_find_fav_wifi_net_by_name(daemon, wlan_p->nww_essid);
                if (fav_net != NULL) {
                    nwamui_debug("Searching for favorite wifi_net '%s', returning 0x%p", wlan_p->nww_essid, fav_net);
                    /* Exists as a favourite, so update it's information */
                    nwamui_wifi_net_update_from_wlan_t(fav_net, wlan_p);
                    g_object_unref(fav_net);
                }

                if (wlan_p->nww_selected) {
                    /* Set owner first to show that which wlan is we are trying
                     * to connect. */
                    nwamui_ncu_set_wifi_info(ncu, wifi_net);
                }

                if (wlan_p->nww_connected) {
                    /* Set owner first befor set wifi state. */
                    /* Update NCU with connected Wifi object */
                    nwamui_ncu_set_wifi_info(ncu, wifi_net);
                    nwamui_wifi_net_set_status(wifi_net, NWAMUI_WIFI_STATUS_CONNECTED);

                    if (!wlan_p->nww_selected) {
                        /* Ignore selected flag. */
                        g_warning("ESSID %s BSSID %s is connected but not selected", wlan_p->nww_essid, wlan_p->nww_bssid);
                    }
                /* } else { */
                    /* Should run this since a wlan with multiple bssid only
                     * one has connect flag, when other are processed will
                     * set it to discoonected.
                     */
                    /* nwamui_wifi_net_set_status(wifi_net, NWAMUI_WIFI_STATUS_DISCONNECTED); */
                }

                g_object_unref(wifi_net);
            }
        }
        g_free(sorted_wlans);
    }
    /* Emit new/dead/modified accordingly */
    nwamui_ncu_wifi_hash_foreach(ncu, foreach_wifi_in_ncu_emit, (gpointer)daemon);

    return( TRUE );
}

static void
dispatch_scan_results_if_wireless(gpointer data, gpointer user_data)
{
    NwamuiDaemon   *daemon = NWAMUI_DAEMON(user_data);
	NwamuiNcu      *ncu = data;

    g_return_if_fail(NWAMUI_IS_NCU(ncu));
    
    if (nwamui_ncu_get_ncu_type(ncu) == NWAMUI_NCU_TYPE_WIRELESS) {
        uint_t          nwlan = 0;
        nwam_wlan_t    *wlans = NULL;
        nwam_error_t    nerr;
        gchar          *name = NULL;

        name = nwamui_ncu_get_device_name (ncu);

        if (name != NULL) {
            if ((nerr = nwam_wlan_get_scan_results(name, &nwlan, &wlans)) == NWAM_SUCCESS) {
                dispatch_scan_results_from_wlan_array(daemon, ncu,  nwlan, wlans);

                free(wlans);
            } else {
                g_assert(wlans == NULL);
                nwamui_debug("Error getting scan results for %s: %s", name, nwam_strerror(nerr) );
            }
            g_free(name);
        } else {
            nwamui_debug("Failed to get device name for ncu %08X", ncu);
        }
    }
}


extern void
nwamui_daemon_dispatch_wifi_scan_events_from_cache(NwamuiDaemon* daemon)
{
    NwamuiDaemonPrivate  *prv = NWAMUI_DAEMON_GET_PRIVATE(daemon);
    
    g_debug("Dispatch wifi scan events from cache called");
    if (prv->active_ncp != NULL && nwamui_ncp_get_wireless_link_num(NWAMUI_NCP(prv->active_ncp)) > 0) {
        nwamui_ncp_foreach_ncu(NWAMUI_NCP(prv->active_ncp), dispatch_scan_results_if_wireless, (gpointer)daemon);
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

gint
nwamui_daemon_get_num_scanned_wifi(NwamuiDaemon* self )
{
    gint    num_scanned_wifi = 0;

    g_return_val_if_fail (NWAMUI_IS_DAEMON (self), num_scanned_wifi );

    g_object_get (G_OBJECT (self),
                  "num_scanned_wifi", &num_scanned_wifi,
                  NULL);

    return( num_scanned_wifi );
}

static void 
nwamui_daemon_set_num_scanned_wifi(NwamuiDaemon* self,  gint num_scanned_wifi)
{
    g_object_set (G_OBJECT (self),
                  "num_scanned_wifi", num_scanned_wifi,
                  NULL); 
}

static void
nwamui_daemon_populate_wifi_fav_list(NwamuiDaemon *self )
{
    NwamuiNcu              *wireless_ncu = NULL;
    NwamuiWifiNet          *wifi = NULL;
    nwam_error_t            nerr;
    int                     cbret;
    fav_wlan_walker_data_t  walker_data;

    if ( self->prv->auto_ncp == NULL || nwamui_ncp_get_wireless_link_num(self->prv->auto_ncp) < 1 ) {
        /* Nothing to do */
        return;
    }

    wireless_ncu = nwamui_ncp_get_first_wireless_ncu(self->prv->auto_ncp);

    if ( wireless_ncu == NULL ) {
        return;
    }

    walker_data.wireless_ncu = wireless_ncu;
    walker_data.fav_list = NULL;

    nerr = nwam_walk_known_wlans(nwam_known_wlan_walker_cb, &walker_data,
                          NWAM_FLAG_KNOWN_WLAN_WALK_PRIORITY_ORDER, &cbret);

    if (nerr != NWAM_SUCCESS) {
        g_debug ("[libnwam] nwam_walk_known_wlans %s", nwam_strerror (nerr));
    }

    /* Use the set method to merge if necessary, but temporarily suspend
     * communication of changes to daemon */
    self->prv->communicate_change_to_daemon = FALSE;
    nwamui_daemon_set_fav_wifi_networks(self, walker_data.fav_list);
    /* nwamui_daemon_dispatch_wifi_scan_events_from_cache( self ); */
    self->prv->communicate_change_to_daemon = TRUE;
    if (walker_data.fav_list) {
        nwamui_util_free_obj_list(walker_data.fav_list);
    }

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
         || !nwamui_ncu_get_ipv4_has_dhcp(ncu) ) {
        g_debug("nwamui_daemon_setup_dhcp_or_wep_key_timeout: Not wireless or DHCP");
        return;
    }

    wifi = nwamui_ncu_get_wifi_info( ncu );
    if ( wifi != NULL ) {
        switch (nwamui_wifi_net_get_security(wifi)) {
#ifdef WEP_ASCII_EQ_HEX 
            case NWAMUI_WIFI_SEC_WEP:
#else
            case NWAMUI_WIFI_SEC_WEP_HEX:
            case NWAMUI_WIFI_SEC_WEP_ASCII:
#endif /* WEP_ASCII_EQ_HEX */
                {
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
    NwamuiEvent         *event     = (NwamuiEvent *)data;
    NwamuiDaemonPrivate *prv       = NWAMUI_DAEMON_GET_PRIVATE(event->daemon);
    NwamuiDaemon        *daemon    = NWAMUI_DAEMON(event->daemon);
	nwam_event_t         nwamevent = event->nwamevent;
    nwam_error_t         err;

    switch (event->e) {
    case NWAMUI_DAEMON_INFO_UNKNOWN:
    case NWAMUI_DAEMON_INFO_ERROR:
    case NWAMUI_DAEMON_INFO_INACTIVE:
        prv->connected_to_nwamd = FALSE;
        nwamui_daemon_set_status(daemon, NWAMUI_DAEMON_STATUS_ERROR);
        break;
    case NWAMUI_DAEMON_INFO_ACTIVE:
        /* Set to UNINITIALIZED first, status will then be got later when icon is to be shown
         */
        prv->connected_to_nwamd = TRUE;

        nwamui_daemon_set_status(daemon, NWAMUI_DAEMON_STATUS_UNINITIALIZED);

		/* Now repopulate data here */
        nwamui_daemon_init_lists( daemon );

        /* Populate wifi list. */
        nwamui_daemon_dispatch_wifi_scan_events_from_cache(daemon);

        /* Trigger notification of active_ncp/env to ensure widgets update */
        /* g_object_notify(G_OBJECT(daemon), "active_ncp"); */
        /* g_object_notify(G_OBJECT(daemon), "active_env"); */

        break;
    case NWAMUI_DAEMON_INFO_RAW:
    {
        switch (nwamevent->nwe_type) {
        case NWAM_EVENT_TYPE_INIT:
            /* should repopulate data here */
            g_debug("%s  NWAM", nwam_event_type_to_string(nwamevent->nwe_type));
                
            /* Redispatch as INFO_ACTIVE */
            g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
              nwamd_event_handler,
              (gpointer) nwamui_event_new(daemon, NWAMUI_DAEMON_INFO_ACTIVE, NULL),
              (GDestroyNotify) nwamui_event_free);
            break;
        case NWAM_EVENT_TYPE_SHUTDOWN:
            g_debug("%s  NWAM", nwam_event_type_to_string(nwamevent->nwe_type));

            /* Redispatch as INFO_INACTIVE */
            g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
              nwamd_event_handler,
              (gpointer) nwamui_event_new(daemon, NWAMUI_DAEMON_INFO_INACTIVE, NULL),
              (GDestroyNotify) nwamui_event_free);
            break;
        case NWAM_EVENT_TYPE_PRIORITY_GROUP: {
            g_debug("%s  %d",
              nwam_event_type_to_string(nwamevent->nwe_type),
              nwamevent->nwe_data.nwe_priority_group_info.nwe_priority);

            if ( prv->active_ncp != NULL ) {
                nwamui_ncp_set_prio_group(NWAMUI_NCP(prv->active_ncp), 
                  nwamevent->nwe_data.nwe_priority_group_info.nwe_priority);
            }
            /* Re-evaluate status since a change in the priority group
             * means that the status could be different.
             */
            /* I think a change of ncp prior group can't change the state of
             * daemon, but probably the sequent object events do.
             */
            /* nwamui_daemon_update_status(daemon); */
        }
            break;
        case NWAM_EVENT_TYPE_IF_STATE:
            if (!nwamevent->nwe_data.nwe_if_state.nwe_addr_valid) {
                g_debug("%s  %s flag(%u) index(%u) valid(%u) added(%u)",
                  nwam_event_type_to_string(nwamevent->nwe_type),
                  nwamevent->nwe_data.nwe_if_state.nwe_name,
                  nwamevent->nwe_data.nwe_if_state.nwe_flags,
                  nwamevent->nwe_data.nwe_if_state.nwe_index,
                  nwamevent->nwe_data.nwe_if_state.nwe_addr_valid,
                  nwamevent->nwe_data.nwe_if_state.nwe_addr_added);

            } else {
                NwamuiNcu          *ncu;
                const gchar        *address = NULL;
                struct sockaddr_in *sin;

                sin = (struct sockaddr_in *)
                  &(nwamevent->nwe_data.nwe_if_state.nwe_addr);
                address = inet_ntoa(sin->sin_addr);

                g_debug("%s  %s flag(%u) index(%u) valid(%u) added(%u) address %s",
                  nwam_event_type_to_string(nwamevent->nwe_type),
                  nwamevent->nwe_data.nwe_if_state.nwe_name,
                  nwamevent->nwe_data.nwe_if_state.nwe_flags,
                  nwamevent->nwe_data.nwe_if_state.nwe_index,
                  nwamevent->nwe_data.nwe_if_state.nwe_addr_valid,
                  nwamevent->nwe_data.nwe_if_state.nwe_addr_added,
                  address);
            }
            break;

        case NWAM_EVENT_TYPE_LINK_STATE: {
            g_debug("%s  %s %s",
              nwam_event_type_to_string(nwamevent->nwe_type),
              nwamevent->nwe_data.nwe_link_state.nwe_name,
              nwamevent->nwe_data.nwe_link_state.nwe_link_up? "up" : "down");

            /* if (prv->active_ncp) { */
            /*     NwamuiObject *ncu = nwamui_ncp_get_ncu_by_device_name(NWAMUI_NCP(prv->active_ncp), nwamevent->nwe_data.nwe_link_state.nwe_name); */
            /* } */
        }
            break;

		case NWAM_EVENT_TYPE_LINK_ACTION: {
            nwam_action_t action = nwamevent->nwe_data.nwe_link_action.nwe_action;
            const gchar*  name   = nwamevent->nwe_data.nwe_link_action.nwe_name;

            switch (action) {
            /* case NWAM_ACTION_ADD: */
            /*     g_debug("Interface %s added", name ); */
            /*     nwamui_object_reload(prv->active_ncp); */
            /*     break; */
            /* case NWAM_ACTION_REMOVE: { */
            /*     NwamuiObject *ncu = nwamui_ncp_get_ncu_by_device_name(NWAMUI_NCP(prv->active_ncp), name); */
            /*     g_debug("Interface %s removed", name); */
            /*     if ( ncu ) { */
            /*         nwamui_ncp_remove_ncu(NWAMUI_NCP(prv->active_ncp), NWAMUI_NCU(ncu)); */
            /*         g_object_unref(ncu); */
            /*     } */
            /* } */
            /*     break; */
            default:
                g_debug("%s  %s %s",
                  nwam_event_type_to_string(nwamevent->nwe_type),
                  nwamevent->nwe_data.nwe_link_action.nwe_name,
                  nwam_action_to_string(nwamevent->nwe_data.nwe_link_action.nwe_action));
                break;
            }
        }
            break;
            
        case NWAM_EVENT_TYPE_OBJECT_STATE:
            g_debug( "%s  %s %s -> %s, %s (parent %s)",
              nwam_event_type_to_string(nwamevent->nwe_type),
              nwam_object_type_to_string(nwamevent->nwe_data.nwe_object_state.nwe_object_type),
              nwamevent->nwe_data.nwe_object_state.nwe_name,
              nwam_state_to_string(nwamevent->nwe_data.nwe_object_state.nwe_state),
              nwam_aux_state_to_string(nwamevent->nwe_data.nwe_object_state.nwe_aux_state),
              nwamevent->nwe_data.nwe_object_state.nwe_parent);

            nwamui_daemon_handle_object_state_event(daemon, nwamevent);
            /* Update daemon status */
            nwamui_daemon_update_status(daemon);
            break;

		case NWAM_EVENT_TYPE_OBJECT_ACTION:
            g_debug( "%s  %s %s %s (parent %s)",
              nwam_event_type_to_string(nwamevent->nwe_type),
              nwam_object_type_to_string(nwamevent->nwe_data.nwe_object_action.nwe_object_type),
              nwamevent->nwe_data.nwe_object_action.nwe_name,
              nwam_action_to_string(nwamevent->nwe_data.nwe_object_action.nwe_action),
              nwamevent->nwe_data.nwe_object_action.nwe_parent);

            nwamui_daemon_handle_object_action_event(daemon, nwamevent);
            /* Update daemon status */
            nwamui_daemon_update_status(daemon);
            break;

		case NWAM_EVENT_TYPE_WLAN_SCAN_REPORT: {
            NwamuiObject * ncu = NULL;

            g_debug( "%s  %s found %u (connected %d)",
              nwam_event_type_to_string(nwamevent->nwe_type),
              nwamevent->nwe_data.nwe_wlan_info.nwe_name,
              nwamevent->nwe_data.nwe_wlan_info.nwe_num_wlans,
              nwamevent->nwe_data.nwe_wlan_info.nwe_connected);

            ncu = nwamui_ncp_get_ncu_by_device_name(NWAMUI_NCP(prv->active_ncp), nwamevent->nwe_data.nwe_wlan_info.nwe_name);

            /* This is strange, this event may be emitted after the ncu
             * is destroyed. So work around it. Maybe a bug of nwamd.
             */
            if (ncu) {
                /* Since nwamd report wlans on a specific NCU, then we only
                 * refresh that NCU instead of all.
                 */
                /* nwamui_daemon_dispatch_wifi_scan_events_from_cache(daemon); */

                dispatch_scan_results_from_wlan_array(daemon, NWAMUI_NCU(ncu),
                  nwamevent->nwe_data.nwe_wlan_info.nwe_num_wlans, 
                  nwamevent->nwe_data.nwe_wlan_info.nwe_wlans);

                if (nwamevent->nwe_data.nwe_wlan_info.nwe_num_wlans > 0) {
                    g_signal_emit (daemon,
                      nwamui_daemon_signals[DAEMON_INFO],
                      0, /* details */
                      NWAMUI_DAEMON_INFO_WLANS_CHANGED,
                      ncu,
                      g_strdup_printf(_("New wireless networks found.")));
                }

                g_object_unref(ncu);

            } else {
                g_warning("Can't find wireless NCU %s in the current active NCP %s",
                  nwamevent->nwe_data.nwe_wlan_info.nwe_name,
                  nwamui_object_get_name(prv->active_ncp));
            }
        }
            break;
        case NWAM_EVENT_TYPE_WLAN_NEED_CHOICE: {
            NwamuiObject *ncu    = NULL;

            g_debug( "%s  %s found %u (connected %d)",
              nwam_event_type_to_string(nwamevent->nwe_type),
              nwamevent->nwe_data.nwe_wlan_info.nwe_name,
              nwamevent->nwe_data.nwe_wlan_info.nwe_num_wlans,
              nwamevent->nwe_data.nwe_wlan_info.nwe_connected);

            ncu = nwamui_ncp_get_ncu_by_device_name(NWAMUI_NCP(prv->active_ncp), nwamevent->nwe_data.nwe_wlan_info.nwe_name);

            if (ncu) {
                /* Since nwamd report wlans on a specific NCU, then we only
                 * refresh that NCU instead of all.
                 */
                /* nwamui_daemon_dispatch_wifi_scan_events_from_cache(daemon); */

                dispatch_scan_results_from_wlan_array(daemon, NWAMUI_NCU(ncu),
                  nwamevent->nwe_data.nwe_wlan_info.nwe_num_wlans,
                  nwamevent->nwe_data.nwe_wlan_info.nwe_wlans);

                if (nwamevent->nwe_data.nwe_wlan_info.nwe_num_wlans > 0) {
                    g_signal_emit (daemon,
                      nwamui_daemon_signals[DAEMON_INFO],
                      0, /* details */
                      NWAMUI_DAEMON_INFO_WLANS_CHANGED,
                      NULL,
                      g_strdup_printf(_("New wireless networks found.")));
                }

                g_signal_emit (daemon,
                  nwamui_daemon_signals[WIFI_SELECTION_NEEDED],
                  0, /* details */
                  ncu );

                g_object_unref(ncu);

            } else {
                g_warning("Can't find wireless NCU %s in the current active NCP %s",
                  nwamevent->nwe_data.nwe_wlan_info.nwe_name,
                  nwamui_object_get_name(prv->active_ncp));
            }
        }
            break;

        case NWAM_EVENT_TYPE_WLAN_CONNECTION_REPORT: {
            NwamuiObject  *ncu       = NULL;

            g_debug( "%s  %s found %u connect to %s (connected %d)",
              nwam_event_type_to_string(nwamevent->nwe_type),
              nwamevent->nwe_data.nwe_wlan_info.nwe_name,
              nwamevent->nwe_data.nwe_wlan_info.nwe_num_wlans,
              nwamevent->nwe_data.nwe_wlan_info.nwe_wlans[0].nww_essid,
              nwamevent->nwe_data.nwe_wlan_info.nwe_connected);

            ncu = nwamui_ncp_get_ncu_by_device_name(NWAMUI_NCP(prv->active_ncp), nwamevent->nwe_data.nwe_wlan_info.nwe_name);

            if (ncu) {
                NwamuiWifiNet *wifi = nwamui_ncu_get_wifi_info(NWAMUI_NCU(ncu));

                if (wifi == NULL ||
                  (wifi && g_strcmp0(nwamevent->nwe_data.nwe_wlan_info.nwe_wlans[0].nww_essid, nwamui_object_get_name(NWAMUI_OBJECT(wifi))) != 0)) {
                    g_warning("NCU %s should already select wlan %s",
                      nwamevent->nwe_data.nwe_wlan_info.nwe_name,
                      nwamevent->nwe_data.nwe_wlan_info.nwe_wlans[0].nww_essid);

                    if (wifi) {
                        g_object_unref(wifi);
                    }

                    /* wifi = nwamui_daemon_find_fav_wifi_net_by_name(daemon, nwamevent->nwe_data.nwe_wlan_info.nwe_wlans[0].nww_essid); */
                    /* wifi = nwamui_ncu_wifi_hash_lookup_by_essid(NWAMUI_NCU(ncu), nwamevent->nwe_data.nwe_wlan_info.nwe_wlans[0].nww_essid); */
                    wifi = nwamui_ncu_wifi_hash_insert_or_update_from_wlan_t(NWAMUI_NCU(ncu), &(nwamevent->nwe_data.nwe_wlan_info.nwe_wlans[0]));
                    nwamui_debug("search for essid '%s' - returning 0x%p", nwamevent->nwe_data.nwe_wlan_info.nwe_wlans[0].nww_essid, wifi);

                    nwamui_ncu_set_wifi_info(NWAMUI_NCU(ncu), wifi);
                }

                if (nwamevent->nwe_data.nwe_wlan_info.nwe_connected) {
                    nwamui_wifi_net_set_status(wifi, NWAMUI_WIFI_STATUS_CONNECTED);

                    /* Is this needed for Phase 1? */
                    /* nwamui_daemon_setup_dhcp_or_wep_key_timeout( daemon, ncu ); */

                    g_signal_emit (daemon,
                      nwamui_daemon_signals[DAEMON_INFO],
                      0, /* details */
                      NWAMUI_DAEMON_INFO_WLAN_CONNECTED,
                      wifi,
                      g_strdup_printf("%s", nwamevent->nwe_data.nwe_wlan_info.nwe_wlans[0].nww_essid));

                } else {
                    nwamui_wifi_net_set_status(wifi, NWAMUI_WIFI_STATUS_FAILED);

                    g_signal_emit (daemon,
                      nwamui_daemon_signals[DAEMON_INFO],
                      0, /* details */
                      NWAMUI_DAEMON_INFO_WLAN_CONNECT_FAILED,
                      wifi,
                      g_strdup_printf(_("%s connection to wireless lan failed"), 
                        nwamevent->nwe_data.nwe_wlan_info.nwe_name));
                }

                g_object_unref(wifi);
                g_object_unref(ncu);

            } else {
                g_warning("Can't find wireless NCU %s in the current active NCP %s",
                  nwamevent->nwe_data.nwe_wlan_info.nwe_name,
                  nwamui_object_get_name(prv->active_ncp));
            }
        }
            break;

        case NWAM_EVENT_TYPE_WLAN_NEED_KEY: {
            NwamuiObject  *ncu  = NULL;

            g_debug( "%s  %s found %u (connected %d)",
              nwam_event_type_to_string(nwamevent->nwe_type),
              nwamevent->nwe_data.nwe_wlan_info.nwe_name,
              nwamevent->nwe_data.nwe_wlan_info.nwe_num_wlans,
              nwamevent->nwe_data.nwe_wlan_info.nwe_connected);

            ncu = nwamui_ncp_get_ncu_by_device_name(NWAMUI_NCP(prv->active_ncp), nwamevent->nwe_data.nwe_wlan_info.nwe_name);

            if (ncu) {                
                NwamuiWifiNet *wifi = nwamui_ncu_get_wifi_info(NWAMUI_NCU(ncu));

                if (wifi == NULL) {
                    g_warning("NCU %s doesn't select the wlan %s",
                      nwamevent->nwe_data.nwe_wlan_info.nwe_name,
                      nwamevent->nwe_data.nwe_wlan_info.nwe_wlans[0].nww_essid);
                }

                /* wifi = nwamui_daemon_find_fav_wifi_net_by_name(daemon, nwamevent->nwe_data.nwe_wlan_info.nwe_wlans[0].nww_essid); */
                /* wifi = nwamui_ncu_wifi_hash_lookup_by_essid(NWAMUI_NCU(ncu), nwamevent->nwe_data.nwe_wlan_info.nwe_wlans[0].nww_essid); */
                wifi = nwamui_ncu_wifi_hash_insert_or_update_from_wlan_t(NWAMUI_NCU(ncu), &(nwamevent->nwe_data.nwe_wlan_info.nwe_wlans[0]));
                nwamui_debug("search for essid '%s' - returning 0x%p", nwamui_object_get_name(NWAMUI_OBJECT(wifi)), wifi);
                
                nwamui_ncu_set_wifi_info(NWAMUI_NCU(ncu), wifi);

                g_signal_emit (daemon,
                  nwamui_daemon_signals[WIFI_KEY_NEEDED],
                  0, /* details */
                  wifi );

                g_object_unref(wifi);
                g_object_unref(ncu);

            } else {
                g_warning("Can't find wireless NCU %s in the current active NCP %s",
                  nwamevent->nwe_data.nwe_wlan_info.nwe_name,
                  nwamui_object_get_name(prv->active_ncp));
            }
        }
            break;

#ifdef NWAM_EVENT_TYPE_QUEUE_QUIET
        case NWAM_EVENT_TYPE_QUEUE_QUIET: {
                /* Nothing to do. */
            }
            break;
#endif

        default:
            g_debug("NWAMUI_DAEMON_INFO_RAW event type %d (%s)", 
              nwamevent->nwe_type,
              nwam_event_type_to_string(nwamevent->nwe_type));

            /* Directly deliver to upper consumers */
            g_signal_emit (daemon,
              nwamui_daemon_signals[DAEMON_INFO],
              0, /* details */
              NWAMUI_DAEMON_INFO_UNKNOWN,
              NULL,
              g_strdup_printf ("Unknown NWAM event type %d.", nwamevent->nwe_type));
            break;
        }
    }
    break;
    default:
        g_warning("Unsupport UI daemon event %d", event->e);
    }
    
    return FALSE;
}

extern void
nwamui_daemon_emit_info_message( NwamuiDaemon* self, const gchar* message )
{
    g_return_if_fail( (NWAMUI_IS_DAEMON(self) && message != NULL ) );

    g_signal_emit (self,
      nwamui_daemon_signals[DAEMON_INFO],
      0, /* details */
      NWAMUI_DAEMON_INFO_GENERIC,
      NULL,
      g_strdup(message?message:""));
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
    NwamuiDaemonPrivate    *prv = NWAMUI_DAEMON_GET_PRIVATE(daemon);
    nwamui_daemon_status_t  old_status;
    guint                   old_status_flags;
    guint                   status_flags = 0;
    nwamui_daemon_status_t  new_status = NWAMUI_DAEMON_STATUS_UNINITIALIZED;
    GQueue                 *signal_queue = g_queue_new();
    gpointer                ptr;

    g_assert(NWAMUI_IS_DAEMON(daemon));

    /* 
     * Determine status from objects 
     */
    old_status = prv->status;
    old_status_flags = prv->status_flags;

    /* First check that the daemon is connected to nwamd */
    if ( !prv->connected_to_nwamd ) {
        status_flags |= STATUS_REASON_DAEMON;
    }
    else {
        /* Now we need to check all objects in the system */

        /* Look at Active NCP and it's NCUs */
        if ( prv->active_ncp == NULL ) {
            status_flags |= STATUS_REASON_NCP;
        }
        else {
            NwamuiNcu       *needs_wifi_selection = NULL;
            NwamuiWifiNet   *needs_wifi_key = NULL;
            if ( !nwamui_ncp_all_ncus_online(NWAMUI_NCP(prv->active_ncp), &needs_wifi_selection, &needs_wifi_key) ) {
                if ( needs_wifi_selection != NULL ) {
                    to_emit_t *sig = (to_emit_t*)g_malloc0( sizeof( to_emit_t ) );
                    sig->signal_id = nwamui_daemon_signals[WIFI_SELECTION_NEEDED];
                    sig->detaiil = 0;
                    sig->param1 = needs_wifi_selection;
                    g_queue_push_tail( signal_queue, (gpointer)sig );
                }
                if ( needs_wifi_key != NULL ) {
                    to_emit_t *sig = (to_emit_t*)g_malloc0( sizeof( to_emit_t ) );
                    sig->signal_id = nwamui_daemon_signals[WIFI_KEY_NEEDED];
                    sig->detaiil = 0;
                    sig->param1 = needs_wifi_key;
                    g_queue_push_tail( signal_queue, (gpointer)sig );
                }
                status_flags |= STATUS_REASON_NCP;
            }
        }

        /* Check the Locations */
        if ( prv->active_env == NULL ) {
            status_flags |= STATUS_REASON_LOC;
        }
        else {
            const gchar      *name = nwamui_object_get_name(NWAMUI_OBJECT(prv->active_env));
            nwam_state_t      state;
            nwam_aux_state_t  aux_state;

            state = nwamui_object_get_nwam_state( NWAMUI_OBJECT(prv->active_env), &aux_state, NULL);
            /* Filter out 'NoNet' */
            if (strcmp(name, NWAM_LOC_NAME_NO_NET) == 0 ||
              (state != NWAM_STATE_ONLINE || aux_state != NWAM_AUX_STATE_ACTIVE)) {
                status_flags |= STATUS_REASON_LOC;
            }
        }

#if 0
        /* According to comments#15,16 of 12079, we don't case ENMs state. */
        /* Assuming we've no found an error yet, check the ENMs */
        new_status = NWAMUI_DAEMON_STATUS_UNINITIALIZED;
        g_list_foreach( prv->managed_list[MANAGED_ENM], check_nwamui_object_online, &new_status );
        if ( new_status == NWAMUI_DAEMON_STATUS_NEEDS_ATTENTION ) {
            status_flags |= STATUS_REASON_ENM;
        }
#endif
    }

    if (status_flags == 0) {
        new_status = NWAMUI_DAEMON_STATUS_ALL_OK;
    } else if (status_flags & STATUS_REASON_DAEMON) {
        new_status = NWAMUI_DAEMON_STATUS_ERROR;
    } else {
        new_status = NWAMUI_DAEMON_STATUS_NEEDS_ATTENTION;
    }

    /* If status has changed, set it, and this will generate an event */
    if ( status_flags != old_status_flags ) {
        prv->status_flags = status_flags;
    }
    nwamui_daemon_set_status(daemon, new_status );

    nwamui_debug("line: %d : status = %d (%s) ; status_flags = %02x", __LINE__, new_status,
                   nwamui_deamon_status_to_string(new_status), status_flags );

    /* Now it's safe to emit any signals generated while gathering status */
    while ( (ptr = g_queue_pop_head( signal_queue )) != NULL ) {
        to_emit_t *sig = (to_emit_t*)ptr;

        g_signal_emit (daemon,
                       sig->signal_id,
                       sig->detaiil,
                       sig->param1 );

        g_free(ptr);
    }
    g_queue_free(signal_queue);
}

static void
nwamui_daemon_handle_object_action_event( NwamuiDaemon   *daemon, nwam_event_t nwamevent )
{
    NwamuiDaemonPrivate    *prv = NWAMUI_DAEMON_GET_PRIVATE(daemon);
    const char             *object_name;

    g_assert(NWAMUI_IS_DAEMON(daemon));
    g_return_if_fail( nwamevent != NULL );

    prv->communicate_change_to_daemon = FALSE;
    object_name = nwamevent->nwe_data.nwe_object_action.nwe_name;

    switch ( nwamevent->nwe_data.nwe_object_action.nwe_object_type ) {
    case NWAM_OBJECT_TYPE_NCP: {
        NwamuiObject   *ncp = nwamui_daemon_get_ncp_by_name( daemon, object_name );

        switch ( nwamevent->nwe_data.nwe_object_action.nwe_action ) {
        case NWAM_ACTION_ADD: {
            if (!ncp) {
                ncp = nwamui_ncp_new(object_name);
                nwamui_daemon_append_object(daemon, ncp);

            } else {
                /* Capplet adds the object first, so we can find it here. Do
                 * nothing. */
                nwamui_debug("%s %s is existed.", nwam_object_type_to_string(nwamevent->nwe_data.nwe_object_action.nwe_object_type), object_name);
                /* nwamui_object_reload(NWAMUI_OBJECT(ncp)); */
            }
        }
            break;
        case NWAM_ACTION_DISABLE:
            break;
        case NWAM_ACTION_ENABLE:
            if (ncp) {
                /* Don't use nwamui_object_set_active() since it will
                 * actually cause a switch again...
                 */
                if (prv->active_ncp != NULL) {
                    g_object_unref(prv->active_ncp);
                }

                prv->active_ncp = g_object_ref(ncp);
                /* We need reload NCP since it may changes when it isn't active. */
                if (!nwamui_object_has_modifications(ncp)) {
                    nwamui_object_reload(ncp);
                }
                g_object_notify(G_OBJECT(daemon), "active_ncp");
            }
            break;
        case NWAM_ACTION_REFRESH:
            if (ncp) {
                nwamui_object_reload(NWAMUI_OBJECT(ncp));
            }
            break;
        case NWAM_ACTION_REMOVE:
            /* Treat REMOVE and DESTROY as the same */
        case NWAM_ACTION_DESTROY: {
            if (ncp) {
                if ( ncp == prv->active_ncp ) {
                    nwamui_warning("Removing active ncp '%s'!", object_name );
                }
                nwamui_daemon_remove_object(daemon, ncp);
            }
        }
            break;
        }
        if (ncp) {
            g_object_unref(ncp);
        }
    }
        break;
    case NWAM_OBJECT_TYPE_NCU: {
        NwamuiObject    *ncp;
        NwamuiObject    *ncu;
        char            *device_name = NULL;
        nwam_ncu_type_t  nwam_ncu_type;

        ncp = nwamui_daemon_get_ncp_by_name(daemon, nwamevent->nwe_data.nwe_object_action.nwe_parent);
        /* NCU's come in the typed name format (e.g. link:ath0) */
        if (ncp) {
            if ( nwam_ncu_typed_name_to_name(object_name, &nwam_ncu_type, &device_name ) == NWAM_SUCCESS ) {
                /* Assumption here is that such events are only for the active NCP
                 * since there is no way to qualify the object's parent from the
                 * name.
                 */
                ncu = nwamui_ncp_get_ncu_by_device_name(NWAMUI_NCP(ncp), device_name);
                free(device_name);
            } else {
                g_assert_not_reached();
            }
        } else {
            g_warning("Work around because nwam_ncp_copy will not emit ncp add event, so the sequenct ncu add will fail here.");
            break;
        }
        switch ( nwamevent->nwe_data.nwe_object_action.nwe_action ) {
        case NWAM_ACTION_ADD: {
            if (ncu == NULL) {
                nwamui_object_reload(NWAMUI_OBJECT(ncp));
            } else {
                /* interface:name/link:name, so a NCU will be added two
                 * times. Depend ncp's reload to add link NCU.
                 *
                 * When add a new NCU to an empty NCP, reload which will run
                 * nwam_ncp_walk_ncus can not get any NCUs, then the new added
                 * NCU will be removed.
                 */
                /* nwamui_object_reload(NWAMUI_OBJECT(ncp)); */
                nwamui_object_reload(NWAMUI_OBJECT(ncu));
            }
        }
            break;
        case NWAM_ACTION_DISABLE:
        case NWAM_ACTION_ENABLE:
        case NWAM_ACTION_REFRESH:
            /* When add a new wusb, nwamd has a bug that add object happens on a
             * NCP, after add a known wlan, the NCU refresh event happens on
             * another. This is a workaround since ncu maybe NULL.
             */
            if (ncu && !nwamui_object_has_modifications(ncu)) {
                nwamui_object_reload(ncu);
            }
            break;
        case NWAM_ACTION_REMOVE:
            /* Fall through. */
        case NWAM_ACTION_DESTROY:
            if (ncu) {
                nwamui_ncp_remove_ncu(NWAMUI_NCP(ncp), NWAMUI_NCU(ncu));
            }
            break;
        }
        if (ncu) {
            g_object_unref(ncu);
        }
        if (ncp) {
            g_object_unref(ncp);
        }
    }
        break;
    case NWAM_OBJECT_TYPE_LOC: {
        NwamuiObject   *env = nwamui_daemon_get_env_by_name( daemon, object_name );

        switch ( nwamevent->nwe_data.nwe_object_action.nwe_action ) {
        case NWAM_ACTION_ADD: {
            if (env == NULL) {
                env = nwamui_env_new( object_name );
                nwamui_daemon_append_object(daemon, env);
            } else {
                /* Capplet adds the object first, so we can find it here. Do
                 * nothing. */
                nwamui_debug("%s %s is existed.", nwam_object_type_to_string(nwamevent->nwe_data.nwe_object_action.nwe_object_type), object_name);
            }
        }
            break;
        case NWAM_ACTION_DISABLE:
            if (env) {
                nwamui_object_set_enabled(env, FALSE);
                g_object_notify(G_OBJECT(daemon), "env_selection_mode");
            }
            break;
        case NWAM_ACTION_ENABLE:
            if (env) {
                /* Ensure that correct active env pointer */
                if ( prv->active_env != env ) {
                    /* New active ENV */
                    nwamui_daemon_set_active_env( daemon, env );
                }

                nwamui_object_set_enabled(env, TRUE);
                g_object_notify(G_OBJECT(daemon), "env_selection_mode");
            }
            break;
        case NWAM_ACTION_REFRESH:
            if (env) {
                nwamui_object_reload(env);
            }
            break;
        case NWAM_ACTION_REMOVE:
            /* Fall through, remove/destroy are the same here */
        case NWAM_ACTION_DESTROY: {
            /* Capplet removes the object first, so we can find it here. Do
             * nothing. */
            if (env) {
                if ( prv->active_env == env ) {
                    g_object_unref(prv->active_env);
                    prv->active_env = NULL;
                }
                nwamui_daemon_remove_object(daemon, env);
            }
        }
            break;
        }
        if (env)
            g_object_unref(env);
    }
        break;
    case NWAM_OBJECT_TYPE_ENM: {
        NwamuiObject   *enm = nwamui_daemon_get_enm_by_name( daemon, object_name );

        switch ( nwamevent->nwe_data.nwe_object_action.nwe_action ) {
        case NWAM_ACTION_ADD: {
            if ( enm == NULL ) {
                enm = nwamui_enm_new( object_name );
                nwamui_daemon_append_object(daemon, enm);
            } else {
                /* Capplet adds the object first, so we can find it here. Do
                 * nothing. */
                nwamui_debug("%s %s is existed.", nwam_object_type_to_string(nwamevent->nwe_data.nwe_object_action.nwe_object_type), object_name);
            }
        }
            break;
        case NWAM_ACTION_DISABLE:
        case NWAM_ACTION_ENABLE:
        case NWAM_ACTION_REFRESH:
            if (enm && !nwamui_object_has_modifications(enm)) {
                nwamui_object_reload(enm);
            }
            break;
        case NWAM_ACTION_REMOVE:
            /* Fall through, basically the same operation */
        case NWAM_ACTION_DESTROY: {
            /* Capplet removes the object first, so we can find it here. Do
             * nothing. */
            if (enm) {
                nwamui_daemon_remove_object(daemon, enm);
            }
        }
            break;
        }
        if (enm) {
            g_object_unref(enm);
        }

        nwamui_daemon_update_online_enm_num(daemon);
    }
        break;
    case NWAM_OBJECT_TYPE_KNOWN_WLAN: {
        NwamuiWifiNet   *wifi;
        NwamuiNcu       *wireless_ncu = nwamui_ncp_get_first_wireless_ncu(prv->auto_ncp);
            
        if ( wireless_ncu == NULL ) {
            nwamui_debug("Got unexpected NULL wireless_ncu when adding a favourite", NULL );
            break;
        }

        wifi = nwamui_daemon_find_fav_wifi_net_by_name(daemon, object_name);
        nwamui_debug("Searching for favorite wifi_net '%s', returning 0x%p", object_name, wifi);

        switch ( nwamevent->nwe_data.nwe_object_action.nwe_action ) {
        case NWAM_ACTION_ADD: {
            nwam_error_t                nerr;
            nwam_known_wlan_handle_t    known_wlan_h;

            if ( wifi == NULL ) {
                /* Check to see if it exists in an NCU hash anywhere
                 */
                wifi = nwamui_daemon_find_wifi_net_in_ncus( daemon, object_name );

                if ( wifi == NULL ) {
                    /* Still didn't find one, so create new fav entry. */
                    nerr = nwam_known_wlan_read(object_name, 0, &known_wlan_h);
                    if (nerr != NWAM_SUCCESS) {
                        nwamui_warning("Error reading new known wlan: %s", nwam_strerror(nerr));
                    }
                    else {
                        wifi = nwamui_wifi_net_new_with_handle( wireless_ncu, known_wlan_h );
                        nwam_known_wlan_free( known_wlan_h );
                    }
                }
                else {
                    /* NULL will read directly from system */
                    nwamui_object_set_handle(NWAMUI_OBJECT(wifi), NULL);
                }
                nwamui_daemon_add_wifi_fav(daemon, wifi);
            }
        }
            break;
        case NWAM_ACTION_DISABLE: {
        }
            break;
        case NWAM_ACTION_ENABLE: {
        }
            break;
        case NWAM_ACTION_REFRESH: {
            /* Need to refresh to catch update to priorities */
            if ( wifi != NULL ) {
                nwamui_object_reload( NWAMUI_OBJECT(wifi) );
            }
        }
            break;
        case NWAM_ACTION_REMOVE: {
        }
            /* Same as Destroy, so fall-through */
        case NWAM_ACTION_DESTROY: {
            if ( wifi != NULL ) {
                prv->communicate_change_to_daemon = FALSE;
                nwamui_daemon_remove_wifi_fav(daemon, wifi );
                prv->communicate_change_to_daemon = TRUE;
            }
        }
            break;
        }
        if ( wifi != NULL ) {
            g_object_unref(wifi);
        }

        g_object_unref(wireless_ncu);
    }
        break;
    }
    prv->communicate_change_to_daemon = TRUE;
}

static void
nwamui_daemon_handle_object_state_event( NwamuiDaemon   *daemon, nwam_event_t nwamevent )
{
    NwamuiDaemonPrivate *prv              = NWAMUI_DAEMON(daemon)->prv;
    guint                status_flags     = 0;
    nwam_object_type_t   object_type      = nwamevent->nwe_data.nwe_object_state.nwe_object_type;
    const char*          object_name      = nwamevent->nwe_data.nwe_object_state.nwe_name;
    nwam_state_t         object_state     = nwamevent->nwe_data.nwe_object_state.nwe_state;
    nwam_aux_state_t     object_aux_state = nwamevent->nwe_data.nwe_object_state.nwe_aux_state;
    NwamuiObject        *obj              = NULL;

    /* First check that the daemon is connected to nwamd */
    if ( prv->connected_to_nwamd ) {
        /* Now we need to check all objects in the system */
        switch ( object_type ) {
        case NWAM_OBJECT_TYPE_NCU: {
            char            *device_name = NULL;
            nwam_ncu_type_t  nwam_ncu_type;
            NwamuiObject    *ncp;

            /* Work around: OBJECT_STATE  ncu link:iwk0 -> offline*, need WiFi network selection (parent ) */
            if (nwamevent->nwe_data.nwe_object_state.nwe_parent
              && *nwamevent->nwe_data.nwe_object_state.nwe_parent == '\0') {
                ncp = g_object_ref(prv->active_ncp);
            } else {
                ncp = nwamui_daemon_get_ncp_by_name(daemon, nwamevent->nwe_data.nwe_object_state.nwe_parent);
            }
            if (ncp) {
                /* NCU's come in the typed name format (e.g. link:ath0) */
                if ( nwam_ncu_typed_name_to_name(object_name, &nwam_ncu_type, &device_name ) == NWAM_SUCCESS ) {
                    obj = nwamui_ncp_get_ncu_by_device_name(NWAMUI_NCP(ncp), device_name);
                    free(device_name);
                } else {
                    g_assert_not_reached();
                }

                g_object_unref(ncp);

                /* Work around since ncu signals of inactive ncp may be received by
                 * active ncp. So ncu may not exist. */
                if (obj) {
                    if (nwam_ncu_type == NWAM_NCU_TYPE_INTERFACE) {
                        nwamui_object_set_nwam_state(obj, object_state, object_aux_state);
                    } else {
                        nwamui_ncu_set_link_nwam_state(NWAMUI_NCU(obj), object_state, object_aux_state);
                    }
                    g_object_unref(obj);
                }
            }
        }
            break;
        case NWAM_OBJECT_TYPE_NCP:
            obj = nwamui_daemon_get_ncp_by_name(daemon, object_name);
            nwamui_object_set_nwam_state(obj, object_state, object_aux_state);
            if ( object_state == NWAM_STATE_ONLINE && object_aux_state == NWAM_AUX_STATE_ACTIVE ) {
                /* Assume that activating, will cause signal to say other was
                 * deactivated too.
                 */
                if (obj != NWAMUI_OBJECT(daemon->prv->active_ncp)) {
                    nwamui_object_set_active(obj, TRUE);
                    g_object_notify(G_OBJECT(daemon), "active_ncp");
                }
            } else {
                status_flags |= STATUS_REASON_NCP;
            }
            g_object_unref(obj);
            break;
        case NWAM_OBJECT_TYPE_LOC:
            obj = nwamui_daemon_get_env_by_name( daemon, object_name);
            nwamui_object_set_nwam_state(obj, object_state, object_aux_state);
            if ( object_state == NWAM_STATE_ONLINE && object_aux_state == NWAM_AUX_STATE_ACTIVE ) {
                nwamui_daemon_set_active_env( daemon, obj);
            } else {
                status_flags |= STATUS_REASON_LOC;
            }
            g_object_unref(obj);
            break;
        case NWAM_OBJECT_TYPE_ENM:
            obj = NWAMUI_OBJECT(nwamui_daemon_get_enm_by_name( daemon, object_name ));
            nwamui_object_set_nwam_state(obj, object_state, object_aux_state);

            nwamui_daemon_update_online_enm_num(daemon);

            if ( object_state == NWAM_STATE_ONLINE && object_aux_state == NWAM_AUX_STATE_ACTIVE ) {
                /* Nothing. */
            } else {
                status_flags |= STATUS_REASON_ENM;
            }
            g_object_unref(obj);
            break;
        case NWAM_OBJECT_TYPE_KNOWN_WLAN:
            /* fall-through */
/*             case NWAM_OBJECT_TYPE_NONE: */
            /* fall-through */
        default:
            /* SKip */
            break;
        }
    }
    else {
        status_flags |= STATUS_REASON_DAEMON;
    }

    DEBUG_STATUS( object_name, object_state, object_aux_state, status_flags );
}

static void
check_nwamui_object_online( gpointer obj, gpointer user_data )
{
    NwamuiObject            *nobj = NWAMUI_OBJECT(obj);
    nwamui_daemon_status_t  *new_status_p = (nwamui_daemon_status_t*)user_data; 
    nwam_state_t             state;
    nwam_aux_state_t         aux_state;

    state = nwamui_object_get_nwam_state( nobj, &aux_state, NULL);

    if ( state != NWAM_STATE_ONLINE || aux_state != NWAM_AUX_STATE_ACTIVE ) {
        *new_status_p = NWAMUI_DAEMON_STATUS_NEEDS_ATTENTION;
    }
}

static void
nwamui_daemon_update_online_enm_num(NwamuiDaemon *self)
{
    /* Get the number of online ENMs. */
    self->prv->online_enm_num = 0;
    g_list_foreach(self->prv->managed_list[MANAGED_ENM], check_nwamui_object_online_num, &self->prv->online_enm_num );
    /* Must emit every time even the num isn't changed, since the active ENMs
     * could be changed.
     */
    g_object_notify(G_OBJECT(self), "online-enm-num");
}

static void
check_nwamui_object_online_num( gpointer obj, gpointer user_data )
{
    NwamuiObject     *nobj       = NWAMUI_OBJECT(obj);
    gint             *online_num = (gint *)user_data;
    nwam_state_t      state;
    nwam_aux_state_t  aux_state;

    if ( nwamui_object_get_active( NWAMUI_OBJECT(nobj) ) ) {
        (*online_num) ++;
    }
}


extern nwamui_daemon_status_t 
nwamui_daemon_get_status_icon_type( NwamuiDaemon *daemon )
{
    if ( daemon != NULL ) {

        g_assert(daemon->prv->status != NWAMUI_DAEMON_STATUS_UNINITIALIZED);

        if ( daemon->prv->status == NWAMUI_DAEMON_STATUS_UNINITIALIZED ) {
            /* Early call, so try to update it's value now */
            nwamui_daemon_update_status( daemon );
            /* Return error. */
        } else {
            return daemon->prv->status;
        }
    }
    return NWAMUI_DAEMON_STATUS_ERROR;
}

extern const gchar*
nwamui_deamon_status_to_string( nwamui_daemon_status_t status )
{
    const gchar*    status_str = NULL;

    switch(status) {
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

    return( status_str );
}

static gboolean
nwamui_daemon_nwam_connect( gboolean block )
{
    gboolean      rval = FALSE;
    gboolean      not_done;
    nwam_error_t  nerr;
    char        *smf_state;

    g_static_mutex_lock (&nwam_event_mutex);
    nwam_init_done = rval;
    g_static_mutex_unlock (&nwam_event_mutex);

    do {
        smf_state = smf_get_state(NWAM_FMRI);
        if (strcmp(smf_state, SCF_STATE_STRING_ONLINE) != 0) {
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
        free(smf_state);
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
    gboolean                connected_to_nwamd = FALSE;

    g_debug ("nwam_events_thread");
    
    if ( nwamui_daemon_nwam_connect( FALSE ) ) {
		/*
         * We can emit NWAMUI_DAEMON_INFO_ACTIVE here, so we can populate all
         * the info in nwamd_event_handler
		 */
        connected_to_nwamd = TRUE;

		g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
		  nwamd_event_handler,
		  (gpointer) nwamui_event_new(daemon, NWAMUI_DAEMON_INFO_ACTIVE, NULL),
		  (GDestroyNotify) nwamui_event_free);
    }
	
	while (event_thread_running()) {
        if ( (err = nwam_event_wait( &nwamevent)) != NWAM_SUCCESS ) {
			g_debug("Event wait error: %s", nwam_strerror(err));

            /* Send event to tell UI there was an error */
            g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
              nwamd_event_handler,
              (gpointer) nwamui_event_new(daemon, NWAMUI_DAEMON_INFO_ERROR, NULL),
              (GDestroyNotify) nwamui_event_free);
              
            connected_to_nwamd = FALSE;

            if ( ! event_thread_running() ) {
                /* If we were waiting for an event and we got an error, make
                 * sure it wasn't intentional, to cause this thread to exit
                 */
                continue;
            }
			if (err != NWAM_SUCCESS ) {
                /* Try again */
                if ( (err = nwam_event_wait( &nwamevent)) != NWAM_SUCCESS ) {
                    g_debug("Attempting to reopen connection to daemon");
                    nwamui_daemon_nwam_disconnect();
                    if ( nwamui_daemon_nwam_connect( TRUE ) ) {
                        g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                          nwamd_event_handler,
                          (gpointer) nwamui_event_new(daemon, NWAMUI_DAEMON_INFO_ACTIVE, NULL),
                          (GDestroyNotify) nwamui_event_free);

                        connected_to_nwamd = TRUE;

                        continue;
                    }
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
        else if ( nwamevent->nwe_type == NWAM_EVENT_TYPE_SHUTDOWN ) {
            /* NWAM has done a clean shutdown, remember this, so we can reset
             * to connected on next event.
             */
            connected_to_nwamd = FALSE;
        }
        else if ( !connected_to_nwamd ) {
            g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
              nwamd_event_handler,
              (gpointer) nwamui_event_new(daemon, NWAMUI_DAEMON_INFO_ACTIVE, NULL),
              (GDestroyNotify) nwamui_event_free);
            connected_to_nwamd = TRUE;
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
default_wifi_selection_needed (NwamuiDaemon *self, NwamuiNcu* ncu, gpointer user_data)
{
    nwamui_debug("Wireless selection needed for network interface '%s'", nwamui_ncu_get_display_name(ncu));
}

static void
default_wifi_key_needed (NwamuiDaemon *self, NwamuiWifiNet* wifi, gpointer user_data)
{
    if ( wifi ) {
        const char *essid = nwamui_object_get_name(NWAMUI_OBJECT(wifi));

        nwamui_debug("Wireless key needed for network '%s'", essid?essid:"???UNKNOWN ESSID???" );
    }
}

static void
default_wifi_scan_started_signal_handler (NwamuiDaemon *self, gpointer user_data)
{
    nwamui_debug("Scan Started", NULL);
}

static void
default_wifi_scan_result_signal_handler (NwamuiDaemon *self, NwamuiWifiNet* wifi_net, gpointer user_data)
{
    if ( wifi_net) {
        nwamui_debug("Scan Result : Got ESSID\t\"%s\"", nwamui_object_get_name(NWAMUI_OBJECT(wifi_net)));
    } else {
        /* End of Scan */
        nwamui_debug("Scan Result : End Of Scan", NULL);
    }
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
    NwamuiDaemonPrivate *prv = NWAMUI_DAEMON_GET_PRIVATE(data);
    NwamuiDaemon        *self = NWAMUI_DAEMON(data);
    char *name;
    nwam_error_t nerr;
    NwamuiObject* new_env;

    g_debug ("nwam_loc_walker_cb 0x%p", env);
    
    if ( (nerr = nwam_loc_get_name (env, &name)) != NWAM_SUCCESS ) {
        g_debug ("Failed to get name for loc, error: %s", nwam_strerror (nerr));
        name = NULL;
    }

    if ( name) {
        if ((new_env = nwamui_daemon_get_env_by_name( self, name )) != NULL ) {
            /* Reload */
            nwamui_object_reload( NWAMUI_OBJECT(new_env) );
            /* Found it, so remove from temp list of ones to be removed */
            self->prv->temp_list = g_list_remove( self->prv->temp_list, new_env );
        } else {
            new_env = nwamui_env_new_with_handle (env);
            prv->managed_list[MANAGED_LOC] = g_list_append(prv->managed_list[MANAGED_LOC], (gpointer)new_env);
        }
        free(name);
    }
            
    if ( new_env != NULL ) {
        if ( nwamui_object_get_active(NWAMUI_OBJECT(new_env)) ) {
            prv->active_env = NWAMUI_OBJECT(g_object_ref(new_env));
        }
    }

    return(0);
}

static int
nwam_enm_walker_cb (nwam_enm_handle_t enm, void *data)
{
    NwamuiDaemonPrivate *prv = NWAMUI_DAEMON_GET_PRIVATE(data);
    NwamuiDaemon        *self = NWAMUI_DAEMON(data);
    char *name;
    nwam_error_t nerr;
    NwamuiObject* new_enm;

    g_debug ("nwam_enm_walker_cb 0x%p", enm);
    
    if ( (nerr = nwam_enm_get_name (enm, &name)) != NWAM_SUCCESS ) {
        g_debug ("Failed to get name for enm, error: %s", nwam_strerror (nerr));
        name = NULL;
    }

    if ( name) {
        if ((new_enm = nwamui_daemon_get_enm_by_name( self, name )) != NULL ) {
            /* Reload */
            nwamui_object_reload(NWAMUI_OBJECT(new_enm));
            /* Found it, so remove from temp list of ones to be removed */
            self->prv->temp_list = g_list_remove( self->prv->temp_list, new_enm );
        } else {
            new_enm = nwamui_enm_new_with_handle (enm);
            prv->managed_list[MANAGED_ENM] = g_list_append(prv->managed_list[MANAGED_ENM], (gpointer)new_enm);
        }
        free(name);
    }

    return(0);
}

static int
nwam_ncp_walker_cb (nwam_ncp_handle_t ncp, void *data)
{
    NwamuiDaemonPrivate *prv = NWAMUI_DAEMON_GET_PRIVATE(data);
    NwamuiDaemon        *self = NWAMUI_DAEMON(data);
    gchar               *name;
    nwam_error_t         nerr;
    NwamuiObject         *new_ncp;

    g_debug ("nwam_ncp_walker_cb 0x%p", ncp);
    
    if ( (nerr = nwam_ncp_get_name (ncp, &name)) != NWAM_SUCCESS ) {
        g_debug ("Failed to get name for ncp, error: %s", nwam_strerror (nerr));
        name = NULL;
    }

    if ( name) {
        if ((new_ncp = nwamui_daemon_get_ncp_by_name( self, name )) != NULL ) {
            /* Reload */
            nwamui_object_reload(NWAMUI_OBJECT(new_ncp));
            /* Found it, so remove from temp list of ones to be removed */
            self->prv->temp_list = g_list_remove( self->prv->temp_list, new_ncp );
        } else {
            new_ncp = nwamui_ncp_new_with_handle (ncp);
            prv->managed_list[MANAGED_NCP] = g_list_append(prv->managed_list[MANAGED_NCP], (gpointer)new_ncp);
        }
        free(name);
    }
        
    if ( new_ncp != NULL ) {
        name = (gchar *)nwamui_object_get_name(NWAMUI_OBJECT(new_ncp));
        if ( name != NULL ) { 
            if ( strncmp( name, NWAM_NCP_NAME_AUTOMATIC, strlen(NWAM_NCP_NAME_AUTOMATIC)) == 0 ) {
                if ( prv->auto_ncp != NWAMUI_NCP(new_ncp)) {
                    prv->auto_ncp = NWAMUI_NCP(g_object_ref( new_ncp ));
                }
            }
        }
        if ( nwamui_object_get_active(NWAMUI_OBJECT(new_ncp)) ) {
            prv->active_ncp = NWAMUI_OBJECT(g_object_ref( new_ncp ));
        }
    } else {
        g_warning("Failed to create NWAMUI_NCP");
    }

    return(0);
}

static int
nwam_known_wlan_walker_cb (nwam_known_wlan_handle_t wlan_h, void *data)
{
    nwam_error_t            nerr;
    NwamuiWifiNet*          wifi = NULL;
    fav_wlan_walker_data_t *walker_data = (fav_wlan_walker_data_t*)data;;

    g_debug ("nwam_known_wlan_walker_cb 0x%p", wlan_h );
    
#if 0
    /* Mix normal wlans and fav wlans. */
    wifi = nwamui_ncu_wifi_hash_insert_or_update_from_handle(walker_data->wireless_ncu, wlan_h);
    nwamui_debug("search for essid '%s' - returning 0x%p", nwamui_object_get_name(NWAMUI_OBJECT(wifi)), wifi);
#else
    /* Seperate normal wlans and fav wlans. */
    wifi = nwamui_wifi_net_new_with_handle(walker_data->wireless_ncu, wlan_h);
#endif

    if ( wifi != NULL ) {
        /* We don't need ref when merging. */
        walker_data->fav_list = g_list_append(walker_data->fav_list, (gpointer)wifi);
    }

    return(0);
}

extern void
nwamui_daemon_foreach_ncp(NwamuiDaemon *self, GFunc func, gpointer user_data)
{
    NwamuiDaemonPrivate  *prv = NWAMUI_DAEMON_GET_PRIVATE(self);

    g_return_if_fail(NWAMUI_IS_DAEMON(self));

    g_list_foreach(prv->managed_list[MANAGED_NCP], func, user_data);
}

extern void
nwamui_daemon_foreach_loc(NwamuiDaemon *self, GFunc func, gpointer user_data)
{
    NwamuiDaemonPrivate  *prv = NWAMUI_DAEMON_GET_PRIVATE(self);

    g_return_if_fail(NWAMUI_IS_DAEMON(self));

    g_list_foreach(prv->managed_list[MANAGED_LOC], func, user_data);
}

extern void
nwamui_daemon_foreach_enm(NwamuiDaemon *self, GFunc func, gpointer user_data)
{
    NwamuiDaemonPrivate  *prv = NWAMUI_DAEMON_GET_PRIVATE(self);

    g_return_if_fail(NWAMUI_IS_DAEMON(self));

    g_list_foreach(prv->managed_list[MANAGED_ENM], func, user_data);
}

extern void
nwamui_daemon_foreach_fav_wifi(NwamuiDaemon *self, GFunc func, gpointer user_data)
{
    NwamuiDaemonPrivate  *prv = NWAMUI_DAEMON_GET_PRIVATE(self);

    g_return_if_fail(NWAMUI_IS_DAEMON(self));

    g_list_foreach(prv->managed_list[MANAGED_FAV_WIFI], func, user_data);
}

extern GList*
nwamui_daemon_find_ncp(NwamuiDaemon *self, GCompareFunc func, gconstpointer user_data)
{
    NwamuiDaemonPrivate  *prv = NWAMUI_DAEMON_GET_PRIVATE(self);

    g_return_val_if_fail(NWAMUI_IS_DAEMON(self), NULL);

    if (func)
        return g_list_find_custom(prv->managed_list[MANAGED_NCP], user_data, func);
    else
        return g_list_find(prv->managed_list[MANAGED_NCP], user_data);
}

extern GList*
nwamui_daemon_find_loc(NwamuiDaemon *self, GCompareFunc func, gconstpointer user_data)
{
    NwamuiDaemonPrivate  *prv = NWAMUI_DAEMON_GET_PRIVATE(self);

    g_return_val_if_fail(NWAMUI_IS_DAEMON(self), NULL);

    if (func)
        return g_list_find_custom(prv->managed_list[MANAGED_LOC], user_data, func);
    else
        return g_list_find(prv->managed_list[MANAGED_LOC], user_data);
}

extern GList*
nwamui_daemon_find_enm(NwamuiDaemon *self, GCompareFunc func, gconstpointer user_data)
{
    NwamuiDaemonPrivate  *prv = NWAMUI_DAEMON_GET_PRIVATE(self);

    g_return_val_if_fail(NWAMUI_IS_DAEMON(self), NULL);

    if (func)
        return g_list_find_custom(prv->managed_list[MANAGED_ENM], user_data, func);
    else
        return g_list_find(prv->managed_list[MANAGED_ENM], user_data);
}

extern GList*
nwamui_daemon_find_fav_wifi(NwamuiDaemon *self, GCompareFunc func, gconstpointer user_data)
{
    NwamuiDaemonPrivate  *prv = NWAMUI_DAEMON_GET_PRIVATE(self);

    g_return_val_if_fail(NWAMUI_IS_DAEMON(self), NULL);

    if (func)
        return g_list_find_custom(prv->managed_list[MANAGED_FAV_WIFI], user_data, func);
    else
        return g_list_find(prv->managed_list[MANAGED_FAV_WIFI], user_data);
}

