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
 * File:   nwamui_prof.c
 *
 */

#include <glib-object.h>
#include <glib/gi18n.h>
#include <gconf/gconf-client.h>

#include "libnwamui.h"

static NwamuiProf *instance = NULL;

enum {
    S_JOIN_WIFI_NOT_IN_FAV = 0,
    S_JOIN_ANY_FAV_WIFI,
    S_ADD_ANY_NEW_WIFI_TO_FAV,
    S_ACTION_ON_NO_FAV_NETWORKS,
    LAST_SIGNAL
};

enum {
    PROP_JOIN_WIFI_NOT_IN_FAV = 1,
    PROP_JOIN_ANY_FAV_WIFI,
    PROP_ADD_ANY_NEW_WIFI_TO_FAV,
    PROP_ACTION_ON_NO_FAV_NETWORKS,
};

static guint nwamui_daemon_signals [LAST_SIGNAL] = { 0 };

#define PROF_GCONF_ROOT "/apps/nwam-manager"
#define PROF_BOOL_JOIN_WIFI_NOT_IN_FAV PROF_GCONF_ROOT \
	"/join_wifi_not_in_fav"
#define PROF_BOOL_JOIN_ANY_FAV_WIFI PROF_GCONF_ROOT \
	"/join_any_fav_wifi"
#define PROF_BOOL_ADD_ANY_NEW_WIFI_TO_FAV PROF_GCONF_ROOT \
    "/add_any_new_wifi_to_fav"
#define PROF_STRING_ACTION_ON_NO_FAV_NETWORKS PROF_GCONF_ROOT \
    "/action_on_no_fav_networks"

struct _NwamuiProfPrivate {

    /*<private>*/
    GConfClient* client;
    guint gconf_notify_id;
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

/* Callbacks */
static void object_notify_cb (GObject *gobject, GParamSpec *arg1, gpointer data);

#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
	NWAMUI_TYPE_PROF, NwamuiProfPrivate)) 

G_DEFINE_TYPE (NwamuiProf, nwamui_prof, G_TYPE_OBJECT)

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

    /* Create some signals */
    nwamui_daemon_signals[S_JOIN_WIFI_NOT_IN_FAV] =   
      g_signal_new ("join_wifi_not_in_fav",
        G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
        0,                            /* No class method */
        NULL, NULL,
        g_cclosure_marshal_VOID__BOOLEAN,
        G_TYPE_NONE,                  /* Return Type */
        1,                            /* Number of Args */
        G_TYPE_BOOLEAN);              /* Types of Args */
    
    nwamui_daemon_signals[S_JOIN_ANY_FAV_WIFI] =   
      g_signal_new ("join_any_fav_wifi",
        G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
        0,                            /* No class method */
        NULL, NULL,
        g_cclosure_marshal_VOID__BOOLEAN,
        G_TYPE_NONE,                  /* Return Type */
        1,                            /* Number of Args */
        G_TYPE_BOOLEAN);              /* Types of Args */

    nwamui_daemon_signals[S_ADD_ANY_NEW_WIFI_TO_FAV] =   
      g_signal_new ("add_any_new_wifi_to_fav",
        G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
        0,                            /* No class method */
        NULL, NULL,
        g_cclosure_marshal_VOID__BOOLEAN,
        G_TYPE_NONE,                  /* Return Type */
        1,                            /* Number of Args */
        G_TYPE_BOOLEAN);              /* Types of Args */

    nwamui_daemon_signals[S_ACTION_ON_NO_FAV_NETWORKS] =   
      g_signal_new ("action_on_no_fav_networks",
        G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
        0,                            /* No class method */
        NULL, NULL,
        g_cclosure_marshal_VOID__INT,
        G_TYPE_NONE,                  /* Return Type */
        1,                            /* Number of Args */
        G_TYPE_INT);              /* Types of Args */
}


static void
nwamui_prof_init (NwamuiProf *self)
{
	NwamuiProfPrivate *prv = GET_PRIVATE(self);
	self->prv = prv;
    GError *err = NULL;
    
    prv->client = gconf_client_get_default ();

    gconf_client_add_dir (self->prv->client,
      PROF_GCONF_ROOT,
      GCONF_CLIENT_PRELOAD_ONELEVEL,
      &err);

    if (err) {
        g_error ("Unable to call gconf_client_add_dir: %s\n", err->message);
        g_error_free (err);
        g_assert_not_reached ();
    }
    
    g_signal_connect(G_OBJECT(self), "notify", (GCallback)object_notify_cb, (gpointer)self);
}

static void
nwamui_prof_set_property (GObject         *object,
  guint            prop_id,
  const GValue    *value,
  GParamSpec      *pspec)
{
    NwamuiProf *self = NWAMUI_PROF(object);
    NwamuiProfPrivate *prv = self->prv;
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
        gconf_client_set_int (prv->client, PROF_STRING_ACTION_ON_NO_FAV_NETWORKS,
          g_value_get_int (value),
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
    NwamuiProf *self = NWAMUI_PROF(object);
    NwamuiProfPrivate *prv = self->prv;
    GError *err = NULL;

    switch (prop_id) {
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
        g_value_set_int (value, gconf_client_get_int (prv->client,
                           PROF_STRING_ACTION_ON_NO_FAV_NETWORKS,
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
    NwamuiProfPrivate *prv = self->prv;
    GError *err = NULL;

    gconf_client_remove_dir (self->prv->client,
      PROF_GCONF_ROOT,
      &err);

    if (err) {
        g_error ("Unable to call gconf_client_remove_dir: %s\n", err->message);
        g_error_free (err);
        g_assert_not_reached ();
    }

    /* we may not need to remove notify */
    if (prv->gconf_notify_id) {
        gconf_client_notify_remove (prv->client, prv->gconf_notify_id);
    }

    if (prv->client) {
        g_object_unref (prv->client);
    }

	(*G_OBJECT_CLASS(nwamui_prof_parent_class)->finalize) (G_OBJECT(self));

    /* tricky */
    g_assert (instance);
    instance = NULL;
}

static void gconf_notify_cb (GConfClient *client,
  guint cnxn_id,
  GConfEntry *entry,
  gpointer user_data)
{
    NwamuiProf *self = NWAMUI_PROF(user_data);
    const char *key;
    GConfValue *value;
    
    key = gconf_entry_get_key (entry);
    value = gconf_entry_get_value (entry);
    
    if (g_ascii_strcasecmp (key, PROF_BOOL_JOIN_WIFI_NOT_IN_FAV) == 0) {
        g_signal_emit (self,
          nwamui_daemon_signals[S_JOIN_WIFI_NOT_IN_FAV],
          0, /* details */
          gconf_value_get_bool (value));
    } else if (g_ascii_strcasecmp (key, PROF_BOOL_JOIN_ANY_FAV_WIFI) == 0) {
        g_signal_emit (self,
          nwamui_daemon_signals[S_JOIN_ANY_FAV_WIFI],
          0, /* details */
          gconf_value_get_bool (value));
    } else if (g_ascii_strcasecmp (key, PROF_BOOL_ADD_ANY_NEW_WIFI_TO_FAV) == 0) {
        g_signal_emit (self,
          nwamui_daemon_signals[S_ADD_ANY_NEW_WIFI_TO_FAV],
          0, /* details */
          gconf_value_get_bool (value));
    } else if (g_ascii_strcasecmp (key, PROF_STRING_ACTION_ON_NO_FAV_NETWORKS) == 0) {
        g_signal_emit (self,
          nwamui_daemon_signals[S_ACTION_ON_NO_FAV_NETWORKS],
          0, /* details */
          gconf_value_get_int (value));
    } else {
        g_assert_not_reached ();
    }
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
        instance = NWAMUI_PROF (g_object_new (NWAMUI_TYPE_PROF, NULL));
    } else {
        g_object_ref (instance);
    }
    
    return (instance);
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

/* Callbacks */

static void
object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
    NwamuiProf* self = NWAMUI_PROF(data);

    g_debug("NwamuiProf: notify %s changed\n", arg1->name);
}
