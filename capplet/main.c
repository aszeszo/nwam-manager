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

/* Command-line options */
static gboolean debug = FALSE;

static gboolean     show_all_widgets = FALSE;
//static gboolean     add_wireless_dialog = FALSE;
static gchar        *add_wireless_dialog = NULL;
static gboolean     env_pref_dialog = FALSE;
static gboolean     location_dialog = FALSE;
static gboolean     vpn_pref_dialog = FALSE;
static gboolean     nwam_pref_dialog = TRUE;
static gboolean     wireless_chooser = FALSE;

static void debug_response_id( gint responseid );

GOptionEntry application_options[] = {
        {"debug", 0, 0, G_OPTION_ARG_NONE, &debug, N_("Enable debugging messages"), NULL },
        { "show-all", 'a', 0, G_OPTION_ARG_NONE, &show_all_widgets, "Show all widgets", NULL  },
        { "add-wireless-dialog", 'w', 0, G_OPTION_ARG_STRING, &add_wireless_dialog, "Show 'Add Wireless' Dialog only", NULL  },
        { "env-pref-dialog", 'e', 0, G_OPTION_ARG_NONE, &env_pref_dialog, "Show 'Location Preferences' Dialog only", NULL  },
        { "location-dialog", 'l', 0, G_OPTION_ARG_NONE, &location_dialog, "Show 'Location Dialog' Dialog only", NULL  },
        { "nwam-pref-dialog", 'p', 0, G_OPTION_ARG_NONE, &nwam_pref_dialog, "Show 'Network Preferences' Dialog only", NULL  },
        { "vpn-pref-dialog", 'n', 0, G_OPTION_ARG_NONE, &vpn_pref_dialog, "Show 'VPN Preferences' Dialog only", NULL  },
        { "wireless-chooser", 'c', 0, G_OPTION_ARG_NONE, &wireless_chooser, "Show 'Wireless Network Chooser' Dialog only", NULL  },
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

    if ( add_wireless_dialog ) {
        capplet_dialog = NWAM_PREF_IFACE(nwam_wireless_dialog_new());
        
        if (*add_wireless_dialog != '\0') {
            nwam_wireless_dialog_set_essid (NWAM_WIRELESS_DIALOG(capplet_dialog), add_wireless_dialog);
        }
        gint responseid = capplet_dialog_run(capplet_dialog, NULL);
        
        debug_response_id( responseid );
    }
    else if( env_pref_dialog ) {
        capplet_dialog = NWAM_PREF_IFACE(nwam_env_pref_dialog_new());
        
        gint responseid = capplet_dialog_run(capplet_dialog, NULL);
        
        debug_response_id( responseid );
    }
    else if( location_dialog ) {
        capplet_dialog = NWAM_PREF_IFACE(nwam_location_dialog_new());
        
        gint responseid = capplet_dialog_run(capplet_dialog, NULL);
        
        debug_response_id( responseid );
    }
    else if( vpn_pref_dialog ) {
        capplet_dialog = NWAM_PREF_IFACE(nwam_vpn_pref_dialog_new());
        
        gint responseid = capplet_dialog_run(capplet_dialog, NULL);

        debug_response_id( responseid );
    }
    else if( wireless_chooser ) {
        capplet_dialog = NWAM_PREF_IFACE(nwam_wireless_chooser_new());
        
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
    else if( nwam_pref_dialog ) {
        capplet_dialog = NWAM_PREF_IFACE(nwam_capplet_dialog_new());
        
        gint responseid = capplet_dialog_run(capplet_dialog, NULL);
        
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

