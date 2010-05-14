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
 * File:   nwamui_prof.c
 *
 */
/* Auth related headers */
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include <auth_attr.h>
#include <secdb.h>

#include <glib-object.h>
#include <glib/gi18n.h>
#include <gconf/gconf-client.h>

#include "libnwamui.h"

static NwamuiProf *instance = NULL;

enum {
    SIG_PLACE_HOLDER = 0,
    LAST_SIGNAL
};

enum {
    PROP_UI_AUTH = 1,
    PROP_JOIN_WIFI_NOT_IN_FAV,
    PROP_JOIN_ANY_FAV_WIFI,
    PROP_ADD_ANY_NEW_WIFI_TO_FAV,
    PROP_ACTION_ON_NO_FAV_NETWORKS,
    PROP_ACTIVE_INTERFACE,
    PROP_NOTIFICATION_DEFAULT_TIMEOUT,
    PROP_NOTIFICATION_FLAGS,
    PROP_NOTIFICATION_NCU_CONNECTED,
    PROP_NOTIFICATION_NCU_DISCONNECTED,
    PROP_NOTIFICATION_NCU_WIFI_CONNECT_FAILED,
    PROP_NOTIFICATION_NCU_WIFI_SELECTION_NEEDED,
    PROP_NOTIFICATION_NCU_WIFI_KEY_NEEDED,
    PROP_NOTIFICATION_NO_WIFI_NETWORKS,
    PROP_NOTIFICATION_NCP_CHANGED,
    PROP_NOTIFICATION_LOCATION_CHANGED,
    PROP_NOTIFICATION_NWAM_UNAVAILABLE,
};

static guint nwamui_prof_signals [LAST_SIGNAL] = { 0 };

struct auth_data {
    const gchar *name;
    guint v;
} auths[] = {
    { AUTOCONF_READ_AUTH, UI_AUTH_AUTOCONF_READ_AUTH },
    { AUTOCONF_SELECT_AUTH, UI_AUTH_AUTOCONF_SELECT_AUTH },
    { AUTOCONF_WLAN_AUTH, UI_AUTH_AUTOCONF_WLAN_AUTH },
    { AUTOCONF_WRITE_AUTH, UI_AUTH_AUTOCONF_WRITE_AUTH },
    { NULL, 0 }
};

/*
 * GConf related settings.
 */
#define PROF_GCONF_ROOT "/apps/nwam-manager"
#define PROF_BOOL_JOIN_WIFI_NOT_IN_FAV PROF_GCONF_ROOT \
	"/join_wifi_not_in_fav"
#define PROF_BOOL_JOIN_ANY_FAV_WIFI PROF_GCONF_ROOT \
	"/join_any_fav_wifi"
#define PROF_BOOL_ADD_ANY_NEW_WIFI_TO_FAV PROF_GCONF_ROOT \
    "/add_any_new_wifi_to_fav"
#define PROF_STRING_ACTION_ON_NO_FAV_NETWORKS PROF_GCONF_ROOT \
    "/action_on_no_fav_networks"

/* To Allow GNOME Netstatus Applet to follow NWAM Active interface */
#define PROF_STRING_ACTIVE_INTERFACE PROF_GCONF_ROOT \
    "/active_interface"

/* Accept positive milliseconds, default is 2000 */
#define PROF_INT_NOTIFICATION_DEFAULT_TIMEOUT PROF_GCONF_ROOT \
    "/notification_default_timeout"

/* Notification flags, what to show and what not to show */
#define PROF_GCONF_NOTIFICATION_ROOT \
    PROF_GCONF_ROOT "/notifications"
#define PROF_BOOL_NOTIFICATION_NCU_CONNECTED \
    PROF_GCONF_NOTIFICATION_ROOT "/ncu_connected"
#define PROF_BOOL_NOTIFICATION_NCU_DISCONNECTED \
    PROF_GCONF_NOTIFICATION_ROOT "/ncu_disconnected"
#define PROF_BOOL_NOTIFICATION_NCU_WIFI_CONNECT_FAILED \
    PROF_GCONF_NOTIFICATION_ROOT "/ncu_wifi_connect_failed"
#define PROF_BOOL_NOTIFICATION_NCU_WIFI_SELECTION_NEEDED \
    PROF_GCONF_NOTIFICATION_ROOT "/ncu_wifi_selection_needed"
#define PROF_BOOL_NOTIFICATION_NCU_WIFI_KEY_NEEDED \
    PROF_GCONF_NOTIFICATION_ROOT "/ncu_wifi_key_needed"
#define PROF_BOOL_NOTIFICATION_NO_WIFI_NETWORKS \
    PROF_GCONF_NOTIFICATION_ROOT "/no_wifi_networks"
#define PROF_BOOL_NOTIFICATION_NCP_CHANGED \
    PROF_GCONF_NOTIFICATION_ROOT "/ncp_changed"
#define PROF_BOOL_NOTIFICATION_LOCATION_CHANGED \
    PROF_GCONF_NOTIFICATION_ROOT "/location_changed"
#define PROF_BOOL_NOTIFICATION_NWAM_UNAVAILABLE \
    PROF_GCONF_NOTIFICATION_ROOT "/nwam_unavailable"


struct _NwamuiProfPrivate {
    GConfClient *client;
    guint        gconf_notify_id;
    guint        ui_auth;
};

static void nwamui_prof_set_property ( GObject         *object,
  guint            prop_id,
  const GValue    *value,
  GParamSpec      *pspec);

static void nwamui_prof_get_property ( GObject         *object,
  guint            prop_id,
  GValue          *value,
  GParamSpec      *pspec);

static void nwamui_prof_finalize (NwamuiProf *self);

static void gconf_notify_cb (GConfClient *client,
  guint cnxn_id,
  GConfEntry *entry,
  gpointer user_data);

static gboolean user_has_autoconf_auth(NwamuiProf *self);

#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
	NWAMUI_TYPE_PROF, NwamuiProfPrivate)) 

G_DEFINE_TYPE (NwamuiProf, nwamui_prof, NWAMUI_TYPE_OBJECT)

static void
nwamui_prof_class_init (NwamuiProfClass *klass)
{
    /* Pointer to GObject Part of Class */
    GObjectClass *gobject_class = (GObjectClass*) klass;
        
    /* Override Some Function Pointers */
    gobject_class->set_property = nwamui_prof_set_property;
    gobject_class->get_property = nwamui_prof_get_property;
    gobject_class->finalize = (void (*)(GObject*)) nwamui_prof_finalize;
    g_type_class_add_private (klass, sizeof (NwamuiProfPrivate));

    /* Create some properties */
    g_object_class_install_property (gobject_class,
      PROP_UI_AUTH,
      g_param_spec_uint ("ui_auth",
        _("ui_auth"),
        _("ui_auth"),
        0,
        G_MAXUINT,
        0,
        G_PARAM_READABLE));

    g_object_class_install_property (gobject_class,
      PROP_JOIN_WIFI_NOT_IN_FAV,
      g_param_spec_boolean ("join_wifi_not_in_fav",
        _("Joining an open network that is not in your favorites list"),
        _("Joining an open network that is not in your favorites list"),
        TRUE,
        G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
      PROP_JOIN_ANY_FAV_WIFI,
      g_param_spec_boolean ("join_any_fav_wifi",
        _("Joining any favorite network"),
        _("Joining any favorite network"),
        FALSE,
        G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
      PROP_ADD_ANY_NEW_WIFI_TO_FAV,
      g_param_spec_boolean ("add_any_new_wifi_to_fav",
        _("Adding any new network to your favorites list"),
        _("Adding any new network to your favorites list"),
        TRUE,
        G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
      PROP_ACTION_ON_NO_FAV_NETWORKS,
      g_param_spec_int ("action_on_no_fav_networks",
        _("The action if no favorite networks are available"),
        _("The action if no favorite networks are available"),
        PROF_SHOW_AVAILABLE_NETWORK_LIST,
        PROF_REMAIN_OFFLINE,
        PROF_SHOW_AVAILABLE_NETWORK_LIST,
        G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
      PROP_NOTIFICATION_DEFAULT_TIMEOUT,
      g_param_spec_int ("notification_default_timeout",
        _("Active Interface"),
        _("Active Interface"),
        1,
        G_MAXINT,
        2000,
        G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
      PROP_NOTIFICATION_NCU_CONNECTED,
      g_param_spec_boolean ("ncu_connected",
        _("Show notifications for ncu connected"),
        _("Show notifications for ncu connected"),
        TRUE,
        G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
      PROP_NOTIFICATION_NCU_DISCONNECTED,
      g_param_spec_boolean ("ncu_disconnected",
        _("Show notifications for ncu disconnected"),
        _("Show notifications for ncu disconnected"),
        TRUE,
        G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
      PROP_NOTIFICATION_NCU_WIFI_CONNECT_FAILED,
      g_param_spec_boolean ("ncu_wifi_connect_failed",
        _("Show notifications for wifi connect failed"),
        _("Show notifications for wifi connect failed"),
        TRUE,
        G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
      PROP_NOTIFICATION_NCU_WIFI_SELECTION_NEEDED,
      g_param_spec_boolean ("ncu_wifi_selection_needed",
        _("Show notifications for wifi selection needed"),
        _("Show notifications for wifi selection needed"),
        TRUE,
        G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
      PROP_NOTIFICATION_NCU_WIFI_KEY_NEEDED,
      g_param_spec_boolean ("ncu_wifi_key_needed",
        _("Show notifications for wifi key needed"),
        _("Show notifications for wifi key needed"),
        TRUE,
        G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
      PROP_NOTIFICATION_NO_WIFI_NETWORKS,
      g_param_spec_boolean ("no_wifi_networks",
        _("Show notifications for no wifi networks found"),
        _("Show notifications for no wifi networks found"),
        TRUE,
        G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
      PROP_NOTIFICATION_NCP_CHANGED,
      g_param_spec_boolean ("ncp_changed",
        _("Show notifications for ncp changed"),
        _("Show notifications for ncp changed"),
        TRUE,
        G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
      PROP_NOTIFICATION_LOCATION_CHANGED,
      g_param_spec_boolean ("location_changed",
        _("Show notifications for location changed"),
        _("Show notifications for location changed"),
        TRUE,
        G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
      PROP_NOTIFICATION_NWAM_UNAVAILABLE,
      g_param_spec_boolean ("nwam_unavailable",
        _("Show notifications for nwam unavailable"),
        _("Show notifications for nwam unavailable"),
        TRUE,
        G_PARAM_READWRITE));

}


static void
nwamui_prof_init (NwamuiProf *self)
{
	NwamuiProfPrivate *prv = GET_PRIVATE(self);
    GError            *err = NULL;
	self->prv              = prv;
    
    user_has_autoconf_auth(self);

    prv->client = gconf_client_get_default ();

    gconf_client_add_dir (prv->client,
      PROF_GCONF_ROOT,
      GCONF_CLIENT_PRELOAD_RECURSIVE,
      &err);

    if (err) {
        g_error ("Unable to call gconf_client_add_dir: %s\n", err->message);
        g_error_free (err);
    }

    nwamui_prof_notify_begin(self);
}

static void
nwamui_prof_set_property (GObject         *object,
  guint            prop_id,
  const GValue    *value,
  GParamSpec      *pspec)
{
	NwamuiProfPrivate *prv = GET_PRIVATE(object);
    GError *err = NULL;

    switch (prop_id) {
    case PROP_JOIN_WIFI_NOT_IN_FAV: {
        gconf_client_set_bool (prv->client, PROF_BOOL_JOIN_WIFI_NOT_IN_FAV,
          g_value_get_boolean (value),
          &err);
    }
        break;

    case PROP_JOIN_ANY_FAV_WIFI: {
        gconf_client_set_bool (prv->client, PROF_BOOL_JOIN_ANY_FAV_WIFI,
          g_value_get_boolean (value),
          &err);
    }
        break;

    case PROP_ADD_ANY_NEW_WIFI_TO_FAV: {
        gconf_client_set_bool (prv->client, PROF_BOOL_ADD_ANY_NEW_WIFI_TO_FAV,
          g_value_get_boolean (value),
          &err);
    }
        break;

    case PROP_ACTION_ON_NO_FAV_NETWORKS: {
        gint conf_value;

        conf_value = g_value_get_int (value);

        if ( conf_value < (gint)NWAMUI_NO_FAV_ACTION_NONE ||
             conf_value >= (gint)NWAMUI_NO_FAV_ACTION_LAST ) {
            conf_value = (gint)NWAMUI_NO_FAV_ACTION_NONE;
        }

        gconf_client_set_int (prv->client, PROF_STRING_ACTION_ON_NO_FAV_NETWORKS,
                              conf_value, &err);
    }
        break;

    case PROP_ACTIVE_INTERFACE: {
        gconf_client_set_string (prv->client, PROF_STRING_ACTIVE_INTERFACE,
          g_value_get_string (value),
          &err);
    }
        break;

    case PROP_NOTIFICATION_DEFAULT_TIMEOUT: {
        gconf_client_set_int (prv->client,
          PROF_INT_NOTIFICATION_DEFAULT_TIMEOUT,
          g_value_get_int (value),
          &err);
    }
        break;


    case PROP_NOTIFICATION_NCU_CONNECTED: {
        gconf_client_set_bool (prv->client, PROF_BOOL_NOTIFICATION_NCU_CONNECTED,
          g_value_get_boolean (value),
          &err);
    }
        break;

    case PROP_NOTIFICATION_NCU_DISCONNECTED: {
        gconf_client_set_bool (prv->client, PROF_BOOL_NOTIFICATION_NCU_DISCONNECTED,
          g_value_get_boolean (value),
          &err);
    }
        break;

    case PROP_NOTIFICATION_NCU_WIFI_CONNECT_FAILED: {
        gconf_client_set_bool (prv->client, PROF_BOOL_NOTIFICATION_NCU_WIFI_CONNECT_FAILED,
          g_value_get_boolean (value),
          &err);
    }
        break;

    case PROP_NOTIFICATION_NCU_WIFI_SELECTION_NEEDED: {
        gconf_client_set_bool (prv->client, PROF_BOOL_NOTIFICATION_NCU_WIFI_SELECTION_NEEDED,
          g_value_get_boolean (value),
          &err);
    }
        break;

    case PROP_NOTIFICATION_NCU_WIFI_KEY_NEEDED: {
        gconf_client_set_bool (prv->client, PROF_BOOL_NOTIFICATION_NCU_WIFI_KEY_NEEDED,
          g_value_get_boolean (value),
          &err);
    }
        break;

    case PROP_NOTIFICATION_NO_WIFI_NETWORKS: {
        gconf_client_set_bool (prv->client, PROF_BOOL_NOTIFICATION_NO_WIFI_NETWORKS,
          g_value_get_boolean (value),
          &err);
    }
        break;

    case PROP_NOTIFICATION_NCP_CHANGED: {
        gconf_client_set_bool (prv->client, PROF_BOOL_NOTIFICATION_NCP_CHANGED,
          g_value_get_boolean (value),
          &err);
    }
        break;

    case PROP_NOTIFICATION_LOCATION_CHANGED: {
        gconf_client_set_bool (prv->client, PROF_BOOL_NOTIFICATION_LOCATION_CHANGED,
          g_value_get_boolean (value),
          &err);
    }
        break;

    case PROP_NOTIFICATION_NWAM_UNAVAILABLE: {
        gconf_client_set_bool (prv->client, PROF_BOOL_NOTIFICATION_NWAM_UNAVAILABLE,
          g_value_get_boolean (value),
          &err);
    }
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }

    if (err) {
        g_error ("Unable to call gconf_client_set_xxx: %s\n", err->message);
        g_error_free (err);
    }
}

static void
nwamui_prof_get_property (GObject         *object,
  guint            prop_id,
  GValue          *value,
  GParamSpec      *pspec)
{
	NwamuiProfPrivate *prv = GET_PRIVATE(object);
    GError *err = NULL;

    switch (prop_id) {
    case PROP_UI_AUTH:
        g_value_set_uint(value, nwamui_prof_get_ui_auth(NWAMUI_PROF(object)));
        break;

    case PROP_JOIN_WIFI_NOT_IN_FAV: {
            g_value_set_boolean (value, gconf_client_get_bool (prv->client,
                                   PROF_BOOL_JOIN_WIFI_NOT_IN_FAV,
                                   &err));
        }
        break;

    case PROP_JOIN_ANY_FAV_WIFI: {
            g_value_set_boolean (value, gconf_client_get_bool (prv->client,
                                   PROF_BOOL_JOIN_ANY_FAV_WIFI,
                                   &err));
        }
        break;

    case PROP_ADD_ANY_NEW_WIFI_TO_FAV: {
            g_value_set_boolean (value, gconf_client_get_bool (prv->client,
                                   PROF_BOOL_ADD_ANY_NEW_WIFI_TO_FAV,
                                   &err));
        }
        break;

    case PROP_ACTION_ON_NO_FAV_NETWORKS: {
            gint conf_value;

            conf_value = gconf_client_get_int (prv->client, PROF_STRING_ACTION_ON_NO_FAV_NETWORKS, &err);

            if ( conf_value < (gint)NWAMUI_NO_FAV_ACTION_NONE ||
                 conf_value >= (gint)NWAMUI_NO_FAV_ACTION_LAST ) {
                conf_value = (gint)NWAMUI_NO_FAV_ACTION_NONE;
            }

            g_value_set_int (value, conf_value );
        }
        break;

    case PROP_ACTIVE_INTERFACE: {
            g_value_set_string (value, gconf_client_get_string (prv->client,
                               PROF_STRING_ACTION_ON_NO_FAV_NETWORKS,
                               &err));
        }
        break;

    case PROP_NOTIFICATION_DEFAULT_TIMEOUT: {
            g_value_set_int (value, gconf_client_get_int (prv->client,
                PROF_INT_NOTIFICATION_DEFAULT_TIMEOUT,
                &err));
        }
        break;

    case PROP_NOTIFICATION_NCU_CONNECTED: {
            g_value_set_boolean (value, gconf_client_get_bool (prv->client,
                                   PROF_BOOL_NOTIFICATION_NCU_CONNECTED,
                                   &err));
        }
        break;

    case PROP_NOTIFICATION_NCU_DISCONNECTED: {
            g_value_set_boolean (value, gconf_client_get_bool (prv->client,
                                   PROF_BOOL_NOTIFICATION_NCU_DISCONNECTED,
                                   &err));
        }
        break;

    case PROP_NOTIFICATION_NCU_WIFI_CONNECT_FAILED: {
            g_value_set_boolean (value, gconf_client_get_bool (prv->client,
                                   PROF_BOOL_NOTIFICATION_NCU_WIFI_CONNECT_FAILED,
                                   &err));
        }
        break;

    case PROP_NOTIFICATION_NCU_WIFI_SELECTION_NEEDED: {
            g_value_set_boolean (value, gconf_client_get_bool (prv->client,
                                   PROF_BOOL_NOTIFICATION_NCU_WIFI_SELECTION_NEEDED,
                                   &err));
        }
        break;

    case PROP_NOTIFICATION_NCU_WIFI_KEY_NEEDED: {
            g_value_set_boolean (value, gconf_client_get_bool (prv->client,
                                   PROF_BOOL_NOTIFICATION_NCU_WIFI_KEY_NEEDED,
                                   &err));
        }
        break;

    case PROP_NOTIFICATION_NO_WIFI_NETWORKS: {
            g_value_set_boolean (value, gconf_client_get_bool (prv->client,
                                   PROF_BOOL_NOTIFICATION_NO_WIFI_NETWORKS,
                                   &err));
        }
        break;

    case PROP_NOTIFICATION_NCP_CHANGED: {
            g_value_set_boolean (value, gconf_client_get_bool (prv->client,
                                   PROF_BOOL_NOTIFICATION_NCP_CHANGED,
                                   &err));
        }
        break;

    case PROP_NOTIFICATION_LOCATION_CHANGED: {
            g_value_set_boolean (value, gconf_client_get_bool (prv->client,
                                   PROF_BOOL_NOTIFICATION_LOCATION_CHANGED,
                                   &err));
        }
        break;

    case PROP_NOTIFICATION_NWAM_UNAVAILABLE: {
            g_value_set_boolean (value, gconf_client_get_bool (prv->client,
                                   PROF_BOOL_NOTIFICATION_NWAM_UNAVAILABLE,
                                   &err));
        }
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }

    if (err) {
        g_error ("Unable to call gconf_client_get_xxx: %s\n", err->message);
        g_error_free (err);
    }
}

static void
nwamui_prof_finalize (NwamuiProf *self)
{
	NwamuiProfPrivate *prv = GET_PRIVATE(self);
    GError *err = NULL;

    /* we may not need to remove notify */
    if (prv->gconf_notify_id) {
        gconf_client_notify_remove (prv->client, prv->gconf_notify_id);
    }

    gconf_client_remove_dir (self->prv->client,
      PROF_GCONF_ROOT,
      &err);

    if (err) {
        g_error ("Unable to call gconf_client_remove_dir: %s\n", err->message);
        g_error_free (err);
    }

    if (prv->client) {
        g_object_unref (prv->client);
    }

	G_OBJECT_CLASS(nwamui_prof_parent_class)->finalize(G_OBJECT(self));

    /* tricky */
    g_assert (instance);
    instance = NULL;
}

static void
gconf_notify_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
    NwamuiProf *self = NWAMUI_PROF(user_data);
    const char *key;
    GConfValue *value;
    gchar *basename;
    
    key = gconf_entry_get_key (entry);
    value = gconf_entry_get_value (entry);
    basename = g_path_get_basename(key);
    
    /* Broadcase the changes. */
    g_object_notify(G_OBJECT(self), basename);
    g_free(basename);

    if (g_ascii_strcasecmp (key, PROF_BOOL_JOIN_WIFI_NOT_IN_FAV) == 0) {
        nwamui_debug( "join_wifi_not_in_fav set to %d",
          gconf_value_get_bool(value));
    } else if (g_ascii_strcasecmp (key, PROF_BOOL_JOIN_ANY_FAV_WIFI) == 0) {
        nwamui_debug( "join_any_fav_wifi set to %d",
          gconf_value_get_bool(value));
    } else if (g_ascii_strcasecmp (key, PROF_BOOL_ADD_ANY_NEW_WIFI_TO_FAV) == 0) {
        nwamui_debug( "add_any_new_wifi_to_fav set to %d",
          gconf_value_get_bool(value));
    } else if (g_ascii_strcasecmp (key, PROF_STRING_ACTION_ON_NO_FAV_NETWORKS) == 0) {
        nwamui_debug( "action_on_no_fav_networks set to %d",
          gconf_value_get_int(value));
    } else if (g_ascii_strcasecmp (key, PROF_INT_NOTIFICATION_DEFAULT_TIMEOUT ) == 0) {
        nwamui_debug( "notification_default_timeout set to %d",
          gconf_value_get_int(value));
    } else if (g_ascii_strcasecmp (key, PROF_BOOL_NOTIFICATION_NCU_CONNECTED) == 0) {
        nwamui_debug( "ncu_connected set to %d",
          gconf_value_get_bool(value));
    } else if (g_ascii_strcasecmp (key, PROF_BOOL_NOTIFICATION_NCU_DISCONNECTED) == 0) {
        nwamui_debug( "ncu_disconnected set to %d",
          gconf_value_get_bool(value));
    } else if (g_ascii_strcasecmp (key, PROF_BOOL_NOTIFICATION_NCU_WIFI_CONNECT_FAILED) == 0) {
        nwamui_debug( "ncu_wifi_connect_failed set to %d",
          gconf_value_get_bool(value));
    } else if (g_ascii_strcasecmp (key, PROF_BOOL_NOTIFICATION_NCU_WIFI_SELECTION_NEEDED) == 0) {
        nwamui_debug( "ncu_wifi_selection_needed set to %d",
          gconf_value_get_bool(value));
    } else if (g_ascii_strcasecmp (key, PROF_BOOL_NOTIFICATION_NCU_WIFI_KEY_NEEDED) == 0) {
        nwamui_debug( "ncu_wifi_key_needed set to %d",
          gconf_value_get_bool(value));
    } else if (g_ascii_strcasecmp (key, PROF_BOOL_NOTIFICATION_NO_WIFI_NETWORKS) == 0) {
        nwamui_debug( "no_wifi_networks set to %d",
          gconf_value_get_bool(value));
    } else if (g_ascii_strcasecmp (key, PROF_BOOL_NOTIFICATION_NCP_CHANGED) == 0) {
        nwamui_debug( "ncp_changed set to %d",
          gconf_value_get_bool(value));
    } else if (g_ascii_strcasecmp (key, PROF_BOOL_NOTIFICATION_LOCATION_CHANGED) == 0) {
        nwamui_debug( "location_changed set to %d",
          gconf_value_get_bool(value));
    } else if (g_ascii_strcasecmp (key, PROF_BOOL_NOTIFICATION_NWAM_UNAVAILABLE) == 0) {
        nwamui_debug( "nwam_unavailable set to %d",
          gconf_value_get_bool(value));
    }
}

static gboolean
user_has_autoconf_auth(NwamuiProf *self)
{
	NwamuiProfPrivate *prv    = GET_PRIVATE(self);
	struct passwd     *pwd;

	if ((pwd = getpwuid(getuid())) != NULL) {
        gint i;
        for (i = 0; auths[i].name; i++) {
            if (chkauthattr(auths[i].name, pwd->pw_name) == 1) {
                prv->ui_auth |= auths[i].v;
                g_debug("User %s has %s", pwd->pw_name, auths[i].name);
            } else {
                g_debug("User %s does not have %s", pwd->pw_name, auths[i].name);
            }
        }

        endpwent();

        /* At least has read auth. */
        if (prv->ui_auth && UI_AUTH_AUTOCONF_READ_AUTH) {
            g_object_notify(G_OBJECT(self), "ui_auth");
            return TRUE;
        }

	} else {
		g_debug("Unable to get users password entry");
	}

    return FALSE;
}

/**
 * nwamui_prof_get_instance_noref:
 * @returns: a #NwamuiProf without updateing ref count.
 *
 **/
NwamuiProf*
nwamui_prof_get_instance_noref ()
{
    if ( instance == NULL ) {
        instance = NWAMUI_PROF (g_object_new (NWAMUI_TYPE_PROF, NULL));
    }
    
    return (instance);
}

/**
 * nwamui_prof_get_instance:
 * @returns: a #NwamuiProf.
 *
 * Creates a singlenton #NwamuiProf.
 **/
NwamuiProf*
nwamui_prof_get_instance ()
{
    if ( instance == NULL ) {
        nwamui_prof_get_instance_noref();
    }

    g_object_ref (instance);

    return (instance);
}

extern gint
nwamui_prof_get_notification_default_timeout (NwamuiProf* self)
{
    gint timeout;
    
    g_return_val_if_fail (NWAMUI_IS_PROF(self), timeout); 
    
    g_object_get (G_OBJECT (self),
      "notification_default_timeout", &timeout,
      NULL);

    return( timeout );

}

extern void
nwamui_prof_set_notification_default_timeout ( NwamuiProf *self, gint notification_default_timeout )
{
    g_return_if_fail (NWAMUI_IS_PROF(self)); 
    
    g_assert (notification_default_timeout > 0 );

    g_object_set (G_OBJECT (self),
      "notification_default_timeout", notification_default_timeout,
      NULL);
}

extern guint
nwamui_prof_get_ui_auth(NwamuiProf *self)
{
	NwamuiProfPrivate *prv    = GET_PRIVATE(self);

    g_return_val_if_fail (NWAMUI_IS_PROF(self), 0);

    return prv->ui_auth;
}

extern gboolean
nwamui_prof_check_ui_auth(NwamuiProf *self, gint auth)
{
	NwamuiProfPrivate *prv    = GET_PRIVATE(self);

    g_return_val_if_fail (NWAMUI_IS_PROF(self), 0);

    return UI_CHECK_AUTH(prv->ui_auth, auth);
}

extern void
nwamui_prof_notify_begin (NwamuiProf* self)
{
    GError *err = NULL;

    self->prv->gconf_notify_id = gconf_client_notify_add (self->prv->client,
      PROF_GCONF_ROOT,
      gconf_notify_cb,
      (gpointer) self,
      NULL,
      &err);
    
    if (err) {
        g_error ("Unable to call gconf_client_notify_add: %s\n", err->message);
        g_error_free (err);
    }
}

const gchar*
nwamui_prof_get_no_fav_action_string( nwamui_action_on_no_fav_networks_t action )
{
    switch( action ) {
        case NWAMUI_NO_FAV_ACTION_NONE:
            return( _("Do not connect") );
        case NWAMUI_NO_FAV_ACTION_SHOW_LIST_DIALOG:
            return( _("Show list of available networks") );
        default:
            return( NULL );
    }
}


extern gboolean
nwamui_prof_get_notification_ncu_connected (NwamuiProf* self)
{
    gboolean value;

    g_return_val_if_fail (NWAMUI_IS_PROF(self), value);

    g_object_get (G_OBJECT (self),
      "ncu_connected", &value,
      NULL);

    return( value );

}

extern void
nwamui_prof_set_notification_ncu_connected ( NwamuiProf *self, gboolean notification_ncu_connected )
{
    g_return_if_fail (NWAMUI_IS_PROF(self));

    g_object_set (G_OBJECT (self),
      "ncu_connected", notification_ncu_connected,
      NULL);
}


extern gboolean
nwamui_prof_get_notification_ncu_disconnected (NwamuiProf* self)
{
    gboolean value;

    g_return_val_if_fail (NWAMUI_IS_PROF(self), value);

    g_object_get (G_OBJECT (self),
      "ncu_disconnected", &value,
      NULL);

    return( value );

}

extern void
nwamui_prof_set_notification_ncu_disconnected ( NwamuiProf *self, gboolean notification_ncu_disconnected )
{
    g_return_if_fail (NWAMUI_IS_PROF(self));

    g_object_set (G_OBJECT (self),
      "ncu_disconnected", notification_ncu_disconnected,
      NULL);
}


extern gboolean
nwamui_prof_get_notification_ncu_wifi_connect_failed (NwamuiProf* self)
{
    gboolean value;

    g_return_val_if_fail (NWAMUI_IS_PROF(self), value);

    g_object_get (G_OBJECT (self),
      "ncu_wifi_connect_failed", &value,
      NULL);

    return( value );

}

extern void
nwamui_prof_set_notification_ncu_wifi_connect_failed ( NwamuiProf *self, gboolean notification_ncu_wifi_connect_failed )
{
    g_return_if_fail (NWAMUI_IS_PROF(self));

    g_object_set (G_OBJECT (self),
      "ncu_wifi_connect_failed", notification_ncu_wifi_connect_failed,
      NULL);
}


extern gboolean
nwamui_prof_get_notification_ncu_wifi_selection_needed (NwamuiProf* self)
{
    gboolean value;

    g_return_val_if_fail (NWAMUI_IS_PROF(self), value);

    g_object_get (G_OBJECT (self),
      "ncu_wifi_selection_needed", &value,
      NULL);

    return( value );

}

extern void
nwamui_prof_set_notification_ncu_wifi_selection_needed ( NwamuiProf *self, gboolean notification_ncu_wifi_selection_needed )
{
    g_return_if_fail (NWAMUI_IS_PROF(self));

    g_object_set (G_OBJECT (self),
      "ncu_wifi_selection_needed", notification_ncu_wifi_selection_needed,
      NULL);
}


extern gboolean
nwamui_prof_get_notification_ncu_wifi_key_needed (NwamuiProf* self)
{
    gboolean value;

    g_return_val_if_fail (NWAMUI_IS_PROF(self), value);

    g_object_get (G_OBJECT (self),
      "ncu_wifi_key_needed", &value,
      NULL);

    return( value );

}

extern void
nwamui_prof_set_notification_ncu_wifi_key_needed ( NwamuiProf *self, gboolean notification_ncu_wifi_key_needed )
{
    g_return_if_fail (NWAMUI_IS_PROF(self));

    g_object_set (G_OBJECT (self),
      "ncu_wifi_key_needed", notification_ncu_wifi_key_needed,
      NULL);
}


extern gboolean
nwamui_prof_get_notification_no_wifi_networks (NwamuiProf* self)
{
    gboolean value;

    g_return_val_if_fail (NWAMUI_IS_PROF(self), value);

    g_object_get (G_OBJECT (self),
      "no_wifi_networks", &value,
      NULL);

    return( value );

}

extern void
nwamui_prof_set_notification_no_wifi_networks ( NwamuiProf *self, gboolean notification_no_wifi_networks )
{
    g_return_if_fail (NWAMUI_IS_PROF(self));

    g_object_set (G_OBJECT (self),
      "no_wifi_networks", notification_no_wifi_networks,
      NULL);
}


extern gboolean
nwamui_prof_get_notification_ncp_changed (NwamuiProf* self)
{
    gboolean value;

    g_return_val_if_fail (NWAMUI_IS_PROF(self), value);

    g_object_get (G_OBJECT (self),
      "ncp_changed", &value,
      NULL);

    return( value );

}

extern void
nwamui_prof_set_notification_ncp_changed ( NwamuiProf *self, gboolean notification_ncp_changed )
{
    g_return_if_fail (NWAMUI_IS_PROF(self));

    g_object_set (G_OBJECT (self),
      "ncp_changed", notification_ncp_changed,
      NULL);
}


extern gboolean
nwamui_prof_get_notification_location_changed (NwamuiProf* self)
{
    gboolean value;

    g_return_val_if_fail (NWAMUI_IS_PROF(self), value);

    g_object_get (G_OBJECT (self),
      "location_changed", &value,
      NULL);

    return( value );

}

extern void
nwamui_prof_set_notification_location_changed ( NwamuiProf *self, gboolean notification_location_changed )
{
    g_return_if_fail (NWAMUI_IS_PROF(self));

    g_object_set (G_OBJECT (self),
      "location_changed", notification_location_changed,
      NULL);
}


extern gboolean
nwamui_prof_get_notification_nwam_unavailable (NwamuiProf* self)
{
    gboolean value;

    g_return_val_if_fail (NWAMUI_IS_PROF(self), value);

    g_object_get (G_OBJECT (self),
      "nwam_unavailable", &value,
      NULL);

    return( value );

}

extern void
nwamui_prof_set_notification_nwam_unavailable ( NwamuiProf *self, gboolean notification_nwam_unavailable )
{
    g_return_if_fail (NWAMUI_IS_PROF(self));

    g_object_set (G_OBJECT (self),
      "nwam_unavailable", notification_nwam_unavailable,
      NULL);
}

