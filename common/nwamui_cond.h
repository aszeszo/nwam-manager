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

#ifndef _NWAMUI_COND_H
#define	_NWAMUI_COND_H

#ifndef _libnwamui_H
#error "Please include libnwamui.h header instead."
#endif

G_BEGIN_DECLS

#define NWAMUI_TYPE_COND               (nwamui_cond_get_type ())
#define NWAMUI_COND(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), NWAMUI_TYPE_COND, NwamuiCond))
#define NWAMUI_COND_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), NWAMUI_TYPE_COND, NwamuiCondClass))
#define NWAMUI_IS_COND(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NWAMUI_TYPE_COND))
#define NWAMUI_IS_COND_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), NWAMUI_TYPE_COND))
#define NWAMUI_COND_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), NWAMUI_TYPE_COND, NwamuiCondClass))


typedef struct _NwamuiCond              NwamuiCond;
typedef struct _NwamuiCondClass         NwamuiCondClass;
typedef struct _NwamuiCondPrivate	NwamuiCondPrivate;

struct _NwamuiCond
{
	NwamuiObject                      object;

	/*< private >*/
	NwamuiCondPrivate    *prv;
};

struct _NwamuiCondClass
{
	NwamuiObjectClass                parent_class;
};

extern  GType                   nwamui_cond_get_type (void) G_GNUC_CONST;

typedef enum {
    NWAMUI_COND_ACTIVATION_MODE_MANUAL,
    NWAMUI_COND_ACTIVATION_MODE_SYSTEM,
    NWAMUI_COND_ACTIVATION_MODE_CONDITIONAL_ANY,
    NWAMUI_COND_ACTIVATION_MODE_CONDITIONAL_ALL,
    NWAMUI_COND_ACTIVATION_MODE_PRIORITIZED,
    NWAMUI_COND_ACTIVATION_MODE_LAST /* Not to be used directly */
} nwamui_cond_activation_mode_t;

typedef enum {
	NWAMUI_COND_PRIORITY_GROUP_MODE_EXCLUSIVE,
	NWAMUI_COND_PRIORITY_GROUP_MODE_SHARED,
	NWAMUI_COND_PRIORITY_GROUP_MODE_ALL,
	NWAMUI_COND_PRIORITY_GROUP_MODE_LAST /* Not to be used directly */
} nwamui_cond_priority_group_mode_t;

typedef enum {
	NWAMUI_COND_FIELD_NCU = 0,
	NWAMUI_COND_FIELD_ENM,
	NWAMUI_COND_FIELD_LOC,
	NWAMUI_COND_FIELD_IP_ADDRESS,
	NWAMUI_COND_FIELD_ADV_DOMAIN,
	NWAMUI_COND_FIELD_SYS_DOMAIN,
	NWAMUI_COND_FIELD_ESSID,
	NWAMUI_COND_FIELD_BSSID,
    NWAMUI_COND_FIELD_LAST /* Not to be used directly */                
} nwamui_cond_field_t;


typedef enum {
    NWAMUI_COND_OP_IS = 0,
    NWAMUI_COND_OP_IS_NOT,
    NWAMUI_COND_OP_INCLUDE,         
    NWAMUI_COND_OP_DOES_NOT_INCLUDE,
    NWAMUI_COND_OP_IS_IN_RANGE,
    NWAMUI_COND_OP_IS_NOT_IN_RANGE,
    NWAMUI_COND_OP_CONTAINS,
    NWAMUI_COND_OP_DOES_NOT_CONTAIN,
    NWAMUI_COND_OP_LAST /* Not to be used directly */
} nwamui_cond_op_t;

extern  NwamuiCond*          nwamui_cond_new ( void );

extern  NwamuiCond*          nwamui_cond_new_from_str( const gchar* condition_str );

extern  NwamuiCond*          nwamui_cond_new_with_args (    nwamui_cond_field_t     field,
                                                            nwamui_cond_op_t        op,
                                                            const gchar*            value );

extern void                 nwamui_cond_set_field ( NwamuiCond *self, nwamui_cond_field_t field );
extern nwamui_cond_field_t  nwamui_cond_get_field ( NwamuiCond *self );


extern void                 nwamui_cond_set_oper ( NwamuiCond *self, nwamui_cond_op_t oper );
extern nwamui_cond_op_t     nwamui_cond_get_oper ( NwamuiCond *self );


extern void                 nwamui_cond_set_value ( NwamuiCond *self, const gchar* value );
extern gchar*               nwamui_cond_get_value ( NwamuiCond *self );

extern void                 nwamui_cond_set_object ( NwamuiCond *self, GObject* object );
extern GObject*             nwamui_cond_get_object ( NwamuiCond *self );

/* Utility functions to convert enum to a displayable string */
extern const gchar*         nwamui_cond_op_to_str( nwamui_cond_op_t op );
extern const gchar*         nwamui_cond_field_to_str( nwamui_cond_field_t field );

/* Convert to string suitable for libnwam - uses libnwam */
extern char*                nwamui_cond_to_string( NwamuiCond* self );

G_END_DECLS

#endif	/* _NWAMUI_COND_H */

