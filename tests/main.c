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
 */

#include <stdlib.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libgnomeui/libgnomeui.h>

#include <libnwamui.h>

static void test_ncp_gobject( void );
static void test_env_gobject( void );
static void test_enm_gobject( void );

GOptionEntry application_options[] = {
    /*
        { "show-all", 'a', 0, G_OPTION_ARG_NONE, &show_all_widgets, "Show all widgets", NULL  },
        { "add-wireless-dialog", 'w', 0, G_OPTION_ARG_STRING, &add_wireless_dialog, "Show 'Add Wireless' Dialog only", NULL  },
        { "env-pref-dialog", 'e', 0, G_OPTION_ARG_NONE, &env_pref_dialog, "Show 'Location Preferences' Dialog only", NULL  },
        { "nwam-pref-dialog", 'p', 0, G_OPTION_ARG_NONE, &nwam_pref_dialog, "Show 'Network Preferences' Dialog only", NULL  },
        { "vpn-pref-dialog", 'n', 0, G_OPTION_ARG_NONE, &vpn_pref_dialog, "Show 'VPN Preferences' Dialog only", NULL  },
        { "wireless-chooser", 'c', 0, G_OPTION_ARG_NONE, &wireless_chooser, "Show 'Wireless Network Chooser' Dialog only", NULL  },
        */
        { NULL }
};

/*
 * 
 */
int
main(int argc, char** argv) 
{
    GOptionContext*	option_context = NULL;
    GError*         err = NULL;
    NwamuiDaemon*   daemon = NULL;
    
    /* Initialise Thread Support */
    g_thread_init( NULL );

    g_type_init_with_debug_flags( G_TYPE_DEBUG_OBJECTS | G_TYPE_DEBUG_SIGNALS  );
    
    /* Initialize i18n support */
    gtk_set_locale ();
    
    option_context = g_option_context_new("nwam-manager-properties");
    g_option_context_add_main_entries(option_context, application_options, NULL);
    

    daemon = nwamui_daemon_get_instance();
    nwamui_daemon_wifi_start_scan(daemon);

    test_ncp_gobject();
    test_env_gobject();
    test_enm_gobject();
        
    return (EXIT_SUCCESS);
}

static gboolean
process_ncu(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
    NwamuiNcu           *ncu = NULL;
    gchar               *vname, *dname;
    nwamui_ncu_type_t   type;
    gchar*              typestr;
    gboolean            active;
    gboolean            ipv4_auto_conf;
    gchar*              ipv4_address;
    gchar*              ipv4_subnet;
    gchar*              ipv4_gateway;
    gboolean            ipv6_active;
    gboolean            ipv6_auto_conf;
    gchar*              ipv6_address;
    gchar*              ipv6_prefix;

    fprintf(stderr,"*************************************************************\n");

    gtk_tree_model_get(model, iter, 0, &ncu, -1);
    
    vname = nwamui_ncu_get_vanity_name( ncu );
    dname = nwamui_ncu_get_device_name( ncu );
    type = nwamui_ncu_get_ncu_type( ncu );
    active = nwamui_ncu_get_active( ncu );
    ipv4_auto_conf = nwamui_ncu_get_ipv4_auto_conf( ncu );
    ipv4_address = nwamui_ncu_get_ipv4_address( ncu );
    ipv4_subnet = nwamui_ncu_get_ipv4_subnet( ncu );
    ipv4_gateway = nwamui_ncu_get_ipv4_gateway( ncu );
    ipv6_active = nwamui_ncu_get_ipv6_active( ncu );
    ipv6_auto_conf = nwamui_ncu_get_ipv6_auto_conf( ncu );
    ipv6_address = nwamui_ncu_get_ipv6_address( ncu );
    ipv6_prefix = nwamui_ncu_get_ipv6_prefix( ncu );

    g_debug("Ncu Name = %s (%s)", vname, dname );
    switch( type ) {
        case NWAMUI_NCU_TYPE_WIRED:
            typestr = "WIRED";
            break;
        case NWAMUI_NCU_TYPE_WIRELESS:
            typestr = "WIRELESS";
            break;
        default:
            typestr = "**** UNKNOWN *****";
            break;
    }
    
    g_debug("Ncu Type = %s", typestr );
    g_debug("Ncu active = %s\n", active?"True":"False" );
    g_debug("Ncu ipv4_auto_conf = %s", ipv4_auto_conf?"True":"False" );
    g_debug("Ncu ipv4_address = %s", ipv4_address?ipv4_address:"NULL" );
    g_debug("Ncu ipv4_subnet = %s", ipv4_subnet?ipv4_subnet:"NULL" );
    g_debug("Ncu ipv4_gateway = %s\n", ipv4_gateway?ipv4_gateway:"NULL" );
    g_debug("Ncu ipv6_active = %s", ipv6_active?"True":"False" );
    g_debug("Ncu ipv6_auto_conf = %s", ipv6_auto_conf?"True":"False" );
    g_debug("Ncu ipv6_address = %s", ipv6_address?ipv6_address:"NULL" );
    g_debug("Ncu ipv6_prefix = %s", ipv6_prefix?ipv6_prefix:"NULL" );

    if ( vname != NULL )
        g_free(vname);
    if ( dname != NULL )
        g_free(dname);
    if ( ipv4_address != NULL ) 
        g_free( ipv4_address );
    if ( ipv4_subnet != NULL ) 
        g_free( ipv4_subnet );
    if ( ipv4_gateway != NULL ) 
        g_free( ipv4_gateway );
    if ( ipv6_address != NULL ) 
        g_free( ipv6_address );
    if ( ipv6_prefix != NULL ) 
        g_free( ipv6_prefix );

    fprintf(stderr,"*************************************************************\n");

    return(FALSE);
}

static void 
process_ncp( gpointer data, gpointer user_data ) 
{
    NwamuiDaemon    *daemon = nwamui_daemon_get_instance();
    NwamuiNcp       *ncp = NWAMUI_NCP(data);
    
    fprintf(stderr,"=============================================================\n");
    if ( ncp != NULL ) {
        gchar * name = nwamui_ncp_get_name( ncp );

        g_debug("Processing ncp : %s", name );

        nwamui_ncp_foreach_ncu(ncp, (GtkTreeModelForeachFunc)process_ncu, NULL );

        g_free(name);
        g_object_unref(G_OBJECT(ncp));
    }

    g_object_unref(G_OBJECT(daemon));
    fprintf(stderr,"=============================================================\n");
    
}

static void 
test_ncp_gobject( void )
{
    NwamuiDaemon    *daemon = nwamui_daemon_get_instance();
    
    GList           *ncp_list = nwamui_daemon_get_ncp_list(daemon);
    
    g_list_foreach(ncp_list, process_ncp, NULL );
    
    g_list_foreach(ncp_list, nwamui_util_obj_unref, NULL );
    g_list_free( ncp_list );
    g_object_unref(G_OBJECT(daemon));
}

static void
process_env( gpointer data, gpointer user_data ) 
{
    NwamuiEnv                  *env = NWAMUI_ENV( data );
    gchar*                      name;
    nwamui_env_proxy_type_t     proxy_type;
    gchar*                      proxy_type_str;
    gchar*                      proxy_pac_file;
    gchar*                      proxy_http_server;
    gchar*                      proxy_https_server;
    gchar*                      proxy_ftp_server;
    gchar*                      proxy_gopher;
    gchar*                      proxy_socks;
    gchar*                      proxy_bypass_list;
    
    name = nwamui_env_get_name( env );
    proxy_type = nwamui_env_get_proxy_type( env );
    proxy_pac_file = nwamui_env_get_proxy_pac_file( env );
    proxy_http_server = nwamui_env_get_proxy_http_server( env );
    proxy_https_server = nwamui_env_get_proxy_https_server( env );
    proxy_ftp_server = nwamui_env_get_proxy_ftp_server( env );
    proxy_gopher = nwamui_env_get_proxy_gopher_server( env );
    proxy_socks = nwamui_env_get_proxy_socks_server( env );
    proxy_bypass_list = nwamui_env_get_proxy_bypass_list( env );

    switch( proxy_type ) {
        case NWAMUI_ENV_PROXY_TYPE_DIRECT:
            proxy_type_str="Direct";
            break;
        case NWAMUI_ENV_PROXY_TYPE_MANUAL:
            proxy_type_str="Manual";
            break;
        case NWAMUI_ENV_PROXY_TYPE_PAC_FILE:
            proxy_type_str="PAC File";
            break;
        default:
            proxy_type_str="**** UNKNOWN *****";
            break;
    }
    g_debug("Env name = %s", name?name:"NULL" );
    g_debug("Env proxy_type = %s", proxy_type_str);
    g_debug("Env proxy_pac_file = %s", proxy_pac_file?proxy_pac_file:"NULL" );
    g_debug("Env proxy_http_server = %s", proxy_http_server?proxy_http_server:"NULL" );
    g_debug("Env proxy_https_server = %s", proxy_https_server?proxy_https_server:"NULL" );
    g_debug("Env proxy_ftp_server = %s", proxy_ftp_server?proxy_ftp_server:"NULL" );
    g_debug("Env proxy_gopher = %s", proxy_gopher?proxy_gopher:"NULL" );
    g_debug("Env proxy_socks = %s", proxy_socks?proxy_socks:"NULL" );
    g_debug("Env proxy_bypass_list = %s", proxy_bypass_list?proxy_bypass_list:"NULL" );

    if ( name != NULL ) g_free( name );
    if ( proxy_pac_file != NULL ) g_free( proxy_pac_file );
    if ( proxy_http_server != NULL ) g_free( proxy_http_server );
    if ( proxy_https_server != NULL ) g_free( proxy_https_server );
    if ( proxy_ftp_server != NULL ) g_free( proxy_ftp_server );
    if ( proxy_gopher != NULL ) g_free( proxy_gopher );
    if ( proxy_socks != NULL ) g_free( proxy_socks );
    if ( proxy_bypass_list != NULL ) g_free( proxy_bypass_list );
}

static void 
test_env_gobject( void )
{
    NwamuiDaemon    *daemon = nwamui_daemon_get_instance();
    GList           *env_list = nwamui_daemon_get_env_list(daemon);
    
    g_list_foreach(env_list, process_env, NULL );
    
    g_list_foreach(env_list, nwamui_util_obj_unref, NULL );
    g_list_free( env_list );
    g_object_unref(G_OBJECT(daemon));
    
}

static void
process_enm( gpointer data, gpointer user_data ) 
{
    NwamuiEnm           *enm = NWAMUI_ENM( data );
    gchar*               name;
    gboolean             active;
    gchar*               start_command;
    gchar*               stop_command;
    gchar*               smf_frmi;

    name = nwamui_enm_get_name ( NWAMUI_ENM(enm) );
    g_debug("NwamuiEnm : name = %s", name?name:"NULL" );


    active = nwamui_enm_get_active ( NWAMUI_ENM(enm) );
    g_debug("NwamuiEnm : active = %s", active?"True":"False" );


    start_command = nwamui_enm_get_start_command ( NWAMUI_ENM(enm) );
    g_debug("NwamuiEnm : start_command = %s", start_command?start_command:"NULL" );


    stop_command = nwamui_enm_get_stop_command ( NWAMUI_ENM(enm) );
    g_debug("NwamuiEnm : stop_command = %s", stop_command?stop_command:"NULL" );


    smf_frmi = nwamui_enm_get_smf_fmri ( NWAMUI_ENM(enm) );
    g_debug("NwamuiEnm : smf_frmi = %s", smf_frmi?smf_frmi:"NULL" );

    if (name != NULL ) {
        g_free( name );
    }

    if (start_command != NULL ) {
        g_free( start_command );
    }

    if (stop_command != NULL ) {
        g_free( stop_command );
    }

    if (smf_frmi != NULL ) {
        g_free( smf_frmi );
    }

}

static void 
test_enm_gobject( void )
{
    NwamuiDaemon    *daemon = nwamui_daemon_get_instance();
    GList           *enm_list = nwamui_daemon_get_enm_list(daemon);
    
    g_list_foreach(enm_list, process_enm, NULL );
    
    g_list_foreach(enm_list, nwamui_util_obj_unref, NULL );
    g_list_free( enm_list );
    g_object_unref(G_OBJECT(daemon));
    
}
