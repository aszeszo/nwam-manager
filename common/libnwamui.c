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
#include <libgnome/libgnome.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <inet/ip.h>
#include <sys/ethernet.h>
#include <libscf.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>

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

/* Use GtkBuilder */

const   gchar* nwamui_ui_file_names[NWAMUI_UI_FILE_LAST + 1] = {
    "nwam-manager-properties.ui",
    "nwam-manager-wireless.ui",
    NULL
};

/* Load the GtkBuilder file and maintain a single reference */
static GtkBuilder *gtk_builder[NWAMUI_UI_FILE_LAST] = { NULL };

static GtkBuilder* 
get_gtk_builder( nwamui_ui_file_index_t index ) {
    static const gchar *build_datadir = NWAM_MANAGER_DATADIR;

    if ( gtk_builder[index] == NULL ) {
        GtkBuilder  *new_builder = gtk_builder_new();
        const gchar * const *sys_data_dirs;
        const gchar *ui_file;
        gint         i;
        GError      *err = NULL;

        sys_data_dirs = g_get_system_data_dirs ();
        if ( g_file_test(build_datadir, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_DIR) ) {
            gchar      *ui_file;

            /* First try with the package name in the path */
            ui_file = g_build_filename (build_datadir, PACKAGE, nwamui_ui_file_names[index], NULL );
            nwamui_debug("Attempting to load : %s", ui_file);
            if ( (gtk_builder_add_from_file(new_builder, ui_file, &err )) != 0 ) {
                nwamui_debug("Found gtk builder file at : %s", ui_file );
                gtk_builder[index] = new_builder;
            }
            else if ( err ) {
                nwamui_warning("Error loading glade file : %s", err->message);
                g_error_free(err);
                err = NULL;
            }
            g_free (ui_file);
            if ( gtk_builder[index]  == NULL ) {
                /* Now try without it */
                ui_file = g_build_filename (build_datadir, nwamui_ui_file_names[index], NULL );
                nwamui_debug("Attempting to load : %s", ui_file);
                if ( (gtk_builder_add_from_file(new_builder, ui_file, &err )) != 0 ) {
                    nwamui_debug("Found gtk builder file at : %s", ui_file );
                    gtk_builder[index]  = new_builder;
                }
                else if ( err ) {
                    nwamui_warning("Error loading glade file : %s", err->message);
                    g_error_free(err);
                    err = NULL;
                }
                g_free (ui_file);
            }
        }
        if (gtk_builder[index]  == NULL ) {
            for (i = 0; sys_data_dirs[i] != NULL; i++) {
                gchar *ui_file;
                /* First try with the package name in the path */
                ui_file = g_build_filename (sys_data_dirs[i], PACKAGE, nwamui_ui_file_names[index], NULL );
                nwamui_debug("Attempting to load : %s", ui_file);
                if ( (gtk_builder_add_from_file(new_builder, ui_file, &err )) != 0 ) {
                    nwamui_debug("Found gtk builder file at : %s", ui_file );
                    gtk_builder[index]  = new_builder;
                    g_free (ui_file);
                    break;
                }
                else if ( err ) {
                    nwamui_warning("Error loading glade file : %s", err->message);
                    g_error_free(err);
                    err = NULL;
                }
                g_free (ui_file);
                /* Now try without it */
                ui_file = g_build_filename (sys_data_dirs[i], nwamui_ui_file_names[index], NULL );
                nwamui_debug("Attempting to load : %s", ui_file);
                if ( (gtk_builder_add_from_file(new_builder, ui_file, &err )) != 0 ) {
                    nwamui_debug("Found gtk builder file at : %s", ui_file );
                    gtk_builder[index]  = new_builder;
                    g_free (ui_file);
                    break;
                }
                else if ( err ) {
                    nwamui_warning("Error loading glade file : %s", err->message);
                    g_error_free(err);
                    err = NULL;
                }
            }
        }

        if ( gtk_builder[index]  == NULL ) {
            nwamui_error("Error locating UI file %s", nwamui_ui_file_names[index] );
            g_object_unref( new_builder );
        }
    }
    return gtk_builder[index];
}

/**
 * nwamui_util_ui_get_widget_from:
 * @index: nwamui_ui_file_index_t index of file to load widget from.
 * @widget_name: name of the widget to load.
 * @returns: the widget loaded from the GktBuilder file.
 *
 **/
extern GtkWidget*
nwamui_util_ui_get_widget_from( nwamui_ui_file_index_t index,  const gchar* widget_name ) 
{
    GtkBuilder* builder;;
    GtkWidget*  widget;

    g_assert( widget_name != NULL );
    
    g_return_val_if_fail( widget_name != NULL, NULL );
    
    builder = get_gtk_builder( index );
    g_return_val_if_fail( builder != NULL, NULL );
    
    widget = GTK_WIDGET(gtk_builder_get_object(builder, widget_name ));
    
    if ( widget == NULL )
        g_error("Failed to get widget by name %s", widget_name );
    
    return widget;
}
        
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

    /* Uncomment if want to assert on critical errors, most useful when
     * debugging. 
     */
    /*
    if ((log_level & G_LOG_LEVEL_MASK) == G_LOG_LEVEL_CRITICAL ) {
        g_assert_not_reached();
    }
    */

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
        case NWAMUI_WIFI_WPA_CONFIG_AUTOMATIC: /*fall through*/
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
    
    g_list_foreach(obj_list,
      (GFunc)nwamui_util_foreach_nwam_object_dup_and_append_to_list,
      (gpointer)&new_list);    

    return new_list;
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
        g_debug("get_pixbuf_with_size failed: pixbuf = NULL stockid = %s", stock_id);
    }
    return( pixbuf );
}

static GdkPixbuf*   
get_pixbuf( const gchar* stock_id, gboolean small )
{

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

struct info_for_env_status_icon {
    gint   activate_wired_num;
    gint   activate_wireless_num;
    gint   average_signal_strength;
    gint64 ncp_prio;
};

extern gint
foreach_gather_info_for_env_status_icon_from_ncp(gconstpointer data, gconstpointer user_data)
{
	NwamuiNcu                       *ncu            = (NwamuiNcu *)data;
    struct info_for_env_status_icon *info           = (struct info_for_env_status_icon *)user_data;
    gint64                           ncu_prio;
    gint                             found_excl_ncu = 1;
	
    g_return_val_if_fail(NWAMUI_IS_NCU(ncu), 1);
    g_return_val_if_fail(info, 0);
    
    ncu_prio = nwamui_ncu_get_priority_group(ncu);

    /* Only care about current prio ncus, and first exclusive NCU
     * that's in UP state 
     */
    if ( ncu_prio == info->ncp_prio) {
        if (nwamui_object_get_active(NWAMUI_OBJECT(ncu))) {
            nwam_state_t            state;
            nwam_aux_state_t        aux_state;
            nwamui_cond_activation_mode_t 
              activation_mode;
            nwamui_cond_priority_group_mode_t 
              prio_group_mode;
                    
            state = nwamui_object_get_nwam_state( NWAMUI_OBJECT(ncu), &aux_state, NULL);
            activation_mode = nwamui_object_get_activation_mode(NWAMUI_OBJECT(ncu));
            prio_group_mode = nwamui_ncu_get_priority_group_mode( ncu );

            if ( activation_mode == NWAMUI_COND_ACTIVATION_MODE_PRIORITIZED ) {
                if ( prio_group_mode == NWAMUI_COND_PRIORITY_GROUP_MODE_EXCLUSIVE ) {
                    if ( state == NWAM_STATE_ONLINE && aux_state == NWAM_AUX_STATE_UP ) {
                        /* Stop here. */
                        found_excl_ncu = 0;
                    }
                }
            }

            switch(nwamui_ncu_get_ncu_type(ncu)) {
#ifdef TUNNEL_SUPPORT
            case NWAMUI_NCU_TYPE_TUNNEL:
#endif /* TUNNEL_SUPPORT */
            case NWAMUI_NCU_TYPE_WIRED:
                info->activate_wired_num++;
                break;
            case NWAMUI_NCU_TYPE_WIRELESS:
                info->activate_wireless_num++;
                info->average_signal_strength += nwamui_ncu_get_wifi_signal_strength(ncu);
                break;
            default:
                g_assert_not_reached();
                break;
            }
        }
    }

    return found_excl_ncu;
}

/* 
 * Returns a GdkPixbuf that reflects the status of the overall environment
 * If force_size equals 0, uses the size of status icon.
 */
extern GdkPixbuf*
nwamui_util_get_env_status_icon( GtkStatusIcon* status_icon, nwamui_daemon_status_t daemon_status, gint force_size )
{
    struct info_for_env_status_icon info;
    nwamui_ncu_type_t               ncu_type;
    nwamui_connection_state_t       connection_state = NWAMUI_STATE_UNKNOWN;

    if (force_size == 0) {
        if (status_icon != NULL && gtk_status_icon_is_embedded(status_icon)) {
            force_size = gtk_status_icon_get_size( status_icon );
        }
        else {
            return( NULL );
        }
    }
    {
        NwamuiDaemon *daemon         = nwamui_daemon_get_instance();
        NwamuiObject *ncp            = nwamui_daemon_get_active_ncp(daemon);

        g_object_unref(daemon);

        if ( ncp ) {
            /* Clean */
            bzero(&info, sizeof(info));

            info.ncp_prio = nwamui_ncp_get_prio_group(NWAMUI_NCP(ncp));
            nwamui_ncp_find_ncu(NWAMUI_NCP(ncp), foreach_gather_info_for_env_status_icon_from_ncp, &info);
            g_object_unref(ncp);
        }
    }

    if (info.activate_wireless_num > 0) {
        ncu_type = NWAMUI_NCU_TYPE_WIRELESS;
        info.average_signal_strength /= info.activate_wireless_num;
    } else {
        ncu_type = NWAMUI_NCU_TYPE_WIRED;
        info.average_signal_strength = NWAMUI_WIFI_STRENGTH_NONE;
    }

    return nwamui_util_get_network_status_icon(ncu_type, 
      info.average_signal_strength,
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

    if ( secured_icon == NULL ) {
        secured_icon = get_pixbuf(NWAM_ICON_NETWORK_SECURE, small);
    }
    if ( open_icon == NULL ) {
        open_icon = get_pixbuf(NWAM_ICON_NETWORK_INSECURE, small);
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
  nwamui_daemon_status_t daemon_status,
  gint size)
{
    static GdkPixbuf*   network_status_icons[NWAMUI_DAEMON_STATUS_LAST][NWAMUI_NCU_TYPE_LAST][NWAMUI_WIFI_STRENGTH_LAST][4] = {NULL};

    GdkPixbuf* env_status_icon = NULL;
    gchar *stock_id = NULL;
    gint icon_size;

    g_return_val_if_fail(ncu_type < NWAMUI_NCU_TYPE_LAST, NULL);
    g_return_val_if_fail(daemon_status < NWAMUI_DAEMON_STATUS_LAST, NULL);
    g_return_val_if_fail(strength < NWAMUI_WIFI_STRENGTH_LAST, NULL);

    if (size <= 16) {size = 16; icon_size = 0;}
    else if (size <= 24) {size = 24; icon_size = 1;}
    else if (size <= 32) {size = 32; icon_size = 2;}
    else {size = 48; icon_size = 3;}

/*     g_debug("%s: returning icon for status = %d; ncu_type = %d, signal = %d; size = %d", __func__,  */
/*             daemon_status, ncu_type, strength, size ); */

    if (network_status_icons[daemon_status][ncu_type][strength][icon_size] == NULL ) {
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

        switch( daemon_status ) {
        case NWAMUI_DAEMON_STATUS_ALL_OK:
            stock_id = NWAM_ICON_CONNECTED;
            break;
        case NWAMUI_DAEMON_STATUS_NEEDS_ATTENTION:
            stock_id = NWAM_ICON_WARNING;
            break;
        case NWAMUI_DAEMON_STATUS_ERROR:
            stock_id = NWAM_ICON_ERROR;
            break;
        default:
            g_assert_not_reached();
            break;
        }
        env_status_icon = get_pixbuf_with_size(stock_id, size);
        PIXBUF_COMPOSITE_NO_SCALE(env_status_icon, inf_icon);
        g_object_unref(env_status_icon);

        network_status_icons[daemon_status][ncu_type][strength][icon_size] = inf_icon;
    }

    return(GDK_PIXBUF(g_object_ref(network_status_icons[daemon_status][ncu_type][strength][icon_size])));
}

extern GdkPixbuf*
nwamui_util_get_ncu_status_icon( NwamuiNcu* ncu, gint size )
{
    nwamui_ncu_type_t             ncu_type;
    nwamui_wifi_signal_strength_t strength         = NWAMUI_WIFI_STRENGTH_NONE;
    nwamui_connection_state_t     connection_state = NWAMUI_STATE_UNKNOWN;
    gboolean                      active           = FALSE;
    nwamui_daemon_status_t        daemon_state     = NWAMUI_DAEMON_STATUS_ERROR;

/*     g_debug("%s",  __func__); */

    if ( ncu != NULL ) {
        ncu_type = nwamui_ncu_get_ncu_type(ncu);
        strength = nwamui_ncu_get_wifi_signal_strength(ncu);
        active = nwamui_object_get_active(NWAMUI_OBJECT(ncu));
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
            daemon_state = NWAMUI_DAEMON_STATUS_ALL_OK;
        }
        else if ( connection_state == NWAMUI_STATE_CONNECTING 
                || connection_state == NWAMUI_STATE_WAITING_FOR_ADDRESS
                || connection_state == NWAMUI_STATE_DHCP_TIMED_OUT
                || connection_state == NWAMUI_STATE_DHCP_DUPLICATE_ADDR
                || connection_state == NWAMUI_STATE_CONNECTING_ESSID ) {
            daemon_state = NWAMUI_DAEMON_STATUS_NEEDS_ATTENTION;
        }
        else {
            daemon_state = NWAMUI_DAEMON_STATUS_ERROR;
        }
    }

    return nwamui_util_get_network_status_icon(ncu_type, strength, daemon_state, size);
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

static GdkCursor    *busy_cursor = NULL;

extern void
nwamui_util_set_busy_cursor( GtkWidget *widget )
{
    GdkWindow   *window = NULL;

    if ( busy_cursor == NULL ) {
        GdkDisplay *display = gtk_widget_get_display( widget );
        if ( display != NULL ) {
            busy_cursor = gdk_cursor_new_for_display( display, GDK_WATCH );
        }
    }

    if ( widget != NULL ) {
        window = gtk_widget_get_window( widget );
    }

    if ( window != NULL ) {
        gdk_window_set_cursor( window, busy_cursor );
    }
    else if ( g_object_get_data(G_OBJECT(widget), "nwamui_signal_realize_id" ) == 0 ) {
        /* Add a handler to do it when realized, but avoid duplicate addition! */
        gulong signal_id = g_signal_connect( widget, "realize", (GCallback)nwamui_util_set_busy_cursor,  NULL );
        g_object_set_data(G_OBJECT(widget), "nwamui_signal_realize_id", (gpointer)signal_id );
    }
}

extern void
nwamui_util_restore_default_cursor( GtkWidget *widget )
{
    GdkWindow   *window = NULL;
    gulong       signal_id;

    if ( widget != NULL ) {
        window = gtk_widget_get_window( widget );
    }

    if ((signal_id = (gulong) g_object_get_data(G_OBJECT(widget),
            "nwamui_signal_realize_id")) != 0) {
        g_signal_handler_disconnect(widget, signal_id);
        g_object_set_data(G_OBJECT(widget), "nwamui_signal_realize_id", 0);
    }

    if ( window != NULL ) {
        gdk_window_set_cursor( window, NULL );
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

extern gboolean
nwamui_util_ask_about_dup_obj(GtkWindow* parent_window, NwamuiObject* obj )
{
    GtkWidget          *message_dialog;
    GtkWidget          *action_area;
    gchar              *summary = NULL;
    const gchar        *obj_name = NULL;
    const gchar        *obj_type_caps;
    const gchar        *obj_type_lower;
    gint                response;
    gchar*              outstr;

    g_return_val_if_fail( NWAMUI_IS_OBJECT(obj), FALSE );

    obj_name = nwamui_object_get_name( obj );

    if ( NWAMUI_IS_ENV( obj ) ) {
        obj_type_caps = _("Locations");
        obj_type_lower = _("location");
    } else if ( NWAMUI_IS_NCP( obj ) ) {
        obj_type_caps = _("Profiles");
        obj_type_lower = _("profile");
    } else {
        g_assert_not_reached();
    }

    summary = g_strdup_printf(_("Cannot rename '%s'"), obj_name?obj_name:"" );

    message_dialog = gtk_message_dialog_new(parent_window, GTK_DIALOG_MODAL |GTK_DIALOG_DESTROY_WITH_PARENT,
                                            GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL, summary );

    gtk_window_set_title(GTK_WINDOW(message_dialog), summary);

    gtk_message_dialog_format_secondary_text( GTK_MESSAGE_DIALOG(message_dialog), 
            _("%s can only be renamed immeditately\nafter they have been created. However, you\ncan duplicate this %s then immediately\nrename the duplicate."),
            obj_type_caps, obj_type_lower);

    /* Change OK to be Duplicate button */
    if ( (action_area = gtk_dialog_get_action_area( GTK_DIALOG(message_dialog) )) != NULL ) {
        GList*  buttons = gtk_container_get_children( GTK_CONTAINER( action_area ) );

        for ( GList *elem = buttons; buttons != NULL; elem = g_list_next(elem) ) {
            if ( GTK_IS_BUTTON( elem->data ) ) {
                const gchar* label = gtk_button_get_label( GTK_BUTTON( elem->data ) );
                if ( label != NULL && strcmp( label, GTK_STOCK_OK )  == 0 ) {
                    gtk_button_set_label( GTK_BUTTON( elem->data ), _("_Duplicate") );
                    break;
                }
            }
        }

        g_list_free( buttons );
    }

    g_free( summary );

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
    for ( int i = 0; elem && i < count; i++ ) {
        if ( elem->data != NULL && NWAMUI_IS_COND( elem->data ) ) {
            gchar* cond_str = nwamui_cond_to_string( NWAMUI_COND(elem->data) );
            if ( cond_str != NULL ) {
                cond_strs[*len] = strdup( cond_str );
                *len = *len + 1;
                g_free(cond_str);
            }
        }
        elem = g_list_next(elem);
    }

    return( cond_strs );
}

extern GList*
nwamui_util_strv_to_glist( gchar **strv ) 
{
    GList   *new_list = NULL;

    for ( char** strp = strv; strp != NULL && *strp != NULL && **strp != '\0'; strp++ ) {
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
 * Stores the netmask in `mask' for the given prefixlen `plen' and also sets
 * `ss_family' in `mask'.
 */
extern int
plen2mask(uint_t prefixlen, sa_family_t af, struct sockaddr_storage *mask)
{
	uint8_t	*addr;

	bzero(mask, sizeof (*mask));
	mask->ss_family = af;
	if (af == AF_INET) {
		if (prefixlen > IP_ABITS)
			return (EINVAL);
		addr = (uint8_t *)&((struct sockaddr_in *)mask)->
		    sin_addr.s_addr;
	} else {
		if (prefixlen > IPV6_ABITS)
			return (EINVAL);
		addr = (uint8_t *)&((struct sockaddr_in6 *)mask)->
		    sin6_addr.s6_addr;
	}

	while (prefixlen > 0) {
		if (prefixlen >= 8) {
			*addr++ = 0xFF;
			prefixlen -= 8;
			continue;
		}
		*addr |= 1 << (8 - prefixlen);
		prefixlen--;
	}
	return (0);
}

/*
 * Convert a prefix length to a netmask string.
 */
extern gchar*
nwamui_util_convert_prefixlen_to_netmask_str(sa_family_t family, guint prefixlen) 
{
    struct sockaddr_storage ss;
    char addr_str[INET6_ADDRSTRLEN];
    const char *addr_p;

    memset(&ss, 0, sizeof(ss));

    if (plen2mask(prefixlen, family, &ss) == 0) {
        if (family == AF_INET) {
            addr_p = inet_ntop(family,
              &((struct sockaddr_in *)&ss)->sin_addr,
              addr_str, INET_ADDRSTRLEN);
        } else {
            addr_p = inet_ntop(family,
              &((struct sockaddr_in6 *)&ss)->sin6_addr,
              addr_str, INET6_ADDRSTRLEN);
        }
    }

    return g_strdup(addr_p);
}

/*
 * Convert a mask to a prefix length.
 * Returns prefix length on success, -1 otherwise.
 */
extern int
mask2plen(const struct sockaddr_storage *mask)
{
	int rc = 0;
	uint8_t last;
	uint8_t *addr;
	int limit;

	if (mask->ss_family == AF_INET) {
		limit = IP_ABITS;
		addr = (uint8_t *)&((struct sockaddr_in *)mask)->
		    sin_addr.s_addr;
	} else {
		limit = IPV6_ABITS;
		addr = (uint8_t *)&((struct sockaddr_in6 *)mask)->
		    sin6_addr.s6_addr;
	}

	while (*addr == 0xff) {
		rc += 8;
		if (rc == limit)
			return (limit);
		addr++;
	}

	last = *addr;
	while (last != 0) {
		rc++;
		last = (last << 1) & 0xff;
	}

	return (rc);
}

/*
 * Convert a netmask string to prefix length
 */
extern gint
nwamui_util_convert_netmask_str_to_prefixlen(sa_family_t family, const gchar* netmask_str)
{
    struct sockaddr_storage ss;
	uint8_t *addr;

    memset(&ss, 0, sizeof(ss));

    if (family == AF_INET) {
		addr = (uint8_t *)&((struct sockaddr_in *)&ss)->sin_addr.s_addr;
    } else {
		addr = (uint8_t *)&((struct sockaddr_in6 *)&ss)->sin6_addr.s6_addr;
    }
    
    if (inet_pton(family, netmask_str, addr) == 1) {
        ss.ss_family = family;
        return mask2plen(&ss);
    }
    return -1;
}

extern void
nwamui_util_ncp_init_acquired_ip(NwamuiNcp *ncp)
{
    struct ifaddrs *ifap;
    struct ifaddrs *idx;
    NwamuiObject *ncu;

    nwamui_ncp_foreach_ncu(ncp, (GFunc)nwamui_ncu_clean_acquired, NULL);

    if (getifaddrs(&ifap) == 0) {

        for (idx = ifap; idx; idx = idx->ifa_next) {
            ncu = nwamui_ncp_get_ncu_by_device_name(ncp, idx->ifa_name);

            if (ncu) {
                char        addr_str[INET6_ADDRSTRLEN];
                char        mask_str[INET6_ADDRSTRLEN];
                const char *addr_p;
                const char *mask_p;

                /* Found it. */
                if (idx->ifa_addr->ss_family == AF_INET) {
                    addr_p = inet_ntop((int)idx->ifa_addr->ss_family,
                      &((struct sockaddr_in *)idx->ifa_addr)->sin_addr,
                      addr_str, INET_ADDRSTRLEN);
                    mask_p = inet_ntop((int)idx->ifa_netmask->ss_family,
                      &((struct sockaddr_in *)idx->ifa_netmask)->sin_addr,
                      mask_str, INET_ADDRSTRLEN);
                } else {
                    addr_p = inet_ntop((int)idx->ifa_addr->ss_family,
                      &((struct sockaddr_in6 *)idx->ifa_addr)->sin6_addr,
                      addr_str, INET6_ADDRSTRLEN);
                    mask_p = inet_ntop((int)idx->ifa_netmask->ss_family,
                      &((struct sockaddr_in6 *)idx->ifa_netmask)->sin6_addr,
                      mask_str, INET6_ADDRSTRLEN);
                }
                nwamui_ncu_add_acquired(NWAMUI_NCU(ncu), addr_p, mask_p, idx->ifa_flags);

                g_object_unref(ncu);
            }
        }

        freeifaddrs(ifap);
    }
}

extern gboolean
nwamui_util_get_interface_address(const char *ifname, sa_family_t family,
  gchar**address_p, gint *prefixlen_p, gboolean *is_dhcp_p)
{
    struct ifaddrs *ifap;
    struct ifaddrs *idx;

    if (getifaddrs(&ifap) == 0) {

        for (idx = ifap; idx; idx = idx->ifa_next) {
            if (g_strcmp0(ifname, idx->ifa_name) == 0
              && idx->ifa_addr->ss_family == family) {
                char        addr_str[INET6_ADDRSTRLEN];
                const char *addr_p;

                /* Found it. */
                if (idx->ifa_addr->ss_family == AF_INET) {
                    addr_p = inet_ntop((int)idx->ifa_addr->ss_family,
                      &((struct sockaddr_in *)idx->ifa_addr)->sin_addr,
                      addr_str, INET_ADDRSTRLEN);
                } else {
                    addr_p = inet_ntop((int)idx->ifa_addr->ss_family,
                      &((struct sockaddr_in6 *)idx->ifa_addr)->sin6_addr,
                      addr_str, INET6_ADDRSTRLEN);
                }

                if (address_p) {
                    *address_p =  g_strdup(addr_p?addr_p:"");
                }
                if (prefixlen_p) {
                    *prefixlen_p = mask2plen(idx->ifa_netmask);
                }
                if (is_dhcp_p != NULL) {
                    if (idx->ifa_flags & IFF_DHCPRUNNING) {
                        *is_dhcp_p = TRUE;
                    } else {
                        *is_dhcp_p = FALSE;
                    }
                }

                break;
            }
        }

        freeifaddrs(ifap);
        return TRUE;
    } else {
        g_debug("getifaddrs failed:", g_strerror(errno));
    }
    return FALSE;
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
insert_entry_valid_text_handler (GtkEditable *editable,
                             const gchar *text,
                             gint         length,
                             gint        *position,
                             gpointer     data)
{
    gboolean    is_v4;
    gboolean    is_v6;
    gboolean    is_prefix_only;
    gboolean    is_ethers;
    gboolean    allow_list;
    gboolean    allow_prefix;
    gboolean    is_valid = TRUE;
    gchar      *lower = g_ascii_strdown(text, length);
    nwamui_entry_validation_flags_t  flags;

    flags = (gboolean)g_object_get_data( G_OBJECT(editable), "validation_flags" );

    is_v4 = (flags & NWAMUI_ENTRY_VALIDATION_IS_V4);
    is_v6 = (flags & NWAMUI_ENTRY_VALIDATION_IS_V6);
    is_prefix_only = (flags & NWAMUI_ENTRY_VALIDATION_IS_PREFIX_ONLY);
    is_ethers = (flags & NWAMUI_ENTRY_VALIDATION_IS_ETHERS);
    allow_list = (flags & NWAMUI_ENTRY_VALIDATION_ALLOW_LIST);
    allow_prefix = (flags & NWAMUI_ENTRY_VALIDATION_ALLOW_PREFIX);

    for ( int i = 0; i < length && is_valid; i++ ) {
        if ( allow_list && (text[i] == ',' || text[i] == ' ') ) {
            /* Allow comma-separated list mode to be entered */
            is_valid = TRUE;
        }
        else if ( allow_prefix && (text[i] == '/' ) ) {
            /* Allow comma-separated list mode to be entered */
            is_valid = TRUE;
        }
        else if ( is_v6 ) {
            if ( is_prefix_only ) {
                /* Valid chars for v6 prefix are ASCII [0-9] */
                is_valid = g_ascii_isdigit( lower[i] );
            }
            else {
                /* Valid chars for v6 are ASCII [0-9a-f:.] */
                is_valid = (g_ascii_isxdigit( lower[i] ) || text[i] == ':' || text[i] == '.' );   
            }
        }
        else if ( is_v4 ) { /* Also handles is_prefix_only for IPv4 subnet */
            /* Valid chars for v4 are ASCII [0-9.] */
            is_valid = (g_ascii_isdigit( lower[i] ) || text[i] == '.' );   
        }
        else if ( is_ethers ) {
            /* Valid chars for ethers are ASCII [0-9a-f:] */
            is_valid = (g_ascii_isxdigit( lower[i] ) || text[i] == ':' );
        }
    }
    
    if ( is_valid ) {
        g_signal_handlers_block_by_func (editable,
                       (gpointer) insert_entry_valid_text_handler, data);

        gtk_editable_insert_text (editable, lower, length, position);
        g_signal_handlers_unblock_by_func (editable,
                                         (gpointer) insert_entry_valid_text_handler, data);
    }
    g_signal_stop_emission_by_name (editable, "insert_text");
    g_free (lower);
}

/* Validate a text entry
 * 
 * If show_error_dialog is TRUE, then a message dialog will be shown to the
 * user.
 *
 * Returns whether the entry was valid or not.
 */
extern gboolean
nwamui_util_validate_text_entry(    GtkWidget           *widget,
                                    const gchar         *text,
                                    nwamui_entry_validation_flags_t  flags,
                                    gboolean            show_error_dialog,
                                    gboolean            show_error_dialog_blocks )
{
    struct lifreq           lifr;
    struct sockaddr_in     *sin = (struct sockaddr_in *)&lifr.lifr_addr;
    struct sockaddr_in6    *sin6 = (struct sockaddr_in6 *)&lifr.lifr_addr;
    GtkWindow              *top_level = NULL;
    gboolean                is_v4 = (flags & NWAMUI_ENTRY_VALIDATION_IS_V4);
    gboolean                is_v6 = (flags & NWAMUI_ENTRY_VALIDATION_IS_V6);
    gboolean                is_prefix_only = (flags & NWAMUI_ENTRY_VALIDATION_IS_PREFIX_ONLY);
    gboolean                is_ethers = (flags & NWAMUI_ENTRY_VALIDATION_IS_ETHERS);
    gboolean                allow_list = (flags & NWAMUI_ENTRY_VALIDATION_ALLOW_LIST);
    gboolean                allow_prefix = (flags & NWAMUI_ENTRY_VALIDATION_ALLOW_PREFIX);
    gboolean                allow_empty = (flags & NWAMUI_ENTRY_VALIDATION_ALLOW_EMPTY);
    gboolean                is_valid = TRUE;
    gchar                  *error_string = NULL;

    if ( widget != NULL ) {
        top_level = GTK_WINDOW(gtk_widget_get_toplevel(widget));
    }

    if ( (widget != NULL && !GTK_WIDGET_IS_SENSITIVE(widget)) ||
         (top_level != NULL && !gtk_window_has_toplevel_focus (top_level)) ) {
        /* Assume valid */
        return( TRUE );;
    }

    if ( text == NULL || strlen (text) == 0 ) {
        if ( !allow_empty ) {
            is_valid = FALSE;
            error_string = g_strdup(_("Empty values are not permitted"));
        }
    }
    else if ( allow_list ) {
        /* Split list and call self recursively */
        nwamui_entry_validation_flags_t     no_list_flags;
        gchar                             **entries = NULL;

        no_list_flags = flags & (~ NWAMUI_ENTRY_VALIDATION_ALLOW_LIST); /* Turn off allow list flag */
        entries = g_strsplit_set(text, ", ", 0 );

        for( int i = 0; entries && entries[i] != NULL; i++ ) {
            /* Skip blank entries caused by combination of comma and space */
            if ( strlen( entries[i] ) == 0 ) {
                continue;
            }
            if ( ! nwamui_util_validate_text_entry( widget, entries[i], no_list_flags, FALSE, FALSE) ) {
                error_string = g_strdup_printf(_("The value '%s' is invalid."), entries[i] );
                gtk_widget_grab_focus( widget );
                is_valid = FALSE;
                break;
            }
        }
        if ( entries ) {
            g_strfreev( entries );
        }
    }
    else { /* Not list and not empty */
        gchar **strs =  NULL;

        /* Allow for address in format "addr/prefix" */
        strs = g_strsplit(text, "/", 2 );

        if ( strs == NULL || strs[0] == NULL ) {
            is_valid = FALSE;
        }
        else if ( !allow_prefix && strs[1] != NULL ) {
            /* Prefix found, but it's not allowed here */
            is_valid = FALSE;
            error_string = g_strdup(_("Specifying a network prefix value is not permitted in this context."));
        }
        else {
            if ( is_prefix_only ) { /* Need to check first since can conflict with IPv4/6 flag */
                if ( is_v4 )  {
                    if ( ! inet_pton ( AF_INET, strs[0], (void*)(&sin->sin_addr) ) ) {
                        is_valid = FALSE;
                        error_string = g_strdup_printf(_("The value '%s' is not a valid IPv4 subnet."), strs[0] );
                    }
                }
                else if ( is_v6 ) {
                    gchar *endptr;
                    gint64 prefix = g_ascii_strtoll( strs[0], &endptr, 10 );
                    if ( *endptr != '\0' || prefix == 0 || prefix > 128 ) {
                        is_valid = FALSE;
                        error_string = g_strdup_printf(_("The value '%s' is not a valid IPv6 network prefix."), strs[0] );
                    }
                }
            }
            else if ( is_ethers ) {
                struct ether_addr *ether = ether_aton( strs[0] );
                if ( ether == NULL ) {
                    is_valid = FALSE;
                    error_string = g_strdup_printf(_("The value '%s' is not a valid ethernet address."), strs[0] );
                }
            }
            else {  /* Is either V4 ot V6 address, could allow either, so need to check both */
                if ( is_v4 ) {
                    /* Validate an IPv4 Address */
                    if ( ! inet_pton ( AF_INET, strs[0], (void*)(&sin->sin_addr) ) ) {
                        is_valid = FALSE;
                        error_string = g_strdup_printf(_("The value '%s' is not a valid IPv4 address."), strs[0] );
                    }
                    if ( is_valid && strs[1] != NULL ) { /* Handle /N */
                        gint64 prefix = g_ascii_strtoll( strs[1], NULL, 10 );
                        if ( prefix == 0 || prefix > 32 ) {
                            is_valid = FALSE;
                            error_string = g_strdup_printf(_("The value '%s' is not a valid IPv4 network prefix."), strs[1] );
                        }
                    }
                }
                /* Only check v6 if we've not checked v4 address or we have
                 * checked the v4 address and it's not valid.
                 */
                if ( is_v6 && (!is_v4 || (is_v4 && !is_valid)) ) {
                    /* Validate an IPv6 Address */
                    if ( ! inet_pton ( AF_INET6, strs[0], (void*)(&sin6->sin6_addr) ) ) {
                        is_valid = FALSE;
                        if ( is_v4 ) {
                            error_string = g_strdup_printf(_("The value '%s' is not a valid IPv4 or IPv6 address."), strs[0] );
                        }
                        else {
                            error_string = g_strdup_printf(_("The value '%s' is not a valid IPv6 address."), strs[0] );
                        }
                    }
                    else {
                        is_valid = TRUE; /* Need to reset since could have been set to FALSE by v4 check */
                    }
                    if ( is_valid && strs[1] != NULL ) { /* Handle /N */
                        gchar *endptr;
                        gint64 prefix = g_ascii_strtoll( strs[1], &endptr, 10 );
                        if ( *endptr != '\0' || prefix == 0 || prefix > 128 ) {
                            is_valid = FALSE;
                            error_string = g_strdup_printf(_("The value '%s' is not a valid IPv6 network prefix."), strs[1] );
                        }
                    }
                }
            }
        }
        if ( strs != NULL) {
            g_strfreev( strs );
        }
    }

    if ( ! is_valid && show_error_dialog ) {
        GString*        message_str;

        if ( error_string != NULL ) {
            message_str = g_string_new(error_string);
            g_string_append(message_str, "\n\n");
        }
        else {
            message_str = g_string_new("");
        }

        if ( is_prefix_only ) {
            if ( is_v6 ) {
                g_string_append( message_str, _("IPv6 prefix length must be a decimal value between 1 and 128\n\n"));
            }
            else {
                g_string_append( message_str, _("Subnets must be in the format:\n\n    w.x.y.z\n\n"));
            }
        }
        else if ( is_ethers ) {
            g_string_append( message_str, _("Valid ethernet addresses or BSSIDs must be in the format::\n\n    xx:xx:xx:xx:xx:xx:xx:xx\n\n"));
        }
        else {
            if ( is_v4 ) {
                g_string_append( message_str,_("IPv4 addresses must be in the format:\n\n    w.x.y.z\n\n"));
                if ( allow_prefix ) {
                    g_string_append( message_str, _("If specifying a network prefix, you may append /N to the address:\n\n    w.x.y.z/N\n\nwhere N is between 1 and 32\n\n"));
                }
            }
            if ( is_v6 ) {
                g_string_append( message_str,_("IPv6 addresses must be in one of the formats :\n\n   x:x:x:x:x:x:x:x\n   x:x::x, etc.\n\n"));

                if ( allow_prefix ) {
                    g_string_append( message_str, _("If specifying a network prefix, you may append /N to the address:\n\n   x:x:x:x:x:x:x:x\n   x:x:x:x:x:x:x:x/N\n\nwhere N is between 1 and 128\n\n"));
                }
            }
        }

        if ( message_str != NULL ) {
            if ( allow_list ) {
                g_string_append(message_str, "You may also specify a list by separating entries using a comma (,) or space ( )");
            }
            nwamui_util_show_message(GTK_WINDOW(top_level), 
                                     GTK_MESSAGE_ERROR, _("Invalid Value"), message_str->str, show_error_dialog_blocks);

            g_string_free( message_str, TRUE );
        }
    }

    if ( error_string != NULL ) {
        g_free( error_string );
    }

    return( is_valid );
}

static gboolean
validate_text_entry_on_focus_exit(GtkWidget     *widget,
                          GdkEventFocus *event,
                          gpointer       data)
{
    gboolean                is_valid = TRUE;
    GtkWindow              *top_level = GTK_WINDOW(gtk_widget_get_toplevel(widget));
    const gchar            *text_str;
    nwamui_entry_validation_flags_t     
                            validation_flags;

    validation_flags = (gboolean)g_object_get_data( G_OBJECT(widget), "validation_flags" );

    if ( !GTK_WIDGET_IS_SENSITIVE(widget) || !gtk_window_has_toplevel_focus (top_level)) {
        /* If not sensitive, do nothing, since user can't edit it */
        return(FALSE);
    }

    g_signal_handlers_block_by_func (widget,
                   (gpointer) validate_text_entry_on_focus_exit, data);

    text_str = gtk_entry_get_text(GTK_ENTRY(widget));

    if ( ! nwamui_util_validate_text_entry( widget, text_str, validation_flags, TRUE, FALSE) ) {
        gtk_widget_grab_focus( widget );
    }

    g_signal_handlers_unblock_by_func (widget,
                   (gpointer) validate_text_entry_on_focus_exit, data);

    return(FALSE); /* Must return FALSE since GtkEntry expects it */
}

/* 
 * Utility function to attach an insert-text handler to a GtkEntry to limit
 * it's input to be characters acceptable to a valid IP address format.
 */
extern void
nwamui_util_set_entry_validation(   GtkEntry                        *entry, 
                                    nwamui_entry_validation_flags_t  flags,
                                    gboolean                         validate_on_focus_out )
{
    if ( entry != NULL ) {
        g_object_set_data( G_OBJECT(entry), "validation_flags", (gpointer)flags );

        g_signal_connect(G_OBJECT(entry), "insert_text", 
                         (GCallback)insert_entry_valid_text_handler, NULL);
        if ( validate_on_focus_out ) {
            g_signal_connect(G_OBJECT(entry), "focus-out-event", 
                             (GCallback)validate_text_entry_on_focus_exit, NULL);
        }
    }
}

/*
 * Change the flags set when checking ip address.
 */
extern void
nwamui_util_set_entry_validation_flags(  GtkEntry                        *entry, 
                                         nwamui_entry_validation_flags_t  flags )
{
    if ( entry != NULL ) {
        g_object_set_data( G_OBJECT(entry), "validation_flags", (gpointer)flags );
    }
}

extern void
nwamui_util_unset_entry_validation( GtkEntry* entry )
{
    if ( entry != NULL ) {
        g_signal_handlers_disconnect_by_func( G_OBJECT(entry), (gpointer)insert_entry_valid_text_handler, NULL );
        g_signal_handlers_disconnect_by_func( G_OBJECT(entry), (gpointer)validate_text_entry_on_focus_exit, NULL );
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

    new_title = g_strdup_printf(_("%s (%s)"), current_title?current_title:" ", hostname);

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
        /* Skip empty entries caused by delims side by side (e.g ', ') */
        if ( strlen( words[i] ) != 0 ) {
            list = g_list_append( list, (gpointer)words[i] );
        }
        else {
            g_free( words[i] ); /* Free empty strings or we leak them */
        }
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

        if ( prefix != NULL ) {
            prefix_num = atoi( delim );
            *prefix = nwamui_util_convert_prefixlen_to_netmask_str( (v6?AF_INET6:AF_INET), prefix_num );
        }
    }

    if ( address != NULL ) {
        *address = tmpstr;
    } else {
        g_free(tmpstr);
    }

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

extern void
nwamui_util_foreach_nwam_object_dup_and_append_to_list(NwamuiObject *obj, GList **list)
{
    g_return_if_fail(NWAMUI_IS_OBJECT(obj));
    *list = g_list_prepend(*list, g_object_ref(obj));
}

extern gint
nwamui_util_find_nwamui_object_by_name(gconstpointer obj, gconstpointer name)
{
    const gchar *cur_name = NULL;
    gchar       *obj_name = (gchar *)name;

    g_return_val_if_fail(NWAMUI_IS_OBJECT(obj), 1);
    g_return_val_if_fail(name, 1);

    cur_name = nwamui_object_get_name(NWAMUI_OBJECT(obj));
    return g_strcmp0(cur_name, obj_name);
}

extern gint
nwamui_util_find_active_nwamui_object(gconstpointer data, gconstpointer user_data)
{
	NwamuiObject  *obj     = (NwamuiObject *)data;
	NwamuiObject **ret_obj = (NwamuiObject **)user_data;
	
    g_return_val_if_fail(NWAMUI_IS_OBJECT(obj), 1);
    g_return_val_if_fail(user_data, 1);
    
	if (nwamui_object_get_active(obj)) {
        *ret_obj = g_object_ref(obj);
        return 0;
	}
    return 1;
}

extern void
nwamui_util_foreach_nwam_object_add_to_list_store(gpointer object, gpointer list_store)
{
    GtkTreeIter   iter;
    NwamuiObject* obj = NWAMUI_OBJECT(object);
    
    gtk_list_store_append(GTK_LIST_STORE(list_store), &iter);
    gtk_list_store_set(GTK_LIST_STORE(list_store), &iter, 0, obj, -1);
}

static gboolean
capplet_tree_model_foreach(GtkTreeModel *model,
  GtkTreePath *path,
  GtkTreeIter *iter,
  gpointer user_data)
{
	CappletForeachData *data = (CappletForeachData *)user_data;

    if (data->foreach_func(model, path, iter, data->user_data)) {
        *(GtkTreeIter *)data->user_data1 = *iter;
        /* flag */
        data->ret_data = data->user_data1;
        return TRUE;
    }
	return FALSE;
}

static gboolean
capplet_model_foreach_find_object(GtkTreeModel *model,
    GtkTreePath *path,
    GtkTreeIter *iter,
    gpointer user_data)
{
	CappletForeachData *data = (CappletForeachData *)user_data;
	GObject *object;

    gtk_tree_model_get( GTK_TREE_MODEL(model), iter, 0, &object, -1);

    if (object == data->user_data) {
        /* Fill in passed GtkTreeIter */
        *(GtkTreeIter *)data->user_data1 = *iter;
        /* return value now */
        data->ret_data = data->user_data1;
    }

	if (object)
		g_object_unref(object);

	return data->ret_data != NULL;
}

/*
 * capplet_model_find_object:
 * @object: Could be null
 * @iter: output var
 * 
 * Find the position of the @object, return its iterator.
 */
extern gboolean
capplet_model_find_object(GtkTreeModel *model, GObject *object, GtkTreeIter *iter)
{
	CappletForeachData data;
	GtkTreeIter temp_iter;

	data.user_data = (gpointer)object;
    if (iter != NULL) {
        data.user_data1 = (gpointer)iter;
    } else {
        data.user_data1 = &temp_iter;
    }
	data.ret_data = NULL;

	gtk_tree_model_foreach(model, capplet_model_foreach_find_object,
      (gpointer)&data);

	return data.ret_data != NULL;
}

/*
 * capplet_model_find_object:
 * @object: Could be null
 * @iter: output var
 * 
 * Find the position of the @object, return its iterator.
 */
extern gboolean
capplet_model_find_object_with_parent(GtkTreeModel *model, GtkTreeIter *parent, GObject *object, GtkTreeIter *iter)
{
	CappletForeachData data;
	GtkTreeIter temp_iter;
    GtkTreeIter i;
    gboolean valid;

	data.user_data = (gpointer)object;
    if (iter != NULL) {
        data.user_data1 = (gpointer)iter;
    } else {
        data.user_data1 = &temp_iter;
    }
	data.ret_data = NULL;

    for (valid = gtk_tree_model_iter_children(model, &i, parent);
         valid;
         valid = gtk_tree_model_iter_next(model, &i)) {
        if (capplet_model_foreach_find_object(model, NULL, &i, (gpointer)&data))
            return TRUE;
    }
    return FALSE;
}

/*
 * capplet_model_find_object:
 * @object: Could be null
 * @iter: output var
 *
 * Customize foreach function by @fun and @user_data, find the correct position,
 * return its iterator.
 */
extern gboolean
capplet_model_foreach(GtkTreeModel *model, GtkTreeModelForeachFunc func, gpointer user_data, GtkTreeIter *iter)
{
	CappletForeachData data;
	GtkTreeIter temp_iter;

    data.foreach_func = func;
	data.user_data = user_data;
    if (iter != NULL) {
        data.user_data1 = (gpointer)iter;
    } else {
        data.user_data1 = &temp_iter;
    }
	data.ret_data = NULL;

	gtk_tree_model_foreach(model, capplet_tree_model_foreach, (gpointer)&data);

	return data.ret_data != NULL;
}

/*
 * capplet_model_find_object:
 * @object: Could be null
 * @iter: output var
 *
 * Customize foreach function by @fun and @user_data, find the correct position,
 * return its iterator.
 */
extern gboolean
capplet_model_1_level_foreach(GtkTreeModel *model, GtkTreeIter *parent, GtkTreeModelForeachFunc func, gpointer user_data, GtkTreeIter *iter)
{
    gboolean valid;
	GtkTreeIter temp_iter;
	GtkTreePath *path = NULL;

    if (parent) {
        path = gtk_tree_model_get_path(model, parent);
    } else {
        path = gtk_tree_path_new_first();
    }

    if (iter == NULL)
        iter = &temp_iter;

    for (valid = gtk_tree_model_iter_children(model, iter, parent), gtk_tree_path_down(path);
         valid;
         valid = gtk_tree_model_iter_next(model, iter), gtk_tree_path_next(path)) {
        if (func(model, path, iter, user_data))
            break;
    }
    gtk_tree_path_free(path);
    return valid;
}

