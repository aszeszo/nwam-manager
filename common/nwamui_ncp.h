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
    void (*activate_ncu) (NwamuiNcp *self, NwamuiNcu *ncu, gpointer user_data);
    void (*deactivate_ncu) (NwamuiNcp *self, NwamuiNcu *ncu, gpointer user_data);
	void (*add_ncu) (NwamuiNcp* ncp, NwamuiNcu* ncu, gpointer user_data);
	void (*remove_ncu) (NwamuiNcp* ncp, NwamuiNcu* ncu, gpointer user_data);
};



extern  GType                   nwamui_ncp_get_type (void) G_GNUC_CONST;

extern NwamuiNcp*               nwamui_ncp_new(const gchar* name );

extern NwamuiNcp*               nwamui_ncp_new_with_handle (nwam_ncp_handle_t ncp);

extern NwamuiNcp*               nwamui_ncp_clone( NwamuiNcp* self, const gchar* name );

extern nwam_ncp_handle_t        nwamui_ncp_get_nwam_handle( NwamuiNcp* self );

extern gboolean                 nwamui_ncp_rollback( NwamuiNcp* self );

extern gboolean                 nwamui_ncp_all_ncus_online (NwamuiNcp       *self,
                                                            NwamuiNcu      **needs_wifi_selection,
                                                            NwamuiWifiNet  **needs_wifi_key );

extern GList*                   nwamui_ncp_get_ncu_list( NwamuiNcp *self );

extern  void                    nwamui_ncp_foreach_ncu_list( NwamuiNcp *self, GFunc func, gpointer user_data );

extern  GList*                  nwamui_ncp_find_ncu_list( NwamuiNcp *self, gconstpointer data, GCompareFunc func);

extern	GtkListStore*           nwamui_ncp_get_ncu_list_store ( NwamuiNcp *self );

extern	GtkTreeStore*           nwamui_ncp_get_ncu_tree_store ( NwamuiNcp *self );

extern  NwamuiNcu*              nwamui_ncp_get_ncu_by_device_name( NwamuiNcp *self, const gchar* device_name );

extern  void                    nwamui_ncp_foreach_ncu( NwamuiNcp *self, GtkTreeModelForeachFunc func, gpointer user_data );

extern void                     nwamui_ncp_populate_ncu_list( NwamuiNcp* self, GObject* _daemon );

extern void                     nwamui_ncp_add_ncu( NwamuiNcp* self, NwamuiNcu* new_ncu );

extern void                     nwamui_ncp_remove_ncu_by_device_name( NwamuiNcp* self, const gchar* device_name );

extern void                     nwamui_ncp_remove_ncu( NwamuiNcp* self, NwamuiNcu* ncu );

extern GList*                   nwamui_ncp_get_wireless_ncus( NwamuiNcp* self );

extern gint                     nwamui_ncp_get_wireless_link_num( NwamuiNcp* self );

extern gint64                   nwamui_ncp_get_prio_group( NwamuiNcp* self );

extern void                     nwamui_ncp_set_prio_group( NwamuiNcp* self, gint64 new_prio );

extern void                     nwamui_ncp_freeze_notify_ncus( NwamuiNcp* self );

extern void                     nwamui_ncp_thaw_notify_ncus( NwamuiNcp* self );


G_END_DECLS

#endif	/* _NWAMUI_NCP_H */

