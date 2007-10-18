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
 * File:   libnwamui.c
 *
 * Created on May 23, 2007, 10:26 AM
 * 
 */

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libnwamui.h>
#include <glade/glade.h>
#include <libgnome/libgnome.h>

#define NWAM_MANAGER_PROPERTIES_GLADE_FILE  "nwam-manager-properties.glade"

#define NWAM_ENVIRONMENT_RENAME     "nwam_environment_rename"
#define RENAME_ENVIRONMENT_ENTRY    "rename_environment_entry"

struct nwamui_wifi_essid {
    gchar*                          essid;
    nwamui_wifi_security_t          security;
    gchar*                          bssid;
    nwamui_wifi_signal_strength_t   signal_strength;
    nwamui_wifi_wpa_config_t        wpa_config;
};

typedef struct nwamui_wifi_essid nwamui_wifi_essid_t;


/* Load the Glade file and maintain a single reference */
static GladeXML *glade_xml_tree = NULL;

static GladeXML* 
get_glade_xml( void ) {
    static const gchar *build_datadir = NWAM_MANAGER_DATADIR;

    if ( glade_xml_tree == NULL ) {
        const gchar * const *sys_data_dirs;
        const gchar *glade_file;
        gint i;

        sys_data_dirs = g_get_system_data_dirs ();
        if ( g_file_test(build_datadir, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_DIR) ) {
            gchar *glade_file;
            /* First try with the package name in the path */
            glade_file = g_build_filename (build_datadir, PACKAGE, NWAM_MANAGER_PROPERTIES_GLADE_FILE, NULL );
            g_debug("Attempting to load : %s", glade_file);
            if ( (glade_xml_tree = glade_xml_new(glade_file, NULL, NULL)) != NULL ) {
                g_debug("Found glade file at : %s", glade_file );
            }
            g_free (glade_file);
            if ( glade_xml_tree == NULL ) {
                /* Now try without it */
                glade_file = g_build_filename (build_datadir, NWAM_MANAGER_PROPERTIES_GLADE_FILE, NULL );
                g_debug("Attempting to load : %s", glade_file);
                if ( (glade_xml_tree = glade_xml_new(glade_file, NULL, NULL)) != NULL ) {
                    g_debug("Found glade file at : %s", glade_file );
                    g_free (glade_file);
                }
            }
        }
        if (glade_xml_tree == NULL ) {
            for (i = 0; sys_data_dirs[i] != NULL; i++) {
                gchar *glade_file;
                /* First try with the package name in the path */
                glade_file = g_build_filename (sys_data_dirs[i], PACKAGE, NWAM_MANAGER_PROPERTIES_GLADE_FILE, NULL );
                g_debug("Attempting to load : %s", glade_file);
                if ( (glade_xml_tree = glade_xml_new(glade_file, NULL, NULL)) != NULL ) {
                    g_debug("Found glade file at : %s", glade_file );
                    break;
                }
                g_free (glade_file);
                /* Now try without it */
                glade_file = g_build_filename (sys_data_dirs[i], NWAM_MANAGER_PROPERTIES_GLADE_FILE, NULL );
                g_debug("Attempting to load : %s", glade_file);
                if ( (glade_xml_tree = glade_xml_new(glade_file, NULL, NULL)) != NULL ) {
                    g_debug("Found glade file at : %s", glade_file );
                    g_free (glade_file);
                    break;
                }
            }
        }

        g_assert( glade_xml_tree != NULL );
    }
    return glade_xml_tree;
}

/**
 * nwamui_util_glade_get_widget:
 * @widget_name: name of the widget to load.
 * @returns: the widget loaded from the Glade file.
 *
 **/
extern GtkWidget*
nwamui_util_glade_get_widget( const gchar* widget_name ) 
{
    GladeXML*   xml;
    GtkWidget*  widget;

    g_assert( widget_name != NULL );
    
    g_return_val_if_fail( widget_name != NULL, NULL );
    
    xml = get_glade_xml();
    g_return_val_if_fail( xml != NULL, NULL );
    
    widget = glade_xml_get_widget(xml, widget_name );
    
    if ( widget == NULL )
        g_debug("Failed to get widget by name %s", widget_name );
    
    g_assert( widget != NULL );
    
    return widget;
}
        
/**
 * nwamui_util_wifi_sec_to_string:
 * @wireless_sec: a #nwamui_wifi_security_t.
 * @returns: a localized string representation of the security type
 *
 **/
extern gchar*
nwamui_util_wifi_sec_to_string( nwamui_wifi_security_t wireless_sec ) {
    g_assert( wireless_sec >= 0 && wireless_sec < NWAMUI_WIFI_SEC_LAST );
    
    switch( wireless_sec ) {
        case NWAMUI_WIFI_SEC_WEP_HEX:           return(_("WEP Hex"));
        case NWAMUI_WIFI_SEC_WEP_ASCII:         return(_("WEP ASCII"));
        case NWAMUI_WIFI_SEC_WPA_PERSONAL:      return(_("WPA Personal(PSK)"));
        case NWAMUI_WIFI_SEC_WPA_ENTERPRISE:    return(_("WPA Enterprise (Radius)"));
        default:                                return(_("None") );
    }
}

/**
 * nwamui_util_wifi_sec_to_short_string:
 * @wireless_sec: a #nwamui_wifi_security_t.
 * @returns: a short localized string representation of the security type
 *
 **/
extern gchar*
nwamui_util_wifi_sec_to_short_string( nwamui_wifi_security_t wireless_sec ) {
    g_assert( wireless_sec >= 0 && wireless_sec < NWAMUI_WIFI_SEC_LAST );
    
    switch( wireless_sec ) {
        case NWAMUI_WIFI_SEC_WEP_HEX:           return(_("WEP"));
        case NWAMUI_WIFI_SEC_WEP_ASCII:         return(_("WEP"));
        case NWAMUI_WIFI_SEC_WPA_PERSONAL:      return(_("WPA"));
        case NWAMUI_WIFI_SEC_WPA_ENTERPRISE:    return(_("WPA"));
        default:                                return(_("Open") );
    }
}


/**
 * nwamui_util_wifi_wpa_config_to_string:
 * @wpa_config: a #nwamui_wifi_wpa_config_t.
 * @returns: a localized string representation of the WPA Config mechanism.
 *
 **/
extern gchar*
nwamui_util_wifi_wpa_config_to_string( nwamui_wifi_wpa_config_t wpa_config ) {
    g_assert( wpa_config >= 0 && wpa_config < NWAMUI_WIFI_WPA_CONFIG_LAST );
    
    switch( wpa_config ) {
        case NWAMUI_WIFI_WPA_CONFIG_LEAP:  	return(_("LEAP")); 
        case NWAMUI_WIFI_WPA_CONFIG_PEAP:  	return(_("PEAP/MSCHAPv2")); 
        case NWAMUI_WIFI_WPA_CONFIG_AUTOMATIC:	/*fall through*/
        default:                                return(_("Automatic"));
    }
}

/**
 * nwamui_util_obj_ref
 *
 * Utility function, for use in g_list_foreach calls to g_object_ref each element.
 *
 **/
extern void
nwamui_util_obj_ref( gpointer obj, gpointer user_data )
{
    g_return_if_fail( G_IS_OBJECT(obj));
    
    g_object_ref( G_OBJECT(obj));
}

/**
 * nwamui_util_obj_unref
 *
 * Utility function, for use in g_list_foreach calls to g_object_unref each element.
 *
 **/
extern void
nwamui_util_obj_unref( gpointer obj, gpointer user_data )
{
    g_return_if_fail( G_IS_OBJECT(obj));
    
    g_object_unref( G_OBJECT(obj));
}


/**
 * nwamui_util_free_obj_list
 *
 * Utility function, given a GList of objects, will unref each and then free the list.
 *
 **/
extern void
nwamui_util_free_obj_list( GList*   obj_list )
{
    g_return_if_fail( obj_list != NULL );
    
    g_list_foreach(obj_list, nwamui_util_obj_unref, NULL );
    
    g_list_free( obj_list );
}

/**
 * nwamui_util_copy_obj_list
 *
 * Utility function, given a GList of objects, will duplicate the list and then ref() each element.
 *
 **/
extern GList*
nwamui_util_copy_obj_list( GList*   obj_list )
{
    GList*  new_list = NULL;
    
    g_return_if_fail( obj_list != NULL );
    
    new_list = g_list_copy( obj_list );
    
    g_list_foreach(new_list, nwamui_util_obj_ref, NULL );    
}

/* 
 * Returns a GdkPixbuf that reflects the type of the network.
 */
extern GdkPixbuf*
nwamui_util_get_network_type_icon( nwamui_ncu_type_t ncu_type )
{
        static GdkPixbuf       *wireless_icon = NULL;
        static GdkPixbuf       *wired_icon = NULL;

        GtkIconTheme           *icon_theme;
        
        icon_theme = gtk_icon_theme_get_default();
        if ( wireless_icon == NULL ) {
            wireless_icon = gtk_icon_theme_load_icon (GTK_ICON_THEME(icon_theme),
                                         "network-wireless", 32, 0, NULL);
        }
        if (wired_icon == NULL ) {
            wired_icon = gtk_icon_theme_load_icon (GTK_ICON_THEME(icon_theme),
                                           "network-idle", 32, 0, NULL);
        }
        
        switch (ncu_type) {
            case NWAMUI_NCU_TYPE_WIRELESS:
                return( GDK_PIXBUF(g_object_ref(G_OBJECT(wireless_icon) )) );
            case NWAMUI_NCU_TYPE_WIRED: 
                /* Fall-through */
            default:
                return( GDK_PIXBUF(g_object_ref(G_OBJECT(wired_icon) )) );
        }
}
       

/* 
 * Returns a GdkPixbuf that reflects the status of the network.
 */
extern GdkPixbuf*
nwamui_util_get_network_status_icon( NwamuiNcu* ncu )
{
        static GdkPixbuf       *enabled_wired_icon = NULL;
        static GdkPixbuf       *enabled_wireless_icon = NULL;
        static GdkPixbuf       *disabled_icon = NULL;

        nwamui_ncu_type_t       ncu_type;
        gboolean                enabled;
        
        
        g_return_val_if_fail( NWAMUI_IS_NCU( ncu ), NULL );
        
        ncu_type = nwamui_ncu_get_ncu_type(ncu);
        enabled = nwamui_ncu_get_active(ncu);

        if ( enabled_wired_icon == NULL ) { /* Load all icons */

            enabled_wired_icon = nwam_ui_icons_get_pixbuf(nwam_ui_icons_get_instance(), NWAM_ICON_NETWORK_IDLE );

            
            enabled_wireless_icon = nwam_ui_icons_get_pixbuf(nwam_ui_icons_get_instance(), NWAM_ICON_NETWORK_IDLE );
            
            
            disabled_icon = nwam_ui_icons_get_pixbuf(nwam_ui_icons_get_instance(), NWAM_ICON_NETWORK_OFFLINE );
        }
        
        /* TODO - handle other status icon cases */
        if ( enabled ) {
            switch (ncu_type) {
                case NWAMUI_NCU_TYPE_WIRELESS:
                    return( GDK_PIXBUF(g_object_ref( G_OBJECT(enabled_wireless_icon) )) );
                case NWAMUI_NCU_TYPE_WIRED: 
                    /* Fall-through */
                default:
                    return( GDK_PIXBUF(g_object_ref( G_OBJECT(enabled_wired_icon) )) );
            }
        }
        else {
            return( GDK_PIXBUF(g_object_ref( G_OBJECT(disabled_icon) )) );
        }
}
       
extern GdkPixbuf*
nwamui_util_get_wireless_strength_icon( NwamuiNcu* ncu  )
{
    static GdkPixbuf*   enabled_wireless_icons[NWAMUI_WIFI_STRENGTH_LAST];
    
    nwamui_wifi_signal_strength_t   signal_strength;
    
    g_return_val_if_fail( NWAMUI_IS_NCU(ncu) 
                          && nwamui_ncu_get_ncu_type(ncu) == NWAMUI_NCU_TYPE_WIRELESS, NULL );
    
    signal_strength = nwamui_ncu_get_wifi_signal_strength( ncu );
    
    if ( enabled_wireless_icons[NWAMUI_WIFI_STRENGTH_NONE] == NULL ) {
        /* TODO - Get wireless icons to match the various strengths */
        enabled_wireless_icons[NWAMUI_WIFI_STRENGTH_NONE] = /* Same as below for now, but need new icons */
        enabled_wireless_icons[NWAMUI_WIFI_STRENGTH_VERY_WEAK] = 
                                    nwam_ui_icons_get_pixbuf(nwam_ui_icons_get_instance(), NWAM_ICON_WIRELESS_STRENGTH_0_24 );

        enabled_wireless_icons[NWAMUI_WIFI_STRENGTH_WEAK] = 
                                    nwam_ui_icons_get_pixbuf(nwam_ui_icons_get_instance(), NWAM_ICON_WIRELESS_STRENGTH_25_49 );

        enabled_wireless_icons[NWAMUI_WIFI_STRENGTH_GOOD] = 
                                    nwam_ui_icons_get_pixbuf(nwam_ui_icons_get_instance(), NWAM_ICON_WIRELESS_STRENGTH_50_74 );

        enabled_wireless_icons[NWAMUI_WIFI_STRENGTH_VERY_GOOD] = /* Same as below for now, but need new icons */
        enabled_wireless_icons[NWAMUI_WIFI_STRENGTH_EXCELLENT] = 
                                    nwam_ui_icons_get_pixbuf(nwam_ui_icons_get_instance(), NWAM_ICON_WIRELESS_STRENGTH_75_100 );
    }
    
    return( GDK_PIXBUF(g_object_ref( G_OBJECT(enabled_wireless_icons[signal_strength]) )) );
    
}

/* 
 * Shows Help Dialog - the link_name refers to either the anchor or sectionid in the help file.
 */
extern void
nwamui_util_show_help( gchar* link_name )
{
  GError *error = NULL;
  
  gnome_help_display("nwam-manager", link_name, &error);
  
  if (error) {
    GtkWidget *dialog;
    dialog = gtk_message_dialog_new_with_markup (NULL, GTK_DIALOG_MODAL, 
                   GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
                   "<span weight=\"bold\" size=\"larger\">%s</span>\n\n%s",
                   _("Could not display help"),
                   error->message);
    g_signal_connect (G_OBJECT (dialog), "response",
          G_CALLBACK (gtk_widget_destroy),
          NULL);
    gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
    gtk_widget_show (dialog);

    g_error_free (error);
  }
}

/*
 * Shows a dialog with a single entry like:
 * 
 *      Name: [     ] 
 *
 *      [Cancel] [OK]
 *
 */
gchar* 
nwamui_util_rename_dialog_run(GtkWindow* parent_window, const gchar* title, const gchar* current_name) 
{
    static GtkWidget*   dialog = NULL;
    static GtkWidget*   entry  = NULL;
    
    gint                response;
    gchar*              outstr;

    g_assert( title != NULL && current_name != NULL );

    g_return_val_if_fail( (title != NULL && current_name != NULL), NULL );

    if ( dialog == NULL ) {
        dialog = nwamui_util_glade_get_widget(NWAM_ENVIRONMENT_RENAME);
        entry = nwamui_util_glade_get_widget(RENAME_ENVIRONMENT_ENTRY);
    }

    if (parent_window != NULL) {
        gtk_window_set_transient_for (GTK_WINDOW(dialog), parent_window);
        gtk_window_set_modal (GTK_WINDOW(dialog), TRUE);
    }
    else {
        gtk_window_set_transient_for (GTK_WINDOW(dialog), NULL);
        gtk_window_set_modal (GTK_WINDOW(dialog), FALSE);
    }

    gtk_window_set_title(GTK_WINDOW(dialog), title );
    gtk_entry_set_text(GTK_ENTRY(entry), current_name );

    gtk_widget_show_all(dialog);
    response = gtk_dialog_run( GTK_DIALOG(dialog) );

    switch( response ) {
       case GTK_RESPONSE_OK:
           outstr = g_strdup(gtk_entry_get_text( GTK_ENTRY(entry) ));
           gtk_widget_hide_all(dialog);
           return( outstr );
       default:
           gtk_widget_hide_all(dialog);
           return( NULL );
    }
}

extern gboolean
nwamui_util_ask_yes_no(GtkWindow* parent_window, const gchar* title, const gchar* message) 
{
    GtkWidget*          message_dialog;
    gint                response;
    gchar*              outstr;

    g_assert( message != NULL );

    g_return_val_if_fail( message != NULL, FALSE );

    message_dialog = gtk_message_dialog_new(parent_window, GTK_DIALOG_MODAL |GTK_DIALOG_DESTROY_WITH_PARENT,
                                            GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, message );
    
    if ( title != NULL ) {
        gtk_window_set_title(GTK_WINDOW(message_dialog), title);
    }
    
    gtk_message_dialog_format_secondary_text( GTK_MESSAGE_DIALOG(message_dialog), _("This operation cannot be undone.") );
    
    switch( gtk_dialog_run(GTK_DIALOG(message_dialog)) ) {
        case GTK_RESPONSE_YES:
            gtk_widget_destroy(message_dialog);
            return( TRUE );
        default:
            gtk_widget_destroy(message_dialog);
            return( FALSE );
    }
}

extern void
nwamui_util_show_message(GtkWindow* parent_window, GtkMessageType type, const gchar* title, const gchar* message)
{
    GtkWidget*          message_dialog;
    gint                response;
    gchar*              outstr;

    g_assert( message != NULL );
    
    g_assert( type != GTK_MESSAGE_QUESTION ); /* Should use nwamui_util_ask_yes_no() for this type */

    g_return_if_fail( message != NULL );

    message_dialog = gtk_message_dialog_new(parent_window, GTK_DIALOG_MODAL |GTK_DIALOG_DESTROY_WITH_PARENT,
                                            type, GTK_BUTTONS_CLOSE, message );
    
    if ( title != NULL ) {
        gtk_window_set_title(GTK_WINDOW(message_dialog), title);
    }
    
    /* Ensure dialog is destroryed when user closes it */
    g_signal_connect_swapped (message_dialog, "response", G_CALLBACK (gtk_widget_destroy), message_dialog);
    
   (void)gtk_dialog_run(GTK_DIALOG(message_dialog));
}

