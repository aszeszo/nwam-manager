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
 * File:   nwamui_env.h
 *
 */

#ifndef _NWAMUI_ENV_H
#define	_NWAMUI_ENV_H

#ifndef _libnwamui_H
#error "Please include libnwamui.h header instead."
#endif

G_BEGIN_DECLS

#define NWAMUI_TYPE_ENV               (nwamui_env_get_type ())
#define NWAMUI_ENV(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), NWAMUI_TYPE_ENV, NwamuiEnv))
#define NWAMUI_ENV_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), NWAMUI_TYPE_ENV, NwamuiEnvClass))
#define NWAMUI_IS_ENV(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NWAMUI_TYPE_ENV))
#define NWAMUI_IS_ENV_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), NWAMUI_TYPE_ENV))
#define NWAMUI_ENV_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), NWAMUI_TYPE_ENV, NwamuiEnvClass))


typedef struct _NwamuiEnv		      NwamuiEnv;
typedef struct _NwamuiEnvClass        NwamuiEnvClass;
typedef struct _NwamuiEnvPrivate	  NwamuiEnvPrivate;

struct _NwamuiEnv
{
	GObject                      object;

	/*< private >*/
	NwamuiEnvPrivate    *prv;
};

struct _NwamuiEnvClass
{
	GObjectClass                parent_class;
};

extern  GType                   nwamui_env_get_type (void) G_GNUC_CONST;

typedef enum {
    NWAMUI_ENV_PROXY_TYPE_DIRECT = 1,
    NWAMUI_ENV_PROXY_TYPE_MANUAL,
    NWAMUI_ENV_PROXY_TYPE_PAC_FILE,
    NWAMUI_ENV_PROXY_TYPE_LAST /* Not to be used directly */
} nwamui_env_proxy_type_t;

extern  NwamuiEnv*          nwamui_env_new ( gchar* name );

extern  NwamuiEnv*          nwamui_env_clone( NwamuiEnv* self );

extern void                 nwamui_env_set_name ( NwamuiEnv *self, const gchar* name );
extern gchar*               nwamui_env_get_name ( NwamuiEnv *self );

extern void                 nwamui_env_set_modifiable ( NwamuiEnv *self, gboolean modifiable );
extern gboolean             nwamui_env_is_modifiable ( NwamuiEnv *self );

extern void                     nwamui_env_set_proxy_type ( NwamuiEnv *self, nwamui_env_proxy_type_t proxy_type );
extern nwamui_env_proxy_type_t  nwamui_env_get_proxy_type ( NwamuiEnv *self );


extern void                 nwamui_env_set_proxy_pac_file ( NwamuiEnv *self, const gchar* proxy_pac_file );
extern gchar*               nwamui_env_get_proxy_pac_file ( NwamuiEnv *self );

extern void                 nwamui_env_set_use_http_proxy_for_all ( NwamuiEnv *self, gboolean use_http_proxy_for_all );
extern gboolean             nwamui_env_get_use_http_proxy_for_all ( NwamuiEnv *self );

extern void                 nwamui_env_set_proxy_http_server ( NwamuiEnv *self, const gchar* proxy_http_server );
extern gchar*               nwamui_env_get_proxy_http_server ( NwamuiEnv *self );


extern void                 nwamui_env_set_proxy_https_server ( NwamuiEnv *self, const gchar* proxy_https_server );
extern gchar*               nwamui_env_get_proxy_https_server ( NwamuiEnv *self );


extern void                 nwamui_env_set_proxy_ftp_server ( NwamuiEnv *self, const gchar* proxy_ftp_server );
extern gchar*               nwamui_env_get_proxy_ftp_server ( NwamuiEnv *self );


extern void                 nwamui_env_set_proxy_gopher_server ( NwamuiEnv *self, const gchar* proxy_gopher_server );
extern gchar*               nwamui_env_get_proxy_gopher_server ( NwamuiEnv *self );


extern void                 nwamui_env_set_proxy_socks_server ( NwamuiEnv *self, const gchar* proxy_socks_server );
extern gchar*               nwamui_env_get_proxy_socks_server ( NwamuiEnv *self );


extern void                 nwamui_env_set_proxy_bypass_list ( NwamuiEnv *self, const gchar* proxy_bypass_list );
extern gchar*               nwamui_env_get_proxy_bypass_list ( NwamuiEnv *self );

extern void                 nwamui_env_set_proxy_http_port ( NwamuiEnv *self, gint proxy_http_port );
extern gint                 nwamui_env_get_proxy_http_port ( NwamuiEnv *self );


extern void                 nwamui_env_set_proxy_https_port ( NwamuiEnv *self, gint proxy_https_port );
extern gint                 nwamui_env_get_proxy_https_port ( NwamuiEnv *self );

extern void                 nwamui_env_set_proxy_ftp_port ( NwamuiEnv *self, gint proxy_ftp_port );
extern gint                 nwamui_env_get_proxy_ftp_port ( NwamuiEnv *self );

extern void                 nwamui_env_set_proxy_gopher_port ( NwamuiEnv *self, gint proxy_gopher_port );
extern gint                 nwamui_env_get_proxy_gopher_port ( NwamuiEnv *self );

extern void                 nwamui_env_set_proxy_socks_port ( NwamuiEnv *self, gint proxy_socks_port );
extern gint                 nwamui_env_get_proxy_socks_port ( NwamuiEnv *self );

typedef nwam_cond;

G_END_DECLS

#endif	/* _NWAMUI_ENV_H */

