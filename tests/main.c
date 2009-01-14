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

static int indent = 0;

static void test_ncp_gobject( void );
static void test_env_gobject( void );
static void test_enm_gobject( void );

/* Command-line options */
static gboolean debug = FALSE;

GOptionEntry application_options[] = {
    /*
        { "show-all", 'a', 0, G_OPTION_ARG_NONE, &show_all_widgets, "Show all widgets", NULL  },
        { "add-wireless-dialog", 'w', 0, G_OPTION_ARG_STRING, &add_wireless_dialog, "Show 'Add Wireless' Dialog only", NULL  },
        { "env-pref-dialog", 'e', 0, G_OPTION_ARG_NONE, &env_pref_dialog, "Show 'Location Preferences' Dialog only", NULL  },
        { "nwam-pref-dialog", 'p', 0, G_OPTION_ARG_NONE, &nwam_pref_dialog, "Show 'Network Preferences' Dialog only", NULL  },
        { "vpn-pref-dialog", 'n', 0, G_OPTION_ARG_NONE, &vpn_pref_dialog, "Show 'VPN Preferences' Dialog only", NULL  },
        { "wireless-chooser", 'c', 0, G_OPTION_ARG_NONE, &wireless_chooser, "Show 'Wireless Network Chooser' Dialog only", NULL  },
        */
        {"debug", 0, 0, G_OPTION_ARG_NONE, &debug, N_("Enable debugging messages"), NULL },
        { NULL }
};

static void
default_log_handler( const gchar    *log_domain,
                     GLogLevelFlags  log_level,
                     const gchar    *message,
                     gpointer        unused_data)
{
    /* If "debug" not set then filter out debug messages. */
    if ((log_level & G_LOG_LEVEL_MASK) == G_LOG_LEVEL_DEBUG && !debug) {
        /* Do nothing */
        return;
    }

    /* Otherwise just log as usual */
    g_log_default_handler (log_domain, log_level, message, unused_data);
}

/*
 * 
 */
int
main(int argc, char** argv) 
{
    GnomeProgram *program;
    GOptionContext*	option_context = NULL;
    GError*         err = NULL;
    NwamuiDaemon*   daemon = NULL;
    
    /* Initialise Thread Support */
    g_thread_init( NULL );

    g_type_init_with_debug_flags( G_TYPE_DEBUG_OBJECTS | G_TYPE_DEBUG_SIGNALS  );
    
    /* Initialize i18n support */
    gtk_set_locale ();
    
    /* Setup log handler to trap debug messages */
    g_log_set_default_handler( default_log_handler, NULL );

    option_context = g_option_context_new("test-nwam");
    g_option_context_add_main_entries(option_context, application_options, NULL);
    
    program = gnome_program_init (PACKAGE, VERSION, LIBGNOMEUI_MODULE,
                                  argc, argv,
                                  GNOME_PARAM_APP_DATADIR, NWAM_MANAGER_DATADIR,
                                  GNOME_PARAM_GOPTION_CONTEXT, option_context,
                                  GNOME_PARAM_NONE);

    daemon = nwamui_daemon_get_instance();
    nwamui_daemon_wifi_start_scan(daemon);

    test_ncp_gobject();
    test_env_gobject();
    test_enm_gobject();
        
    g_object_unref (G_OBJECT (program));

    return (EXIT_SUCCESS);
}

static void
print_conditions( NwamuiObject* obj )
{
    GList* cond_list = NULL;
    nwamui_cond_activation_mode_t activation_mode;
    const char*obj_name = NULL;

    if ( NWAMUI_IS_ENM(obj) ) {
        obj_name = "NwamuiEnm";
        activation_mode = nwamui_enm_get_activation_mode( NWAMUI_ENM(obj) );
        cond_list = nwamui_enm_get_selection_conditions( NWAMUI_ENM(obj) );
    }

    if (obj_name != NULL ) { 
        const char* activation_str;
        GList      *elem;

        switch( activation_mode ) {
            case NWAMUI_COND_ACTIVATION_MODE_MANUAL:
                activation_str = "NWAMUI_COND_ACTIVATION_MODE_MANUAL";
                break;
            case NWAMUI_COND_ACTIVATION_MODE_SYSTEM:
                activation_str = "NWAMUI_COND_ACTIVATION_MODE_SYSTEM";
                break;
            case NWAMUI_COND_ACTIVATION_MODE_PRIORITIZED:
                activation_str = "NWAMUI_COND_ACTIVATION_MODE_PRIORITIZED";
                break;
            case NWAMUI_COND_ACTIVATION_MODE_CONDITIONAL_ANY:
                activation_str = "NWAMUI_COND_ACTIVATION_MODE_CONDITIONAL_ANY";
                break;
            default:
                activation_str = "????";
                break;
        }
        printf("%-*s%s : name = %s\n", indent, "", obj_name, activation_str );

        printf("%-*s%s : conditions = \n", indent, "", obj_name );
        if ( cond_list != NULL ) {
            for ( elem = g_list_first( cond_list );
                  elem != NULL;
                  elem = g_list_next( elem ) ) {
                if ( elem->data && NWAMUI_IS_COND(elem->data) ) {
                    gchar* cond_str = nwamui_cond_to_string( NWAMUI_COND(elem->data) );
                    printf("%-*s%s :     '%s'\n", indent, "", obj_name, cond_str?cond_str:"NULL" );
                    g_free(cond_str);
                }
            }
        }
        else {
            printf("%-*s%s :     NONE\n", indent, "", obj_name);
        }
        g_list_foreach( cond_list, nwamui_util_obj_unref, NULL );
        g_list_free( cond_list );
    }
}


static gboolean
process_ncu(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
    NwamuiNcu           *ncu = NULL;
    gchar               *vname, *dname, *display_name;
    nwamui_ncu_type_t   type;
    gchar*              typestr;
    gchar*              statestr;
    gchar*              state_detailstr;
    gboolean            active;
    gboolean            ipv4_auto_conf;
    gchar*              ipv4_address;
    gchar*              ipv4_subnet;
    gchar*              ipv4_gateway;
    gboolean            ipv6_active;
    gboolean            ipv6_auto_conf;
    gchar*              ipv6_address;
    gchar*              ipv6_prefix;

    printf("%-*s*************************************************************\n", indent, "");

    gtk_tree_model_get(model, iter, 0, &ncu, -1);
    
    vname = nwamui_ncu_get_vanity_name( ncu );
    dname = nwamui_ncu_get_device_name( ncu );
    display_name = nwamui_ncu_get_display_name( ncu );
    type = nwamui_ncu_get_ncu_type( ncu );
    active = nwamui_ncu_get_active( ncu );
    statestr = nwamui_ncu_get_connection_state_string( ncu );
    state_detailstr = nwamui_ncu_get_connection_state_detail_string( ncu );
    ipv4_auto_conf = nwamui_ncu_get_ipv4_auto_conf( ncu );
    ipv4_address = nwamui_ncu_get_ipv4_address( ncu );
    ipv4_subnet = nwamui_ncu_get_ipv4_subnet( ncu );
    ipv4_gateway = nwamui_ncu_get_ipv4_gateway( ncu );
    ipv6_active = nwamui_ncu_get_ipv6_active( ncu );
    ipv6_auto_conf = nwamui_ncu_get_ipv6_auto_conf( ncu );
    ipv6_address = nwamui_ncu_get_ipv6_address( ncu );
    ipv6_prefix = nwamui_ncu_get_ipv6_prefix( ncu );

    indent += 4;

    printf("%-*sNcu Name = %s (%s)\n", indent, "", vname, dname );
    printf("%-*sNcu Display Name = %s\n", indent, "", display_name );
    switch( type ) {
        case NWAMUI_NCU_TYPE_WIRED:
            typestr = "WIRED";
            break;
        case NWAMUI_NCU_TYPE_WIRELESS:
            typestr = "WIRELESS";
            break;
        case NWAMUI_NCU_TYPE_TUNNEL:
            typestr = "TUNNEL";
            break;
        default:
            typestr = "**** UNKNOWN *****";
            break;
    }
    
    printf("%-*sNcu Type = %s\n", indent, "", typestr );
    printf("%-*sNcu Connection State = %s\n", indent, "", statestr );
    printf("%-*sNcu Connection State Detail = %s\n", indent, "", state_detailstr?state_detailstr:"NULL" );
    printf("%-*sNcu active = %s\n\n", indent, "", active?"True":"False" );
    printf("%-*sNcu ipv4_auto_conf = %s\n", indent, "", ipv4_auto_conf?"True":"False" );
    printf("%-*sNcu ipv4_address = %s\n", indent, "", ipv4_address?ipv4_address:"NULL" );
    printf("%-*sNcu ipv4_subnet = %s\n", indent, "", ipv4_subnet?ipv4_subnet:"NULL" );
    printf("%-*sNcu ipv4_gateway = %s\n\n", indent, "", ipv4_gateway?ipv4_gateway:"NULL" );
    printf("%-*sNcu ipv6_active = %s\n", indent, "", ipv6_active?"True":"False" );
    printf("%-*sNcu ipv6_auto_conf = %s\n", indent, "", ipv6_auto_conf?"True":"False" );
    printf("%-*sNcu ipv6_address = %s\n", indent, "", ipv6_address?ipv6_address:"NULL" );
    printf("%-*sNcu ipv6_prefix = %s\n", indent, "", ipv6_prefix?ipv6_prefix:"NULL" );

    indent -= 4;

    if ( vname != NULL )
        g_free(vname);
    if ( dname != NULL )
        g_free(dname);
    if ( display_name != NULL )
        g_free(display_name);
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

    printf("%-*s*************************************************************\n", indent, "");

    return(FALSE);
}

static void 
process_ncp( gpointer data, gpointer user_data ) 
{
    NwamuiDaemon    *daemon = nwamui_daemon_get_instance();
    NwamuiNcp       *ncp = NWAMUI_NCP(data);
    
    if ( ncp != NULL ) {
        gchar * name = nwamui_ncp_get_name( ncp );

        printf("%-*s-------------------------------------------------------------\n", indent, "");
        printf("%-*sProcessing ncp : %s\n", indent, "", name );
        printf("%-*s-------------------------------------------------------------\n", indent, "");

        indent +=4;
        nwamui_ncp_foreach_ncu(ncp, (GtkTreeModelForeachFunc)process_ncu, NULL );
        indent -=4;

        g_free(name);
        g_object_unref(G_OBJECT(ncp));
    }

    g_object_unref(G_OBJECT(daemon));
    
}

static void 
test_ncp_gobject( void )
{
    NwamuiDaemon    *daemon = nwamui_daemon_get_instance();
    
    GList           *ncp_list = nwamui_daemon_get_ncp_list(daemon);
    
    printf("%-*s=============================================================\n", indent, "");
    printf("%-*s NCPs \n", indent, "");
    printf("%-*s=============================================================\n", indent, "");

    indent += 4;
    g_list_foreach(ncp_list, process_ncp, NULL );
    indent -= 4;
    
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
    printf("%-*s*************************************************************\n", indent, "");
    indent += 4;
    printf("%-*sEnv name = %s\n", indent, "", name?name:"NULL" );
    printf("%-*sEnv proxy_type = %s\n", indent, "", proxy_type_str);
    printf("%-*sEnv proxy_pac_file = %s\n", indent, "", proxy_pac_file?proxy_pac_file:"NULL" );
    printf("%-*sEnv proxy_http_server = %s\n", indent, "", proxy_http_server?proxy_http_server:"NULL" );
    printf("%-*sEnv proxy_https_server = %s\n", indent, "", proxy_https_server?proxy_https_server:"NULL" );
    printf("%-*sEnv proxy_ftp_server = %s\n", indent, "", proxy_ftp_server?proxy_ftp_server:"NULL" );
    printf("%-*sEnv proxy_gopher = %s\n", indent, "", proxy_gopher?proxy_gopher:"NULL" );
    printf("%-*sEnv proxy_socks = %s\n", indent, "", proxy_socks?proxy_socks:"NULL" );
    printf("%-*sEnv proxy_bypass_list = %s\n", indent, "", proxy_bypass_list?proxy_bypass_list:"NULL" );
    indent -= 4;
    printf("%-*s*************************************************************\n", indent, "");

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
    
    printf("%-*s=============================================================\n", indent, "");
    printf("%-*s Locations (ENVs) \n", indent, "");
    printf("%-*s=============================================================\n", indent, "");
    indent += 4;
    g_list_foreach(env_list, process_env, NULL );
    indent -= 4;
    
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

    printf("%-*s*************************************************************\n", indent, "");

    indent += 4;

    name = nwamui_enm_get_name ( NWAMUI_ENM(enm) );
    printf("%-*sNwamuiEnm : name = %s\n", indent, "", name?name:"NULL" );


    active = nwamui_enm_get_active ( NWAMUI_ENM(enm) );
    printf("%-*sNwamuiEnm : active = %s\n", indent, "", active?"True":"False" );


    start_command = nwamui_enm_get_start_command ( NWAMUI_ENM(enm) );
    printf("%-*sNwamuiEnm : start_command = %s\n", indent, "", start_command?start_command:"NULL" );


    stop_command = nwamui_enm_get_stop_command ( NWAMUI_ENM(enm) );
    printf("%-*sNwamuiEnm : stop_command = %s\n", indent, "", stop_command?stop_command:"NULL" );


    smf_frmi = nwamui_enm_get_smf_fmri ( NWAMUI_ENM(enm) );
    printf("%-*sNwamuiEnm : smf_frmi = %s\n", indent, "", smf_frmi?smf_frmi:"NULL" );

    print_conditions( enm );

    indent -= 4;

    printf("%-*s*************************************************************\n", indent, "");

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
    
    printf("%-*s=============================================================\n", indent, "");
    printf("%-*s ENMs \n", indent, "");
    printf("%-*s=============================================================\n", indent, "");
    indent += 4;
    g_list_foreach(enm_list, process_enm, NULL );
    indent -= 4;
    
    g_list_foreach(enm_list, nwamui_util_obj_unref, NULL );
    g_list_free( enm_list );
    g_object_unref(G_OBJECT(daemon));
    
}

