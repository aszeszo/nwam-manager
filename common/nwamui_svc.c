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
 * File:   nwamui_svc.c
 *
 */

#include <libnwam.h>
#include <glib-object.h>
#include <glib/gi18n.h>
#include <strings.h>

#include "libnwamui.h"
#include "nwamui_svc.h"
#include "libscf.h"

#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
	NWAMUI_TYPE_SVC, NwamuiSvcPrivate)) 

struct _NwamuiSvcPrivate {
    gchar*                      svc_fmri;
    gchar*                      svc_description;
    gboolean                    status;
    gboolean                    is_default;
    gboolean                    is_new;
};

static void nwamui_svc_set_property (GObject		 *object,
  guint			 prop_id,
  const GValue	*value,
  GParamSpec		*pspec);

static void nwamui_svc_get_property (GObject		 *object,
  guint			 prop_id,
  GValue			*value,
  GParamSpec		*pspec);

static void nwamui_svc_finalize (NwamuiSvc *self);

static void populate_svc( NwamuiSvc *self );

/* Callbacks */

enum {
	PROP_FMRI = 1,
	PROP_DESC,
	PROP_STAT,
	PROP_DEFAULT,
};

G_DEFINE_TYPE (NwamuiSvc, nwamui_svc, NWAMUI_TYPE_OBJECT)

static void
nwamui_svc_class_init (NwamuiSvcClass *klass)
{
	/* Pointer to GObject Part of Class */
	GObjectClass *gobject_class = (GObjectClass*) klass;
		
	/* Override Some Function Pointers */
	gobject_class->set_property = nwamui_svc_set_property;
	gobject_class->get_property = nwamui_svc_get_property;
	gobject_class->finalize = (void (*)(GObject*)) nwamui_svc_finalize;
	g_type_class_add_private (klass, sizeof (NwamuiSvcPrivate));

	/* Create some properties */
	g_object_class_install_property (gobject_class,
      PROP_FMRI,
      g_param_spec_string ("fmri",
        _("fmri"),
        _("fmri"),
        "",
        G_PARAM_READABLE));

	g_object_class_install_property (gobject_class,
      PROP_DESC,
      g_param_spec_string ("description",
        _("Description"),
        _("Description"),
        "",
        G_PARAM_READABLE));

	g_object_class_install_property (gobject_class,
      PROP_STAT,
      g_param_spec_boolean ("status",
        _("status"),
        _("status"),
        FALSE,
        G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class,
      PROP_DEFAULT,
      g_param_spec_boolean ("is-default",
        _("is-default"),
        _("is-default"),
        FALSE,
        G_PARAM_READWRITE));
}


static void
nwamui_svc_init (NwamuiSvc *self)
{
	NwamuiSvcPrivate *prv = GET_PRIVATE(self);
	self->prv = prv;

	g_signal_connect(G_OBJECT(self), "notify", (GCallback)object_notify_cb, (gpointer)self);
}

static void
nwamui_svc_set_property (GObject		 *object,
  guint			 prop_id,
  const GValue	*value,
  GParamSpec		*pspec)
{
	NwamuiSvcPrivate *prv = GET_PRIVATE(object);

	switch (prop_id) {
    case PROP_FMRI: {
        if ( prv->svc_fmri != NULL ) {
            g_free(prv->svc_fmri);
        }
        prv->svc_fmri = g_strdup( g_value_get_string( value ) );
    }
        break;
    case PROP_DESC: {
        if ( prv->svc_description != NULL ) {
            g_free(prv->svc_description);
        }
        prv->svc_description = g_strdup( g_value_get_string( value ) );
    }
        break;
#if 0
    case PROP_STAT: {
        // TODO, enable/disable svc
        prv->status = g_value_get_boolean (value);
    }
        break;
    case PROP_DEFAULT: {
        prv->is_default = g_value_get_boolean (value);
    }
        break;
#endif
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
	}
}

static void
nwamui_svc_get_property (GObject		 *object,
  guint			   prop_id,
  GValue		  *value,
  GParamSpec	  *pspec)
{
	NwamuiSvcPrivate *prv = GET_PRIVATE(object);

	switch (prop_id) {
    case PROP_FMRI: {
        g_value_set_string(value, prv->svc_fmri);
    }
        break;
    case PROP_DESC: {
        g_value_set_string(value, prv->svc_description);
    }
        break;
#if 0 
    case PROP_STAT: {
        g_value_set_boolean (value, prv->status);
    }
        break;
    case PROP_DEFAULT: {
        g_value_set_boolean (value, prv->is_default);
    }
        break;
#endif
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
	}
}

static void
nwamui_svc_finalize (NwamuiSvc *self)
{
	NwamuiSvcPrivate *prv = GET_PRIVATE(self);

    if (prv->svc_fmri != NULL) {
        g_free( prv->svc_fmri );
    }
    
    if (prv->svc_description != NULL) {
        g_free( prv->svc_description );
    }
    
	G_OBJECT_CLASS(nwamui_svc_parent_class)->finalize(G_OBJECT(self));
}

/* Exported Functions */


/**
 * nwamui_svc_new:
 *
 * @fmri: 
 * 
 * @returns: a new #NwamuiSvc.
 *
 * Creates a new #NwamuiSvc.
 **/
extern	NwamuiSvc*
nwamui_svc_new (const gchar* fmri)
{
	NwamuiSvc*	self = NULL;

    if ( fmri != NULL ) {
        /* Validate FMRI */
        const char* state = smf_get_state( fmri );
        if ( state == NULL ) {
            /* Failed to validate */
            return( NULL );
        }
    }
	
	self = NWAMUI_SVC(g_object_new (NWAMUI_TYPE_SVC,
                        "fmri", fmri,
                        NULL));

    populate_svc( self );
	
	return( self );
}

static void
populate_svc( NwamuiSvc *self )
{
    static scf_handle_t        *handle = NULL;
	static ssize_t              max_scf_name_length = 0;
    static scf_instance_t      *instance = NULL;
    static scf_propertygroup_t *pg = NULL;
    static scf_property_t      *prop = NULL;
    static scf_value_t         *value = NULL;
    gchar  *desc = g_malloc0( max_scf_name_length + 1 );


    if ( handle == NULL ) {
        max_scf_name_length = scf_limit(SCF_LIMIT_MAX_NAME_LENGTH);
        instance = scf_instance_create( handle );
        pg = scf_pg_create( handle );
        prop = scf_property_create( handle );
        value = scf_value_create( handle );

        if (scf_handle_bind(handle) == -1 ) {
            g_warning( "couldn't bind to smf service: %s", scf_strerror(scf_error()) );
        }
    }


    if ( scf_handle_decode_fmri( handle, self->prv->svc_fmri , NULL, NULL, instance, NULL, NULL,
            SCF_DECODE_FMRI_REQUIRE_INSTANCE ) < 0 ) {
        g_warning( "%s", scf_strerror(scf_error()) );
    }

    if ( scf_instance_get_pg( instance, SCF_PG_TM_COMMON_NAME, pg) < 0 ) {
        g_warning( "%s", scf_strerror(scf_error()) );
    }

    if ( scf_pg_get_property(pg, "C", prop ) < 0 ) {
        g_warning( "%s", scf_strerror(scf_error()) );
    }

    if ( scf_property_get_value(prop, value ) < 0 ) {
        g_warning( "%s", scf_strerror(scf_error()) );
    }
    else {
        if ( scf_value_get_ustring(value, desc, max_scf_name_length + 1 ) < 0 ) {
            g_warning( "%s", scf_strerror(scf_error()) );
        }
        else {
            g_object_set( self,
                          "description", desc,
                          NULL);
        }
    }

    g_free(desc);
}

/**
 * nwamui_svc_get_description:
 * @nwamui_svc: a #NwamuiSvc.
 * @returns: the description.
 *
 **/
extern gchar*
nwamui_svc_get_description (NwamuiSvc *self)
{
	gchar*	description = NULL; 

	g_return_val_if_fail (NWAMUI_IS_SVC (self), description);

	g_object_get (G_OBJECT (self),
				  "description", &description,
				  NULL);

	return( description );
}

extern gchar *
nwamui_svc_get_name (NwamuiSvc *self)
{
    gchar *name = NULL;
    
	g_return_val_if_fail (NWAMUI_IS_SVC (self), name);

    g_object_get (self, "fmri", &name, NULL);
    return name;
}

/*
 * TODO: Decide if we still need these.
 */
#if 0
extern void
nwamui_svc_set_status (NwamuiSvc *self, gboolean status)
{
	g_return_if_fail (NWAMUI_IS_SVC (self));

    g_object_set (self, "status", status, NULL);
}

extern gboolean
nwamui_svc_get_status (NwamuiSvc *self)
{
    gboolean status = FALSE;
    
	g_return_val_if_fail (NWAMUI_IS_SVC (self), status);

    g_object_get (self, "status", &status, NULL);

    return status;
}

extern void
nwamui_svc_set_default (NwamuiSvc *self, gboolean is_default)
{
	g_return_if_fail (NWAMUI_IS_SVC (self));

    g_object_set (self, "is-default", is_default, NULL);
}

extern gboolean
nwamui_svc_is_default (NwamuiSvc *self)
{
    gboolean is_default = FALSE;
    
	g_return_val_if_fail (NWAMUI_IS_SVC (self), is_default);

    g_object_get (self, "is-default", &is_default, NULL);

    return is_default;
}
#endif /* 0 */

/* Callbacks */
