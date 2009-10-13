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
 * File:   nwamui_prof.h
 *
 */

#ifndef _NWAMUI_PROF_H
#define	_NWAMUI_PROF_H

#ifndef _libnwamui_H
#error "Please include libnwamui.h header instead."
#endif

G_BEGIN_DECLS

#define NWAMUI_TYPE_PROF               (nwamui_prof_get_type ())
#define NWAMUI_PROF(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), NWAMUI_TYPE_PROF, NwamuiProf))
#define NWAMUI_PROF_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), NWAMUI_TYPE_PROF, NwamuiProfClass))
#define NWAMUI_IS_PROF(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NWAMUI_TYPE_PROF))
#define NWAMUI_IS_PROF_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), NWAMUI_TYPE_PROF))
#define NWAMUI_PROF_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), NWAMUI_TYPE_PROF, NwamuiProfClass))

typedef enum {
    NWAMUI_NO_FAV_ACTION_NONE = 0,
    NWAMUI_NO_FAV_ACTION_SHOW_LIST_DIALOG,
    NWAMUI_NO_FAV_ACTION_LAST           /* Not to be used directly */
} nwamui_action_on_no_fav_networks_t;

typedef struct _NwamuiProf		      NwamuiProf;
typedef struct _NwamuiProfClass       NwamuiProfClass;
typedef struct _NwamuiProfPrivate	  NwamuiProfPrivate;

struct _NwamuiProf
{
	NwamuiObject                      object;

	/*< private >*/
	NwamuiProfPrivate    *prv;
};

struct _NwamuiProfClass
{
	NwamuiObjectClass                parent_class;
};

extern GType                nwamui_prof_get_type (void) G_GNUC_CONST;

extern NwamuiProf*          nwamui_prof_get_instance ();

extern NwamuiProf*          nwamui_prof_get_instance_noref ();

extern void                 nwamui_prof_notify_begin (NwamuiProf* self);

extern void                 nwamui_prof_set_notification_default_timeout ( NwamuiProf *self, gint notification_default_timeout );

extern gint                 nwamui_prof_get_notification_default_timeout (NwamuiProf* self);

const gchar*                nwamui_prof_get_no_fav_action_string( nwamui_action_on_no_fav_networks_t action );

extern gboolean             nwamui_prof_get_notification_ncu_connected (NwamuiProf* self);

extern void                 nwamui_prof_set_notification_ncu_connected ( NwamuiProf *self, gboolean notification_ncu_connected );

extern gboolean             nwamui_prof_get_notification_ncu_disconnected (NwamuiProf* self);

extern void                 nwamui_prof_set_notification_ncu_disconnected ( NwamuiProf *self, gboolean notification_ncu_disconnected );


extern gboolean             nwamui_prof_get_notification_ncu_wifi_connect_failed (NwamuiProf* self);

extern void                 nwamui_prof_set_notification_ncu_wifi_connect_failed ( NwamuiProf *self, gboolean notification_ncu_wifi_connect_failed );

extern gboolean             nwamui_prof_get_notification_ncu_wifi_selection_needed (NwamuiProf* self);

extern void                 nwamui_prof_set_notification_ncu_wifi_selection_needed ( NwamuiProf *self, gboolean notification_ncu_wifi_selection_needed );


extern gboolean             nwamui_prof_get_notification_ncu_wifi_key_needed (NwamuiProf* self);

extern void                 nwamui_prof_set_notification_ncu_wifi_key_needed ( NwamuiProf *self, gboolean notification_ncu_wifi_key_needed );


extern gboolean             nwamui_prof_get_notification_no_wifi_networks (NwamuiProf* self);

extern void                 nwamui_prof_set_notification_no_wifi_networks ( NwamuiProf *self, gboolean notification_no_wifi_networks );


extern gboolean             nwamui_prof_get_notification_ncp_changed (NwamuiProf* self);

extern void                 nwamui_prof_set_notification_ncp_changed ( NwamuiProf *self, gboolean notification_ncp_changed );


extern gboolean             nwamui_prof_get_notification_location_changed (NwamuiProf* self);

extern void                 nwamui_prof_set_notification_location_changed ( NwamuiProf *self, gboolean notification_location_changed );


extern gboolean             nwamui_prof_get_notification_nwam_unavailable (NwamuiProf* self);

extern void                 nwamui_prof_set_notification_nwam_unavailable ( NwamuiProf *self, gboolean notification_nwam_unavailable );

G_END_DECLS

#endif	/* _NWAMUI_PROF_H */

