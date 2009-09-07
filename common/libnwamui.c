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
 * File:   libnwamui.c
 *
 * Created on May 23, 2007, 10:26 AM
 * 
 */

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <glib/gi18n.h>
#include <libnwamui.h>
#include <glade/glade.h>
#include <libgnome/libgnome.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <inet/ip.h>
#include <inetcfg.h>
#include <libscf.h>

#define USE_GLADE 0

#if USE_GLADE
#define NWAM_MANAGER_PROPERTIES_GLADE_FILE  "nwam-manager-properties.glade"
#else /* Use GtkBuilder */
#define NWAM_MANAGER_PROPERTIES_UI_FILE  "nwam-manager-properties.ui"
#endif


#define NWAM_ENVIRONMENT_RENAME     "nwam_environment_rename"
#define RENAME_ENVIRONMENT_ENTRY    "rename_environment_entry"
#define RENAME_ENVIRONMENT_OK_BTN   "rename_environment_ok_btn"

#define PIXBUF_COMPOSITE_NO_SCALE(src, dest)                            \
    gdk_pixbuf_composite(GDK_PIXBUF(src), GDK_PIXBUF(dest),             \
      0, 0,                                                             \
      gdk_pixbuf_get_width(dest),                                       \
      gdk_pixbuf_get_height(dest),                                      \
      (double)0.0, (double)0.0,                                         \
      (double)gdk_pixbuf_get_width(dest)/gdk_pixbuf_get_width(src),     \
      (double)gdk_pixbuf_get_height(dest)/gdk_pixbuf_get_height(src),   \
      GDK_INTERP_NEAREST, 255)

#if USE_GLADE
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
        g_error("Failed to get widget by name %s", widget_name );
    
    g_assert( widget != NULL );
    
    return widget;
}
        
#else /* Use GtkBuilder */

/* Load the GtkBuilder file and maintain a single reference */
static GtkBuilder *gtk_builder = NULL;

static GtkBuilder* 
get_gtk_builder( void ) {
    static const gchar *build_datadir = NWAM_MANAGER_DATADIR;

    if ( gtk_builder == NULL ) {
        GtkBuilder  *new_builder = gtk_builder_new();
        const gchar * const *sys_data_dirs;
        const gchar *ui_file;
        gint         i;
        GError      *err = NULL;

        sys_data_dirs = g_get_system_data_dirs ();
        if ( g_file_test(build_datadir, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_DIR) ) {
            gchar      *ui_file;

            /* First try with the package name in the path */
            ui_file = g_build_filename (build_datadir, PACKAGE, NWAM_MANAGER_PROPERTIES_UI_FILE, NULL );
            nwamui_debug("Attempting to load : %s", ui_file);
            if ( (gtk_builder_add_from_file(new_builder, ui_file, &err )) != 0 ) {
                nwamui_debug("Found gtk builder file at : %s", ui_file );
                gtk_builder = new_builder;
            }
            else if ( err ) {
                nwamui_debug("Error loading glade file : %s", err->message);
                g_error_free(err);
                err = NULL;
            }
            g_free (ui_file);
            if ( gtk_builder == NULL ) {
                /* Now try without it */
                ui_file = g_build_filename (build_datadir, NWAM_MANAGER_PROPERTIES_UI_FILE, NULL );
                nwamui_debug("Attempting to load : %s", ui_file);
                if ( (gtk_builder_add_from_file(new_builder, ui_file, &err )) != 0 ) {
                    nwamui_debug("Found gtk builder file at : %s", ui_file );
                    gtk_builder = new_builder;
                }
                else if ( err ) {
                    nwamui_debug("Error loading glade file : %s", err->message);
                    g_error_free(err);
                    err = NULL;
                }
                g_free (ui_file);
            }
        }
        if (gtk_builder == NULL ) {
            for (i = 0; sys_data_dirs[i] != NULL; i++) {
                gchar *ui_file;
                /* First try with the package name in the path */
                ui_file = g_build_filename (sys_data_dirs[i], PACKAGE, NWAM_MANAGER_PROPERTIES_UI_FILE, NULL );
                nwamui_debug("Attempting to load : %s", ui_file);
                if ( (gtk_builder_add_from_file(new_builder, ui_file, &err )) != 0 ) {
                    nwamui_debug("Found gtk builder file at : %s", ui_file );
                    gtk_builder = new_builder;
                    g_free (ui_file);
                    break;
                }
                else if ( err ) {
                    nwamui_debug("Error loading glade file : %s", err->message);
                    g_error_free(err);
                    err = NULL;
                }
                g_free (ui_file);
                /* Now try without it */
                ui_file = g_build_filename (sys_data_dirs[i], NWAM_MANAGER_PROPERTIES_UI_FILE, NULL );
                nwamui_debug("Attempting to load : %s", ui_file);
                if ( (gtk_builder_add_from_file(new_builder, ui_file, &err )) != 0 ) {
                    nwamui_debug("Found gtk builder file at : %s", ui_file );
                    gtk_builder = new_builder;
                    g_free (ui_file);
                    break;
                }
                else if ( err ) {
                    nwamui_debug("Error loading glade file : %s", err->message);
                    g_error_free(err);
                    err = NULL;
                }
            }
        }

        if ( gtk_builder == NULL ) {
            nwamui_error("Error locating UI file %s", NWAM_MANAGER_PROPERTIES_UI_FILE );
            g_object_unref( new_builder );
        }
    }
    return gtk_builder;
}

/**
 * nwamui_util_glade_get_widget:
 * @widget_name: name of the widget to load.
 * @returns: the widget loaded from the GktBuilder file.
 *
 * (The glade name is kept for code compatibility, for now)
 **/
extern GtkWidget*
nwamui_util_glade_get_widget( const gchar* widget_name ) 
{
    GtkBuilder* builder;;
    GtkWidget*  widget;

    g_assert( widget_name != NULL );
    
    g_return_val_if_fail( widget_name != NULL, NULL );
    
    builder = get_gtk_builder();
    g_return_val_if_fail( builder != NULL, NULL );
    
    widget = GTK_WIDGET(gtk_builder_get_object(builder, widget_name ));
    
    if ( widget == NULL )
        g_error("Failed to get widget by name %s", widget_name );
    
    return widget;
}
        
#endif /* USE_GLADE */

static gboolean _debug = FALSE;

/*
 * Get/Set debug mode.
 */

extern void
nwamui_util_set_debug_mode( gboolean enabled )
{
    _debug = enabled;
}

extern gboolean
nwamui_util_is_debug_mode( void )
{
    return( _debug );
}

/* 
 * Default log handler setup
 */
static void
default_log_handler( const gchar    *log_domain,
                     GLogLevelFlags  log_level,
                     const gchar    *message,
                     gpointer        unused_data)
{
    /* If "debug" not set then filter out debug messages. */
    if ((log_level & G_LOG_LEVEL_MASK) == G_LOG_LEVEL_DEBUG && !_debug) {
        /* Do nothing */
        return;
    }

    /* Otherwise just log as usual */
    g_log_default_handler (log_domain, log_level, message, unused_data);
}

extern void
nwamui_util_default_log_handler_init( void )
{
    g_log_set_default_handler( default_log_handler, NULL );
}    

/*
 * nwamui_util_wifi_sec_to_string:
 * @wireless_sec: a #nwamui_wifi_security_t.
 * @returns: a localized string representation of the security type
 *
 **/
extern gchar*
nwamui_util_wifi_sec_to_string( nwamui_wifi_security_t wireless_sec ) {
    g_assert( wireless_sec >= 0 && wireless_sec < NWAMUI_WIFI_SEC_LAST );
    
    switch( wireless_sec ) {
#ifdef WEP_ASCII_EQ_HEX 
        case NWAMUI_WIFI_SEC_WEP:               return(_("WEP"));
#else
        case NWAMUI_WIFI_SEC_WEP_HEX:           return(_("WEP Hex"));
        case NWAMUI_WIFI_SEC_WEP_ASCII:         return(_("WEP ASCII"));
#endif /* WEP_ASCII_EQ_HEX */
        case NWAMUI_WIFI_SEC_WPA_PERSONAL:      return(_("WPA Personal(PSK)"));
#if 0
        /* ENTERPRISE currently not supported */
        case NWAMUI_WIFI_SEC_WPA_ENTERPRISE:    return(_("WPA Enterprise (Radius)"));
#endif
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
#ifdef WEP_ASCII_EQ_HEX 
        case NWAMUI_WIFI_SEC_WEP:           return(_("WEP"));
#else
        case NWAMUI_WIFI_SEC_WEP_HEX:           return(_("WEP"));
        case NWAMUI_WIFI_SEC_WEP_ASCII:         return(_("WEP"));
#endif /* WEP_ASCII_EQ_HEX */
        case NWAMUI_WIFI_SEC_WPA_PERSONAL:      return(_("WPA"));
#if 0
        /* ENTERPRISE currently not supported */
        case NWAMUI_WIFI_SEC_WPA_ENTERPRISE:    return(_("WPA"));
#endif
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
    if ( obj_list == NULL ) {
        return;
    }
    
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
    
    g_return_val_if_fail( obj_list != NULL, NULL );
    
    new_list = g_list_copy( obj_list );
    
    g_list_foreach(new_list, nwamui_util_obj_ref, NULL );    

    return( new_list );
}

static gint             small_icon_size = -1;
static gint             normal_icon_size = -1;
static gint             theme_changed_id = -1;
static GtkIconTheme*    icon_theme = NULL;

static void
icon_theme_changed ( GtkIconTheme  *_icon_theme, gpointer data )
{
    g_debug("Theme Changed");
    if ( theme_changed_id != -1 && _icon_theme != icon_theme ) {
        g_signal_handler_disconnect( icon_theme, theme_changed_id );
        theme_changed_id = -1;
        g_object_unref(icon_theme);
        icon_theme = NULL;

        icon_theme = GTK_ICON_THEME(g_object_ref( _icon_theme ));
        theme_changed_id = g_signal_connect (  icon_theme, "changed",
                                               G_CALLBACK (icon_theme_changed), NULL);
    }

    /* Invalidate known sizes now */
    small_icon_size = -1;
    normal_icon_size = -1;
}

static GdkPixbuf*   
get_pixbuf_with_size( const gchar* stock_id, gint size )
{
    GdkPixbuf*      pixbuf = NULL;
    GError*         error = NULL;


    if ( icon_theme == NULL ) {
        icon_theme = GTK_ICON_THEME(g_object_ref(gtk_icon_theme_get_default()));
        theme_changed_id = g_signal_connect (  icon_theme, "changed",
                                               G_CALLBACK (icon_theme_changed), NULL);

    }

    pixbuf = gtk_icon_theme_load_icon( icon_theme, stock_id, 
                                       (size > 0)?(size):(32), 0, &error );

    if ( pixbuf == NULL ) {
        g_debug("get_pixbuf_with_size: pixbuf = NULL stockid = %s", stock_id);
    }
/*     else { */
/*         g_debug("get_pixbuf_with_size: pixbuf loaded"); */
/*     } */
    return( pixbuf );
}

static GdkPixbuf*   
get_pixbuf( const gchar* stock_id, gboolean small )
{
/*     g_debug("get_pixbuf: Seeking %s stock_id = %s", small?"small":"normal", stock_id ); */

    if ( small_icon_size == -1 ) {
        gint dummy;
        if ( !gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &small_icon_size, &dummy) ) {
            small_icon_size=16;
        }
        if ( !gtk_icon_size_lookup(GTK_ICON_SIZE_LARGE_TOOLBAR, &normal_icon_size, &dummy) ) {
            normal_icon_size=24;
        }
        g_debug("get_pixbuf: small_icon_size = %d", small_icon_size );
        g_debug("get_pixbuf: normal_icon_size = %d", normal_icon_size );
    }

    return (get_pixbuf_with_size( stock_id, small?small_icon_size:normal_icon_size ));
}

/* 
 * Returns a GdkPixbuf that reflects the status of the overall environment
 * If force_size equals 0, uses the size of status icon.
 */
extern GdkPixbuf*
nwamui_util_get_env_status_icon( GtkStatusIcon* status_icon, nwamui_env_status_t daemon_status, gint force_size )
{
    gint                      activate_wired_num      = 0;
    gint                      activate_wireless_num   = 0;
    gint                      average_signal_strength = 0;
    nwamui_ncu_type_t         ncu_type;
    nwamui_connection_state_t connection_state        = NWAMUI_STATE_UNKNOWN;
    nwamui_env_status_t       env_status              = NWAMUI_ENV_STATUS_ERROR;

/*     g_debug("%s",  __func__); */

    if (force_size == 0) {
        if (status_icon != NULL && 
          gtk_status_icon_is_embedded(status_icon)) {
            force_size = gtk_status_icon_get_size( status_icon );
        }
        else {
            return( NULL );
        }
    }
    {
        NwamuiDaemon *daemon = nwamui_daemon_get_instance();
        NwamuiNcp *ncp = nwamui_daemon_get_active_ncp(daemon);
        GList *ncu_list;
        NwamuiNcu *ncu;

        g_object_unref(daemon);

        ncu_list = nwamui_ncp_get_ncu_list(ncp);

        if ( ncp ) {
            g_object_unref(ncp);
        }

        while (ncu_list) {
            ncu = NWAMUI_NCU(ncu_list->data);
            ncu_list = g_list_delete_link(ncu_list, ncu_list);

            if (nwamui_object_get_active(NWAMUI_OBJECT(ncu))) {
                switch(nwamui_ncu_get_ncu_type(ncu)) {
#ifdef TUNNEL_SUPPORT
                case NWAMUI_NCU_TYPE_TUNNEL:
#endif /* TUNNEL_SUPPORT */
                case NWAMUI_NCU_TYPE_WIRED:
                    activate_wired_num++;
                    break;
                case NWAMUI_NCU_TYPE_WIRELESS:
                    activate_wireless_num++;
                    average_signal_strength += nwamui_ncu_get_wifi_signal_strength(ncu);
                    break;
                default:
                    g_assert_not_reached();
                    break;
                }

                /* Double check the state using nwam state */
                connection_state = nwamui_ncu_get_connection_state( ncu);
                if ( connection_state == NWAMUI_STATE_CONNECTED 
                  || connection_state == NWAMUI_STATE_CONNECTED_ESSID ) {
                    env_status = NWAMUI_ENV_STATUS_CONNECTED;
                }
                else if ( connection_state == NWAMUI_STATE_CONNECTING 
                  || connection_state == NWAMUI_STATE_WAITING_FOR_ADDRESS
                  || connection_state == NWAMUI_STATE_DHCP_TIMED_OUT
                  || connection_state == NWAM_AUX_STATE_IF_DUPLICATE_ADDR
                  || connection_state == NWAMUI_STATE_CONNECTING_ESSID ) {
                    env_status = NWAMUI_ENV_STATUS_WARNING;
                }
                else {
                    env_status = NWAMUI_ENV_STATUS_ERROR;
                }
            } else {
                /* Default */
                /* env_status = NWAMUI_ENV_STATUS_ERROR; */
            }
        }
    }

    /* 
     * Make sure it's daemon status AND all ncu status.
     */
    if (daemon_status == NWAMUI_ENV_STATUS_CONNECTED) {
        daemon_status = env_status;
    }

    if (activate_wireless_num > 0) {
        ncu_type = NWAMUI_NCU_TYPE_WIRELESS;
        average_signal_strength /= activate_wireless_num;
    } else {
        ncu_type = NWAMUI_NCU_TYPE_WIRED;
        average_signal_strength = NWAMUI_WIFI_STRENGTH_NONE;
    }

    return nwamui_util_get_network_status_icon(ncu_type, 
      average_signal_strength,
      daemon_status,
      force_size);
}

/* 
 * Returns a GdkPixbuf that reflects the type of the network.
 */
extern GdkPixbuf*
nwamui_util_get_network_type_icon( nwamui_ncu_type_t ncu_type )
{
        static GdkPixbuf       *wireless_icon = NULL;
        static GdkPixbuf       *wired_icon = NULL;

        if ( wireless_icon == NULL ) {
            wireless_icon = get_pixbuf("network-wireless", FALSE);
        }
        if (wired_icon == NULL ) {
            wired_icon = get_pixbuf("network-idle", FALSE);
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
 * Returns a GdkPixbuf that reflects the security type of the network.
 */
extern GdkPixbuf*
nwamui_util_get_network_security_icon( nwamui_wifi_security_t sec_type, gboolean small )
{
    static GdkPixbuf       *secured_icon = NULL;
    static GdkPixbuf       *open_icon = NULL;

    /* TODO - get REAL icons for drop-downlist, these are borrowed */
    if ( secured_icon == NULL ) {
        secured_icon = get_pixbuf(NWAM_ICON_NETWORK_SECURE, small);
    }
    if ( open_icon == NULL ) {
        open_icon = get_pixbuf(NWAM_ICON_NETWORK_INSECURE, small);
        if (!open_icon)
            open_icon = gdk_pixbuf_new(GDK_COLORSPACE_RGB,
              TRUE,
              8,
              24,
              24);
    }

    switch (sec_type) {
#ifdef WEP_ASCII_EQ_HEX 
        case NWAMUI_WIFI_SEC_WEP:
#else
        case NWAMUI_WIFI_SEC_WEP_ASCII:
        case NWAMUI_WIFI_SEC_WEP_HEX:
#endif /* WEP_ASCII_EQ_HEX */
        /* case NWAMUI_WIFI_SEC_WPA_ENTERPRISE: - Currently not supported */
        case NWAMUI_WIFI_SEC_WPA_PERSONAL:
            return( GDK_PIXBUF(g_object_ref(G_OBJECT(secured_icon) )) );
        case NWAMUI_WIFI_SEC_NONE: 
            /* Fall-through */
        default:
            return( GDK_PIXBUF(g_object_ref(G_OBJECT(open_icon) )) );
    }
}
       
/* 
 * Returns a GdkPixbuf that reflects the status of the network.
 */
extern GdkPixbuf*
nwamui_util_get_network_status_icon(nwamui_ncu_type_t ncu_type,
  nwamui_wifi_signal_strength_t strength,
  nwamui_env_status_t env_status,
  gint size)
{
    static GdkPixbuf*   network_status_icons[NWAMUI_ENV_STATUS_LAST][NWAMUI_NCU_TYPE_LAST][NWAMUI_WIFI_STRENGTH_LAST][4] = {NULL};

    GdkPixbuf* env_status_icon = NULL;
    gchar *stock_id = NULL;
    gint icon_size;

    g_return_val_if_fail(ncu_type < NWAMUI_NCU_TYPE_LAST, NULL);
    g_return_val_if_fail(env_status < NWAMUI_ENV_STATUS_LAST, NULL);
    g_return_val_if_fail(strength < NWAMUI_WIFI_STRENGTH_LAST, NULL);

    if (size <= 16) {size = 16; icon_size = 0;}
    else if (size <= 24) {size = 24; icon_size = 1;}
    else if (size <= 32) {size = 32; icon_size = 2;}
    else {size = 48; icon_size = 3;}

/*     g_debug("%s: returning icon for status = %d; ncu_type = %d, signal = %d; size = %d", __func__,  */
/*             env_status, ncu_type, strength, size ); */

    if (network_status_icons[env_status][ncu_type][strength][icon_size] == NULL ) {
        GdkPixbuf* inf_icon = NULL;
        GdkPixbuf* temp_icon = NULL;

        switch(ncu_type) {
#ifdef TUNNEL_SUPPORT
        case NWAMUI_NCU_TYPE_TUNNEL:
#endif /* TUNNEL_SUPPORT */
        case NWAMUI_NCU_TYPE_WIRED:
            temp_icon = get_pixbuf_with_size(NWAM_ICON_NETWORK_WIRED, size);
            break;
        case NWAMUI_NCU_TYPE_WIRELESS:
            temp_icon = nwamui_util_get_wireless_strength_icon_with_size(strength, NWAMUI_WIRELESS_ICON_TYPE_RADAR, size);
            break;
        default:
            g_assert_not_reached();
        }

        inf_icon = gdk_pixbuf_copy(temp_icon);
        g_object_unref(temp_icon);

        switch( env_status ) {
        case NWAMUI_ENV_STATUS_CONNECTED:
            stock_id = NWAM_ICON_CONNECTED;
            break;
        case NWAMUI_ENV_STATUS_WARNING:
            stock_id = NWAM_ICON_WARNING;
            break;
        case NWAMUI_ENV_STATUS_ERROR:
            stock_id = NWAM_ICON_ERROR;
            break;
        default:
            g_assert_not_reached();
            break;
        }
        env_status_icon = get_pixbuf_with_size(stock_id, size);
        PIXBUF_COMPOSITE_NO_SCALE(env_status_icon, inf_icon);
        g_object_unref(env_status_icon);

        network_status_icons[env_status][ncu_type][strength][icon_size] = inf_icon;
    }

    return(GDK_PIXBUF(g_object_ref(network_status_icons[env_status][ncu_type][strength][icon_size])));
}

extern GdkPixbuf*
nwamui_util_get_ncu_status_icon( NwamuiNcu* ncu, gint size )
{
    nwamui_ncu_type_t               ncu_type;
    nwamui_wifi_signal_strength_t   strength = NWAMUI_WIFI_STRENGTH_NONE;
    nwamui_connection_state_t       connection_state = NWAMUI_STATE_UNKNOWN;
    gboolean                        active = FALSE;
    nwamui_env_status_t             env_state = NWAMUI_ENV_STATUS_ERROR;

/*     g_debug("%s",  __func__); */

    if ( ncu != NULL ) {
        ncu_type = nwamui_ncu_get_ncu_type(ncu);
        strength = nwamui_ncu_get_wifi_signal_strength(ncu);
        active = nwamui_ncu_get_active(ncu);
    }
    else {
        /* Fallback to a Wired connection */
        ncu_type = NWAMUI_NCU_TYPE_WIRED;
        active = FALSE;
    }
    if ( active ) {
        /* Double check the state using nwam state */
        connection_state = nwamui_ncu_get_connection_state( ncu);
        if ( connection_state == NWAMUI_STATE_CONNECTED 
           || connection_state == NWAMUI_STATE_CONNECTED_ESSID ) {
            env_state = NWAMUI_ENV_STATUS_CONNECTED;
        }
        else if ( connection_state == NWAMUI_STATE_CONNECTING 
                || connection_state == NWAMUI_STATE_WAITING_FOR_ADDRESS
                || connection_state == NWAMUI_STATE_DHCP_TIMED_OUT
                || connection_state == NWAM_AUX_STATE_IF_DUPLICATE_ADDR
                || connection_state == NWAMUI_STATE_CONNECTING_ESSID ) {
            env_state = NWAMUI_ENV_STATUS_WARNING;
        }
        else {
            env_state = NWAMUI_ENV_STATUS_ERROR;
        }
    }

    return nwamui_util_get_network_status_icon(ncu_type, strength, env_state, size);
}
       
extern const gchar*
nwamui_util_get_ncu_group_icon( NwamuiNcu* ncu  )
{
    switch (nwamui_ncu_get_priority_group_mode(ncu)) {
    case NWAMUI_COND_PRIORITY_GROUP_MODE_EXCLUSIVE:
        return NWAM_ICON_COND_PRIORITY_GROUP_MODE_EXCLUSIVE;
    case NWAMUI_COND_PRIORITY_GROUP_MODE_SHARED:
        return NWAM_ICON_COND_PRIORITY_GROUP_MODE_SHARED;
    case NWAMUI_COND_PRIORITY_GROUP_MODE_ALL:
        return NWAM_ICON_COND_PRIORITY_GROUP_MODE_ALL;
    default:
        g_assert_not_reached();
        return NULL;
    }
}

extern const gchar*
nwamui_util_get_active_mode_icon( NwamuiObject *object  )
{
    switch (nwamui_object_get_activation_mode(object)) {
    case NWAMUI_COND_ACTIVATION_MODE_MANUAL:
        return NWAM_ICON_COND_ACT_MODE_MANUAL;
    case NWAMUI_COND_ACTIVATION_MODE_SYSTEM:
        return NWAM_ICON_COND_ACT_MODE_SYSTEM;
    case NWAMUI_COND_ACTIVATION_MODE_PRIORITIZED:
        return NWAM_ICON_COND_ACT_MODE_PRIORITIZED;
    case NWAMUI_COND_ACTIVATION_MODE_CONDITIONAL_ANY:
        return NWAM_ICON_COND_ACT_MODE_CONDITIONAL_ANY;
    case NWAMUI_COND_ACTIVATION_MODE_CONDITIONAL_ALL:
        return NWAM_ICON_COND_ACT_MODE_CONDITIONAL_ALL;
    default:
        g_assert_not_reached();
        break;
    }
}

extern GdkPixbuf*
nwamui_util_get_wireless_strength_icon_with_size( nwamui_wifi_signal_strength_t signal_strength, 
                                                  nwamui_wireless_icon_type_t icon_type,
                                                  gint size)
{
    static GdkPixbuf*   enabled_wireless_icons[NWAMUI_WIFI_STRENGTH_LAST][NWAMUI_WIRELESS_ICON_TYPE_LAST][4];
    gint icon_size;

    if (size <= 16) {size = 16; icon_size = 0;}
    else if (size <= 24) {size = 24; icon_size = 1;}
    else if (size <= 32) {size = 32; icon_size = 2;}
    else {size = 48; icon_size = 3;}

    if ( enabled_wireless_icons[NWAMUI_WIFI_STRENGTH_NONE][NWAMUI_WIRELESS_ICON_TYPE_RADAR][icon_size] == NULL ) {
        GdkPixbuf* inf_icon = NULL;
        GdkPixbuf* wireless_strength_icon = NULL;
        GdkPixbuf* composed_icon = NULL;

        /* Basic interface icon. */
/*         inf_icon = gdk_pixbuf_new(GDK_COLORSPACE_RGB, */
/*           TRUE, */
/*           8, */
/*           24, */
/*           24); */

        /* Create Composed RADAR icons */

        inf_icon = get_pixbuf_with_size(NWAM_ICON_NETWORK_WIRELESS, size);
/*         size = (size - 16)/2 + 16; */

        /* Compose icon. */
        composed_icon = gdk_pixbuf_copy(inf_icon);
        wireless_strength_icon = get_pixbuf_with_size(NWAM_RADAR_ICON_WIRELESS_STRENGTH_NONE, size);
        PIXBUF_COMPOSITE_NO_SCALE(wireless_strength_icon, composed_icon);
        g_object_unref(wireless_strength_icon);

        /* Mapping : NONE = NONE */
        enabled_wireless_icons[NWAMUI_WIFI_STRENGTH_NONE][NWAMUI_WIRELESS_ICON_TYPE_RADAR][icon_size] = composed_icon;
          
        composed_icon = gdk_pixbuf_copy(inf_icon);
        wireless_strength_icon = get_pixbuf_with_size(NWAM_RADAR_ICON_WIRELESS_STRENGTH_POOR, size);
        PIXBUF_COMPOSITE_NO_SCALE(wireless_strength_icon, composed_icon);
        g_object_unref(wireless_strength_icon);

        /* Mapping : VERY_WEAK = POOR */
        enabled_wireless_icons[NWAMUI_WIFI_STRENGTH_VERY_WEAK][NWAMUI_WIRELESS_ICON_TYPE_RADAR][icon_size] = composed_icon;

        composed_icon = gdk_pixbuf_copy(inf_icon);
        wireless_strength_icon = get_pixbuf_with_size(NWAM_RADAR_ICON_WIRELESS_STRENGTH_FAIR, size);
        PIXBUF_COMPOSITE_NO_SCALE(wireless_strength_icon, composed_icon);
        g_object_unref(wireless_strength_icon);

        /* Mapping : WEAK = FAIR */
        enabled_wireless_icons[NWAMUI_WIFI_STRENGTH_WEAK][NWAMUI_WIRELESS_ICON_TYPE_RADAR][icon_size] = composed_icon;

        composed_icon = gdk_pixbuf_copy(inf_icon);
        wireless_strength_icon = get_pixbuf_with_size(NWAM_RADAR_ICON_WIRELESS_STRENGTH_GOOD, size);
        PIXBUF_COMPOSITE_NO_SCALE(wireless_strength_icon, composed_icon);
        g_object_unref(wireless_strength_icon);

        /* Mapping : GOOD = GOOD */
        /* Mapping : VERY_GOOD = GOOD */
        enabled_wireless_icons[NWAMUI_WIFI_STRENGTH_GOOD][NWAMUI_WIRELESS_ICON_TYPE_RADAR][icon_size] = composed_icon;
        enabled_wireless_icons[NWAMUI_WIFI_STRENGTH_VERY_GOOD][NWAMUI_WIRELESS_ICON_TYPE_RADAR][icon_size]= g_object_ref(composed_icon);

        composed_icon = gdk_pixbuf_copy(inf_icon);
        wireless_strength_icon = get_pixbuf_with_size(NWAM_RADAR_ICON_WIRELESS_STRENGTH_EXCELLENT, size);
        PIXBUF_COMPOSITE_NO_SCALE(wireless_strength_icon, composed_icon);
        g_object_unref(wireless_strength_icon);

        /* Mapping : EXCELLENT = EXCELLENT */
        enabled_wireless_icons[NWAMUI_WIFI_STRENGTH_EXCELLENT][NWAMUI_WIRELESS_ICON_TYPE_RADAR][icon_size]= composed_icon;

        g_object_unref(inf_icon);

        /* Create simpler Bar Icons */
        wireless_strength_icon = get_pixbuf_with_size(NWAM_BAR_ICON_WIRELESS_STRENGTH_NONE, size);
        enabled_wireless_icons[NWAMUI_WIFI_STRENGTH_NONE][NWAMUI_WIRELESS_ICON_TYPE_BARS][icon_size] = wireless_strength_icon;
          
        wireless_strength_icon = get_pixbuf_with_size(NWAM_BAR_ICON_WIRELESS_STRENGTH_POOR, size);
        enabled_wireless_icons[NWAMUI_WIFI_STRENGTH_VERY_WEAK][NWAMUI_WIRELESS_ICON_TYPE_BARS][icon_size] = wireless_strength_icon;
        enabled_wireless_icons[NWAMUI_WIFI_STRENGTH_WEAK][NWAMUI_WIRELESS_ICON_TYPE_BARS][icon_size] = g_object_ref(wireless_strength_icon);

        wireless_strength_icon = get_pixbuf_with_size(NWAM_BAR_ICON_WIRELESS_STRENGTH_FAIR, size);
        enabled_wireless_icons[NWAMUI_WIFI_STRENGTH_GOOD][NWAMUI_WIRELESS_ICON_TYPE_BARS][icon_size] = wireless_strength_icon;

        wireless_strength_icon = get_pixbuf_with_size(NWAM_BAR_ICON_WIRELESS_STRENGTH_GOOD, size);
        enabled_wireless_icons[NWAMUI_WIFI_STRENGTH_VERY_GOOD][NWAMUI_WIRELESS_ICON_TYPE_BARS][icon_size]= wireless_strength_icon;

        wireless_strength_icon = get_pixbuf_with_size(NWAM_BAR_ICON_WIRELESS_STRENGTH_EXCELLENT, size);
        enabled_wireless_icons[NWAMUI_WIFI_STRENGTH_EXCELLENT][NWAMUI_WIRELESS_ICON_TYPE_BARS][icon_size]= wireless_strength_icon;
    }

    return( GDK_PIXBUF(g_object_ref( G_OBJECT(enabled_wireless_icons[signal_strength][icon_type][icon_size]) )) );
}

/* 
 * Shows Help Dialog - the link_name refers to either the anchor or sectionid in the help file.
 */
extern void
nwamui_util_show_help( const gchar* link_name )
{
  GError *error = NULL;
  
  gnome_help_display(PACKAGE, (link_name?link_name:""), &error);
  
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

static void
disable_widget_if_empty (GtkEditable *editable, gpointer  user_data)  
{
    const gchar* text = gtk_entry_get_text(GTK_ENTRY(editable));

    if ( text == NULL || strlen(text) == 0 ) {
        gtk_widget_set_sensitive( GTK_WIDGET(user_data), FALSE);
    }
    else {
        gtk_widget_set_sensitive( GTK_WIDGET(user_data), TRUE);
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
    static GtkWidget*   ok_btn  = NULL;
    
    gint                response;
    gchar*              outstr;

    g_assert( title != NULL && current_name != NULL );

    g_return_val_if_fail( (title != NULL && current_name != NULL), NULL );

    if ( dialog == NULL ) {
        dialog = nwamui_util_glade_get_widget(NWAM_ENVIRONMENT_RENAME);
        entry = nwamui_util_glade_get_widget(RENAME_ENVIRONMENT_ENTRY);
        ok_btn = nwamui_util_glade_get_widget(RENAME_ENVIRONMENT_OK_BTN);
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
    g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(disable_widget_if_empty), (gpointer)ok_btn );
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

extern gboolean
nwamui_util_confirm_removal(GtkWindow* parent_window, const gchar* title, const gchar* message) 
{
    GtkWidget          *message_dialog;
    GtkWidget          *action_area;
    gint                response;
    gchar*              outstr;

    g_assert( message != NULL );

    g_return_val_if_fail( message != NULL, FALSE );

    message_dialog = gtk_message_dialog_new(parent_window, GTK_DIALOG_MODAL |GTK_DIALOG_DESTROY_WITH_PARENT,
                                            GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL, message );
    
    if ( title != NULL ) {
        gtk_window_set_title(GTK_WINDOW(message_dialog), title);
    }
    
    gtk_message_dialog_format_secondary_text( GTK_MESSAGE_DIALOG(message_dialog), _("This operation cannot be undone.") );
    
    /* Change OK to be Remove button */
    if ( (action_area = gtk_dialog_get_action_area( GTK_DIALOG(message_dialog) )) != NULL ) {
        GList*  buttons = gtk_container_get_children( GTK_CONTAINER( action_area ) );

        for ( GList *elem = buttons; buttons != NULL; elem = g_list_next(elem) ) {
            if ( GTK_IS_BUTTON( elem->data ) ) {
                const gchar* label = gtk_button_get_label( GTK_BUTTON( elem->data ) );
                if ( label != NULL && strcmp( label, GTK_STOCK_OK )  == 0 ) {
                    gtk_button_set_label( GTK_BUTTON( elem->data ), GTK_STOCK_REMOVE );
                    break;
                }
            }
        }

        g_list_free( buttons );
    }
    switch( gtk_dialog_run(GTK_DIALOG(message_dialog)) ) {
        case GTK_RESPONSE_OK:
            gtk_widget_destroy(message_dialog);
            return( TRUE );
        default:
            gtk_widget_destroy(message_dialog);
            return( FALSE );
    }
}

extern void
nwamui_util_show_message(GtkWindow* parent_window, GtkMessageType type, const gchar* title, const gchar* message, gboolean block )
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
    
    if ( block ) {
       (void)gtk_dialog_run(GTK_DIALOG(message_dialog));
    }
    else {
        gtk_widget_show( GTK_WIDGET(g_object_ref(message_dialog)) );
    }

}

extern void
nwamui_util_set_a11y_label_for_widget(  GtkLabel       *label,
                                        GtkWidget      *widget )
{
    AtkObject       *atk_widget, *atk_label;
    AtkRelationSet  *relation_set;
    AtkRelation     *relation;
    AtkObject       *targets[1];

    atk_widget = gtk_widget_get_accessible(widget);
    atk_label = gtk_widget_get_accessible (GTK_WIDGET(label));

    relation_set = atk_object_ref_relation_set (atk_label);

    targets[0] = atk_widget;
    relation = atk_relation_new(targets,1, ATK_RELATION_LABEL_FOR);

    atk_relation_set_add(relation_set,relation);

    g_object_unref(G_OBJECT(relation));
}

extern void
nwamui_util_set_widget_a11y_info(   GtkWidget      *widget, 
                                    const gchar    *name,
                                    const gchar    *description )
{
    AtkObject *obj;

    g_return_if_fail( widget != NULL && GTK_IS_WIDGET(widget) );

    obj = gtk_widget_get_accessible(widget);

    if ( obj ) {
        if ( name != NULL ) {
            atk_object_set_name(obj, name );
        }
        if ( description != NULL ) {
            atk_object_set_description(obj, description);
        }
    }
}

extern GList*
nwamui_util_map_condition_strings_to_object_list( char** conditions )
{
    GList*  new_list = NULL;

    if ( conditions == NULL ) {
        return( NULL );
    }

    for ( int i = 0; conditions[i] != NULL; i++ ) {
        NwamuiCond* cond = nwamui_cond_new_from_str( conditions[i] );
        if ( cond != NULL ) {
            new_list = g_list_append( new_list, cond);
        }
    }

    return( new_list );
}

extern char**
nwamui_util_map_object_list_to_condition_strings( GList* conditions, guint *len )
{
    gchar** cond_strs = NULL;
    guint   count = 0;
    GList*  elem = NULL;

    if ( conditions == NULL || len == NULL ) {
        return( NULL );
    }

    count = g_list_length( conditions ); 

    if ( count == 0 ) {
        return( NULL );
    }

    cond_strs = malloc( sizeof( gchar* ) * count );

    *len = 0;
    elem = g_list_first(conditions);
    for ( int i = 0; elem != NULL && i < count; i++ ) {
        if ( elem->data != NULL && NWAMUI_IS_COND( elem->data ) ) {
            gchar* cond_str = nwamui_cond_to_string( NWAMUI_COND(elem->data) );
            if ( cond_str != NULL ) {
                cond_strs[*len] = strdup( cond_str );
                *len = *len + 1;
            }
        }
        elem = g_list_next( conditions );
    }

    return( cond_strs );
}

extern GList*
nwamui_util_strv_to_glist( gchar **strv ) 
{
    GList   *new_list = NULL;

    for ( char** strp = strv; strp != NULL && *strp != NULL; strp++ ) {
        new_list = g_list_append( new_list, g_strdup( *strp ) );
    }

    return ( new_list );
}

extern gchar**
nwamui_util_glist_to_strv( GList *list ) 
{
    gchar** new_strv = NULL;

    if ( list != NULL ) {
        int     list_len = g_list_length( list );
        int     i = 0;

        new_strv = (gchar**)g_malloc0( sizeof(gchar*) * (list_len+1) );

        i = 0;
        for ( GList *element  = g_list_first( list );
              element != NULL && element->data != NULL;
              element = g_list_next( element ) ) {
            new_strv[i]  = g_strdup ( element->data );
            i++;
        }
        new_strv[list_len]=NULL;
    }

    return ( new_strv );
}

/* If there is any underscores we need to replace them with two since
 * otherwise it's interpreted as a mnemonic
 *
 * Will modify the label, possibly reallocating memory.
 *
 * Returns the modified pointer.
 */
extern gchar*
nwamui_util_encode_menu_label( gchar **modified_label )
{
    if ( modified_label == NULL ) {
        return NULL;
    }

    if ( *modified_label != NULL && strchr( *modified_label, '_' ) != NULL ) {
        /* Allocate a GString, with space for 2 extra underscores to
         * miminize need to reallocate, several times, but using GString
         * provides to possibility that it may grow.
         */
        GString *gstr = g_string_sized_new( strlen(*modified_label + 2 ) );
        for ( gchar *c = *modified_label; c != NULL && *c != '\0'; c++ ) {
            if ( *c == '_' ) {
                /* add extra underscore */
                g_string_append_c( gstr, '_' );
            }
            g_string_append_c( gstr, *c );
        }
        g_free(*modified_label);
        *modified_label = g_string_free(gstr, FALSE);
    }

    return( *modified_label );
}

#define MAX_NOTIFICATION_FAILURES   (20)

extern gboolean
notification_area_ready ( GtkStatusIcon* status_icon )
{
    static gint count_attempts = 0;

    GdkAtom selection_atom;
    GdkDisplay *display;
    Display* xdisplay;
    Window manager_window;
    gboolean retval = FALSE;
    int i, screen_count;
    char buffer[256];
    display = gdk_display_get_default();
    xdisplay = GDK_DISPLAY_XDISPLAY( display );

    /* Only allow this to fail a max number of times otherwise we will never
     * show messages */
    if ( (count_attempts++) > MAX_NOTIFICATION_FAILURES ) {
        g_debug("notification_area_ready() returning TRUE (max failues reached)");
        return TRUE;
    }

    screen_count = gdk_display_get_n_screens (display);

    gdk_x11_grab_server();

    for (i=0; i<screen_count; i++) {
        g_snprintf (buffer, sizeof (buffer),
                    "_NET_SYSTEM_TRAY_S%d", i);
                    
        selection_atom = gdk_atom_intern(buffer, FALSE);
  
        manager_window = XGetSelectionOwner( xdisplay, gdk_x11_atom_to_xatom(selection_atom) );

        if ( manager_window != NULL ) {
            break;
        }
  
    }

    gdk_x11_ungrab_server();

    gdk_flush();

    if (manager_window != NULL ) {
        /* We need to process all events to get status of embedding
         * correctly, otherwise is always FALSE */
        while (gtk_events_pending ())
          gtk_main_iteration ();

        if ( status_icon != NULL && gtk_status_icon_is_embedded( status_icon ) ) {
            retval = TRUE;
            /* Make it always return TRUE from now on */
            count_attempts = MAX_NOTIFICATION_FAILURES + 1;
        }
    }

    g_debug("notification_area_ready() returning %s (attempts = %d)", retval?"TRUE":"FALSE", count_attempts);
    
    return retval;
}

/* VOID:INT,POINTER */
void
marshal_VOID__INT_POINTER (GClosure     *closure,
  GValue       *return_value G_GNUC_UNUSED,
  guint         n_param_values,
  const GValue *param_values,
  gpointer      invocation_hint G_GNUC_UNUSED,
  gpointer      marshal_data)
{
    typedef void (*GMarshalFunc_VOID__INT_POINTER) (gpointer     data1,
      gint         arg_1,
      gpointer     arg_2,
      gpointer     data2);
    register GMarshalFunc_VOID__INT_POINTER callback;
    register GCClosure *cc = (GCClosure*) closure;
    register gpointer data1, data2;
 
    g_return_if_fail (n_param_values == 3);
 
    if (G_CCLOSURE_SWAP_DATA (closure)) {
        data1 = closure->data;
        data2 = g_value_peek_pointer (param_values + 0);
    } else {
        data1 = g_value_peek_pointer (param_values + 0);
        data2 = closure->data;
    }
    callback = (GMarshalFunc_VOID__INT_POINTER) (marshal_data ? marshal_data : cc->callback);
 
    callback (data1,
      g_value_get_int (param_values + 1),
      g_value_get_pointer (param_values + 2),
      data2);
}

/* VOID:OBJECT, OBJECT */
void
marshal_VOID__OBJECT_OBJECT (GClosure     *closure,
  GValue       *return_value G_GNUC_UNUSED,
  guint         n_param_values,
  const GValue *param_values,
  gpointer      invocation_hint G_GNUC_UNUSED,
  gpointer      marshal_data)
{
    typedef void (*GMarshalFunc_VOID__OBJECT_OBJECT) (gpointer     data1,
      gpointer     arg_1,
      gpointer     arg_2,
      gpointer     data2);
    register GMarshalFunc_VOID__OBJECT_OBJECT callback;
    register GCClosure *cc = (GCClosure*) closure;
    register gpointer data1, data2;
 
    g_return_if_fail (n_param_values == 3);
 
    if (G_CCLOSURE_SWAP_DATA (closure)) {
        data1 = closure->data;
        data2 = g_value_peek_pointer (param_values + 0);
    } else {
        data1 = g_value_peek_pointer (param_values + 0);
        data2 = closure->data;
    }
    callback = (GMarshalFunc_VOID__OBJECT_OBJECT) (marshal_data ? marshal_data : cc->callback);
 
    callback (data1,
      g_value_get_object (param_values + 1),
      g_value_get_object (param_values + 2),
      data2);
}

/* VOID:INT, OBJECT, POINTER */
void
marshal_VOID__INT_OBJECT_POINTER (GClosure     *closure,
  GValue       *return_value G_GNUC_UNUSED,
  guint         n_param_values,
  const GValue *param_values,
  gpointer      invocation_hint G_GNUC_UNUSED,
  gpointer      marshal_data)
{
    typedef void (*GMarshalFunc_VOID__INT_OBJECT_POINTER) (gpointer     data1,
      gint         arg_1,
      gpointer     arg_2,
      gpointer     arg_3,
      gpointer     data2);
    register GMarshalFunc_VOID__INT_OBJECT_POINTER callback;
    register GCClosure *cc = (GCClosure*) closure;
    register gpointer data1, data2;
 
    g_return_if_fail (n_param_values == 4);
 
    if (G_CCLOSURE_SWAP_DATA (closure)) {
        data1 = closure->data;
        data2 = g_value_peek_pointer (param_values + 0);
    } else {
        data1 = g_value_peek_pointer (param_values + 0);
        data2 = closure->data;
    }
    callback = (GMarshalFunc_VOID__INT_OBJECT_POINTER) (marshal_data ? marshal_data : cc->callback);
 
    callback (data1,
      g_value_get_int (param_values + 1),
      g_value_get_object (param_values + 2),
      g_value_get_pointer (param_values + 3),
      data2);
}

/* OBJECT:VOID */
void
marshal_OBJECT__VOID(GClosure     *closure,
  GValue       *return_value,
  guint         n_param_values,
  const GValue *param_values,
  gpointer      invocation_hint G_GNUC_UNUSED,
  gpointer      marshal_data)
{
    typedef GObject* (*GMarshalFunc_OBJECT__VOID) (gpointer     data1,
      gpointer     data2);
    register GMarshalFunc_OBJECT__VOID callback;
    register GCClosure *cc = (GCClosure*) closure;
    register gpointer data1, data2;
    register GObject* obj;
 
    g_return_if_fail (n_param_values == 1);
 
    if (G_CCLOSURE_SWAP_DATA (closure)) {
        data1 = closure->data;
        data2 = g_value_peek_pointer (param_values + 0);
    } else {
        data1 = g_value_peek_pointer (param_values + 0);
        data2 = closure->data;
    }
    callback = (GMarshalFunc_OBJECT__VOID) (marshal_data ? marshal_data : cc->callback);
 
    obj = callback (data1,
      data2);

    g_value_set_object(return_value, obj);
}

/* VOID:OBJECT, POINTER */
void
marshal_VOID__OBJECT_POINTER (GClosure     *closure,
  GValue       *return_value G_GNUC_UNUSED,
  guint         n_param_values,
  const GValue *param_values,
  gpointer      invocation_hint G_GNUC_UNUSED,
  gpointer      marshal_data)
{
    typedef void (*GMarshalFunc_VOID__OBJECT_POINTER) (gpointer     data1,
      gpointer     arg_1,
      gpointer     arg_2,
      gpointer     data2);
    register GMarshalFunc_VOID__OBJECT_POINTER callback;
    register GCClosure *cc = (GCClosure*) closure;
    register gpointer data1, data2;
 
    g_return_if_fail (n_param_values == 3);
 
    if (G_CCLOSURE_SWAP_DATA (closure)) {
        data1 = closure->data;
        data2 = g_value_peek_pointer (param_values + 0);
    } else {
        data1 = g_value_peek_pointer (param_values + 0);
        data2 = closure->data;
    }
    callback = (GMarshalFunc_VOID__OBJECT_POINTER) (marshal_data ? marshal_data : cc->callback);
 
    callback (data1,
      g_value_get_object (param_values + 1),
      g_value_get_pointer (param_values + 2),
      data2);
}

/*
 * Convert a prefix length to a netmask string.
 */
extern gchar*
nwamui_util_convert_prefixlen_to_netmask_str( sa_family_t family, guint prefixlen ) 
{
    gchar*              netmask_str = NULL;
    guint               maxlen = 0;
	struct lifreq       lifr;
    struct sockaddr_in *sin = (struct sockaddr_in *)&lifr.lifr_addr;
    struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&lifr.lifr_addr;
    uchar_t            *mask;

	memset(&lifr.lifr_addr, 0, sizeof (lifr.lifr_addr));

	lifr.lifr_addr.ss_family = family;

    switch( family ) {
        case AF_INET:
            maxlen = IP_ABITS;
            break;
        case AF_INET6:
            maxlen = IPV6_ABITS;
            netmask_str = g_strdup_printf("%d", prefixlen);
            return( netmask_str );
        default:
            break;
    }

    /* If v6, we will have returned already, but have some code here just
     * incase we need it again... */
	if ((prefixlen < 0) || (prefixlen > maxlen)) {
        return( netmask_str );
	}

    switch( family ) {
        case AF_INET:
            mask = (uchar_t *)&(sin->sin_addr);
            break;
        case AF_INET6:
            mask = (uchar_t *)&(sin6->sin6_addr);
            break;
        default:
            break;
    }

	while (prefixlen > 0) {
		if (prefixlen >= 8) {
			*mask++ = 0xFF;
			prefixlen -= 8;
			continue;
		}
		*mask |= 1 << (8 - prefixlen);
		prefixlen--;
	}

    switch( family ) {
        case AF_INET:
            netmask_str = g_strdup( (char*)inet_ntoa( sin->sin_addr ));
            break;
        case AF_INET6:
            netmask_str = g_strdup_printf("%d", prefixlen);
            break;
        default:
            break;
    }

    return( netmask_str );
}

/*
 * Convert a netmask string to prefix length
 */
extern guint
nwamui_util_convert_netmask_str_to_prefixlen( sa_family_t family, const gchar* netmask_str ) 
{
   uint32_t               prefixlen = 0;
   uint32_t               maxlen = 0;
   struct lifreq        lifr;
   struct sockaddr_in  *sin = (struct sockaddr_in *)&lifr.lifr_addr;
   struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&lifr.lifr_addr;
   uchar_t             *mask = NULL;

   memset(&lifr.lifr_addr, 0, sizeof (lifr.lifr_addr));

    switch( family ) {
        case AF_INET:
            maxlen = IP_ABITS;
            break;
        case AF_INET6:
            maxlen = IPV6_ABITS;
            prefixlen = (uint32_t)atoi( netmask_str );
            return( prefixlen );
        default:
            break;
    }

    /* If v6, we will have returned already, but have some code here just
     * incase we need it again... */
	if ((prefixlen < 0) || (prefixlen > maxlen)) {
        return( prefixlen );
	}

    switch( family ) {
        case AF_INET:
            if ( inet_pton ( family, netmask_str, (void*)(&sin->sin_addr) ) ) {
                mask = (uchar_t *)&(sin->sin_addr);
            }
            break;
        case AF_INET6:
            if ( inet_pton ( family, netmask_str, (void*)(&sin6->sin6_addr) ) ) {
                mask = (uchar_t *)&(sin6->sin6_addr);
            }
            break;
        default:
            break;
    }

    if ( mask == NULL ) {
        return( 0 );
    }

    for ( int i = 0; i < maxlen / 8; i++ ) {
        if ( *mask == 0xFF ) {
            prefixlen += 8;
        }
        else if ( *mask == 0 ) {
            break;
        }
        else {
            for ( int j = 1; j <= 8; j++ ) {
                if ( *mask & (1 << (8 - j))) {
                    prefixlen++;
                }
                else {
                    /* If we hit a 0, then break out, we're done */
                    break;
                }
            }
        }
        mask++;
    }

    return(prefixlen);
}

extern gboolean
nwamui_util_get_interface_address(const char *ifname, sa_family_t family,
                                  gchar**address_p, gint *prefixlen_p, gboolean *is_dhcp_p )
{
	icfg_if_t           intf;
	icfg_handle_t       h;
	struct sockaddr    *sin_p;
	struct sockaddr_in  _sin;
	struct sockaddr_in6 _sin6;
	socklen_t           sin_len = sizeof (struct sockaddr_in);
	socklen_t           sin6_len = sizeof (struct sockaddr_in6);
	socklen_t           addrlen;
	int                 prefixlen = 0;
	uint64_t            flags = 0;

    if ( family == AF_INET6 ) {
        sin_p = (struct sockaddr *)&_sin6;
        addrlen = sin6_len;
    }
    else if ( family == AF_INET ) {
        sin_p = (struct sockaddr *)&_sin;
        addrlen = sin_len;
    }

	(void) strlcpy(intf.if_name, ifname, sizeof (intf.if_name));
	intf.if_protocol = family;
	if (icfg_open(&h, &intf) != ICFG_SUCCESS) {
		g_debug( "icfg_open failed on interface %s", ifname);
		return( FALSE );
	}
	if (icfg_get_addr(h, sin_p, &addrlen, &prefixlen,
	    B_TRUE) != ICFG_SUCCESS) {
		g_debug( "icfg_get_addr failed on interface %s for family %s", ifname, (family == AF_INET6)?"v6":"v4");
		icfg_close(h);
		return( FALSE );
	}

	if (icfg_get_flags(h, &flags) != ICFG_SUCCESS) {
		flags = 0;
	}

    if ( is_dhcp_p != NULL ) {
        if ( flags & IFF_DHCPRUNNING ) {
            *is_dhcp_p = TRUE;
        }
        else {
            *is_dhcp_p = FALSE;
        }
    }

    if ( address_p != NULL ) {
        char        addr_str[INET6_ADDRSTRLEN];
        const char *addr_p;

        if ( family == AF_INET6 ) {
            addr_p = inet_ntop(family, (const void*)&_sin6.sin6_addr, addr_str, INET6_ADDRSTRLEN);
        }
        else {
            addr_p = inet_ntop(family, (const void*)&_sin.sin_addr, addr_str, INET6_ADDRSTRLEN);
        }

        *address_p =  g_strdup(addr_p?addr_p:"");
    }

    if ( prefixlen_p != NULL ) {
        *prefixlen_p =  prefixlen;
    }

	icfg_close(h);

    return( TRUE );
}

static void
foreach_service( GFunc callback, gpointer user_data )
{
    scf_handle_t   *handle = scf_handle_create( SCF_VERSION );
	ssize_t         max_scf_name_length = scf_limit(SCF_LIMIT_MAX_NAME_LENGTH);
	ssize_t         max_scf_fmri_length = scf_limit(SCF_LIMIT_MAX_FMRI_LENGTH);
    scf_scope_t    *scope = scf_scope_create( handle );
    scf_service_t  *service = scf_service_create( handle );
    scf_instance_t *instance = scf_instance_create( handle );
    scf_iter_t     *svc_iter = scf_iter_create(handle);
    scf_iter_t     *inst_iter = scf_iter_create(handle);
    char           *name = malloc( max_scf_name_length + 1 );
    char           *sname = malloc( max_scf_name_length + 1 );
    char           *fmri = malloc( max_scf_fmri_length + 1 );


    if ( !handle  || !scope  || !service  || !instance || !svc_iter || !inst_iter ) {
        g_warning("Couldn't allocation SMF handles" );
        return;
    }

    if (scf_handle_bind(handle) == -1 ) {
        g_warning("Couldn't bind to smf service: %s\n", scf_strerror(scf_error()) );
        scf_handle_destroy( handle );
        return;
    }

    if ( scf_handle_get_scope( handle, SCF_SCOPE_LOCAL, scope ) == 0 ) {
        if ( scf_iter_scope_services( svc_iter, scope ) == 0 ) {
            for ( int r = scf_iter_next_service( svc_iter, service );
                  r == 1;
                  r = scf_iter_next_service( svc_iter, service ) ) {
                if( scf_service_get_name( service, sname, max_scf_name_length + 1) != -1 ) {
                    if ( scf_iter_service_instances( inst_iter, service ) == 0 ) {
                        for ( int rv = scf_iter_next_instance( inst_iter, instance );
                              rv == 1;
                              rv = scf_iter_next_instance( inst_iter, instance ) ) {
                            if ( scf_instance_get_name( instance, name, max_scf_name_length + 1) != -1 ) {
                                if ( callback != NULL ) {
                                    snprintf( fmri, max_scf_fmri_length, "svc:/%s:%s", sname, name );
                                    (*callback)( (gpointer)fmri, (gpointer)user_data );
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    scf_handle_destroy( handle );

    free(name);
    free(sname);
    free(fmri);
}

static void
add_smf_fmri( gpointer data, gpointer user_data )
{
    gchar          *name = (gchar*)data;
    GtkListStore   *lstore = GTK_LIST_STORE(user_data);
    GtkTreeIter     iter;

    gtk_list_store_append( lstore, &iter );
    gtk_list_store_set( lstore, &iter, 0, name, -1 );
}

static gboolean
partial_smf_completion_func(GtkEntryCompletion *completion,
                            const gchar *key,
                            GtkTreeIter *iter,
                            gpointer user_data)
{
    GtkTreeModel   *model = GTK_TREE_MODEL(user_data);
    gchar          *name = NULL;
    gboolean        rval = FALSE;

    gtk_tree_model_get( model, iter, 0, &name, -1 );

    if ( name && g_strstr_len( name, strlen(name), key ) != NULL ) {
        rval = TRUE;
    }

    g_free(name);

    return( rval );
}

/* 
 * Utility function to attach an FNRI completion support to a GtkEntry.
 */
extern gboolean
nwamui_util_set_entry_smf_fmri_completion( GtkEntry* entry )
{
    static GtkListStore    *fmri_model  = NULL;
    GtkEntryCompletion     *completion = NULL;

    if ( entry == NULL || !GTK_IS_ENTRY( entry ) ) {
        return( FALSE );
    }

    if ( fmri_model == NULL ) {
        fmri_model  = GTK_LIST_STORE(g_object_ref(gtk_list_store_new( 1, G_TYPE_STRING )));
    }

    foreach_service( add_smf_fmri, fmri_model );

    completion = gtk_entry_completion_new();

    gtk_entry_completion_set_model( completion, GTK_TREE_MODEL(g_object_ref( fmri_model )) );
    gtk_entry_completion_set_minimum_key_length( completion, 3 );
    gtk_entry_completion_set_text_column( completion, 0 );
    gtk_entry_completion_set_inline_completion( completion, FALSE );
    gtk_entry_completion_set_match_func( completion, partial_smf_completion_func, fmri_model, NULL );
    gtk_entry_set_completion( entry, completion );

    return( TRUE );
}

static void
insert_text_ip_only_handler (GtkEditable *editable,
                             const gchar *text,
                             gint         length,
                             gint        *position,
                             gpointer     data)
{
    gboolean    is_v6 = (gboolean)data;
    gboolean    is_valid = TRUE;
    gchar      *lower = g_ascii_strdown(text, length);

    for ( int i = 0; i < length && is_valid; i++ ) {
        if ( is_v6 ) {
            /* Valid chars for v6 are ASCII [0-9a-f:./] */
            is_valid = (g_ascii_isxdigit( lower[i] ) || text[i] == '/' || text[i] == ':' || text[i] == '.' );   
        }
        else {
            /* Valid chars for v4 are ASCII [0-9./] */
            is_valid = (g_ascii_isdigit( lower[i] ) || text[i] == '.' || text[i] == '/' );   
        }
    }
    
    if ( is_valid ) {
        g_signal_handlers_block_by_func (editable,
                       (gpointer) insert_text_ip_only_handler, data);

        gtk_editable_insert_text (editable, lower, length, position);
        g_signal_handlers_unblock_by_func (editable,
                                         (gpointer) insert_text_ip_only_handler, data);
    }
    g_signal_stop_emission_by_name (editable, "insert_text");
    g_free (lower);
}

/* Validate an prefix values
 * 
 * If show_error_dialog is TRUE, then a message dialog will be shown to the
 * user.
 *
 * Returns whether the prefix was valid or not.
 */
extern gboolean
nwamui_util_validate_prefix_value(  GtkWidget   *widget,
                                    const gchar *prefix_str,
                                    gboolean     is_v6,
                                    gboolean     show_error_dialog )
{
    struct lifreq           lifr;
    struct sockaddr_in     *sin = (struct sockaddr_in *)&lifr.lifr_addr;
    struct sockaddr_in6    *sin6 = (struct sockaddr_in6 *)&lifr.lifr_addr;
    GtkWindow              *top_level = NULL;
    gboolean                is_valid = TRUE;

    if ( widget != NULL ) {
        top_level = GTK_WINDOW(gtk_widget_get_toplevel(widget));
    }

    if ( (widget != NULL && !GTK_WIDGET_IS_SENSITIVE(widget)) ||
         (top_level != NULL && !gtk_window_has_toplevel_focus (top_level)) ) {
        /* Assume valid */
        return( TRUE );;
    }

    if ( prefix_str == NULL || strlen (prefix_str) == 0 ) {
        is_valid = FALSE;
    }
    else {
        gchar **strs =  NULL;

        /* Allow for address or numbers in format "addr/prefix" */
        strs = g_strsplit(prefix_str, "/", 2 );

        if ( strs == NULL || strs[0] == NULL ) {
            is_valid = FALSE;
        }
        else {
            if ( is_v6 ) {
                gint64 prefix = g_ascii_strtoll( strs[0], NULL, 10 );
                if ( prefix == 0 || prefix > 128 ) {
                    is_valid = FALSE;
                }
            }
            else {
                if ( ! inet_pton ( AF_INET, strs[0], (void*)(&sin->sin_addr) ) ) {
                    is_valid = FALSE;
                }
            }
        }
        if ( strs != NULL) {
            g_strfreev( strs );
        }
    }

    if ( ! is_valid && show_error_dialog ) {
        const gchar*    message;
        if ( is_v6 ) {
            message = _("Valid prefix values are between 1 and 128");
        }
        else {
            message = _("Subnets must be in the format:\n\n    w.x.y.z\n");
        }

        nwamui_util_show_message(GTK_WINDOW(top_level), 
                                 GTK_MESSAGE_ERROR, _("Invalid Subnet or Prefix"), message, FALSE);
    }

    return( is_valid );
}

/* Validate an IP address. 
 * 
 * If show_error_dialog is TRUE, then a message dialog will be shown to the
 * user.
 *
 * Returns whether the address was valid or not.
 */
extern gboolean
nwamui_util_validate_ip_address(    GtkWidget   *widget,
                                    const gchar *address_str,
                                    gboolean     is_v6,
                                    gboolean     show_error_dialog )
{
    struct lifreq           lifr;
    struct sockaddr_in     *sin = (struct sockaddr_in *)&lifr.lifr_addr;
    struct sockaddr_in6    *sin6 = (struct sockaddr_in6 *)&lifr.lifr_addr;
    GtkWindow              *top_level = NULL;
    gboolean                is_valid = TRUE;

    if ( widget != NULL ) {
        top_level = GTK_WINDOW(gtk_widget_get_toplevel(widget));
    }

    if ( (widget != NULL && !GTK_WIDGET_IS_SENSITIVE(widget)) ||
         (top_level != NULL && !gtk_window_has_toplevel_focus (top_level)) ) {
        /* Assume valid */
        return( TRUE );;
    }

    if ( address_str == NULL || strlen (address_str) == 0 ) {
        is_valid = FALSE;
    }
    else {
        gchar **strs =  NULL;

        /* Allow for address in format "addr/prefix" */
        strs = g_strsplit(address_str, "/", 2 );

        if ( strs == NULL || strs[0] == NULL ) {
            is_valid = FALSE;
        }
        else {
            if ( is_v6 ) {
                if ( ! inet_pton ( AF_INET6, strs[0], (void*)(&sin6->sin6_addr) ) ) {
                    is_valid = FALSE;
                }
                if ( strs[1] != NULL ) { /* Handle /N */
                    gint64 prefix = g_ascii_strtoll( strs[1], NULL, 10 );
                    if ( prefix == 0 || prefix > 128 ) {
                        is_valid = FALSE;
                    }
                }
            }
            else {
                if ( ! inet_pton ( AF_INET, strs[0], (void*)(&sin->sin_addr) ) ) {
                    is_valid = FALSE;
                }
                if ( strs[1] != NULL ) { /* Handle /N */
                    gint64 prefix = g_ascii_strtoll( strs[1], NULL, 10 );
                    if ( prefix == 0 || prefix > 32 ) {
                        is_valid = FALSE;
                    }
                }
            }
        }
        if ( strs != NULL) {
            g_strfreev( strs );
        }
    }

    if ( ! is_valid && show_error_dialog ) {
        const gchar*    message;
        if ( is_v6 ) {
            message = _("IP addresses must be in one of the formats :\n\n   x:x:x:x:x:x:x:x\n   x:x::x, etc./N\n\nwhere N is between 1 and 128");
        }
        else {
            message = _("IP addresses must be in the one of the formats:\n\n    w.x.y.z\n    w.x.y.z/N\n\nwhere N is between 1 and 32");
        }

        nwamui_util_show_message(GTK_WINDOW(top_level), 
                                 GTK_MESSAGE_ERROR, _("Invalid IP address"), message, FALSE);
    }

    return( is_valid );
}

static gboolean
validate_ip_on_focus_exit(GtkWidget     *widget,
                          GdkEventFocus *event,
                          gpointer       data)
{
    gboolean                is_v6 = (gboolean)data;
    gboolean                is_valid = TRUE;
    GtkWindow              *top_level = GTK_WINDOW(gtk_widget_get_toplevel(widget));
    const gchar            *address_str;

    if ( !GTK_WIDGET_IS_SENSITIVE(widget) || !gtk_window_has_toplevel_focus (top_level)) {
        /* If not sensitive, do nothing, since user can't edit it */
        return(FALSE);
    }

    g_signal_handlers_block_by_func (widget,
                   (gpointer) validate_ip_on_focus_exit, data);

    address_str = gtk_entry_get_text(GTK_ENTRY(widget));
    

    if ( ! nwamui_util_validate_ip_address( widget, address_str, is_v6, TRUE) ) {
        gtk_widget_grab_focus( widget );
    }

    g_signal_handlers_unblock_by_func (widget,
                   (gpointer) validate_ip_on_focus_exit, data);

    return(FALSE); /* Must return FALSE since GtkEntry expects it */
}

/* 
 * Utility function to attach an insert-text handler to a GtkEntry to limit
 * it's input to be characters acceptable to a valid IP address format.
 */
extern void
nwamui_util_set_entry_ip_address_only( GtkEntry* entry, gboolean is_v6, gboolean validate_on_focus_out )
{
    if ( entry != NULL ) {
        g_signal_connect(G_OBJECT(entry), "insert_text", 
                         (GCallback)insert_text_ip_only_handler, (gpointer)is_v6);
        if ( validate_on_focus_out ) {
            g_signal_connect(G_OBJECT(entry), "focus-out-event", 
                             (GCallback)validate_ip_on_focus_exit, (gpointer)is_v6);
        }
    }
}

extern void
nwamui_util_unset_entry_ip_address_only( GtkEntry* entry )
{
    if ( entry != NULL ) {
        g_signal_handlers_disconnect_by_func( G_OBJECT(entry), (gpointer)insert_text_ip_only_handler, NULL );
        g_signal_handlers_disconnect_by_func( G_OBJECT(entry), (gpointer)validate_ip_on_focus_exit, NULL );
    }
}


extern void
nwamui_util_window_title_append_hostname( GtkDialog* dialog )
{
    const gchar    *current_title = NULL;
    gchar           hostname[MAXHOSTNAMELEN+1];
    gchar          *new_title = NULL;

    if ( dialog == NULL || !GTK_WINDOW(dialog)) {
        return;
    }
    if ( gethostname(hostname, MAXHOSTNAMELEN) != 0 ) {
        g_debug("gethostname returned error: %s", strerror( errno ) );
        return;
    }

    hostname[MAXHOSTNAMELEN] = '\0'; /* Just in case */

    current_title = gtk_window_get_title( GTK_WINDOW(dialog) );

    new_title = g_strdup_printf(_("%s (%s )"), current_title?current_title:"", hostname);

    gtk_window_set_title( GTK_WINDOW(dialog), new_title );

    g_debug("Setting Window title: %s", new_title?new_title:"NULL" );

    g_free( new_title );
}

#define LIST_DELIM  _(", \t\n")
#define LIST_JOIN   _(",")

/**
 * Parses a list of strings, separated by space or comma into a GList of char*
 **/
extern GList*
nwamui_util_parse_string_to_glist( const gchar* str )
{
    GList      *list = NULL;
    gchar     **words = NULL;

    words = g_strsplit_set(str, LIST_DELIM, 0 );

    for( int i = 0; words && words[i] != NULL; i++ ) {
        list = g_list_append( list, (gpointer)words[i] );
    }
                
    /* Don't free words[] since memory is now in GList */

    return( list );
}

extern gchar*
nwamui_util_glist_to_comma_string( GList* list )
{
    GString*    g_str = g_string_new( "" );
    GList*      item = g_list_first( list );
    gboolean    first = TRUE;
    
    while ( item != NULL ) {
        gchar* str = (gchar*)item->data;
        if ( str != NULL ) {
            if ( !first ) {
                g_str = g_string_append( g_str, LIST_JOIN );
            }
            g_str = g_string_append( g_str, str );
            first = FALSE;
        }
        item = g_list_next( item );
    }
    return( g_string_free( g_str, FALSE ) );
}

/* Given an address of the format:
 *
 *  <addr>/<prefixlen>
 *
 *  Split it out to appropriate format for family, v6 or v4, so for v4 it will
 *  be a subnet x.x.x.x, while for v6 it's simply the number in string format.
 *
 */
extern gboolean
nwamui_util_split_address_prefix( gboolean v6, const gchar* address_prefix, gchar **address, gchar **prefix )
{
    gchar       *delim;
    gchar       *tmpstr;
    gchar       *prefix_str = NULL;

    if ( address_prefix == NULL ) {
        return( FALSE );
    }

    tmpstr = g_strdup( address_prefix ); /* Worker string */

    if ( (delim = strrchr( tmpstr, '/' )) != NULL ) {
        gint prefix_num = 0;
        /* Format is x.x.x.x/N for subnet prefix */
        *delim = '\0'; /* Split string */
        delim++;
        prefix_num = atoi( delim );
        prefix_str = nwamui_util_convert_prefixlen_to_netmask_str( (v6?AF_INET6:AF_INET), prefix_num );
    }

    if ( address != NULL ) {
        *address = g_strdup( tmpstr );
    }

    if ( prefix != NULL ) {
        if ( prefix_str == NULL ) {
            *prefix = NULL;
        }
        else {
            *prefix = g_strdup( prefix_str );
        }
    }

    g_free(tmpstr);

    return( TRUE );
}

/* Given an address and prefix, generate a string of the format:
 *
 *  <addr>/<prefixlen>
 *
 *  Prefix shoul be appropriate format for family, v6 or v4, so for v4 it
 *  should be a subnet x.x.x.x, while for v6 it's simply the number in string
 *  format.
 *
 * If prefix == NULL, then will omit the /prefix and only generate x.x.x.x
 */
extern gchar*
nwamui_util_join_address_prefix( gboolean v6, const gchar *address, const gchar *prefix )
{
    gchar* retstr = NULL;

    if ( address == NULL ) {
        return( NULL );
    }
    if ( prefix != NULL ) {
        retstr = g_strdup_printf("%s/%d", address, 
                        nwamui_util_convert_netmask_str_to_prefixlen(v6?AF_INET6:AF_INET, prefix) );
    }
    else {
        retstr = g_strdup(address);
    }

    return( retstr );
}


extern FILE*
get_stdio( void )
{
    return( stdout );
}
