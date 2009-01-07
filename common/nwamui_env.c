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
 * File:   nwamui_env.c
 *
 */

#include <libnwam.h>
#include <glib-object.h>
#include <glib/gi18n.h>
#include <strings.h>
#include <stdlib.h>

#include "libnwamui.h"

static GObjectClass *parent_class = NULL;

enum {
    SVC_OBJECT = 0,
    SVC_N_COL,
};

struct _NwamuiEnvPrivate {
    gchar*                      name;
    gboolean                    modifiable;
    nwamui_cond_activation_mode_t
                                activation_mode;
    GList*                      conditions;
    nwam_loc_handle_t			nwam_env;
    gboolean                    enabled;
    gboolean                    nameservice_discover;
    GList*                      nameservices;
    gchar*                      nameservices_config_file;
    gchar*                      domainname;
    GList*                      dns_nameservice_servers;
    gchar*                      dns_nameservice_search;
    GList*                      nis_nameservice_servers;
    GList*                      nisplus_nameservice_servers;
    GList*                      ldap_nameservice_servers;
    gchar*                      hosts_file;
    gchar*                      nfsv4_domain;
    gchar*                      ipfilter_config_file;
    gchar*                      ipfilter_v6_config_file;
    gchar*                      ipnat_config_file;
    gchar*                      ippool_config_file;
    gchar*                      ike_config_file;
    gchar*                      ipseckey_config_file;
    gchar*                      ipsecpolicy_config_file;
    GList*                      svcs_enable;
    GList*                      svcs_disable;

    nwamui_env_proxy_type_t     proxy_type;
    gboolean                    use_http_proxy_for_all;
    gchar*                      proxy_pac_file;
    gchar*                      proxy_http_server;
    gchar*                      proxy_https_server;
    gchar*                      proxy_ftp_server;
    gchar*                      proxy_gopher_server;
    gchar*                      proxy_socks_server;
    gchar*                      proxy_bypass_list;
    gint                        proxy_http_port;
    gint                        proxy_https_port;
    gint                        proxy_ftp_port;
    gint                        proxy_gopher_port;
    gint                        proxy_socks_port;
    gchar*                      proxy_username;
    gchar*                      proxy_password;
    GtkListStore*               svcs_model;
    GtkListStore*               sys_svcs_model;

};

enum {
    PROP_NAME=1,
    PROP_MODIFIABLE,
    PROP_ENABLED,
    PROP_NAMESERVICE_DISCOVER,
    PROP_NAMESERVICES,
    PROP_NAMESERVICES_CONFIG_FILE,
    PROP_DOMAINNAME,
    PROP_DNS_NAMESERVICE_SERVERS,
    PROP_DNS_NAMESERVICE_SEARCH,
    PROP_NIS_NAMESERVICE_SERVERS,
    PROP_NISPLUS_NAMESERVICE_SERVERS,
    PROP_LDAP_NAMESERVICE_SERVERS,
    PROP_HOSTS_FILE,
    PROP_NFSV4_DOMAIN,
    PROP_IPFILTER_CONFIG_FILE,
    PROP_IPFILTER_V6_CONFIG_FILE,
    PROP_IPNAT_CONFIG_FILE,
    PROP_IPPOOL_CONFIG_FILE,
    PROP_IKE_CONFIG_FILE,
    PROP_IPSECKEY_CONFIG_FILE,
    PROP_IPSECPOLICY_CONFIG_FILE,
    PROP_SVCS_ENABLE,
    PROP_SVCS_DISABLE,
    PROP_CONDITIONS,
    PROP_ACTIVATION_MODE,
    PROP_NWAM_ENV,
    PROP_PROXY_TYPE,
    PROP_PROXY_PAC_FILE,
    PROP_USE_HTTP_PROXY_FOR_ALL,
    PROP_PROXY_HTTP_SERVER,
    PROP_PROXY_HTTPS_SERVER,
    PROP_PROXY_FTP_SERVER,
    PROP_PROXY_GOPHER_SERVER,
    PROP_PROXY_SOCKS_SERVER,
    PROP_PROXY_BYPASS_LIST,
    PROP_PROXY_HTTP_PORT,
    PROP_PROXY_HTTPS_PORT,
    PROP_PROXY_FTP_PORT,
    PROP_PROXY_GOPHER_PORT,
    PROP_PROXY_SOCKS_PORT,
    PROP_PROXY_USERNAME,
    PROP_PROXY_PASSWORD,
    PROP_SVCS
};

static void nwamui_env_set_property ( GObject         *object,
                                      guint            prop_id,
                                      const GValue    *value,
                                      GParamSpec      *pspec);

static void nwamui_env_get_property ( GObject         *object,
                                      guint            prop_id,
                                      GValue          *value,
                                      GParamSpec      *pspec);

static void nwamui_env_finalize (     NwamuiEnv *self);

static void populate_env_with_handle( NwamuiEnv* env, nwam_loc_handle_t nwam_loc );

#if 0
/* These are not needed right now since we don't support property templates,
 * but would like to keep around for when we do.
 */
static gboolean nwamui_env_svc_commit (NwamuiEnv *self, NwamuiSvc *svc);
#endif /* 0 */

/* Callbacks */
static void object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);
static void svc_row_inserted_or_changed_cb (GtkTreeModel *tree_model, GtkTreePath *path, GtkTreeIter *iter, gpointer user_data); 
static void svc_row_deleted_cb (GtkTreeModel *tree_model, GtkTreePath *path, gpointer user_data);

#if 0
/* These are not needed right now since we don't support property templates,
 * but would like to keep around for when we do.
 */
/* walkers */
static int nwam_loc_svc_walker_cb (nwam_loc_prop_template_t svc, void *data);
static int nwam_loc_sys_svc_walker_cb (nwam_loc_handle_t env, nwam_loc_prop_template_t svc, void *data);
#endif /* 0 */

G_DEFINE_TYPE (NwamuiEnv, nwamui_env, NWAMUI_TYPE_OBJECT)

static void
nwamui_env_class_init (NwamuiEnvClass *klass)
{
    /* Pointer to GObject Part of Class */
    GObjectClass *gobject_class = (GObjectClass*) klass;
        
    /* Initialise Static Parent Class pointer */
    parent_class = g_type_class_peek_parent (klass);

    /* Override Some Function Pointers */
    gobject_class->set_property = nwamui_env_set_property;
    gobject_class->get_property = nwamui_env_get_property;
    gobject_class->finalize = (void (*)(GObject*)) nwamui_env_finalize;

    /* Create some properties */
    g_object_class_install_property (gobject_class,
                                     PROP_NAME,
                                     g_param_spec_string ("name",
                                                          _("name"),
                                                          _("name"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_MODIFIABLE,
                                     g_param_spec_boolean ("modifiable",
                                                          _("modifiable"),
                                                          _("modifiable"),
                                                          TRUE,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_ACTIVATION_MODE,
                                     g_param_spec_int ("activation_mode",
                                                       _("activation_mode"),
                                                       _("activation_mode"),
                                                       NWAMUI_COND_ACTIVATION_MODE_MANUAL,
                                                       NWAMUI_COND_ACTIVATION_MODE_LAST-1,
                                                       NWAMUI_COND_ACTIVATION_MODE_MANUAL,
                                                       G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_ENABLED,
                                     g_param_spec_boolean ("enabled",
                                                          _("enabled"),
                                                          _("enabled"),
                                                          FALSE,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_NAMESERVICE_DISCOVER,
                                     g_param_spec_boolean ("nameservice_discover",
                                                          _("nameservice_discover"),
                                                          _("nameservice_discover"),
                                                          FALSE,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_NAMESERVICES,
                                     g_param_spec_pointer ("nameservices",
                                                          _("nameservices"),
                                                          _("nameservices"),
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_NAMESERVICES_CONFIG_FILE,
                                     g_param_spec_string ("nameservices_config_file",
                                                          _("nameservices_config_file"),
                                                          _("nameservices_config_file"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_DOMAINNAME,
                                     g_param_spec_string ("domainname",
                                                          _("domainname"),
                                                          _("domainname"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_DNS_NAMESERVICE_SERVERS,
                                     g_param_spec_pointer ("dns_nameservice_servers",
                                                          _("dns_nameservice_servers"),
                                                          _("dns_nameservice_servers"),
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_DNS_NAMESERVICE_SEARCH,
                                     g_param_spec_string ("dns_nameservice_search",
                                                          _("dns_nameservice_search"),
                                                          _("dns_nameservice_search"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_NIS_NAMESERVICE_SERVERS,
                                     g_param_spec_pointer ("nis_nameservice_servers",
                                                          _("nis_nameservice_servers"),
                                                          _("nis_nameservice_servers"),
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_NISPLUS_NAMESERVICE_SERVERS,
                                     g_param_spec_pointer ("nisplus_nameservice_servers",
                                                          _("nisplus_nameservice_servers"),
                                                          _("nisplus_nameservice_servers"),
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_LDAP_NAMESERVICE_SERVERS,
                                     g_param_spec_pointer ("ldap_nameservice_servers",
                                                          _("ldap_nameservice_servers"),
                                                          _("ldap_nameservice_servers"),
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_HOSTS_FILE,
                                     g_param_spec_string ("hosts_file",
                                                          _("hosts_file"),
                                                          _("hosts_file"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_NFSV4_DOMAIN,
                                     g_param_spec_string ("nfsv4_domain",
                                                          _("nfsv4_domain"),
                                                          _("nfsv4_domain"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_IPFILTER_CONFIG_FILE,
                                     g_param_spec_string ("ipfilter_config_file",
                                                          _("ipfilter_config_file"),
                                                          _("ipfilter_config_file"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_IPFILTER_V6_CONFIG_FILE,
                                     g_param_spec_string ("ipfilter_v6_config_file",
                                                          _("ipfilter_v6_config_file"),
                                                          _("ipfilter_v6_config_file"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_IPNAT_CONFIG_FILE,
                                     g_param_spec_string ("ipnat_config_file",
                                                          _("ipnat_config_file"),
                                                          _("ipnat_config_file"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_IPPOOL_CONFIG_FILE,
                                     g_param_spec_string ("ippool_config_file",
                                                          _("ippool_config_file"),
                                                          _("ippool_config_file"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_IKE_CONFIG_FILE,
                                     g_param_spec_string ("ike_config_file",
                                                          _("ike_config_file"),
                                                          _("ike_config_file"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_IPSECKEY_CONFIG_FILE,
                                     g_param_spec_string ("ipseckey_config_file",
                                                          _("ipseckey_config_file"),
                                                          _("ipseckey_config_file"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_IPSECPOLICY_CONFIG_FILE,
                                     g_param_spec_string ("ipsecpolicy_config_file",
                                                          _("ipsecpolicy_config_file"),
                                                          _("ipsecpolicy_config_file"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_SVCS_ENABLE,
                                     g_param_spec_pointer ("svcs_enable",
                                                          _("svcs_enable"),
                                                          _("svcs_enable"),
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_SVCS_DISABLE,
                                     g_param_spec_pointer ("svcs_disable",
                                                          _("svcs_disable"),
                                                          _("svcs_disable"),
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_CONDITIONS,
                                     g_param_spec_pointer ("conditions",
                                                          _("conditions"),
                                                          _("conditions"),
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_NWAM_ENV,
                                     g_param_spec_pointer ("nwam_env",
                                                          _("Nwam Env handle"),
                                                          _("Nwam Env handle"),
                                                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));


    g_object_class_install_property (gobject_class,
                                     PROP_PROXY_TYPE,
                                     g_param_spec_int ("proxy_type",
                                                       _("proxy_type"),
                                                       _("proxy_type"),
                                                       NWAMUI_ENV_PROXY_TYPE_DIRECT,
                                                       NWAMUI_ENV_PROXY_TYPE_LAST-1,
                                                       NWAMUI_ENV_PROXY_TYPE_DIRECT,
                                                       G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_PROXY_PAC_FILE,
                                     g_param_spec_string ("proxy_pac_file",
                                                          _("proxy_pac_file"),
                                                          _("proxy_pac_file"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_USE_HTTP_PROXY_FOR_ALL,
                                     g_param_spec_boolean ("use_http_proxy_for_all",
                                                          _("use_http_proxy_for_all"),
                                                          _("use_http_proxy_for_all"),
                                                          FALSE,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_PROXY_HTTP_SERVER,
                                     g_param_spec_string ("proxy_http_server",
                                                          _("proxy_http_server"),
                                                          _("proxy_http_server"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_PROXY_HTTPS_SERVER,
                                     g_param_spec_string ("proxy_https_server",
                                                          _("proxy_https_server"),
                                                          _("proxy_https_server"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_PROXY_FTP_SERVER,
                                     g_param_spec_string ("proxy_ftp_server",
                                                          _("proxy_ftp_server"),
                                                          _("proxy_ftp_server"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_PROXY_GOPHER_SERVER,
                                     g_param_spec_string ("proxy_gopher_server",
                                                          _("proxy_gopher_server"),
                                                          _("proxy_gopher_server"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_PROXY_SOCKS_SERVER,
                                     g_param_spec_string ("proxy_socks_server",
                                                          _("proxy_socks_server"),
                                                          _("proxy_socks_server"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_PROXY_BYPASS_LIST,
                                     g_param_spec_string ("proxy_bypass_list",
                                                          _("proxy_bypass_list"),
                                                          _("proxy_bypass_list"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_PROXY_HTTP_PORT,
                                     g_param_spec_int ("proxy_http_port",
                                                       _("proxy_http_port"),
                                                       _("proxy_http_port"),
                                                       0,
                                                       G_MAXINT,
                                                       80,
                                                       G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_PROXY_HTTPS_PORT,
                                     g_param_spec_int ("proxy_https_port",
                                                       _("proxy_https_port"),
                                                       _("proxy_https_port"),
                                                       0,
                                                       G_MAXINT,
                                                       80,
                                                       G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_PROXY_FTP_PORT,
                                     g_param_spec_int ("proxy_ftp_port",
                                                       _("proxy_ftp_port"),
                                                       _("proxy_ftp_port"),
                                                       0,
                                                       G_MAXINT,
                                                       80,
                                                       G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_PROXY_GOPHER_PORT,
                                     g_param_spec_int ("proxy_gopher_port",
                                                       _("proxy_gopher_port"),
                                                       _("proxy_gopher_port"),
                                                       0,
                                                       G_MAXINT,
                                                       80,
                                                       G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_PROXY_SOCKS_PORT,
                                     g_param_spec_int ("proxy_socks_port",
                                                       _("proxy_socks_port"),
                                                       _("proxy_socks_port"),
                                                       0,
                                                       G_MAXINT,
                                                       1080,
                                                       G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_PROXY_USERNAME,
                                     g_param_spec_string ("proxy_username",
                                                          _("proxy_username"),
                                                          _("proxy_username"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_PROXY_PASSWORD,
                                     g_param_spec_string ("proxy_password",
                                                          _("proxy_password"),
                                                          _("proxy_password"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_SVCS,
                                     g_param_spec_object ("svcs",
                                                          _("smf services"),
                                                          _("smf services"),
                                                          GTK_TYPE_TREE_MODEL,
                                                          G_PARAM_READABLE));
    
}


static void
nwamui_env_init (NwamuiEnv *self)
{
    self->prv = g_new0 (NwamuiEnvPrivate, 1);
    
    self->prv->name = NULL;
    self->prv->modifiable = TRUE;
    self->prv->activation_mode = NWAMUI_COND_ACTIVATION_MODE_MANUAL;
    self->prv->conditions = NULL;
    self->prv->enabled = FALSE;
    self->prv->nameservice_discover = FALSE;
    self->prv->nameservices = NULL;
    self->prv->nameservices_config_file = NULL;
    self->prv->domainname = NULL;
    self->prv->dns_nameservice_servers = NULL;
    self->prv->dns_nameservice_search = NULL;
    self->prv->nis_nameservice_servers = NULL;
    self->prv->nisplus_nameservice_servers = NULL;
    self->prv->ldap_nameservice_servers = NULL;
    self->prv->hosts_file = NULL;
    self->prv->nfsv4_domain = NULL;
    self->prv->ipfilter_config_file = NULL;
    self->prv->ipfilter_v6_config_file = NULL;
    self->prv->ipnat_config_file = NULL;
    self->prv->ippool_config_file = NULL;
    self->prv->ike_config_file = NULL;
    self->prv->ipseckey_config_file = NULL;
    self->prv->ipsecpolicy_config_file = NULL;
    self->prv->svcs_enable = NULL;
    self->prv->svcs_disable = NULL;

    self->prv->proxy_type = NWAMUI_ENV_PROXY_TYPE_DIRECT;
    self->prv->use_http_proxy_for_all = FALSE;
    self->prv->proxy_pac_file = NULL;
    self->prv->proxy_http_server = NULL;
    self->prv->proxy_https_server = NULL;
    self->prv->proxy_ftp_server = NULL;
    self->prv->proxy_gopher_server = NULL;
    self->prv->proxy_socks_server = NULL;
    self->prv->proxy_bypass_list = NULL;
    self->prv->proxy_http_port = 80;
    self->prv->proxy_https_port = 80;
    self->prv->proxy_ftp_port = 80;
    self->prv->proxy_gopher_port = 80;
    self->prv->proxy_socks_port = 1080;
    self->prv->proxy_username = NULL;
    self->prv->proxy_password = NULL;
    self->prv->svcs_model = gtk_list_store_new(SVC_N_COL, G_TYPE_OBJECT);


    g_signal_connect(G_OBJECT(self), "notify", (GCallback)object_notify_cb, (gpointer)self);
    g_signal_connect(G_OBJECT(self->prv->svcs_model), "row-deleted", (GCallback)svc_row_deleted_cb, (gpointer)self);
    g_signal_connect(G_OBJECT(self->prv->svcs_model), "row-changed", (GCallback)svc_row_inserted_or_changed_cb, (gpointer)self);
    g_signal_connect(G_OBJECT(self->prv->svcs_model), "row-inserted", (GCallback)svc_row_inserted_or_changed_cb, (gpointer)self);
}

static void
nwamui_env_set_property (   GObject         *object,
                            guint            prop_id,
                            const GValue    *value,
                            GParamSpec      *pspec)
{
    NwamuiEnv *self = NWAMUI_ENV(object);
    NwamuiEnvPrivate *prv = NWAMUI_ENV(object)->prv;
    gchar*      tmpstr = NULL;
    gint        tmpint = 0;
    nwam_error_t nerr;

    /* TODO - Handle modifiable in a "copy on write" fashion" */
    g_assert( self->prv->modifiable != FALSE );
    
    g_return_if_fail( self->prv->modifiable );
    
    switch (prop_id) {
        case PROP_NAME: {
                if ( self->prv->name != NULL ) {
                    g_free( self->prv->name );
                }
                self->prv->name = g_strdup( g_value_get_string( value ) );
                /* we may rename here */
                if (prv->nwam_env == NULL) {
                    nerr = nwam_loc_read (prv->name, 0, &prv->nwam_env);
                    if (nerr == NWAM_SUCCESS) {
                        g_debug ("nwamui_env_set_property found nwam_loc_handle %s", prv->name);
                    } else {
                        nerr = nwam_loc_create (self->prv->name, &self->prv->nwam_env);
                        g_assert (nerr == NWAM_SUCCESS);
                    }
                }
                nerr = nwam_loc_set_name (self->prv->nwam_env, self->prv->name);
                if (nerr != NWAM_SUCCESS) {
                    g_debug ("nwam_loc_set_name %s error: %s", self->prv->name, nwam_strerror (nerr));
                }
            }
            break;

        case PROP_MODIFIABLE: {
                self->prv->modifiable = g_value_get_boolean( value );
            }
            break;

        case PROP_ACTIVATION_MODE: {
                self->prv->activation_mode = (nwamui_cond_activation_mode_t)g_value_get_int( value );
            }
            break;

        case PROP_CONDITIONS: {
                if ( self->prv->conditions != NULL ) {
                        nwamui_util_free_obj_list( self->prv->conditions );
                }
                self->prv->conditions = nwamui_util_copy_obj_list((GList*)g_value_get_pointer( value ));
            }
            break;

        case PROP_NWAM_ENV: {
            g_assert (prv->nwam_env == NULL);
            prv->nwam_env = g_value_get_pointer (value);
            }
            break;

        case PROP_ENABLED: {
                self->prv->enabled = g_value_get_boolean( value );
            }
            break;

        case PROP_NAMESERVICE_DISCOVER: {
                self->prv->nameservice_discover = g_value_get_boolean( value );
            }
            break;

        case PROP_NAMESERVICES: {
                if ( self->prv->nameservices != NULL ) {
                        g_free( self->prv->nameservices );
                }
                self->prv->nameservices = g_value_get_pointer( value );
            }
            break;

        case PROP_NAMESERVICES_CONFIG_FILE: {
                if ( self->prv->nameservices_config_file != NULL ) {
                        g_free( self->prv->nameservices_config_file );
                }
                self->prv->nameservices_config_file = g_strdup( g_value_get_string( value ) );
            }
            break;

        case PROP_DOMAINNAME: {
                if ( self->prv->domainname != NULL ) {
                        g_free( self->prv->domainname );
                }
                self->prv->domainname = g_strdup( g_value_get_string( value ) );
            }
            break;

        case PROP_DNS_NAMESERVICE_SERVERS: {
                if ( self->prv->dns_nameservice_servers != NULL ) {
                        g_free( self->prv->dns_nameservice_servers );
                }
                self->prv->dns_nameservice_servers = g_value_get_pointer( value );
            }
            break;

        case PROP_DNS_NAMESERVICE_SEARCH: {
                if ( self->prv->dns_nameservice_search != NULL ) {
                        g_free( self->prv->dns_nameservice_search );
                }
                self->prv->dns_nameservice_search = g_strdup( g_value_get_string( value ) );
            }
            break;

        case PROP_NIS_NAMESERVICE_SERVERS: {
                if ( self->prv->nis_nameservice_servers != NULL ) {
                        g_free( self->prv->nis_nameservice_servers );
                }
                self->prv->nis_nameservice_servers = g_value_get_pointer( value );
            }
            break;

        case PROP_NISPLUS_NAMESERVICE_SERVERS: {
                if ( self->prv->nisplus_nameservice_servers != NULL ) {
                        g_free( self->prv->nisplus_nameservice_servers );
                }
                self->prv->nisplus_nameservice_servers = g_value_get_pointer( value );
            }
            break;

        case PROP_LDAP_NAMESERVICE_SERVERS: {
                if ( self->prv->ldap_nameservice_servers != NULL ) {
                        g_free( self->prv->ldap_nameservice_servers );
                }
                self->prv->ldap_nameservice_servers = g_value_get_pointer( value );
            }
            break;

        case PROP_HOSTS_FILE: {
                if ( self->prv->hosts_file != NULL ) {
                        g_free( self->prv->hosts_file );
                }
                self->prv->hosts_file = g_strdup( g_value_get_string( value ) );
            }
            break;

        case PROP_NFSV4_DOMAIN: {
                if ( self->prv->nfsv4_domain != NULL ) {
                        g_free( self->prv->nfsv4_domain );
                }
                self->prv->nfsv4_domain = g_strdup( g_value_get_string( value ) );
            }
            break;

        case PROP_IPFILTER_CONFIG_FILE: {
                if ( self->prv->ipfilter_config_file != NULL ) {
                        g_free( self->prv->ipfilter_config_file );
                }
                self->prv->ipfilter_config_file = g_strdup( g_value_get_string( value ) );
            }
            break;

        case PROP_IPFILTER_V6_CONFIG_FILE: {
                if ( self->prv->ipfilter_v6_config_file != NULL ) {
                        g_free( self->prv->ipfilter_v6_config_file );
                }
                self->prv->ipfilter_v6_config_file = g_strdup( g_value_get_string( value ) );
            }
            break;

        case PROP_IPNAT_CONFIG_FILE: {
                if ( self->prv->ipnat_config_file != NULL ) {
                        g_free( self->prv->ipnat_config_file );
                }
                self->prv->ipnat_config_file = g_strdup( g_value_get_string( value ) );
            }
            break;

        case PROP_IPPOOL_CONFIG_FILE: {
                if ( self->prv->ippool_config_file != NULL ) {
                        g_free( self->prv->ippool_config_file );
                }
                self->prv->ippool_config_file = g_strdup( g_value_get_string( value ) );
            }
            break;

        case PROP_IKE_CONFIG_FILE: {
                if ( self->prv->ike_config_file != NULL ) {
                        g_free( self->prv->ike_config_file );
                }
                self->prv->ike_config_file = g_strdup( g_value_get_string( value ) );
            }
            break;

        case PROP_IPSECKEY_CONFIG_FILE: {
                if ( self->prv->ipseckey_config_file != NULL ) {
                        g_free( self->prv->ipseckey_config_file );
                }
                self->prv->ipseckey_config_file = g_strdup( g_value_get_string( value ) );
            }
            break;

        case PROP_IPSECPOLICY_CONFIG_FILE: {
                if ( self->prv->ipsecpolicy_config_file != NULL ) {
                        g_free( self->prv->ipsecpolicy_config_file );
                }
                self->prv->ipsecpolicy_config_file = g_strdup( g_value_get_string( value ) );
            }
            break;

        case PROP_SVCS_ENABLE: {
                if ( self->prv->svcs_enable != NULL ) {
                        g_free( self->prv->svcs_enable );
                }
                self->prv->svcs_enable = g_value_get_pointer( value );
            }
            break;

        case PROP_SVCS_DISABLE: {
                if ( self->prv->svcs_disable != NULL ) {
                        g_free( self->prv->svcs_disable );
                }
                self->prv->svcs_disable = g_value_get_pointer( value );
            }
            break;

        case PROP_PROXY_TYPE: {
                self->prv->proxy_type = (nwamui_env_proxy_type_t)g_value_get_int( value );
            }
            break;

        case PROP_PROXY_PAC_FILE: {
                if ( self->prv->proxy_pac_file != NULL ) {
                        g_free( self->prv->proxy_pac_file );
                }
                self->prv->proxy_pac_file = g_strdup( g_value_get_string( value ) );
            }
            break;

        case PROP_USE_HTTP_PROXY_FOR_ALL: {
                self->prv->use_http_proxy_for_all = g_value_get_boolean( value );
            }
            break;

        case PROP_PROXY_HTTP_SERVER: {
                if ( self->prv->proxy_http_server != NULL ) {
                        g_free( self->prv->proxy_http_server );
                }
                self->prv->proxy_http_server = g_strdup( g_value_get_string( value ) );
            }
            break;

        case PROP_PROXY_HTTPS_SERVER: {
                if ( self->prv->proxy_https_server != NULL ) {
                        g_free( self->prv->proxy_https_server );
                }
                self->prv->proxy_https_server = g_strdup( g_value_get_string( value ) );
            }
            break;

        case PROP_PROXY_FTP_SERVER: {
                if ( self->prv->proxy_ftp_server != NULL ) {
                        g_free( self->prv->proxy_ftp_server );
                }
                self->prv->proxy_ftp_server = g_strdup( g_value_get_string( value ) );
            }
            break;

        case PROP_PROXY_GOPHER_SERVER: {
                if ( self->prv->proxy_gopher_server != NULL ) {
                        g_free( self->prv->proxy_gopher_server );
                }
                self->prv->proxy_gopher_server = g_strdup( g_value_get_string( value ) );
            }
            break;

        case PROP_PROXY_SOCKS_SERVER: {
                if ( self->prv->proxy_socks_server != NULL ) {
                        g_free( self->prv->proxy_socks_server );
                }
                self->prv->proxy_socks_server = g_strdup( g_value_get_string( value ) );
            }
            break;

        case PROP_PROXY_BYPASS_LIST: {
                if ( self->prv->proxy_bypass_list != NULL ) {
                        g_free( self->prv->proxy_bypass_list );
                }
                self->prv->proxy_bypass_list = g_strdup( g_value_get_string( value ) );
            }
            break;

        case PROP_PROXY_HTTP_PORT: {
                self->prv->proxy_http_port = g_value_get_int( value );
            }
            break;

        case PROP_PROXY_HTTPS_PORT: {
                self->prv->proxy_https_port = g_value_get_int( value );
            }
            break;

        case PROP_PROXY_FTP_PORT: {
                self->prv->proxy_ftp_port = g_value_get_int( value );
            }
            break;

        case PROP_PROXY_GOPHER_PORT: {
                self->prv->proxy_gopher_port = g_value_get_int( value );
            }
            break;

        case PROP_PROXY_SOCKS_PORT: {
                g_debug("socks_port = %d", self->prv->proxy_socks_port );
                self->prv->proxy_socks_port = g_value_get_int( value );
            }
            break;

        case PROP_PROXY_USERNAME: {
                if ( self->prv->proxy_username != NULL ) {
                        g_free( self->prv->proxy_username );
                }
                self->prv->proxy_username = g_strdup( g_value_get_string( value ) );
            }
            break;

        case PROP_PROXY_PASSWORD: {
                if ( self->prv->proxy_password != NULL ) {
                        g_free( self->prv->proxy_password );
                }
                self->prv->proxy_password = g_strdup( g_value_get_string( value ) );
            }
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
nwamui_env_get_property (GObject         *object,
                                  guint            prop_id,
                                  GValue          *value,
                                  GParamSpec      *pspec)
{
    NwamuiEnv *self = NWAMUI_ENV(object);
    nwam_error_t nerr;
    int num;
    nwam_value_t **nwamdata;

    switch (prop_id) {
        case PROP_NAME: {
                if (self->prv->name == NULL) {
                    char *name;
                    nerr = nwam_loc_get_name (self->prv->nwam_env, &name);
                    if (nerr != NWAM_SUCCESS) {
                        g_debug ("nwam_loc_get_name %s error: %s", self->prv->name, nwam_strerror (nerr));
                    }
                    if (g_ascii_strcasecmp (self->prv->name, name) != 0) {
                        g_assert_not_reached ();
                    }
                    free (name);
                }
                g_value_set_string( value, self->prv->name );
            }
            break;

        case PROP_MODIFIABLE: {
                g_value_set_boolean( value, self->prv->modifiable );
            }
            break;

        case PROP_ACTIVATION_MODE: {
                g_value_set_int( value, (gint)self->prv->activation_mode );
            }
            break;
            
        case PROP_CONDITIONS: {
                g_value_set_pointer( value, self->prv->conditions );
            }
            break;

        case PROP_ENABLED: {
                g_value_set_boolean( value, self->prv->enabled );
            }
            break;

        case PROP_NAMESERVICE_DISCOVER: {
                g_value_set_boolean( value, self->prv->nameservice_discover );
            }
            break;

        case PROP_NAMESERVICES: {
                g_value_set_pointer( value, self->prv->nameservices );
            }
            break;

        case PROP_NAMESERVICES_CONFIG_FILE: {
                g_value_set_string( value, self->prv->nameservices_config_file );
            }
            break;

        case PROP_DOMAINNAME: {
                g_value_set_string( value, self->prv->domainname );
            }
            break;

        case PROP_DNS_NAMESERVICE_SERVERS: {
                g_value_set_pointer( value, self->prv->dns_nameservice_servers );
            }
            break;

        case PROP_DNS_NAMESERVICE_SEARCH: {
                g_value_set_string( value, self->prv->dns_nameservice_search );
            }
            break;

        case PROP_NIS_NAMESERVICE_SERVERS: {
                g_value_set_pointer( value, self->prv->nis_nameservice_servers );
            }
            break;

        case PROP_NISPLUS_NAMESERVICE_SERVERS: {
                g_value_set_pointer( value, self->prv->nisplus_nameservice_servers );
            }
            break;

        case PROP_LDAP_NAMESERVICE_SERVERS: {
                g_value_set_pointer( value, self->prv->ldap_nameservice_servers );
            }
            break;

        case PROP_HOSTS_FILE: {
                g_value_set_string( value, self->prv->hosts_file );
            }
            break;

        case PROP_NFSV4_DOMAIN: {
                g_value_set_string( value, self->prv->nfsv4_domain );
            }
            break;

        case PROP_IPFILTER_CONFIG_FILE: {
                g_value_set_string( value, self->prv->ipfilter_config_file );
            }
            break;

        case PROP_IPFILTER_V6_CONFIG_FILE: {
                g_value_set_string( value, self->prv->ipfilter_v6_config_file );
            }
            break;

        case PROP_IPNAT_CONFIG_FILE: {
                g_value_set_string( value, self->prv->ipnat_config_file );
            }
            break;

        case PROP_IPPOOL_CONFIG_FILE: {
                g_value_set_string( value, self->prv->ippool_config_file );
            }
            break;

        case PROP_IKE_CONFIG_FILE: {
                g_value_set_string( value, self->prv->ike_config_file );
            }
            break;

        case PROP_IPSECKEY_CONFIG_FILE: {
                g_value_set_string( value, self->prv->ipseckey_config_file );
            }
            break;

        case PROP_IPSECPOLICY_CONFIG_FILE: {
                g_value_set_string( value, self->prv->ipsecpolicy_config_file );
            }
            break;

        case PROP_SVCS_ENABLE: {
                g_value_set_pointer( value, self->prv->svcs_enable );
            }
            break;

        case PROP_SVCS_DISABLE: {
                g_value_set_pointer( value, self->prv->svcs_disable );
            }
            break;

        case PROP_PROXY_TYPE: {
                g_value_set_int( value, (gint)self->prv->proxy_type );
            }
            break;

        case PROP_PROXY_PAC_FILE: {
                g_value_set_string( value, self->prv->proxy_pac_file );
            }
            break;

        case PROP_USE_HTTP_PROXY_FOR_ALL: {
                g_value_set_boolean( value, self->prv->use_http_proxy_for_all );
            }
            break;

        case PROP_PROXY_HTTP_SERVER: {
                g_value_set_string( value, self->prv->proxy_http_server );
            }
            break;

        case PROP_PROXY_HTTPS_SERVER: {
                g_value_set_string( value, self->prv->proxy_https_server );
            }
            break;

        case PROP_PROXY_FTP_SERVER: {
                g_value_set_string( value, self->prv->proxy_ftp_server );
            }
            break;

        case PROP_PROXY_GOPHER_SERVER: {
                g_value_set_string( value, self->prv->proxy_gopher_server );
            }
            break;

        case PROP_PROXY_SOCKS_SERVER: {
                g_value_set_string( value, self->prv->proxy_socks_server );
            }
            break;

        case PROP_PROXY_BYPASS_LIST: {
                g_value_set_string( value, self->prv->proxy_bypass_list );
            }
            break;

        case PROP_PROXY_HTTP_PORT: {
                g_value_set_int( value, self->prv->proxy_http_port );
            }
            break;

        case PROP_PROXY_HTTPS_PORT: {
                g_value_set_int( value, self->prv->proxy_https_port );
            }
            break;

        case PROP_PROXY_FTP_PORT: {
                g_value_set_int( value, self->prv->proxy_ftp_port );
            }
            break;

        case PROP_PROXY_GOPHER_PORT: {
                g_value_set_int( value, self->prv->proxy_gopher_port );
            }
            break;

        case PROP_PROXY_SOCKS_PORT: {
                g_value_set_int( value, self->prv->proxy_socks_port );
            }
            break;

        case PROP_PROXY_USERNAME: {
                g_value_set_string( value, self->prv->proxy_username );
            }
            break;

        case PROP_PROXY_PASSWORD: {
                g_value_set_string( value, self->prv->proxy_password );
            }
            break;

        case PROP_SVCS: {
                g_value_set_object (value, self->prv->svcs_model);
            }
            break;
            
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

/**
 * nwamui_env_new:
 * @returns: a new #NwamuiEnv.
 *
 * Creates a new #NwamuiEnv.
 **/
NwamuiEnv*
nwamui_env_new ( gchar* name )
{
    NwamuiEnv   *env = NWAMUI_ENV(g_object_new (NWAMUI_TYPE_ENV, NULL));
    
    g_object_set (G_OBJECT (env),
      "name", name,
      NULL);

    return( env );
}

/**
 * nwamui_env_new_with_handle:
 * @returns: a new #NwamuiEnv.
 *
 * Creates a new #NwamuiEnv.
 **/
NwamuiEnv*
nwamui_env_new_with_handle (nwam_loc_handle_t envh)
{
    NwamuiEnv          *env = NWAMUI_ENV(g_object_new (NWAMUI_TYPE_ENV,
                                    NULL));
    char               *name;
    NwamuiEnvPrivate   *prv = env->prv;

    {
        nwam_error_t nerr;
        int          rval;
        
        nerr = nwam_loc_get_name (envh, (char **)&name);
        if (nerr != NWAM_SUCCESS) {
            g_assert_not_reached ();
        }

        prv->name = g_strdup( name );

        nerr = nwam_loc_read (prv->name, 0, &prv->nwam_env);
        if (nerr != NWAM_SUCCESS) {
            g_error ("nwamui_env_new_with_handle failed to read nwam_loc_handle %s", prv->name);
            g_object_unref(env);
            return (NULL);
        }
    
        populate_env_with_handle( env, prv->nwam_env );

        g_debug ("loading nwamui_env_new_with_handle %s", name);

        free (name);

#if 0
/* These are not needed right now since we don't support property templates,
 * but would like to keep around for when we do.
 */
        nerr = nwam_walk_loc_prop_templates(nwam_loc_svc_walker_cb, env, 0, &rval );
        if (nerr != NWAM_SUCCESS) {
            g_debug ("[libnwam] nwamui_env_new_with_handle walk svc %s", nwam_strerror (nerr));
        }
#endif /* 0 */
    }
    
    return( env );
}

/**
 * nwamui_env_clone:
 * @returns: a copy of an existing #NwamuiEnv.
 *
 * Creates a new #NwamuiEnv and copies properties.
 *
 **/
extern NwamuiEnv*
nwamui_env_clone( NwamuiEnv* self ) 
{
    NwamuiEnv   *new_env;
    gchar       *new_name;
    nwam_error_t nerr;
    nwam_loc_handle_t new_env_h;
        
    nerr = nwam_loc_copy (self->prv->nwam_env, self->prv->name, &new_env_h);
    new_env = nwamui_env_new_with_handle (new_env_h);

    /* TODO - we may not have to set "name" again */
    /* Generate a new name */
    /* TODO - handle case where new name exists... (e.g. Copy(2) of ) */
    new_name = g_strdup_printf(_("Copy of %s"), self->prv->name );
    
    g_object_set (G_OBJECT (new_env),
            "name", new_name,
            "modifiable", TRUE, /* Should be modifiable, regardless of orig */
            "proxy_type", self->prv->proxy_type,
            "use_http_proxy_for_all", self->prv->use_http_proxy_for_all,
            "proxy_pac_file", self->prv->proxy_pac_file,
            "proxy_http_server", self->prv->proxy_http_server,
            "proxy_https_server", self->prv->proxy_https_server,
            "proxy_ftp_server", self->prv->proxy_ftp_server,
            "proxy_gopher_server", self->prv->proxy_gopher_server,
            "proxy_socks_server", self->prv->proxy_socks_server,
            "proxy_bypass_list", self->prv->proxy_bypass_list,
            "proxy_http_port", self->prv->proxy_http_port,
            "proxy_https_port", self->prv->proxy_https_port,
            "proxy_ftp_port", self->prv->proxy_ftp_port,
            "proxy_gopher_port", self->prv->proxy_gopher_port,
            "proxy_socks_port", self->prv->proxy_socks_port,
             NULL);
    
    g_free( new_name );
    
    return( new_env );
}

static gchar*
get_nwam_loc_string_prop( nwam_loc_handle_t loc, const char* prop_name )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    gchar*              retval = NULL;
    char*               value = NULL;

    g_return_val_if_fail( prop_name != NULL, retval );

    if ( (nerr = nwam_loc_get_prop_type( prop_name, &nwam_type ) ) != NWAM_SUCCESS 
         || nwam_type != NWAM_VALUE_TYPE_STRING ) {
        g_warning("Unexpected type for loc property %s\n", prop_name );
        return retval;
    }

    if ( (nerr = nwam_loc_get_prop_value (loc, prop_name, &nwam_data)) != NWAM_SUCCESS ) {
        g_debug("No value for loc property %s, error = %s\n", prop_name, nwam_strerror( nerr ) );
        return retval;
    }

    if ( (nerr = nwam_value_get_string(nwam_data, &value )) != NWAM_SUCCESS ) {
        g_debug("Unable to get string value for loc property %s, error = %s\n", prop_name, nwam_strerror( nerr ) );
        return retval;
    }

    if ( value != NULL ) {
        retval  = g_strdup ( value );
    }

    nwam_value_free(nwam_data);
    
    return( retval );
}

static gchar**
get_nwam_loc_string_array_prop( nwam_loc_handle_t loc, const char* prop_name )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    gchar**             retval = NULL;
    char**              value = NULL;
    uint_t              num = 0;

    g_return_val_if_fail( prop_name != NULL, retval );

    if ( (nerr = nwam_loc_get_prop_type( prop_name, &nwam_type ) ) != NWAM_SUCCESS 
         || nwam_type != NWAM_VALUE_TYPE_STRING ) {
        g_warning("Unexpected type for loc property %s\n", prop_name );
        return retval;
    }

    if ( (nerr = nwam_loc_get_prop_value (loc, prop_name, &nwam_data)) != NWAM_SUCCESS ) {
        g_debug("No value for loc property %s, error = %s\n", prop_name, nwam_strerror( nerr ) );
        return retval;
    }

    if ( (nerr = nwam_value_get_string_array(nwam_data, &value, &num )) != NWAM_SUCCESS ) {
        g_debug("Unable to get string value for loc property %s, error = %s\n", prop_name, nwam_strerror( nerr ) );
        return retval;
    }

    if ( value != NULL && num > 0 ) {
        /* Create a NULL terminated list of stirngs, allocate 1 extra place
         * for NULL termination. */
        retval = (gchar**)g_malloc0( sizeof(gchar*) * (num+1) );

        for (int i = 0; i < num; i++ ) {
            retval[i]  = g_strdup ( value[i] );
        }
        retval[num]=NULL;
    }

    nwam_value_free(nwam_data);
    
    return( retval );
}

static gboolean
get_nwam_loc_boolean_prop( nwam_loc_handle_t loc, const char* prop_name )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    boolean_t           value = FALSE;

    g_return_val_if_fail( prop_name != NULL, value );

    if ( (nerr = nwam_loc_get_prop_type( prop_name, &nwam_type ) ) != NWAM_SUCCESS 
         || nwam_type != NWAM_VALUE_TYPE_BOOLEAN ) {
        g_warning("Unexpected type for loc property %s\n", prop_name );
        return value;
    }

    if ( (nerr = nwam_loc_get_prop_value (loc, prop_name, &nwam_data)) != NWAM_SUCCESS ) {
        g_debug("No value for loc property %s, error = %s\n", prop_name, nwam_strerror( nerr ) );
        return value;
    }

    if ( (nerr = nwam_value_get_boolean(nwam_data, &value )) != NWAM_SUCCESS ) {
        g_debug("Unable to get boolean value for loc property %s, error = %s\n", prop_name, nwam_strerror( nerr ) );
        return value;
    }

    nwam_value_free(nwam_data);

    return( (gboolean)value );
}

static guint64
get_nwam_loc_uint64_prop( nwam_loc_handle_t loc, const char* prop_name )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    uint64_t            value = 0;

    g_return_val_if_fail( prop_name != NULL, value );

    if ( (nerr = nwam_loc_get_prop_type( prop_name, &nwam_type ) ) != NWAM_SUCCESS 
         || nwam_type != NWAM_VALUE_TYPE_UINT64 ) {
        g_warning("Unexpected type for loc property %s\n", prop_name );
        return value;
    }

    if ( (nerr = nwam_loc_get_prop_value (loc, prop_name, &nwam_data)) != NWAM_SUCCESS ) {
        g_debug("No value for loc property %s, error = %s\n", prop_name, nwam_strerror( nerr ) );
        return value;
    }

    if ( (nerr = nwam_value_get_uint64(nwam_data, &value )) != NWAM_SUCCESS ) {
        g_debug("Unable to get uint64 value for loc property %s, error = %s\n", prop_name, nwam_strerror( nerr ) );
        return value;
    }

    nwam_value_free(nwam_data);

    return( (guint64)value );
}

static guint64* 
get_nwam_loc_uint64_array_prop( nwam_loc_handle_t loc, const char* prop_name , guint *out_num )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    uint64_t           *value = NULL;
    uint_t              num = 0;
    guint64            *retval = NULL;

    g_return_val_if_fail( prop_name != NULL && out_num != NULL, retval );

    if ( (nerr = nwam_loc_get_prop_type( prop_name, &nwam_type ) ) != NWAM_SUCCESS 
         || nwam_type != NWAM_VALUE_TYPE_UINT64 ) {
        g_warning("Unexpected type for loc property %s\n", prop_name );
        return value;
    }

    if ( (nerr = nwam_loc_get_prop_value (loc, prop_name, &nwam_data)) != NWAM_SUCCESS ) {
        g_debug("No value for loc property %s, error = %s\n", prop_name, nwam_strerror( nerr ) );
        return value;
    }

    if ( (nerr = nwam_value_get_uint64_array(nwam_data, &value, &num )) != NWAM_SUCCESS ) {
        g_debug("Unable to get uint64 value for loc property %s, error = %s\n", prop_name, nwam_strerror( nerr ) );
        return value;
    }

    retval = (guint64*)g_malloc( sizeof(guint64) * num );
    for ( int i = 0; i < num; i++ ) {
        retval[i] = (guint64)value[i];
    }

    nwam_value_free(nwam_data);

    *out_num = num;

    return( retval );
}

static GList*
convert_name_services_array_to_glist( nwam_nameservices_t* ns_list, guint count )
{
    GList*  new_list = NULL;

    if ( ns_list != NULL ) {
        for ( int i = 0; i < count; i++ ) {
            /* Right now there is a 1-1 mapping between enums, so no real
             * conversion needed, just store the value */
            new_list = g_list_append( new_list, (gpointer)ns_list[i] );
        }
    }

    return( new_list );
}

static nwam_nameservices_t*
convert_name_services_glist_to_array( GList* ns_glist, guint *count )
{
    nwam_nameservices_t*    ns_list = NULL;

    if ( ns_glist != NULL && count != NULL ) {
        int     list_len = g_list_length( ns_glist );
        int     i = 0;

        ns_list = (nwam_nameservices_t*)g_malloc0( sizeof(nwam_nameservices_t*) * (list_len) );

        i = 0;
        for ( GList *element  = g_list_first( ns_glist );
              element != NULL && element->data != NULL;
              element = g_list_next( element ) ) {
            nwam_nameservices_t ns = (nwam_nameservices_t) element->data;
            /* Make sure it's a valid value */
            if ( ns >= NWAM_NAMESERVICES_DNS && ns <= NWAM_NAMESERVICES_LDAP ) {
                ns_list[i]  = (nwam_nameservices_t)ns;
                i++;
            }
            else {
                g_error("Got unexpected nameservice value %d", ns );
            }
        }
        *count = list_len;
    }

    return( ns_list );
}

static void 
populate_env_with_handle( NwamuiEnv* env, nwam_loc_handle_t nwam_loc )
{
    NwamuiEnvPrivate   *prv = env->prv;
    gchar             **condition_str = NULL;
    guint               num_nameservices = 0;
    nwam_nameservices_t *nameservices = NULL;

    prv->modifiable = !get_nwam_loc_boolean_prop( nwam_loc, NWAM_LOC_PROP_READ_ONLY );
    prv->activation_mode = (nwamui_cond_activation_mode_t)get_nwam_loc_uint64_prop( nwam_loc, NWAM_LOC_PROP_ACTIVATION_MODE );
    condition_str = get_nwam_loc_string_array_prop( nwam_loc, NWAM_LOC_PROP_CONDITION );
    prv->conditions = nwamui_util_map_condition_strings_to_object_list( condition_str);
    g_strfreev( condition_str );

    prv->enabled = get_nwam_loc_boolean_prop( nwam_loc, NWAM_LOC_PROP_ENABLED );

    /* Nameservice location properties */
    prv->nameservice_discover = get_nwam_loc_boolean_prop( nwam_loc, NWAM_LOC_PROP_NAMESERVICE_DISCOVER );
    nameservices = (nwam_nameservices_t*)get_nwam_loc_uint64_array_prop(
                                           nwam_loc, NWAM_LOC_PROP_NAMESERVICES, &num_nameservices );
    prv->nameservices = convert_name_services_array_to_glist( nameservices, num_nameservices );
    prv->nameservices_config_file = get_nwam_loc_string_prop( nwam_loc, NWAM_LOC_PROP_NAMESERVICES_CONFIG_FILE );
    prv->domainname = get_nwam_loc_string_prop( nwam_loc, NWAM_LOC_PROP_DOMAINNAME );
    prv->dns_nameservice_servers = nwamui_util_strv_to_glist(
            get_nwam_loc_string_array_prop( nwam_loc, NWAM_LOC_PROP_DNS_NAMESERVICE_SERVERS ) );
    prv->dns_nameservice_search = get_nwam_loc_string_prop( nwam_loc, NWAM_LOC_PROP_DNS_NAMESERVICE_SEARCH );
    prv->nis_nameservice_servers = nwamui_util_strv_to_glist(
        get_nwam_loc_string_array_prop( nwam_loc, NWAM_LOC_PROP_NIS_NAMESERVICE_SERVERS ) );
    prv->nisplus_nameservice_servers = nwamui_util_strv_to_glist(
        get_nwam_loc_string_array_prop( nwam_loc, NWAM_LOC_PROP_NISPLUS_NAMESERVICE_SERVERS ) );
    prv->ldap_nameservice_servers = nwamui_util_strv_to_glist(
        get_nwam_loc_string_array_prop( nwam_loc, NWAM_LOC_PROP_LDAP_NAMESERVICE_SERVERS ) );

    /* Path to hosts/ipnodes database */
    prv->hosts_file = get_nwam_loc_string_prop( nwam_loc, NWAM_LOC_PROP_HOSTS_FILE );

    /* NFSv4 domain */
    prv->nfsv4_domain = get_nwam_loc_string_prop( nwam_loc, NWAM_LOC_PROP_NFSV4_DOMAIN );

    /* IPfilter configuration */
    prv->ipfilter_config_file = get_nwam_loc_string_prop( nwam_loc, NWAM_LOC_PROP_IPFILTER_CONFIG_FILE );
    prv->ipfilter_v6_config_file = get_nwam_loc_string_prop( nwam_loc, NWAM_LOC_PROP_IPFILTER_V6_CONFIG_FILE );
    prv->ipnat_config_file = get_nwam_loc_string_prop( nwam_loc, NWAM_LOC_PROP_IPNAT_CONFIG_FILE );
    prv->ippool_config_file = get_nwam_loc_string_prop( nwam_loc, NWAM_LOC_PROP_IPPOOL_CONFIG_FILE );

    /* IPsec configuration */
    prv->ike_config_file = get_nwam_loc_string_prop( nwam_loc, NWAM_LOC_PROP_IKE_CONFIG_FILE );
    prv->ipseckey_config_file = get_nwam_loc_string_prop( nwam_loc, NWAM_LOC_PROP_IPSECKEY_CONFIG_FILE );
    prv->ipsecpolicy_config_file = get_nwam_loc_string_prop( nwam_loc, NWAM_LOC_PROP_IPSECPOLICY_CONFIG_FILE );

    /* List of SMF services to enable/disable */
    prv->svcs_enable = nwamui_util_strv_to_glist(
            get_nwam_loc_string_array_prop( nwam_loc, NWAM_LOC_PROP_SVCS_ENABLE ) );
    prv->svcs_disable = nwamui_util_strv_to_glist(
        get_nwam_loc_string_array_prop( nwam_loc, NWAM_LOC_PROP_SVCS_DISABLE ) );
}

    
/** 
 * nwamui_env_set_name:
 * @nwamui_env: a #NwamuiEnv.
 * @name: The name to set.
 * 
 **/ 
extern void
nwamui_env_set_name (   NwamuiEnv *self,
                              const gchar*  name )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));
    g_assert (name != NULL );

    if ( name != NULL ) {
        g_object_set (G_OBJECT (self),
                      "name", name,
                      NULL);
    }
}

/**
 * nwamui_env_get_name:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the name.
 *
 **/
extern gchar*
nwamui_env_get_name (NwamuiEnv *self)
{
    gchar*  name = NULL; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), name);

    g_object_get (G_OBJECT (self),
                  "name", &name,
                  NULL);

    return( name );
}

/** 
 * nwamui_env_set_modifiable:
 * @nwamui_env: a #NwamuiEnv.
 * @modifiable: Value to set modifiable to.
 * 
 * Once you set the object unmodifiable, then any attempts to change any values will fail.
 *
 **/ 
extern void
nwamui_env_set_modifiable (   NwamuiEnv *self,
                              gboolean        modifiable )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));

    g_object_set (G_OBJECT (self),
                  "modifiable", modifiable,
                  NULL);
}

/**
 * nwamui_env_is_modifiable:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the modifiable.
 *
 **/
extern gboolean
nwamui_env_is_modifiable (NwamuiEnv *self)
{
    gboolean  modifiable = FALSE; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), modifiable);

    g_object_get (G_OBJECT (self),
                  "modifiable", &modifiable,
                  NULL);

    return( modifiable );
}

/** 
 * nwamui_env_set_enabled:
 * @nwamui_env: a #NwamuiEnv.
 * @enabled: Value to set enabled to.
 * 
 **/ 
extern void
nwamui_env_set_enabled (   NwamuiEnv *self,
                              gboolean        enabled )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));

    g_object_set (G_OBJECT (self),
                  "enabled", enabled,
                  NULL);
}

/**
 * nwamui_env_get_enabled:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the enabled.
 *
 **/
extern gboolean
nwamui_env_get_enabled (NwamuiEnv *self)
{
    gboolean  enabled = FALSE; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), enabled);

    g_object_get (G_OBJECT (self),
                  "enabled", &enabled,
                  NULL);

    return( enabled );
}

/** 
 * nwamui_env_set_nameservice_discover:
 * @nwamui_env: a #NwamuiEnv.
 * @nameservice_discover: Value to set nameservice_discover to.
 * 
 **/ 
extern void
nwamui_env_set_nameservice_discover (   NwamuiEnv *self,
                              gboolean        nameservice_discover )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));

    g_object_set (G_OBJECT (self),
                  "nameservice_discover", nameservice_discover,
                  NULL);
}

/**
 * nwamui_env_get_nameservice_discover:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the nameservice_discover.
 *
 **/
extern gboolean
nwamui_env_get_nameservice_discover (NwamuiEnv *self)
{
    gboolean  nameservice_discover = FALSE; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), nameservice_discover);

    g_object_get (G_OBJECT (self),
                  "nameservice_discover", &nameservice_discover,
                  NULL);

    return( nameservice_discover );
}

/** 
 * nwamui_env_set_nameservices:
 * @nwamui_env: a #NwamuiEnv.
 * @nameservices: should be a GList of nwamui_env_nameservices_t nameservices.
 * 
 **/ 
extern void
nwamui_env_set_nameservices (   NwamuiEnv *self,
                              const GList*    nameservices )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));
    g_assert (nameservices != NULL );

    if ( nameservices != NULL ) {
        g_object_set (G_OBJECT (self),
                      "nameservices", nameservices,
                      NULL);
    }
}

/**
 * nwamui_env_get_nameservices:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: a GList of nwamui_env_nameservices_t nameservices.
 *
 **/
extern GList*  
nwamui_env_get_nameservices (NwamuiEnv *self)
{
    GList*    nameservices = NULL; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), nameservices);

    g_object_get (G_OBJECT (self),
                  "nameservices", &nameservices,
                  NULL);

    return( nameservices );
}

/** 
 * nwamui_env_set_nameservices_config_file:
 * @nwamui_env: a #NwamuiEnv.
 * @nameservices_config_file: Value to set nameservices_config_file to.
 * 
 **/ 
extern void
nwamui_env_set_nameservices_config_file (   NwamuiEnv *self,
                              const gchar*  nameservices_config_file )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));
    g_assert (nameservices_config_file != NULL );

    if ( nameservices_config_file != NULL ) {
        g_object_set (G_OBJECT (self),
                      "nameservices_config_file", nameservices_config_file,
                      NULL);
    }
}

/**
 * nwamui_env_get_nameservices_config_file:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the nameservices_config_file.
 *
 **/
extern gchar*
nwamui_env_get_nameservices_config_file (NwamuiEnv *self)
{
    gchar*  nameservices_config_file = NULL; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), nameservices_config_file);

    g_object_get (G_OBJECT (self),
                  "nameservices_config_file", &nameservices_config_file,
                  NULL);

    return( nameservices_config_file );
}

/** 
 * nwamui_env_set_domainname:
 * @nwamui_env: a #NwamuiEnv.
 * @domainname: Value to set domainname to.
 * 
 **/ 
extern void
nwamui_env_set_domainname (   NwamuiEnv *self,
                              const gchar*  domainname )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));
    g_assert (domainname != NULL );

    if ( domainname != NULL ) {
        g_object_set (G_OBJECT (self),
                      "domainname", domainname,
                      NULL);
    }
}

/**
 * nwamui_env_get_domainname:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the domainname.
 *
 **/
extern gchar*
nwamui_env_get_domainname (NwamuiEnv *self)
{
    gchar*  domainname = NULL; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), domainname);

    g_object_get (G_OBJECT (self),
                  "domainname", &domainname,
                  NULL);

    return( domainname );
}

/** 
 * nwamui_env_set_dns_nameservice_servers:
 * @nwamui_env: a #NwamuiEnv.
 * @dns_nameservice_servers: Value to set dns_nameservice_servers to.
 * 
 **/ 
extern void
nwamui_env_set_dns_nameservice_servers (   NwamuiEnv *self,
                              const GList*    dns_nameservice_servers )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));
    g_assert (dns_nameservice_servers != NULL );

    if ( dns_nameservice_servers != NULL ) {
        g_object_set (G_OBJECT (self),
                      "dns_nameservice_servers", dns_nameservice_servers,
                      NULL);
    }
}

/**
 * nwamui_env_get_dns_nameservice_servers:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the dns_nameservice_servers.
 *
 **/
extern GList*  
nwamui_env_get_dns_nameservice_servers (NwamuiEnv *self)
{
    GList*    dns_nameservice_servers = NULL; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), dns_nameservice_servers);

    g_object_get (G_OBJECT (self),
                  "dns_nameservice_servers", &dns_nameservice_servers,
                  NULL);

    return( dns_nameservice_servers );
}

/** 
 * nwamui_env_set_dns_nameservice_search:
 * @nwamui_env: a #NwamuiEnv.
 * @dns_nameservice_search: Value to set dns_nameservice_search to.
 * 
 **/ 
extern void
nwamui_env_set_dns_nameservice_search (   NwamuiEnv *self,
                              const gchar*  dns_nameservice_search )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));
    g_assert (dns_nameservice_search != NULL );

    if ( dns_nameservice_search != NULL ) {
        g_object_set (G_OBJECT (self),
                      "dns_nameservice_search", dns_nameservice_search,
                      NULL);
    }
}

/**
 * nwamui_env_get_dns_nameservice_search:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the dns_nameservice_search.
 *
 **/
extern gchar*
nwamui_env_get_dns_nameservice_search (NwamuiEnv *self)
{
    gchar*  dns_nameservice_search = NULL; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), dns_nameservice_search);

    g_object_get (G_OBJECT (self),
                  "dns_nameservice_search", &dns_nameservice_search,
                  NULL);

    return( dns_nameservice_search );
}

/** 
 * nwamui_env_set_nis_nameservice_servers:
 * @nwamui_env: a #NwamuiEnv.
 * @nis_nameservice_servers: Value to set nis_nameservice_servers to.
 * 
 **/ 
extern void
nwamui_env_set_nis_nameservice_servers (   NwamuiEnv *self,
                              const GList*    nis_nameservice_servers )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));
    g_assert (nis_nameservice_servers != NULL );

    if ( nis_nameservice_servers != NULL ) {
        g_object_set (G_OBJECT (self),
                      "nis_nameservice_servers", nis_nameservice_servers,
                      NULL);
    }
}

/**
 * nwamui_env_get_nis_nameservice_servers:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the nis_nameservice_servers.
 *
 **/
extern GList*  
nwamui_env_get_nis_nameservice_servers (NwamuiEnv *self)
{
    GList*    nis_nameservice_servers = NULL; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), nis_nameservice_servers);

    g_object_get (G_OBJECT (self),
                  "nis_nameservice_servers", &nis_nameservice_servers,
                  NULL);

    return( nis_nameservice_servers );
}

/** 
 * nwamui_env_set_nisplus_nameservice_servers:
 * @nwamui_env: a #NwamuiEnv.
 * @nisplus_nameservice_servers: Value to set nisplus_nameservice_servers to.
 * 
 **/ 
extern void
nwamui_env_set_nisplus_nameservice_servers (   NwamuiEnv *self,
                              const GList*    nisplus_nameservice_servers )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));
    g_assert (nisplus_nameservice_servers != NULL );

    if ( nisplus_nameservice_servers != NULL ) {
        g_object_set (G_OBJECT (self),
                      "nisplus_nameservice_servers", nisplus_nameservice_servers,
                      NULL);
    }
}

/**
 * nwamui_env_get_nisplus_nameservice_servers:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the nisplus_nameservice_servers.
 *
 **/
extern GList*  
nwamui_env_get_nisplus_nameservice_servers (NwamuiEnv *self)
{
    GList*    nisplus_nameservice_servers = NULL; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), nisplus_nameservice_servers);

    g_object_get (G_OBJECT (self),
                  "nisplus_nameservice_servers", &nisplus_nameservice_servers,
                  NULL);

    return( nisplus_nameservice_servers );
}

/** 
 * nwamui_env_set_ldap_nameservice_servers:
 * @nwamui_env: a #NwamuiEnv.
 * @ldap_nameservice_servers: Value to set ldap_nameservice_servers to.
 * 
 **/ 
extern void
nwamui_env_set_ldap_nameservice_servers (   NwamuiEnv *self,
                              const GList*    ldap_nameservice_servers )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));
    g_assert (ldap_nameservice_servers != NULL );

    if ( ldap_nameservice_servers != NULL ) {
        g_object_set (G_OBJECT (self),
                      "ldap_nameservice_servers", ldap_nameservice_servers,
                      NULL);
    }
}

/**
 * nwamui_env_get_ldap_nameservice_servers:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the ldap_nameservice_servers.
 *
 **/
extern GList*  
nwamui_env_get_ldap_nameservice_servers (NwamuiEnv *self)
{
    GList*    ldap_nameservice_servers = NULL; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), ldap_nameservice_servers);

    g_object_get (G_OBJECT (self),
                  "ldap_nameservice_servers", &ldap_nameservice_servers,
                  NULL);

    return( ldap_nameservice_servers );
}

/** 
 * nwamui_env_set_hosts_file:
 * @nwamui_env: a #NwamuiEnv.
 * @hosts_file: Value to set hosts_file to.
 * 
 **/ 
extern void
nwamui_env_set_hosts_file (   NwamuiEnv *self,
                              const gchar*  hosts_file )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));
    g_assert (hosts_file != NULL );

    if ( hosts_file != NULL ) {
        g_object_set (G_OBJECT (self),
                      "hosts_file", hosts_file,
                      NULL);
    }
}

/**
 * nwamui_env_get_hosts_file:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the hosts_file.
 *
 **/
extern gchar*
nwamui_env_get_hosts_file (NwamuiEnv *self)
{
    gchar*  hosts_file = NULL; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), hosts_file);

    g_object_get (G_OBJECT (self),
                  "hosts_file", &hosts_file,
                  NULL);

    return( hosts_file );
}

/** 
 * nwamui_env_set_nfsv4_domain:
 * @nwamui_env: a #NwamuiEnv.
 * @nfsv4_domain: Value to set nfsv4_domain to.
 * 
 **/ 
extern void
nwamui_env_set_nfsv4_domain (   NwamuiEnv *self,
                              const gchar*  nfsv4_domain )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));
    g_assert (nfsv4_domain != NULL );

    if ( nfsv4_domain != NULL ) {
        g_object_set (G_OBJECT (self),
                      "nfsv4_domain", nfsv4_domain,
                      NULL);
    }
}

/**
 * nwamui_env_get_nfsv4_domain:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the nfsv4_domain.
 *
 **/
extern gchar*
nwamui_env_get_nfsv4_domain (NwamuiEnv *self)
{
    gchar*  nfsv4_domain = NULL; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), nfsv4_domain);

    g_object_get (G_OBJECT (self),
                  "nfsv4_domain", &nfsv4_domain,
                  NULL);

    return( nfsv4_domain );
}

/** 
 * nwamui_env_set_ipfilter_config_file:
 * @nwamui_env: a #NwamuiEnv.
 * @ipfilter_config_file: Value to set ipfilter_config_file to.
 * 
 **/ 
extern void
nwamui_env_set_ipfilter_config_file (   NwamuiEnv *self,
                              const gchar*  ipfilter_config_file )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));
    g_assert (ipfilter_config_file != NULL );

    if ( ipfilter_config_file != NULL ) {
        g_object_set (G_OBJECT (self),
                      "ipfilter_config_file", ipfilter_config_file,
                      NULL);
    }
}

/**
 * nwamui_env_get_ipfilter_config_file:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the ipfilter_config_file.
 *
 **/
extern gchar*
nwamui_env_get_ipfilter_config_file (NwamuiEnv *self)
{
    gchar*  ipfilter_config_file = NULL; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), ipfilter_config_file);

    g_object_get (G_OBJECT (self),
                  "ipfilter_config_file", &ipfilter_config_file,
                  NULL);

    return( ipfilter_config_file );
}

/** 
 * nwamui_env_set_ipfilter_v6_config_file:
 * @nwamui_env: a #NwamuiEnv.
 * @ipfilter_v6_config_file: Value to set ipfilter_v6_config_file to.
 * 
 **/ 
extern void
nwamui_env_set_ipfilter_v6_config_file (   NwamuiEnv *self,
                              const gchar*  ipfilter_v6_config_file )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));
    g_assert (ipfilter_v6_config_file != NULL );

    if ( ipfilter_v6_config_file != NULL ) {
        g_object_set (G_OBJECT (self),
                      "ipfilter_v6_config_file", ipfilter_v6_config_file,
                      NULL);
    }
}

/**
 * nwamui_env_get_ipfilter_v6_config_file:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the ipfilter_v6_config_file.
 *
 **/
extern gchar*
nwamui_env_get_ipfilter_v6_config_file (NwamuiEnv *self)
{
    gchar*  ipfilter_v6_config_file = NULL; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), ipfilter_v6_config_file);

    g_object_get (G_OBJECT (self),
                  "ipfilter_v6_config_file", &ipfilter_v6_config_file,
                  NULL);

    return( ipfilter_v6_config_file );
}

/** 
 * nwamui_env_set_ipnat_config_file:
 * @nwamui_env: a #NwamuiEnv.
 * @ipnat_config_file: Value to set ipnat_config_file to.
 * 
 **/ 
extern void
nwamui_env_set_ipnat_config_file (   NwamuiEnv *self,
                              const gchar*  ipnat_config_file )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));
    g_assert (ipnat_config_file != NULL );

    if ( ipnat_config_file != NULL ) {
        g_object_set (G_OBJECT (self),
                      "ipnat_config_file", ipnat_config_file,
                      NULL);
    }
}

/**
 * nwamui_env_get_ipnat_config_file:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the ipnat_config_file.
 *
 **/
extern gchar*
nwamui_env_get_ipnat_config_file (NwamuiEnv *self)
{
    gchar*  ipnat_config_file = NULL; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), ipnat_config_file);

    g_object_get (G_OBJECT (self),
                  "ipnat_config_file", &ipnat_config_file,
                  NULL);

    return( ipnat_config_file );
}

/** 
 * nwamui_env_set_ippool_config_file:
 * @nwamui_env: a #NwamuiEnv.
 * @ippool_config_file: Value to set ippool_config_file to.
 * 
 **/ 
extern void
nwamui_env_set_ippool_config_file (   NwamuiEnv *self,
                              const gchar*  ippool_config_file )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));
    g_assert (ippool_config_file != NULL );

    if ( ippool_config_file != NULL ) {
        g_object_set (G_OBJECT (self),
                      "ippool_config_file", ippool_config_file,
                      NULL);
    }
}

/**
 * nwamui_env_get_ippool_config_file:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the ippool_config_file.
 *
 **/
extern gchar*
nwamui_env_get_ippool_config_file (NwamuiEnv *self)
{
    gchar*  ippool_config_file = NULL; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), ippool_config_file);

    g_object_get (G_OBJECT (self),
                  "ippool_config_file", &ippool_config_file,
                  NULL);

    return( ippool_config_file );
}

/** 
 * nwamui_env_set_ike_config_file:
 * @nwamui_env: a #NwamuiEnv.
 * @ike_config_file: Value to set ike_config_file to.
 * 
 **/ 
extern void
nwamui_env_set_ike_config_file (   NwamuiEnv *self,
                              const gchar*  ike_config_file )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));
    g_assert (ike_config_file != NULL );

    if ( ike_config_file != NULL ) {
        g_object_set (G_OBJECT (self),
                      "ike_config_file", ike_config_file,
                      NULL);
    }
}

/**
 * nwamui_env_get_ike_config_file:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the ike_config_file.
 *
 **/
extern gchar*
nwamui_env_get_ike_config_file (NwamuiEnv *self)
{
    gchar*  ike_config_file = NULL; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), ike_config_file);

    g_object_get (G_OBJECT (self),
                  "ike_config_file", &ike_config_file,
                  NULL);

    return( ike_config_file );
}

/** 
 * nwamui_env_set_ipseckey_config_file:
 * @nwamui_env: a #NwamuiEnv.
 * @ipseckey_config_file: Value to set ipseckey_config_file to.
 * 
 **/ 
extern void
nwamui_env_set_ipseckey_config_file (   NwamuiEnv *self,
                              const gchar*  ipseckey_config_file )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));
    g_assert (ipseckey_config_file != NULL );

    if ( ipseckey_config_file != NULL ) {
        g_object_set (G_OBJECT (self),
                      "ipseckey_config_file", ipseckey_config_file,
                      NULL);
    }
}

/**
 * nwamui_env_get_ipseckey_config_file:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the ipseckey_config_file.
 *
 **/
extern gchar*
nwamui_env_get_ipseckey_config_file (NwamuiEnv *self)
{
    gchar*  ipseckey_config_file = NULL; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), ipseckey_config_file);

    g_object_get (G_OBJECT (self),
                  "ipseckey_config_file", &ipseckey_config_file,
                  NULL);

    return( ipseckey_config_file );
}

/** 
 * nwamui_env_set_ipsecpolicy_config_file:
 * @nwamui_env: a #NwamuiEnv.
 * @ipsecpolicy_config_file: Value to set ipsecpolicy_config_file to.
 * 
 **/ 
extern void
nwamui_env_set_ipsecpolicy_config_file (   NwamuiEnv *self,
                              const gchar*  ipsecpolicy_config_file )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));
    g_assert (ipsecpolicy_config_file != NULL );

    if ( ipsecpolicy_config_file != NULL ) {
        g_object_set (G_OBJECT (self),
                      "ipsecpolicy_config_file", ipsecpolicy_config_file,
                      NULL);
    }
}

/**
 * nwamui_env_get_ipsecpolicy_config_file:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the ipsecpolicy_config_file.
 *
 **/
extern gchar*
nwamui_env_get_ipsecpolicy_config_file (NwamuiEnv *self)
{
    gchar*  ipsecpolicy_config_file = NULL; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), ipsecpolicy_config_file);

    g_object_get (G_OBJECT (self),
                  "ipsecpolicy_config_file", &ipsecpolicy_config_file,
                  NULL);

    return( ipsecpolicy_config_file );
}

/** 
 * nwamui_env_set_svcs_enable:
 * @nwamui_env: a #NwamuiEnv.
 * @svcs_enable: Value to set svcs_enable to.
 * 
 **/ 
extern void
nwamui_env_set_svcs_enable (   NwamuiEnv *self,
                              const GList*    svcs_enable )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));
    g_assert (svcs_enable != NULL );

    if ( svcs_enable != NULL ) {
        g_object_set (G_OBJECT (self),
                      "svcs_enable", svcs_enable,
                      NULL);
    }
}

/**
 * nwamui_env_get_svcs_enable:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the svcs_enable.
 *
 **/
extern GList*  
nwamui_env_get_svcs_enable (NwamuiEnv *self)
{
    GList*    svcs_enable = NULL; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), svcs_enable);

    g_object_get (G_OBJECT (self),
                  "svcs_enable", &svcs_enable,
                  NULL);

    return( svcs_enable );
}

/** 
 * nwamui_env_set_svcs_disable:
 * @nwamui_env: a #NwamuiEnv.
 * @svcs_disable: Value to set svcs_disable to.
 * 
 **/ 
extern void
nwamui_env_set_svcs_disable (   NwamuiEnv *self,
                              const GList*    svcs_disable )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));
    g_assert (svcs_disable != NULL );

    if ( svcs_disable != NULL ) {
        g_object_set (G_OBJECT (self),
                      "svcs_disable", svcs_disable,
                      NULL);
    }
}

/**
 * nwamui_env_get_svcs_disable:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the svcs_disable.
 *
 **/
extern GList*  
nwamui_env_get_svcs_disable (NwamuiEnv *self)
{
    GList*    svcs_disable = NULL; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), svcs_disable);

    g_object_get (G_OBJECT (self),
                  "svcs_disable", &svcs_disable,
                  NULL);

    return( svcs_disable );
}

/** 
 * nwamui_env_set_proxy_type:
 * @nwamui_env: a #NwamuiEnv.
 * @proxy_type: The proxy_type to set.
 * 
 **/ 
extern void
nwamui_env_set_proxy_type (   NwamuiEnv                *self,
                              nwamui_env_proxy_type_t   proxy_type )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));
    g_assert (proxy_type >= NWAMUI_ENV_PROXY_TYPE_DIRECT && proxy_type <= NWAMUI_ENV_PROXY_TYPE_LAST-1 );

    g_object_set (G_OBJECT (self),
                  "proxy_type", proxy_type,
                  NULL);
}

/**
 * nwamui_env_get_proxy_type:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the proxy_type.
 *
 **/
extern nwamui_env_proxy_type_t
nwamui_env_get_proxy_type (NwamuiEnv *self)
{
    gint  proxy_type = NWAMUI_ENV_PROXY_TYPE_DIRECT; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), proxy_type);

    g_object_get (G_OBJECT (self),
                  "proxy_type", &proxy_type,
                  NULL);

    return( (nwamui_env_proxy_type_t)proxy_type );
}

/** 
 * nwamui_env_set_proxy_pac_file:
 * @nwamui_env: a #NwamuiEnv.
 * @proxy_pac_file: The proxy_pac_file to set.
 * 
 **/ 
extern void
nwamui_env_set_proxy_pac_file (   NwamuiEnv *self,
                              const gchar*  proxy_pac_file )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));
    g_assert (proxy_pac_file != NULL );

    if ( proxy_pac_file != NULL ) {
        g_object_set (G_OBJECT (self),
                      "proxy_pac_file", proxy_pac_file,
                      NULL);
    }
}

/**
 * nwamui_env_get_proxy_pac_file:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the proxy_pac_file.
 *
 **/
extern gchar*
nwamui_env_get_proxy_pac_file (NwamuiEnv *self)
{
    gchar*  proxy_pac_file = NULL; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), proxy_pac_file);

    g_object_get (G_OBJECT (self),
                  "proxy_pac_file", &proxy_pac_file,
                  NULL);

    return( proxy_pac_file );
}

/** 
 * nwamui_env_set_proxy_http_server:
 * @nwamui_env: a #NwamuiEnv.
 * @proxy_http_server: The proxy_http_server to set.
 * 
 **/ 
extern void
nwamui_env_set_proxy_http_server (   NwamuiEnv *self,
                              const gchar*  proxy_http_server )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));
    g_assert (proxy_http_server != NULL );

    if ( proxy_http_server != NULL ) {
        g_object_set (G_OBJECT (self),
                      "proxy_http_server", proxy_http_server,
                      NULL);
    }
}

/**
 * nwamui_env_get_proxy_http_server:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the proxy_http_server.
 *
 **/
extern gchar*
nwamui_env_get_proxy_http_server (NwamuiEnv *self)
{
    gchar*  proxy_http_server = NULL; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), proxy_http_server);

    g_object_get (G_OBJECT (self),
                  "proxy_http_server", &proxy_http_server,
                  NULL);

    return( proxy_http_server );
}

/** 
 * nwamui_env_set_proxy_https_server:
 * @nwamui_env: a #NwamuiEnv.
 * @proxy_https_server: The proxy_https_server to set.
 * 
 **/ 
extern void
nwamui_env_set_proxy_https_server (   NwamuiEnv *self,
                              const gchar*  proxy_https_server )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));
    g_assert (proxy_https_server != NULL );

    if ( proxy_https_server != NULL ) {
        g_object_set (G_OBJECT (self),
                      "proxy_https_server", proxy_https_server,
                      NULL);
    }
}

/**
 * nwamui_env_get_proxy_https_server:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the proxy_https_server.
 *
 **/
extern gchar*
nwamui_env_get_proxy_https_server (NwamuiEnv *self)
{
    gchar*  proxy_https_server = NULL; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), proxy_https_server);

    g_object_get (G_OBJECT (self),
                  "proxy_https_server", &proxy_https_server,
                  NULL);

    return( proxy_https_server );
}

/** 
 * nwamui_env_set_proxy_ftp_server:
 * @nwamui_env: a #NwamuiEnv.
 * @proxy_ftp_server: The proxy_ftp_server to set.
 * 
 **/ 
extern void
nwamui_env_set_proxy_ftp_server (   NwamuiEnv *self,
                              const gchar*  proxy_ftp_server )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));
    g_assert (proxy_ftp_server != NULL );

    if ( proxy_ftp_server != NULL ) {
        g_object_set (G_OBJECT (self),
                      "proxy_ftp_server", proxy_ftp_server,
                      NULL);
    }
}

/**
 * nwamui_env_get_proxy_ftp_server:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the proxy_ftp_server.
 *
 **/
extern gchar*
nwamui_env_get_proxy_ftp_server (NwamuiEnv *self)
{
    gchar*  proxy_ftp_server = NULL; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), proxy_ftp_server);

    g_object_get (G_OBJECT (self),
                  "proxy_ftp_server", &proxy_ftp_server,
                  NULL);

    return( proxy_ftp_server );
}

/** 
 * nwamui_env_set_proxy_gopher_server:
 * @nwamui_env: a #NwamuiEnv.
 * @proxy_gopher_server: The proxy_gopher_server to set.
 * 
 **/ 
extern void
nwamui_env_set_proxy_gopher_server (   NwamuiEnv *self,
                              const gchar*  proxy_gopher_server )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));
    g_assert (proxy_gopher_server != NULL );

    if ( proxy_gopher_server != NULL ) {
        g_object_set (G_OBJECT (self),
                      "proxy_gopher_server", proxy_gopher_server,
                      NULL);
    }
}

/**
 * nwamui_env_get_proxy_gopher_server:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the proxy_gopher_server.
 *
 **/
extern gchar*
nwamui_env_get_proxy_gopher_server (NwamuiEnv *self)
{
    gchar*  proxy_gopher_server = NULL; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), proxy_gopher_server);

    g_object_get (G_OBJECT (self),
                  "proxy_gopher_server", &proxy_gopher_server,
                  NULL);

    return( proxy_gopher_server );
}

/** 
 * nwamui_env_set_proxy_socks_server:
 * @nwamui_env: a #NwamuiEnv.
 * @proxy_socks_server: The proxy_socks_server to set.
 * 
 **/ 
extern void
nwamui_env_set_proxy_socks_server (   NwamuiEnv *self,
                              const gchar*  proxy_socks_server )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));
    g_assert (proxy_socks_server != NULL );

    if ( proxy_socks_server != NULL ) {
        g_object_set (G_OBJECT (self),
                      "proxy_socks_server", proxy_socks_server,
                      NULL);
    }
}

/**
 * nwamui_env_get_proxy_socks_server:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the proxy_socks_server.
 *
 **/
extern gchar*
nwamui_env_get_proxy_socks_server (NwamuiEnv *self)
{
    gchar*  proxy_socks_server = NULL; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), proxy_socks_server);

    g_object_get (G_OBJECT (self),
                  "proxy_socks_server", &proxy_socks_server,
                  NULL);

    return( proxy_socks_server );
}

/** 
 * nwamui_env_set_proxy_bypass_list:
 * @nwamui_env: a #NwamuiEnv.
 * @proxy_bypass_list: The proxy_bypass_list to set.
 * 
 **/ 
extern void
nwamui_env_set_proxy_bypass_list (   NwamuiEnv *self,
                              const gchar*  proxy_bypass_list )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));
    g_assert (proxy_bypass_list != NULL );

    if ( proxy_bypass_list != NULL ) {
        g_object_set (G_OBJECT (self),
                      "proxy_bypass_list", proxy_bypass_list,
                      NULL);
    }
}

/**
 * nwamui_env_get_proxy_bypass_list:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the proxy_bypass_list.
 *
 **/
extern gchar*
nwamui_env_get_proxy_bypass_list (NwamuiEnv *self)
{
    gchar*  proxy_bypass_list = NULL; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), proxy_bypass_list);

    g_object_get (G_OBJECT (self),
                  "proxy_bypass_list", &proxy_bypass_list,
                  NULL);

    return( proxy_bypass_list );
}

/** 
 * nwamui_env_set_use_http_proxy_for_all:
 * @nwamui_env: a #NwamuiEnv.
 * @use_http_proxy_for_all: Value to set use_http_proxy_for_all to.
 * 
 **/ 
extern void
nwamui_env_set_use_http_proxy_for_all (   NwamuiEnv *self,
                              gboolean        use_http_proxy_for_all )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));

    g_object_set (G_OBJECT (self),
                  "use_http_proxy_for_all", use_http_proxy_for_all,
                  NULL);
}

/**
 * nwamui_env_get_use_http_proxy_for_all:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the use_http_proxy_for_all.
 *
 **/
extern gboolean
nwamui_env_get_use_http_proxy_for_all (NwamuiEnv *self)
{
    gboolean  use_http_proxy_for_all = FALSE; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), use_http_proxy_for_all);

    g_object_get (G_OBJECT (self),
                  "use_http_proxy_for_all", &use_http_proxy_for_all,
                  NULL);

    return( use_http_proxy_for_all );
}

/** 
 * nwamui_env_set_proxy_http_port:
 * @nwamui_env: a #NwamuiEnv.
 * @proxy_http_port: Value to set proxy_http_port to.
 * 
 **/ 
extern void
nwamui_env_set_proxy_http_port (   NwamuiEnv *self,
                              gint        proxy_http_port )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));
    g_assert (proxy_http_port >= 0 && proxy_http_port <= G_MAXINT );

    g_object_set (G_OBJECT (self),
                  "proxy_http_port", proxy_http_port,
                  NULL);
}

/**
 * nwamui_env_get_proxy_http_port:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the proxy_http_port.
 *
 **/
extern gint
nwamui_env_get_proxy_http_port (NwamuiEnv *self)
{
    gint  proxy_http_port = 80; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), proxy_http_port);

    g_object_get (G_OBJECT (self),
                  "proxy_http_port", &proxy_http_port,
                  NULL);

    return( proxy_http_port );
}

/** 
 * nwamui_env_set_proxy_https_port:
 * @nwamui_env: a #NwamuiEnv.
 * @proxy_https_port: Value to set proxy_https_port to.
 * 
 **/ 
extern void
nwamui_env_set_proxy_https_port (   NwamuiEnv *self,
                              gint        proxy_https_port )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));
    g_assert (proxy_https_port >= 0 && proxy_https_port <= G_MAXINT );

    g_object_set (G_OBJECT (self),
                  "proxy_https_port", proxy_https_port,
                  NULL);
}

/**
 * nwamui_env_get_proxy_https_port:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the proxy_https_port.
 *
 **/
extern gint
nwamui_env_get_proxy_https_port (NwamuiEnv *self)
{
    gint  proxy_https_port = 80; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), proxy_https_port);

    g_object_get (G_OBJECT (self),
                  "proxy_https_port", &proxy_https_port,
                  NULL);

    return( proxy_https_port );
}

/** 
 * nwamui_env_set_proxy_ftp_port:
 * @nwamui_env: a #NwamuiEnv.
 * @proxy_ftp_port: Value to set proxy_ftp_port to.
 * 
 **/ 
extern void
nwamui_env_set_proxy_ftp_port (   NwamuiEnv *self,
                              gint        proxy_ftp_port )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));
    g_assert (proxy_ftp_port >= 0 && proxy_ftp_port <= G_MAXINT );

    g_object_set (G_OBJECT (self),
                  "proxy_ftp_port", proxy_ftp_port,
                  NULL);
}

/**
 * nwamui_env_get_proxy_ftp_port:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the proxy_ftp_port.
 *
 **/
extern gint
nwamui_env_get_proxy_ftp_port (NwamuiEnv *self)
{
    gint  proxy_ftp_port = 80; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), proxy_ftp_port);

    g_object_get (G_OBJECT (self),
                  "proxy_ftp_port", &proxy_ftp_port,
                  NULL);

    return( proxy_ftp_port );
}

/** 
 * nwamui_env_set_proxy_gopher_port:
 * @nwamui_env: a #NwamuiEnv.
 * @proxy_gopher_port: Value to set proxy_gopher_port to.
 * 
 **/ 
extern void
nwamui_env_set_proxy_gopher_port (   NwamuiEnv *self,
                              gint        proxy_gopher_port )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));
    g_assert (proxy_gopher_port >= 0 && proxy_gopher_port <= G_MAXINT );

    g_object_set (G_OBJECT (self),
                  "proxy_gopher_port", proxy_gopher_port,
                  NULL);
}

/**
 * nwamui_env_get_proxy_gopher_port:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the proxy_gopher_port.
 *
 **/
extern gint
nwamui_env_get_proxy_gopher_port (NwamuiEnv *self)
{
    gint  proxy_gopher_port = 80; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), proxy_gopher_port);

    g_object_get (G_OBJECT (self),
                  "proxy_gopher_port", &proxy_gopher_port,
                  NULL);

    return( proxy_gopher_port );
}

/** 
 * nwamui_env_set_proxy_socks_port:
 * @nwamui_env: a #NwamuiEnv.
 * @proxy_socks_port: Value to set proxy_socks_port to.
 * 
 **/ 
extern void
nwamui_env_set_proxy_socks_port (   NwamuiEnv *self,
                              gint        proxy_socks_port )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));
    g_assert (proxy_socks_port >= 0 && proxy_socks_port <= G_MAXINT );

    g_object_set (G_OBJECT (self),
                  "proxy_socks_port", proxy_socks_port,
                  NULL);
}

/** 
 * nwamui_env_set_proxy_username:
 * @nwamui_env: a #NwamuiEnv.
 * @proxy_username: Value to set proxy_username to.
 * 
 **/ 
extern void
nwamui_env_set_proxy_username (   NwamuiEnv *self,
                              const gchar*  proxy_username )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));
    g_assert (proxy_username != NULL );

    if ( proxy_username != NULL ) {
        g_object_set (G_OBJECT (self),
                      "proxy_username", proxy_username,
                      NULL);
    }
}

/**
 * nwamui_env_get_proxy_username:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the proxy_username.
 *
 **/
extern gchar*
nwamui_env_get_proxy_username (NwamuiEnv *self)
{
    gchar*  proxy_username = NULL; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), proxy_username);

    g_object_get (G_OBJECT (self),
                  "proxy_username", &proxy_username,
                  NULL);

    return( proxy_username );
}

/** 
 * nwamui_env_set_proxy_password:
 * @nwamui_env: a #NwamuiEnv.
 * @proxy_password: Value to set proxy_password to.
 * 
 **/ 
extern void
nwamui_env_set_proxy_password (   NwamuiEnv *self,
                              const gchar*  proxy_password )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));
    g_assert (proxy_password != NULL );

    if ( proxy_password != NULL ) {
        g_object_set (G_OBJECT (self),
                      "proxy_password", proxy_password,
                      NULL);
    }
}

/**
 * nwamui_env_get_proxy_password:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the proxy_password.
 *
 **/
extern gchar*
nwamui_env_get_proxy_password (NwamuiEnv *self)
{
    gchar*  proxy_password = NULL; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), proxy_password);

    g_object_get (G_OBJECT (self),
                  "proxy_password", &proxy_password,
                  NULL);

    return( proxy_password );
}

/**
 * nwamui_env_get_proxy_socks_port:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the proxy_socks_port.
 *
 **/
extern gint
nwamui_env_get_proxy_socks_port (NwamuiEnv *self)
{
    gint  proxy_socks_port = 1080; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), proxy_socks_port);

    g_object_get (G_OBJECT (self),
                  "proxy_socks_port", &proxy_socks_port,
                  NULL);

    return( proxy_socks_port );
}

/** 
 * nwamui_env_set_activation_mode:
 * @nwamui_env: a #NwamuiEnv.
 * @activation_mode: Value to set activation_mode to.
 * 
 **/ 
extern void
nwamui_env_set_activation_mode (   NwamuiEnv *self,
                                  nwamui_cond_activation_mode_t        activation_mode )
{
    g_return_if_fail (NWAMUI_IS_ENV(self));
    g_assert (activation_mode >= NWAMUI_COND_ACTIVATION_MODE_MANUAL && activation_mode <= NWAMUI_COND_ACTIVATION_MODE_LAST );

    g_object_set (G_OBJECT (self),
                  "activation_mode", (gint)activation_mode,
                  NULL);
}

/**
 * nwamui_env_get_activation_mode:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the activation_mode.
 *
 **/
extern nwamui_cond_activation_mode_t
nwamui_env_get_activation_mode (NwamuiEnv *self)
{
    gint  activation_mode = NWAMUI_COND_ACTIVATION_MODE_MANUAL; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), activation_mode);

    g_object_get (G_OBJECT (self),
                  "activation_mode", &activation_mode,
                  NULL);

    return( (nwamui_cond_activation_mode_t)activation_mode );
}

/** 
 * nwamui_env_set_conditions:
 * @nwamui_env: a #NwamuiEnv.
 * @conditions: Value to set conditions to.
 * 
 **/ 
extern void
nwamui_env_set_conditions (   NwamuiEnv *self,
                             const GList* conditions )
{
    g_return_if_fail (NWAMUI_IS_ENV (self));
    g_assert (conditions != NULL );

    if ( conditions != NULL ) {
        g_object_set (G_OBJECT (self),
                      "conditions", (gpointer)conditions,
                      NULL);
    }
}

/**
 * nwamui_env_get_conditions:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the conditions.
 *
 **/
extern GList*
nwamui_env_get_conditions (NwamuiEnv *self)
{
    gpointer  conditions = NULL; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), conditions);

    g_object_get (G_OBJECT (self),
                  "conditions", &conditions,
                  NULL);

    return( (GList*)conditions );
}


extern void
nwamui_env_condition_add (NwamuiEnv *self, NwamuiCond* cond)
{
    g_return_if_fail( NWAMUI_IS_ENV( self ) && NWAMUI_IS_COND(cond));
    
    self->prv->conditions = g_list_append( self->prv->conditions, NWAMUI_COND(g_object_ref(cond)) );
    
}

extern void
nwamui_env_condition_remove (NwamuiEnv *self, NwamuiCond* cond)
{
    g_return_if_fail( NWAMUI_IS_ENV( self ) && NWAMUI_IS_COND(cond));
    
    self->prv->conditions = g_list_remove( self->prv->conditions, cond );
    g_object_unref(cond);
}

extern void
nwamui_env_condition_foreach (NwamuiEnv *self, GFunc func, gpointer data)
{
    g_list_foreach(self->prv->conditions, func, data );
}


extern GtkTreeModel *
nwamui_env_get_condition_subject ()
{
    static GtkTreeModel *condition_subject = NULL;
	GtkTreeIter iter;
    gint i;

    if (condition_subject != NULL) {
        return condition_subject;
    }

	condition_subject = GTK_TREE_MODEL(gtk_list_store_new (1, G_TYPE_INT));
	/* Note we use NWAMUI_COND_FIELD_LAST to display <No conditions> */
    for (i = NWAMUI_COND_FIELD_IP_ADDRESS; i <= NWAMUI_COND_FIELD_LAST; i++) {
        gtk_list_store_append (GTK_LIST_STORE(condition_subject), &iter);
        gtk_list_store_set (GTK_LIST_STORE(condition_subject), &iter, 0, i, -1);
    }
    return condition_subject;
}

extern GtkTreeModel *
nwamui_env_get_condition_predicate ()
{
    static GtkTreeModel *condition_predicate = NULL;
	GtkTreeIter iter;
    gint i;

    if (condition_predicate != NULL) {
        return condition_predicate;
    }

	condition_predicate = GTK_TREE_MODEL(gtk_list_store_new (1, G_TYPE_INT));
	
    for (i = NWAMUI_COND_OP_IS; i < NWAMUI_COND_OP_LAST; i++) {
        gtk_list_store_append (GTK_LIST_STORE(condition_predicate), &iter);
        gtk_list_store_set (GTK_LIST_STORE(condition_predicate), &iter, 0, i, -1);
    }
    return condition_predicate;
}

#if 0
/* These are not needed right now since we don't support property templates,
 * but would like to keep around for when we do.
 */
extern GtkTreeModel *
nwamui_env_get_svcs (NwamuiEnv *self)
{
    GtkTreeModel *model;
    
    g_object_get (G_OBJECT (self),
      "svcs", &model,
      NULL);

    return model;
}

extern NwamuiSvc*
nwamui_env_get_svc (NwamuiEnv *self, GtkTreeIter *iter)
{
    NwamuiSvc *svcobj = NULL;

    g_return_val_if_fail (NWAMUI_IS_ENV (self), svcobj);

    gtk_tree_model_get (GTK_TREE_MODEL (self->prv->svcs_model), iter, SVC_OBJECT, &svcobj, -1);

    return svcobj;
}

extern NwamuiSvc*
nwamui_env_find_svc (NwamuiEnv *self, const gchar *svc)
{
    NwamuiSvc *svcobj = NULL;
    GtkTreeIter iter;

    g_return_val_if_fail (svc, NULL);
    g_return_val_if_fail (NWAMUI_IS_ENV (self), svcobj);

    if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL(self->prv->svcs_model), &iter)) {
        do {
            gchar *name;
            
            gtk_tree_model_get (GTK_TREE_MODEL(self->prv->svcs_model), &iter, SVC_OBJECT, &svcobj, -1);
            name = nwamui_svc_get_name (svcobj);
            
            if (g_ascii_strcasecmp (name, svc) == 0) {
                g_free (name);
                return svcobj;
            }
            g_free (name);
            g_object_unref (svcobj);
        } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(self->prv->svcs_model), &iter));
    }
    return NULL;
}

extern void
nwamui_env_svc_remove (NwamuiEnv *self, GtkTreeIter *iter)
{
    gtk_list_store_remove (GTK_LIST_STORE(self->prv->svcs_model), iter);
}

extern void
nwamui_env_svc_foreach (NwamuiEnv *self, GtkTreeModelForeachFunc func, gpointer data)
{
    gtk_tree_model_foreach (GTK_TREE_MODEL(self->prv->svcs_model), func, data);
}

extern gboolean
nwamui_env_svc_insert (NwamuiEnv *self, NwamuiSvc *svc)
{
	GtkTreeIter iter;
    NwamuiSvc *svcobj;
    nwam_error_t nerr;

    gtk_list_store_append (GTK_LIST_STORE(self->prv->svcs_model), &iter);
    gtk_list_store_set (GTK_LIST_STORE(self->prv->svcs_model), &iter,
      SVC_OBJECT, svcobj, -1);
    
    return TRUE;
}

extern gboolean
nwamui_env_svc_delete (NwamuiEnv *self, NwamuiSvc *svc)
{
	GtkTreeIter iter;
    NwamuiSvc *svcobj;
    nwam_error_t nerr;

    gtk_list_store_append (GTK_LIST_STORE(self->prv->svcs_model), &iter);
    gtk_list_store_set (GTK_LIST_STORE(self->prv->svcs_model), &iter,
      SVC_OBJECT, svcobj, -1);
   
    return TRUE;
}

extern NwamuiSvc*
nwamui_env_svc_add (NwamuiEnv *self, nwam_loc_prop_template_t svc)
{
	GtkTreeIter iter;
    NwamuiSvc *svcobj = NULL;
    nwam_error_t err;
    const char* fmri = NULL;
    
    if ( ( err = nwam_loc_prop_template_get_fmri( svc, &fmri )) == NWAM_SUCCESS ) {
        if ( fmri != NULL && (svcobj = nwamui_env_find_svc (self, fmri)) == NULL) {
            svcobj = nwamui_svc_new (svc);
            gtk_list_store_append (GTK_LIST_STORE(self->prv->svcs_model), &iter);
            gtk_list_store_set (GTK_LIST_STORE(self->prv->svcs_model), &iter,
              SVC_OBJECT, svcobj, -1);
        }
    }
    return svcobj;
}

extern void
nwamui_env_svc_add_full (NwamuiEnv *self,
  nwam_loc_prop_template_t svc,
  gboolean is_default,
  gboolean status)
{
    NwamuiSvc *svcobj;
    svcobj = nwamui_env_svc_add (self, svc);
    nwamui_svc_set_default (svcobj, is_default);
    nwamui_svc_set_status (svcobj, status);
    g_object_unref (svcobj);
}

static gboolean
nwamui_env_svc_commit (NwamuiEnv *self, NwamuiSvc *svc)
{
    nwam_error_t nerr = NWAM_INVALID_ARG;
    nwam_loc_prop_template_t *svc_h;
    
    /* FIXME: Can we actually add a service? */
    g_error("ERROR: Unable to add a service");
    
    g_object_get (svc, "nwam_svc", &svc_h, NULL);
    /* nerr = nwam_loc_svc_insert (self->prv->nwam_env, &svc_h, 1); */
    return nerr == NWAM_SUCCESS;
}
#endif /* 0 */

extern gboolean
nwamui_env_activate (NwamuiEnv *self)
{
    nwam_error_t nerr;
    nerr = nwam_loc_enable (self->prv->nwam_env);
    return nerr == NWAM_SUCCESS;
}

extern gboolean
nwamui_env_commit (NwamuiEnv *self)
{
    nwam_error_t nerr;
	GtkTreeIter iter;
    NwamuiSvc *svcobj;
    
#if 0
/* These are not needed right now since we don't support property templates,
 * but would like to keep around for when we do.
 */
    if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL(self->prv->svcs_model), &iter)) {
        do {
            gtk_tree_model_get (GTK_TREE_MODEL(self->prv->svcs_model), &iter, 0, &svcobj, -1);
            if (! nwamui_env_svc_commit (self, svcobj)) {
                return FALSE;
            }
        } while (gtk_tree_model_iter_next (GTK_TREE_MODEL(self->prv->svcs_model), &iter));
    }
#endif /* 0 */
    
    nerr = nwam_loc_commit (self->prv->nwam_env, 0);
    return nerr == NWAM_SUCCESS;
}

static void
nwamui_env_finalize (NwamuiEnv *self)
{
    g_assert_not_reached();
    
    if (self->prv->name != NULL ) {
        g_free( self->prv->name );
    }

    if (self->prv->nwam_env != NULL) {
        nwam_loc_free (self->prv->nwam_env);
    }
    
    if (self->prv->nameservices != NULL ) {
        g_list_free( self->prv->nameservices );
    }

    if (self->prv->nameservices_config_file != NULL ) {
        g_free( self->prv->nameservices_config_file );
    }

    if (self->prv->domainname != NULL ) {
        g_free( self->prv->domainname );
    }

    if (self->prv->dns_nameservice_servers != NULL ) {
        g_list_foreach( self->prv->dns_nameservice_servers , (GFunc)g_free, NULL );
        g_list_free( self->prv->dns_nameservice_servers );
    }

    if (self->prv->dns_nameservice_search != NULL ) {
        g_free( self->prv->dns_nameservice_search );
    }

    if (self->prv->nis_nameservice_servers != NULL ) {
        g_list_foreach( self->prv->nis_nameservice_servers , (GFunc)g_free, NULL );
        g_list_free( self->prv->nis_nameservice_servers );
    }

    if (self->prv->nisplus_nameservice_servers != NULL ) {
        g_list_foreach( self->prv->nisplus_nameservice_servers , (GFunc)g_free, NULL );
        g_list_free( self->prv->nisplus_nameservice_servers );
    }

    if (self->prv->ldap_nameservice_servers != NULL ) {
        g_list_foreach( self->prv->ldap_nameservice_servers , (GFunc)g_free, NULL );
        g_list_free( self->prv->ldap_nameservice_servers );
    }

    if (self->prv->hosts_file != NULL ) {
        g_free( self->prv->hosts_file );
    }

    if (self->prv->nfsv4_domain != NULL ) {
        g_free( self->prv->nfsv4_domain );
    }

    if (self->prv->ipfilter_config_file != NULL ) {
        g_free( self->prv->ipfilter_config_file );
    }

    if (self->prv->ipfilter_v6_config_file != NULL ) {
        g_free( self->prv->ipfilter_v6_config_file );
    }

    if (self->prv->ipnat_config_file != NULL ) {
        g_free( self->prv->ipnat_config_file );
    }

    if (self->prv->ippool_config_file != NULL ) {
        g_free( self->prv->ippool_config_file );
    }

    if (self->prv->ike_config_file != NULL ) {
        g_free( self->prv->ike_config_file );
    }

    if (self->prv->ipseckey_config_file != NULL ) {
        g_free( self->prv->ipseckey_config_file );
    }

    if (self->prv->ipsecpolicy_config_file != NULL ) {
        g_free( self->prv->ipsecpolicy_config_file );
    }

    if (self->prv->svcs_enable != NULL ) {
        g_list_foreach( self->prv->svcs_enable , (GFunc)g_free, NULL );
        g_list_free( self->prv->svcs_enable );
    }

    if (self->prv->svcs_disable != NULL ) {
        g_list_foreach( self->prv->svcs_disable , (GFunc)g_free, NULL );
        g_list_free( self->prv->svcs_disable );
    }

    if (self->prv->proxy_pac_file != NULL ) {
        g_free( self->prv->proxy_pac_file );
    }

    if (self->prv->proxy_http_server != NULL ) {
        g_free( self->prv->proxy_http_server );
    }

    if (self->prv->proxy_https_server != NULL ) {
        g_free( self->prv->proxy_https_server );
    }

    if (self->prv->proxy_ftp_server != NULL ) {
        g_free( self->prv->proxy_ftp_server );
    }

    if (self->prv->proxy_gopher_server != NULL ) {
        g_free( self->prv->proxy_gopher_server );
    }

    if (self->prv->proxy_socks_server != NULL ) {
        g_free( self->prv->proxy_socks_server );
    }

    if (self->prv->proxy_bypass_list != NULL ) {
        g_free( self->prv->proxy_bypass_list );
    }

    if (self->prv->svcs_model != NULL ) {
        g_object_unref( self->prv->svcs_model );
    }

    if (self->prv->proxy_username != NULL ) {
        g_free( self->prv->proxy_username );
    }

    if (self->prv->proxy_password != NULL ) {
        g_free( self->prv->proxy_password );
    }

    g_free (self->prv); 
    self->prv = NULL;

    parent_class->finalize (G_OBJECT (self));
}

/* Callbacks */

static void
object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
    NwamuiEnv* self = NWAMUI_ENV(data);

    g_debug("NwamuiEnv: notify %s changed\n", arg1->name);
}

static void 
svc_row_inserted_or_changed_cb (GtkTreeModel *tree_model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
    NwamuiNcu* self = NWAMUI_NCU(data);

    /* Chain events in the tree to the NCU object*/
    g_object_notify(G_OBJECT(self), "svcs_model");
}

static void 
svc_row_deleted_cb (GtkTreeModel *tree_model, GtkTreePath *path, gpointer data)
{
    NwamuiNcu* self = NWAMUI_NCU(data);

    /* Chain events in the tree to the NCU object*/
    g_object_notify(G_OBJECT(self), "svcs_model");
}

#if 0
/* These are not needed right now since we don't support property templates,
 * but would like to keep around for when we do.
 */
/* walkers */
static int
nwam_loc_svc_walker_cb (nwam_loc_prop_template_t svc, void *data)
{
    NwamuiEnv* self = NWAMUI_ENV(data);
    NwamuiEnvPrivate* prv = NWAMUI_ENV(data)->prv;

    /* TODO: status, is default? */
    nwamui_env_svc_add_full (self, svc, TRUE, TRUE);
}

static int
nwam_loc_sys_svc_walker_cb (nwam_loc_handle_t env, nwam_loc_prop_template_t svc, void *data)
{
    NwamuiEnv* self = NWAMUI_ENV(data);
    NwamuiEnvPrivate* prv = NWAMUI_ENV(data)->prv;

    /* TODO: status, is default? */
	GtkTreeIter iter;
    NwamuiSvc *svcobj;

    svcobj = nwamui_svc_new (svc);
    gtk_list_store_append (GTK_LIST_STORE(self->prv->sys_svcs_model), &iter);
    gtk_list_store_set (GTK_LIST_STORE(self->prv->sys_svcs_model), &iter,
      SVC_OBJECT, svcobj, -1);
    g_object_unref (svcobj);
}
#endif /* 0 */

