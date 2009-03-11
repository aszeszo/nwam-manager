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
 */
#include <glib-object.h>
#include <glib/gi18n.h>
#include <strings.h>

#include "libnwamui.h"
        
static GObjectClass    *parent_class    = NULL;


struct _NwamuiCondPrivate {
    nwamui_cond_field_t     field;
    nwamui_cond_op_t        op;
    gchar*                  value;

    /* NCU, ENM and LOC require a pointer to an object */
    GObject*                object;
};

enum {
    PROP_FIELD = 1,
    PROP_OPER,
    PROP_VALUE,
    PROP_OBJECT
};

static void nwamui_cond_set_property ( GObject         *object,
                                       guint            prop_id,
                                       const GValue    *value,
                                       GParamSpec      *pspec);

static void nwamui_cond_get_property ( GObject         *object,
                                       guint            prop_id,
                                       GValue          *value,
                                       GParamSpec      *pspec);

static void nwamui_cond_finalize (     NwamuiCond *self);


/* Callbacks */
static void object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);

G_DEFINE_TYPE (NwamuiCond, nwamui_cond, NWAMUI_TYPE_OBJECT)

static void
nwamui_cond_class_init (NwamuiCondClass *klass)
{
    /* Pointer to GObject Part of Class */
    GObjectClass *gobject_class = (GObjectClass*) klass;
        
    /* Initialise Static Parent Class pointer */
    parent_class = g_type_class_peek_parent (klass);

    /* Override Some Function Pointers */
    gobject_class->set_property = nwamui_cond_set_property;
    gobject_class->get_property = nwamui_cond_get_property;
    gobject_class->finalize = (void (*)(GObject*)) nwamui_cond_finalize;

    /* Create some properties */
    g_object_class_install_property (gobject_class,
                                     PROP_FIELD,
                                     g_param_spec_int ("field",
                                                       _("field"),
                                                       _("field"),
                                                       NWAMUI_COND_FIELD_NCU,
                                                       NWAMUI_COND_FIELD_LAST,
                                                       NWAMUI_COND_FIELD_NCU,
                                                       G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_OPER,
                                     g_param_spec_int ("op",
                                                       _("op"),
                                                       _("op"),
                                                       NWAMUI_COND_OP_IS,
                                                       NWAMUI_COND_OP_LAST,
                                                       NWAMUI_COND_OP_INCLUDE,
                                                       G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_VALUE,
                                     g_param_spec_string ("value",
                                                          _("value"),
                                                          _("value"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_OBJECT,
                                     g_param_spec_object ("object",
                                                          _("object"),
                                                          _("object"),
                                                          G_TYPE_OBJECT,
                                                          G_PARAM_READWRITE));

}


typedef struct _ncu_info    ncu_info_t;

static void
nwamui_cond_init ( NwamuiCond *self)
{  
    self->prv = g_new0 (NwamuiCondPrivate, 1);
    
    self->prv->field = NWAMUI_COND_FIELD_NCU;
    self->prv->op = NWAMUI_COND_OP_INCLUDE;
    self->prv->value = g_strdup("");
    self->prv->object = NULL;

    g_signal_connect(G_OBJECT(self), "notify", (GCallback)object_notify_cb, (gpointer)self);
}

static void
nwamui_cond_finalize (NwamuiCond *self)
{
    g_debug("nwamui_cond_finalize called");
    if (self->prv->value != NULL ) {
        g_free( self->prv->value );
    }

    if (self->prv->object != NULL ) {
        g_object_unref( G_OBJECT(self->prv->object) );
    }

    g_free (self->prv); 
    self->prv = NULL;

    parent_class->finalize (G_OBJECT (self));
}

static void
nwamui_cond_set_property (   GObject         *object,
                            guint            prop_id,
                            const GValue    *value,
                            GParamSpec      *pspec)
{
    NwamuiCond *self = NWAMUI_COND(object);
    gchar*      tmpstr = NULL;
    gint        tmpint = 0;

    switch (prop_id) {
        case PROP_FIELD: {
                self->prv->field = (nwamui_cond_field_t)g_value_get_int( value );
            }
            break;

        case PROP_OPER: {
                self->prv->op = (nwamui_cond_op_t)g_value_get_int( value );
            }
            break;

        case PROP_VALUE: {
                if ( self->prv->value != NULL ) {
                        g_free( self->prv->value );
                }
                self->prv->value = g_strdup( g_value_get_string( value ) );
            }
            break;

        case PROP_OBJECT: {
                if ( self->prv->object != NULL ) {
                        g_object_unref( self->prv->object );
                }
                self->prv->object = G_OBJECT(g_value_dup_object( value ));
            }
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
nwamui_cond_get_property (   GObject         *object,
                            guint            prop_id,
                            GValue          *value,
                            GParamSpec      *pspec)
{
    NwamuiCond *self = NWAMUI_COND(object);

    switch (prop_id) {
        case PROP_FIELD: {
                g_value_set_int( value, (gint)self->prv->field );
            }
            break;

        case PROP_OPER: {
                g_value_set_int( value, (gint)self->prv->op );
            }
            break;

        case PROP_VALUE: {
                g_value_set_string( value, self->prv->value );
            }
            break;

        case PROP_OBJECT: {
                g_value_set_object( value, (gpointer)self->prv->object);
            }
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}


/**
 * nwamui_cond_new:
 * @returns: a new #NwamuiCond.
 *
 **/
extern  NwamuiCond*          
nwamui_cond_new ( void )
{
    NwamuiCond *self = NWAMUI_COND(g_object_new (NWAMUI_TYPE_COND, NULL));
    
    return( self );
}

/**
 * nwamui_cond_new_with_args:
 * @returns: a new #NwamuiCond.
 *
 **/
extern  NwamuiCond*          
nwamui_cond_new_with_args ( nwamui_cond_field_t     field,
                            nwamui_cond_op_t        op,
                            const gchar*            value )
{
    NwamuiCond *self = NWAMUI_COND(g_object_new (NWAMUI_TYPE_COND, NULL));

    g_object_set (G_OBJECT (self),
                  "field", field,
                  "op", op,
                  "value", value,
                  NULL);
    
    return( self );
}

static nwamui_cond_field_t
map_condition_obj_to_field( nwam_condition_object_type_t condition_object )
{
    nwamui_cond_field_t field;

    switch( condition_object ) {
        case NWAM_CONDITION_OBJECT_TYPE_NCU:
            field = NWAMUI_COND_FIELD_NCU;
            break;
        case NWAM_CONDITION_OBJECT_TYPE_ENM:
            field = NWAMUI_COND_FIELD_ENM;
            break;
        case NWAM_CONDITION_OBJECT_TYPE_LOC:
            field = NWAMUI_COND_FIELD_LOC;
            break;
        case NWAM_CONDITION_OBJECT_TYPE_IP_ADDRESS:
            field = NWAMUI_COND_FIELD_IP_ADDRESS;
            break;
        case NWAM_CONDITION_OBJECT_TYPE_ADV_DOMAIN:
            field = NWAMUI_COND_FIELD_ADV_DOMAIN;
            break;
        case NWAM_CONDITION_OBJECT_TYPE_SYS_DOMAIN:
            field = NWAMUI_COND_FIELD_SYS_DOMAIN;
            break;
        case NWAM_CONDITION_OBJECT_TYPE_ESSID:
            field = NWAMUI_COND_FIELD_ESSID;
            break;
        case NWAM_CONDITION_OBJECT_TYPE_BSSID:
            field = NWAMUI_COND_FIELD_BSSID;
            break;
        default:
            g_assert_not_reached();
    }

    return( field );
}

static nwam_condition_object_type_t 
map_field_to_condition_obj( nwamui_cond_field_t field )
{
    nwam_condition_object_type_t condition_object;

    switch( field ) {
        case NWAMUI_COND_FIELD_NCU:
            condition_object = NWAM_CONDITION_OBJECT_TYPE_NCU;
            break;
        case NWAMUI_COND_FIELD_ENM:
            condition_object = NWAM_CONDITION_OBJECT_TYPE_ENM;
            break;
        case NWAMUI_COND_FIELD_LOC:
            condition_object = NWAM_CONDITION_OBJECT_TYPE_LOC;
            break;
        case NWAMUI_COND_FIELD_IP_ADDRESS:
            condition_object = NWAM_CONDITION_OBJECT_TYPE_IP_ADDRESS;
            break;
        case NWAMUI_COND_FIELD_ADV_DOMAIN:
            condition_object = NWAM_CONDITION_OBJECT_TYPE_ADV_DOMAIN;
            break;
        case NWAMUI_COND_FIELD_SYS_DOMAIN:
            condition_object = NWAM_CONDITION_OBJECT_TYPE_SYS_DOMAIN;
            break;
        case NWAMUI_COND_FIELD_ESSID:
            condition_object = NWAM_CONDITION_OBJECT_TYPE_ESSID;
            break;
        case NWAMUI_COND_FIELD_BSSID:
            condition_object = NWAM_CONDITION_OBJECT_TYPE_BSSID;
            break;
        default:
            g_assert_not_reached();
    }

    return( condition_object );
}

static nwamui_cond_op_t
map_condition_to_op( nwam_condition_t condition, gboolean field_is_obj ) 
{
    nwamui_cond_op_t op;

    switch( condition ) {
        case NWAM_CONDITION_IS:
            if ( field_is_obj ) {
                op = NWAMUI_COND_OP_INCLUDE;
            }
            else {
                op = NWAMUI_COND_OP_IS;
            }
            break;
        case NWAM_CONDITION_IS_NOT:
            if ( field_is_obj ) {
                op = NWAMUI_COND_OP_DOES_NOT_INCLUDE;
            }
            else {
                op = NWAMUI_COND_OP_IS_NOT;
            }
            break;
        case NWAM_CONDITION_IS_IN_RANGE:
            op = NWAMUI_COND_OP_IS_IN_RANGE;
            break;
        case NWAM_CONDITION_IS_NOT_IN_RANGE:
            op = NWAMUI_COND_OP_IS_NOT_IN_RANGE;
            break;
        case NWAM_CONDITION_CONTAINS:
            op = NWAMUI_COND_OP_CONTAINS;
            break;
        case NWAM_CONDITION_DOES_NOT_CONTAIN:
            op = NWAMUI_COND_OP_DOES_NOT_CONTAIN;
            break;
        default:
            g_assert_not_reached();
    }

    return( op );
}

static nwam_condition_t 
map_op_to_condition( nwamui_cond_op_t op )
{
    nwam_condition_t condition;

    switch( op ) {
        case NWAMUI_COND_OP_IS:
        case NWAMUI_COND_OP_INCLUDE:
            condition = NWAM_CONDITION_IS;
            break;
        case NWAMUI_COND_OP_IS_NOT:
        case NWAMUI_COND_OP_DOES_NOT_INCLUDE:
            condition = NWAM_CONDITION_IS_NOT;
            break;
        case NWAMUI_COND_OP_IS_IN_RANGE:
            condition = NWAM_CONDITION_IS_IN_RANGE;
            break;
        case NWAMUI_COND_OP_IS_NOT_IN_RANGE:
            condition = NWAM_CONDITION_IS_NOT_IN_RANGE;
            break;
        case NWAMUI_COND_OP_CONTAINS:
            condition = NWAM_CONDITION_CONTAINS;
            break;
        case NWAMUI_COND_OP_DOES_NOT_CONTAIN:
            condition = NWAM_CONDITION_DOES_NOT_CONTAIN;
            break;
        default:
            g_assert_not_reached();
    }

    return( condition );
}

static gboolean
populate_fields_from_string( NwamuiCond* self, const gchar* condition_str ) 
{
    nwam_error_t nerr;
    nwam_condition_object_type_t condition_object;
    nwam_condition_t             condition;
    char *                       value;
    gboolean                     field_is_obj;
    
    g_return_val_if_fail( NWAMUI_IS_COND(self), FALSE );
    g_return_val_if_fail( condition_str != NULL, FALSE );

    if ( (nerr = nwam_condition_string_to_condition( condition_str, &condition_object, 
                                                     &condition, &value )) != NWAM_SUCCESS ) {
        g_warning("Failed to parse condition string: %s", condition_str?condition_str:"NULL");
        return( FALSE );
    }

    self->prv->field = map_condition_obj_to_field( condition_object );
    field_is_obj = ( self->prv->field == NWAMUI_COND_FIELD_NCU ||
                     self->prv->field == NWAMUI_COND_FIELD_ENM ||
                     self->prv->field == NWAMUI_COND_FIELD_LOC );
    self->prv->op = map_condition_to_op( condition, field_is_obj );
    self->prv->value = g_strdup((value != NULL)?(value):(""));

    return( TRUE );
}

/**
 * nwamui_cond_new_from_str:
 * args: condition_str - libnwam formatted condition string
 * @returns: a new #NwamuiCond, or NULL if string can't be parsed.
 *
 **/
extern  NwamuiCond*          
nwamui_cond_new_from_str( const gchar* condition_str )
{
    NwamuiCond         *self = NWAMUI_COND(g_object_new (NWAMUI_TYPE_COND, NULL));
    nwamui_cond_field_t field = NWAMUI_COND_FIELD_NCU;
    nwamui_cond_op_t    op = NWAMUI_COND_OP_INCLUDE;
    char*               value = "";

    populate_fields_from_string( self, condition_str );

    return( self );
}

/**
 * nwamui_cond_to_string:
 * args: condition: a #NwamuiCond
 * @returns: libnwam formatted condition string
 *
 **/
extern char*
nwamui_cond_to_string( NwamuiCond* self )         
{
    char*                        new_str = NULL;
    nwam_error_t                 nerr;
    nwam_condition_object_type_t condition_object;
    nwam_condition_t             condition;
    char *                       value;

    condition_object = map_field_to_condition_obj( self->prv->field );
    condition = map_op_to_condition( self->prv->op );
    value = strdup( self->prv->value );

    if ( (nerr = nwam_condition_to_condition_string( condition_object, 
                                                     condition, value, &new_str )) != NWAM_SUCCESS ) {
        g_warning("Failed to generate condition string");
        return( NULL );
    }

    free(value);

    return( new_str );
}

/** 
 * nwamui_cond_set_field:
 * @nwamui_cond: a #NwamuiCond.
 * @field: Value to set field to.
 * 
 **/ 
extern void
nwamui_cond_set_field (   NwamuiCond *self,
                              nwamui_cond_field_t        field )
{
    g_return_if_fail (NWAMUI_IS_COND (self));
    g_assert (field >= NWAMUI_COND_FIELD_NCU && field <= NWAMUI_COND_FIELD_LAST );

    g_object_set (G_OBJECT (self),
                  "field", (gint)field,
                  NULL);
}

/**
 * nwamui_cond_get_field:
 * @nwamui_cond: a #NwamuiCond.
 * @returns: the field.
 *
 **/
extern nwamui_cond_field_t
nwamui_cond_get_field (NwamuiCond *self)
{
    gint  field = NWAMUI_COND_FIELD_IP_ADDRESS; 

    g_return_val_if_fail (NWAMUI_IS_COND (self), field);

    g_object_get (G_OBJECT (self),
                  "field", &field,
                  NULL);

    return( (nwamui_cond_field_t)field );
}

/** 
 * nwamui_cond_set_op:
 * @nwamui_cond: a #NwamuiCond.
 * @op: Value to set op to.
 * 
 **/ 
extern void
nwamui_cond_set_oper (   NwamuiCond *self,
                              nwamui_cond_op_t        op )
{
    g_return_if_fail (NWAMUI_IS_COND (self));
    g_assert (op >= NWAMUI_COND_OP_IS && op <= NWAMUI_COND_OP_LAST );

    g_object_set (G_OBJECT (self),
                  "op", (gint)op,
                  NULL);
}

/**
 * nwamui_cond_get_op:
 * @nwamui_cond: a #NwamuiCond.
 * @returns: the op.
 *
 **/
extern nwamui_cond_op_t
nwamui_cond_get_oper (NwamuiCond *self)
{
    gint  op = NWAMUI_COND_OP_IS; 

    g_return_val_if_fail (NWAMUI_IS_COND (self), op);

    g_object_get (G_OBJECT (self),
                  "op", &op,
                  NULL);

    return( (nwamui_cond_op_t)op );
}

/** 
 * nwamui_cond_set_value:
 * @nwamui_cond: a #NwamuiCond.
 * @value: Value to set value to.
 * 
 **/ 
extern void
nwamui_cond_set_value (   NwamuiCond *self,
                              const gchar*  value )
{
    g_return_if_fail (NWAMUI_IS_COND (self));
    g_assert (value != NULL );

    if ( value != NULL ) {
        g_object_set (G_OBJECT (self),
                      "value", value,
                      NULL);
    }
}

/**
 * nwamui_cond_get_value:
 * @nwamui_cond: a #NwamuiCond.
 * @returns: the value.
 *
 **/
extern gchar*
nwamui_cond_get_value (NwamuiCond *self)
{
    gchar*  value = NULL; 

    g_return_val_if_fail (NWAMUI_IS_COND (self), value);

    g_object_get (G_OBJECT (self),
                  "value", &value,
                  NULL);

    return( value );
}

/** 
 * nwamui_cond_set_object:
 * @nwamui_cond: a #NwamuiCond.
 * @object: Value to set object to.
 * 
 **/ 
extern void
nwamui_cond_set_object (   NwamuiCond *self,
                              GObject*  object )
{
    g_return_if_fail (NWAMUI_IS_COND (self));
    g_assert (object != NULL );

    switch ( self->prv->field ) {
        case NWAMUI_COND_FIELD_NCU:
            g_return_if_fail( NWAMUI_IS_NCU( object ) );
            break;
        case NWAMUI_COND_FIELD_ENM:
            g_return_if_fail( NWAMUI_IS_ENM( object ) );
            break;
        case NWAMUI_COND_FIELD_LOC:
            g_return_if_fail( NWAMUI_IS_ENV( object ) );
            break;
        default:
            g_warning("Setting object with field type '%s'", 
                      nwamui_cond_field_to_str( self->prv->field ) );
            return;
    }

    if ( object != NULL ) {
        g_object_set (G_OBJECT (self),
                      "object", object,
                      NULL);
    }
}

/**
 * nwamui_cond_get_object:
 * @nwamui_cond: a #NwamuiCond.
 * @returns: the object.
 *
 **/
extern GObject*
nwamui_cond_get_object (NwamuiCond *self)
{
    GObject*  object = NULL; 

    g_return_val_if_fail (NWAMUI_IS_COND (self), object);

    g_object_get (G_OBJECT (self),
                  "object", &object,
                  NULL);

    return( object );
}

extern const gchar*
nwamui_cond_field_to_str( nwamui_cond_field_t field )
{
    
    switch( field ) {
        case NWAMUI_COND_FIELD_NCU:          return(_("Active Connections"));
        case NWAMUI_COND_FIELD_ENM:          return(_("Running VPN Applications"));
        case NWAMUI_COND_FIELD_LOC:          return(_("Current Location"));
        case NWAMUI_COND_FIELD_IP_ADDRESS:   return(_("Any IP Address"));
        case NWAMUI_COND_FIELD_ADV_DOMAIN:   return(_("Advertised Domain Name"));
        case NWAMUI_COND_FIELD_SYS_DOMAIN:   return(_("System Domain Name"));
        case NWAMUI_COND_FIELD_ESSID:        return(_("Wireless Network Name"));
        case NWAMUI_COND_FIELD_BSSID:        return(_("Wireless network (BSSID)"));
        default:
            g_assert_not_reached();
            return(NULL);
    }
}

extern const gchar*
nwamui_cond_op_to_str( nwamui_cond_op_t op )
{
    switch( op ) {
        case NWAMUI_COND_OP_IS:                 return(_("is"));
        case NWAMUI_COND_OP_IS_NOT:             return(_("is not"));
        case NWAMUI_COND_OP_INCLUDE:            return(_("include"));
        case NWAMUI_COND_OP_DOES_NOT_INCLUDE:   return(_("does not include"));
        case NWAMUI_COND_OP_IS_IN_RANGE:        return(_("is in the range"));
        case NWAMUI_COND_OP_IS_NOT_IN_RANGE:    return(_("is not in the range"));
        case NWAMUI_COND_OP_CONTAINS:           return(_("contains"));
        case NWAMUI_COND_OP_DOES_NOT_CONTAIN:   return(_("does not contain"));
        default:
            g_assert_not_reached();
            return(NULL);
    }
}

/* Callbacks */

static void
object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
    NwamuiCond* self = NWAMUI_COND(data);

    g_debug("NwamuiCond: notify %s changed\n", arg1->name );
    g_debug("%s %s %s", nwamui_cond_field_to_str(self->prv->field),
            nwamui_cond_op_to_str(self->prv->op),
            self->prv->value?self->prv->value:"NULL");
}


