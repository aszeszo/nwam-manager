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
 * File:   nwamui_enm.c
 *
 */

#include <glib-object.h>
#include <glib/gi18n.h>
#include <strings.h>

#include "libnwamui.h"

static GObjectClass    *parent_class    = NULL;

struct _NwamuiEnmPrivate {
    gchar*               name;
    gboolean             active;
    gchar*               start_command;
    gchar*               stop_command;
    gchar*               smf_frmi;
};

enum {
    PROP_NAME = 1,
    PROP_ACTIVE,
    PROP_START_COMMAND,
    PROP_STOP_COMMAND,
    PROP_SMF_FRMI
};


static void nwamui_enm_set_property ( GObject         *object,
                                      guint            prop_id,
                                      const GValue    *value,
                                      GParamSpec      *pspec);

static void nwamui_enm_get_property ( GObject         *object,
                                      guint            prop_id,
                                      GValue          *value,
                                      GParamSpec      *pspec);

static void nwamui_enm_finalize (     NwamuiEnm *self);


/* Callbacks */
static void object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);

G_DEFINE_TYPE (NwamuiEnm, nwamui_enm, G_TYPE_OBJECT)

static void
nwamui_enm_class_init (NwamuiEnmClass *klass)
{
    /* Pointer to GObject Part of Class */
    GObjectClass *gobject_class = (GObjectClass*) klass;
        
    /* Initialise Static Parent Class pointer */
    parent_class = g_type_class_peek_parent (klass);

    /* Override Some Function Pointers */
    gobject_class->set_property = nwamui_enm_set_property;
    gobject_class->get_property = nwamui_enm_get_property;
    gobject_class->finalize = (void (*)(GObject*)) nwamui_enm_finalize;

    /* Create some properties */
    g_object_class_install_property (gobject_class,
                                     PROP_NAME,
                                     g_param_spec_string ("name",
                                                          _("name"),
                                                          _("name"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_ACTIVE,
                                     g_param_spec_boolean ("active",
                                                          _("active"),
                                                          _("active"),
                                                          FALSE,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_START_COMMAND,
                                     g_param_spec_string ("start_command",
                                                          _("start_command"),
                                                          _("start_command"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_STOP_COMMAND,
                                     g_param_spec_string ("stop_command",
                                                          _("stop_command"),
                                                          _("stop_command"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_SMF_FRMI,
                                     g_param_spec_string ("smf_frmi",
                                                          _("smf_frmi"),
                                                          _("smf_frmi"),
                                                          "",
                                                          G_PARAM_READWRITE));

}


typedef struct _ncu_info    ncu_info_t;

static void
nwamui_enm_init ( NwamuiEnm *self)
{  
    self->prv = g_new0 (NwamuiEnmPrivate, 1);
    
    self->prv->name = NULL;
    self->prv->active = FALSE;
    self->prv->start_command = NULL;
    self->prv->stop_command = NULL;
    self->prv->smf_frmi = NULL;

    g_signal_connect(G_OBJECT(self), "notify", (GCallback)object_notify_cb, (gpointer)self);
}

static void
nwamui_enm_set_property (   GObject         *object,
                            guint            prop_id,
                            const GValue    *value,
                            GParamSpec      *pspec)
{
    NwamuiEnm *self = NWAMUI_ENM(object);
    gchar*      tmpstr = NULL;
    gint        tmpint = 0;

    switch (prop_id) {
       case PROP_NAME: {
                if ( self->prv->name != NULL ) {
                        g_free( self->prv->name );
                }
                self->prv->name = g_strdup( g_value_get_string( value ) );
            }
            break;

        case PROP_ACTIVE: {
                self->prv->active = g_value_get_boolean( value );
            }
            break;

        case PROP_START_COMMAND: {
                if ( self->prv->start_command != NULL ) {
                        g_free( self->prv->start_command );
                }
                self->prv->start_command = g_strdup( g_value_get_string( value ) );
            }
            break;

        case PROP_STOP_COMMAND: {
                if ( self->prv->stop_command != NULL ) {
                        g_free( self->prv->stop_command );
                }
                self->prv->stop_command = g_strdup( g_value_get_string( value ) );
            }
            break;

        case PROP_SMF_FRMI: {
                if ( self->prv->smf_frmi != NULL ) {
                        g_free( self->prv->smf_frmi );
                }
                self->prv->smf_frmi = g_strdup( g_value_get_string( value ) );
            }
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
nwamui_enm_get_property (   GObject         *object,
                            guint            prop_id,
                            GValue          *value,
                            GParamSpec      *pspec)
{
    NwamuiEnm *self = NWAMUI_ENM(object);

    switch (prop_id) {
        case PROP_NAME: {
                g_value_set_string( value, self->prv->name );
            }
            break;

        case PROP_ACTIVE: {
                g_value_set_boolean( value, self->prv->active );
            }
            break;

        case PROP_START_COMMAND: {
                g_value_set_string( value, self->prv->start_command );
            }
            break;

        case PROP_STOP_COMMAND: {
                g_value_set_string( value, self->prv->stop_command );
            }
            break;

        case PROP_SMF_FRMI: {
                g_value_set_string( value, self->prv->smf_frmi );
            }
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}


/**
 * nwamui_enm_new:
 * @returns: a new #NwamuiEnm.
 *
 **/
extern  NwamuiEnm*          
nwamui_enm_new (    const gchar*    name, 
                    gboolean        active, 
                    const gchar*    smf_frmi,
                    const gchar*    start_command,
                    const gchar*    stop_command )
{
    NwamuiEnm *self = NWAMUI_ENM(g_object_new (NWAMUI_TYPE_ENM, NULL));

    g_object_set (G_OBJECT (self),
                  "name", name,
                  "active", active,
                  "smf_frmi", smf_frmi,
                  "start_command", start_command,
                  "stop_command", stop_command,
                  NULL);
    
    return( self );
}


/** 
 * nwamui_enm_set_name:
 * @nwamui_enm: a #NwamuiEnm.
 * @name: Value to set name to.
 * 
 **/ 
extern void
nwamui_enm_set_name (   NwamuiEnm *self,
                              const gchar*  name )
{
    g_return_if_fail (NWAMUI_IS_ENM (self));
    g_assert (name != NULL );

    if ( name != NULL ) {
        g_object_set (G_OBJECT (self),
                      "name", name,
                      NULL);
    }
}

/**
 * nwamui_enm_get_name:
 * @nwamui_enm: a #NwamuiEnm.
 * @returns: the name.
 *
 **/
extern gchar*
nwamui_enm_get_name (NwamuiEnm *self)
{
    gchar*  name = NULL; 

    g_return_val_if_fail (NWAMUI_IS_ENM (self), name);

    g_object_get (G_OBJECT (self),
                  "name", &name,
                  NULL);

    return( name );
}

/** 
 * nwamui_enm_set_active:
 * @nwamui_enm: a #NwamuiEnm.
 * @active: Value to set active to.
 * 
 **/ 
extern void
nwamui_enm_set_active (   NwamuiEnm *self,
                              gboolean        active )
{
    g_return_if_fail (NWAMUI_IS_ENM (self));

    g_object_set (G_OBJECT (self),
                  "active", active,
                  NULL);
}

/**
 * nwamui_enm_get_active:
 * @nwamui_enm: a #NwamuiEnm.
 * @returns: the active.
 *
 **/
extern gboolean
nwamui_enm_get_active (NwamuiEnm *self)
{
    gboolean  active = FALSE; 

    g_return_val_if_fail (NWAMUI_IS_ENM (self), active);

    g_object_get (G_OBJECT (self),
                  "active", &active,
                  NULL);

    return( active );
}

/** 
 * nwamui_enm_set_start_command:
 * @nwamui_enm: a #NwamuiEnm.
 * @start_command: Value to set start_command to.
 * 
 **/ 
extern void
nwamui_enm_set_start_command (   NwamuiEnm *self,
                              const gchar*  start_command )
{
    g_return_if_fail (NWAMUI_IS_ENM (self));
    g_assert (start_command != NULL );

    if ( start_command != NULL ) {
        g_object_set (G_OBJECT (self),
                      "start_command", start_command,
                      NULL);
    }
}

/**
 * nwamui_enm_get_start_command:
 * @nwamui_enm: a #NwamuiEnm.
 * @returns: the start_command.
 *
 **/
extern gchar*
nwamui_enm_get_start_command (NwamuiEnm *self)
{
    gchar*  start_command = NULL; 

    g_return_val_if_fail (NWAMUI_IS_ENM (self), start_command);

    g_object_get (G_OBJECT (self),
                  "start_command", &start_command,
                  NULL);

    return( start_command );
}

/** 
 * nwamui_enm_set_stop_command:
 * @nwamui_enm: a #NwamuiEnm.
 * @stop_command: Value to set stop_command to.
 * 
 **/ 
extern void
nwamui_enm_set_stop_command (   NwamuiEnm *self,
                              const gchar*  stop_command )
{
    g_return_if_fail (NWAMUI_IS_ENM (self));
    g_assert (stop_command != NULL );

    if ( stop_command != NULL ) {
        g_object_set (G_OBJECT (self),
                      "stop_command", stop_command,
                      NULL);
    }
}

/**
 * nwamui_enm_get_stop_command:
 * @nwamui_enm: a #NwamuiEnm.
 * @returns: the stop_command.
 *
 **/
extern gchar*
nwamui_enm_get_stop_command (NwamuiEnm *self)
{
    gchar*  stop_command = NULL; 

    g_return_val_if_fail (NWAMUI_IS_ENM (self), stop_command);

    g_object_get (G_OBJECT (self),
                  "stop_command", &stop_command,
                  NULL);

    return( stop_command );
}

/** 
 * nwamui_enm_set_smf_frmi:
 * @nwamui_enm: a #NwamuiEnm.
 * @smf_frmi: Value to set smf_frmi to.
 * 
 **/ 
extern void
nwamui_enm_set_smf_frmi (   NwamuiEnm *self,
                              const gchar*  smf_frmi )
{
    g_return_if_fail (NWAMUI_IS_ENM (self));
    g_assert (smf_frmi != NULL );

    if ( smf_frmi != NULL ) {
        g_object_set (G_OBJECT (self),
                      "smf_frmi", smf_frmi,
                      NULL);
    }
}

/**
 * nwamui_enm_get_smf_frmi:
 * @nwamui_enm: a #NwamuiEnm.
 * @returns: the smf_frmi.
 *
 **/
extern gchar*
nwamui_enm_get_smf_frmi (NwamuiEnm *self)
{
    gchar*  smf_frmi = NULL; 

    g_return_val_if_fail (NWAMUI_IS_ENM (self), smf_frmi);

    g_object_get (G_OBJECT (self),
                  "smf_frmi", &smf_frmi,
                  NULL);

    return( smf_frmi );
}

static void
nwamui_enm_finalize (NwamuiEnm *self)
{

    if (self->prv->name != NULL ) {
        g_free( self->prv->name );
    }

    if (self->prv->start_command != NULL ) {
        g_free( self->prv->start_command );
    }

    if (self->prv->stop_command != NULL ) {
        g_free( self->prv->stop_command );
    }

    if (self->prv->smf_frmi != NULL ) {
        g_free( self->prv->smf_frmi );
    }

    g_free (self->prv); 
    self->prv = NULL;

    parent_class->finalize (G_OBJECT (self));
}

/* Callbacks */

static void
object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
    NwamuiEnm* self = NWAMUI_ENM(data);

    g_debug("NwamuiEnm: notify %s changed\n", arg1->name);
}

