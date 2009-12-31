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
 * File:   nwamui_enm.h
 *
 */

#ifndef _NWAMUI_ENM_H
#define	_NWAMUI_ENM_H

#ifndef _libnwamui_H
#error "Please include libnwamui.h header instead."
#endif

G_BEGIN_DECLS

#define NWAMUI_TYPE_ENM               (nwamui_enm_get_type ())
#define NWAMUI_ENM(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), NWAMUI_TYPE_ENM, NwamuiEnm))
#define NWAMUI_ENM_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), NWAMUI_TYPE_ENM, NwamuiEnmClass))
#define NWAMUI_IS_ENM(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NWAMUI_TYPE_ENM))
#define NWAMUI_IS_ENM_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), NWAMUI_TYPE_ENM))
#define NWAMUI_ENM_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), NWAMUI_TYPE_ENM, NwamuiEnmClass))


typedef struct _NwamuiEnm	      NwamuiEnm;
typedef struct _NwamuiEnmClass        NwamuiEnmClass;
typedef struct _NwamuiEnmPrivate      NwamuiEnmPrivate;

struct _NwamuiEnm
{
	NwamuiObject                      object;

	/*< private >*/
	NwamuiEnmPrivate            *prv;
};

struct _NwamuiEnmClass
{
	NwamuiObjectClass                parent_class;
};



extern  GType               nwamui_enm_get_type (void) G_GNUC_CONST;

extern  NwamuiObject*       nwamui_enm_new (const gchar*    name );

extern  NwamuiObject*       nwamui_enm_new_with_handle (nwam_enm_handle_t enm);

extern gboolean             nwamui_enm_set_start_command ( NwamuiEnm *self, const gchar* start_command );
extern gchar*               nwamui_enm_get_start_command ( NwamuiEnm *self );


extern gboolean             nwamui_enm_set_stop_command ( NwamuiEnm *self, const gchar* stop_command );
extern gchar*               nwamui_enm_get_stop_command ( NwamuiEnm *self );


extern gboolean             nwamui_enm_set_smf_fmri ( NwamuiEnm *self, const gchar* smf_frmi );
extern gchar*               nwamui_enm_get_smf_fmri ( NwamuiEnm *self );

extern gboolean             nwamui_enm_has_modifications( NwamuiEnm* self );
extern gboolean             nwamui_enm_validate( NwamuiEnm* self, gchar **prop_name_ret );

G_END_DECLS

#endif	/* _NWAMUI_ENM_H */

