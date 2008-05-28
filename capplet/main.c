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

#include "nwam_wireless_dialog.h"
#include "nwam_env_pref_dialog.h"
#include "nwam_vpn_pref_dialog.h"
#include "nwam_wireless_chooser.h"
#include "nwam_pref_dialog.h"
#include <libnwamui.h>

static gboolean     show_all_widgets = FALSE;
//static gboolean     add_wireless_dialog = FALSE;
static gchar        *add_wireless_dialog = NULL;
static gboolean     env_pref_dialog = FALSE;
static gboolean     vpn_pref_dialog = FALSE;
static gboolean     nwam_pref_dialog = TRUE;
static gboolean     wireless_chooser = FALSE;

static void debug_response_id( gint responseid );
static void test_nwam_wireless_dialog( NwamWirelessDialog* dialog );
static void test_ncu_gobject( void );
static void test_env_gobject( void );
static void test_enm_gobject( void );

GOptionEntry application_options[] = {
        { "show-all", 'a', 0, G_OPTION_ARG_NONE, &show_all_widgets, "Show all widgets", NULL  },
        { "add-wireless-dialog", 'w', 0, G_OPTION_ARG_STRING, &add_wireless_dialog, "Show 'Add Wireless' Dialog only", NULL  },
        { "env-pref-dialog", 'e', 0, G_OPTION_ARG_NONE, &env_pref_dialog, "Show 'Location Preferences' Dialog only", NULL  },
        { "nwam-pref-dialog", 'p', 0, G_OPTION_ARG_NONE, &nwam_pref_dialog, "Show 'Network Preferences' Dialog only", NULL  },
        { "vpn-pref-dialog", 'n', 0, G_OPTION_ARG_NONE, &vpn_pref_dialog, "Show 'VPN Preferences' Dialog only", NULL  },
        { "wireless-chooser", 'c', 0, G_OPTION_ARG_NONE, &wireless_chooser, "Show 'Wireless Network Chooser' Dialog only", NULL  },
        { NULL }
};

/*
 * 
 */
int
main(int argc, char** argv) 
{
    GnomeProgram*   program = NULL;
    GError*         err = NULL;
    
    int         do_tests = (getenv("DO_TESTS") != NULL );
    
    /* Initialise Thread Support */
    g_thread_init( NULL );
    
    /* Initialize i18n support */
    gtk_set_locale ();
    
    g_debug("*** Default Behaviour Changed: Now to see all widgets please use the -a option");

    if ( do_tests )
        g_debug("DO_TESTS is set, will run test functions.");
    else 
        g_debug("DO_TESTS not set");

    program = gnome_program_init (PACKAGE, VERSION, LIBGNOMEUI_MODULE,
                                  argc, argv,
                                  GNOME_PARAM_APP_DATADIR,
                                  NWAM_MANAGER_DATADIR,
                                  NULL);

    if (gtk_init_with_args(&argc, &argv, _("NWAM Configuration Capplet"), application_options, NULL, &err) == FALSE ) {
        if ( err != NULL && err->message != NULL ) {
            g_printerr(err->message);
            g_printerr("\n");
        }
        else {
            g_warning(_("Error initialising application\n"));
        }
        exit(1);
    }

    if ( add_wireless_dialog ) {
        NwamWirelessDialog *wireless_dialog = NWAM_WIRELESS_DIALOG(nwam_wireless_dialog_new());
        
        if ( do_tests ) {
            NwamuiDaemon* daemon = nwamui_daemon_get_instance();
            nwamui_daemon_wifi_start_scan(daemon);

            test_ncu_gobject();
            test_env_gobject();
            test_enm_gobject();
            test_nwam_wireless_dialog( wireless_dialog );
        }
        
	if (*add_wireless_dialog != '\0') {
		nwam_wireless_dialog_set_essid (wireless_dialog, add_wireless_dialog);
	}
        gint responseid = nwam_wireless_dialog_run( (NWAM_WIRELESS_DIALOG(wireless_dialog)) );
        
        debug_response_id( responseid );
    }
    else if( env_pref_dialog ) {
        NwamEnvPrefDialog *env_pref_dialog = NWAM_ENV_PREF_DIALOG(nwam_env_pref_dialog_new());
        
        gint responseid = nwam_env_pref_dialog_run( (NWAM_ENV_PREF_DIALOG(env_pref_dialog)), NULL );
        
        debug_response_id( responseid );
    }
    else if( vpn_pref_dialog ) {
        NwamVPNPrefDialog *vpn_pref_dialog = nwam_vpn_pref_dialog_new();
        
        gint responseid = nwam_vpn_pref_dialog_run( vpn_pref_dialog, NULL );
        
        debug_response_id( responseid );
    }
    else if( wireless_chooser ) {
        NwamWirelessChooser *wifi_chooser = nwam_wireless_chooser_new();
        
        gint responseid = nwam_wireless_chooser_run( wifi_chooser, NULL );
        
        debug_response_id( responseid );
    }
    else if ( show_all_widgets ) { /* Show All */
        gchar*  dialog_names[] = { 
                "nwam_capplet", "nwam_environment",
                "add_wireless_network", "connect_wireless_network",
                "nwam_environment_rename", "vpn_config", 
                NULL
        };
        
        for (int i = 0; dialog_names[i] != NULL; i++) {
            GtkWidget*  dialog = NULL;

            /* get a widget (useful if you want to change something) */
            dialog = nwamui_util_glade_get_widget(dialog_names[i]);
            
            if ( dialog == NULL ) {
                g_debug("Didn't get widget for : %s ", dialog_names[i]);
            }

            gtk_widget_show_all(GTK_WIDGET(dialog));
            gtk_widget_realize(GTK_WIDGET(dialog));
        }
        gtk_main();
    }
    else if( nwam_pref_dialog ) {
        NwamCappletDialog *nwam_pref_dialog = NWAM_CAPPLET_DIALOG(nwam_capplet_dialog_new());
        
        gint responseid = nwam_capplet_dialog_run( (NWAM_CAPPLET_DIALOG(nwam_pref_dialog)) );
        
        debug_response_id( responseid );
    }

/*
 *  Not needed since all the _run calls have own main-loop.
 *
 *   gtk_main();
 */
    
    return (EXIT_SUCCESS);
}

static void debug_response_id( gint responseid ) 
{
    g_debug("Dialog returned response : %d ", responseid );
    switch (responseid) {
        case GTK_RESPONSE_NONE:
            g_debug("GTK_RESPONSE_NONE");
            break;
        case GTK_RESPONSE_REJECT:
            g_debug("GTK_RESPONSE_REJECT");
            break;
        case GTK_RESPONSE_ACCEPT:
            g_debug("GTK_RESPONSE_ACCEPT");
            break;
        case GTK_RESPONSE_DELETE_EVENT:
            g_debug("GTK_RESPONSE_DELETE_EVENT");
            break;
        case GTK_RESPONSE_OK:
            g_debug("GTK_RESPONSE_OK");
            break;
        case GTK_RESPONSE_CANCEL:
            g_debug("GTK_RESPONSE_CANCEL");
            break;
        case GTK_RESPONSE_CLOSE:
            g_debug("GTK_RESPONSE_CLOSE");
            break;
        case GTK_RESPONSE_YES:
            g_debug("GTK_RESPONSE_YES");
            break;
        case GTK_RESPONSE_NO:
            g_debug("GTK_RESPONSE_NO");
            break;
        case GTK_RESPONSE_APPLY:
            g_debug("GTK_RESPONSE_APPLY");
            break;
        case GTK_RESPONSE_HELP:
            g_debug("GTK_RESPONSE_HELP");
            break;
    }
}

static void
test_nwam_wireless_dialog( NwamWirelessDialog* wireless_dialog )
{
    g_debug("Running wireless dialog tests\n");
    {
        gchar *iface = "ath0";
        nwam_wireless_dialog_set_ncu(wireless_dialog, iface);
        g_assert( g_ascii_strcasecmp( nwam_wireless_dialog_get_ncu(wireless_dialog), iface) ==0 );
    }

    {
        gchar *essid = "MyTestESSID";
        nwam_wireless_dialog_set_essid(wireless_dialog, essid);
        g_assert( g_ascii_strcasecmp(nwam_wireless_dialog_get_essid (wireless_dialog ), essid) == 0 );
    }
        

    {
        nwamui_wifi_security_t security = NWAMUI_WIFI_SEC_WEP_HEX;
        nwam_wireless_dialog_set_security (wireless_dialog, security );
        g_assert( nwam_wireless_dialog_get_security (wireless_dialog ) == security );
    }

    {
        gchar* security_key = "12340981723087as15assdf1";
        nwam_wireless_dialog_set_key (wireless_dialog, security_key );
        g_assert( g_ascii_strcasecmp(nwam_wireless_dialog_get_key (wireless_dialog ), security_key ) == 0 );
    }
    
    {
        nwamui_wifi_wpa_config_t wpa_config = NWAMUI_WIFI_WPA_CONFIG_AUTOMATIC;
        nwam_wireless_dialog_set_wpa_config_type (wireless_dialog, wpa_config );
        g_assert( nwam_wireless_dialog_get_wpa_config_type (wireless_dialog ) == wpa_config );
    }

    {
        gchar* wpa_username = "MyWPAUsername";
        nwam_wireless_dialog_set_wpa_username (wireless_dialog, wpa_username );
        g_assert( g_ascii_strcasecmp(nwam_wireless_dialog_get_wpa_username (wireless_dialog ), wpa_username ) == 0 );
    }
    
    {
        gchar* wpa_password = "MyWPAPassword";
        nwam_wireless_dialog_set_wpa_password (wireless_dialog, wpa_password );
        g_assert( g_ascii_strcasecmp(nwam_wireless_dialog_get_wpa_password (wireless_dialog ), wpa_password ) == 0 );
    }
    
    {
        gchar* bssid = "00:11:34:55:11:44";
        nwam_wireless_dialog_set_bssid (wireless_dialog, bssid );
        g_assert( g_ascii_strcasecmp(nwam_wireless_dialog_get_bssid (wireless_dialog ), bssid ) == 0 );
    }
    
    {
        gboolean transient = TRUE;
        nwam_wireless_dialog_set_transient (wireless_dialog, transient);
        g_assert( nwam_wireless_dialog_get_transient (wireless_dialog ) == transient );
    }
}

static void
process_ncu( gpointer data, gpointer user_data ) 
{
    NwamuiNcu           *ncu = NWAMUI_NCU( data );
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
}

static void 
test_ncu_gobject( void )
{
    NwamuiDaemon    *daemon = nwamui_daemon_get_instance();
    NwamuiNcp       *ncp = nwamui_daemon_get_active_ncp(daemon);
    GList           *ncu_list = nwamui_ncp_get_ncu_list_store(ncp);
    
    g_list_foreach(ncu_list, process_ncu, NULL );
    
    
    g_list_foreach(ncu_list, nwamui_util_obj_unref, NULL );
    g_list_free( ncu_list );
    g_object_unref(G_OBJECT(ncp));
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
