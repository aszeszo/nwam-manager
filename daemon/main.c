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
#include <glib/gi18n.h>

#include "status_icon.h"
#include "notify.h"
#include "libnwamui.h"
#include "nwam-menu.h"

static GtkWidget *my_menu = NULL;
static GtkStatusIcon *status_icon = NULL;

static void 
cleanup_and_exit(GtkWidget *widget,
        gpointer data)
{
    gtk_main_quit ();
}

static void
on_blink_change(GtkStatusIcon *widget, 
        gpointer data)
{
    gboolean blink = GPOINTER_TO_UINT(data);
    g_debug("Set blinking %s", (blink) ? "on" : "off");
    gtk_status_icon_set_blinking(GTK_STATUS_ICON(status_icon), blink);
}

void
get_state (GObject *obj, gpointer data)
{
	GString *str = (GString *)data;
	NwamuiNcu *ncu = NWAMUI_NCU (obj);
	gchar *format = NULL;
	gchar *name = NULL;
	
	switch (nwamui_ncu_get_ncu_type (ncu)) {
		case NWAMUI_NCU_TYPE_WIRED:
			format = _("\nWired (%s): %s");
			break;
		case NWAMUI_NCU_TYPE_WIRELESS:
			format = _("\nWireless (%s): %s");
			break;
	}
	name = nwamui_ncu_get_device_name (ncu);
	g_string_append_printf (str, format, name, "test again");
}

static void
on_trigger_network_changed (NwamuiDaemon* daemon, NwamuiEnv* env, gint sicon_id)
{
	GString *str;
	gchar *env_name = NULL;
	NwamuiNcp *ncp = NULL;
	
	env_name = nwamui_env_get_name (env);
	str = g_string_new (_("Environment: "));
	g_string_append_printf (str, "%s", env_name);
	g_free (env_name);
	
	ncp = nwamui_daemon_get_active_ncp (daemon);
	nwamui_ncp_foreach_ncu (ncp, G_CALLBACK(get_state), (gpointer)str);
	nwam_status_icon_set_tooltip (sicon_id, str->str);
	g_string_free (str, TRUE);
}

static void
activate_cb ()
{
    /*
    nwam_notification_show_message("This is the summary", "This is the body...\nPara1", NULL );
    */
    nwam_exec ("-e");
}

int main( int argc, 
      char* argv[] )
{
    gint    status_icon_index = 0;
    NwamMenu *menu;
    NwamuiDaemon* daemon;
    
    gtk_init( &argc, &argv );

    daemon = nwamui_daemon_get_instance ();
    status_icon_index = nwam_status_icon_create();
    menu = get_nwam_menu_instance();
    
    nwam_notification_init( nwam_status_icon_get_widget(status_icon_index) );

    nwam_status_icon_set_activate_callback( status_icon_index, G_CALLBACK(activate_cb) );

    nwam_status_icon_show( status_icon_index );
    g_signal_connect(daemon, "active_env_changed",
	    G_CALLBACK(on_trigger_network_changed),
	    status_icon_index);
    
    /* first time app trigger tooltip */
    on_trigger_network_changed (daemon, nwamui_daemon_get_active_env (daemon), status_icon_index);
    gtk_main();

    nwam_notification_cleanup();
    g_object_unref (daemon);
    g_object_unref (G_OBJECT (menu));
    
    return 0;
}
