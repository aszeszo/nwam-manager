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
 * File:   nwamui_enm.c
 *
 */

#include <glib-object.h>
#include <glib/gi18n.h>
#include <strings.h>
#include <stdlib.h>

#include <libnwam.h>
#include "libnwamui.h"

static GObjectClass    *parent_class    = NULL;

struct _NwamuiEnmPrivate {
    gchar*               name;

    nwam_enm_handle_t	nwam_enm;
    gboolean        	nwam_enm_modified;
};

enum {
    PROP_NAME = 1,
    PROP_ACTIVE,
    PROP_ENABLED,
    PROP_START_COMMAND,
    PROP_STOP_COMMAND,
    PROP_SMF_FMRI,
    PROP_NWAM_ENM,
    PROP_ACTIVATION_MODE,
    PROP_CONDITIONS,
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

static gboolean get_nwam_enm_boolean_prop( nwam_enm_handle_t enm, const char* prop_name );

static gchar* get_nwam_enm_string_prop( nwam_enm_handle_t enm, const char* prop_name );

static gchar*       get_nwam_enm_string_prop( nwam_enm_handle_t enm, const char* prop_name );
static gboolean     set_nwam_enm_string_prop( nwam_enm_handle_t enm, const char* prop_name, const char* str );

static gchar**      get_nwam_enm_string_array_prop( nwam_enm_handle_t enm, const char* prop_name );
static gboolean     set_nwam_enm_string_array_prop( nwam_enm_handle_t enm, const char* prop_name, char** strs, guint len);

static guint64      get_nwam_enm_uint64_prop( nwam_enm_handle_t enm, const char* prop_name );
static gboolean     set_nwam_enm_uint64_prop( nwam_enm_handle_t enm, const char* prop_name, guint64 value );

static nwam_state_t nwamui_enm_get_nwam_state(NwamuiObject *object, nwam_aux_state_t* aux_state_p, const gchar**aux_state_string_p );
static gboolean     nwamui_enm_destroy( NwamuiEnm* self );

/* Callbacks */
static void object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);

G_DEFINE_TYPE (NwamuiEnm, nwamui_enm, NWAMUI_TYPE_OBJECT)

static void
nwamui_enm_class_init (NwamuiEnmClass *klass)
{
    /* Pointer to GObject Part of Class */
    GObjectClass *gobject_class = (GObjectClass*) klass;
    NwamuiObjectClass *nwamuiobject_class = NWAMUI_OBJECT_CLASS(klass);
        
    /* Initialise Static Parent Class pointer */
    parent_class = g_type_class_peek_parent (klass);

    /* Override Some Function Pointers */
    gobject_class->set_property = nwamui_enm_set_property;
    gobject_class->get_property = nwamui_enm_get_property;
    gobject_class->finalize = (void (*)(GObject*)) nwamui_enm_finalize;

    nwamuiobject_class->get_name = (nwamui_object_get_name_func_t)nwamui_enm_get_name;
    nwamuiobject_class->set_name = (nwamui_object_set_name_func_t)nwamui_enm_set_name;
    nwamuiobject_class->get_conditions = (nwamui_object_get_conditions_func_t)nwamui_enm_get_selection_conditions;
    nwamuiobject_class->set_conditions = (nwamui_object_set_conditions_func_t)nwamui_enm_set_selection_conditions;
    nwamuiobject_class->get_activation_mode = (nwamui_object_get_activation_mode_func_t)nwamui_enm_get_activation_mode;
    nwamuiobject_class->set_activation_mode = (nwamui_object_set_activation_mode_func_t)nwamui_enm_set_activation_mode;
    nwamuiobject_class->get_active = (nwamui_object_get_active_func_t)nwamui_enm_get_active;
    nwamuiobject_class->set_active = (nwamui_object_set_active_func_t)nwamui_enm_set_active;
    nwamuiobject_class->get_nwam_state = (nwamui_object_get_nwam_state_func_t)nwamui_enm_get_nwam_state;
    nwamuiobject_class->commit = (nwamui_object_commit_func_t)nwamui_enm_commit;
    nwamuiobject_class->reload = (nwamui_object_reload_func_t)nwamui_enm_reload;
    nwamuiobject_class->destroy = (nwamui_object_destroy_func_t)nwamui_enm_destroy;

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
                                     PROP_ENABLED,
                                     g_param_spec_boolean ("enabled",
                                                          _("enabled"),
                                                          _("enabled"),
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
                                     PROP_SMF_FMRI,
                                     g_param_spec_string ("smf_fmri",
                                                          _("smf_fmri"),
                                                          _("smf_fmri"),
                                                          "",
                                                          G_PARAM_READWRITE));


    g_object_class_install_property (gobject_class,
      PROP_NWAM_ENM,
      g_param_spec_pointer ("nwam_enm",
        _("Nwam Enm handle"),
        _("Nwam Enm handle"),
        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

    g_object_class_install_property (gobject_class,
                                     PROP_ACTIVATION_MODE,
                                     g_param_spec_int ("activation_mode",
                                                       _("activation_mode"),
                                                       _("activation_mode"),
                                                       NWAMUI_COND_ACTIVATION_MODE_MANUAL,
                                                       NWAMUI_COND_ACTIVATION_MODE_LAST,
                                                       NWAMUI_COND_ACTIVATION_MODE_MANUAL,
                                                       G_PARAM_READWRITE));


    g_object_class_install_property (gobject_class,
                                     PROP_CONDITIONS,
                                     g_param_spec_pointer ("conditions",
                                                          _("conditions"),
                                                          _("conditions"),
                                                          G_PARAM_READWRITE));
}


typedef struct _ncu_info    ncu_info_t;

static void
nwamui_enm_init ( NwamuiEnm *self)
{  
    self->prv = g_new0 (NwamuiEnmPrivate, 1);
    
    self->prv->name = NULL;

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
    nwam_error_t nerr;
    int num;
    nwam_value_t nwamdata;

    switch (prop_id) {
       case PROP_NAME: {
                if ( self->prv->name != NULL ) {
                    g_free( self->prv->name );
                }
                self->prv->name = g_strdup( g_value_get_string( value ) );
                /* we may rename here */
                if (self->prv->nwam_enm == NULL) {
                    nerr = nwam_enm_read (self->prv->name, 0, &self->prv->nwam_enm);
                    if (nerr == NWAM_SUCCESS) {
                        g_debug ("nwamui_enm_set_property found nwam_enm_handle %s", self->prv->name);
                    } else {
                        nerr = nwam_enm_create (self->prv->name, NULL, &self->prv->nwam_enm);
                        if (nerr != NWAM_SUCCESS) {
                            g_debug ("nwamui_enm_set_property error creating nwam_enm_handle %s", self->prv->name);
                            self->prv->nwam_enm == NULL;
                        }
                    }
                }

                if (self->prv->nwam_enm != NULL) {
                    nerr = nwam_enm_set_name (self->prv->nwam_enm, self->prv->name);
                    if (nerr != NWAM_SUCCESS) {
                        g_debug ("nwam_enm_set_name %s error: %s", self->prv->name, nwam_strerror (nerr));
                    }
                }
                else {
                    g_warning("Unexpected null enm handle");
                }
            }
            break;

        case PROP_ACTIVE: {
                gboolean active = g_value_get_boolean( value );
                nwamui_enm_set_active( self, active );
            }
            break;

        case PROP_ENABLED: {
                gboolean enabled = g_value_get_boolean( value );
                nwamui_enm_set_enabled( self, enabled );
            }
            break;

        case PROP_START_COMMAND: {
                const gchar *start_command = g_value_get_string( value );
                nwamui_enm_set_start_command ( self, start_command );
            }
            break;

        case PROP_STOP_COMMAND: {
                const gchar *stop_command = g_value_get_string( value );
                nwamui_enm_set_stop_command ( self, stop_command );
            }
            break;

        case PROP_SMF_FMRI: {
                const gchar *smf_fmri = g_value_get_string( value );
                nwamui_enm_set_smf_fmri ( self, smf_fmri );
            }
            break;

        case PROP_NWAM_ENM: {
                self->prv->nwam_enm = g_value_get_pointer (value);
            }
            break;

        case PROP_ACTIVATION_MODE: {
                if (self->prv->nwam_enm != NULL) {
                    nwamui_cond_activation_mode_t activation_mode = 
                        (nwamui_cond_activation_mode_t)g_value_get_int( value );

                    set_nwam_enm_uint64_prop( self->prv->nwam_enm, NWAM_ENM_PROP_ACTIVATION_MODE, (guint64)activation_mode );
                }
                else {
                    g_warning("Unexpected null enm handle");
                }
            }
            break;

        case PROP_CONDITIONS: {
                if (self->prv->nwam_enm != NULL) {
                    GList *conditions = g_value_get_pointer( value );
                    char  **condition_strs = NULL;
                    guint   len = 0;

                    condition_strs = nwamui_util_map_object_list_to_condition_strings( conditions, &len);
                    if ( condition_strs == NULL ) {
                        /* We need to set activation mode to MANUAL or it will
                         * not allow deletion of conditions
                         */
                        nwamui_enm_set_enabled ( self, FALSE );
                        nwamui_enm_set_activation_mode ( self, NWAMUI_COND_ACTIVATION_MODE_MANUAL );
                    }
                    set_nwam_enm_string_array_prop( self->prv->nwam_enm, NWAM_ENM_PROP_CONDITIONS, condition_strs, len );
                }
                else {
                    g_warning("Unexpected null enm handle");
                }
            }
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
    self->prv->nwam_enm_modified = TRUE;
}

static void
nwamui_enm_get_property (   GObject         *object,
                            guint            prop_id,
                            GValue          *value,
                            GParamSpec      *pspec)
{
    NwamuiEnm *self = NWAMUI_ENM(object);
    nwam_error_t nerr;
    int num;
    nwam_value_t nwamdata;

    switch (prop_id) {
        case PROP_NAME: {
            if (self->prv->name == NULL) {
                char *name;

                if (self->prv->nwam_enm != NULL) {
                    if ( (nerr = nwam_enm_get_name (self->prv->nwam_enm, &name)) != NWAM_SUCCESS ) {
                        g_debug ("Failed to get name for enm, error: %s", nwam_strerror (nerr));
                    }

                    if (g_ascii_strcasecmp (self->prv->name, name) != 0) {
                        g_free(self->prv->name);
                        self->prv->name = g_strdup(name);
                        g_object_notify(G_OBJECT(self), "name");
                    }
                    free (name);
                }
                else {
                    g_warning("Unexpected null enm handle");
                }
            }
            g_value_set_string( value, self->prv->name );
        }
        break;

        case PROP_ACTIVE: {
                gboolean active = FALSE;
                if ( self->prv->nwam_enm ) {
                    nwam_state_t        state = NWAM_STATE_OFFLINE;
                    nwam_aux_state_t    aux_state = NWAM_AUX_STATE_UNINITIALIZED;

                    /* Use cached state in nwamui_object... */
                    state = nwamui_object_get_nwam_state( NWAMUI_OBJECT(self), &aux_state, NULL );

                    if ( state == NWAM_STATE_ONLINE && aux_state == NWAM_AUX_STATE_ACTIVE ) {
                        active = TRUE;
                    }
                }
                g_value_set_boolean( value, active );
        }
        break;

        case PROP_ENABLED: {
                gboolean enabled = FALSE;
                if (self->prv->nwam_enm != NULL) {
                    enabled = get_nwam_enm_boolean_prop( self->prv->nwam_enm, NWAM_ENM_PROP_ENABLED ); 
                }
                else {
                    g_warning("Unexpected null enm handle");
                }
                g_value_set_boolean( value, enabled );
        }
        break;

        case PROP_START_COMMAND: {
                gchar* start_command = NULL;
                if (self->prv->nwam_enm != NULL) {
                    start_command = get_nwam_enm_string_prop( self->prv->nwam_enm, NWAM_ENM_PROP_START );
                }
                else {
                    g_warning("Unexpected null enm handle");
                }
                g_value_set_string( value, start_command );
        }
        break;

        case PROP_STOP_COMMAND: {
                gchar* stop_command = NULL;
                if (self->prv->nwam_enm != NULL) {
                    stop_command = get_nwam_enm_string_prop( self->prv->nwam_enm, NWAM_ENM_PROP_STOP );
                }
                else {
                    g_warning("Unexpected null enm handle");
                }
                g_value_set_string( value, stop_command );
        }
        break;

        case PROP_SMF_FMRI: {
                gchar* smf_fmri = NULL;
                if (self->prv->nwam_enm != NULL) {
                    smf_fmri = get_nwam_enm_string_prop( self->prv->nwam_enm, NWAM_ENM_PROP_FMRI );
                }
                else {
                    g_warning("Unexpected null enm handle");
                }
                g_value_set_string( value, smf_fmri );
        }
        break;

        case PROP_ACTIVATION_MODE: {
                nwamui_cond_activation_mode_t activation_mode = NWAMUI_COND_ACTIVATION_MODE_MANUAL;
                if (self->prv->nwam_enm != NULL) {
                    activation_mode = (nwamui_cond_activation_mode_t)
                                        get_nwam_enm_uint64_prop( self->prv->nwam_enm, NWAM_ENM_PROP_ACTIVATION_MODE );
                }
                else {
                    g_warning("Unexpected null enm handle");
                }
                g_value_set_int( value, (gint)activation_mode );

            }
            break;

        case PROP_CONDITIONS: {
                gpointer    ptr = NULL;

                if (self->prv->nwam_enm != NULL) {
                    gchar** condition_strs = get_nwam_enm_string_array_prop( self->prv->nwam_enm, 
                                                                             NWAM_ENM_PROP_CONDITIONS );

                    GList *conditions = nwamui_util_map_condition_strings_to_object_list( condition_strs );

                    ptr = (gpointer)conditions;

                    g_strfreev( condition_strs );
                }
                else {
                    g_warning("Unexpected null enm handle");
                }
                g_value_set_pointer( value, ptr );
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
nwamui_enm_new (    const gchar*    name )
{
    NwamuiEnm *self = NWAMUI_ENM(g_object_new (NWAMUI_TYPE_ENM, 
                "name", name,
                NULL));

    g_object_set (G_OBJECT (self),
                  "enabled", FALSE,
                  "activation_mode", NWAMUI_COND_ACTIVATION_MODE_MANUAL,
                  NULL);
    
    self->prv->nwam_enm_modified = FALSE;

    return( self );
}


static gboolean
get_nwam_enm_boolean_prop( nwam_enm_handle_t enm, const char* prop_name )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    boolean_t           value = FALSE;

    g_return_val_if_fail( prop_name != NULL, value );

    if ( (nerr = nwam_enm_get_prop_type( prop_name, &nwam_type ) ) != NWAM_SUCCESS 
         || nwam_type != NWAM_VALUE_TYPE_BOOLEAN ) {
        g_warning("Unexpected type for enm property %s - got %d\n", prop_name, nwam_type );
        return value;
    }

    if ( (nerr = nwam_enm_get_prop_value (enm, prop_name, &nwam_data)) != NWAM_SUCCESS ) {
        g_debug("No value for enm property %s, error = %s", prop_name, nwam_strerror( nerr ) );
        return value;
    }

    if ( (nerr = nwam_value_get_boolean(nwam_data, &value )) != NWAM_SUCCESS ) {
        g_debug("Unable to get boolean value for enm property %s, error = %s", prop_name, nwam_strerror( nerr ) );
        return value;
    }

    nwam_value_free(nwam_data);

    return( (gboolean)value );
}

static gboolean
set_nwam_enm_boolean_prop( nwam_enm_handle_t enm, const char* prop_name, gboolean bool_value )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    gboolean            retval = FALSE;
    g_return_val_if_fail( prop_name != NULL, retval );

    if ( enm == NULL ) {
        return( retval );
    }

    if ( (nerr = nwam_enm_get_prop_type( prop_name, &nwam_type ) ) != NWAM_SUCCESS 
         || nwam_type != NWAM_VALUE_TYPE_BOOLEAN ) {
        g_warning("Unexpected type for enm property %s - got %d\n", prop_name, nwam_type );
        return retval;
    }

    if ( (nerr = nwam_value_create_boolean ((boolean_t)bool_value, &nwam_data )) != NWAM_SUCCESS ) {
        g_debug("Error creating a boolean value" );
        return retval;
    }

    if ( (nerr = nwam_enm_set_prop_value (enm, prop_name, nwam_data)) != NWAM_SUCCESS ) {
        g_debug("Unable to set value for enm property %s, error = %s", prop_name, nwam_strerror( nerr ) );
    }
    else {
        retval = TRUE;
    }

    nwam_value_free(nwam_data);
    
    return( retval );
}


static gchar*
get_nwam_enm_string_prop( nwam_enm_handle_t enm, const char* prop_name )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    gchar*              retval = NULL;
    char*               value = NULL;

    g_return_val_if_fail( prop_name != NULL, retval );

    if ( enm == NULL ) {
        return( retval );
    }

    if ( (nerr = nwam_enm_get_prop_type( prop_name, &nwam_type ) ) != NWAM_SUCCESS 
         || nwam_type != NWAM_VALUE_TYPE_STRING ) {
        g_warning("Unexpected type for enm property %s - got %d\n", prop_name, nwam_type );
        return retval;
    }

    if ( (nerr = nwam_enm_get_prop_value (enm, prop_name, &nwam_data)) != NWAM_SUCCESS ) {
        g_debug("No value for enm property %s, error = %s", prop_name, nwam_strerror( nerr ) );
        return retval;
    }

    if ( (nerr = nwam_value_get_string(nwam_data, &value )) != NWAM_SUCCESS ) {
        g_debug("Unable to get string value for enm property %s, error = %s", prop_name, nwam_strerror( nerr ) );
        return retval;
    }

    if ( value != NULL ) {
        retval  = g_strdup ( value );
    }

    nwam_value_free(nwam_data);
    
    return( retval );
}

static gboolean
set_nwam_enm_string_prop( nwam_enm_handle_t enm, const char* prop_name, const gchar* str )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    gboolean            retval = FALSE;
    g_return_val_if_fail( prop_name != NULL, retval );

    if ( enm == NULL ) {
        return( retval );
    }

    if ( (nerr = nwam_enm_get_prop_type( prop_name, &nwam_type ) ) != NWAM_SUCCESS 
         || nwam_type != NWAM_VALUE_TYPE_STRING ) {
        g_warning("Unexpected type for enm property %s - got %d\n", prop_name, nwam_type );
        return retval;
    }

    if ( (nerr = nwam_value_create_string( (char*)str, &nwam_data )) != NWAM_SUCCESS ) {
        g_debug("Error creating a string value for string %s", str?str:"NULL");
        return retval;
    }

    if ( (nerr = nwam_enm_set_prop_value (enm, prop_name, nwam_data)) != NWAM_SUCCESS ) {
        g_debug("Unable to set value for enm property %s, error = %s", prop_name, nwam_strerror( nerr ) );
    }
    else {
        retval = TRUE;
    }

    nwam_value_free(nwam_data);
    
    return( retval );
}

static gchar**
get_nwam_enm_string_array_prop( nwam_enm_handle_t enm, const char* prop_name )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    gchar**             retval = NULL;
    char**              value = NULL;
    uint_t              num = 0;

    g_return_val_if_fail( prop_name != NULL, retval );

    if ( enm == NULL ) {
        return( retval );
    }

    if ( (nerr = nwam_enm_get_prop_type( prop_name, &nwam_type ) ) != NWAM_SUCCESS 
         || nwam_type != NWAM_VALUE_TYPE_STRING ) {
        g_warning("Unexpected type for enm property %s - got %d\n", prop_name, nwam_type );
        return retval;
    }

    if ( (nerr = nwam_enm_get_prop_value (enm, prop_name, &nwam_data)) != NWAM_SUCCESS ) {
        g_debug("No value for enm property %s, error = %s", prop_name, nwam_strerror( nerr ) );
        return retval;
    }

    if ( (nerr = nwam_value_get_string_array(nwam_data, &value, &num )) != NWAM_SUCCESS ) {
        g_debug("Unable to get string value for enm property %s, error = %s", prop_name, nwam_strerror( nerr ) );
        return retval;
    }

    if ( value != NULL && num > 0 ) {
        /* Create a NULL terminated list of stirngs, allocate 1 extra place
         * for NULL termination. */
        retval = (gchar**)g_malloc0( sizeof(gchar*) * (num+1) );

        for (int i = 0; i < num; i++ ) {
            retval[i]  = g_strdup ( value[i] );
        }
        retval[num]=NULL;
    }

    nwam_value_free(nwam_data);
    
    return( retval );
}

static gboolean
set_nwam_enm_string_array_prop( nwam_enm_handle_t enm, const char* prop_name, char** strs, guint len )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    gboolean            retval = FALSE;

    g_return_val_if_fail( prop_name != NULL, retval );

    if ( enm == NULL ) {
        return( retval );
    }

    if ( (nerr = nwam_enm_get_prop_type( prop_name, &nwam_type ) ) != NWAM_SUCCESS 
         || nwam_type != NWAM_VALUE_TYPE_STRING ) {
        g_warning("Unexpected type for enm property %s - got %d\n", prop_name, nwam_type );
        return retval;
    }

    if ( strs == NULL ) {
        nwam_error_t nerr;

        /* Delete property, don't set to empty list */
        if ( (nerr = nwam_enm_delete_prop( enm, prop_name ))  != NWAM_SUCCESS ) {
            retval = FALSE;
        }
        else {
            retval = TRUE;
        }
        return retval;
    }


    if ( len == 0 ) { /* Assume a strv, i.e. NULL terminated list, otherwise strs would be NULL */
        int i;

        for( i = 0; strs != NULL && strs[i] != NULL; i++ ) {
            /* Do Nothing, just count. */
        }
        len = i;
    }

    if ( (nerr = nwam_value_create_string_array (strs, len, &nwam_data )) != NWAM_SUCCESS ) {
        g_debug("Error creating a value for string array 0x%08X", strs);
        return retval;
    }

    if ( (nerr = nwam_enm_set_prop_value (enm, prop_name, nwam_data)) != NWAM_SUCCESS ) {
        g_debug("Unable to set value for enm property %s, error = %s", prop_name, nwam_strerror( nerr ) );
    }
    else {
        retval = TRUE;
    }

    nwam_value_free(nwam_data);
    
    return( retval );
}

static guint64
get_nwam_enm_uint64_prop( nwam_enm_handle_t enm, const char* prop_name )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    uint64_t            value = 0;

    g_return_val_if_fail( prop_name != NULL, value );

    if ( enm == NULL ) {
        return( value );
    }

    if ( (nerr = nwam_enm_get_prop_type( prop_name, &nwam_type ) ) != NWAM_SUCCESS 
         || nwam_type != NWAM_VALUE_TYPE_UINT64 ) {
        g_warning("Unexpected type for enm property %s - got %d\n", prop_name, nwam_type );
        return value;
    }

    if ( (nerr = nwam_enm_get_prop_value (enm, prop_name, &nwam_data)) != NWAM_SUCCESS ) {
        g_debug("No value for enm property %s, error = %s", prop_name, nwam_strerror( nerr ) );
        return value;
    }

    if ( (nerr = nwam_value_get_uint64(nwam_data, &value )) != NWAM_SUCCESS ) {
        g_debug("Unable to get uint64 value for enm property %s, error = %s", prop_name, nwam_strerror( nerr ) );
        return value;
    }

    nwam_value_free(nwam_data);

    return( (guint64)value );
}

static gboolean
set_nwam_enm_uint64_prop( nwam_enm_handle_t enm, const char* prop_name, guint64 value )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    gboolean            retval = FALSE;
    g_return_val_if_fail( prop_name != NULL, retval );

    if ( enm == NULL ) {
        return( retval );
    }

    if ( (nerr = nwam_enm_get_prop_type( prop_name, &nwam_type ) ) != NWAM_SUCCESS 
         || nwam_type != NWAM_VALUE_TYPE_UINT64 ) {
        g_warning("Unexpected type for enm property %s - got %d\n", prop_name, nwam_type );
        return retval;
    }

    if ( (nerr = nwam_value_create_uint64 (value, &nwam_data )) != NWAM_SUCCESS ) {
        g_debug("Error creating a uint64 value" );
        return retval;
    }

    if ( (nerr = nwam_enm_set_prop_value (enm, prop_name, nwam_data)) != NWAM_SUCCESS ) {
        g_debug("Unable to set value for enm property %s, error = %s", prop_name, nwam_strerror( nerr ) );
    }
    else {
        retval = TRUE;
    }

    nwam_value_free(nwam_data);
    
    return( retval );
}


/**
 * nwamui_enm_update_with_handle:
 * @returns: TRUE if successfully updated
 *
 **/
extern gboolean
nwamui_enm_update_with_handle (NwamuiEnm* self, nwam_enm_handle_t enm)
{
    char*           name = NULL;
    nwam_error_t    nerr;
    nwam_enm_handle_t nwam_enm;
    
    if ( (nerr = nwam_enm_get_name (enm, &name)) != NWAM_SUCCESS ) {
        g_debug ("Failed to get name for enm, error: %s", nwam_strerror (nerr));
        return( FALSE );
    }

    if ( ( nerr = nwam_enm_read (name, 0, &nwam_enm) ) != NWAM_SUCCESS ) {
        g_debug ("Failed to create private handle for enm, error: %s", nwam_strerror (nerr));
        return( FALSE );
    }

    if ( self->prv->nwam_enm != NULL ) {
         nwam_enm_free(self->prv->nwam_enm);
    }
    self->prv->nwam_enm = nwam_enm;

    if ( self->prv->name != NULL ) {
         g_free(self->prv->name);
    }
    self->prv->name = name;

    self->prv->nwam_enm_modified = FALSE;

    return( TRUE );
}

/**
 * nwamui_enm_new_with_handle:
 * @returns: a new #NwamuiEnm.
 *
 **/
extern  NwamuiEnm*          
nwamui_enm_new_with_handle (nwam_enm_handle_t enm)
{
    NwamuiEnm*      self = NWAMUI_ENM(g_object_new (NWAMUI_TYPE_ENM,
                                   NULL));
    
    if ( ! nwamui_enm_update_with_handle (self, enm) ) {
        g_object_unref( self );
        return( NULL );
    }
    
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
extern gboolean
nwamui_enm_set_active (   NwamuiEnm *self,
                          gboolean        active )
{
    nwam_error_t    nerr;
    g_return_val_if_fail (NWAMUI_IS_ENM (self), FALSE);

    if (self->prv->nwam_enm != NULL) {
        if (active) {
            if ((nerr = nwam_enm_enable (self->prv->nwam_enm)) != NWAM_SUCCESS) {
                g_debug ("nwam_enm_start error: %s", nwam_strerror (nerr));
                return( FALSE );
            }
        } else {
            if ((nerr = nwam_enm_disable (self->prv->nwam_enm)) != NWAM_SUCCESS) {
                g_debug ("nwam_enm_stop error: %s", nwam_strerror (nerr));
                return( FALSE );
            }
        }
        self->prv->nwam_enm_modified = TRUE;
        g_object_notify(G_OBJECT(self), "active");
    }
    else {
        g_warning("Unexpected null enm handle");
    }
    return( FALSE );
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
 * nwamui_enm_set_enabled:
 * @nwamui_enm: a #NwamuiEnm.
 * @enabled: Value to set enabled to.
 * 
 **/ 
extern gboolean
nwamui_enm_set_enabled (   NwamuiEnm *self,
                           gboolean        enabled )
{
    nwam_error_t    nerr;
    g_return_val_if_fail (NWAMUI_IS_ENM (self), FALSE);

    if (self->prv->nwam_enm != NULL) {
        if ( !set_nwam_enm_boolean_prop( self->prv->nwam_enm, NWAM_ENM_PROP_ENABLED, enabled ) ) {
            g_debug("Error setting ENM boolean prop ENABLED");
            return( FALSE) ;
        }
        g_object_notify (G_OBJECT (self), "enabled" );
        self->prv->nwam_enm_modified = TRUE;
        return(TRUE);
    }
    else {
        g_warning("Unexpected null enm handle");
    }
    return( FALSE );
}

/**
 * nwamui_enm_get_enabled:
 * @nwamui_enm: a #NwamuiEnm.
 * @returns: the enabled.
 *
 **/
extern gboolean
nwamui_enm_get_enabled (NwamuiEnm *self)
{
    gboolean  enabled = FALSE; 

    g_return_val_if_fail (NWAMUI_IS_ENM (self), enabled);

    g_object_get (G_OBJECT (self),
                  "enabled", &enabled,
                  NULL);

    return( enabled );
}

/** 
 * nwamui_enm_set_start_command:
 * @nwamui_enm: a #NwamuiEnm.
 * @start_command: Value to set start_command to.
 * 
 **/ 
extern gboolean
nwamui_enm_set_start_command (   NwamuiEnm *self,
                              const gchar*  start_command )
{
    nwam_error_t    nerr;
    g_return_val_if_fail (NWAMUI_IS_ENM (self), FALSE);

    if (self->prv->nwam_enm != NULL) {
        gboolean delete_prop = TRUE;

        if ( start_command != NULL && strlen( start_command ) > 0 ) {
            if ( !set_nwam_enm_string_prop( self->prv->nwam_enm, NWAM_ENM_PROP_START, start_command ) ) {
                return( FALSE );
            }
        }
        else {
            nwam_error_t nerr;

            /* Delete property, don't set to empty string */
            if ( (nerr = nwam_enm_delete_prop( self->prv->nwam_enm, NWAM_ENM_PROP_START ))  != NWAM_SUCCESS ) {
                return( FALSE );
            }
        }
        g_object_notify(G_OBJECT (self), "start_command");
        self->prv->nwam_enm_modified = TRUE;
    }
    else {
        g_warning("Unexpected null enm handle");
        return( FALSE );
    }

    return( TRUE );
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

extern gboolean
nwamui_enm_set_stop_command (   NwamuiEnm *self,
                              const gchar*  stop_command )
{
    nwam_error_t    nerr;
    g_return_val_if_fail (NWAMUI_IS_ENM (self), FALSE);

    if (self->prv->nwam_enm != NULL) {
        gboolean delete_prop = TRUE;

        if ( stop_command != NULL && strlen( stop_command ) > 0 ) {
            if ( !set_nwam_enm_string_prop( self->prv->nwam_enm, NWAM_ENM_PROP_STOP, stop_command ) ) {
                return( FALSE );
            }
        }
        else {
            nwam_error_t nerr;

            /* Delete property, don't set to empty string */
            if ( (nerr = nwam_enm_delete_prop( self->prv->nwam_enm, NWAM_ENM_PROP_STOP ))  != NWAM_SUCCESS ) {
                return( FALSE );
            }
        }
        g_object_notify(G_OBJECT (self), "stop_command");
        self->prv->nwam_enm_modified = TRUE;
    }
    else {
        g_warning("Unexpected null enm handle");
        return( FALSE );
    }

    return( TRUE );
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
 * nwamui_enm_set_smf_fmri:
 * @nwamui_enm: a #NwamuiEnm.
 * @smf_fmri: Value to set smf_fmri to.
 * 
 **/ 
extern gboolean
nwamui_enm_set_smf_fmri (   NwamuiEnm *self,
                              const gchar*  smf_fmri )
{
    nwam_error_t    nerr;
    g_return_val_if_fail (NWAMUI_IS_ENM (self), FALSE);

    if (self->prv->nwam_enm != NULL) {
        gboolean delete_prop = TRUE;

        if ( smf_fmri != NULL && strlen( smf_fmri ) > 0 ) {
            if ( !set_nwam_enm_string_prop( self->prv->nwam_enm, NWAM_ENM_PROP_FMRI, smf_fmri ) ) {
                return( FALSE );
            }
        }
        else {
            nwam_error_t nerr;

            /* Delete property, don't set to empty string */
            if ( (nerr = nwam_enm_delete_prop( self->prv->nwam_enm, NWAM_ENM_PROP_FMRI ))  != NWAM_SUCCESS ) {
                return( FALSE );
            }
        }
        g_object_notify(G_OBJECT (self), "smf_fmri");
        self->prv->nwam_enm_modified = TRUE;
    }
    else {
        g_warning("Unexpected null enm handle");
        return( FALSE );
    }

    return( TRUE );
}

/**
 * nwamui_enm_get_smf_fmri:
 * @nwamui_enm: a #NwamuiEnm.
 * @returns: the smf_fmri.
 *
 **/
extern gchar*
nwamui_enm_get_smf_fmri (NwamuiEnm *self)
{
    gchar*  smf_fmri = NULL; 

    g_return_val_if_fail (NWAMUI_IS_ENM (self), smf_fmri);

    g_object_get (G_OBJECT (self),
                  "smf_fmri", &smf_fmri,
                  NULL);

    return( smf_fmri );
}

extern void
nwamui_enm_set_activation_mode ( NwamuiEnm *self, 
                                 nwamui_cond_activation_mode_t activation_mode )
{
    g_return_if_fail (NWAMUI_IS_ENM (self));

    g_object_set (G_OBJECT (self),
                  "activation_mode", activation_mode,
                  NULL);
}

extern nwamui_cond_activation_mode_t 
nwamui_enm_get_activation_mode ( NwamuiEnm *self )
{
    nwamui_cond_activation_mode_t activation_mode = NWAMUI_COND_ACTIVATION_MODE_MANUAL;

    g_return_val_if_fail (NWAMUI_IS_ENM (self), activation_mode );

    g_object_get (G_OBJECT (self),
                  "activation_mode", &activation_mode,
                  NULL);

    return( activation_mode );
}


extern GList*
nwamui_enm_get_selection_conditions( NwamuiEnm* self )
{
    GList*  conditions = NULL;

    g_return_val_if_fail (NWAMUI_IS_ENM (self), conditions );

    g_object_get (G_OBJECT (self),
                  "conditions", &conditions,
                  NULL);

    return( conditions );
}


extern void
nwamui_enm_set_selection_conditions( NwamuiEnm* self, GList* conditions )
{
    g_return_if_fail (NWAMUI_IS_ENM (self));

    g_object_set (G_OBJECT (self),
                  "conditions", conditions,
                  NULL);
}

/**
 * nwamui_enm_reload:   re-load stored configuration
 **/
extern void
nwamui_enm_reload( NwamuiEnm* self )
{
    g_return_if_fail( NWAMUI_IS_ENM(self) );

    /* nwamui_enm_update_with_handle will cause re-read from configuration */
    g_object_freeze_notify(G_OBJECT(self));

    if ( self->prv->nwam_enm != NULL ) {
        nwamui_enm_update_with_handle( self, self->prv->nwam_enm );
    }

    g_object_thaw_notify(G_OBJECT(self));
}

/**
 * nwamui_enm_destroy:   destroy in-memory configuration, to persistant storage
 * @returns: TRUE if succeeded, FALSE if failed
 **/
static gboolean
nwamui_enm_destroy( NwamuiEnm* self )
{
    nwam_error_t    nerr;

    g_return_val_if_fail( NWAMUI_IS_ENM(self), FALSE );

    if ( self->prv->nwam_enm != NULL ) {

        if ( (nerr = nwam_enm_destroy( self->prv->nwam_enm, 0 ) ) != NWAM_SUCCESS ) {
            g_warning("Failed when destroying ENM for %s", self->prv->name);
            return( FALSE );
        }
        self->prv->nwam_enm = NULL;
    }

    return( TRUE );
}

/**
 * nwamui_enm_has_modifications:   test if there are un-saved changes
 * @returns: TRUE if unsaved changes exist.
 **/
extern gboolean
nwamui_enm_has_modifications( NwamuiEnm* self )
{
    if ( NWAMUI_IS_ENM(self) && self->prv->nwam_enm_modified ) {
        return( TRUE );
    }

    return( FALSE );
}

/**
 * nwamui_enm_validate:   validate in-memory configuration
 * @prop_name_ret:  If non-NULL, the name of the property that failed will be
 *                  returned, should be freed by caller.
 * @returns: TRUE if valid, FALSE if failed
 **/
extern gboolean
nwamui_enm_validate( NwamuiEnm* self, gchar **prop_name_ret )
{
    nwam_error_t    nerr;
    const char*     prop_name = NULL;

    g_return_val_if_fail( NWAMUI_IS_ENM(self), FALSE );

    if ( self->prv->nwam_enm_modified && self->prv->nwam_enm != NULL ) {
        if ( (nerr = nwam_enm_validate( self->prv->nwam_enm, &prop_name ) ) != NWAM_SUCCESS ) {
            g_debug("Failed when validating ENM for %s : invalid value for %s", 
                    self->prv->name, prop_name);
            if ( prop_name_ret != NULL ) {
                *prop_name_ret = g_strdup( prop_name );
            }
            return( FALSE );
        }
    }

    return( TRUE );
}

/**
 * nwamui_enm_commit:   commit in-memory configuration, to persistant storage
 * @returns: TRUE if succeeded, FALSE if failed
 **/
extern gboolean
nwamui_enm_commit( NwamuiEnm* self )
{
    nwam_error_t    nerr;

    g_return_val_if_fail( NWAMUI_IS_ENM(self), FALSE );

    if ( self->prv->nwam_enm_modified && self->prv->nwam_enm != NULL ) {
        if ( (nerr = nwam_enm_commit( self->prv->nwam_enm, 0 ) ) != NWAM_SUCCESS ) {
            g_warning("Failed when committing ENM for %s, %s", self->prv->name, nwam_strerror( nerr ));
            return( FALSE );
        }
    }

    return( TRUE );
}

static void
nwamui_enm_finalize (NwamuiEnm *self)
{
    if (self->prv->nwam_enm != NULL) {
        nwam_enm_free (self->prv->nwam_enm);
    }

    if (self->prv->name != NULL ) {
        g_free( self->prv->name );
    }

    g_free (self->prv); 
    self->prv = NULL;

    parent_class->finalize (G_OBJECT (self));
}

static nwam_state_t
nwamui_enm_get_nwam_state(NwamuiObject *object, nwam_aux_state_t* aux_state_p, const gchar**aux_state_string_p )
{
    nwam_state_t    rstate = NWAM_STATE_UNINITIALIZED;

    if ( aux_state_p ) {
        *aux_state_p = NWAM_AUX_STATE_UNINITIALIZED;
    }

    if ( aux_state_string_p ) {
        *aux_state_string_p = (const gchar*)nwam_aux_state_to_string( NWAM_AUX_STATE_UNINITIALIZED );
    }

    if ( NWAMUI_IS_ENM( object ) ) {
        NwamuiEnm   *enm = NWAMUI_ENM(object);
        nwam_state_t        state;
        nwam_aux_state_t    aux_state;


        /* First look at the LINK state, then the IP */
        if ( enm->prv->nwam_enm &&
             nwam_enm_get_state( enm->prv->nwam_enm, &state, &aux_state ) == NWAM_SUCCESS ) {

            rstate = state;
            if ( aux_state_p ) {
                *aux_state_p = aux_state;
            }
            if ( aux_state_string_p ) {
                *aux_state_string_p = (const gchar*)nwam_aux_state_to_string( aux_state );
            }
        }
    }

    return(rstate);
}

/* Callbacks */

static void
object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
/*     NwamuiEnm* self = NWAMUI_ENM(data); */
}

