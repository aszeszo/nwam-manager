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
#include <gtk/gtk.h>
#include <gtk/gtkstatusicon.h>
#include <gconf/gconf-client.h>

#include "nwam-menu.h"
#include "nwam-menuitem.h"
#include "libnwamui.h"

/* This might seem a bit overkill, but once we start handling more than 1 icon it should make it 
 * easier to do. Currently the index parameter is ignored...
 */

static GtkStatusIcon*   status_icon                     = NULL; /* TODO - Allow for more than 1 icon. */
static GCallback       status_icon_activate_callback    = NULL;

static GtkStatusIcon*   get_status_icon( gint index );
static gint icon_stock_index;

static gboolean animation_panel_icon_timeout (gpointer data);
static void trigger_animation_panel_icon (GConfClient *client,
                                          guint cnxn_id,
                                          GConfEntry *entry,
                                          gpointer user_data);

static gboolean
animation_panel_icon_timeout (gpointer data)
{
	gtk_status_icon_set_from_pixbuf(status_icon, 
                                    nwamui_util_get_env_status_icon(
                                            (nwamui_env_status_t)(++icon_stock_index)%NWAMUI_ENV_STATUS_LAST));
	return TRUE;
}

static void
trigger_animation_panel_icon (GConfClient *client,
                              guint cnxn_id,
                              GConfEntry *entry,
                              gpointer user_data)
{
	static gint animation_panel_icon_timeout_id = 0;
	GConfValue *value = NULL;
	
	g_assert (entry);
	value = gconf_entry_get_value (entry);
	g_assert (value);
	if (gconf_value_get_bool (value)) {
		g_assert (animation_panel_icon_timeout_id == 0);
		animation_panel_icon_timeout_id = 
			g_timeout_add (333, animation_panel_icon_timeout, NULL);
		g_assert (animation_panel_icon_timeout_id > 0);
	} else {
		g_assert (animation_panel_icon_timeout_id > 0);
		g_source_remove (animation_panel_icon_timeout_id);
		animation_panel_icon_timeout_id = 0;
		/* reset everything of animation_panel_icon here */
		gtk_status_icon_set_from_pixbuf(status_icon, 
                                        nwamui_util_get_env_status_icon(NWAMUI_ENV_STATUS_CONNECTED));
		icon_stock_index = 0;
	}
}

static void
animation_panel_icon_init ()
{
    gtk_status_icon_set_from_pixbuf(status_icon, 
                                    nwamui_util_get_env_status_icon(NWAMUI_ENV_STATUS_CONNECTED));
}

gint 
nwam_status_icon_create( void )
{
    GtkStatusIcon* s_icon = get_status_icon( 0 ); /* TODO - have to handle dynamic index */
    
    return ( 0 );
}

GtkStatusIcon* nwam_status_icon_get_widget( gint index )
{
    GtkStatusIcon* s_icon = get_status_icon( index );
    return s_icon;
}

void 
nwam_status_icon_set_activate_callback( gint index, GCallback activate_cb )
{
    status_icon_activate_callback = activate_cb;
}

void
nwam_status_icon_show( gint index )
{
    GtkStatusIcon* s_icon = get_status_icon( index );
    
    gtk_status_icon_set_visible( s_icon, TRUE);
}

void
nwam_status_icon_set_tooltip (gint index, const gchar *str)
{
	const gchar *ttstr = str;
	GtkStatusIcon* s_icon = get_status_icon (index);
	
	if (str && strlen (str) == 0)
		ttstr = NULL;
	
	gtk_status_icon_set_tooltip (s_icon, ttstr);
}

void
nwam_status_icon_hide( gint index )
{
    GtkStatusIcon* s_icon = get_status_icon( index );
    
    gtk_status_icon_set_visible( s_icon, TRUE);
}

void
nwam_status_icon_blink( gint index )
{
    GtkStatusIcon* s_icon = get_status_icon( index );
    
    gtk_status_icon_set_blinking( s_icon, TRUE );
}

void
nwam_status_icon_no_blink( gint index )
{
    GtkStatusIcon* s_icon = get_status_icon( index );
    
    gtk_status_icon_set_blinking( s_icon, FALSE );
}

static void
status_icon_show_menu( gint index, guint button, guint activate_time ) 
{
	GtkStatusIcon* s_icon = get_status_icon( index );
	GtkWidget*     s_menu = get_status_icon_menu( index );

	/* TODO - handle dynamic menu, for now let's do nothing, if there's no menu. */
	if ( s_menu != NULL ) {
		gtk_menu_popup(GTK_MENU(s_menu),
			       NULL,
			       NULL,
			       gtk_status_icon_position_menu,
			       s_icon,
			       button,
			       activate_time);
		gtk_widget_show_all(s_menu);
	}
}

static void
status_icon_popup_cb(GtkStatusIcon *status_icon,
        guint button,
        guint activate_time,
        gpointer user_data)
{
    gint index = (gint)user_data;
    
    status_icon_show_menu( index, button, activate_time );
}

static void
status_icon_activate_cb(GtkStatusIcon *status_icon, gpointer user_data)
{
    gint index = (gint)user_data;

    if ( status_icon_activate_callback != NULL ) {
        (*status_icon_activate_callback)( );
    }
}

static GtkStatusIcon*
get_status_icon( gint index ) { 
    if ( status_icon == NULL ) {  

	status_icon = gtk_status_icon_new();
	animation_panel_icon_init ();
        
        /* Connect signals */
        g_signal_connect (G_OBJECT (status_icon), "popup-menu",
                          G_CALLBACK (status_icon_popup_cb), (gpointer)index);

        g_signal_connect (G_OBJECT (status_icon), "activate",
                          G_CALLBACK (status_icon_activate_cb), (gpointer)index);

    }
    g_assert( status_icon != NULL );

    return (status_icon);
}

