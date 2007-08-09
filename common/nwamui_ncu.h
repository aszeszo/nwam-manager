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
	GObject                      object;

	/*< private >*/
	NwamuiNcuPrivate    *prv;
};

struct _NwamuiNcuClass
{
	GObjectClass                parent_class;
};

extern  GType                   nwamui_ncu_get_type (void) G_GNUC_CONST;

typedef enum {
    NWAMUI_NCU_TYPE_WIRED = 1,
    NWAMUI_NCU_TYPE_WIRELESS,
    NWAMUI_NCU_TYPE_LAST /* Not to be used directly */
} nwamui_ncu_type_t;


extern  NwamuiNcu*          nwamui_ncu_new (    const gchar*        vanity_name,
                                                const gchar*        device_name,
                                                nwamui_ncu_type_t   ncu_type,
                                                gboolean            enabled,
                                                gboolean            ipv4_auto_conf,
                                                const gchar*        ipv4_address,
                                                const gchar*        ipv4_subnet,
                                                const gchar*        ipv4_gateway,
                                                gboolean            ipv6_active,
                                                gboolean            ipv6_auto_conf,
                                                const gchar*        ipv6_address,
                                                const gchar*        ipv6_prefix,
                                                NwamuiWifiNet*      wifi_info  );

extern gchar*               nwamui_ncu_get_vanity_name ( NwamuiNcu *self );
extern void                 nwamui_ncu_set_vanity_name ( NwamuiNcu *self, const gchar* name );

extern gchar*               nwamui_ncu_get_device_name ( NwamuiNcu *self );
extern void                 nwamui_ncu_set_device_name ( NwamuiNcu *self, const gchar* name );

extern gchar*               nwamui_ncu_get_display_name ( NwamuiNcu *self );

extern void                 nwamui_ncu_set_ncu_type ( NwamuiNcu *self, nwamui_ncu_type_t ncu_type );
extern nwamui_ncu_type_t    nwamui_ncu_get_ncu_type ( NwamuiNcu *self );


extern void                 nwamui_ncu_set_active ( NwamuiNcu *self, gboolean active );
extern gboolean             nwamui_ncu_get_active ( NwamuiNcu *self );


extern void                 nwamui_ncu_set_ipv4_auto_conf ( NwamuiNcu *self, gboolean ipv4_auto_conf );
extern gboolean             nwamui_ncu_get_ipv4_auto_conf ( NwamuiNcu *self );


extern void                 nwamui_ncu_set_ipv4_address ( NwamuiNcu *self, const gchar* ipv4_address );
extern gchar*               nwamui_ncu_get_ipv4_address ( NwamuiNcu *self );


extern void                 nwamui_ncu_set_ipv4_subnet ( NwamuiNcu *self, const gchar* ipv4_subnet );
extern gchar*               nwamui_ncu_get_ipv4_subnet ( NwamuiNcu *self );


extern void                 nwamui_ncu_set_ipv4_gateway ( NwamuiNcu *self, const gchar* ipv4_gateway );
extern gchar*               nwamui_ncu_get_ipv4_gateway ( NwamuiNcu *self );


extern void                 nwamui_ncu_set_ipv6_active ( NwamuiNcu *self, gboolean ipv6_active );
extern gboolean             nwamui_ncu_get_ipv6_active ( NwamuiNcu *self );


extern void                 nwamui_ncu_set_ipv6_auto_conf ( NwamuiNcu *self, gboolean ipv6_auto_conf );
extern gboolean             nwamui_ncu_get_ipv6_auto_conf ( NwamuiNcu *self );


extern void                 nwamui_ncu_set_ipv6_address ( NwamuiNcu *self, const gchar* ipv6_address );
extern gchar*               nwamui_ncu_get_ipv6_address ( NwamuiNcu *self );


extern void                 nwamui_ncu_set_ipv6_prefix ( NwamuiNcu *self, const gchar* ipv6_prefix );
extern gchar*               nwamui_ncu_get_ipv6_prefix ( NwamuiNcu *self );

extern NwamuiWifiNet*       nwamui_ncu_get_wifi_info ( NwamuiNcu *self );
extern void                 nwamui_ncu_set_wifi_info ( NwamuiNcu *self, NwamuiWifiNet* wifi_info );

extern nwamui_wifi_signal_strength_t    nwamui_ncu_get_wifi_signal_strength ( NwamuiNcu *self );

G_END_DECLS

#endif	/* _NWAMUI_NCU_H */

