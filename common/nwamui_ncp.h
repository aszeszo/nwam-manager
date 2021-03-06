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
 * File:   nwamui_ncp.h
 *
 */

#ifndef _NWAMUI_NCP_H
#define	_NWAMUI_NCP_H

#ifndef _libnwamui_H
#error "Please include libnwamui.h header instead."
#endif

G_BEGIN_DECLS

#define NWAMUI_TYPE_NCP               (nwamui_ncp_get_type ())
#define NWAMUI_NCP(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), NWAMUI_TYPE_NCP, NwamuiNcp))
#define NWAMUI_NCP_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), NWAMUI_TYPE_NCP, NwamuiNcpClass))
#define NWAMUI_IS_NCP(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NWAMUI_TYPE_NCP))
#define NWAMUI_IS_NCP_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), NWAMUI_TYPE_NCP))
#define NWAMUI_NCP_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), NWAMUI_TYPE_NCP, NwamuiNcpClass))


typedef struct _NwamuiNcp	      NwamuiNcp;
typedef struct _NwamuiNcpClass        NwamuiNcpClass;
typedef struct _NwamuiNcpPrivate      NwamuiNcpPrivate;

struct _NwamuiNcp
{
	NwamuiObject                      object;

	/*< private >*/
	NwamuiNcpPrivate            *prv;
};

struct _NwamuiNcpClass
{
	NwamuiObjectClass parent_class;
};



extern  GType                   nwamui_ncp_get_type (void) G_GNUC_CONST;

extern NwamuiObject*            nwamui_ncp_new(const gchar* name );

extern NwamuiObject*            nwamui_ncp_new_with_handle (nwam_ncp_handle_t ncp);

extern nwam_ncp_handle_t        nwamui_ncp_get_nwam_handle( NwamuiNcp* self );

extern gboolean                 nwamui_ncp_all_ncus_online (NwamuiNcp       *self,
                                                            NwamuiNcu      **needs_wifi_selection,
                                                            NwamuiWifiNet  **needs_wifi_key );

extern gint                     nwamui_ncp_get_ncu_num(NwamuiNcp *self);

extern  GList*                  nwamui_ncp_find_ncu(NwamuiNcp *self, GCompareFunc func, gconstpointer data);

extern	GtkListStore*           nwamui_ncp_get_ncu_list_store ( NwamuiNcp *self );

extern  NwamuiObject*           nwamui_ncp_get_ncu_by_device_name( NwamuiNcp *self, const gchar* device_name );

extern void                     nwamui_ncp_foreach_ncu(NwamuiNcp *self, GFunc func, gpointer user_data);

extern void                     nwamui_ncp_foreach_ncu_foreach_wifi_info(NwamuiNcp *self, GHFunc func, gpointer user_data);

extern GList*                   nwamui_ncp_get_wireless_ncus( NwamuiNcp* self );

extern NwamuiNcu*               nwamui_ncp_get_first_wireless_ncu(NwamuiNcp *self);

extern gint                     nwamui_ncp_get_wireless_link_num( NwamuiNcp* self );

extern gint64                   nwamui_ncp_get_prio_group( NwamuiNcp* self );

extern void                     nwamui_ncp_set_prio_group( NwamuiNcp* self, gint64 new_prio );

extern void                     nwamui_ncp_freeze_notify_ncus( NwamuiNcp* self );

extern void                     nwamui_ncp_thaw_notify_ncus( NwamuiNcp* self );


G_END_DECLS

#endif	/* _NWAMUI_NCP_H */

