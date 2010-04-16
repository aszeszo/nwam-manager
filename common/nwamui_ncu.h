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

/* NCU */
/* Values picked to put always ON at top of list, and always off at bottom */
#define ALWAYS_ON_GROUP_ID              (0)
#define ALWAYS_OFF_GROUP_ID             (G_MAXINT)

typedef enum {
    NWAMUI_NCU_TYPE_WIRED = 1,
    NWAMUI_NCU_TYPE_WIRELESS,
#ifdef TUNNEL_SUPPORT
    NWAMUI_NCU_TYPE_TUNNEL,
#endif /* TUNNEL_SUPPORT */
    NWAMUI_NCU_TYPE_LAST /* Not to be used directly */
} nwamui_ncu_type_t;

/* NOTE: If updated please change status_string_fmt */
typedef enum {
    NWAMUI_STATE_UNKNOWN = 0,
    NWAMUI_STATE_DISABLED,
    NWAMUI_STATE_NOT_CONNECTED,
    NWAMUI_STATE_SCANNING,
    NWAMUI_STATE_NEEDS_SELECTION,
    NWAMUI_STATE_NEEDS_KEY_ESSID,
    NWAMUI_STATE_WAITING_FOR_ADDRESS,
    NWAMUI_STATE_DHCP_TIMED_OUT,
    NWAMUI_STATE_DHCP_DUPLICATE_ADDR,
    NWAMUI_STATE_CONNECTING,
    NWAMUI_STATE_CONNECTED,
    NWAMUI_STATE_CONNECTING_ESSID,
    NWAMUI_STATE_CONNECTED_ESSID,
    NWAMUI_STATE_NETWORK_UNAVAILABLE,
    NWAMUI_STATE_CABLE_UNPLUGGED,
    NWAMUI_STATE_LAST   /* Not be used directly */
} nwamui_connection_state_t;

extern struct _NwamuiNcp;

extern NwamuiObject*        nwamui_ncu_new_with_handle( struct _NwamuiNcp* ncp, nwam_ncu_handle_t ncu );

extern gboolean             nwamui_ncu_validate( NwamuiNcu* self, gchar **prop_name_ret );

extern gchar*               nwamui_ncu_get_device_name ( NwamuiNcu *self );
extern void                 nwamui_ncu_set_device_name ( NwamuiNcu *self, const gchar* name );

extern gchar*               nwamui_ncu_get_phy_address ( NwamuiNcu *self );
extern void                 nwamui_ncu_set_phy_address ( NwamuiNcu *self, const gchar* phy_address );

extern gchar*               nwamui_ncu_get_nwam_qualified_name ( NwamuiNcu *self );

extern const gchar*         nwamui_ncu_get_display_name ( NwamuiNcu *self );

extern void                 nwamui_ncu_set_ncu_type ( NwamuiNcu *self, nwamui_ncu_type_t ncu_type );
extern nwamui_ncu_type_t    nwamui_ncu_get_ncu_type ( NwamuiNcu *self );



extern guint                nwamui_ncu_get_speed ( NwamuiNcu *self );

extern void                 nwamui_ncu_set_ipv4_active ( NwamuiNcu *self, gboolean ipv4_active );
extern gboolean             nwamui_ncu_get_ipv4_active ( NwamuiNcu *self );

extern void                 nwamui_ncu_set_ipv4_has_dhcp ( NwamuiNcu *self, gboolean ipv4_has_dhcp );
extern gboolean             nwamui_ncu_get_ipv4_has_dhcp ( NwamuiNcu *self );

extern gboolean             nwamui_ncu_get_ipv4_has_static (NwamuiNcu *self);

extern gchar*               nwamui_ncu_get_ipv4_address ( NwamuiNcu *self );
extern void                 nwamui_ncu_set_ipv4_address(NwamuiNcu *self, const gchar *address);


extern gchar*               nwamui_ncu_get_ipv4_subnet ( NwamuiNcu *self );
extern void                 nwamui_ncu_set_ipv4_subnet(NwamuiNcu *self, const gchar* ipv4_subnet);

extern void                 nwamui_ncu_set_ipv4_default_route ( NwamuiNcu *self,
                                                                const gchar*  ipv4_default_route );
extern gchar*               nwamui_ncu_get_ipv4_default_route (NwamuiNcu *self);

extern void                 nwamui_ncu_set_ipv6_active ( NwamuiNcu *self, gboolean ipv6_active );
extern gboolean             nwamui_ncu_get_ipv6_active ( NwamuiNcu *self );


extern void                 nwamui_ncu_set_ipv6_has_dhcp ( NwamuiNcu *self, gboolean ipv6_has_dhcp );
extern gboolean             nwamui_ncu_get_ipv6_has_dhcp ( NwamuiNcu *self );

extern void                 nwamui_ncu_set_ipv6_has_auto_conf ( NwamuiNcu *self, gboolean ipv6_has_auto_conf );
extern gboolean             nwamui_ncu_get_ipv6_has_auto_conf ( NwamuiNcu *self );

extern gboolean             nwamui_ncu_get_ipv6_has_static (NwamuiNcu *self);

extern gchar*               nwamui_ncu_get_ipv6_address ( NwamuiNcu *self );

extern gchar*               nwamui_ncu_get_ipv6_prefix ( NwamuiNcu *self );

extern void                 nwamui_ncu_set_ipv6_default_route ( NwamuiNcu *self,
                                                                const gchar*  ipv6_default_route );
extern gchar*               nwamui_ncu_get_ipv6_default_route (NwamuiNcu *self);

extern void                 nwamui_ncu_set_v4addresses ( NwamuiNcu *self, GtkListStore* v4addresses );
extern GtkListStore*        nwamui_ncu_get_v4addresses ( NwamuiNcu *self );


extern void                 nwamui_ncu_set_v6addresses ( NwamuiNcu *self, GtkListStore* v6addresses );
extern GtkListStore*        nwamui_ncu_get_v6addresses ( NwamuiNcu *self );

extern NwamuiWifiNet*       nwamui_ncu_get_wifi_info ( NwamuiNcu *self );
extern void                 nwamui_ncu_set_wifi_info ( NwamuiNcu *self, NwamuiWifiNet* wifi_info );

extern nwamui_wifi_signal_strength_t    
                            nwamui_ncu_get_wifi_signal_strength ( NwamuiNcu *self );

extern void                 nwamui_ncu_set_priority_group ( NwamuiNcu *self, gint priority_group );
extern gint                 nwamui_ncu_get_priority_group ( NwamuiNcu *self );

extern gint                 nwamui_ncu_get_priority_group_for_view(NwamuiNcu *ncu);

extern void                 nwamui_ncu_set_priority_group_mode ( NwamuiNcu *self, 
                                                                 nwamui_cond_priority_group_mode_t priority_group_mode );
extern nwamui_cond_priority_group_mode_t 
                            nwamui_ncu_get_priority_group_mode ( NwamuiNcu *self );

extern void                 nwamui_ncu_wifi_hash_clean_dead_wifi_nets(NwamuiNcu *self);

extern void                 nwamui_ncu_wifi_hash_mark_each(NwamuiNcu *self, nwamui_wifi_life_state_t state);

extern NwamuiWifiNet*       nwamui_ncu_wifi_hash_lookup_by_essid( NwamuiNcu    *self, 
                                                                  const gchar  *essid );

extern void                 nwamui_ncu_wifi_hash_insert_wifi_net( NwamuiNcu     *self, 
                                                                  NwamuiWifiNet *wifi_net );

extern NwamuiWifiNet*       nwamui_ncu_wifi_hash_insert_or_update_from_wlan_t( NwamuiNcu    *self, 
                                                                               nwam_wlan_t  *wlan );

extern NwamuiWifiNet*       nwamui_ncu_wifi_hash_insert_or_update_from_handle( NwamuiNcu                 *self, 
                                                                               nwam_known_wlan_handle_t   wlan_h );

extern gboolean             nwamui_ncu_wifi_hash_remove_by_essid( NwamuiNcu     *self, 
                                                                  const gchar   *essid );

extern gboolean             nwamui_ncu_wifi_hash_remove_wifi_net( NwamuiNcu     *self, 
                                                                  NwamuiWifiNet *wifi_net );

extern void                 nwamui_ncu_wifi_hash_foreach(NwamuiNcu *self, GHFunc func, gpointer user_data);

extern nwamui_wifi_signal_strength_t nwamui_ncu_get_signal_strength_from_dladm( NwamuiNcu* self );

extern const gchar*         nwamui_ncu_get_signal_strength_string( NwamuiNcu* self );

extern nwamui_connection_state_t
                            nwamui_ncu_update_state( NwamuiNcu* self );

extern nwamui_connection_state_t
                            nwamui_ncu_get_connection_state( NwamuiNcu* self );

extern gchar*               nwamui_ncu_get_connection_state_string( NwamuiNcu* self );

extern gchar*               nwamui_ncu_get_connection_state_detail_string( NwamuiNcu* ncu, gboolean use_newline );

extern gchar*               nwamui_ncu_get_configuration_summary_string( NwamuiNcu* self );

extern nwam_state_t         nwamui_ncu_get_interface_nwam_state(NwamuiObject *object, nwam_aux_state_t* aux_state_p, const gchar**aux_state_string_p);

extern nwam_state_t         nwamui_ncu_get_link_nwam_state(NwamuiNcu *self, nwam_aux_state_t* aux_state, const gchar**aux_state_string);
extern void                 nwamui_ncu_set_link_nwam_state(NwamuiNcu *self, nwam_state_t state, nwam_aux_state_t aux_state);

G_END_DECLS

#endif	/* _NWAMUI_NCU_H */

