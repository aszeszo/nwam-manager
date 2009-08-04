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
    PROP_JOIN_WIFI_NOT_IN_FAV = 1,
    PROP_JOIN_ANY_FAV_WIFI,
    PROP_ADD_ANY_NEW_WIFI_TO_FAV,
    PROP_ACTION_ON_NO_FAV_NETWORKS,
    PROP_ACTIVE_INTERFACE,
    PROP_NOTIFICATION_DEFAULT_TIMEOUT,
    PROP_LOCK_CURRENT_LOC,
};

static guint nwamui_prof_signals [LAST_SIGNAL] = { 0 };

#define PROF_GCONF_ROOT "/apps/nwam-manager"
#define PROF_BOOL_JOIN_WIFI_NOT_IN_FAV PROF_GCONF_ROOT \
	"/join_wifi_not_in_fav"
#define PROF_BOOL_JOIN_ANY_FAV_WIFI PROF_GCONF_ROOT \
	"/join_any_fav_wifi"
#define PROF_BOOL_ADD_ANY_NEW_WIFI_TO_FAV PROF_GCONF_ROOT \
    "/add_any_new_wifi_to_fav"
#define PROF_STRING_ACTION_ON_NO_FAV_NETWORKS PROF_GCONF_ROOT \
    "/action_on_no_fav_networks"
#define PROF_BOOL_LOCK_CURRENT_LOC PROF_GCONF_ROOT  \
    "/lock_current_loc"

/* To Allow GNOME Netstatus Applet to follow NWAM Active interface */
#define PROF_STRING_ACTIVE_INTERFACE PROF_GCONF_ROOT \
    "/active_interface"

/* Accept positive milliseconds, default is 2000 */
#define PROF_INT_NOTIFICATION_DEFAULT_TIMEOUT PROF_GCONF_ROOT \
    "/notification_default_timeout"

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
      PROP_ACTIVE_INTERFACE,
      g_param_spec_string ("active_interface",
        _("Active Interface"),
        _("Active Interface"),
        "",
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
      PROP_LOCK_CURRENT_LOC,
      g_param_spec_boolean ("lock_current_loc",
        _("Lock current location"),
        _("Lock current location"),
        FALSE,
        G_PARAM_READWRITE));

}


static void
nwamui_prof_init (NwamuiProf *self)
{
	NwamuiProfPrivate *prv = GET_PRIVATE(self);
    GError *err = NULL;
	self->prv = prv;
    
    prv->client = gconf_client_get_default ();

    gconf_client_add_dir (prv->client,
      PROF_GCONF_ROOT,
      GCONF_CLIENT_PRELOAD_ONELEVEL,
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

    case PROP_LOCK_CURRENT_LOC:
        gconf_client_set_bool (prv->client, PROF_BOOL_LOCK_CURRENT_LOC,
          g_value_get_boolean (value),
          &err);
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

    case PROP_LOCK_CURRENT_LOC:
        g_value_set_boolean (value, gconf_client_get_bool (prv->client,
            PROF_BOOL_LOCK_CURRENT_LOC,
            &err));
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
    
    key = gconf_entry_get_key (entry);
    value = gconf_entry_get_value (entry);
    
    if (g_ascii_strcasecmp (key, PROF_BOOL_JOIN_WIFI_NOT_IN_FAV) == 0) {
        g_object_set(self, "join_wifi_not_in_fav",
          gconf_value_get_bool(value), NULL);
    } else if (g_ascii_strcasecmp (key, PROF_BOOL_JOIN_ANY_FAV_WIFI) == 0) {
        g_object_set(self, "join_any_fav_wifi",
          gconf_value_get_bool(value), NULL);
    } else if (g_ascii_strcasecmp (key, PROF_BOOL_ADD_ANY_NEW_WIFI_TO_FAV) == 0) {
        g_object_set(self, "add_any_new_wifi_to_fav",
          gconf_value_get_bool(value), NULL);
    } else if (g_ascii_strcasecmp (key, PROF_STRING_ACTION_ON_NO_FAV_NETWORKS) == 0) {
        g_object_set(self, "action_on_no_fav_networks",
          gconf_value_get_int(value), NULL);
    } else if (g_ascii_strcasecmp (key, PROF_STRING_ACTIVE_INTERFACE ) == 0) {
        g_object_set(self, "active_interface",
          gconf_value_get_string(value), NULL);
    } else if (g_ascii_strcasecmp (key, PROF_INT_NOTIFICATION_DEFAULT_TIMEOUT ) == 0) {
        g_object_set(self, "notification_default_timeout",
          gconf_value_get_int(value), NULL);
    } else if (g_ascii_strcasecmp (key, PROF_BOOL_LOCK_CURRENT_LOC) == 0) {
        g_object_set(self, "lock_current_loc",
          gconf_value_get_bool(value), NULL);
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
    }
    g_object_ref (instance);
    
    return (instance);
}

extern gchar*
nwamui_prof_get_active_interface (NwamuiProf* self)
{
    gchar*  interface = NULL;
    
    g_return_val_if_fail (NWAMUI_IS_PROF(self), interface); 
    
    g_object_get (G_OBJECT (self),
                  "active_interface", &interface,
                  NULL);

    return( interface );

}

extern void
nwamui_prof_set_active_interface ( NwamuiProf *self, const gchar* active_interface )
{
    g_return_if_fail (NWAMUI_IS_PROF(self)); 
    
    g_assert (active_interface != NULL );

    if ( active_interface != NULL ) {
        g_object_set (G_OBJECT (self),
                      "active_interface", active_interface,
                      NULL);
    }
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
