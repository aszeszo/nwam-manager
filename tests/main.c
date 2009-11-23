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
 */

#include <stdlib.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libgnomeui/libgnomeui.h>

#include <libnwamui.h>

static int indent = 0;

static void test_daemon_gobject( void );
static void test_ncp_gobject( void );
static void test_env_gobject( void );
static void test_enm_gobject( void );
static void test_known_wlan_gobject( void );

static void scan_started(GObject *daemon, gpointer data);
static void scan_result (GObject *daemon, GObject *wifi, gpointer data);

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

    /* g_type_init_with_debug_flags( G_TYPE_DEBUG_OBJECTS | G_TYPE_DEBUG_SIGNALS  ); */
    
    /* Initialize i18n support */
    gtk_set_locale ();
    
    /* Setup log handler to trap debug messages */
    nwamui_util_default_log_handler_init();

    option_context = g_option_context_new("test-nwam");
    g_option_context_add_main_entries(option_context, application_options, NULL);
    
    program = gnome_program_init (PACKAGE, VERSION, LIBGNOMEUI_MODULE,
                                  argc, argv,
                                  GNOME_PARAM_APP_DATADIR, NWAM_MANAGER_DATADIR,
                                  GNOME_PARAM_GOPTION_CONTEXT, option_context,
                                  GNOME_PARAM_NONE);

    nwamui_util_set_debug_mode( debug );

    daemon = nwamui_daemon_get_instance();

    g_signal_connect(daemon, "wifi_scan_result",
          (GCallback)scan_started, (gpointer) NULL);

    g_signal_connect(daemon, "wifi_scan_result",
          (GCallback)scan_result, (gpointer) NULL);

    nwamui_daemon_wifi_start_scan(daemon);

    test_daemon_gobject();
    test_ncp_gobject();
    test_env_gobject();
    test_enm_gobject();
    test_known_wlan_gobject();
        

    gtk_main();

    g_object_unref (G_OBJECT (program));

    return (EXIT_SUCCESS);
}

static void
print_ns_list_and_free( gchar* obj_name, gchar *property, GList* ns_list )
{
    GList* elem = NULL;
    printf("%-*s%s : %s = \n", indent, "", obj_name, property );
    if ( ns_list != NULL ) {
        for ( elem = g_list_first( ns_list );
              elem != NULL;
              elem = g_list_next( elem ) ) {
            if ( elem->data ) {
                nwam_nameservices_t svc = (nwam_nameservices_t)elem->data;
                const gchar*        str = NULL;
                switch ( svc ) {
                    case NWAM_NAMESERVICES_DNS:
                        str = "NWAM_NAMESERVICES_DNS";
                        break;
                    case NWAM_NAMESERVICES_FILES:
                        str = "NWAM_NAMESERVICES_FILES";
                        break;
                    case NWAM_NAMESERVICES_NIS:
                        str = "NWAM_NAMESERVICES_NIS";
                        break;
                    case NWAM_NAMESERVICES_LDAP:
                        str = "NWAM_NAMESERVICES_LDAP";
                        break;
                    default:
                        str = "**** UNKNOWN Nameservice Type ****";
                        break;
                }
                printf("%-*s%s :     '%s'\n", indent, "", obj_name, str );
            }
        }
    }
    else {
        printf("%-*s%s :     NONE\n", indent, "", obj_name);
    }
    g_list_free( ns_list );
}

static void
print_string_list_and_free( gchar* obj_name, gchar *property, GList* str_list )
{
    GList* elem = NULL;
    printf("%-*s%s : %s = \n", indent, "", obj_name, property );
    if ( str_list != NULL ) {
        for ( elem = g_list_first( str_list );
              elem != NULL;
              elem = g_list_next( elem ) ) {
            if ( elem->data ) {
                gchar* str = (gchar*)elem->data;
                printf("%-*s%s :     '%s'\n", indent, "", obj_name, str?str:"NULL" );
            }
        }
    }
    else {
        printf("%-*s%s :     NONE\n", indent, "", obj_name);
    }
    g_list_foreach( str_list, (GFunc)g_free, NULL );
    g_list_free( str_list );
}

static void
print_conditions( GObject* obj )
{
    GList* cond_list = NULL;
    nwamui_cond_activation_mode_t activation_mode;
    const char*obj_name = NULL;

    if ( NWAMUI_IS_NCU(obj) ) {
        obj_name = "NwamuiNcu";
        activation_mode = nwamui_object_get_activation_mode( NWAMUI_OBJECT(obj) );
        cond_list = NULL; /* Don't have conditions for NCU */
    }
    else if ( NWAMUI_IS_ENV(obj) || NWAMUI_IS_ENM(obj) ) {
        obj_name = g_type_name(G_TYPE_FROM_INSTANCE(obj));
        activation_mode = nwamui_object_get_activation_mode(NWAMUI_OBJECT(obj));
        cond_list = nwamui_object_get_conditions(NWAMUI_OBJECT(obj));
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
        printf("%-*s%s : activation_mode = %s\n", indent, "", obj_name, activation_str );

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
    NwamuiNcu         *ncu = NULL;
    const gchar       *vname, *display_name;
    gchar             *dname, *nwam_qualified_name;
    nwamui_ncu_type_t  type;
    gchar*             typestr;
    gchar*             statestr;
    gchar*             state_detailstr;
    gchar*             cfg_str;
    gboolean           active;
    gboolean           enabled;
    gboolean           ipv4_has_dhcp;
    gchar*             ipv4_address;
    gchar*             ipv4_subnet;
    gboolean           ipv6_active;
    gboolean           ipv6_has_dhcp;
    gboolean           ipv6_has_auto_conf;
    gchar*             ipv6_address;
    gchar*             ipv6_prefix;

    printf("%-*s*************************************************************\n", indent, "");

    gtk_tree_model_get(model, iter, 0, &ncu, -1);
    
    vname = nwamui_object_get_name(NWAMUI_OBJECT(ncu));
    dname = nwamui_ncu_get_device_name( ncu );
    display_name = nwamui_ncu_get_display_name( ncu );
    nwam_qualified_name = nwamui_ncu_get_nwam_qualified_name( ncu );
    type = nwamui_ncu_get_ncu_type( ncu );
    active = nwamui_object_get_active(NWAMUI_OBJECT(ncu));
    enabled = nwamui_object_get_enabled(NWAMUI_OBJECT(ncu));
    statestr = nwamui_ncu_get_connection_state_string( ncu );
    state_detailstr = nwamui_ncu_get_connection_state_detail_string( ncu, TRUE );
    cfg_str = nwamui_ncu_get_configuration_summary_string( ncu );

    ipv4_has_dhcp = nwamui_ncu_get_ipv4_has_dhcp( ncu );
    ipv4_address = nwamui_ncu_get_ipv4_address( ncu );
    ipv4_subnet = nwamui_ncu_get_ipv4_subnet( ncu );
    ipv6_active = nwamui_ncu_get_ipv6_active( ncu );
    ipv6_has_dhcp = nwamui_ncu_get_ipv6_has_dhcp( ncu );
    ipv6_has_auto_conf = nwamui_ncu_get_ipv6_has_auto_conf( ncu );
    ipv6_address = nwamui_ncu_get_ipv6_address( ncu );
    ipv6_prefix = nwamui_ncu_get_ipv6_prefix( ncu );

    indent += 4;

    printf("%-*sNcu Name = %s (%s)\n", indent, "", vname, dname );
    printf("%-*sNcu Display Name = %s\n", indent, "", display_name );
    printf("%-*sNcu NWAM Qualified Name = %s\n", indent, "", nwam_qualified_name );
    switch( type ) {
        case NWAMUI_NCU_TYPE_WIRED:
            typestr = "WIRED";
            break;
        case NWAMUI_NCU_TYPE_WIRELESS:
            typestr = "WIRELESS";
            break;
#ifdef TUNNEL_SUPPORT
        case NWAMUI_NCU_TYPE_TUNNEL:
            typestr = "TUNNEL";
            break;
#endif /* TUNNEL_SUPPORT */
        default:
            typestr = "**** UNKNOWN *****";
            break;
    }
    
    printf("%-*sNcu Type = %s\n", indent, "", typestr );
    printf("%-*sNcu Connection State = %s\n", indent, "", statestr );
    printf("%-*sNcu Connection State Detail = %s\n", indent, "", state_detailstr?state_detailstr:"NULL" );
    printf("%-*sNcu Configuration Summary = %s\n", indent, "", cfg_str?cfg_str:"NULL" );
    printf("%-*sNcu active = %s\n", indent, "", active?"True":"False" );
    printf("%-*sNcu enabled = %s\n", indent, "", enabled?"True":"False" );
    printf("%-*sNcu ipv4_has_dhcp = %s\n", indent, "", ipv4_has_dhcp?"True":"False" );
    printf("%-*sNcu ipv4_address = %s\n", indent, "", ipv4_address?ipv4_address:"NULL" );
    printf("%-*sNcu ipv4_subnet = %s\n", indent, "", ipv4_subnet?ipv4_subnet:"NULL" );
    printf("%-*sNcu ipv6_active = %s\n", indent, "", ipv6_active?"True":"False" );
    printf("%-*sNcu ipv6_has_dhcp = %s\n", indent, "", ipv6_has_dhcp?"True":"False" );
    printf("%-*sNcu ipv6_has_auto_conf = %s\n", indent, "", ipv6_has_auto_conf?"True":"False" );
    printf("%-*sNcu ipv6_address = %s\n", indent, "", ipv6_address?ipv6_address:"NULL" );
    printf("%-*sNcu ipv6_prefix = %s\n", indent, "", ipv6_prefix?ipv6_prefix:"NULL" );

    indent -= 4;

    if ( nwamui_ncu_has_modifications( ncu ) ) {
        g_error("Object shouldn't have modifications");
    }

    if ( dname != NULL )
        g_free(dname);
    if ( ipv4_address != NULL ) 
        g_free( ipv4_address );
    if ( ipv4_subnet != NULL ) 
        g_free( ipv4_subnet );
    if ( ipv6_address != NULL ) 
        g_free( ipv6_address );
    if ( ipv6_prefix != NULL ) 
        g_free( ipv6_prefix );

    g_free( cfg_str );

    printf("%-*s*************************************************************\n", indent, "");

    return(FALSE);
}

static void 
process_ncp( gpointer data, gpointer user_data ) 
{
    NwamuiDaemon    *daemon = nwamui_daemon_get_instance();
    NwamuiNcp       *ncp = NWAMUI_NCP(data);
    
    if ( ncp != NULL ) {
        const gchar *name   = nwamui_object_get_name(NWAMUI_OBJECT(ncp));
        gboolean     active = nwamui_ncp_get_active( ncp );

        printf("%-*s-------------------------------------------------------------\n", indent, "");
        printf("%-*sProcessing ncp : %s (%s)\n", indent, "", name, active?"ACTIVE":"inactive" );
        printf("%-*s-------------------------------------------------------------\n", indent, "");

        indent +=4;
        nwamui_ncp_foreach_ncu(ncp, (GtkTreeModelForeachFunc)process_ncu, NULL );
        indent -=4;

        g_object_unref(G_OBJECT(ncp));
    }

    g_object_unref(G_OBJECT(daemon));
    
}

static void 
test_daemon_gobject( void )
{
    NwamuiDaemon    *daemon = nwamui_daemon_get_instance();
    gboolean         selection_manual  =  nwamui_daemon_env_selection_is_manual( daemon );
    
    printf("%-*s=============================================================\n", indent, "");
    printf("%-*s DAEMON \n", indent, "");
    printf("%-*s=============================================================\n", indent, "");

    printf("%-*s*************************************************************\n", indent, "");
    indent += 4;

    printf("%-*sEnv selection_manual  = %s\n", indent, "", selection_manual ?"TRUE":"FALSE" );

    indent -= 4;
    printf("%-*s*************************************************************\n", indent, "");

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

static const gchar*
config_source_to_str( nwamui_env_config_source_t src )
{
    switch( src ) {
        case NWAMUI_ENV_CONFIG_SOURCE_DHCP:   
            return("DHCP");
        case NWAMUI_ENV_CONFIG_SOURCE_MANUAL:   
            return("MANUAL");
        default:
            return("***UNKNOWN***");
    }
}

static void
process_env( gpointer data, gpointer user_data ) 
{
    NwamuiEnv  *env = NWAMUI_ENV( data );
    const gchar*name;
    nwamui_cond_activation_mode_t
                activation_mode  =  nwamui_object_get_activation_mode(NWAMUI_OBJECT(env));
    GList*      conditions  =  nwamui_object_get_conditions(NWAMUI_OBJECT(env));
    gboolean    active  =  nwamui_object_get_active(NWAMUI_OBJECT(env));
    gboolean    enabled  =  nwamui_object_get_enabled(NWAMUI_OBJECT(env));
    GList*      nameservices  =  nwamui_env_get_nameservices ( env );
    gchar*      nameservices_config_file  =  nwamui_env_get_nameservices_config_file ( env );
    gchar*      default_domainname  =  nwamui_env_get_default_domainname ( env );
    nwamui_env_config_source_t
                dns_config_source  =  nwamui_env_get_dns_nameservice_config_source ( env );
    gchar*      dns_nameservice_domain  =  nwamui_env_get_dns_nameservice_domain ( env );
    GList*      dns_nameservice_servers  =  nwamui_env_get_dns_nameservice_servers ( env );
    GList*      dns_nameservice_search  =  nwamui_env_get_dns_nameservice_search ( env );
    GList*      nameservice_servers  =  nwamui_env_get_nis_nameservice_servers ( env );
    nwamui_env_config_source_t
                nis_config_source  =  nwamui_env_get_nis_nameservice_config_source ( env );
    GList*      nis_nameservice_servers  =  nwamui_env_get_nis_nameservice_servers ( env );
    nwamui_env_config_source_t
                ldap_config_source  =  nwamui_env_get_ldap_nameservice_config_source ( env );
    GList*      ldap_nameservice_servers  =  nwamui_env_get_ldap_nameservice_servers ( env );
#ifndef _DISABLE_HOSTS_FILE
    gchar*      hosts_file  =  nwamui_env_get_hosts_file ( env );
#endif /* _DISABLE_HOSTS_FILE */
    gchar*      nfsv4_domain  =  nwamui_env_get_nfsv4_domain ( env );
    gchar*      ipfilter_config_file  =  nwamui_env_get_ipfilter_config_file ( env );
    gchar*      ipfilter_v6_config_file  =  nwamui_env_get_ipfilter_v6_config_file ( env );
    gchar*      ipnat_config_file  =  nwamui_env_get_ipnat_config_file ( env );
    gchar*      ippool_config_file  =  nwamui_env_get_ippool_config_file ( env );
    gchar*      ike_config_file  =  nwamui_env_get_ike_config_file ( env );
    gchar*      ipsecpolicy_config_file  =  nwamui_env_get_ipsecpolicy_config_file ( env );
#ifdef ENABLE_NETSERVICES
    GList*      svcs_enable  =  nwamui_env_get_svcs_enable ( env );
    GList*      svcs_disable  =  nwamui_env_get_svcs_disable ( env );
#endif /* ENABLE_NETSERVICES */
    
    name = nwamui_object_get_name(NWAMUI_OBJECT(env));
    printf("%-*s*************************************************************\n", indent, "");
    indent += 4;
    printf("%-*sEnv name = %s\n", indent, "", name?name:"NULL" );
    printf("%-*sEnv active  = %s\n", indent, "", active ?"TRUE":"FALSE" );
    printf("%-*sEnv enabled  = %s\n", indent, "", enabled ?"TRUE":"FALSE" );
    print_ns_list_and_free( "Env", "nameservices", nameservices );
    printf("%-*sEnv nameservices_config_file  = %s\n", indent, "", nameservices_config_file ?nameservices_config_file :"NULL" );
    printf("%-*sEnv default_domainname  = %s\n", indent, "", default_domainname ?default_domainname :"NULL" );
    printf("%-*sEnv dns_nameservice_config_source  = %s\n", indent, "", config_source_to_str(dns_config_source));
    printf("%-*sEnv dns_nameservice_domain  = %s\n", indent, "", dns_nameservice_domain ?dns_nameservice_domain :"NULL" );
    print_string_list_and_free( "Env", "dns_nameservice_servers", dns_nameservice_servers );
    print_string_list_and_free( "Env", "dns_nameservice_search", dns_nameservice_search );
    printf("%-*sEnv nis_nameservice_config_source  = %s\n", indent, "", config_source_to_str(nis_config_source));
    print_string_list_and_free( "Env", "nis_nameservice_servers", nis_nameservice_servers );
    printf("%-*sEnv ldap_nameservice_config_source  = %s\n", indent, "", config_source_to_str(ldap_config_source));
    print_string_list_and_free( "Env", "ldap_nameservice_servers", ldap_nameservice_servers );
#ifndef _DISABLE_HOSTS_FILE
    printf("%-*sEnv hosts_file  = %s\n", indent, "", hosts_file ?hosts_file :"NULL" );
#endif /* _DISABLE_HOSTS_FILE */
    printf("%-*sEnv nfsv4_domain  = %s\n", indent, "", nfsv4_domain ?nfsv4_domain :"NULL" );
    printf("%-*sEnv ipfilter_config_file  = %s\n", indent, "", ipfilter_config_file ?ipfilter_config_file :"NULL" );
    printf("%-*sEnv ipfilter_v6_config_file  = %s\n", indent, "", ipfilter_v6_config_file ?ipfilter_v6_config_file :"NULL" );
    printf("%-*sEnv ipnat_config_file  = %s\n", indent, "", ipnat_config_file ?ipnat_config_file :"NULL" );
    printf("%-*sEnv ippool_config_file  = %s\n", indent, "", ippool_config_file ?ippool_config_file :"NULL" );
    printf("%-*sEnv ike_config_file  = %s\n", indent, "", ike_config_file ?ike_config_file :"NULL" );
    printf("%-*sEnv ipsecpolicy_config_file  = %s\n", indent, "", ipsecpolicy_config_file ?ipsecpolicy_config_file :"NULL" );
#ifdef ENABLE_NETSERVICES
    print_string_list_and_free( "Env", "svcs_enable", svcs_enable );
    print_string_list_and_free( "Env", "svcs_disable", svcs_disable );
#endif /* ENABLE_NETSERVICES */

    print_conditions( G_OBJECT(env) );

    indent -= 4;
    printf("%-*s*************************************************************\n", indent, "");
    if ( nwamui_env_has_modifications( env ) ) {
        g_error("Object shouldn't have modifications");
    }
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
    const gchar* name;
    gboolean     active;
    gboolean     enabled;
    gchar*       start_command;
    gchar*       stop_command;
    gchar*       smf_frmi;

    printf("%-*s*************************************************************\n", indent, "");

    indent += 4;

    name = nwamui_object_get_name(NWAMUI_OBJECT(enm));
    printf("%-*sNwamuiEnm : name = %s\n", indent, "", name?name:"NULL" );


    active = nwamui_object_get_active(NWAMUI_OBJECT(enm));
    printf("%-*sNwamuiEnm : active = %s\n", indent, "", active?"True":"False" );


    start_command = nwamui_enm_get_start_command ( NWAMUI_ENM(enm) );
    printf("%-*sNwamuiEnm : start_command = %s\n", indent, "", start_command?start_command:"NULL" );


    stop_command = nwamui_enm_get_stop_command ( NWAMUI_ENM(enm) );
    printf("%-*sNwamuiEnm : stop_command = %s\n", indent, "", stop_command?stop_command:"NULL" );


    smf_frmi = nwamui_enm_get_smf_fmri ( NWAMUI_ENM(enm) );
    printf("%-*sNwamuiEnm : smf_frmi = %s\n", indent, "", smf_frmi?smf_frmi:"NULL" );

    enabled = nwamui_object_get_enabled ( NWAMUI_OBJECT(enm) );
    printf("%-*sNwamuiEnm : enabled = %s\n", indent, "", enabled?"True":"False" );

    print_conditions( G_OBJECT(enm) );

    indent -= 4;

    printf("%-*s*************************************************************\n", indent, "");

    if ( nwamui_enm_has_modifications( enm ) ) {
        g_error("Object shouldn't have modifications");
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

static void 
process_known_wlan( gpointer data, gpointer user_data ) 
{
    NwamuiDaemon    *daemon = nwamui_daemon_get_instance();
    NwamuiWifiNet   *wifi = NWAMUI_WIFI_NET(data);
    const gchar     *signal_str;
    
    if ( wifi != NULL ) {
        const gchar *essid          = nwamui_object_get_name(NWAMUI_OBJECT(wifi));
        GList       *fav_bssid_list = nwamui_wifi_net_get_fav_bssid_list( wifi );
        GList       *bssid_list     = nwamui_wifi_net_get_bssid_list( wifi );

        printf("%-*s=============================================================\n", indent, "");
        indent +=4;
        printf("%-*sWLAN : essid = %s\n", indent, "", essid?essid:"NULL" );

        signal_str = nwamui_wifi_net_get_signal_strength_string( wifi );

        printf("%-*sWLAN : signal = %s\n", indent, "", signal_str?signal_str:"NULL" );

        print_string_list_and_free( "WLAN", "fav_bssid_list", fav_bssid_list );
        print_string_list_and_free( "WLAN", "bssid_list", bssid_list );

        indent -=4;
        printf("%-*s=============================================================\n", indent, "");

        if ( nwamui_wifi_net_has_modifications( wifi ) ) {
            g_error("Object shouldn't have modifications");
        }
        g_object_unref(G_OBJECT(wifi));
    }

    g_object_unref(G_OBJECT(daemon));
    
}

static void
test_known_wlan_gobject( void )
{
    NwamuiDaemon    *daemon = nwamui_daemon_get_instance();
    
    GList           *known_wlan_list = nwamui_daemon_get_fav_wifi_networks(daemon);
    
    printf("%-*s=============================================================\n", indent, "");
    printf("%-*s WLANs \n", indent, "");
    printf("%-*s=============================================================\n", indent, "");

    indent += 4;
    g_list_foreach(known_wlan_list, process_known_wlan, NULL );
    indent -= 4;
    
    g_list_foreach(known_wlan_list, nwamui_util_obj_unref, NULL );
    g_list_free( known_wlan_list );
    g_object_unref(G_OBJECT(daemon));
}

static void
scan_started(GObject *daemon, gpointer data)
{
    g_debug( "** Scan Started **");
}

static void
scan_result (GObject *daemon, GObject *wifi, gpointer data)
{
	if (wifi) {
        printf( "** Scan RESULT START **\n");
        indent += 4;
        process_known_wlan( (gpointer) wifi, NULL);
        indent -= 4;
        printf( "** Scan RESULT END   **\n");
	} else {
        printf("** END OF SCAN RESULTS **\n");
        gtk_main_quit();
    }
}

