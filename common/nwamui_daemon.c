/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set expandtab ts=4 shiftwidth=4: */

/* 
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
 */

/*
 * Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
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
#include "nwam-scf.h"

static NwamuiDaemon*    instance        = NULL;

/* Use above mutex for accessing these variables */
static GStaticMutex nwam_event_mutex = G_STATIC_MUTEX_INIT;
static gboolean nwam_event_thread_terminate = FALSE; /* To tell event thread to terminate set to TRUE */
static gboolean nwam_init_done = FALSE; /* Whether to call nwam_events_fini() or not */
/* End of mutex protected variables */


#define WLAN_TIMEOUT_SCAN_RATE_SEC (60)
#define WEP_TIMEOUT_SEC (20)

#define DEBUG_STATUS( name, state, aux_state, status_flag )             \
    nwamui_debug("line: %d : name = %s : state = %d (%s) : aux_state = %d (%s) status_flag: %02x", \
      __LINE__, name,                                                   \
      (state), nwam_state_to_string(state),                             \
      (aux_state), nwam_aux_state_to_string(aux_state),                 \
      status_flag)

typedef struct _to_emit {
    guint       event;
    gpointer    data;
} to_emit_t;

enum {
    PROP_ACTIVE_ENV = 1,
    PROP_ACTIVE_NCP,
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
	MANAGED_NCP = 0,
	MANAGED_LOC,
	MANAGED_ENM,
	MANAGED_KNOWN_WLAN,
	N_MANAGED
};

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
    guint                   wep_timeout_id;
    GQueue                 *wlan_scan_queue;
    gint                    num_scanned_wifi;
    gint                    online_enm_num;
};

#define NWAMUI_DAEMON_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE((o), NWAMUI_TYPE_DAEMON, NwamuiDaemonPrivate))

typedef struct _NwamuiEvent {
    nwamui_daemon_info_t e;     /* ui daemon event type */
    nwam_event_t         nwamevent; /* daemon data */
    NwamuiDaemon*        daemon;
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

static void nwamui_daemon_set_num_scanned_wifi(NwamuiDaemon* self,  gint num_scanned_wifi);

static void nwamui_daemon_finalize (     NwamuiDaemon *self);

static void check_nwamui_object_online( gpointer obj, gpointer user_data );
static void nwamui_daemon_update_online_enm_num(NwamuiDaemon *self);
static void check_nwamui_object_online_num( gpointer obj, gpointer user_data );

static void nwamui_daemon_update_status( NwamuiDaemon   *daemon );

static gboolean nwamui_daemon_nwam_connect( gboolean block );
static gboolean nwamui_is_nwam_enabled( void );

static void     nwamui_daemon_nwam_disconnect( void );

static void nwamui_daemon_handle_object_action_event( NwamuiDaemon   *daemon, nwam_event_t nwamevent );
static void nwamui_daemon_handle_object_state_event( NwamuiDaemon   *daemon, nwam_event_t nwamevent );
static void nwamui_daemon_set_status( NwamuiDaemon* self, nwamui_daemon_status_t status );

static void     nwamui_object_real_reload(NwamuiObject* object);
static gboolean nwamui_object_real_validate(NwamuiObject *object, gchar **prop_name_ret);
static gboolean nwamui_object_real_commit(NwamuiObject *object);
static void     nwamui_object_real_event(NwamuiObject *object, guint event, gpointer data);
static void     nwamui_object_real_add(NwamuiObject *object, NwamuiObject *child);
static void     nwamui_object_real_remove(NwamuiObject *object, NwamuiObject *child);


/* Callbacks */
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

    nwamuiobject_class->reload = nwamui_object_real_reload;
    nwamuiobject_class->validate = nwamui_object_real_validate;
    nwamuiobject_class->commit = nwamui_object_real_commit;
    nwamuiobject_class->event = nwamui_object_real_event;
    nwamuiobject_class->add = nwamui_object_real_add;
    nwamuiobject_class->remove = nwamui_object_real_remove;

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

}

static void
nwamui_daemon_init (NwamuiDaemon *self)
{
    NwamuiDaemonPrivate *prv      = NWAMUI_DAEMON_GET_PRIVATE(self);
    GError              *error    = NULL;
    nwam_error_t         nerr;
    
    self->prv = prv;
    
    prv->nwam_events_gthread = g_thread_create(nwam_events_thread, g_object_ref(self), TRUE, &error);
    if( prv->nwam_events_gthread == NULL ) {
        g_debug("Error creating nwam events thread: %s", (error && error->message)?error->message:"" );
    }

    /* nwam_events_thread include the initial function call, so this is
     * dup. Note capplet require this dup call because capplet doesn't handle
     * daemon related info changes, e.g. NCP list changes, so it may lose info
     * without this dup call.
     */
    nwamui_object_real_reload(NWAMUI_OBJECT(self));
}

/**
 * Do not populate fav/ wlan in this function call, because this function will
 * be invoked two times at initilly time.
 */
static void
nwamui_object_real_reload(NwamuiObject* object)
{
    NwamuiDaemonPrivate *prv      = NWAMUI_DAEMON_GET_PRIVATE(object);
    NwamuiDaemon        *self     = NWAMUI_DAEMON(object);
    nwam_error_t nerr;
    int cbret;
    
    /* NCPs */

    /* Get list of Ncps from libnwam */
    prv->temp_list = g_list_copy( prv->managed_list[MANAGED_NCP] );
    g_debug ("### nwam_walk_ncps start ###");
    nerr = nwam_walk_ncps (nwam_ncp_walker_cb, (void *)self, 0, &cbret);
    if (nerr == NWAM_SUCCESS) {
        for(;
            prv->temp_list != NULL;
            prv->temp_list = g_list_delete_link(prv->temp_list, prv->temp_list) ) {
            nwamui_object_remove(NWAMUI_OBJECT(self), NWAMUI_OBJECT(prv->temp_list->data));
        }
    } else {
        g_warning("nwam_walk_ncps %s", nwam_strerror (nerr));
        g_list_free(prv->temp_list);
        prv->temp_list = NULL;
    }
    g_debug ("### nwam_walk_ncps  end ###");

    /* Env / Locations */
    prv->temp_list = g_list_copy( prv->managed_list[MANAGED_LOC] );
    g_debug ("### nwam_walk_locs start ###");
    nerr = nwam_walk_locs (nwam_loc_walker_cb, (void *)self, 0, &cbret);
    if (nerr == NWAM_SUCCESS) {
        for(;
            prv->temp_list != NULL;
            prv->temp_list = g_list_delete_link(prv->temp_list, prv->temp_list) ) {
            nwamui_object_remove(NWAMUI_OBJECT(self), NWAMUI_OBJECT(prv->temp_list->data));
        }
    } else {
        g_warning("nwam_walk_locs %s", nwam_strerror (nerr));
        g_list_free(prv->temp_list);
        prv->temp_list = NULL;
    }
    g_debug ("### nwam_walk_locs  end ###");

    /* ENMs */
    prv->temp_list = g_list_copy( prv->managed_list[MANAGED_ENM] );
    g_debug ("### nwam_walk_enms start ###");
    nerr = nwam_walk_enms (nwam_enm_walker_cb, (void *)self, 0, &cbret);
    if (nerr == NWAM_SUCCESS) {
        for(;
            prv->temp_list != NULL;
            prv->temp_list = g_list_delete_link(prv->temp_list, prv->temp_list) ) {
            nwamui_object_remove(NWAMUI_OBJECT(self), NWAMUI_OBJECT(prv->temp_list->data));
        }
    } else {
        g_warning("nwam_walk_enms %s", nwam_strerror (nerr));
        g_list_free(prv->temp_list);
        prv->temp_list = NULL;
    }
    g_debug ("### nwam_walk_enms  end ###");

    /* Will generate an event if status changes */
    nwamui_daemon_update_status(self);

    nwamui_daemon_update_online_enm_num(self);

    /* KnownWlans */
    prv->temp_list = g_list_copy(prv->managed_list[MANAGED_KNOWN_WLAN]);
    g_debug ("### nwam_walk_know_wlans start ###");
    nerr = nwam_walk_known_wlans(nwam_known_wlan_walker_cb, (void *)self,
      NWAM_FLAG_KNOWN_WLAN_WALK_PRIORITY_ORDER, &cbret);
    if (nerr == NWAM_SUCCESS) {
        for(;
            prv->temp_list != NULL;
            prv->temp_list = g_list_delete_link(prv->temp_list, prv->temp_list) ) {
            nwamui_object_remove(NWAMUI_OBJECT(self), NWAMUI_OBJECT(prv->temp_list->data));
        }
    } else {
        g_warning("nwam_walk_known_wlans %s", nwam_strerror(nerr));
        g_list_free(prv->temp_list);
        prv->temp_list = NULL;
    }
    g_debug ("### nwam_walk_know_wlans  end ###");
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
        case PROP_STATUS:
            g_value_set_int(value, nwamui_daemon_get_status(self));
            break;
        case PROP_NUM_SCANNED_WIFI: {
                g_value_set_int(value, self->prv->num_scanned_wifi);
            }
            break;
        case PROP_ONLINE_ENM_NUM:
            g_value_set_int(value, nwamui_daemon_get_online_enm_num(self));
            break;
        case PROP_ENV_SELECTION_MODE:
            g_value_set_boolean(value, nwamui_daemon_env_selection_is_manual(self));
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
    NwamuiDaemonPrivate *prv      = NWAMUI_DAEMON_GET_PRIVATE(self);
    int             retval;
    int             err;
    nwam_error_t    nerr;
    
    nwamui_daemon_nwam_disconnect();

    if ( prv->nwam_events_gthread != NULL ) {
        nwamui_daemon_terminate_event_thread( self );
    }
    
    if (prv->active_env != NULL ) {
        g_object_unref( G_OBJECT(prv->active_env) );
    }

    for (gint i = 0; i < N_MANAGED; i++) {
        if (prv->managed_list[i]) {
            g_list_foreach(prv->managed_list[i], nwamui_util_obj_unref, NULL);
            g_list_free(prv->managed_list[i]);
        }
    }
    
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
    GList *found_elem;
    GString       *have_names = NULL;
    NwamuiObject  *ncp        = NULL;

    g_return_val_if_fail( NWAMUI_IS_DAEMON(self) && name != NULL, ncp );

    found_elem = g_list_find_custom(self->prv->managed_list[MANAGED_NCP], name,
      (GCompareFunc)nwamui_util_find_nwamui_object_by_name);
    if (found_elem != NULL) {
        ncp = NWAMUI_OBJECT(g_object_ref(G_OBJECT(found_elem->data)));
    }

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
    GList *found_elem;
    NwamuiObject*  env = NULL;

    g_assert( NWAMUI_IS_DAEMON( self ) );

    g_return_val_if_fail( NWAMUI_IS_DAEMON(self) && name != NULL, env );

    found_elem = g_list_find_custom(self->prv->managed_list[MANAGED_LOC], name,
      (GCompareFunc)nwamui_util_find_nwamui_object_by_name);
    if (found_elem != NULL) {
        env = NWAMUI_OBJECT(g_object_ref(G_OBJECT(found_elem->data)));
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
    GList *found_elem;
    NwamuiObject*  enm = NULL;

    g_assert( NWAMUI_IS_DAEMON( self ) );

    g_return_val_if_fail( NWAMUI_IS_DAEMON(self) && name != NULL, enm );

    found_elem = g_list_find_custom(self->prv->managed_list[MANAGED_ENM], name,
      (GCompareFunc)nwamui_util_find_nwamui_object_by_name);
    if (found_elem != NULL) {
        enm = NWAMUI_OBJECT(g_object_ref(G_OBJECT(found_elem->data)));
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

extern NwamuiNcu*
nwamui_ncp_get_first_wireless_ncu_from_active_ncp(NwamuiDaemon *self)
{
    NwamuiDaemonPrivate  *prv = NWAMUI_DAEMON_GET_PRIVATE(self);

    return nwamui_ncp_get_first_wireless_ncu(NWAMUI_NCP(prv->active_ncp));
}

static void
nwamui_object_real_event(NwamuiObject *object, guint event, gpointer data)
{
    gchar *msg = NULL;

    switch (event) {
    case NWAMUI_DAEMON_INFO_WLAN_CONNECTED:
        msg = g_strdup_printf("%s connected", nwamui_object_get_name(data));
        break;
    case NWAMUI_DAEMON_INFO_WLAN_CONNECT_FAILED:
        msg = g_strdup_printf("%s connection to wireless lan failed", nwamui_object_get_name(data));
        break;
    case NWAMUI_DAEMON_INFO_WLANS_CHANGED:
        if (data) {
            msg = g_strdup_printf("NCU %s: New wireless networks found.", nwamui_object_get_name(data));
        } else {
            msg = g_strdup_printf("New wireless networks found.");
        }
        break;
    case NWAMUI_DAEMON_INFO_WIFI_SELECTION_NEEDED:
        msg = g_strdup_printf("Wireless selection needed for network interface '%s'", nwamui_ncu_get_display_name(data));
        break;

    case NWAMUI_DAEMON_INFO_WIFI_KEY_NEEDED:
        msg = g_strdup_printf("Wireless key needed for network '%s'", nwamui_object_get_name(data));
        break;

    case NWAMUI_DAEMON_INFO_GENERIC:
        msg = g_strdup(data);
        break;

    default:
        g_warning("Unknown NWAM event type %d.", event);
        return;
    }

    g_debug ("Daemon got information type %4d msg %s.", event, msg);
	
	g_free (msg);
}

/*
 * Compare two wifi_net objects by priority.
 */
static gint
find_compare_wifi_net_by_prio(gconstpointer a, gconstpointer b)
{
    return nwamui_object_sort(NWAMUI_OBJECT(a), NWAMUI_OBJECT(b), NWAMUI_OBJECT_SORT_BY_PRIO);
}

/**
 * nwamui_daemon_object_append:
 * @self: NwamuiDaemon*
 * @new_ncp: New #NwamuiObject to add
 *
 **/
static void
nwamui_object_real_add(NwamuiObject *object, NwamuiObject *child)
{
    NwamuiDaemonPrivate *prv  = NWAMUI_DAEMON_GET_PRIVATE(object);
    gint                 idx;
    GCompareFunc         func = (GCompareFunc)nwamui_object_sort_by_name;

    g_assert( NWAMUI_IS_DAEMON(object) );

    if ( NWAMUI_IS_NCP(child)) {
        idx = MANAGED_NCP;
    } else if ( NWAMUI_IS_ENV(child)) {
        idx = MANAGED_LOC;
    } else if ( NWAMUI_IS_ENM(child)) {
        idx = MANAGED_ENM;
    } else if (NWAMUI_IS_KNOWN_WLAN(child)) {
        /* Must check before check is_wifi_net, because it inherits WIFI_NET. */
        idx = MANAGED_KNOWN_WLAN;
        func = find_compare_wifi_net_by_prio;
    } else if (NWAMUI_IS_WIFI_NET(child)) {
        /* Ignore wifi, simple emit signal. */
        return;
    } else {
        g_warning("Unsupported object %s", g_type_name(G_TYPE_FROM_INSTANCE(child)));
        return;
    }

    if (!g_list_find(prv->managed_list[idx], child)) {
        prv->managed_list[idx] = g_list_insert_sorted(prv->managed_list[idx], (gpointer)g_object_ref(child), func);
        g_debug("Add '%s(0x%p)' to '%s'", nwamui_object_get_name(child), child, nwamui_object_get_name(object));
    } else {
        nwamui_warning("Found existing '%s(0x%p)' for '%s'", nwamui_object_get_name(child), child, nwamui_object_get_name(object));
        g_signal_stop_emission_by_name(object, "add");
    }
}

/**
 * nwamui_object_real_remove:
 * @self: NwamuiDaemon*
 * @ncp: #NwamuiObject to remove
 *
 * @returns: TRUE if successfully removed.
 *
 **/
static void
nwamui_object_real_remove(NwamuiObject *object, NwamuiObject *child)
{
    NwamuiDaemonPrivate  *prv = NWAMUI_DAEMON_GET_PRIVATE(object);
    gint         idx;
    
    g_assert(NWAMUI_IS_DAEMON(object));
    
    if ( NWAMUI_IS_NCP(child)) {
        idx = MANAGED_NCP;
    } else if ( NWAMUI_IS_ENV(child)) {
        idx = MANAGED_LOC;
    } else if ( NWAMUI_IS_ENM(child)) {
        idx = MANAGED_ENM;
    } else if (NWAMUI_IS_KNOWN_WLAN(child)) {
        /* Must check before check is_wifi_net, because it inherits WIFI_NET. */
        idx = MANAGED_KNOWN_WLAN;
    } else if (NWAMUI_IS_WIFI_NET(child)) {
        /* Ignore wifi, simple emit signal. */
        return;
    } else {
        g_warning("Unsupported object %s", g_type_name(G_TYPE_FROM_INSTANCE(child)));
        return;
    }

    if (nwamui_object_is_modifiable(child)) {
        g_assert(g_list_find(prv->managed_list[idx], child));
        prv->managed_list[idx] = g_list_remove(prv->managed_list[idx], (gpointer)child);
        g_debug("Remove '%s(0x%p)' from '%s'", nwamui_object_get_name(child), child, nwamui_object_get_name(object));
        g_object_unref(child);
    } else {
        g_warning("%s '%s' is not removeable", g_type_name(G_TYPE_FROM_INSTANCE(child)), nwamui_object_get_name(child));
        g_signal_stop_emission_by_name(object, "remove");
    }
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

/**
 * nwamui_daemon_find_fav_wifi_net_by_name
 * @self: NwamuiDaemon*
 * @returns: a NwamuiObject*, or NULL if not found.
 *
 * Searches the list of the Favourite (Preferred) Wifi Networks by name
 * (ESSID).
 *
 **/
extern NwamuiObject*
nwamui_daemon_find_fav_wifi_net_by_name(NwamuiDaemon *self, const gchar* name) 
{
    NwamuiObject *found_wifi_net = NULL;
    GList        *found_elem     = NULL;

    g_return_val_if_fail(NWAMUI_IS_DAEMON(self), NULL);
    g_return_val_if_fail(name, NULL);

    found_elem = g_list_find_custom(self->prv->managed_list[MANAGED_KNOWN_WLAN], name,
      (GCompareFunc)nwamui_util_find_nwamui_object_by_name);
    if (found_elem != NULL) {
        found_wifi_net = g_object_ref(G_OBJECT(found_elem->data));
    }
    
    return found_wifi_net;
}

static gboolean
nwamui_object_real_validate(NwamuiObject *object, gchar **prop_name_ret)
{
    return TRUE;
}

static gboolean
nwamui_object_real_commit(NwamuiObject *object)
{
    NwamuiDaemon *daemon = NWAMUI_DAEMON(object);
    gboolean      rval   = TRUE;

    g_return_val_if_fail (NWAMUI_IS_DAEMON(object), FALSE);

    /* Commit changed objects. */
    for (gint i = 0; rval && i < N_MANAGED; i++) {
        for (GList* idx = daemon->prv->managed_list[i]; idx; idx = g_list_next(idx)) {
            NwamuiObject *child = NWAMUI_OBJECT(idx->data);

            if (nwamui_object_has_modifications(child) && !nwamui_object_commit(child)) {
                rval = FALSE;
                break;
            }
        }
    }

    return rval;
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
        nwamui_object_add(NWAMUI_OBJECT(self), NWAMUI_OBJECT(wifi_net));
        break;

    case NWAMUI_WIFI_LIFE_MODIFIED:
        /* Ignore updated signal since data is updated will trigger its
         * notify.
         */
        /* nwamui_object_modified(NWAMUI_OBJECT(self), NWAMUI_OBJECT(wifi_net)); */
        break;

    case NWAMUI_WIFI_LIFE_DEAD:
        nwamui_object_remove(NWAMUI_OBJECT(self), NWAMUI_OBJECT(wifi_net));
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
                NwamuiObject  *fav_net = NULL;
                /* Ensure it's being cached in the ncu, and if already
                 * theere will update it with new information.
                 */
                wifi_net = nwamui_ncu_wifi_hash_insert_or_update_from_wlan_t(ncu, wlan_p);

                fav_net = nwamui_daemon_find_fav_wifi_net_by_name(daemon, wlan_p->nww_essid);
                if (fav_net != NULL) {
                    /* Exists as a favourite, so update it's information */
                    nwamui_known_wlan_update_from_wlan_t(NWAMUI_KNOWN_WLAN(fav_net), wlan_p);
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

            nwamui_object_event(NWAMUI_OBJECT(daemon),
              NWAMUI_DAEMON_INFO_WIFI_KEY_NEEDED,
              wifi);
        }

        g_free(ipv4_address);
    }
#endif    

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
                           g_timeout_add_seconds_full(G_PRIORITY_DEFAULT,
                             WEP_TIMEOUT_SEC,
                             wep_key_timeout_handler,
                             g_object_ref(ncu),
                             (GDestroyNotify)g_object_unref);
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
        nwamui_object_real_reload(NWAMUI_OBJECT(daemon));

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
                g_debug("%s  %s flag(%8X) valid(%u) added(%u)",
                  nwam_event_type_to_string(nwamevent->nwe_type),
                  nwamevent->nwe_data.nwe_if_state.nwe_name,
                  nwamevent->nwe_data.nwe_if_state.nwe_flags,
                  nwamevent->nwe_data.nwe_if_state.nwe_addr_valid,
                  nwamevent->nwe_data.nwe_if_state.nwe_addr_added);

            } else if (nwamevent->nwe_data.nwe_if_state.nwe_flags & (IFF_UP | IFF_RUNNING)) {
                NwamuiObject    *ncu;
                char             addr_str[INET6_ADDRSTRLEN];
                char             mask_str[INET6_ADDRSTRLEN];
                const gchar     *address = NULL;
                const gchar     *netmask = NULL;
                struct sockaddr *sa_addr;
                struct sockaddr *sa_mask;
                uint32_t         flags;
                int              plen;

                sa_addr = (struct sockaddr *)&(nwamevent->nwe_data.nwe_if_state.nwe_addr);
                sa_mask = (struct sockaddr *)&(nwamevent->nwe_data.nwe_if_state.nwe_netmask);
                plen = mask2plen(&(nwamevent->nwe_data.nwe_if_state.nwe_netmask));
                flags = nwamevent->nwe_data.nwe_if_state.nwe_flags;

                if (flags & IFF_IPV4) {
                    address = inet_ntop(sa_addr->sa_family,
                      &((struct sockaddr_in *)sa_addr)->sin_addr,
                      addr_str, INET6_ADDRSTRLEN);
                    netmask = inet_ntop(sa_mask->sa_family,
                      &((struct sockaddr_in *)sa_mask)->sin_addr,
                      mask_str, INET6_ADDRSTRLEN);

                    if (flags & IFF_DHCPRUNNING) {
                    }

                } else {
                    address = inet_ntop(sa_addr->sa_family,
                      &((struct sockaddr_in6 *)sa_addr)->sin6_addr,
                      addr_str, INET6_ADDRSTRLEN);
                    netmask = inet_ntop(sa_mask->sa_family,
                      &((struct sockaddr_in6 *)sa_mask)->sin6_addr,
                      mask_str, INET6_ADDRSTRLEN);

                    if (flags & IFF_DHCPRUNNING) {
                    }

                }

                ncu = nwamui_ncp_get_ncu_by_device_name(NWAMUI_NCP(prv->active_ncp), nwamevent->nwe_data.nwe_if_state.nwe_name);

                if (ncu) {
                    if (address && netmask) {
                        nwamui_ncu_add_acquired(NWAMUI_NCU(ncu), address, netmask, flags);
                    }

                    g_object_unref(ncu);
                } else {
                    nwamui_warning("NCP %s found NCU %s FAILED", nwamui_object_get_name(prv->active_ncp), nwamevent->nwe_data.nwe_if_state.nwe_name);
                }

                g_debug("%s  %s flag(%8X) valid(%u) added(%u) address %s",
                  nwam_event_type_to_string(nwamevent->nwe_type),
                  nwamevent->nwe_data.nwe_if_state.nwe_name,
                  nwamevent->nwe_data.nwe_if_state.nwe_flags,
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
                    nwamui_object_event(NWAMUI_OBJECT(daemon), NWAMUI_DAEMON_INFO_WLANS_CHANGED, ncu);
                }

                g_object_unref(ncu);

            } else {
                /* Comment out the warning because this: */
/* OBJECT_ACTION   ncu link:rum1 -> action destroy                                  */
/* WLAN_SCAN_REPORT  */
/* -               1: -- ESSID ABlackFish BSSID 0:18:39:a5:30:68                    */
/* OBJECT_ACTION   ncu interface:rum1 -> action destroy                             */
           /* g_warning("Can't find wireless NCU %s in the current active NCP %s", */
           /*        nwamevent->nwe_data.nwe_wlan_info.nwe_name, */
           /*        nwamui_object_get_name(prv->active_ncp)); */
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
                    nwamui_object_event(NWAMUI_OBJECT(daemon), NWAMUI_DAEMON_INFO_WLANS_CHANGED, ncu);
                }

                nwamui_object_event(NWAMUI_OBJECT(daemon),
                  NWAMUI_DAEMON_INFO_WIFI_SELECTION_NEEDED,
                  ncu);

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

            /* Note: connect fails info may comes after we select another
             * wlan. So we always search for the wifi instead of get the
             * current selected wifi of NCU by nwamui_ncu_get_wifi_info().
             */
            if (ncu) {
                NwamuiWifiNet *wifi = NULL;

                wifi = nwamui_ncu_wifi_hash_insert_or_update_from_wlan_t(NWAMUI_NCU(ncu), &(nwamevent->nwe_data.nwe_wlan_info.nwe_wlans[0]));
                nwamui_debug("Get essid '%s(0x%p)'", nwamui_object_get_name(NWAMUI_OBJECT(wifi)), wifi);

                if (nwamevent->nwe_data.nwe_wlan_info.nwe_connected) {
                    /* Connect successfully, update the selected wifi and its
                     * status.
                     */
                    nwamui_ncu_set_wifi_info(NWAMUI_NCU(ncu), wifi);
                    nwamui_wifi_net_set_status(wifi, NWAMUI_WIFI_STATUS_CONNECTED);

                    /* nwamui_daemon_setup_dhcp_or_wep_key_timeout( daemon, ncu ); */

                    nwamui_object_event(NWAMUI_OBJECT(daemon), NWAMUI_DAEMON_INFO_WLAN_CONNECTED, wifi);

                } else {
                    /* Fails, only update the status of the wifi. */
                    nwamui_wifi_net_set_status(wifi, NWAMUI_WIFI_STATUS_FAILED);

                    nwamui_object_event(NWAMUI_OBJECT(daemon), NWAMUI_DAEMON_INFO_WLAN_CONNECT_FAILED, wifi);
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
                NwamuiWifiNet *wifi = NULL;

                wifi = nwamui_ncu_wifi_hash_insert_or_update_from_wlan_t(NWAMUI_NCU(ncu), &(nwamevent->nwe_data.nwe_wlan_info.nwe_wlans[0]));
                nwamui_debug("Get essid '%s(0x%p)'", nwamui_object_get_name(NWAMUI_OBJECT(wifi)), wifi);
                
                nwamui_ncu_set_wifi_info(NWAMUI_NCU(ncu), wifi);

                nwamui_object_event(NWAMUI_OBJECT(daemon),
                  NWAMUI_DAEMON_INFO_WIFI_KEY_NEEDED,
                  wifi);

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
            nwamui_object_event(NWAMUI_OBJECT(daemon), NWAMUI_DAEMON_INFO_UNKNOWN, NULL);
            break;
        }
    }
    break;
    default:
        g_warning("Unsupport UI daemon event %d", event->e);
    }
    
    return FALSE;
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
                    sig->event = NWAMUI_DAEMON_INFO_WIFI_SELECTION_NEEDED;
                    sig->data = needs_wifi_selection;
                    g_queue_push_tail( signal_queue, (gpointer)sig );
                }
                if ( needs_wifi_key != NULL ) {
                    to_emit_t *sig = (to_emit_t*)g_malloc0( sizeof( to_emit_t ) );
                    sig->event = NWAMUI_DAEMON_INFO_WIFI_KEY_NEEDED;
                    sig->data = needs_wifi_selection;
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

        nwamui_object_event(NWAMUI_OBJECT(daemon), sig->event, sig->data);

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

    object_name = nwamevent->nwe_data.nwe_object_action.nwe_name;

    switch ( nwamevent->nwe_data.nwe_object_action.nwe_object_type ) {
    case NWAM_OBJECT_TYPE_NCP: {
        NwamuiObject   *ncp = nwamui_daemon_get_ncp_by_name( daemon, object_name );

        switch ( nwamevent->nwe_data.nwe_object_action.nwe_action ) {
        case NWAM_ACTION_ADD: {
            if (!ncp) {
                ncp = nwamui_ncp_new(object_name);
                nwamui_object_add(NWAMUI_OBJECT(daemon), NWAMUI_OBJECT(ncp));
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
                nwamui_object_remove(NWAMUI_OBJECT(daemon), NWAMUI_OBJECT(ncp));
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
                nwamui_object_remove(NWAMUI_OBJECT(ncp), NWAMUI_OBJECT(ncu));
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
                nwamui_object_add(NWAMUI_OBJECT(daemon), NWAMUI_OBJECT(env));
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
                nwamui_daemon_set_active_env( daemon, env );

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
                nwamui_object_remove(NWAMUI_OBJECT(daemon), NWAMUI_OBJECT(env));
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
                nwamui_object_add(NWAMUI_OBJECT(daemon), NWAMUI_OBJECT(enm));
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
                nwamui_object_remove(NWAMUI_OBJECT(daemon), NWAMUI_OBJECT(enm));
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
        NwamuiObject *wifi = nwamui_daemon_find_fav_wifi_net_by_name(daemon, object_name);
            
        switch ( nwamevent->nwe_data.nwe_object_action.nwe_action ) {
        case NWAM_ACTION_ADD: {
            if (wifi == NULL) {
                /* Still didn't find one, so create new fav entry. */
                wifi = nwamui_known_wifi_new(object_name);
                nwamui_object_add(NWAMUI_OBJECT(daemon), NWAMUI_OBJECT(wifi));
            } else {
                /* Capplet adds the object first, so we can find it here. Do
                 * nothing. */
                nwamui_debug("%s %s is existed.", nwam_object_type_to_string(nwamevent->nwe_data.nwe_object_action.nwe_object_type), object_name);
            }
        }
            break;
        case NWAM_ACTION_DISABLE:
        case NWAM_ACTION_ENABLE:
            break;
        case NWAM_ACTION_REFRESH: {
            /* Need to refresh to catch update to priorities */
            if ( wifi != NULL ) {
                nwamui_object_reload( NWAMUI_OBJECT(wifi) );
            }
        }
            break;
        case NWAM_ACTION_REMOVE:
            /* Same as Destroy, so fall-through */
        case NWAM_ACTION_DESTROY: {
            if ( wifi != NULL ) {
                nwamui_object_remove(NWAMUI_OBJECT(daemon), NWAMUI_OBJECT(wifi));
            }
        }
            break;
        }
        if ( wifi != NULL ) {
            g_object_unref(wifi);
        }
    }
        break;
    }
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
            if (obj) {
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
            }
            break;
        case NWAM_OBJECT_TYPE_LOC:
            obj = nwamui_daemon_get_env_by_name( daemon, object_name);
            /* Nwambug: state events may come after destroy event, so ignore. */
            if (obj) {
                nwamui_object_set_nwam_state(obj, object_state, object_aux_state);
                if ( object_state == NWAM_STATE_ONLINE && object_aux_state == NWAM_AUX_STATE_ACTIVE ) {
                    nwamui_daemon_set_active_env( daemon, obj);
                } else {
                    status_flags |= STATUS_REASON_LOC;
                }
                g_object_unref(obj);
            }
            break;
        case NWAM_OBJECT_TYPE_ENM:
            obj = NWAMUI_OBJECT(nwamui_daemon_get_enm_by_name( daemon, object_name ));
            /* Nwambug: state events may come after destroy event, so ignore. */
            if (obj) {
                nwamui_object_set_nwam_state(obj, object_state, object_aux_state);

                nwamui_daemon_update_online_enm_num(daemon);

                if ( object_state == NWAM_STATE_ONLINE && object_aux_state == NWAM_AUX_STATE_ACTIVE ) {
                    /* Nothing. */
                } else {
                    status_flags |= STATUS_REASON_ENM;
                }
                g_object_unref(obj);
            }
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
nwamui_is_nwam_enabled ( void )
{
    gboolean            is_nwam_enabled = FALSE;
    char                *smf_state;
    char                activencp[NWAM_MAX_NAME_LEN];
    scf_error_t         serr;    
    
    smf_state = smf_get_state(NWAMUI_FMRI);
    
    if (strcmp(smf_state, SCF_STATE_STRING_ONLINE) != 0) {
        g_debug("%s: NWAM service appears to be off-line", __func__);
    } else {
        if (get_active_ncp(activencp, sizeof (activencp), &serr) != 0) {
                g_debug("Failed to retrieve active NCP from SMF: %s", 
                        scf_strerror(serr));
        } else {
            is_nwam_enabled = !NWAM_NCP_DEF_FIXED(activencp);
        }
    }

    free(smf_state);
    
    return is_nwam_enabled;
}

static gboolean
nwamui_daemon_nwam_connect( gboolean block )
{
    gboolean      rval = FALSE;
    gboolean      not_done;
    nwam_error_t  nerr;

    g_static_mutex_lock (&nwam_event_mutex);
    nwam_init_done = rval;
    g_static_mutex_unlock (&nwam_event_mutex);

    do {

        if ( ! nwamui_is_nwam_enabled() ) {
            g_debug("NWAM not enabled" );
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

/* walkers */
static int
nwam_loc_walker_cb (nwam_loc_handle_t env, void *data)
{
    NwamuiDaemonPrivate *prv  = NWAMUI_DAEMON_GET_PRIVATE(data);
    NwamuiDaemon        *self = NWAMUI_DAEMON(data);
    char                *name;
    nwam_error_t         nerr;
    NwamuiObject*        new_env;

    if ( (nerr = nwam_loc_get_name (env, &name)) != NWAM_SUCCESS ) {
        g_warning("Failed to get name for loc, error: %s", nwam_strerror (nerr));
        return 0;
    }

    if ( name) {
        if ((new_env = nwamui_daemon_get_env_by_name( self, name )) != NULL ) {
            /* Reload */
            nwamui_object_reload( NWAMUI_OBJECT(new_env) );
            /* Found it, so remove from temp list of ones to be removed */
            self->prv->temp_list = g_list_remove( self->prv->temp_list, new_env );
        } else {
            new_env = nwamui_env_new_with_handle (env);
            if (new_env) {
                nwamui_object_add(NWAMUI_OBJECT(self), NWAMUI_OBJECT(new_env));
            }
        }
        free(name);
    }
            
    if ( new_env != NULL ) {
        if ( nwamui_object_get_active(NWAMUI_OBJECT(new_env)) ) {
            prv->active_env = NWAMUI_OBJECT(g_object_ref(new_env));
        }
        g_object_unref(new_env);
    }

    return(0);
}

static int
nwam_enm_walker_cb (nwam_enm_handle_t enm, void *data)
{
    NwamuiDaemonPrivate *prv  = NWAMUI_DAEMON_GET_PRIVATE(data);
    NwamuiDaemon        *self = NWAMUI_DAEMON(data);
    char                *name;
    nwam_error_t         nerr;
    NwamuiObject*        new_enm;

    if ( (nerr = nwam_enm_get_name (enm, &name)) != NWAM_SUCCESS ) {
        g_warning("Failed to get name for enm, error: %s", nwam_strerror (nerr));
        return 0;
    }

    if ( name) {
        if ((new_enm = nwamui_daemon_get_enm_by_name( self, name )) != NULL ) {
            /* Reload */
            nwamui_object_reload(NWAMUI_OBJECT(new_enm));
            /* Found it, so remove from temp list of ones to be removed */
            self->prv->temp_list = g_list_remove( self->prv->temp_list, new_enm );
        } else {
            new_enm = nwamui_enm_new_with_handle (enm);
            if (new_enm) {
                nwamui_object_add(NWAMUI_OBJECT(self), NWAMUI_OBJECT(new_enm));
            }
        }
        free(name);

        g_object_unref(new_enm);
    }

    return(0);
}

static int
nwam_ncp_walker_cb (nwam_ncp_handle_t ncp, void *data)
{
    NwamuiDaemonPrivate *prv  = NWAMUI_DAEMON_GET_PRIVATE(data);
    NwamuiDaemon        *self = NWAMUI_DAEMON(data);
    char                *name;
    nwam_error_t         nerr;
    NwamuiObject        *new_ncp;

    if ( (nerr = nwam_ncp_get_name (ncp, &name)) != NWAM_SUCCESS ) {
        g_warning("Failed to get name for ncp, error: %s", nwam_strerror (nerr));
        return 0;
    }

    if ( name) {
        if ((new_ncp = nwamui_daemon_get_ncp_by_name( self, name )) != NULL ) {
            /* Reload */
            nwamui_object_reload(NWAMUI_OBJECT(new_ncp));
            /* Found it, so remove from temp list of ones to be removed */
            self->prv->temp_list = g_list_remove( self->prv->temp_list, new_ncp );
        } else {
            new_ncp = nwamui_ncp_new_with_handle (ncp);
            if (new_ncp) {
                nwamui_object_add(NWAMUI_OBJECT(self), NWAMUI_OBJECT(new_ncp));
            }
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
        /* Initialize the current active NCP, and populate each NCU for getting
         * the acquired IP addresses.
         */
        if ( nwamui_object_get_active(NWAMUI_OBJECT(new_ncp)) ) {

            if (prv->active_ncp) {
                g_object_unref(prv->active_ncp);
            }
            prv->active_ncp = NWAMUI_OBJECT(g_object_ref(new_ncp));
            
            nwamui_util_ncp_init_acquired_ip(NWAMUI_NCP(prv->active_ncp));
        }

        g_object_unref(new_ncp);
    } else {
        g_warning("Failed to create NWAMUI_NCP");
    }

    return(0);
}

static int
nwam_known_wlan_walker_cb (nwam_known_wlan_handle_t wlan_h, void *data)
{
    NwamuiDaemonPrivate *prv  = NWAMUI_DAEMON_GET_PRIVATE(data);
    NwamuiDaemon        *self = NWAMUI_DAEMON(data);
    nwam_error_t         nerr;
    NwamuiObject        *wifi = NULL;
    char                *name;

    if ((nerr = nwam_known_wlan_get_name(wlan_h, &name)) != NWAM_SUCCESS) {
        g_warning("Error getting name of known wlan: %s", nwam_strerror(nerr));
        return 0;
    }

    /* Seperate normal wlans and fav wlans. */
    if (name) {
        if ((wifi = nwamui_daemon_find_fav_wifi_net_by_name(self, name)) != NULL ) {
            /* Reload */
            nwamui_object_reload(wifi);
            /* Found it, so remove from temp list of ones to be removed */
            prv->temp_list = g_list_remove( prv->temp_list, wifi);
        } else {
            wifi = nwamui_known_wlan_new_with_handle(wlan_h);
            if (wifi) {
                nwamui_object_add(NWAMUI_OBJECT(self), wifi);
            }
        }
        free(name);
    }
    if (wifi) {
        g_object_unref(wifi);
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

    g_list_foreach(prv->managed_list[MANAGED_KNOWN_WLAN], func, user_data);
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
        return g_list_find_custom(prv->managed_list[MANAGED_KNOWN_WLAN], user_data, func);
    else
        return g_list_find(prv->managed_list[MANAGED_KNOWN_WLAN], user_data);
}

