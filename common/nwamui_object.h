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
 * File:   nwamui_object.h
 *
 */

#ifndef _NWAMUI_OBJECT_H
#define	_NWAMUI_OBJECT_H

#ifndef _libnwamui_H
#error "Please include libnwamui.h header instead."
#endif

G_BEGIN_DECLS

#define NWAMUI_TYPE_OBJECT               (nwamui_object_get_type ())
#define NWAMUI_OBJECT(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), NWAMUI_TYPE_OBJECT, NwamuiObject))
#define NWAMUI_OBJECT_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), NWAMUI_TYPE_OBJECT, NwamuiObjectClass))
#define NWAMUI_IS_OBJECT(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NWAMUI_TYPE_OBJECT))
#define NWAMUI_IS_OBJECT_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), NWAMUI_TYPE_OBJECT))
#define NWAMUI_OBJECT_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), NWAMUI_TYPE_OBJECT, NwamuiObjectClass))


typedef struct _NwamuiObject		     NwamuiObject;
typedef struct _NwamuiObjectClass        NwamuiObjectClass;
typedef struct _NwamuiObjectPrivate	     NwamuiObjectPrivate;

struct _NwamuiObject
{
	GObject                      object;
    
    NwamuiObjectPrivate         *prv;
};

struct _NwamuiObjectClass
{
	GObjectClass                                parent_class;

    /* Create or open handle from the name. */
    gint (*open)(NwamuiObject *object, gint flag);
    /* Set name based on the passed handle, then open handle from the name. */
    void (*set_handle)(NwamuiObject *object, const gpointer handle);
    const gchar* (*get_name)(NwamuiObject *object);
    gboolean (*can_rename)(NwamuiObject *object);
    /* Set name only on init. If change name, need call nwam_*_set_name. */
    gboolean (*set_name)(NwamuiObject *object, const gchar* name);
    GList* (*get_conditions)(NwamuiObject *object);
    void (*set_conditions)(NwamuiObject *object, const GList* conditions);
    gint (*get_activation_mode)(NwamuiObject *object);
    void (*set_activation_mode)(NwamuiObject *object, gint activation_mode);
    gboolean (*get_active)(NwamuiObject *object);
    void (*set_active)(NwamuiObject *object, gboolean active);
    gboolean (*get_enabled)(NwamuiObject *object);
    void (*set_enabled)(NwamuiObject *object, gboolean enabled);
    nwam_state_t (*get_nwam_state)(NwamuiObject *object, nwam_aux_state_t* aux_state, const gchar**aux_state_string);
    void (*set_nwam_state)(NwamuiObject *object, nwam_state_t state, nwam_aux_state_t aux_state);
    gint (*sort)(NwamuiObject *object, NwamuiObject *other, guint sort_by);
    gboolean (*commit)(NwamuiObject *object);
    void (*reload)(NwamuiObject *object);
    gboolean (*destroy)(NwamuiObject *object);
    gboolean (*is_modifiable)(NwamuiObject *object);
    gboolean (*has_modifications)(NwamuiObject *object);
    NwamuiObject *(*clone)(NwamuiObject *object, const gchar *name, NwamuiObject *parent);
};

enum {
    NWAMUI_OBJECT_CREATE = 0,
    NWAMUI_OBJECT_OPEN,
};

enum {
    NWAMUI_OBJECT_SORT_BY_NAME = 0,
    NWAMUI_OBJECT_SORT_BY_GROUP,
};

extern GType               nwamui_object_get_type (void) G_GNUC_CONST;

extern gint          nwamui_object_open(NwamuiObject *object, gint flag);
extern void          nwamui_object_set_handle(NwamuiObject *object, const gpointer handle);
extern const gchar*  nwamui_object_get_name(NwamuiObject *object);
extern gboolean      nwamui_object_can_rename (NwamuiObject *object);
extern gboolean      nwamui_object_set_name(NwamuiObject *object, const gchar* name);
extern void          nwamui_object_set_conditions ( NwamuiObject *object, const GList* conditions );
extern GList*        nwamui_object_get_conditions ( NwamuiObject *object );
extern gint          nwamui_object_get_activation_mode(NwamuiObject *object);
extern void          nwamui_object_set_activation_mode(NwamuiObject *object, gint activation_mode);
extern gboolean      nwamui_object_get_active(NwamuiObject *object);
extern void          nwamui_object_set_active(NwamuiObject *object, gboolean active);
extern gboolean      nwamui_object_get_enabled(NwamuiObject *object);
extern void          nwamui_object_set_enabled(NwamuiObject *object, gboolean enabled);
extern nwam_state_t  nwamui_object_get_nwam_state(NwamuiObject *object, nwam_aux_state_t* aux_state, const gchar**aux_state_string);
extern void          nwamui_object_set_nwam_state(NwamuiObject *object, nwam_state_t state, nwam_aux_state_t aux_state);
extern gint          nwamui_object_sort(NwamuiObject *object, NwamuiObject *other, guint sort_by);
extern gint          nwamui_object_sort_by_name(NwamuiObject *object, NwamuiObject *other);
extern gboolean      nwamui_object_commit(NwamuiObject *object);
extern void          nwamui_object_reload(NwamuiObject *object);
extern gboolean      nwamui_object_destroy(NwamuiObject *object);
extern gboolean      nwamui_object_is_modifiable(NwamuiObject *object);
extern gboolean      nwamui_object_has_modifications(NwamuiObject *object);
extern NwamuiObject* nwamui_object_clone(NwamuiObject *object, const gchar *name, NwamuiObject *parent);

G_END_DECLS

#endif	/* _NWAMUI_OBJECT_H */

