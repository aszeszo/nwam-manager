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
 * File:   nwamui_ip.h
 * Author: darrenk
 *
 */

#ifndef _NWAMUI_IP_H
#define	_NWAMUI_IP_H

#ifndef _libnwamui_H
#error "Please include libnwamui.h header instead."
#endif

G_BEGIN_DECLS

/* GObject Creation */

#define NWAMUI_TYPE_IP               (nwamui_ip_get_type ())
#define NWAMUI_IP(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), NWAMUI_TYPE_IP, NwamuiIp))
#define NWAMUI_IP_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), NWAMUI_TYPE_IP, NwamuiIpClass))
#define NWAMUI_IS_IP(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NWAMUI_TYPE_IP))
#define NWAMUI_IS_IP_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), NWAMUI_TYPE_IP))
#define NWAMUI_IP_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), NWAMUI_TYPE_IP, NwamuiIpClass))


typedef struct _NwamuiIp		NwamuiIp;
typedef struct _NwamuiIpClass   NwamuiIpClass;
typedef struct _NwamuiIpPrivate	NwamuiIpPrivate;

struct _NwamuiIp
{
	NwamuiObject                 object;

	/*< private >*/
	NwamuiIpPrivate        *prv;
};

struct _NwamuiIpClass
{
	NwamuiObjectClass            parent_class;
};

extern  GType               nwamui_ip_get_type (void) G_GNUC_CONST;

extern  NwamuiIp*            nwamui_ip_new(  NwamuiNcu*      ncu_parent,
                                             const gchar*    addr, 
                                             const gchar*    subnet_prefix, 
                                             gboolean        is_v6,
                                             gboolean        is_dhcp,
                                             gboolean        is_autoconf,
                                             gboolean        is_static );

extern void                 nwamui_ip_set_v6 ( NwamuiIp *self, gboolean is_v6 );
extern gboolean             nwamui_ip_is_v6  ( NwamuiIp *self );


extern void                 nwamui_ip_set_dhcp ( NwamuiIp *self, gboolean is_dhcp );
extern gboolean             nwamui_ip_is_dhcp  ( NwamuiIp *self );


extern void                 nwamui_ip_set_autoconf ( NwamuiIp *self, gboolean is_autoconf );
extern gboolean             nwamui_ip_is_autoconf  ( NwamuiIp *self );


extern void                 nwamui_ip_set_static ( NwamuiIp *self, gboolean is_static );
extern gboolean             nwamui_ip_is_static  ( NwamuiIp *self );


extern void                 nwamui_ip_set_address ( NwamuiIp *self, const gchar* address );
extern gchar*               nwamui_ip_get_address ( NwamuiIp *self );


extern void                 nwamui_ip_set_subnet_prefix ( NwamuiIp *self, const gchar* subnet_prefix );
extern gchar*               nwamui_ip_get_subnet_prefix ( NwamuiIp *self );


G_END_DECLS
        
#endif	/* _NWAMUI_IP_H */

