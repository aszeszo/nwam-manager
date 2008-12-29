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

#include <libnwam.h>
#include <glib-object.h>
#include <glib/gi18n.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include "libnwamui.h"

static GObjectClass    *parent_class    = NULL;
static NwamuiDaemon*    instance        = NULL;

static GStaticMutex nwam_event_mutex = G_STATIC_MUTEX_INIT;
static gboolean nwam_event_thread_terminate = FALSE; /* To tell event thread to terminate set to TRUE */

#define WLAN_TIMEOUT_SCAN_RATE_SEC (60)
#define WEP_TIMEOUT_SEC (20)

enum {
    DAEMON_STATUS_CHANGED,
    DAEMON_INFO,
    WIFI_SCAN_RESULT,
    ACTIVE_NCP_CHANGED,
    ACTIVE_ENV_CHANGED,
    WIFI_KEY_NEEDED,
    WIFI_SELECTION_NEEDED,
    S_NCU_CREATE,
    S_NCU_DESTROY,
    S_NCU_UP,
    S_NCU_DOWN,
    S_WIFI_FAV_ADD,
    S_WIFI_FAV_REMOVE,
    LAST_SIGNAL
};

enum {
    PROP_ACTIVE_ENV = 1,
    PROP_ACTIVE_NCP,
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
    nwamui_daemon_status_t       status;
    nwamui_daemon_event_cause_t  event_cause;
    GThread                     *nwam_events_gthread;
    gboolean                     communicate_change_to_daemon;
    guint                        wep_timeout_id;
    gboolean                     emit_wlan_changed_signals;
    GList                       *wifi_list;
};

typedef struct _NwamuiEvent {
    int e;	/* ui daemon event type */
    nwam_events_msg_t *led;	/* daemon data */
    NwamuiDaemon* daemon;
} NwamuiEvent;

static gboolean nwamd_event_handler(gpointer data);

static NwamuiEvent* nwamui_event_new(NwamuiDaemon* daemon, int e, nwam_events_msg_t *led);

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


/* Callbacks */
static void object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);

/* Default Callback Handlers */
static int nwam_events_callback (nwam_events_msg_t *msg, int size, int nouse);
static void default_active_env_changed_signal_handler (NwamuiDaemon *self, GObject* data, gpointer user_data);
static void default_active_ncp_changed_signal_handler (NwamuiDaemon *self, GObject* data, gpointer user_data);
static void default_add_wifi_fav_signal_handler (NwamuiDaemon *self, NwamuiWifiNet* new_wifi, gpointer user_data);
static void default_daemon_info_signal_handler (NwamuiDaemon *self, gint type, GObject *obj, gpointer data, gpointer user_data);
static void default_ncu_create_signal_handler (NwamuiDaemon *self, NwamuiNcu* ncu, gpointer user_data);
static void default_ncu_destroy_signal_handler (NwamuiDaemon *self, NwamuiNcu* ncu, gpointer user_data);
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
                                                          NWAMUI_DAEMON_STATUS_INACTIVE,
                                                          NWAMUI_DAEMON_STATUS_LAST-1,
                                                          NWAMUI_DAEMON_STATUS_INACTIVE,
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
    klass->ncu_create = default_ncu_create_signal_handler;
    klass->ncu_destroy = default_ncu_destroy_signal_handler;
    klass->ncu_up = default_ncu_up_signal_handler;
    klass->ncu_down = default_ncu_down_signal_handler;
    klass->daemon_info = default_daemon_info_signal_handler;
    klass->add_wifi_fav = default_add_wifi_fav_signal_handler;
    klass->remove_wifi_fav = default_remove_wifi_fav_signal_handler;
}

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
    {"My Absolute Fav Network", NWAMUI_WIFI_SEC_NONE, "11:22:33:44:55:66", "g", 54, NWAMUI_WIFI_STRENGTH_GOOD , NWAMUI_WIFI_WPA_CONFIG_AUTOMATIC },
    {"2nd Favourite", NWAMUI_WIFI_SEC_WEP_HEX, "22:33:44:55:66:77", "b", 11, NWAMUI_WIFI_STRENGTH_NONE, NWAMUI_WIFI_WPA_CONFIG_AUTOMATIC },
    {"Last Favourite", NWAMUI_WIFI_SEC_WPA_PERSONAL, "33:44:55:66:77:88", "b", 2, NWAMUI_WIFI_STRENGTH_VERY_WEAK, NWAMUI_WIFI_WPA_CONFIG_PEAP },
    {NULL, NULL, NULL, NULL, NULL},
};

static nwamui_wifi_essid_t demo_essids[] = {
    { "MyESSID", NWAMUI_WIFI_SEC_NONE, "2001:0db8:0:0:0:0:1428:57ab", "g", 54, NWAMUI_WIFI_STRENGTH_GOOD , NWAMUI_WIFI_WPA_CONFIG_AUTOMATIC },
#if 1
    {"My Absolute Fav Network", NWAMUI_WIFI_SEC_NONE, "11:22:33:44:55:66", "g", 54, NWAMUI_WIFI_STRENGTH_VERY_WEAK , NWAMUI_WIFI_WPA_CONFIG_AUTOMATIC },
    {"essid_open", NWAMUI_WIFI_SEC_NONE, "AA:bb:cc:dd:ee:ff", "b", 11, NWAMUI_WIFI_STRENGTH_GOOD, NWAMUI_WIFI_WPA_CONFIG_AUTOMATIC },
    {"essid_wep", NWAMUI_WIFI_SEC_WEP_HEX, "BB:bb:cc:dd:ee:ff", "g", 54, NWAMUI_WIFI_STRENGTH_GOOD, NWAMUI_WIFI_WPA_CONFIG_AUTOMATIC },
    {"essid_wpa", NWAMUI_WIFI_SEC_WPA_PERSONAL, "CC:bb:cc:dd:ee:ff", "g", 36, NWAMUI_WIFI_STRENGTH_GOOD, NWAMUI_WIFI_WPA_CONFIG_PEAP },
#endif
    {NULL, NULL, NULL, NULL, NULL},
};

static void
nwamui_daemon_init (NwamuiDaemon *self)
{
    GError                 *error = NULL;
    nwamui_wifi_essid_t    *ptr;
    NwamuiWifiNet          *wifi_net = NULL;
    NwamuiEnm              *new_enm = NULL;
    
    self->prv = g_new0 (NwamuiDaemonPrivate, 1);
    
    self->prv->status = NWAMUI_DAEMON_STATUS_INACTIVE;
    self->prv->event_cause = NWAMUI_DAEMON_EVENT_CAUSE_NONE;
    self->prv->wep_timeout_id = 0;
    self->prv->communicate_change_to_daemon = TRUE;
    self->prv->active_env = NULL;
    self->prv->active_ncp = NULL;

#if 0
    /* TODO: Register for Events here */
    if (tid == 0) {
        nwam_events_register (nwam_events_callback, &tid);
    }
    
    if ( libnwam_init(0) == -1 ) {
        g_debug("NWAM Daemon doesn't appear to be running, errno = %s", strerror(errno) );
    } else {
		/*
		 * according to the logic of nwam_events_thead, we can emit NWAMUI_DAEMON_INFO_ACTIVE
		 * here, so we can populate all the info in nwamd_event_handler
		 */
		g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
		  nwamd_event_handler,
		  (gpointer) nwamui_event_new(self, NWAMUI_DAEMON_INFO_ACTIVE, NULL),
		  (GDestroyNotify) nwamui_event_free);

        nwamui_daemon_populate_wifi_fav_list( self );
    }
#endif /* 0 */
	
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
    
    /* FIXME: Assume first NCP is the active until we can determine otherwise
     * using the API */
    if ( self->prv->active_ncp == NULL && self->prv->ncp_list  != NULL ) {
        GList* first_element = g_list_first( self->prv->ncp_list );


        if ( first_element != NULL && first_element->data != NULL )  {
            self->prv->active_ncp = NWAMUI_NCP(g_object_ref(G_OBJECT(first_element->data)));
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

                /* TODO - I presume that keep the prev active ncp if failded */
                /* FIXME: For demo, always set active_ncp! */
                if (1 || nwamui_ncp_activate (ncp) ) {
                    self->prv->active_ncp = ncp;
                    
                    g_signal_emit (self,
                      nwamui_daemon_signals[ACTIVE_NCP_CHANGED],
                      0, /* details */
                      self->prv->active_ncp );
                } else {
                    /* TODO - We should tell user we are failed */
                }
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
    int retval;
    
    /* TODO - kill tid before destruct self */
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
    nwamui_daemon_status_t status = NWAMUI_DAEMON_STATUS_INACTIVE;

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
    
    g_assert( status >= NWAMUI_DAEMON_STATUS_INACTIVE && status < NWAMUI_DAEMON_STATUS_LAST );

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

    g_assert( NWAMUI_IS_NCU(ncu) );
    
	name = nwamui_ncu_get_device_name (ncu);

    g_debug("i/f %s  = %s (%d)", name,  nwamui_ncu_get_ncu_type (ncu) == NWAMUI_NCU_TYPE_WIRELESS?"Wireless":"Wired", nwamui_ncu_get_ncu_type (ncu));
	if (nwamui_ncu_get_ncu_type (ncu) != NWAMUI_NCU_TYPE_WIRELESS ) {
        g_object_unref( ncu );
        g_free (name);
        return( FALSE );
	}


    if ( name != NULL ) {
        g_debug("calling libnwam_start_rescan( %s )", name );
        /* TODO: Can we trigger a rescan? */
        g_debug("Tried to cause rescan, but not supported!!");
        /* libnwam_start_rescan( name ); */
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
        nwamui_ncp_foreach_ncu (ncp, trigger_scan_if_wireless, NULL);
        g_object_unref(ncp);
    }
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
    self->prv->wifi_fav_list = g_list_append(self->prv->wifi_fav_list, (gpointer)g_object_ref(new_wifi) );
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

static gint
compare_essid_bssid( gconstpointer _a, gconstpointer _b )
{
    NwamuiWifiNet* a = NWAMUI_WIFI_NET(_a); 
    NwamuiWifiNet *b = NWAMUI_WIFI_NET(_b);
    gint rval = -1;
    gchar *essid_a = NULL;
    gchar *essid_b = NULL;
    gchar *bssid_a = NULL;
    gchar *bssid_b = NULL;

    g_return_val_if_fail( NWAMUI_IS_WIFI_NET(a) && NWAMUI_IS_WIFI_NET(b), rval );

    essid_a = nwamui_wifi_net_get_essid( a );
    essid_b = nwamui_wifi_net_get_essid( b );

    if ( essid_a == NULL && essid_b == NULL ) {
        rval = 0;
    }
    else if ( essid_a == NULL || essid_b == NULL ) {
        rval = essid_a == NULL ? -1:1;
    }
    else if ( ( rval = strcmp(essid_a, essid_b)) == 0 ) {
        bssid_a = nwamui_wifi_net_get_bssid( a );
        bssid_b = nwamui_wifi_net_get_bssid( b );

        if ( bssid_a == NULL && bssid_b == NULL ) {
            rval = 0;
        }
        else if ( bssid_a == NULL || bssid_b == NULL ) {
            rval = bssid_a == NULL ? -1:1;
        }
        else {
            rval = strcmp(bssid_a, bssid_b);
        }
        g_free( bssid_a );
        g_free( bssid_b );
    }

    g_free( essid_a );
    g_free( essid_b );
    return( rval );
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
        GList *old_copy = nwamui_daemon_get_fav_wifi_networks(self);
        GList *new_items = NULL;

        for( GList* item = g_list_first( new_list ); 
             item != NULL; 
             item = g_list_next( item ) ) {
            GList* found_item = NULL;
            
            if ( (found_item = g_list_find_custom( old_copy, item->data, compare_essid_bssid )) != NULL ) {
                /* Same */
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
                gchar* essid = nwamui_wifi_net_get_essid( wifi );
                gchar* bssid = nwamui_wifi_net_get_bssid( wifi );

                g_debug("Removing deleted known network %s ; %s", essid?essid:"",  bssid?bssid:"" );
#if 0         
                /* TODO: Figure out how to delete a known AP */
                if (self->prv->communicate_change_to_daemon 
                    && libnwam_delete_known_ap( essid?essid:"",  bssid?bssid:"") != 0) {
				    g_debug("Call libnwam_delete_known_ap failed: %s\n", strerror(errno));
                }
                else {
                    /* If deleted, then remove from merged list */
                    merged_list = g_list_remove( merged_list, item->data );
                    g_object_unref(G_OBJECT( item->data));
                }
#else
                g_error("Unable to delete a known AP yet");
#endif                
                g_free(essid);
                g_free(bssid);
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
                gchar* essid = nwamui_wifi_net_get_essid( wifi );
                gchar* bssid = nwamui_wifi_net_get_bssid( wifi );

                g_debug("Adding new known network %s ; %s", essid?essid:"",  bssid?bssid:"" );
#if 0         
                /* TODO: Figure out how to add a known AP */
                if (self->prv->communicate_change_to_daemon 
                    && libnwam_add_known_ap( essid?essid:"",  bssid?bssid:"") != 0) {
				    g_debug("Call libnwam_add_known_ap failed: %s\n", strerror(errno));
                }
                else {
                    /* If added, then add to merged list */
                    merged_list = g_list_append( merged_list, item->data );
                    /* Don't ref, transfer ownership, since will be freeing new_items list anyway */
                }
#else
                g_error("Unable to add a known AP yet");
#endif                
                g_free(essid);
                g_free(bssid);
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
                gchar* essid = nwamui_wifi_net_get_essid( wifi );
                gchar* bssid = nwamui_wifi_net_get_bssid( wifi );

                g_debug("Removing deleted known network %s ; %s", essid?essid:"",  bssid?bssid:"" );
#if 0
                /* TODO: Figure out how to delete a known AP */
                if (self->prv->communicate_change_to_daemon 
                    && libnwam_delete_known_ap( essid?essid:"",  bssid?bssid:"") != 0) {
				    g_debug("Call libnwam_delete_known_ap failed: %s\n", strerror(errno));
                }
#else
                g_error("Unable to delete a known AP yet");
#endif                
                g_free(essid);
                g_free(bssid);

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
            gchar* essid = nwamui_wifi_net_get_essid( wifi );
            gchar* bssid = nwamui_wifi_net_get_bssid( wifi );

            g_debug("Adding new known network %s ; %s", essid?essid:"",  bssid?bssid:"" );
#if 0
                /* TODO: Figure out how to add a known AP */
            if (self->prv->communicate_change_to_daemon 
                && libnwam_add_known_ap( essid?essid:"",  bssid?bssid:"") != 0) {
                g_debug("Call libnwam_add_known_ap failed: %s\n", strerror(errno));
            } else {
                /* Copy on succeeded */
                self->prv->wifi_fav_list = g_list_prepend(self->prv->wifi_fav_list, g_object_ref(item->data));
            }
#else
                g_error("Unable to add a known AP yet");
#endif                
            g_free(essid);
            g_free(bssid);
        }
        
    }

    return( TRUE );
}


/* Callbacks */

static void
object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
    NwamuiDaemon* self = NWAMUI_DAEMON(data);

    g_debug("NwamuiDaemon: notify %s changed\n", arg1->name);
}

/**
 * get_ncu_by_device_name:
 *
 * Unref is needed
 */
static NwamuiNcu* 
get_ncu_by_device_name( NwamuiDaemon *daemon, const char*device_name )
{
    NwamuiNcp*  ncp;
    NwamuiNcu*  ncu = NULL;

    ncp = nwamui_daemon_get_active_ncp( daemon );
    if ( ncp ) {
        ncu = nwamui_ncp_get_ncu_by_device_name( ncp, device_name );
    }

    g_object_unref(ncp);
    return( ncu );
}

static void
dispatch_wifi_scan_events_from_cache(NwamuiDaemon* daemon )
{
#if 0
    /* TODO: Figure out how to dispatch scanned wifi APs */

    NwamuiWifiNet          *wifi_net = NULL;
    NwamuiNcu              *ncu = NULL;
    libnwam_wlan_t         *wlan, *origwlan;
    uint_t                  nwlan;
    const char*             prev_iface_name = NULL;

    origwlan = wlan = libnwam_get_wlan_list(&nwlan);
    g_debug("%d WLANs detected.\n", nwlan);
    while (nwlan-- > 0) {
        /* Try avoid potentially costly look up ncu object every time */
        if ( prev_iface_name == NULL || strcmp(prev_iface_name, wlan->wlan_interface) != 0 ) {
            if ( ncu ) {
                g_object_unref(ncu);
            }
            ncu = get_ncu_by_device_name( daemon, wlan->wlan_interface );
        }

        g_debug("Interface: %-10s\n\t%-3s  "
            "Key: %s  %s\n",
            wlan->wlan_interface,
            wlan->wlan_known ? "Old" : "New",
            wlan->wlan_haskey ? "Set" : "Unset",
            wlan->wlan_connected ? "Connected" :
            "Unconnected");
        /* FIXME - Skipping empty ESSID seems wrong here, what if we
         * actually connect to this... Will it still appear in menu??
         */
        if ( strlen(wlan->wlan_attrs.wla_essid) > 0  && ncu != NULL ) {

            wifi_net = nwamui_wifi_net_new_from_wlan_attrs( ncu, &(wlan->wlan_attrs));

            if ( wlan->wlan_connected ) { /* connect WLAN, set in NCU... */
                nwamui_ncu_set_wifi_info (ncu, wifi_net);
                nwamui_wifi_net_set_status( wifi_net, NWAMUI_WIFI_STATUS_CONNECTED );
            }

            if (daemon->prv->emit_wlan_changed_signals) {
                /* trigger event */
                g_signal_emit (daemon,
                  nwamui_daemon_signals[WIFI_SCAN_RESULT],
                  0, /* details */
                  wifi_net);
            }

            g_object_unref(wifi_net);
        }
        wlan++;
    }
    libnwam_free_wlan_list(origwlan);
    g_object_unref(ncu);
 
#endif

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
nwamui_event_new(NwamuiDaemon* daemon, int e, nwam_events_msg_t *led)
{
    NwamuiEvent *event = g_new(NwamuiEvent, 1);
    event->e = e;
    event->led = led;
    event->daemon = g_object_ref(daemon);
    return event;
}

static void
nwamui_event_free(NwamuiEvent *event)
{
    g_object_unref(event->daemon);
    /* TODO: Can we free an event? */
    if (event->led) {
        free(event->led);
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
#if 0 
    /* TODO: Find API for getting known networks */
    libnwam_known_ap_t *kap, *origka;
    uint_t nkas;

    origka = kap = libnwam_get_known_ap_list(&nkas);
    g_debug("%d known APs.\n", nkas);
    while (nkas-- > 0) {
        g_debug("ESSID %-32s  BSSID %s\n", kap->ka_essid, kap->ka_bssid);
        g_debug("  %s\n", kap->ka_haskey ? "Has key" : "No key");

        wifi = nwamui_wifi_net_new_from_known_ap( kap );
        new_list = g_list_append(new_list, (gpointer)g_object_ref(wifi) );
        kap++;
    }
    libnwam_free_known_ap_list(origka);

    /* Use the set method to merge if necessary, but temporarily suspend
     * communication of changes to daemon */
    self->prv->communicate_change_to_daemon = FALSE;
    nwamui_daemon_set_fav_wifi_networks(self, new_list);
    self->prv->communicate_change_to_daemon = TRUE;
#endif    
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

#ifdef PHASE_0_5
static gboolean
nwamd_event_handler(gpointer data)
{
    NwamuiEvent             *event = (NwamuiEvent *)data;
    NwamuiDaemon            *daemon = event->daemon;
	libnwam_event_data_t    *led = event->led;
    int                     err;

    daemon->prv->event_cause = NWAMUI_DAEMON_EVENT_CAUSE_NONE;

    g_debug("nwamd_event_handler : %d", event->e );

    switch (event->e) {
    case NWAMUI_DAEMON_INFO_UNKNOWN:
    case NWAMUI_DAEMON_INFO_ERROR:
    case NWAMUI_DAEMON_INFO_INACTIVE:
        nwamui_ncp_set_active_ncu( daemon->prv->active_ncp, NULL );
        nwamui_daemon_set_status(daemon, NWAMUI_DAEMON_STATUS_INACTIVE);
        nwamui_ncp_populate_ncu_list(daemon->prv->active_ncp, G_OBJECT(daemon));
        break;
    case NWAMUI_DAEMON_INFO_ACTIVE:
        /* Set active first to make sure status icon is set to visible before
         * any notification is shown (if status icon is invisible, notification
         * will show warnings) */
        nwamui_daemon_set_status(daemon, NWAMUI_DAEMON_STATUS_ACTIVE);

		/* should repopulate data here */
        nwamui_ncp_populate_ncu_list(daemon->prv->active_ncp, G_OBJECT(daemon));
        nwamui_daemon_populate_wifi_fav_list(daemon);

        /* Initially populate data */
        nwamui_daemon_initially_populate(daemon);

        break;
    case NWAMUI_DAEMON_INFO_RAW:
    {
        g_debug("NWAMUI_DAEMON_INFO_RAW led_type %d", led->led_type);
        switch (led->led_type) {
        case deInitial: {
			/* should repopulate data here */
			
            g_debug ("NWAM daemon started.");
            /* Redispatch as INFO_ACTIVE */
            g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
              nwamd_event_handler,
              (gpointer) nwamui_event_new(daemon, NWAMUI_DAEMON_INFO_ACTIVE, NULL),
              (GDestroyNotify) nwamui_event_free);
        }
            break;
        case deInterfaceUp: {
            NwamuiNcu* ncu;
            g_debug ("Interface %s is up with address %s/%d", led->led_interface, 
              inet_ntoa(led->led_v4address), led->led_prefixlen );

            daemon->prv->is_connected = TRUE;

            ncu = get_ncu_by_device_name( daemon, led->led_interface );
            nwamui_ncu_set_ipv4_address( ncu, inet_ntoa(led->led_v4address) );
            /* TODO - do we care about the pefix? */
            /* nwamui_ncu_set_ipv4_subnet( ncu, led->led_prefixlen ); */

            /* Directly deliver to upper consumers */
            g_signal_emit (daemon,
              nwamui_daemon_signals[S_NCU_UP],
              0, /* details */
              ncu );

            g_object_unref(ncu);
        }
            break;
        case deInterfaceDown: {
            NwamuiNcu* ncu;
            const char* reason = NULL;
            nwamui_daemon_set_event_cause( daemon, convert_cause( led->led_cause ));
            reason = nwamui_daemon_get_event_cause_string( daemon );

            g_debug ("Interface %s is down, Reason '%s'", led->led_interface, reason );

            /* Directly deliver to upper consumers */
            ncu = get_ncu_by_device_name( daemon, led->led_interface );
            nwamui_ncu_set_ipv4_address( ncu, "" );
            g_signal_emit (daemon,
              nwamui_daemon_signals[S_NCU_DOWN],
              0, /* details */
              ncu );
            g_object_unref(ncu);
        }
            break;

		case deInterfaceAdded: {
            NwamuiNcp * ncp = nwamui_daemon_get_active_ncp( daemon );

            nwamui_ncp_populate_ncu_list( ncp, G_OBJECT(daemon) );

			g_debug("Interface %s added\n", led->led_interface);
        }
			break;
		case deInterfaceRemoved: {
            NwamuiNcp * ncp = nwamui_daemon_get_active_ncp( daemon );

            nwamui_ncp_remove_ncu( ncp, led->led_interface );
			g_debug("Interface %s removed\n", led->led_interface);
        }
			break;
        case deScanChange:
            g_debug("New wireless networks found.", led->led_interface );

            /* Emit signals on found wlans */
            daemon->prv->emit_wlan_changed_signals = TRUE;

            /* Fall through */
        case deScanSame:
            /* deScanChange or initial scan on daemon action will emit
             * signals */
            if (daemon->prv->emit_wlan_changed_signals) {
                NwamuiNcu * ncu = get_ncu_by_device_name( daemon, led->led_interface );

                g_signal_emit (daemon,
                  nwamui_daemon_signals[DAEMON_INFO],
                  0, /* details */
                  NWAMUI_DAEMON_INFO_WLAN_CHANGED,
                  ncu,
                  g_strdup_printf(_("New wireless networks found.")));

                dispatch_wifi_scan_events_from_cache( daemon );
                g_object_unref(ncu);
            }
            break;

        case deULPActivated: {
            g_debug("User script '%s' activated", led->led_interface );
            g_signal_emit (daemon,
              nwamui_daemon_signals[DAEMON_INFO],
              0, /* details */
              NWAMUI_DAEMON_INFO_GENERIC,
              NULL,
              g_strdup_printf(_("User script '%s' activated"), led->led_interface ));
        }
            break;
        case deULPDeactivated: {
            g_debug("User script '%s' deactivated", led->led_interface );
            g_signal_emit (daemon,
              nwamui_daemon_signals[DAEMON_INFO],
              0, /* details */
              NWAMUI_DAEMON_INFO_GENERIC,
              NULL,
              g_strdup_printf(_("User script '%s' deactivated"), led->led_interface ));
        }
            break;
        case deWlanConnectFail: {
            NwamuiNcu * ncu = get_ncu_by_device_name( daemon, led->led_interface );
            NwamuiWifiNet* wifi = NULL;
            gchar*          name = NULL;

            g_debug("Wireless connection on  '%s' failed->", led->led_interface);
            daemon->prv->is_connected = FALSE;

            g_assert(ncu);
            if ( ncu !=  NULL ) {
                wifi = nwamui_ncu_get_wifi_info(ncu);

                if ( wifi ) {
                    nwamui_wifi_net_set_status(wifi, NWAMUI_WIFI_STATUS_FAILED);
                }

                name = nwamui_ncu_get_vanity_name( ncu );
            }

            g_signal_emit (daemon,
              nwamui_daemon_signals[DAEMON_INFO],
              0, /* details */
              NWAMUI_DAEMON_INFO_WLAN_CONNECT_FAILED,
              wifi,
              g_strdup_printf("%s connection to wireless lan failed", name?name:led->led_interface));

            g_free(name);

            if ( wifi )
                g_object_unref(wifi);

            if ( ncu )
                g_object_unref(ncu);
        }
            break;
        case deWlanDisconnect: {
            NwamuiNcu * ncu = get_ncu_by_device_name( daemon, led->led_interface );
            NwamuiWifiNet* wifi = nwamui_ncu_get_wifi_info(ncu);

            daemon->prv->is_connected = FALSE;

            g_debug("Wireless network '%s' disconnected", led->led_wlan.wla_essid );
            g_assert(ncu);

            if ( wifi ) {
                nwamui_wifi_net_set_status(wifi, NWAMUI_WIFI_STATUS_DISCONNECTED);
                /* Do not reuse wifi net instance which will mess up menus */
/*                 nwamui_wifi_net_set_from_wlan_attrs( wifi, &led->led_wlan  ); */
                nwamui_ncu_set_wifi_info(ncu, NULL);
            }
            else {
                wifi = nwamui_wifi_net_new_from_wlan_attrs( ncu, &led->led_wlan  );
            }

            g_signal_emit (daemon,
              nwamui_daemon_signals[DAEMON_INFO],
              0, /* details */
              NWAMUI_DAEMON_INFO_WLAN_DISCONNECTED,
              wifi,
              g_strdup_printf("%s", led->led_wlan.wla_essid ));

            g_object_unref(wifi);
            g_object_unref(ncu);
        }
            break;
        case deWlanConnected: {
            NwamuiNcu * ncu = get_ncu_by_device_name( daemon, led->led_interface );
            NwamuiWifiNet* wifi = nwamui_ncu_get_wifi_info(ncu);

            g_debug("Wireless network '%s' connected", led->led_wlan.wla_essid );
            g_assert(ncu);

            if ( wifi ) {
                nwamui_wifi_net_set_status(wifi, NWAMUI_WIFI_STATUS_CONNECTED);
                nwamui_wifi_net_set_from_wlan_attrs( wifi, &led->led_wlan  );
            }
            else {
                wifi = nwamui_wifi_net_new_from_wlan_attrs( ncu, &led->led_wlan  );
                nwamui_ncu_set_wifi_info(ncu, wifi);
            }

            nwamui_daemon_setup_dhcp_or_wep_key_timeout( daemon, ncu );

            /* Retrieve fav wifi list */
            nwamui_daemon_populate_wifi_fav_list(daemon);

            g_signal_emit (daemon,
              nwamui_daemon_signals[DAEMON_INFO],
              0, /* details */
              NWAMUI_DAEMON_INFO_WLAN_CONNECTED,
              wifi,
              g_strdup_printf("%s", led->led_wlan.wla_essid ));

            g_object_unref(wifi);
            g_object_unref(ncu);
        }
            break;

        case deLLPSelected: {
            NwamuiNcu * ncu = get_ncu_by_device_name( daemon, led->led_interface );
            NwamuiNcp * ncp = nwamui_daemon_get_active_ncp( daemon );
            gchar*      name = NULL;

            g_debug("Interface  '%s' selected.", led->led_interface);

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
            NwamuiNcu * ncu = get_ncu_by_device_name( daemon, led->led_interface );
            NwamuiNcp * ncp = nwamui_daemon_get_active_ncp( daemon );
            NwamuiNcu * current_ncu = NULL;
            const char* reason = NULL;
            gchar*      name = NULL;
            nwamui_daemon_set_event_cause( daemon, convert_cause( led->led_cause ));
            reason = nwamui_daemon_get_event_cause_string( daemon );

            g_debug("'%s' network interface unselected, Reason: '%s'", led->led_interface, reason);

            g_assert(ncp && ncu);

            current_ncu = nwamui_ncp_get_active_ncu(ncp);

            if (current_ncu) {
                if (current_ncu == ncu) {
                    daemon->prv->is_connected = FALSE;
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

        case deWlanKeyNeeded: {
            NwamuiNcu * ncu = get_ncu_by_device_name( daemon, led->led_interface );
            NwamuiNcp * ncp = nwamui_daemon_get_active_ncp( daemon );
            NwamuiNcu * current_ncu = NULL;
            NwamuiWifiNet *wifi = nwamui_ncu_get_wifi_info( ncu );

            daemon->prv->is_connected = FALSE;

            current_ncu = nwamui_ncp_get_active_ncu(ncp);

            if (current_ncu) {
                /* Remove current timeout */
                nwamui_daemon_disable_dhcp_or_wep_key_timeout( daemon, current_ncu );

                g_object_unref(current_ncu);
            }

            if ( wifi == NULL ) {
                wifi = nwamui_wifi_net_new_from_wlan_attrs( ncu, &led->led_wlan );
                nwamui_ncu_set_wifi_info( ncu, wifi );
            }
            else {
                char* essid = nwamui_wifi_net_get_essid( wifi );
                char* bssid = nwamui_wifi_net_get_bssid( wifi );

                if( !( ( essid != NULL && strncmp( essid, led->led_wlan.wla_essid, strlen(essid) ) == 0 ) 
                       && ( bssid != NULL && strncmp( bssid, led->led_wlan.wla_bssid, strlen(bssid) ) == 0 ) ) ) { 
                    /* ESSID and BSSID are different, so we should update the ncu */
                    NwamuiWifiNet* new_wifi = nwamui_wifi_net_new_from_wlan_attrs( ncu, &led->led_wlan );
                    g_debug("deWlanKeyNeeded: WifiNets differ, replacing");
                    nwamui_ncu_set_wifi_info( ncu, new_wifi );
                    g_object_unref(wifi);
                    wifi = new_wifi;
                }

                g_free(essid);
                g_free(bssid);
            }

            g_assert(wifi);

            g_signal_emit (daemon,
              nwamui_daemon_signals[WIFI_KEY_NEEDED],
              0, /* details */
              wifi );
            g_object_unref(ncu);
            g_object_unref(ncp);
        }
            break;
        case deWlanSelectionNeeded: {
            NwamuiNcu * ncu = get_ncu_by_device_name( daemon, led->led_interface );
            NwamuiNcp * ncp = nwamui_daemon_get_active_ncp( daemon );
            NwamuiNcu * current_ncu = NULL;

            daemon->prv->is_connected = FALSE;

            g_debug("No suitable wireless networks found, selection needed.");

            current_ncu = nwamui_ncp_get_active_ncu(ncp);

            if (current_ncu) {
                /* Remove current timeout */
                nwamui_daemon_disable_dhcp_or_wep_key_timeout( daemon, current_ncu );

                g_object_unref(current_ncu);
            }

            if (daemon->prv->emit_wlan_changed_signals) {
                g_signal_emit (daemon,
                  nwamui_daemon_signals[DAEMON_INFO],
                  0, /* details */
                  NWAMUI_DAEMON_INFO_WLAN_CHANGED,
                  NULL,
                  g_strdup_printf(_("New wireless networks found.")));

                dispatch_wifi_scan_events_from_cache( daemon );
            }

            g_signal_emit (daemon,
              nwamui_daemon_signals[WIFI_SELECTION_NEEDED],
              0, /* details */
              ncu );

            g_object_unref(ncu);
            g_object_unref(ncp);
        }
            break;
        default:
            /* Directly deliver to upper consumers */
            g_signal_emit (daemon,
              nwamui_daemon_signals[DAEMON_INFO],
              0, /* details */
              NWAMUI_DAEMON_INFO_UNKNOWN,
              NULL,
              g_strdup_printf ("Unknown NWAM event type %d.", led->led_type));
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


#endif /* PHASE_0_5 */
/**
 * nwam_events_callback:
 *
 * This callback is needed to be MT safe.
 */
static int
nwam_events_callback (nwam_events_msg_t *msg, int size, int nouse)
{
    nwam_events_msg_t *idx;
    NwamuiDaemon *self = nwamui_daemon_get_instance ();
    
    g_debug ("nwam_events_callback\n");
    
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
        case NWAM_EVENT_TYPE_SCAN_REPORT: {
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
                wifi_net = nwamui_wifi_net_new (NULL,
                  wlans->essid,
                  nwamui_wifi_net_security_map (wlans->security_mode),
                  wlans->bssid,
                  NULL,
                  0,
                  nwamui_wifi_net_strength_map (wlans->signal_strength),
                  NWAMUI_WIFI_BSS_TYPE_AUTO,
                  NULL);
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
nwamui_daemon_emit_signals_from_event_msg( NwamuiDaemon* self, NwamuiNcu* ncu, nwam_events_msg_t* event )
{
    gchar *vanity_name = NULL;

    g_return_if_fail( self != NULL && ncu != NULL && event != NULL );

    if ( event->type != NWAM_EVENT_TYPE_SCAN_REPORT ) {
        g_debug("Tried to emit wifi info from event != NWAM_EVENT_TYPE_SCAN_REPORT");
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

/**
 * nwam_events_thread:
 *
 * This callback is needed to be MT safe.
 */
static gpointer
nwam_events_thread ( gpointer data )
{
    NwamuiDaemon           *daemon = NWAMUI_DAEMON( data );
#if 0 
    /* TODO: Implement Event thread when NWAM supports it */
	libnwam_event_data_t   *led = NULL;
    int                     err;

    g_debug ("nwam_events_thread\n");
    
	while (event_thread_running()) {
		errno = 0;
		if ((led = libnwam_wait_event()) == NULL) {
			err = errno;
			g_debug("Event wait error: %s", strerror(errno));

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
			if (err == EBADF ) {
				g_debug("Attempting to reopen connection to daemon");
				(void) libnwam_fini();
				if (libnwam_init(-1) == 0) {
                    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                      nwamd_event_handler,
                      (gpointer) nwamui_event_new(daemon, NWAMUI_DAEMON_INFO_ACTIVE, NULL),
                      (GDestroyNotify) nwamui_event_free);

					continue;
                }
                else {
                    g_debug("libnwam_init(-1) error: %s", strerror(errno));
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
          (gpointer) nwamui_event_new(daemon, NWAMUI_DAEMON_INFO_RAW, led),
          (GDestroyNotify) nwamui_event_free);
    }
    
    g_object_unref (daemon);
#endif   
    g_debug("Exiting event thread");

    return NULL;
}


/* Default Signal Handlers */
static void
default_status_changed_signal_handler(NwamuiDaemon *self, nwamui_daemon_status_t status, gpointer user_data)
{
    const char* status_str;

    switch( status) {
        case NWAMUI_DAEMON_STATUS_ACTIVE:
            status_str="ACTIVE";
            break;
        case NWAMUI_DAEMON_STATUS_INACTIVE:
            status_str="INACTIVE";
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
default_ncu_create_signal_handler (NwamuiDaemon *self, NwamuiNcu* ncu, gpointer user_data)
{
	gchar*      name = nwamui_ncu_get_display_name(ncu);
	
	g_debug("Create NCU %s", name );
	
	g_free(name);
}

static void
default_ncu_destroy_signal_handler (NwamuiDaemon *self, NwamuiNcu* ncu, gpointer user_data)
{
	gchar*      name = nwamui_ncu_get_display_name(ncu);
	
	g_debug("Destroy NCU %s", name );
	
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
        
    nwamui_env_set_modifiable(new_env, TRUE);
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
    char *name;
    nwam_error_t nerr;
    NwamuiNcp* new_ncp;
    
    NwamuiDaemonPrivate *prv = NWAMUI_DAEMON(data)->prv;

    g_debug ("nwam_ncp_walker_cb 0x%p", ncp);
    
    new_ncp = nwamui_ncp_new_with_handle (ncp);
        
    prv->ncp_list = g_list_append(prv->ncp_list, (gpointer)new_ncp);

    return(0);
}

