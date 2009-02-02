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
 * File:   nwam_wireless_dialog.c
 *
 */

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n.h>

#include "libnwamui.h"
#include "nwam_wireless_chooser.h"
#include "nwam_pref_iface.h"
#include "nwam_wireless_dialog.h"

/* Names of Widgets in Glade file */
#define WIRELESS_CHOOSER_DIALOG        "connect_wireless_network"
#define WIRELESS_TABLE                 "wireless_list"
#define WIRELESS_ADD_TO_PREFERRED_CBOX "add_to_preferred_cbox"

struct _NwamWirelessChooserPrivate {
	/* Widget Pointers */
    GtkDialog* wireless_chooser;
    GtkTreeView *wifi_tv;
    GtkCheckButton *add_to_preferred_cbox;

	/* Other Data */
    NwamuiDaemon*       daemon;
    gboolean            join_open;
    gboolean            join_preferred;
    gboolean            add_any_wifi;
    gint                action_if_no_fav;
};

enum {
	WIFI_FAV_ESSID=0,
	WIFI_FAV_SECURITY,
	WIFI_FAV_SPEED,
    WIFI_FAV_SIGNAL
};

static void nwam_pref_init (gpointer g_iface, gpointer iface_data);
static gboolean refresh(NwamPrefIFace *iface, gpointer user_data, gboolean force);
static gboolean apply(NwamPrefIFace *iface, gpointer user_data);
static gboolean help(NwamPrefIFace *iface, gpointer user_data);
static gint dialog_run(NwamPrefIFace *iface, GtkWindow *parent);

static void nwam_wireless_chooser_finalize(NwamWirelessChooser *self);
static void nwam_create_wifi_cb (GObject *daemon, GObject *wifi, gpointer data);
static void nwam_rescan_wifi (NwamWirelessChooser *self);

/* Callbacks */
static void object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);
static void nwam_wifi_chooser_cell_cb (GtkTreeViewColumn *col,
  GtkCellRenderer   *renderer,
  GtkTreeModel      *model,
  GtkTreeIter       *iter,
  gpointer           data);
static gint nwam_wifi_chooser_comp_cb (GtkTreeModel *model,
  GtkTreeIter *a,
  GtkTreeIter *b,
  gpointer user_data);
static void wifi_add (NwamWirelessChooser *self, GtkTreeModel *model, NwamuiWifiNet *wifi);
static void wifi_remove (NwamWirelessChooser *self, GtkTreeModel *model, NwamuiWifiNet *wifi);

G_DEFINE_TYPE_EXTENDED (NwamWirelessChooser,
                        nwam_wireless_chooser,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (NWAM_TYPE_PREF_IFACE, nwam_pref_init))

static void
nwam_pref_init (gpointer g_iface, gpointer iface_data)
{
	NwamPrefInterface *iface = (NwamPrefInterface *)g_iface;
	iface->refresh = refresh;
	iface->apply = apply;
    iface->help = help;
    iface->dialog_run = dialog_run;
}

static void
nwam_wireless_chooser_class_init(NwamWirelessChooserClass *klass)
{
	/* Pointer to GObject Part of Class */
	GObjectClass *gobject_class = (GObjectClass*) klass;
		
	/* Override Some Function Pointers */
	gobject_class->finalize = (void (*)(GObject*)) nwam_wireless_chooser_finalize;
}

static void
nwam_compose_wifi_chooser_view (NwamWirelessChooser *self, GtkTreeView *view)
{
    GtkTreeViewColumn *col;
    GtkCellRenderer *renderer;
    GtkTreeModel *model;

    model = GTK_TREE_MODEL(gtk_list_store_new (1, G_TYPE_OBJECT /* NwamuiWifiNet Object */));
    gtk_tree_view_set_model (view, model);

    g_object_set (G_OBJECT(view),
      "headers-clickable", FALSE,
      "reorderable", TRUE,
      NULL);

    // Column:	WIFI_FAV_ESSID
	renderer = gtk_cell_renderer_pixbuf_new();
	col = gtk_tree_view_column_new_with_attributes(_("Name (ESSID)"),
      renderer,
      "expand", TRUE,
      "resizable", TRUE,
      "clickable", FALSE,
      "sort-indicator", FALSE,
      "reorderable", FALSE,
      NULL);
    gtk_tree_view_append_column (view, col);
    /* first signal strength icon cell */
	gtk_tree_view_column_set_cell_data_func (col,
      renderer,
      nwam_wifi_chooser_cell_cb,
      (gpointer) 0,
      NULL);
    g_object_set_data(G_OBJECT(renderer), "nwam_wifi_fav_column_id", GUINT_TO_POINTER(WIFI_FAV_SIGNAL));

    /* second ESSID text cell */
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_end(col, renderer, TRUE);
    gtk_tree_view_column_set_cell_data_func (col,
      renderer,
      nwam_wifi_chooser_cell_cb,
      (gpointer) self,
      NULL);
    g_object_set(G_OBJECT(renderer), "editable", FALSE, NULL);
    g_object_set_data(G_OBJECT(renderer), "nwam_wifi_fav_column_id", GUINT_TO_POINTER(WIFI_FAV_ESSID));
    
    // Column:	WIFI_FAV_SPEED
    renderer = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes(_("Speed"),
      renderer,
      "expand", TRUE,
      "resizable", TRUE,
      "clickable", FALSE,
      "sort-indicator", FALSE,
      "reorderable", FALSE,
      NULL);
    gtk_tree_view_append_column (view, col);
    gtk_tree_view_column_set_cell_data_func (col,
      renderer,
      nwam_wifi_chooser_cell_cb,
      (gpointer) self,
      NULL);
    g_object_set(G_OBJECT(renderer), "editable", FALSE, NULL);
    g_object_set_data(G_OBJECT(renderer), "nwam_wifi_fav_column_id", GUINT_TO_POINTER(WIFI_FAV_SPEED));

    // Column:	WIFI_FAV_SECURITY
    renderer = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes(_("Security"),
      renderer,
      "expand", TRUE,
      "resizable", TRUE,
      "clickable", FALSE,
      "sort-indicator", FALSE,
      "reorderable", FALSE,
      NULL);
    gtk_tree_view_append_column (view, col);
    gtk_tree_view_column_set_cell_data_func (col,
      renderer,
      nwam_wifi_chooser_cell_cb,
      (gpointer) self,
      NULL);
    g_object_set(G_OBJECT(renderer), "editable", FALSE, NULL);
    g_object_set_data(G_OBJECT(renderer), "nwam_wifi_fav_column_id", GUINT_TO_POINTER(WIFI_FAV_SECURITY));

}

static void
nwam_wireless_chooser_init(NwamWirelessChooser *self)
{
	self->prv = g_new0(NwamWirelessChooserPrivate, 1);
	
    self->prv->daemon = nwamui_daemon_get_instance();
    
	/* Iniialise pointers to important widgets */
    self->prv->wireless_chooser = GTK_DIALOG(nwamui_util_glade_get_widget(WIRELESS_CHOOSER_DIALOG));
    self->prv->wifi_tv  = GTK_TREE_VIEW(nwamui_util_glade_get_widget(WIRELESS_TABLE));
    nwam_compose_wifi_chooser_view ( self, self->prv->wifi_tv );

    self->prv->add_to_preferred_cbox = GTK_CHECK_BUTTON(nwamui_util_glade_get_widget(WIRELESS_ADD_TO_PREFERRED_CBOX));

	g_signal_connect((gpointer) self->prv->daemon, "wifi_scan_result",
      (GCallback)nwam_create_wifi_cb, (gpointer) self);
	g_signal_connect(G_OBJECT(self), "notify",
      (GCallback)object_notify_cb, NULL);

    /* Populate WiFi conditions */
    {
        NwamuiProf*     prof;

        prof = nwamui_prof_get_instance ();
        g_object_get (prof,
          "action_on_no_fav_networks", &self->prv->action_if_no_fav,
          "join_wifi_not_in_fav", &self->prv->join_open,
          "join_any_fav_wifi", &self->prv->join_preferred,
          "add_any_new_wifi_to_fav", &self->prv->add_any_wifi,
          NULL);
            
        g_object_unref (prof);
    }
    nwam_pref_refresh (NWAM_PREF_IFACE(self), NULL, TRUE);
}

static void
populate_panel( NwamWirelessChooser* self, gboolean set_initial_state )
{
    g_assert( NWAM_IS_WIRELESS_CHOOSER(self));

    /* Populate WiFi */
    nwam_rescan_wifi (self);
}

static void
wifi_add (NwamWirelessChooser *self, GtkTreeModel *model, NwamuiWifiNet *wifi)
{
	GtkTreeIter         iter;
	
	gtk_list_store_prepend(GTK_LIST_STORE(model), &iter );
	gtk_list_store_set(GTK_LIST_STORE(model), &iter, 0, wifi, -1);
}

static void
wifi_remove (NwamWirelessChooser *self, GtkTreeModel *model, NwamuiWifiNet *wifi)
{
    /* TODO - seems we dont need to remove a wifi */
}

static void
nwam_rescan_wifi (NwamWirelessChooser *self)
{
    GtkTreeModel *model;

    model = gtk_tree_view_get_model (self->prv->wifi_tv);

    gtk_widget_hide (GTK_WIDGET(self->prv->wifi_tv));
    
    gtk_list_store_clear (GTK_LIST_STORE(model));

    nwamui_daemon_wifi_start_scan (self->prv->daemon);
}

/**
 * nwam_wireless_chooser_new:
 * @returns: a new #NwamWirelessChooser.
 *
 * Creates a new #NwamWirelessChooser
 **/
NwamWirelessChooser*
nwam_wireless_chooser_new(void)
{
	return NWAM_WIRELESS_CHOOSER(g_object_new(NWAM_TYPE_WIRELESS_CHOOSER, NULL));
}

static gint
dialog_run(NwamPrefIFace *iface, GtkWindow *parent)
{
    NwamWirelessChooser *self = NWAM_WIRELESS_CHOOSER(iface);
    NwamWirelessChooserPrivate* prv = self->prv;
	gint response = GTK_RESPONSE_NONE;
	
	g_assert(NWAM_IS_WIRELESS_CHOOSER(self));
	if (parent) {
		gtk_window_set_transient_for (GTK_WINDOW(self->prv->wireless_chooser), parent);
		gtk_window_set_modal (GTK_WINDOW(self->prv->wireless_chooser), TRUE);
	} else {
		gtk_window_set_transient_for (GTK_WINDOW(self->prv->wireless_chooser), NULL);
		gtk_window_set_modal (GTK_WINDOW(self->prv->wireless_chooser), FALSE);		
	}
	
	if ( self->prv->wireless_chooser != NULL ) {
		response =  gtk_dialog_run(GTK_DIALOG(self->prv->wireless_chooser));
		
		gtk_widget_hide( GTK_WIDGET(self->prv->wireless_chooser) );
	}
	return(response);
}

/**
 * refresh:
 *
 * Refresh #NwamWirelessChooser
 **/
static gboolean
refresh(NwamPrefIFace *iface, gpointer user_data, gboolean force)
{
    NwamWirelessChooser *self = NWAM_WIRELESS_CHOOSER(iface);
    NwamWirelessChooserPrivate* prv = self->prv;
    g_assert( NWAM_IS_WIRELESS_CHOOSER(self));
    
    populate_panel(NWAM_WIRELESS_CHOOSER(self), force);
}

static gboolean
apply(NwamPrefIFace *iface, gpointer user_data)
{
    NwamWirelessChooser *self = NWAM_WIRELESS_CHOOSER(iface);
    NwamWirelessChooserPrivate* prv = self->prv;
    g_assert( NWAM_IS_WIRELESS_CHOOSER(iface));
}

/**
 * help:
 *
 * Help #NwamWirelessChooser
 **/
static gboolean
help(NwamPrefIFace *iface, gpointer user_data)
{
    g_debug ("NwamWirelessChooser: Help");
    nwamui_util_show_help ("");
}

static void
nwam_wireless_chooser_finalize(NwamWirelessChooser *self)
{
    if ( self->prv->daemon != NULL ) {
        g_object_unref( self->prv->daemon );
    }

    g_free(self->prv);
    self->prv = NULL;

    G_OBJECT_CLASS(nwam_wireless_chooser_parent_class)->finalize(G_OBJECT(self));
}

/* Callbacks */

/*
 * We don't need a comp here actually due to requirements
 */
static gint
nwam_wifi_chooser_comp_cb (GtkTreeModel *model,
  GtkTreeIter *a,
  GtkTreeIter *b,
  gpointer user_data)
{
	return 0;
}

static void
object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
	g_debug("NwamWirelessChooser: notify %s changed\n", arg1->name);
}

static void
nwam_wifi_chooser_cell_cb (GtkTreeViewColumn *col,
  GtkCellRenderer   *renderer,
  GtkTreeModel      *model,
  GtkTreeIter       *iter,
  gpointer           data)
{
    NwamWirelessChooser *self = NWAM_WIRELESS_CHOOSER(data);
    NwamuiWifiNet       *wifi_info  = NULL;

    gtk_tree_model_get(GTK_TREE_MODEL(model), iter, 0, &wifi_info, -1);
    
    g_assert(NWAMUI_IS_WIFI_NET(wifi_info));
    
    switch (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (renderer), "nwam_wifi_fav_column_id"))) {
    case WIFI_FAV_ESSID:
    {   
        gchar*  essid = nwamui_wifi_net_get_essid(wifi_info);
            
        g_object_set (G_OBJECT(renderer),
          "text", essid?essid:"",
          NULL);
                    
        g_free(essid);
    }
    break;
    case WIFI_FAV_SECURITY:
    {   
        nwamui_wifi_security_t sec = nwamui_wifi_net_get_security(wifi_info);
            
        g_object_set (G_OBJECT(renderer),
          "text", nwamui_util_wifi_sec_to_short_string(sec),
          NULL);
                    
    }
    break;
    case WIFI_FAV_SPEED:
    {   
        guint   speed = nwamui_wifi_net_get_speed( wifi_info );
        gchar*  str = g_strdup_printf("%uMb", speed);
            
        g_object_set (G_OBJECT(renderer),
          "text", str,
          NULL);
            
        g_free( str );
    }
    break;
    case WIFI_FAV_SIGNAL:
    {   
        GdkPixbuf              *status_icon;

        nwamui_wifi_signal_strength_t signal = nwamui_wifi_net_get_signal_strength(wifi_info);
        status_icon = nwamui_util_get_wireless_strength_icon(signal, TRUE);
        g_object_set (G_OBJECT(renderer),
          "pixbuf", status_icon,
          NULL);
        g_object_unref(G_OBJECT(status_icon));
    }
    break;
    default:
        g_assert_not_reached ();
    }
}

static void
nwam_create_wifi_cb (GObject *daemon, GObject *wifi, gpointer data)
{
    NwamWirelessChooser* self = NWAM_WIRELESS_CHOOSER(data);
    GtkTreeModel *model;

    model = gtk_tree_view_get_model (self->prv->wifi_tv);

    /* TODO - Make this more efficient */
	if (wifi) {
        wifi_add (self, model, NWAMUI_WIFI_NET(wifi));
	} else {
        /* scan is over */
        gtk_widget_show (GTK_WIDGET(self->prv->wifi_tv));
    }
}

