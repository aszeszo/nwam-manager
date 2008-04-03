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

#include "libnwamui.h"

static GObjectClass *parent_class = NULL;

enum {
    SVC_OBJECT = 0,
    SVC_N_COL,
};

struct _NwamuiEnvPrivate {
    gchar*                      name;
    gboolean                    modifiable;
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
    nwamui_cond_match_t         condition_match;
    GList*                      conditions;
    GtkListStore*               svcs_model;
    GtkListStore*               sys_svcs_model;

    nwam_env_handle_t			nwam_env;
};

enum {
    PROP_NAME=1,
    PROP_MODIFIABLE,
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
    PROP_SVCS,
    PROP_CONDITIONS,
    PROP_CONDITION_MATCH,
    PROP_NWAM_ENV,
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
static gboolean nwamui_env_svc_commit (NwamuiEnv *self, NwamuiSvc *svc);

/* Callbacks */
static void object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);
static void svc_row_inserted_or_changed_cb (GtkTreeModel *tree_model, GtkTreePath *path, GtkTreeIter *iter, gpointer user_data); 
static void svc_row_deleted_cb (GtkTreeModel *tree_model, GtkTreePath *path, gpointer user_data);

/* walkers */
static int nwam_env_svc_walker_cb (nwam_env_svc_t *svc, void *data);
static int nwam_env_sys_svc_walker_cb (nwam_env_handle_t env, nwam_env_svc_t *svc, void *data);

G_DEFINE_TYPE (NwamuiEnv, nwamui_env, G_TYPE_OBJECT)

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
    
    g_object_class_install_property (gobject_class,
                                     PROP_CONDITION_MATCH,
                                     g_param_spec_int ("condition_match",
                                                       _("condition_match"),
                                                       _("condition_match"),
                                                       NWAMUI_COND_MATCH_ANY,
                                                       NWAMUI_COND_MATCH_LAST-1,
                                                       NWAMUI_COND_MATCH_ANY,
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

}


static void
nwamui_env_init (NwamuiEnv *self)
{
    self->prv = g_new0 (NwamuiEnvPrivate, 1);
    
    self->prv->name = NULL;
    self->prv->modifiable = TRUE;
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
    self->prv->condition_match = NWAMUI_COND_MATCH_ANY;
    self->prv->conditions = NULL;
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
                nerr = nwam_env_load (prv->name, NWAM_FLAG_NONBLOCK,
                  &prv->nwam_env);
                if (nerr == NWAM_SUCCESS) {
                    g_debug ("nwamui_env_set_property found nwam_env_handle %s", prv->name);
                } else {
                    nerr = nwam_env_create (self->prv->name, &self->prv->nwam_env);
                    g_assert (nerr == NWAM_SUCCESS);
                }
            }
            nerr = nwam_env_set_name (self->prv->nwam_env, self->prv->name);
            if (nerr != NWAM_SUCCESS) {
                g_debug ("nwam_env_set_name %s error: %s", self->prv->name, nwam_strerror (nerr));
            }
            }
            break;

        case PROP_MODIFIABLE: {
                self->prv->modifiable = g_value_get_boolean( value );
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

        case PROP_CONDITION_MATCH: {
                self->prv->condition_match = (nwamui_cond_match_t)g_value_get_int( value );
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
    nwam_data_t **nwamdata;

    switch (prop_id) {
        case PROP_NAME: {
            if (self->prv->name == NULL) {
                char *name;
                nerr = nwam_env_get_name (self->prv->nwam_env, &name);
                if (nerr != NWAM_SUCCESS) {
                    g_debug ("nwam_env_get_name %s error: %s", self->prv->name, nwam_strerror (nerr));
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
            
        case PROP_CONDITION_MATCH: {
                g_value_set_int( value, (gint)self->prv->condition_match );
            }
            break;
            
        case PROP_CONDITIONS: {
                g_value_set_pointer( value, self->prv->conditions );
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
nwamui_env_new_with_handle (nwam_env_handle_t envh)
{
    NwamuiEnv   *env = NWAMUI_ENV(g_object_new (NWAMUI_TYPE_ENV,
                                    "nwam_env", envh,
                                    NULL));
    char *name;
    
    /* TODO, test data */
    {
        nwam_error_t nerr;
        nerr = nwam_env_get_name (envh, (char **)&name);
        if (nerr != NWAM_SUCCESS) {
            g_assert_not_reached ();
        }
        g_object_set (G_OBJECT (env),
          "name", name,
          NULL);

        free (name);

        g_debug ("loading nwamui_env_new_with_handle %s", name);
        nerr = nwam_env_walk_svc (env->prv->nwam_env, nwam_env_svc_walker_cb, env);
        if (nerr != NWAM_SUCCESS) {
            g_debug ("[libnwam] nwamui_env_new_with_handle walk svc %s", nwam_strerror (nerr));
        }

        nerr = nwam_env_walk_svc (env->prv->nwam_env, nwam_env_sys_svc_walker_cb, env);
        if (nerr != NWAM_SUCCESS) {
            g_debug ("[libnwam] nwamui_env_new_with_handle walk sys svc %s", nwam_strerror (nerr));
        }
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
    nwam_env_handle_t new_env_h;
        
    nerr = nwam_env_copy (self->prv->nwam_env, self->prv->name, &new_env_h);
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
 * nwamui_env_set_condition_match:
 * @nwamui_env: a #NwamuiEnv.
 * @condition_match: Value to set condition_match to.
 * 
 **/ 
extern void
nwamui_env_set_condition_match (   NwamuiEnv *self,
                                  nwamui_cond_match_t        condition_match )
{
    g_return_if_fail (NWAMUI_IS_ENV(self));
    g_assert (condition_match >= NWAMUI_COND_MATCH_ANY && condition_match <= NWAMUI_COND_MATCH_LAST );

    g_object_set (G_OBJECT (self),
                  "condition_match", (gint)condition_match,
                  NULL);
}

/**
 * nwamui_env_get_condition_match:
 * @nwamui_env: a #NwamuiEnv.
 * @returns: the condition_match.
 *
 **/
extern nwamui_cond_match_t
nwamui_env_get_condition_match (NwamuiEnv *self)
{
    gint  condition_match = NWAMUI_COND_MATCH_ANY; 

    g_return_val_if_fail (NWAMUI_IS_ENV (self), condition_match);

    g_object_get (G_OBJECT (self),
                  "condition_match", &condition_match,
                  NULL);

    return( (nwamui_cond_match_t)condition_match );
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
	
    for (i = NWAMUI_COND_OP_EQUALS; i < NWAMUI_COND_OP_LAST; i++) {
        gtk_list_store_append (GTK_LIST_STORE(condition_predicate), &iter);
        gtk_list_store_set (GTK_LIST_STORE(condition_predicate), &iter, 0, i, -1);
    }
    return condition_predicate;
}

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

    if (gtk_tree_model_get_iter_first (self->prv->svcs_model, &iter)) {
        do {
            gchar *name;
            
            gtk_tree_model_get (self->prv->svcs_model, &iter, SVC_OBJECT, &svcobj, -1);
            name = nwamui_svc_get_name (svcobj);
            
            if (g_ascii_strcasecmp (name, svc) == 0) {
                g_free (name);
                return svcobj;
            }
            g_free (name);
            g_object_unref (svcobj);
        } while (gtk_tree_model_iter_next(self->prv->svcs_model, &iter));
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
    gtk_tree_model_foreach (self->prv->svcs_model, func, data);
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
    
    return svcobj;
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
    
    return svcobj;
}

extern NwamuiSvc*
nwamui_env_svc_add (NwamuiEnv *self, nwam_env_svc_t *svc)
{
	GtkTreeIter iter;
    NwamuiSvc *svcobj;

    if ((svcobj = nwamui_env_find_svc (self, svc)) == NULL) {
        svcobj = nwamui_svc_new (svc);
        gtk_list_store_append (GTK_LIST_STORE(self->prv->svcs_model), &iter);
        gtk_list_store_set (GTK_LIST_STORE(self->prv->svcs_model), &iter,
          SVC_OBJECT, svcobj, -1);
    }
    return svcobj;
}

extern void
nwamui_env_svc_add_full (NwamuiEnv *self,
  nwam_env_svc_t *svc,
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
    nwam_error_t nerr;
    nwam_env_svc_t *svc_h;
    
    g_object_get (svc, "nwam_svc", &svc_h, NULL);
    nerr = nwam_env_svc_insert (self->prv->nwam_env, &svc_h, 1);
    return nerr == NWAM_SUCCESS;
}

extern gboolean
nwamui_env_activate (NwamuiEnv *self)
{
    nwam_error_t nerr;
    nerr = nwam_env_activate (self->prv->nwam_env);
    return nerr == NWAM_SUCCESS;
}

extern gboolean
nwamui_env_commit (NwamuiEnv *self)
{
    nwam_error_t nerr;
	GtkTreeIter iter;
    NwamuiSvc *svcobj;
    
    if (gtk_tree_model_get_iter_first (self->prv->svcs_model, &iter)) {
        do {
            gtk_tree_model_get (self->prv->svcs_model, &iter, 0, &svcobj, -1);
            if (! nwamui_env_svc_commit (self, svcobj)) {
                return FALSE;
            }
        } while (gtk_tree_model_iter_next (self->prv->svcs_model, &iter));
    }
    
    nerr = nwam_env_commit (self->prv->nwam_env, NWAM_FLAG_NONBLOCK);
    return nerr == NWAM_SUCCESS;
}

static void
nwamui_env_finalize (NwamuiEnv *self)
{
    g_assert_not_reached();
    
    if (self->prv->name != NULL ) {
        g_free( self->prv->name );
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

    if (self->prv->nwam_env != NULL) {
        nwam_env_free (self->prv->nwam_env);
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

/* walkers */
static int
nwam_env_svc_walker_cb (nwam_env_svc_t *svc, void *data)
{
    NwamuiEnv* self = NWAMUI_ENV(data);
    NwamuiEnvPrivate* prv = NWAMUI_ENV(data)->prv;

    /* TODO: status, is default? */
    nwamui_env_svc_add_full (self, svc, TRUE, TRUE);
}

static int
nwam_env_sys_svc_walker_cb (nwam_env_handle_t env, nwam_env_svc_t *svc, void *data)
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
