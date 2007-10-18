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
};

enum {
    PROP_FIELD = 1,
    PROP_OPER,
    PROP_VALUE
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

G_DEFINE_TYPE (NwamuiCond, nwamui_cond, G_TYPE_OBJECT)

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
                                                       NWAMUI_COND_FIELD_IP_ADDRESS,
                                                       NWAMUI_COND_FIELD_LAST,
                                                       NWAMUI_COND_FIELD_IP_ADDRESS,
                                                       G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_OPER,
                                     g_param_spec_int ("op",
                                                       _("op"),
                                                       _("op"),
                                                       NWAMUI_COND_OP_EQUALS,
                                                       NWAMUI_COND_OP_LAST,
                                                       NWAMUI_COND_OP_EQUALS,
                                                       G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_VALUE,
                                     g_param_spec_string ("value",
                                                          _("value"),
                                                          _("value"),
                                                          "",
                                                          G_PARAM_READWRITE));


}


typedef struct _ncu_info    ncu_info_t;

static void
nwamui_cond_init ( NwamuiCond *self)
{  
    self->prv = g_new0 (NwamuiCondPrivate, 1);
    
    self->prv->field = NWAMUI_COND_FIELD_IP_ADDRESS;
    self->prv->op = NWAMUI_COND_OP_EQUALS;
    self->prv->value = NULL;

    g_signal_connect(G_OBJECT(self), "notify", (GCallback)object_notify_cb, (gpointer)self);
}

static void
nwamui_cond_finalize (NwamuiCond *self)
{
    g_debug("nwamui_cond_finalize called");
    if (self->prv->value != NULL ) {
        g_free( self->prv->value );
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
                            gchar*                  value )
{
    NwamuiCond *self = NWAMUI_COND(g_object_new (NWAMUI_TYPE_COND, NULL));

    g_object_set (G_OBJECT (self),
                  "field", field,
                  "op", op,
                  "value", value,
                  NULL);
    
    return( self );
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
    g_assert (field >= NWAMUI_COND_FIELD_IP_ADDRESS && field <= NWAMUI_COND_FIELD_LAST );

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
    g_assert (op >= NWAMUI_COND_OP_EQUALS && op <= NWAMUI_COND_OP_LAST );

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
    gint  op = NWAMUI_COND_OP_EQUALS; 

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

extern const gchar*
nwamui_cond_field_to_str( nwamui_cond_field_t field )
{
    
    switch( field ) {
        case NWAMUI_COND_FIELD_IP_ADDRESS:   return(_("IP Address"));
        case NWAMUI_COND_FIELD_DOMAIN_NAME:  return(_("Domain Name"));
        case NWAMUI_COND_FIELD_ESSID:        return(_("Wireless Name (ESSID)"));
        case NWAMUI_COND_FIELD_BSSID:        return(_("Wireless AP (BSSID)"));
        default:
            g_assert_not_reached();
            return(NULL);
    }
}

extern const gchar*
nwamui_cond_op_to_str( nwamui_cond_op_t op )
{
    switch( op ) {
        case NWAMUI_COND_OP_EQUALS:         return(_("equal to"));
        case NWAMUI_COND_OP_NOT_EQUAL:      return(_("not equal to"));
        case NWAMUI_COND_OP_IN_RANGE:       return(_("in the range"));
        case NWAMUI_COND_OP_BEGINS:         return(_("begins with"));
        case NWAMUI_COND_OP_ENDS:           return(_("ends with"));
        case NWAMUI_COND_OP_CONTAINS:       return(_("contains"));
        case NWAMUI_COND_OP_DOESNT_CONTAIN: return(_("doesn't contain"));
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


