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
#include <glade/glade-xml.h>

#include <libnwamui.h>
#include "nwam_tree_view.h"
#include "nwam_wireless_dialog.h"
#include "nwam_env_pref_dialog.h"
#include "nwam_vpn_pref_dialog.h"
#include "nwam_wireless_chooser.h"
#include "nwam_pref_dialog.h"
#include "nwam_location_dialog.h"
#include "nwam_condition_vbox.h"
#include "capplet-utils.h"

/* Command-line options */
static gboolean     debug = FALSE;
static gboolean     loc_pref_dialog = FALSE;
static gboolean     vpn_pref_dialog = FALSE;
static gboolean     net_pref_dialog = TRUE;

#ifdef DEBUG_OPTS
static gchar       *add_wireless_dialog = NULL;
static gboolean     location_dialog = FALSE;
static gboolean     show_all_widgets = FALSE;
static gboolean     wireless_chooser = FALSE;
#endif /* DEBUG_OPTS */

static void debug_response_id( gint responseid );

GOptionEntry application_options[] = {
        { "loc-pref-dialog", 'l', 0, G_OPTION_ARG_NONE, &loc_pref_dialog, "Show 'Location Preferences' Dialog", NULL  },
        { "net-pref-dialog", 'p', 0, G_OPTION_ARG_NONE, &net_pref_dialog, "Show 'Network Preferences' Dialog", NULL  },
        { "vpn-pref-dialog", 'n', 0, G_OPTION_ARG_NONE, &vpn_pref_dialog, "Show 'VPN Preferences' Dialog", NULL  },
        {"debug", 0, 0, G_OPTION_ARG_NONE, &debug, N_("Enable debugging messages"), NULL },
#ifdef DEBUG_OPTS
        { "wireless-chooser", 'c', 0, G_OPTION_ARG_NONE, &wireless_chooser, "Show 'Wireless Network Chooser' Dialog", NULL  },
        { "location-dialog", 'L', 0, G_OPTION_ARG_NONE, &location_dialog, "Show 'Location Dialog' Dialog", NULL  },
        { "add-wireless-dialog", 'w', 0, G_OPTION_ARG_STRING, &add_wireless_dialog, "Show 'Add Wireless' Dialog", NULL  },
        { "show-all", 'a', 0, G_OPTION_ARG_NONE, &show_all_widgets, "Show all widgets", NULL  },
#endif /* DEBUG_OPTS */
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

static GtkWidget*
customwidgethandler(GladeXML *xml,
  gchar *func_name,
  gchar *name,
  gchar *string1,
  gchar *string2,
  gint int1,
  gint int2,
  gpointer user_data)
{
    if (g_ascii_strcasecmp(name, "address_table") == 0 ||
      g_ascii_strcasecmp(name, "connection_status_table") == 0 ||
      g_ascii_strcasecmp(name, "location_tree") == 0 ||
      g_ascii_strcasecmp(name, "vpn_apps_list") == 0 ||
      g_ascii_strcasecmp(name, "network_profile_table") == 0) {
        g_debug("CUSTOMIZED WIDGET %s", name);
        return nwam_tree_view_new();
    }

    return NULL;
}

/*
 * 
 */
int
main(int argc, char** argv) 
{
    GnomeProgram*   program = NULL;
    GOptionContext*	option_context = NULL;
    GError*         err = NULL;
    NwamPrefIFace *capplet_dialog = NULL;
    
    /* Initialise Thread Support */
    g_thread_init( NULL );
    
    /* Initialize i18n support */
    gtk_set_locale ();
    
    /* Setup log handler to trap debug messages */
    g_log_set_default_handler( default_log_handler, NULL );

    option_context = g_option_context_new("nwam-manager-properties");
    g_option_context_add_main_entries(option_context, application_options, NULL);

    program = gnome_program_init (PACKAGE, VERSION, LIBGNOMEUI_MODULE,
                                  argc, argv,
                                  GNOME_PARAM_GOPTION_CONTEXT, option_context,
                                  GNOME_PARAM_APP_DATADIR, NWAM_MANAGER_DATADIR,
                                  GNOME_PARAM_NONE);

    gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
                           NWAM_MANAGER_DATADIR G_DIR_SEPARATOR_S "icons");

#if 0
    /*
     * FIXME, we probably don't need this any more if we are using
     * gnome_program_init.
     */
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
#endif

    glade_set_custom_handler(customwidgethandler, NULL);

    if( loc_pref_dialog ) {
        capplet_dialog = NWAM_PREF_IFACE(nwam_env_pref_dialog_new());
        
        gint responseid = capplet_dialog_run(capplet_dialog, NULL);
        
        debug_response_id( responseid );
    }
    else if( net_pref_dialog ) {
        capplet_dialog = NWAM_PREF_IFACE(nwam_capplet_dialog_new());
        
        gint responseid = capplet_dialog_run(capplet_dialog, NULL);
        
        debug_response_id( responseid );
    }
    else if( vpn_pref_dialog ) {
        capplet_dialog = NWAM_PREF_IFACE(nwam_vpn_pref_dialog_new());
        
        gint responseid = capplet_dialog_run(capplet_dialog, NULL);

        debug_response_id( responseid );
    }
#ifdef DEBUG_OPTS
    else if( wireless_chooser ) {
        capplet_dialog = NWAM_PREF_IFACE(nwam_wireless_chooser_new());
        
        gint responseid = capplet_dialog_run(capplet_dialog, NULL);
        
        debug_response_id( responseid );
    }
    else if( location_dialog ) {
        capplet_dialog = NWAM_PREF_IFACE(nwam_location_dialog_new());
        
        gint responseid = capplet_dialog_run(capplet_dialog, NULL);
        
        debug_response_id( responseid );
    }
    else if ( add_wireless_dialog ) {
        capplet_dialog = NWAM_PREF_IFACE(nwam_wireless_dialog_new());
        
#if 0
        static NwamWirelessDialog *wifi_dialog = NULL;
        NwamuiDaemon *daemon = nwamui_daemon_get_instance();
        NwamuiNcp *ncp = nwamui_daemon_get_active_ncp(daemon);
        NwamuiNcu *ncu = NULL;

        /* TODO popup key dialog */
        if (wifi_dialog == NULL) {
            wifi_dialog = nwam_wireless_dialog_new();
        }

        /* ncu could be NULL due to daemon may not know the active llp */
        if ((ncu = nwamui_ncp_get_active_ncu(ncp)) == NULL ||
          nwamui_ncu_get_ncu_type(ncu) != NWAMUI_NCU_TYPE_WIRELESS) {
            /* we need find a wireless interface */
            nwamui_ncp_foreach_ncu(ncp,
              (GtkTreeModelForeachFunc)find_wireless_interface,
              (gpointer)&ncu);
        }

        g_object_unref(ncp);
        g_object_unref(daemon);

        if (ncu && nwamui_ncu_get_ncu_type(ncu) == NWAMUI_NCU_TYPE_WIRELESS) {
            gchar *device = nwamui_ncu_get_device_name(ncu);
            nwam_wireless_dialog_set_ncu(wifi_dialog, device);
            g_free(device);
        } else {
            if (ncu) {
                g_object_unref(ncu);
            }
            return;
        }

        /* wifi may be NULL -- join a wireless */
        {
            gchar *name = NULL;
            if (wifi) {
                name = nwamui_wifi_net_get_unique_name(wifi);
            }
            g_debug("%s ## wifi 0x%p %s", __func__, wifi, name ? name : "nil");
            g_free(name);
        }
        nwam_wireless_dialog_set_wifi_net(wifi_dialog, wifi);

        if (capplet_dialog_run(NWAM_PREF_IFACE(wifi_dialog), NULL) == GTK_RESPONSE_OK) {
            gchar *name = NULL;
            gchar *passwd = NULL;

            /* get new wifi */
            wifi = nwam_wireless_dialog_get_wifi_net(wifi_dialog);
            g_assert(wifi);

            switch (nwam_wireless_dialog_get_security(wifi_dialog)) {
            case NWAMUI_WIFI_SEC_NONE:
                break;
            case NWAMUI_WIFI_SEC_WEP_HEX:
            case NWAMUI_WIFI_SEC_WEP_ASCII:
                passwd = nwam_wireless_dialog_get_key(wifi_dialog);
                nwamui_wifi_net_set_wep_password(wifi, passwd);
                g_debug("\nWIRELESS WEP '%s'", passwd);
                break;
            case NWAMUI_WIFI_SEC_WPA_PERSONAL:
            case NWAMUI_WIFI_SEC_WPA_ENTERPRISE:
                name = nwam_wireless_dialog_get_wpa_username(wifi_dialog);
                passwd = nwam_wireless_dialog_get_wpa_password(wifi_dialog);
                nwamui_wifi_net_set_wpa_username(wifi, name);
                nwamui_wifi_net_set_wpa_password(wifi, passwd);
                g_debug("\nWIRELESS WAP '%s : %s'", name, passwd);
                break;
            default:
                g_assert_not_reached();
                break;
            }

            g_free(name);
            g_free(passwd);

            /* try to connect */
            if (nwamui_wifi_net_get_status(wifi) != NWAMUI_WIFI_STATUS_CONNECTED) {
                nwamui_ncu_set_wifi_info(ncu, wifi);
                nwamui_wifi_net_connect(wifi);
            } else {
                g_debug("wireless is connected, just need a key!!!");
            }
        }

        g_object_unref(ncu);
#endif    

        if (*add_wireless_dialog != '\0') {
            nwam_wireless_dialog_set_essid (NWAM_WIRELESS_DIALOG(capplet_dialog), add_wireless_dialog);
        }
        gint responseid = capplet_dialog_run(capplet_dialog, NULL);
        
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
#endif /* DEBUG_OPTS */

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

