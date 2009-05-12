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
 * File:   nwamui_ncu.h
 *
 */

#ifndef _NWAMUI_NCU_H
#define	_NWAMUI_NCU_H

#ifndef _libnwamui_H
#error "Please include libnwamui.h header instead."
#endif

G_BEGIN_DECLS

#define NWAMUI_TYPE_NCU               (nwamui_ncu_get_type ())
#define NWAMUI_NCU(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), NWAMUI_TYPE_NCU, NwamuiNcu))
#define NWAMUI_NCU_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), NWAMUI_TYPE_NCU, NwamuiNcuClass))
#define NWAMUI_IS_NCU(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NWAMUI_TYPE_NCU))
#define NWAMUI_IS_NCU_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), NWAMUI_TYPE_NCU))
#define NWAMUI_NCU_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), NWAMUI_TYPE_NCU, NwamuiNcuClass))


typedef struct _NwamuiNcu		  NwamuiNcu;
typedef struct _NwamuiNcuClass            NwamuiNcuClass;
typedef struct _NwamuiNcuPrivate	  NwamuiNcuPrivate;

struct _NwamuiNcu
{
	NwamuiObject                      object;

	/*< private >*/
	NwamuiNcuPrivate    *prv;
};

struct _NwamuiNcuClass
{
	NwamuiObjectClass                parent_class;
};

extern  GType                   nwamui_ncu_get_type (void) G_GNUC_CONST;

typedef enum {
    NWAMUI_NCU_TYPE_WIRED = 1,
    NWAMUI_NCU_TYPE_WIRELESS,
    NWAMUI_NCU_TYPE_TUNNEL,
    NWAMUI_NCU_TYPE_LAST /* Not to be used directly */
} nwamui_ncu_type_t;

/* NOTE: If updated please change status_string_fmt */
typedef enum {
    NWAMUI_STATE_UNKNOWN,
    NWAMUI_STATE_NOT_CONNECTED,
    NWAMUI_STATE_CONNECTING,
    NWAMUI_STATE_CONNECTED,
    NWAMUI_STATE_CONNECTING_ESSID,
    NWAMUI_STATE_CONNECTED_ESSID,
    NWAMUI_STATE_NETWORK_UNAVAILABLE,
    NWAMUI_STATE_CABLE_UNPLUGGED,
    NWAMUI_STATE_LAST   /* Not be used directly */
} nwamui_connection_state_t;

extern struct _NwamuiNcp;

extern NwamuiNcu*           nwamui_ncu_new_with_handle( struct _NwamuiNcp* ncp, nwam_ncu_handle_t ncu );
extern void                 nwamui_ncu_update_with_handle( NwamuiNcu* self, nwam_ncu_handle_t ncu   );

extern gboolean             nwamui_ncu_is_modifiable (NwamuiNcu *self);

extern gboolean             nwamui_ncu_has_modifications( NwamuiNcu* self );
extern gboolean             nwamui_ncu_validate( NwamuiNcu* self, gchar **prop_name_ret );
extern gboolean             nwamui_ncu_commit( NwamuiNcu* self );
extern void                 nwamui_ncu_reload( NwamuiNcu* self );

extern gchar*               nwamui_ncu_get_vanity_name ( NwamuiNcu *self );
extern void                 nwamui_ncu_set_vanity_name ( NwamuiNcu *self, const gchar* name );

extern gchar*               nwamui_ncu_get_device_name ( NwamuiNcu *self );
extern void                 nwamui_ncu_set_device_name ( NwamuiNcu *self, const gchar* name );

extern gchar*               nwamui_ncu_get_phy_address ( NwamuiNcu *self );
extern void                 nwamui_ncu_set_phy_address ( NwamuiNcu *self, const gchar* phy_address );

extern gchar*               nwamui_ncu_get_display_name ( NwamuiNcu *self );

extern void                 nwamui_ncu_set_ncu_type ( NwamuiNcu *self, nwamui_ncu_type_t ncu_type );
extern nwamui_ncu_type_t    nwamui_ncu_get_ncu_type ( NwamuiNcu *self );


extern void                 nwamui_ncu_set_active ( NwamuiNcu *self, gboolean active );
extern gboolean             nwamui_ncu_get_active ( NwamuiNcu *self );


extern guint                nwamui_ncu_get_speed ( NwamuiNcu *self );

extern void                 nwamui_ncu_set_ipv4_dhcp ( NwamuiNcu *self, gboolean ipv4_dhcp );
extern gboolean             nwamui_ncu_get_ipv4_dhcp ( NwamuiNcu *self );

extern void                 nwamui_ncu_set_ipv4_auto_conf ( NwamuiNcu *self, gboolean ipv4_auto_conf );
extern gboolean             nwamui_ncu_get_ipv4_auto_conf ( NwamuiNcu *self );


extern void                 nwamui_ncu_set_ipv4_address ( NwamuiNcu *self, const gchar* ipv4_address );
extern gchar*               nwamui_ncu_get_ipv4_address ( NwamuiNcu *self );


extern void                 nwamui_ncu_set_ipv4_subnet ( NwamuiNcu *self, const gchar* ipv4_subnet );
extern gchar*               nwamui_ncu_get_ipv4_subnet ( NwamuiNcu *self );


extern void                 nwamui_ncu_set_ipv6_active ( NwamuiNcu *self, gboolean ipv6_active );
extern gboolean             nwamui_ncu_get_ipv6_active ( NwamuiNcu *self );


extern void                 nwamui_ncu_set_ipv6_dhcp ( NwamuiNcu *self, gboolean ipv6_dhcp );
extern gboolean             nwamui_ncu_get_ipv6_dhcp ( NwamuiNcu *self );

extern void                 nwamui_ncu_set_ipv6_auto_conf ( NwamuiNcu *self, gboolean ipv6_auto_conf );
extern gboolean             nwamui_ncu_get_ipv6_auto_conf ( NwamuiNcu *self );


extern void                 nwamui_ncu_set_ipv6_address ( NwamuiNcu *self, const gchar* ipv6_address );
extern gchar*               nwamui_ncu_get_ipv6_address ( NwamuiNcu *self );


extern void                 nwamui_ncu_set_ipv6_prefix ( NwamuiNcu *self, const gchar* ipv6_prefix );
extern gchar*               nwamui_ncu_get_ipv6_prefix ( NwamuiNcu *self );

extern void                 nwamui_ncu_set_v4addresses ( NwamuiNcu *self, GtkListStore* v4addresses );
extern GtkListStore*        nwamui_ncu_get_v4addresses ( NwamuiNcu *self );


extern void                 nwamui_ncu_set_v6addresses ( NwamuiNcu *self, GtkListStore* v6addresses );
extern GtkListStore*        nwamui_ncu_get_v6addresses ( NwamuiNcu *self );

extern NwamuiWifiNet*       nwamui_ncu_get_wifi_info ( NwamuiNcu *self );
extern void                 nwamui_ncu_set_wifi_info ( NwamuiNcu *self, NwamuiWifiNet* wifi_info );

extern nwamui_wifi_signal_strength_t    
                            nwamui_ncu_get_wifi_signal_strength ( NwamuiNcu *self );

extern void                 nwamui_ncu_set_activation_mode ( NwamuiNcu *self, 
                                                              nwamui_cond_activation_mode_t activation_mode );
extern nwamui_cond_activation_mode_t 
                            nwamui_ncu_get_activation_mode ( NwamuiNcu *self );

extern void                 nwamui_ncu_set_enabled ( NwamuiNcu *self, gboolean enabled );
extern gboolean             nwamui_ncu_get_enabled ( NwamuiNcu *self );


extern void                 nwamui_ncu_set_priority_group ( NwamuiNcu *self, gint priority_group );
extern gint                 nwamui_ncu_get_priority_group ( NwamuiNcu *self );


extern void                 nwamui_ncu_set_priority_group_mode ( NwamuiNcu *self, 
                                                                 nwamui_cond_priority_group_mode_t priority_group_mode );
extern nwamui_cond_priority_group_mode_t 
                            nwamui_ncu_get_priority_group_mode ( NwamuiNcu *self );


extern nwamui_wifi_signal_strength_t nwamui_ncu_get_signal_strength_from_dladm( NwamuiNcu* self );

extern const gchar*         nwamui_ncu_get_signal_strength_string( NwamuiNcu* self );

extern nwamui_connection_state_t
                            nwamui_ncu_get_connection_state( NwamuiNcu* self );

extern gchar*               nwamui_ncu_get_connection_state_string( NwamuiNcu* self );

extern gchar*               nwamui_ncu_get_connection_state_detail_string( NwamuiNcu* ncu );

G_END_DECLS

#endif	/* _NWAMUI_NCU_H */

