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
 * File:   nwamui_env.h
 *
 */

#ifndef _NWAMUI_ENV_H
#define	_NWAMUI_ENV_H

#ifndef _libnwamui_H
#error "Please include libnwamui.h header instead."
#endif

#include <libnwam.h>

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
	NwamuiObject                      object;

	/*< private >*/
	NwamuiEnvPrivate    *prv;
};

struct _NwamuiEnvClass
{
	NwamuiObjectClass                parent_class;
};

extern  GType                   nwamui_env_get_type (void) G_GNUC_CONST;

typedef enum {
    NWAMUI_ENV_PROXY_TYPE_DIRECT = 1,
    NWAMUI_ENV_PROXY_TYPE_MANUAL,
    NWAMUI_ENV_PROXY_TYPE_PAC_FILE,
    NWAMUI_ENV_PROXY_TYPE_LAST /* Not to be used directly */
} nwamui_env_proxy_type_t;

typedef enum {
    NWAMUI_ENV_STATUS_CONNECTED,
    NWAMUI_ENV_STATUS_WARNING,
    NWAMUI_ENV_STATUS_ERROR,
    NWAMUI_ENV_STATUS_LAST /* Not to be used directly */
} nwamui_env_status_t; /* TODO - provide means to get status in env */

typedef enum {
	NWAMUI_ENV_NAMESERVICES_DNS   = NWAM_NAMESERVICES_DNS,
	NWAMUI_ENV_NAMESERVICES_FILES = NWAM_NAMESERVICES_FILES,
	NWAMUI_ENV_NAMESERVICES_NIS   = NWAM_NAMESERVICES_NIS,
	NWAMUI_ENV_NAMESERVICES_LDAP  = NWAM_NAMESERVICES_LDAP,
	NWAMUI_ENV_NAMESERVICES_LAST /* Not to be used directly */
} nwamui_env_nameservices_t;

typedef enum {
    NWAMUI_ENV_CONFIG_SOURCE_MANUAL = NWAM_CONFIGSRC_MANUAL,
    NWAMUI_ENV_CONFIG_SOURCE_DHCP   = NWAM_CONFIGSRC_DHCP,
    NWAMUI_ENV_CONFIG_SOURCE_LAST
} nwamui_env_config_source_t;

extern NwamuiEnv*           nwamui_env_new ( gchar* name );

extern NwamuiEnv*			nwamui_env_new_with_handle (nwam_loc_handle_t envh);

extern gboolean             nwamui_env_update_with_handle (NwamuiEnv* self, nwam_loc_handle_t envh);

extern NwamuiEnv*           nwamui_env_clone( NwamuiEnv* self );

extern void                 nwamui_env_set_name ( NwamuiEnv *self, const gchar* name );
extern gchar*               nwamui_env_get_name ( NwamuiEnv *self );

extern void                 nwamui_env_set_activation_mode ( NwamuiEnv *self, 
                                                             nwamui_cond_activation_mode_t activation_mode );
extern nwamui_cond_activation_mode_t 
                            nwamui_env_get_activation_mode ( NwamuiEnv *self );

extern void                 nwamui_env_set_conditions ( NwamuiEnv *self, const GList* conditions );
extern GList*               nwamui_env_get_conditions ( NwamuiEnv *self );

extern gboolean             nwamui_env_is_active (NwamuiEnv *self);

extern void                 nwamui_env_set_enabled ( NwamuiEnv *self, gboolean enabled );
extern gboolean             nwamui_env_get_enabled ( NwamuiEnv *self );


extern void                 nwamui_env_set_nameservices ( NwamuiEnv *self, const GList*   nameservices );
extern GList*               nwamui_env_get_nameservices ( NwamuiEnv *self );


extern void                 nwamui_env_set_nameservices_config_file ( NwamuiEnv *self, const gchar* nameservices_config_file );
extern gchar*               nwamui_env_get_nameservices_config_file ( NwamuiEnv *self );


extern void                 nwamui_env_set_default_domainname ( NwamuiEnv *self, const gchar* default_domainname );
extern gchar*               nwamui_env_get_default_domainname ( NwamuiEnv *self );

extern void                 nwamui_env_set_dns_nameservice_domain ( NwamuiEnv *self, const gchar* dns_nameservice_domain );
extern gchar*               nwamui_env_get_dns_nameservice_domain ( NwamuiEnv *self );

extern void                 nwamui_env_set_dns_nameservice_config_source (  NwamuiEnv *self,
                                                                            nwamui_env_config_source_t dns_nameservice_config_source );
extern nwamui_env_config_source_t
                            nwamui_env_get_dns_nameservice_config_source (NwamuiEnv *self);

extern void                 nwamui_env_set_dns_nameservice_servers ( NwamuiEnv *self, const GList*   dns_nameservice_servers );
extern GList*               nwamui_env_get_dns_nameservice_servers ( NwamuiEnv *self );


extern void                 nwamui_env_set_dns_nameservice_search ( NwamuiEnv *self, const GList* dns_nameservice_search );
extern GList*               nwamui_env_get_dns_nameservice_search ( NwamuiEnv *self );


extern void                 nwamui_env_set_nis_nameservice_config_source (  NwamuiEnv *self,
                                                                            nwamui_env_config_source_t nis_nameservice_config_source );
extern nwamui_env_config_source_t
                            nwamui_env_get_nis_nameservice_config_source (NwamuiEnv *self);

extern void                 nwamui_env_set_nis_nameservice_servers ( NwamuiEnv *self, const GList*   nis_nameservice_servers );
extern GList*               nwamui_env_get_nis_nameservice_servers ( NwamuiEnv *self );


extern void                 nwamui_env_set_ldap_nameservice_config_source (  NwamuiEnv *self,
                                                                            nwamui_env_config_source_t ldap_nameservice_config_source );
extern nwamui_env_config_source_t
                            nwamui_env_get_ldap_nameservice_config_source (NwamuiEnv *self);

extern void                 nwamui_env_set_ldap_nameservice_servers ( NwamuiEnv *self, const GList*   ldap_nameservice_servers );
extern GList*               nwamui_env_get_ldap_nameservice_servers ( NwamuiEnv *self );


#ifndef _DISABLE_HOSTS_FILE
extern void                 nwamui_env_set_hosts_file ( NwamuiEnv *self, const gchar* hosts_file );
extern gchar*               nwamui_env_get_hosts_file ( NwamuiEnv *self );
#endif /* _DISABLE_HOSTS_FILE */


extern void                 nwamui_env_set_nfsv4_domain ( NwamuiEnv *self, const gchar* nfsv4_domain );
extern gchar*               nwamui_env_get_nfsv4_domain ( NwamuiEnv *self );


extern void                 nwamui_env_set_ipfilter_config_file ( NwamuiEnv *self, const gchar* ipfilter_config_file );
extern gchar*               nwamui_env_get_ipfilter_config_file ( NwamuiEnv *self );


extern void                 nwamui_env_set_ipfilter_v6_config_file ( NwamuiEnv *self, const gchar* ipfilter_v6_config_file );
extern gchar*               nwamui_env_get_ipfilter_v6_config_file ( NwamuiEnv *self );


extern void                 nwamui_env_set_ipnat_config_file ( NwamuiEnv *self, const gchar* ipnat_config_file );
extern gchar*               nwamui_env_get_ipnat_config_file ( NwamuiEnv *self );


extern void                 nwamui_env_set_ippool_config_file ( NwamuiEnv *self, const gchar* ippool_config_file );
extern gchar*               nwamui_env_get_ippool_config_file ( NwamuiEnv *self );


extern void                 nwamui_env_set_ike_config_file ( NwamuiEnv *self, const gchar* ike_config_file );
extern gchar*               nwamui_env_get_ike_config_file ( NwamuiEnv *self );


extern void                 nwamui_env_set_ipsecpolicy_config_file ( NwamuiEnv *self, const gchar* ipsecpolicy_config_file );
extern gchar*               nwamui_env_get_ipsecpolicy_config_file ( NwamuiEnv *self );


extern void                 nwamui_env_set_svcs_enable ( NwamuiEnv *self, const GList*   svcs_enable );
extern GList*               nwamui_env_get_svcs_enable ( NwamuiEnv *self );


extern void                 nwamui_env_set_svcs_disable ( NwamuiEnv *self, const GList*   svcs_disable );
extern GList*               nwamui_env_get_svcs_disable ( NwamuiEnv *self );

#ifdef ENABLE_PROXY
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

extern void                 nwamui_env_set_proxy_username ( NwamuiEnv *self, const gchar* proxy_username );
extern gchar*               nwamui_env_get_proxy_username ( NwamuiEnv *self );

extern void                 nwamui_env_set_proxy_password ( NwamuiEnv *self, const gchar* proxy_password );
extern gchar*               nwamui_env_get_proxy_password ( NwamuiEnv *self );
#endif /* ENABLE_PROXY */

#if 0
extern GtkTreeModel *       nwamui_env_get_svcs (NwamuiEnv *self);

extern NwamuiSvc*           nwamui_env_svc_add (NwamuiEnv *self, nwam_loc_prop_template_t svc);

extern void                 nwamui_env_svc_add_full (NwamuiEnv *self,
  nwam_loc_prop_template_t svc,
  gboolean is_default,
  gboolean status);
extern void                 nwamui_env_svc_remove (NwamuiEnv *self, GtkTreeIter *iter);

extern void                 nwamui_env_svc_foreach (NwamuiEnv *self, GtkTreeModelForeachFunc func, gpointer data);
#endif

extern gboolean             nwamui_env_activate (NwamuiEnv *self);

extern gboolean             nwamui_env_has_modifications( NwamuiEnv* self );
extern gboolean             nwamui_env_commit( NwamuiEnv* self );
extern gboolean             nwamui_env_validate( NwamuiEnv* self, gchar **prop_name_ret );
extern void                 nwamui_env_reload( NwamuiEnv* self );

const gchar*                nwam_nameservices_enum_to_string(nwam_nameservices_t ns);

G_END_DECLS

#endif	/* _NWAMUI_ENV_H */

