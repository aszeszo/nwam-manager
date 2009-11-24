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
    WIFI_SCAN_STARTED,
    WIFI_SCAN_RESULT,
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

static guint nwamui_daemon_signals[LAST_SIGNAL] = { 0, 0 };

struct _NwamuiDaemonPrivate {
    NwamuiEnv   *active_env;
    NwamuiNcp   *active_ncp;
    NwamuiNcp   *auto_ncp;  /* For quick access */
    
    GList       *env_list;
    GList       *ncp_list;
    GList       *enm_list;
    GList       *wifi_fav_list;

    GList       *temp_list; /* Used to temporarily track not found objects in walkers */

    /* others */
    gboolean                     connected_to_nwamd;
    nwamui_daemon_status_t       status;
    guint                        status_flags; 
    GThread                     *nwam_events_gthread;
    gboolean                     communicate_change_to_daemon;
    guint                        wep_timeout_id;
    gboolean                     emit_wlan_changed_signals;
    GQueue                       *wlan_scan_queue;
    gint                         num_scanned_wireless;
};

typedef struct _fav_wlan_walker_data {
    NwamuiNcu  *wireless_ncu;
    GList      *fav_list;
} fav_wlan_walker_data_t;

typedef struct _NwamuiEvent {
    int           e;	/* ui daemon event type */
    nwam_event_t  nwamevent;	/* daemon data */
    NwamuiDaemon* daemon;
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

static void check_enm_online( gpointer obj, gpointer user_data );

static void nwamui_daemon_update_status( NwamuiDaemon   *daemon );

static NwamuiNcu* nwamui_daemon_get_first_wireless_ncu( NwamuiDaemon *self );

static gboolean nwamui_daemon_nwam_connect( gboolean block );
static void     nwamui_daemon_nwam_disconnect( void );

static void nwamui_daemon_handle_object_action_event( NwamuiDaemon   *daemon, nwam_event_t nwamevent );
static void nwamui_daemon_handle_object_state_event( NwamuiDaemon   *daemon, nwam_event_t nwamevent );
static void nwamui_daemon_set_status( NwamuiDaemon* self, nwamui_daemon_status_t status );

/* Callbacks */
static void object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);

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

    /* Create some signals */
    nwamui_daemon_signals[WIFI_SCAN_STARTED] =   
            g_signal_new ("wifi_scan_started",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (NwamuiDaemonClass, wifi_scan_started),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID, 
                  G_TYPE_NONE,                  /* Return Type */
                  0,                            /* Number of Args */
                  NULL);                        /* Types of Args */
    
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
    GError                 *error = NULL;
    NwamuiWifiNet          *wifi_net = NULL;
    NwamuiEnm              *new_enm = NULL;
    nwam_error_t            nerr;
    NwamuiNcp*              user_ncp;
    
    self->prv = g_new0 (NwamuiDaemonPrivate, 1);
    
    self->prv->connected_to_nwamd = FALSE;
    self->prv->status_flags = 0;
    self->prv->wep_timeout_id = 0;
    self->prv->communicate_change_to_daemon = TRUE;
    self->prv->active_env = NULL;
    self->prv->active_ncp = NULL;
    self->prv->auto_ncp = NULL;
    self->prv->ncp_list = NULL;
    self->prv->env_list = NULL;
    self->prv->enm_list = NULL;
    self->prv->wlan_scan_queue = g_queue_new();

    self->prv->nwam_events_gthread = g_thread_create(nwam_events_thread, g_object_ref(self), TRUE, &error);
    if( self->prv->nwam_events_gthread == NULL ) {
        g_debug("Error creating nwam events thread: %s", (error && error->message)?error->message:"" );
    }

    nwamui_daemon_init_lists ( self );

    g_signal_connect(G_OBJECT(self), "notify", (GCallback)object_notify_cb, (gpointer)self);
}


static void
nwamui_daemon_init_lists (NwamuiDaemon *self)
{
    GError                 *error = NULL;
    NwamuiWifiNet          *wifi_net = NULL;
    NwamuiEnm              *new_enm = NULL;
    nwam_error_t            nerr;
    NwamuiNcp*              user_ncp;
    
    /* NCPs */

    /* Get list of Ncps from libnwam */
    self->prv->temp_list = g_list_copy( self->prv->ncp_list ); /* not worried about refs here */
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
            nwamui_daemon_ncp_remove( self, NWAMUI_NCP(item->data));
        }
    }

    /* If there isn't a User NCP, then we need to create one which is a clone
     * of the Automatic NCP - this is only for Phase 1, we will need to look
     * at an alternative mechanism in the GUI for more than 2 NCP case.
     */
    if ( (user_ncp = nwamui_daemon_get_ncp_by_name( self, NWAM_NCP_NAME_USER ) ) == NULL ) {
        if ( self->prv->auto_ncp != NULL ) { /* If Auto doesn't exist do nothing */
            user_ncp = nwamui_ncp_clone( self->prv->auto_ncp, NWAM_NCP_NAME_USER );
            if ( user_ncp != NULL ) {
                self->prv->ncp_list = g_list_append(self->prv->ncp_list, (gpointer)g_object_ref(user_ncp));
            }
        }
    }

    /* Env / Locations */
    self->prv->temp_list = g_list_copy( self->prv->env_list ); /* not worried about refs here */
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
    if ( self->prv->temp_list != NULL ) {
        for( GList* item = g_list_first( self->prv->temp_list ); 
             item != NULL;
             item = g_list_next( item ) ) {
            nwamui_daemon_env_remove( self, NWAMUI_ENV(item->data));
        }
    }

    /* ENMs */
    self->prv->temp_list = g_list_copy( self->prv->enm_list ); /* not worried about refs here */
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
            nwamui_daemon_enm_remove( self, NWAMUI_ENM(item->data));
        }
    }

    /* Populate Wifi Favourites List */
    nwamui_daemon_populate_wifi_fav_list( self );
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
            NwamuiEnv *env;
            
            if ( self->prv->active_env != NULL ) {
                g_object_unref(G_OBJECT(self->prv->active_env));
            }

            env = NWAMUI_ENV(g_value_dup_object( value ));

            /* Always send out this signal. To set active env should
             * call nwamui_env_activate (env) directly, then if it
             * is successful event handler will update this prop.
             * This should be a private function instead of a public prop.
             */
            self->prv->active_env = env;
            g_object_notify(G_OBJECT(self), "active_env");
            g_object_notify(G_OBJECT(self), "env_selection_mode");
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
        case PROP_STATUS:
            nwamui_daemon_set_status(self, g_value_get_int( value ));
            break;
        case PROP_NUM_SCANNED_WIFI: {
                self->prv->num_scanned_wireless = g_value_get_int(value);
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
        case PROP_NCP_LIST: {
                g_value_set_pointer( value, nwamui_util_copy_obj_list( self->prv->ncp_list ) );
            }
            break;
        case PROP_STATUS: {
                g_value_set_int(value, self->prv->status);
            }
            break;
        case PROP_WIFI_FAV: {
                GList *wifi_fav = NULL;

                wifi_fav = nwamui_daemon_get_fav_wifi_networks( self );

                g_value_set_pointer( value, wifi_fav );
            }
            break;
        case PROP_NUM_SCANNED_WIFI: {
                g_value_set_int(value, self->prv->num_scanned_wireless);
            }
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

    if ( self->prv->wifi_fav_list != NULL ) {
        g_list_foreach(self->prv->wifi_fav_list, nwamui_util_obj_unref, NULL );
        g_list_free( self->prv->wifi_fav_list );
    }
    
    if ( self->prv->wlan_scan_queue != NULL ) {
        g_queue_free( self->prv->wlan_scan_queue );
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
nwamui_daemon_get_status( NwamuiDaemon* self )
{
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

    g_debug("i/f %s  = %s (%d)", name,  
            nwamui_ncu_get_ncu_type (ncu) == NWAMUI_NCU_TYPE_WIRELESS?"Wireless":"Wired", nwamui_ncu_get_ncu_type (ncu));

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

typedef struct {
    const gchar    *essid;
    NwamuiWifiNet  *wifi_net;
}
find_wifi_net_info_t;

static gboolean
find_wifi_net(  GtkTreeModel *model,
                           GtkTreePath *path,
                           GtkTreeIter *iter,
                           gpointer data)
{
	NwamuiNcu              *ncu = NULL;
    NwamuiWifiNet          *wifi = NULL;
    find_wifi_net_info_t   *find_info_p = (find_wifi_net_info_t*)data;
	
  	gtk_tree_model_get(model, iter, 0, &ncu, -1);

    if ( ncu == NULL || !NWAMUI_IS_NCU(ncu) || find_info_p == NULL ) {
        return( FALSE );
    }
    g_assert( NWAMUI_IS_NCU(ncu) );
    
	if (nwamui_ncu_get_ncu_type (ncu) != NWAMUI_NCU_TYPE_WIRELESS ) {
        g_object_unref( ncu );
        return( FALSE );
	}
    
    wifi = nwamui_ncu_wifi_hash_lookup_by_essid( ncu, find_info_p->essid );

    if ( wifi != NULL ) {
        find_info_p->wifi_net = wifi;
    }
    
    g_object_unref( ncu );
    
    return( find_info_p->wifi_net != NULL );
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
    NwamuiNcp              *ncp;
    find_wifi_net_info_t    find_info;
    
    ncp = nwamui_daemon_get_active_ncp (self);

    if ( ncp != NULL ) {
        find_info.essid = essid;
        find_info.wifi_net = NULL;

        nwamui_ncp_foreach_ncu (ncp, find_wifi_net, (gpointer)&find_info );
        g_object_unref(ncp);
    }

    return( find_info.wifi_net );
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
    GString    *have_names = NULL;
    NwamuiNcp*  ncp   = NULL;

    g_return_val_if_fail( NWAMUI_IS_DAEMON(self) && name != NULL, ncp );

    have_names = g_string_new("");
    g_string_append_printf(have_names, "daemon -> find ncp %s in [ ", name);

    for( GList* item = g_list_first( self->prv->ncp_list ); 
         item != NULL;
         item = g_list_next( item ) ) {
        if ( item->data != NULL && NWAMUI_IS_NCP(item->data) ) {
            NwamuiNcp* tmp_ncp = NWAMUI_NCP(item->data);
            const gchar*     ncp_name = nwamui_object_get_name(NWAMUI_OBJECT(tmp_ncp));

            g_string_append_printf(have_names, "%s ", ncp_name);

            if ( strcmp( name, ncp_name) == 0 ) {
                ncp = NWAMUI_NCP(g_object_ref(G_OBJECT(tmp_ncp)));
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
            const gchar*     env_name = nwamui_object_get_name(NWAMUI_OBJECT(tmp_env));

            if ( strcmp( name, env_name) == 0 ) {
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
            const gchar*     enm_name = nwamui_object_get_name(NWAMUI_OBJECT(tmp_enm));

            if ( strcmp( name, enm_name) == 0 ) {
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

    name = g_strdup(nwamui_object_get_name(NWAMUI_OBJECT(active_ncp)));

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

    if ( ncp != NULL && ncp != self->prv->active_ncp ) {
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

    if ( !g_list_find( self->prv->ncp_list, new_ncp ) ) {
        self->prv->ncp_list = g_list_append(self->prv->ncp_list, (gpointer)g_object_ref(new_ncp));
        g_object_notify(G_OBJECT(self), "ncp_list" );
    }
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
        g_object_unref(ncp);
        g_object_notify(G_OBJECT(self), "ncp_list" );
        rval = TRUE;
    }
    else {
        g_debug("Ncp is not removeable");
    }

    return(rval);
}


static void
check_env_enabled( gpointer obj, gpointer user_data )
{
    NwamuiObject            *nobj = NWAMUI_OBJECT(obj);
    gboolean                *is_manual_p = (gboolean*)user_data;

    if ( is_manual_p == NULL ) {
        return;
    }

    /* Only check if we've not already found that something's enabled */
    if ( !(*is_manual_p) && nwamui_object_get_enabled( NWAMUI_OBJECT(nobj) ) ) {
        *is_manual_p = TRUE;
    }
}

/**
 * nwamui_daemon_env_selection_is_manual
 * @self: NwamuiDaemon*
 *
 * @returns: TRUE if the env selection is manual.
 *
 **/
extern gboolean
nwamui_daemon_env_selection_is_manual(NwamuiDaemon *self)
{
    gboolean rval = FALSE;

    g_return_val_if_fail( NWAMUI_IS_DAEMON(self), rval );

    /* This is determined by looking at the enabled property of env objects */
    g_list_foreach( self->prv->env_list, check_env_enabled, (gpointer)&rval ); 

    return( rval );
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
    state = nwamui_object_get_nwam_state(nobj, &aux_state, NULL);
    if ( state != NWAM_STATE_OFFLINE && state != NWAM_STATE_DISABLED ) {
        nwamui_object_set_active(nobj, FALSE);
    }
}

/**
 * nwamui_daemon_env_selection_set_manual
 * @self: NwamuiDaemon*
 *
 * Sets activation of Envs to be manual with immediate effect.
 *
 * This entails doing the following:
 *
 * - If Manual
 *
 *   Specifcially activate the passed environment.
 *
 * - If not Manual
 *
 *   Loop through each Env and ensure it's disabled, returning control to the
 *   system to decide on best selection.
 *
 **/
extern void
nwamui_daemon_env_selection_set_manual(NwamuiDaemon *self, gboolean manual, NwamuiEnv* manual_env )
{
    g_return_if_fail( NWAMUI_IS_DAEMON(self) );

    if ( manual ) {
        /* Specifcially set the env active, will disable others  */
        nwamui_object_set_active( NWAMUI_OBJECT(manual_env), TRUE );
    }
    else {
        /* This is determined by looking at the enabled property of env objects */
        g_list_foreach( self->prv->env_list, set_env_disabled, NULL ); 
    }
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
 * To set active env should call nwamui_env_activate (env) directly, then if it
 * is successful event handler will update this prop.
 * This should be a private function instead of a public prop.
 *
 **/
static void
nwamui_daemon_set_active_env( NwamuiDaemon* self, NwamuiEnv* env )
{
    g_assert( NWAMUI_IS_DAEMON(self) );

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

    if ( !g_list_find( self->prv->env_list, new_env ) ) {
        self->prv->env_list = g_list_append(self->prv->env_list, (gpointer)g_object_ref(new_env));
        g_object_notify(G_OBJECT(self), "env_list" );
    }
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

    if ( g_list_find( self->prv->env_list, (gpointer)env) ) {
        self->prv->env_list = g_list_remove(self->prv->env_list, (gpointer)env);
        g_object_notify(G_OBJECT(self), "env_list" );
        rval = TRUE;
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

    if ( !g_list_find( self->prv->enm_list, new_enm ) ) {
        self->prv->enm_list = g_list_append(self->prv->enm_list, (gpointer)g_object_ref(new_enm));
        g_object_notify(G_OBJECT(self), "enm_list" );
    }
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

    if ( g_list_find( self->prv->enm_list, (gpointer)enm) ) {
        self->prv->enm_list = g_list_remove(self->prv->enm_list, (gpointer)enm);
        g_object_notify(G_OBJECT(self), "enm_list" );
        rval = TRUE;
    }

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
    NwamuiWifiNet *wifi_net  = NWAMUI_WIFI_NET(a);
    const gchar   *name      = (const gchar*)b;
    const gchar   *wifi_name = NULL;
    gint           rval      = -1;

    if ( wifi_net != NULL ) {
        wifi_name = nwamui_object_get_name(NWAMUI_OBJECT(wifi_net));

        if ( wifi_name == NULL && name == NULL ) {
            rval = 0;
            g_debug("%s: wifi_name == NULL && name == NULL : returning %d", __func__, rval );
        }
        else if (wifi_name == NULL || name == NULL ) {
            rval = (wifi_name == NULL)?1:-1;
            g_debug("%s: wifi_name == NULL || name == NULL : returning %d", __func__, rval );
        }
        else {
            int len = NWAM_MAX_NAME_LEN;
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
        nwamui_debug("Searching for wifi_net '%s', returning NULL", name?name:"NULL" );
        return( found_wifi_net );
    }

    if ( (found_elem = g_list_find_custom( self->prv->wifi_fav_list, name, 
                                           (GCompareFunc)find_compare_wifi_net_with_name)) != NULL ) {
        if ( found_elem != NULL && found_elem->data != NULL && NWAMUI_IS_WIFI_NET(found_elem->data)) {
            found_wifi_net = NWAMUI_WIFI_NET(g_object_ref(G_OBJECT(found_elem->data)));
        }
    }
    
    nwamui_debug("Searching for wifi_net '%s', returning 0x%08x", name, found_wifi_net );
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
        NwamuiNcp*   ncp      = NWAMUI_NCP(ncp_item->data);
        const gchar* ncp_name = nwamui_object_get_name(NWAMUI_OBJECT(ncp));

        if ( nwamui_ncp_is_modifiable( ncp ) ) {
            nwamui_debug("NCP : %s modifiable", ncp_name );
            for( GList* ncu_item = g_list_first(nwamui_ncp_get_ncu_list( NWAMUI_NCP(ncp) ) ); 
                 rval && ncu_item != NULL; 
                 ncu_item = g_list_next( ncu_item ) ) {
                NwamuiNcu*   ncu      = NWAMUI_NCU(ncu_item->data);
                const gchar* ncu_name = nwamui_ncu_get_display_name( ncu );

                if ( nwamui_ncu_has_modifications( ncu ) ) {
                    nwamui_debug("Going to commit changes for %s : %s", ncp_name, ncu_name );
                    if ( !nwamui_object_commit(NWAMUI_OBJECT(ncu)) ) {
                        nwamui_debug("Commit FAILED for %s : %s", ncp_name, ncu_name );
                        rval = FALSE;
                        break;
                    }
                }
                else {
                    nwamui_debug("No changes for %s : %s", ncp_name, ncu_name );
                }
            }
        }
        else {
            nwamui_debug("NCP : %s read-only", ncp_name );
        }

    }
    return( rval );
}

/* Callbacks */

static void
object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
/*     NwamuiDaemon* self = NWAMUI_DAEMON(data); */
}

static void
nwamui_daemon_emit_scan_started_event( NwamuiDaemon *daemon )
{
    if (daemon->prv->emit_wlan_changed_signals) {
        g_queue_clear( daemon->prv->wlan_scan_queue );
        /* Signal Scan Start */
        g_signal_emit (daemon,
          nwamui_daemon_signals[WIFI_SCAN_STARTED],
          0, NULL );
    }
}

/* Assumes ownership of wifi_net, so should not be unref-ed by caller */
static void
nwamui_daemon_queue_scan_result( NwamuiDaemon *daemon, NwamuiWifiNet *wifi_net )
{
    if (daemon->prv->emit_wlan_changed_signals) {
        if ( g_queue_find( daemon->prv->wlan_scan_queue, (gpointer)wifi_net) == NULL ) {
            g_queue_push_tail(daemon->prv->wlan_scan_queue, (gpointer)wifi_net);
        }
    }
}

/* Assumes ownership of wifi_net, so should not be unref-ed by caller */
static void
nwamui_daemon_emit_scan_result( NwamuiWifiNet *wifi_net, gpointer data )
{
    NwamuiDaemon *daemon = NWAMUI_DAEMON(data);

    if (daemon->prv->emit_wlan_changed_signals) {
        /* trigger event */
        g_signal_emit (daemon,
          nwamui_daemon_signals[WIFI_SCAN_RESULT],
          0, /* details */
          wifi_net); /* Hand off ownership of wifi_net */
    }
}

/* Assumes ownership of wifi_net, so should not be unref-ed by caller */
static void
nwamui_daemon_emit_queued_scan_results( NwamuiDaemon *daemon )
{
    if (daemon->prv->emit_wlan_changed_signals) {
        g_queue_foreach( daemon->prv->wlan_scan_queue, 
                         (GFunc)nwamui_daemon_emit_scan_result, (gpointer)daemon );
    }
    g_queue_clear( daemon->prv->wlan_scan_queue );
}

static void
nwamui_daemon_emit_scan_end_of_results( NwamuiDaemon *daemon )
{
    if (daemon->prv->emit_wlan_changed_signals) {
        /* Signal Scan Start */
        g_signal_emit (daemon,
          nwamui_daemon_signals[WIFI_SCAN_RESULT],
          0, /* details */
          NULL); /* NULL Wifi == End of Results */

        daemon->prv->emit_wlan_changed_signals = FALSE;
    }
}

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

static gboolean
dispatch_scan_results_from_wlan_array( NwamuiDaemon *daemon, NwamuiNcu* ncu,  uint_t nwlan, nwam_wlan_t *wlans )
{
    gchar*  name = NULL;

    if ( nwlan != daemon->prv->num_scanned_wireless ) {
        nwamui_daemon_set_num_scanned_wifi( daemon, nwlan );
    }

    if ( ncu == NULL || !NWAMUI_IS_NCU(ncu) ) {
        return( FALSE );
    }

    g_assert( NWAMUI_IS_NCU(ncu) );
    
	name = nwamui_ncu_get_device_name (ncu);

    nwamui_debug("Dispatch scan events for i/f %s  = %s (%d)", name,  nwamui_ncu_get_ncu_type (ncu) == NWAMUI_NCU_TYPE_WIRELESS?"Wireless":"Wired", nwamui_ncu_get_ncu_type (ncu));
	if (nwamui_ncu_get_ncu_type (ncu) != NWAMUI_NCU_TYPE_WIRELESS ) {
        g_free (name);
        return( FALSE );
	}

    if ( name != NULL ) {
        nwam_error_t    nerr;
        NwamuiWifiNet  *wifi_net = NULL;
        NwamuiWifiNet  *fav_net = NULL;

        if ( nwlan > 0 && wlans != NULL ) {
            nwam_wlan_t** sorted_wlans = NULL;

            nwamui_debug("%d WLANs in cache.", nwlan);
            sorted_wlans = sort_wlan_array_by_strength( nwlan, wlans );
            for (int i = 0; i < nwlan; i++) {
                nwam_wlan_t* wlan_p = sorted_wlans[i];

                nwamui_debug( "%d: ESSID %s BSSID %s : connected = %s : selected = %s", i + 1,
                              wlan_p->nww_essid, wlan_p->nww_bssid, 
                              wlan_p->nww_connected?"TRUE":"FALSE", wlan_p->nww_selected?"TRUE":"FALSE" );

                /* Skipping empty ESSID seems wrong here, what if we actually connect
                 * to this... Will it still appear in menu??
                 */
                if ( strlen(wlan_p->nww_essid) > 0  && ncu != NULL ) {
                    /* Ensure it's being cached in the ncu, and if already
                     * theere will update it with new information.
                     */
                    wifi_net = nwamui_ncu_wifi_hash_insert_or_update_from_wlan_t( ncu, wlan_p);

                    nwamui_debug( "ESSID: %s ; NCU = %08X ; wifi_net = %08X", wlan_p->nww_essid, ncu, wifi_net );

                    if ( (fav_net = nwamui_daemon_find_fav_wifi_net_by_name( daemon, 
                                                                wlan_p->nww_essid ) ) != NULL ) {
                        /* Exists as a favourite, so update it's information */
                        nwamui_wifi_net_update_from_wlan_t( fav_net, wlan_p);
                        g_object_unref(fav_net);
                    }

                    if ( wlan_p->nww_connected || wlan_p->nww_selected ) {
                        NwamuiWifiNet* ncu_wifi = nwamui_ncu_get_wifi_info( ncu );

                        /* Set owner first befor set wifi state. */
                        if ( ncu_wifi != wifi_net ) {
                            /* Update NCU with connected Wifi object */
                            nwamui_ncu_set_wifi_info(ncu, wifi_net);
                        }

                        if ( wlan_p->nww_connected ) {
                            nwamui_wifi_net_set_status(wifi_net, NWAMUI_WIFI_STATUS_CONNECTED);
                        }

                        if (ncu_wifi) {
                            g_object_unref(ncu_wifi);
                        }
                    }
                    else {
                        nwamui_wifi_net_set_status(wifi_net, NWAMUI_WIFI_STATUS_DISCONNECTED);
                    }

                    nwamui_daemon_queue_scan_result( daemon, wifi_net );
                }
            }
            nwamui_daemon_emit_queued_scan_results( daemon );

            g_free(sorted_wlans);
        }
        else {
            nwamui_debug("No wlans to dispatch events for: nwlan = %d", nwlan );
        }
    }
    g_free (name);
    
    return( TRUE );
}


static gboolean
dispatch_scan_results_if_wireless(  GtkTreeModel *model,
                                    GtkTreePath *path,
                                    GtkTreeIter *iter,
                                    gpointer data)
{
    NwamuiDaemon   *daemon = NWAMUI_DAEMON(data);
	NwamuiNcu      *ncu = NULL;
	
  	gtk_tree_model_get(model, iter, 0, &ncu, -1);

    if ( ncu == NULL || !NWAMUI_IS_NCU(ncu) ) {
        return( FALSE );
    }

    g_assert( NWAMUI_IS_NCU(ncu) );
    
    if ( ncu != NULL ) {
        uint_t          nwlan = 0;
        nwam_wlan_t    *wlans = NULL;
        nwam_error_t    nerr;
        NwamuiWifiNet  *wifi_net = NULL;
        NwamuiWifiNet  *fav_net = NULL;
        gchar          *name = NULL;

        name = nwamui_ncu_get_device_name (ncu);

        if ( name != NULL ) {
            if ( (nerr = nwam_wlan_get_scan_results( name, &nwlan, &wlans )) == NWAM_SUCCESS ) {
                dispatch_scan_results_from_wlan_array( daemon, ncu,  nwlan, wlans );

                free(wlans);
            }
            else {
                g_assert(wlans == NULL);
                nwamui_debug("Error getting scan results for %s: %s", name, nwam_strerror(nerr) );
            }
            g_free(name);
        }
        else {
            nwamui_debug("Failed to get device name for ncu %08X", ncu );
        }

        g_object_unref( ncu );
    }
    
    return( FALSE );
}


extern void
nwamui_daemon_dispatch_wifi_scan_events_from_cache(NwamuiDaemon* daemon )
{
    NwamuiNcp              *ncp;
    
    g_debug("Dispatch wifi scan events from cache called");
 
    ncp = nwamui_daemon_get_active_ncp (daemon);

    if ( ncp != NULL && nwamui_ncp_get_wireless_link_num(ncp) > 0 ) {
        daemon->prv->emit_wlan_changed_signals = TRUE;

        nwamui_daemon_emit_scan_started_event( daemon );

        nwamui_ncp_foreach_ncu (ncp, dispatch_scan_results_if_wireless, (gpointer)daemon);
        g_object_unref(ncp);

        nwamui_daemon_emit_scan_end_of_results( daemon );
    }
}

static void
dispatch_wifi_scan_events_from_event(NwamuiDaemon* daemon, nwam_event_t event )
{
    NwamuiWifiNet          *wifi_net = NULL;
    NwamuiNcu              *ncu = NULL;
    uint_t                  nwlan;
    const char*             prev_iface_name = NULL;
    const gchar*            name = event->nwe_data.nwe_wlan_info.nwe_name;

	if ( event == NULL || event->nwe_type != NWAM_EVENT_TYPE_WLAN_SCAN_REPORT ) {
        return;
    }


    ncu = nwamui_ncp_get_ncu_by_device_name(daemon->prv->active_ncp, name);

    nwamui_daemon_emit_scan_started_event( daemon );

    if ( ncu != NULL ) {
        dispatch_scan_results_from_wlan_array( daemon, ncu,  
                                               event->nwe_data.nwe_wlan_info.nwe_num_wlans,
                                               event->nwe_data.nwe_wlan_info.nwe_wlans );

        g_object_unref(ncu);
    }
 
    nwamui_daemon_emit_scan_end_of_results( daemon );
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

static NwamuiNcu*
nwamui_daemon_get_first_wireless_ncu( NwamuiDaemon *self )
{
    NwamuiNcu              *wireless_ncu = NULL;
    GList                  *wireless_ncus = NULL;
    guint                   num_wireless;

    /* Always use relative to the Automatic NCP since it will always be
     * present and should be complete.
     */
    if ( self->prv->auto_ncp == NULL || (num_wireless = nwamui_ncp_get_wireless_link_num(self->prv->auto_ncp)) < 1 ) {
        /* Nothing to do */
        return( NULL );
    }

    wireless_ncus = nwamui_ncp_get_wireless_ncus( self->prv->auto_ncp );
    if ( wireless_ncus == NULL ) {
        nwamui_warning("Got unexpected empty wireless_ncu list, when num_wirelss = %d", num_wireless );
        return( NULL );
    }
    else {
        for ( GList *elem = g_list_first(wireless_ncus);
              elem != NULL;
              elem = g_list_next( elem ) ) {

            if ( elem->data && NWAMUI_IS_NCU(elem->data) ) {
                wireless_ncu = NWAMUI_NCU(g_object_ref(G_OBJECT(wireless_ncus->data)));
                /* Just use the first one */
                break;
            }
        }
        /* Clean up list */
        g_list_foreach( wireless_ncus, nwamui_util_obj_unref, NULL );
        g_list_free( wireless_ncus );
    }

    return( wireless_ncu );
}


static void
nwamui_daemon_populate_wifi_fav_list(NwamuiDaemon *self )
{
    NwamuiNcu              *wireless_ncu = NULL;
    NwamuiWifiNet          *wifi = NULL;
    GList                  *wireless_ncus = NULL;
    nwam_error_t            nerr;
    int                     cbret;
    fav_wlan_walker_data_t  walker_data;

    if ( self->prv->auto_ncp == NULL || nwamui_ncp_get_wireless_link_num(self->prv->auto_ncp) < 1 ) {
        /* Nothing to do */
        return;
    }

    wireless_ncu = nwamui_daemon_get_first_wireless_ncu( self );

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
    NwamuiEvent             *event = (NwamuiEvent *)data;
    NwamuiDaemon            *daemon = event->daemon;
	nwam_event_t             nwamevent = event->nwamevent;
    nwam_error_t             err;

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

		/* Now repopulate data here */
        nwamui_daemon_init_lists ( daemon );

        nwamui_daemon_dispatch_wifi_scan_events_from_cache(daemon);

        /* Will generate an event if status changes */
        nwamui_daemon_update_status( daemon );

        /* Trigger notification of active_ncp/env to ensure widgets update */
/*         g_object_notify(G_OBJECT(daemon), "active_ncp"); */
/*         g_object_notify(G_OBJECT(daemon), "active_env"); */

        break;
    case NWAMUI_DAEMON_INFO_RAW:
    {
        g_debug("NWAMUI_DAEMON_INFO_RAW event type %d (%s)", 
                nwamevent->nwe_type, nwam_event_type_to_string( nwamevent->nwe_type ));
        switch (nwamevent->nwe_type) {
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
              nwamevent->nwe_data.nwe_priority_group_info.nwe_priority);

            if ( daemon->prv->active_ncp != NULL ) {
                nwamui_ncp_set_current_prio_group( daemon->prv->active_ncp, 
                  nwamevent->nwe_data.nwe_priority_group_info.nwe_priority);
            }
            /* Re-evaluate status since a change in the priority group
             * means that the status could be different.
             */
            nwamui_daemon_update_status( daemon );
        }
            break;
        case NWAM_EVENT_TYPE_IF_STATE: {
                NwamuiNcu*      ncu;
                const gchar*    name = nwamevent->nwe_data.nwe_if_state.nwe_name;
                const gchar*    address = NULL;


                if (nwamevent->nwe_data.nwe_if_state.nwe_addr_valid) {
                    struct sockaddr_in *sin;

                    sin = (struct sockaddr_in *)
                        &(nwamevent->nwe_data.nwe_if_state.nwe_addr);
                    address = inet_ntoa(sin->sin_addr);
                    g_debug ("Interface %s is up with address %s", name, address );
                }
            }
            break;

        case NWAM_EVENT_TYPE_LINK_STATE: {
                const gchar*    name = nwamevent->nwe_data.nwe_link_state.nwe_name;

                g_debug( "%s\t%s %s",
                  nwam_event_type_to_string(nwamevent->nwe_type),
                  nwamevent->nwe_data.nwe_link_state.nwe_name,
                  nwamevent->nwe_data.nwe_link_state.nwe_link_up? "up" : "down" );
            }
            break;

		case NWAM_EVENT_TYPE_LINK_ACTION: {
            NwamuiNcp*      ncp = nwamui_daemon_get_active_ncp( daemon );
            nwam_action_t   action = nwamevent->nwe_data.nwe_link_action.nwe_action;
            const gchar*    name = nwamevent->nwe_data.nwe_link_action.nwe_name;

            switch (action) {
            case NWAM_ACTION_ADD:
/*                 g_debug("Interface %s added", name ); */
/*                 nwamui_object_reload(NWAMUI_OBJECT(ncp)); */
                break;
            case NWAM_ACTION_REMOVE: {
/*                 NwamuiNcu *ncu = get_ncu_by_device_name( daemon, ncp, name ); */
/*                 g_debug("Interface %s removed", name ); */
/*                 if ( ncu ) { */
/*                     nwamui_ncp_remove_ncu( ncp, ncu ); */
/*                     g_object_unref(ncu); */
/*                 } */
            }
                break;
            default:
                g_debug( "%s\t%s %s %s",
                  nwam_event_type_to_string(nwamevent->nwe_type),
                  nwamevent->nwe_data.nwe_link_action.nwe_name,
                  nwam_action_to_string(nwamevent->nwe_data.nwe_link_action.nwe_action));
                break;
            }
            if ( ncp ) {
                g_object_unref(G_OBJECT(ncp));
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
            nwamui_daemon_handle_object_state_event( daemon, nwamevent );
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
            nwamui_daemon_handle_object_action_event( daemon, nwamevent );
            /* Update daemon status */
            nwamui_daemon_update_status(daemon);
            break;

		case NWAM_EVENT_TYPE_WLAN_SCAN_REPORT: {
                const gchar*    name = nwamevent->nwe_data.nwe_wlan_info.nwe_name;
                g_debug("Wireless networks found on %s", name );

                if ( nwamui_ncp_get_wireless_link_num(daemon->prv->auto_ncp) > 0 ) {
                    NwamuiNcu * ncu = nwamui_ncp_get_ncu_by_device_name(daemon->prv->active_ncp, name);

                    /* This is strange, this event may be emitted after the ncu
                     * is destroyed. So work around it. Maybe a bug of nwamd.
                     */
                    if (ncu) {
                        daemon->prv->emit_wlan_changed_signals = TRUE;

                        nwamui_daemon_dispatch_wifi_scan_events_from_cache(daemon);

                        g_signal_emit (daemon,
                          nwamui_daemon_signals[DAEMON_INFO],
                          0, /* details */
                          NWAMUI_DAEMON_INFO_WLAN_CHANGED,
                          ncu,
                          g_strdup_printf(_("New wireless networks found.")));

                        g_object_unref(ncu);
                    }
                } else {
                    g_debug("Num wifi i/fs returned 0");
                }
            }
            break;
        case NWAM_EVENT_TYPE_WLAN_CONNECTION_REPORT: {
                const gchar*   device    = nwamevent->nwe_data.nwe_wlan_info.nwe_name;
                NwamuiNcu *    ncu       = nwamui_ncp_get_ncu_by_device_name(daemon->prv->active_ncp, device);
                NwamuiWifiNet* wifi      = NULL;
                const gchar*   name      = NULL;
                const gchar*   wifi_name = NULL;


                g_assert(ncu);
                wifi = nwamui_ncu_get_wifi_info(ncu);
                if ( wifi ) {
                    wifi_name = nwamui_object_get_name(NWAMUI_OBJECT(wifi));
                }
                name = nwamui_object_get_name(NWAMUI_OBJECT(ncu));

                if ( nwamevent->nwe_data.nwe_wlan_info.nwe_connected ) {

                    g_debug("Wireless network '%s' connected", 
                            nwamevent->nwe_data.nwe_wlan_info.nwe_wlans[0].nww_essid );

                    if ( wifi && wifi_name && 
                         strncmp( wifi_name, nwamevent->nwe_data.nwe_wlan_info.nwe_wlans[0].nww_essid, NWAM_MAX_NAME_LEN) == 0 ) {
                        nwamui_wifi_net_set_status(wifi, NWAMUI_WIFI_STATUS_CONNECTED);
                        nwamui_wifi_net_update_from_wlan_t( wifi, 
                                                &(nwamevent->nwe_data.nwe_wlan_info.nwe_wlans[0]));
                    }
                    else {
                        if ( wifi ) {
                            /* Different wifi_net */
                            nwamui_ncu_set_wifi_info(ncu, NULL);
                            g_object_unref(wifi); 
                        }

                        if ( (wifi = nwamui_daemon_find_fav_wifi_net_by_name( daemon, 
                                        nwamevent->nwe_data.nwe_wlan_info.nwe_wlans[0].nww_essid ) ) != NULL ) {
                            /* Exists as a favourite, so re-use */
                            nwamui_wifi_net_update_from_wlan_t( wifi, 
                                                    &(nwamevent->nwe_data.nwe_wlan_info.nwe_wlans[0]));
                        }
                        else {
                            wifi = nwamui_ncu_wifi_hash_insert_or_update_from_wlan_t( ncu, 
                                                    &(nwamevent->nwe_data.nwe_wlan_info.nwe_wlans[0]));
                        }

                        nwamui_ncu_set_wifi_info(ncu, wifi);
                        /* Set wifi status after set ncu, since UI may check
                         * ncu info. */
                        nwamui_wifi_net_set_status(wifi, NWAMUI_WIFI_STATUS_CONNECTED);
                    }

                    /* Is this needed for Phase 1? */
                    /* nwamui_daemon_setup_dhcp_or_wep_key_timeout( daemon, ncu ); */

                    g_signal_emit (daemon,
                      nwamui_daemon_signals[DAEMON_INFO],
                      0, /* details */
                      NWAMUI_DAEMON_INFO_WLAN_CONNECTED,
                      wifi,
                      g_strdup_printf("%s", nwamevent->nwe_data.nwe_wlan_info.nwe_wlans[0].nww_essid));
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
                if ( wifi )
                    g_object_unref(wifi);

                if ( ncu )
                    g_object_unref(ncu);

            }
            break;
        case NWAM_EVENT_TYPE_WLAN_NEED_CHOICE: {
                const gchar*    device = nwamevent->nwe_data.nwe_wlan_info.nwe_name;
                NwamuiNcu *     ncu = nwamui_ncp_get_ncu_by_device_name(daemon->prv->active_ncp, device);

                g_debug("No suitable wireless networks found, selection needed.");

                if (daemon->prv->emit_wlan_changed_signals) {
                    nwamui_daemon_dispatch_wifi_scan_events_from_cache(daemon);

                    g_signal_emit (daemon,
                      nwamui_daemon_signals[DAEMON_INFO],
                      0, /* details */
                      NWAMUI_DAEMON_INFO_WLAN_CHANGED,
                      NULL,
                      g_strdup_printf(_("New wireless networks found.")));
                }

                g_signal_emit (daemon,
                  nwamui_daemon_signals[WIFI_SELECTION_NEEDED],
                  0, /* details */
                  ncu );

                g_object_unref(ncu);
            }
            break;

        case NWAM_EVENT_TYPE_WLAN_NEED_KEY: {
                const gchar*    device = nwamevent->nwe_data.nwe_wlan_info.nwe_name;
                NwamuiNcu*      ncu = nwamui_ncp_get_ncu_by_device_name(daemon->prv->active_ncp, device);
                NwamuiWifiNet*  wifi = nwamui_ncu_get_wifi_info( ncu );


                if ( wifi == NULL ) {
                    if ( (wifi = nwamui_daemon_find_fav_wifi_net_by_name( daemon, 
                                    nwamevent->nwe_data.nwe_wlan_info.nwe_wlans[0].nww_essid ) ) != NULL ) {
                        /* Exists as a favourite, so re-use */
                        nwamui_wifi_net_update_from_wlan_t( wifi, 
                                                &(nwamevent->nwe_data.nwe_wlan_info.nwe_wlans[0]));
                    } else {
                        wifi = nwamui_ncu_wifi_hash_lookup_by_essid( ncu, 
                                    nwamevent->nwe_data.nwe_wlan_info.nwe_wlans[0].nww_essid );
                    }
                    nwamui_ncu_set_wifi_info( ncu, wifi );
                }
                else {
                    const char* essid = nwamui_object_get_name(NWAMUI_OBJECT(wifi));

                    if( essid == NULL ||
                        strncmp( essid, nwamevent->nwe_data.nwe_wlan_info.nwe_wlans[0].nww_essid, strlen(essid) ) != 0) {
                        /* ESSIDs are different, so we should update the ncu */
                        NwamuiWifiNet* new_wifi = NULL;

                        if ( (new_wifi = nwamui_daemon_find_fav_wifi_net_by_name( daemon, 
                                                    nwamevent->nwe_data.nwe_wlan_info.nwe_wlans[0].nww_essid ) ) != NULL ) {
                            /* Exists as a favourite, so re-use */
                            nwamui_wifi_net_update_from_wlan_t( new_wifi, 
                                                    &(nwamevent->nwe_data.nwe_wlan_info.nwe_wlans[0]));
                        } else {
                            new_wifi = nwamui_ncu_wifi_hash_lookup_by_essid( ncu, 
                                            nwamevent->nwe_data.nwe_wlan_info.nwe_wlans[0].nww_essid );
                        }

                        g_debug("deWlanKeyNeeded: WifiNets differ, replacing");
                        nwamui_ncu_set_wifi_info( ncu, new_wifi );
                        g_object_unref(wifi);
                        wifi = new_wifi;
                    }

                }

                if ( wifi != NULL ) {
                    g_signal_emit (daemon,
                      nwamui_daemon_signals[WIFI_KEY_NEEDED],
                      0, /* details */
                      wifi );
                }
                else {
                    g_warning("Unable to find wifi object for essid '%s' to request key",
                              nwamevent->nwe_data.nwe_wlan_info.nwe_wlans[0].nww_essid ); 
                }
                g_object_unref(ncu);
            }
            break;

#ifdef NWAM_EVENT_TYPE_QUEUE_QUIET
        case NWAM_EVENT_TYPE_QUEUE_QUIET: {
                /* Nothing to do. */
            }
            break;
#endif

        default:
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
    case NWAMUI_DAEMON_INFO_WLAN_CHANGED:
        /* fall through */
    default:
        g_assert_not_reached();
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
    nwamui_daemon_status_t  old_status;
    guint                   old_status_flags;
    guint                   status_flags = 0;
    nwamui_daemon_status_t  new_status = NWAMUI_DAEMON_STATUS_UNINITIALIZED;
    NwamuiDaemonPrivate    *prv;
    GQueue                 *signal_queue = g_queue_new();
    gpointer                ptr;

    /* 
     * Determine status from objects 
     */
    g_assert(NWAMUI_IS_DAEMON(daemon));

    old_status = daemon->prv->status;
    old_status_flags = daemon->prv->status_flags;
    prv = NWAMUI_DAEMON(daemon)->prv;

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
            if ( !nwamui_ncp_all_ncus_online( prv->active_ncp, &needs_wifi_selection, &needs_wifi_key) ) {
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

        /* According to comments#15,16 of 12079, we don't case ENMs state. */
        /* Assuming we've no found an error yet, check the ENMs */
#if 0
        new_status = NWAMUI_DAEMON_STATUS_UNINITIALIZED;
        g_list_foreach( daemon->prv->enm_list, check_enm_online, &new_status );
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
        daemon->prv->status_flags = status_flags;
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
    NwamuiDaemonPrivate    *prv;
    const char             *object_name;

    if ( daemon == NULL || !NWAMUI_IS_DAEMON(daemon) ) {
        return;
    }

    g_return_if_fail( nwamevent != NULL );

    daemon->prv->communicate_change_to_daemon = FALSE;

    prv = daemon->prv;
    object_name = nwamevent->nwe_data.nwe_object_action.nwe_name;

    switch ( nwamevent->nwe_data.nwe_object_action.nwe_object_type ) {
    case NWAM_OBJECT_TYPE_NCP: {
        NwamuiNcp   *ncp = nwamui_daemon_get_ncp_by_name( daemon, object_name );

        switch ( nwamevent->nwe_data.nwe_object_action.nwe_action ) {
        case NWAM_ACTION_ADD: {
            g_assert(ncp == NULL);
            if (!ncp) {
/*                 g_warning("We probably shouldn't do this, because we do remove NCPs."); */
/*                 nwamui_object_reload(NWAMUI_OBJECT(ncp)); */
/*             } else { */
                ncp = nwamui_ncp_new( object_name );
                nwamui_daemon_ncp_append(daemon, ncp );
            }
        }
            break;
        case NWAM_ACTION_DISABLE:
            break;
        case NWAM_ACTION_ENABLE: {
            /* Don't use nwamui_ncp_set_active() since it will
             * actually cause a switch again...
             */
            if ( daemon->prv->active_ncp != NULL ) {
                g_object_unref(G_OBJECT(daemon->prv->active_ncp));
            }

            daemon->prv->active_ncp = NWAMUI_NCP(g_object_ref( ncp ));
            /* We need reload NCP since it may changes when it isn't active. */
/*             nwamui_object_reload(NWAMUI_OBJECT(ncp)); */
            g_object_notify(G_OBJECT(daemon), "active_ncp");
        }
            break;
        case NWAM_ACTION_REFRESH:
            nwamui_object_reload(NWAMUI_OBJECT(ncp));
            break;
        case NWAM_ACTION_REMOVE:
            /* Treat REMOVE and DESTROY as the same */
        case NWAM_ACTION_DESTROY: {
            if (ncp) {
                if ( ncp == prv->active_ncp ) {
                    nwamui_warning("Removing active ncp '%s'!", object_name );
                }
                nwamui_daemon_ncp_remove(daemon, ncp );
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
        NwamuiNcu       *ncu;
        char            *device_name = NULL;
        nwam_ncu_type_t  nwam_ncu_type;

        ncp = NWAMUI_OBJECT(nwamui_daemon_get_ncp_by_name(daemon,
            nwamevent->nwe_data.nwe_object_action.nwe_parent));
        /* NCU's come in the typed name format (e.g. link:ath0) */
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

        switch ( nwamevent->nwe_data.nwe_object_action.nwe_action ) {
        case NWAM_ACTION_ADD: {
            if (ncu == NULL) {
                nwamui_object_reload(NWAMUI_OBJECT(ncp));
            } else {
                /* interface:name/link:name, so a NCU will be added two times. */
                nwamui_object_reload(NWAMUI_OBJECT(ncu));
            }
        }
            break;
        case NWAM_ACTION_DISABLE: {
            if ( !nwamui_ncu_has_modifications( ncu ) ) {
                nwamui_object_reload(NWAMUI_OBJECT(ncu));
            }
        }
            break;
        case NWAM_ACTION_ENABLE: {
            if ( !nwamui_ncu_has_modifications( ncu ) ) {
                nwamui_object_reload(NWAMUI_OBJECT(ncu));
            }
        }
            break;
        case NWAM_ACTION_REFRESH: {
            if ( !nwamui_ncu_has_modifications( ncu ) ) {
                nwamui_object_reload(NWAMUI_OBJECT(ncu));
            }
        }
            break;
        case NWAM_ACTION_REMOVE:
            /* Fall through. */
        case NWAM_ACTION_DESTROY: {
            if (ncu)
                nwamui_ncp_remove_ncu(NWAMUI_NCP(ncp), ncu );
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
        NwamuiEnv   *env = nwamui_daemon_get_env_by_name( daemon, object_name );

        switch ( nwamevent->nwe_data.nwe_object_action.nwe_action ) {
        case NWAM_ACTION_ADD: {
            if (env == NULL) {
                env = nwamui_env_new( object_name );
                nwamui_daemon_env_append( daemon, env );
            }
        }
            break;
        case NWAM_ACTION_DISABLE: {
            nwamui_object_set_enabled(NWAMUI_OBJECT(env), FALSE);
            g_object_notify(G_OBJECT(daemon), "env_selection_mode");
        }
            break;
        case NWAM_ACTION_ENABLE: {
            /* Ensure that correct active env pointer */
            if ( prv->active_env != env ) {
                /* New active ENV */
                nwamui_daemon_set_active_env( daemon, env );
            }

            nwamui_object_set_enabled(NWAMUI_OBJECT(env), TRUE);
            g_object_notify(G_OBJECT(daemon), "env_selection_mode");
        }
            break;
        case NWAM_ACTION_REFRESH: {
            nwamui_object_reload(NWAMUI_OBJECT(env));
        }
            break;
        case NWAM_ACTION_REMOVE:
            /* Fall through, remove/destroy are the same here */
        case NWAM_ACTION_DESTROY: {
            if (env) {
                if ( prv->active_env == env ) {
                    g_object_unref(prv->active_env);
                    prv->active_env = NULL;
                }
                nwamui_daemon_env_remove( daemon, env );
            }
        }
            break;
        }
        if (env)
            g_object_unref(env);
    }
        break;
    case NWAM_OBJECT_TYPE_ENM: {
        NwamuiEnm   *enm = nwamui_daemon_get_enm_by_name( daemon, object_name );

        switch ( nwamevent->nwe_data.nwe_object_action.nwe_action ) {
        case NWAM_ACTION_ADD: {
            if ( enm == NULL ) {
                enm = nwamui_enm_new( object_name );
                nwamui_daemon_enm_append( daemon, enm );
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
            nwamui_object_reload(NWAMUI_OBJECT(enm));
        }
            break;
        case NWAM_ACTION_REMOVE:
            /* Fall through, basically the same operation */
        case NWAM_ACTION_DESTROY: {
            if (enm)
                nwamui_daemon_enm_remove( daemon, enm );
        }
            break;
        }
        if (enm)
            g_object_unref(enm);
    }
        break;
    case NWAM_OBJECT_TYPE_KNOWN_WLAN: {
        NwamuiWifiNet   *wifi;
        NwamuiNcu       *wireless_ncu = nwamui_daemon_get_first_wireless_ncu( daemon );
            
        if ( wireless_ncu == NULL ) {
            nwamui_debug("Got unexpected NULL wireless_ncu when adding a favourite", NULL );
            break;
        }

        wifi = nwamui_daemon_find_fav_wifi_net_by_name( daemon, object_name );

        switch ( nwamevent->nwe_data.nwe_object_action.nwe_action ) {
        case NWAM_ACTION_ADD: {
            nwam_error_t                nerr;
            nwam_known_wlan_handle_t    known_wlan_h;

            nwamui_debug("Got NWAM_ACTION_ADD for object %s", 
              object_name );

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
                    nwamui_wifi_net_update_with_handle( wifi, NULL ); 
                }
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
                daemon->prv->communicate_change_to_daemon = FALSE;
                nwamui_daemon_remove_wifi_fav(daemon, wifi );
                daemon->prv->communicate_change_to_daemon = TRUE;
            }
        }
            break;
        }
        if ( wifi != NULL ) {
            g_object_unref(wifi);
        }
        if ( wireless_ncu != NULL ) {
            g_object_unref(wireless_ncu);
        }
    }
        break;
    }
    daemon->prv->communicate_change_to_daemon = TRUE;
}

static void
nwamui_daemon_handle_object_state_event( NwamuiDaemon   *daemon, nwam_event_t nwamevent )
{
    NwamuiDaemonPrivate    *prv                = NWAMUI_DAEMON(daemon)->prv;
    guint                   status_flags       = 0;
    nwam_object_type_t      object_type        = nwamevent->nwe_data.nwe_object_state.nwe_object_type;
    const char*             object_name        = nwamevent->nwe_data.nwe_object_state.nwe_name;
    nwam_state_t            object_state       = nwamevent->nwe_data.nwe_object_state.nwe_state;
    nwam_aux_state_t        object_aux_state   = nwamevent->nwe_data.nwe_object_state.nwe_aux_state;
    NwamuiObject *obj = NULL;

    /* First check that the daemon is connected to nwamd */
    if ( prv->connected_to_nwamd ) {
        /* Now we need to check all objects in the system */
        switch ( object_type ) {
        case NWAM_OBJECT_TYPE_NCU: {
            char            *device_name = NULL;
            nwam_ncu_type_t  nwam_ncu_type;
            NwamuiNcp       *ncp;

            ncp = nwamui_daemon_get_ncp_by_name(daemon, nwamevent->nwe_data.nwe_object_state.nwe_parent);
            if (ncp) {
                /* NCU's come in the typed name format (e.g. link:ath0) */
                if ( nwam_ncu_typed_name_to_name(object_name, &nwam_ncu_type, &device_name ) == NWAM_SUCCESS ) {
                    obj = NWAMUI_OBJECT(nwamui_ncp_get_ncu_by_device_name(ncp, device_name));
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
            obj = NWAMUI_OBJECT(nwamui_daemon_get_ncp_by_name( daemon, object_name ));
            nwamui_object_set_nwam_state(obj, object_state, object_aux_state);
            if ( object_state == NWAM_STATE_ONLINE && object_aux_state == NWAM_AUX_STATE_ACTIVE ) {
                nwamui_daemon_set_active_ncp( daemon, NWAMUI_NCP(obj));
            } else {
                status_flags |= STATUS_REASON_NCP;
            }
            g_object_unref(obj);
            break;
        case NWAM_OBJECT_TYPE_LOC:
            obj = NWAMUI_OBJECT(nwamui_daemon_get_env_by_name( daemon, object_name ));
            nwamui_object_set_nwam_state(obj, object_state, object_aux_state);
            if ( object_state == NWAM_STATE_ONLINE && object_aux_state == NWAM_AUX_STATE_ACTIVE ) {
                nwamui_daemon_set_active_env( daemon, NWAMUI_ENV(obj) );
            } else {
                status_flags |= STATUS_REASON_LOC;
            }
            g_object_unref(obj);
            break;
        case NWAM_OBJECT_TYPE_ENM:
            obj = NWAMUI_OBJECT(nwamui_daemon_get_enm_by_name( daemon, object_name ));
            nwamui_object_set_nwam_state(obj, object_state, object_aux_state);
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
check_enm_online( gpointer obj, gpointer user_data )
{
    NwamuiObject            *nobj = NWAMUI_OBJECT(obj);
    nwamui_daemon_status_t  *new_status_p = (nwamui_daemon_status_t*)user_data; 
    nwam_state_t             state;
    nwam_aux_state_t         aux_state;

    if ( nwamui_object_get_enabled( NWAMUI_OBJECT(nobj) ) ) {
        state = nwamui_object_get_nwam_state( nobj, &aux_state, NULL);

        if ( state != NWAM_STATE_ONLINE || aux_state != NWAM_AUX_STATE_ACTIVE ) {
            *new_status_p = NWAMUI_DAEMON_STATUS_NEEDS_ATTENTION;
        }
    }
}


extern nwamui_daemon_status_t 
nwamui_daemon_get_status_icon_type( NwamuiDaemon *daemon )
{
    if ( daemon != NULL ) {
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
    char *name;
    nwam_error_t nerr;
    NwamuiEnv* new_env;
        
    NwamuiDaemon        *self = NWAMUI_DAEMON(data);
    NwamuiDaemonPrivate *prv = self->prv;

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
            prv->env_list = g_list_append(prv->env_list, (gpointer)new_env);
        }
        free(name);
    }
            
    if ( new_env != NULL ) {
        if ( nwamui_object_get_active(NWAMUI_OBJECT(new_env)) ) {
            prv->active_env = NWAMUI_ENV(g_object_ref(new_env));
        }
    }

    return(0);
}

static int
nwam_enm_walker_cb (nwam_enm_handle_t enm, void *data)
{
    char *name;
    nwam_error_t nerr;
    NwamuiEnm* new_enm;
    
    NwamuiDaemon        *self = NWAMUI_DAEMON(data);
    NwamuiDaemonPrivate *prv = self->prv;

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
            prv->enm_list = g_list_append(prv->enm_list, (gpointer)new_enm);
        }
        free(name);
    }

    return(0);
}

static int
nwam_ncp_walker_cb (nwam_ncp_handle_t ncp, void *data)
{
    NwamuiDaemon        *self = NWAMUI_DAEMON(data);
    NwamuiDaemonPrivate *prv  = self->prv;
    gchar               *name;
    nwam_error_t         nerr;
    NwamuiNcp           *new_ncp;

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
            prv->ncp_list = g_list_append(prv->ncp_list, (gpointer)new_ncp);
        }
        free(name);
    }
        
    if ( new_ncp != NULL ) {
        name = (gchar *)nwamui_object_get_name(NWAMUI_OBJECT(new_ncp));
        if ( name != NULL ) { 
            if ( strncmp( name, NWAM_NCP_NAME_AUTOMATIC, strlen(NWAM_NCP_NAME_AUTOMATIC)) == 0 ) {
                if ( prv->auto_ncp != new_ncp ) {
                    prv->auto_ncp = NWAMUI_NCP(g_object_ref( new_ncp ));
                }
            }
        }
        if ( nwamui_ncp_get_active( new_ncp ) ) {
            prv->active_ncp = NWAMUI_NCP(g_object_ref( new_ncp ));
        }
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
    
    wifi = nwamui_ncu_wifi_hash_insert_or_update_from_handle( walker_data->wireless_ncu, wlan_h);
        
    if ( wifi != NULL ) {
        walker_data->fav_list = g_list_append(walker_data->fav_list, (gpointer)wifi );
    }

    return(0);
}

