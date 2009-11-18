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
 * File:   nwamui_object.c
 *
 */

#include <glib-object.h>
#include <glib/gi18n.h>

#include "libnwamui.h"

enum {
    PROP_NWAM_STATE = 1,
    LAST_PROP
};

enum {
	PLACEHOLDER,
	LAST_SIGNAL
};

static guint nwamui_object_signals[LAST_SIGNAL] = {0};

struct _NwamuiObjectPrivate {
    /* State caching, store up to NWAM_NCU_TYPE_ANY values to allow for NCU
     * classes to have extra state per NCU class type */
    nwam_state_t                    nwam_state;
    nwam_aux_state_t                nwam_aux_state;
    time_t                          nwam_state_last_update;
};

static GObject* nwamui_object_constructor(GType type,
  guint n_construct_properties,
  GObjectConstructParam *construct_properties);

static void nwamui_object_set_property(GObject         *object,
    guint            prop_id,
    const GValue    *value,
    GParamSpec      *pspec);

static void nwamui_object_get_property(GObject         *object,
    guint            prop_id,
    GValue          *value,
    GParamSpec      *pspec);

static void nwamui_object_finalize(NwamuiObject *self);

static nwam_state_t default_nwamui_object_get_nwam_state(NwamuiObject *object, nwam_aux_state_t* aux_state_p, const gchar**aux_state_string_p);

/* Callbacks */
static void nwamui_object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);

#define NWAMUI_OBJECT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE((o), NWAMUI_TYPE_OBJECT, NwamuiObjectPrivate))
G_DEFINE_TYPE(NwamuiObject, nwamui_object, G_TYPE_OBJECT)

static void
nwamui_object_class_init(NwamuiObjectClass *klass)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->constructor = nwamui_object_constructor;
	gobject_class->finalize = (void (*)(GObject*)) nwamui_object_finalize;
	gobject_class->set_property = nwamui_object_set_property;
	gobject_class->get_property = nwamui_object_get_property;

    klass->get_name = NULL;
    klass->set_name = NULL;
    klass->get_conditions = NULL;
    klass->set_conditions = NULL;
    klass->get_activation_mode = NULL;
    klass->set_activation_mode = NULL;
    klass->get_nwam_state = NULL;
/*     klass->get_nwam_state = default_nwamui_object_get_nwam_state; */

	g_type_class_add_private(klass, sizeof (NwamuiObjectPrivate));

    g_object_class_install_property (gobject_class,
      PROP_NWAM_STATE,
      g_param_spec_uint ("nwam_state",
        _("nwam_state"),
        _("nwam_state"),
        0,
        G_MAXUINT,
        0,
        G_PARAM_READABLE));
}

static void
nwamui_object_init(NwamuiObject *self)
{
    self->prv = g_new0 (NwamuiObjectPrivate, 1);

/*     g_signal_connect(G_OBJECT(self), "notify", (GCallback)nwamui_object_notify_cb, (gpointer)self); */
}

static void
nwamui_object_set_property(GObject         *object,
    guint            prop_id,
    const GValue    *value,
    GParamSpec      *pspec)
{
	NwamuiObject *self = NWAMUI_OBJECT(object);

	switch (prop_id) {
        default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nwamui_object_get_property(GObject         *object,
    guint            prop_id,
    GValue          *value,
    GParamSpec      *pspec)
{
	NwamuiObject *self = NWAMUI_OBJECT(object);

	switch (prop_id) {
        case PROP_NWAM_STATE: {
                g_value_set_uint( value, (guint)self->prv->nwam_state );
            }
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static GObject*
nwamui_object_constructor(GType type,
  guint n_construct_properties,
  GObjectConstructParam *construct_properties)
{
	NwamuiObject *self;
	GObject *object;
	GError *error = NULL;

	object = G_OBJECT_CLASS(nwamui_object_parent_class)->constructor(type,
	    n_construct_properties,
	    construct_properties);
	self = NWAMUI_OBJECT(object);

	return object;
}

static void
nwamui_object_finalize(NwamuiObject *self)
{
	G_OBJECT_CLASS(nwamui_object_parent_class)->finalize(G_OBJECT (self));
}

/**
 * nwamui_object_can_rename:
 * @nwamui_object: a #NwamuiObject.
 * @returns: TRUE if the name.can be changed.
 *
 **/
extern gboolean
nwamui_object_can_rename (NwamuiObject *object)
{
    g_return_val_if_fail (NWAMUI_IS_OBJECT (object), FALSE);
    g_return_val_if_fail (NWAMUI_OBJECT_GET_CLASS (object)->can_rename, FALSE);

    return NWAMUI_OBJECT_GET_CLASS (object)->can_rename(object);
}

/** 
 * nwamui_object_set_name:
 * @nwamui_object: a #NwamuiObject.
 * @name: Value to set name to.
 * 
 **/ 
extern void
nwamui_object_set_name (   NwamuiObject *object,
                             const gchar* name )
{
    g_return_if_fail (NWAMUI_IS_OBJECT (object));
    g_return_if_fail (NWAMUI_OBJECT_GET_CLASS (object)->set_name);
    g_assert (name != NULL );

    NWAMUI_OBJECT_GET_CLASS (object)->set_name(object, name);
}

/**
 * nwamui_object_get_name:
 * @nwamui_object: a #NwamuiObject.
 * @returns: the name.
 *
 **/
extern gchar *
nwamui_object_get_name (NwamuiObject *object)
{
    g_return_val_if_fail (NWAMUI_IS_OBJECT (object), NULL);
/*     g_return_val_if_fail (NWAMUI_OBJECT_GET_CLASS (object)->get_name, NULL); */

    if (NWAMUI_OBJECT_GET_CLASS (object)->get_name)
        return NWAMUI_OBJECT_GET_CLASS (object)->get_name(object);
    else
        return NULL;
}

/** 
 * nwamui_object_set_conditions:
 * @nwamui_object: a #NwamuiObject.
 * @conditions: Value to set conditions to.
 * 
 **/ 
extern void
nwamui_object_set_conditions (   NwamuiObject *object,
                             const GList* conditions )
{
    g_return_if_fail (NWAMUI_IS_OBJECT (object));
    g_return_if_fail (NWAMUI_OBJECT_GET_CLASS (object)->set_conditions);

    NWAMUI_OBJECT_GET_CLASS (object)->set_conditions(object, conditions);
}

/**
 * nwamui_object_get_conditions:
 * @nwamui_object: a #NwamuiObject.
 * @returns: the conditions.
 *
 **/
extern GList*
nwamui_object_get_conditions (NwamuiObject *object)
{
    g_return_val_if_fail (NWAMUI_IS_OBJECT (object), NULL);
    g_return_val_if_fail (NWAMUI_OBJECT_GET_CLASS (object)->get_conditions, NULL);

    return NWAMUI_OBJECT_GET_CLASS (object)->get_conditions(object);
}

extern gint
nwamui_object_get_activation_mode(NwamuiObject *object)
{
    g_return_val_if_fail (NWAMUI_IS_OBJECT (object), NWAMUI_COND_ACTIVATION_MODE_LAST);
    g_return_val_if_fail (NWAMUI_OBJECT_GET_CLASS (object)->get_activation_mode, NWAMUI_COND_ACTIVATION_MODE_LAST);

    return NWAMUI_OBJECT_GET_CLASS (object)->get_activation_mode(object);
}

extern void
nwamui_object_set_activation_mode(NwamuiObject *object, gint activation_mode)
{
    g_return_if_fail (NWAMUI_IS_OBJECT (object));
    g_return_if_fail (NWAMUI_OBJECT_GET_CLASS (object)->set_activation_mode);

    NWAMUI_OBJECT_GET_CLASS (object)->set_activation_mode(object, activation_mode);
}

extern gboolean
nwamui_object_get_active(NwamuiObject *object)
{
    g_return_val_if_fail (NWAMUI_IS_OBJECT (object), FALSE);
    g_return_val_if_fail (NWAMUI_OBJECT_GET_CLASS (object)->get_active, FALSE);

    return NWAMUI_OBJECT_GET_CLASS (object)->get_active(object);
}

extern void
nwamui_object_set_active(NwamuiObject *object, gboolean active)
{
    g_return_if_fail (NWAMUI_IS_OBJECT (object));
    g_return_if_fail (NWAMUI_OBJECT_GET_CLASS (object)->set_active);

    NWAMUI_OBJECT_GET_CLASS (object)->set_active(object, active);
}

extern gboolean
nwamui_object_get_enabled(NwamuiObject *object)
{
    g_return_val_if_fail (NWAMUI_IS_OBJECT (object), FALSE);
    g_return_val_if_fail (NWAMUI_OBJECT_GET_CLASS (object)->get_enabled, FALSE);

    return NWAMUI_OBJECT_GET_CLASS (object)->get_enabled(object);
}

extern void
nwamui_object_set_enabled(NwamuiObject *object, gboolean enabled)
{
    g_return_if_fail (NWAMUI_IS_OBJECT (object));
    g_return_if_fail (NWAMUI_OBJECT_GET_CLASS (object)->set_enabled);

    NWAMUI_OBJECT_GET_CLASS (object)->set_enabled(object, enabled);
}

extern gboolean
nwamui_object_commit(NwamuiObject *object)
{
    g_return_val_if_fail (NWAMUI_IS_OBJECT (object), FALSE);
    g_return_val_if_fail (NWAMUI_OBJECT_GET_CLASS (object)->commit, FALSE);

    return NWAMUI_OBJECT_GET_CLASS (object)->commit(object);
}

extern gboolean
nwamui_object_destroy(NwamuiObject *object)
{
    g_return_val_if_fail (NWAMUI_IS_OBJECT (object), FALSE);
    g_return_val_if_fail (NWAMUI_OBJECT_GET_CLASS (object)->destroy, FALSE);

    return NWAMUI_OBJECT_GET_CLASS (object)->destroy(object);
}

extern void
nwamui_object_reload(NwamuiObject *object)
{
    g_return_if_fail (NWAMUI_IS_OBJECT (object));
    g_return_if_fail (NWAMUI_OBJECT_GET_CLASS (object)->reload);

    NWAMUI_OBJECT_GET_CLASS (object)->reload(object);
}

static time_t   _nwamui_state_timeout = 60; /* Seconds */

extern nwam_state_t         
nwamui_object_get_nwam_state(NwamuiObject *object, nwam_aux_state_t* aux_state_p, const gchar**aux_state_string_p)
{
    NwamuiObjectPrivate *prv         = NWAMUI_OBJECT_GET_PRIVATE(object);
    nwam_state_t         rval        = NWAM_STATE_UNINITIALIZED;
    nwam_state_t         _state      = NWAM_STATE_UNINITIALIZED;
    nwam_aux_state_t     _aux_state  = NWAM_AUX_STATE_UNINITIALIZED;
    time_t               _elapsed_time;
    time_t               _current_time;

    g_return_val_if_fail (NWAMUI_IS_OBJECT (object), rval );

    if ( NWAMUI_OBJECT_GET_CLASS (object)->get_nwam_state == NULL ) {
        
        if ( prv->nwam_state_last_update > 0) {
            /* It's possible that the object doesn't have it's own get_state
             * function, yet the state has been updated due to an event.
             *
             * If this has happened, then use that information.
             */
            _state = prv->nwam_state;
            _aux_state = prv->nwam_aux_state;
        }

        if ( aux_state_p ) {
            *aux_state_p = _aux_state;
        }

        if ( aux_state_string_p ) {
            *aux_state_string_p = (const gchar*)nwam_aux_state_to_string( _aux_state );
        }

/*         g_debug("Get default: %-10s - %s", nwam_state_to_string(_state), nwam_aux_state_to_string(_aux_state)); */
        return( _state );
    }

    _current_time = time( NULL );;

    _elapsed_time = _current_time - prv->nwam_state_last_update;

    if ( _elapsed_time > _nwamui_state_timeout ) {
        gboolean state_changed = FALSE;

        _state = NWAMUI_OBJECT_GET_CLASS (object)->get_nwam_state(object, &_aux_state, NULL);

        if ( _state != prv->nwam_state ) {
            state_changed = TRUE;
        }
        if ( _aux_state != prv->nwam_aux_state ) {
            state_changed = TRUE;
        }

        /* Update internal cache */
        prv->nwam_state = _state;
        prv->nwam_aux_state = _aux_state;
        prv->nwam_state_last_update = _current_time;

        if ( state_changed ) {
            g_object_notify(G_OBJECT(object), "nwam_state" );
        }

        /* Active property is likely to have changed too, but first check if there
         * is a get_active function, if so the active property should exist. */
        /* For NCUs, only care cache_index == 0. Compatible to ENV, ENM, etc. */
        if ( state_changed && NWAMUI_OBJECT_GET_CLASS (object)->get_active != NULL ) {
            g_object_notify(G_OBJECT(object), "active" );
        }
    }
    else {
        _state = prv->nwam_state;
        _aux_state = prv->nwam_aux_state;
    }

    rval = _state;

    if ( aux_state_p ) {
        *aux_state_p = _aux_state;
    }
    if ( aux_state_string_p ) {
        *aux_state_string_p = _((const gchar*)nwam_aux_state_to_string( _aux_state ));
    }

/*     g_debug("Get: %-10s - %s", nwam_state_to_string(_state), nwam_aux_state_to_string(_aux_state)); */
    return(rval);
}

extern void
nwamui_object_set_nwam_state(NwamuiObject *object, nwam_state_t state, nwam_aux_state_t aux_state)
{
    NwamuiObjectPrivate *prv           = NWAMUI_OBJECT_GET_PRIVATE(object);
    gboolean             state_changed = FALSE;

    g_return_if_fail (NWAMUI_IS_OBJECT (object));

    /* For NCU object state, we will see dup events for link and interface. But
     * we actually only (or should) care interface events to avoid dup events and
     * dup GUI notifications.
     * We will map cache_index = 0 to type_interface. Then We should monitor 
     * cache_index is 0 
     */
    if ( prv->nwam_state != state ||
         prv->nwam_aux_state != aux_state ) {
        state_changed = TRUE;
    }

    prv->nwam_state = state;
    prv->nwam_aux_state = aux_state;
    prv->nwam_state_last_update = time( NULL );;
    if ( state_changed ) {
/*         g_debug("Set: %-10s - %s", nwam_state_to_string(state), nwam_aux_state_to_string(aux_state)); */
        g_object_notify(G_OBJECT(object), "nwam_state" );
    }

    /* Active property is likely to have changed too, but first check if there
     * is a get_active function, if so the active property should exist. */
    /* For NCUs, only care cache_index == 0. Compatible to ENV, ENM, etc. */
    if ( state_changed && NWAMUI_OBJECT_GET_CLASS (object)->get_active != NULL ) {
        g_object_notify(G_OBJECT(object), "active" );
    }
}

/* Callbacks */

static void
nwamui_object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
    NwamuiObject* self = NWAMUI_OBJECT(data);

/*     g_debug("%s 0x%p : notify '%s'", g_type_name(G_TYPE_FROM_INSTANCE(gobject)), gobject, arg1->name); */
    g_debug("%s '%s' 0x%p : notify '%s'", g_type_name(G_TYPE_FROM_INSTANCE(gobject)), nwamui_object_get_name(self), gobject, arg1->name);
}

