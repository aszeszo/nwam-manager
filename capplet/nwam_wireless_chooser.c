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
 * File:   nwam_wireless_dialog.c
 *
 */

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "libnwamui.h"
#include "capplet-utils.h"
#include "nwam_wireless_chooser.h"
#include "nwam_pref_iface.h"
#include "nwam_wireless_dialog.h"

/* Names of Widgets in Glade file */
#define WIRELESS_CHOOSER_DIALOG        "connect_wireless_network"
#define WIRELESS_TABLE                 "wireless_list"
#define WIRELESS_ADD_TO_PREFERRED_CBOX "chooser_add_to_preferred_cbox"
#define WIRELESS_CONNECT_WIRELESS_OK_BTN "connect_wireless_connect_btn"
#define WIRELESS_CONNECT_WIRELESS_REFRESH_BTN "connect_wireless_refresh_btn"

struct _NwamWirelessChooserPrivate {
	/* Widget Pointers */
    GtkDialog* wireless_chooser;
    GtkTreeView *wifi_tv;
    GtkCheckButton *add_to_preferred_cbox;
    GtkWidget      *connect_wireless_connect_btn;
    GtkWidget      *connect_wireless_refresh_btn;

	/* Other Data */
    NwamuiDaemon*       daemon;
    gboolean            join_open;
    gboolean            join_preferred;
    gint                action_if_no_fav;
    NwamuiWifiNet      *selected_wifi;
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
static gboolean cancel(NwamPrefIFace *iface, gpointer user_data);
static gboolean help(NwamPrefIFace *iface, gpointer user_data);
static gint dialog_run(NwamPrefIFace *iface, GtkWindow *parent);
static GtkWindow* dialog_get_window(NwamPrefIFace *iface);

static void nwam_wireless_chooser_finalize(NwamWirelessChooser *self);

/* Callbacks */
static void refresh_btn_clicked( GtkButton *widget, gpointer user_data);
static void nwam_wifi_selection_changed(GtkTreeSelection *selection, gpointer data);
static void response_cb( GtkWidget* widget, gint repsonseid, gpointer data );
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
static void presistant_cb(GtkToggleButton* widget, gpointer data);

/* Daemon */
static void daemon_status_changed(NwamuiDaemon *daemon, GParamSpec *arg1, gpointer user_data);
static void daemon_active_ncp_changed(NwamuiDaemon *daemon, GParamSpec *arg1, gpointer user_data);
static void daemon_add_object(NwamuiDaemon *daemon, NwamuiObject* object, gpointer user_data);
static void daemon_remove_object(NwamuiDaemon *daemon, NwamuiObject* object, gpointer user_data);

/* Prof */
static void add_any_new_wifi_to_fav(GObject *gobject, GParamSpec *arg1, gpointer data);

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
	iface->cancel = cancel;
    iface->help = help;
    iface->dialog_run = dialog_run;
    iface->dialog_get_window = dialog_get_window;
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

    model = GTK_TREE_MODEL(gtk_list_store_new (1, NWAMUI_TYPE_WIFI_NET));
    gtk_tree_view_set_model (view, model);

    g_object_set (G_OBJECT(view),
      "headers-clickable", FALSE,
      "reorderable", FALSE,
      NULL);

    // Column:	WIFI_FAV_ESSID
	col = capplet_column_new(view,
      "title", _("Name (ESSID)"),
      "expand", TRUE,
      "resizable", TRUE,
      "clickable", FALSE,
      "sort-indicator", FALSE,
      "reorderable", FALSE,
      NULL);

    /* first signal strength icon cell */
    renderer = capplet_column_append_cell(col, gtk_cell_renderer_pixbuf_new(),
      FALSE, nwam_wifi_chooser_cell_cb, (gpointer)self, NULL);

    g_object_set_data(G_OBJECT(renderer), "nwam_wifi_fav_column_id", GUINT_TO_POINTER(WIFI_FAV_SIGNAL));

    /* second ESSID text cell */
    renderer = capplet_column_append_cell(col, gtk_cell_renderer_text_new(),
      TRUE, nwam_wifi_chooser_cell_cb, (gpointer)self, NULL);
    g_object_set(G_OBJECT(renderer), "editable", FALSE, NULL);
    g_object_set_data(G_OBJECT(renderer), "nwam_wifi_fav_column_id", GUINT_TO_POINTER(WIFI_FAV_ESSID));
    
    // Column:	WIFI_FAV_SPEED
	col = capplet_column_new(view,
      "title", _("Speed"),
      "expand", TRUE,
      "resizable", TRUE,
      "clickable", FALSE,
      "sort-indicator", FALSE,
      "reorderable", FALSE,
      NULL);

    renderer = capplet_column_append_cell(col, gtk_cell_renderer_text_new(),
      FALSE, nwam_wifi_chooser_cell_cb, (gpointer)self, NULL);
    g_object_set(G_OBJECT(renderer), "editable", FALSE, NULL);
    g_object_set_data(G_OBJECT(renderer), "nwam_wifi_fav_column_id", GUINT_TO_POINTER(WIFI_FAV_SPEED));

    // Column:	WIFI_FAV_SECURITY
	col = capplet_column_new(view,
      "title", _("Security"),
      "expand", TRUE,
      "resizable", TRUE,
      "clickable", FALSE,
      "sort-indicator", FALSE,
      "reorderable", FALSE,
      NULL);

    renderer = capplet_column_append_cell(col, gtk_cell_renderer_text_new(),
      FALSE, nwam_wifi_chooser_cell_cb, (gpointer)self, NULL);
    g_object_set(G_OBJECT(renderer), "editable", FALSE, NULL);
    g_object_set_data(G_OBJECT(renderer), "nwam_wifi_fav_column_id", GUINT_TO_POINTER(WIFI_FAV_SECURITY));

	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(view),
      GTK_SELECTION_SINGLE);

    g_signal_connect(gtk_tree_view_get_selection(view),
      "changed",
      G_CALLBACK(nwam_wifi_selection_changed),
      (gpointer)self);

}

static void
nwam_wireless_chooser_init(NwamWirelessChooser *self)
{
	self->prv = g_new0(NwamWirelessChooserPrivate, 1);
	
    self->prv->daemon = nwamui_daemon_get_instance();
    
	/* Iniialise pointers to important widgets */
    self->prv->wireless_chooser = 
        GTK_DIALOG(nwamui_util_ui_get_widget_from(NWAMUI_UI_FILE_WIRELESS, WIRELESS_CHOOSER_DIALOG));
    self->prv->wifi_tv  = 
        GTK_TREE_VIEW(nwamui_util_ui_get_widget_from(NWAMUI_UI_FILE_WIRELESS, WIRELESS_TABLE));
    self->prv->connect_wireless_connect_btn = 
        GTK_WIDGET(nwamui_util_ui_get_widget_from(NWAMUI_UI_FILE_WIRELESS, WIRELESS_CONNECT_WIRELESS_OK_BTN));
    self->prv->connect_wireless_refresh_btn =
        GTK_WIDGET(nwamui_util_ui_get_widget_from(NWAMUI_UI_FILE_WIRELESS, WIRELESS_CONNECT_WIRELESS_REFRESH_BTN));
    self->prv->add_to_preferred_cbox = 
        GTK_CHECK_BUTTON(nwamui_util_ui_get_widget_from(NWAMUI_UI_FILE_WIRELESS, WIRELESS_ADD_TO_PREFERRED_CBOX));

    nwam_compose_wifi_chooser_view ( self, self->prv->wifi_tv );

    self->prv->selected_wifi = NULL;

    gtk_widget_set_sensitive (GTK_WIDGET(self->prv->connect_wireless_connect_btn), FALSE);


    g_signal_connect(self->prv->connect_wireless_refresh_btn, "clicked", (GCallback)refresh_btn_clicked, (gpointer)self);
	g_signal_connect(self->prv->wireless_chooser, "response", (GCallback)response_cb, (gpointer)self);
	g_signal_connect(G_OBJECT(self), "notify", (GCallback)object_notify_cb, NULL);
    g_signal_connect(GTK_TOGGLE_BUTTON(self->prv->add_to_preferred_cbox), "toggled", (GCallback)presistant_cb, (gpointer)self);

    /* Populate WiFi conditions */
    g_object_get (nwamui_prof_get_instance_noref(),
      "action_on_no_fav_networks", &self->prv->action_if_no_fav,
      "join_wifi_not_in_fav", &self->prv->join_open,
      "join_any_fav_wifi", &self->prv->join_preferred,
      NULL);

    /* Update gui according to preferences. */
    g_signal_connect(nwamui_prof_get_instance_noref(),
      "notify::add-any-new-wifi-to-fav",
      (GCallback)add_any_new_wifi_to_fav,
      (gpointer)self);
    add_any_new_wifi_to_fav(G_OBJECT(nwamui_prof_get_instance_noref()), NULL, (gpointer)self);

    g_signal_connect(self->prv->daemon, "notify::status", G_CALLBACK(daemon_status_changed), (gpointer)self);
    g_signal_connect(self->prv->daemon, "notify::active-ncp", G_CALLBACK(daemon_active_ncp_changed), (gpointer) self);
    g_signal_connect(self->prv->daemon, "add",
                     G_CALLBACK(daemon_add_object), (gpointer)self);

    g_signal_connect(self->prv->daemon, "remove",
                     G_CALLBACK(daemon_remove_object), (gpointer)self);

    nwam_pref_refresh(NWAM_PREF_IFACE(self), NULL, TRUE);
}

static void
foreach_wifi_in_ncu_add_to_list_store(gpointer key, gpointer value, gpointer user_data)
{
    NwamuiWifiNet          *wifi  = value;
    GtkTreeModel           *model = (GtkTreeModel*)user_data;
    GtkTreeIter             iter;

    g_return_if_fail(NWAMUI_WIFI_NET(wifi));
    g_debug("add '%s' to list store", nwamui_object_get_name(NWAMUI_OBJECT(wifi)));

    if (nwamui_wifi_net_get_life_state(wifi) != NWAMUI_WIFI_LIFE_DEAD) {
        gtk_list_store_append(GTK_LIST_STORE(model), &iter);
        gtk_list_store_set(GTK_LIST_STORE (model), &iter, 0, wifi, -1);
    }
}

static void
populate_panel( NwamWirelessChooser* self, gboolean set_initial_state )
{
    g_assert( NWAM_IS_WIRELESS_CHOOSER(self));

    if (set_initial_state) {
        NwamuiObject *ncp   = nwamui_daemon_get_active_ncp(self->prv->daemon);
        GtkTreeModel *model = gtk_tree_view_get_model(self->prv->wifi_tv);

        gtk_widget_hide (GTK_WIDGET(self->prv->wifi_tv));

        nwamui_util_set_busy_cursor( GTK_WIDGET(self->prv->wireless_chooser) );

        gtk_widget_set_sensitive(GTK_WIDGET(self->prv->connect_wireless_refresh_btn), FALSE);

        gtk_list_store_clear (GTK_LIST_STORE(model));

        /* Init WiFis */
        if (ncp) {
            nwamui_ncp_foreach_ncu_foreach_wifi_info(NWAMUI_NCP(ncp), foreach_wifi_in_ncu_add_to_list_store, (gpointer)model);
            g_object_unref(ncp);
        }

        gtk_widget_show (GTK_WIDGET(self->prv->wifi_tv));

        nwamui_util_restore_default_cursor(GTK_WIDGET(self->prv->wireless_chooser));

        gtk_widget_set_sensitive(GTK_WIDGET(self->prv->connect_wireless_refresh_btn), TRUE);
    } else {
        /* Populate WiFis */
        nwamui_daemon_wifi_start_scan(self->prv->daemon);
    }
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

static GtkWindow* 
dialog_get_window(NwamPrefIFace *iface)
{
    NwamWirelessChooser *self = NWAM_WIRELESS_CHOOSER(iface);

    if ( self->prv->wireless_chooser ) {
        return ( GTK_WINDOW(self->prv->wireless_chooser) );
    }

    return(NULL);
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
cancel(NwamPrefIFace *iface, gpointer user_data)
{
}

static gboolean
apply(NwamPrefIFace *iface, gpointer user_data)
{
    NwamWirelessChooser *self = NWAM_WIRELESS_CHOOSER(iface);
    NwamWirelessChooserPrivate* prv = self->prv;
    g_assert( NWAM_IS_WIRELESS_CHOOSER(iface));

    if ( prv->selected_wifi != NULL ) {
        nwamui_wifi_net_connect(prv->selected_wifi,
          gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON( self->prv->add_to_preferred_cbox)));

        return( TRUE );
    }

    return(FALSE);
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
    nwamui_util_show_help(HELP_REF_WIRELESS_CHOOSER);
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

static void
join_wireless(NwamuiWifiNet *wifi, gboolean do_connect )
{
    static NwamWirelessDialog *wifi_dialog = NULL;

    NwamuiDaemon *daemon = nwamui_daemon_get_instance();
    NwamuiObject *ncp    = nwamui_daemon_get_active_ncp(daemon);
    NwamuiNcu    *ncu    = NULL;

    /* TODO popup key dialog */
    if (wifi_dialog == NULL) {
        wifi_dialog = nwam_wireless_dialog_get_instance();
    }

    /* ncu could be NULL due to daemon may not know the active llp */
    if ( wifi != NULL ) {
        ncu = nwamui_wifi_net_get_ncu(wifi);
    }

    if ( ncu == NULL || nwamui_ncu_get_ncu_type(ncu) != NWAMUI_NCU_TYPE_WIRELESS) {
        /* we need find a wireless interface */
        ncu = nwamui_ncp_get_first_wireless_ncu(NWAMUI_NCP(ncp));
    }

    g_object_unref(ncp);
    g_object_unref(daemon);

    if (ncu && nwamui_ncu_get_ncu_type(ncu) == NWAMUI_NCU_TYPE_WIRELESS) {
        nwam_wireless_dialog_set_ncu(wifi_dialog, ncu);
    } else {
        if (ncu) {
            g_object_unref(ncu);
        }
        return;
    }

    /* wifi may be NULL -- join a wireless */
    if (wifi) {
        g_debug("%s ## wifi 0x%p %s", __func__, wifi, nwamui_object_get_name(NWAMUI_OBJECT(wifi)));
    }

    nwam_pref_set_purpose(NWAM_PREF_IFACE(wifi_dialog), NWAMUI_DIALOG_PURPOSE_JOIN );
    nwam_wireless_dialog_set_wifi_net(wifi_dialog, wifi);
    nwam_wireless_dialog_set_do_connect(wifi_dialog, do_connect);

    (void)nwam_pref_dialog_run(NWAM_PREF_IFACE(wifi_dialog), NULL);

    g_object_unref(ncu);
}

/* Callbacks */
static void
refresh_btn_clicked( GtkButton *widget, gpointer user_data)
{
	NwamWirelessChooser *self = NWAM_WIRELESS_CHOOSER(user_data);
    NwamWirelessChooserPrivate* prv = self->prv;

    /* Populate WiFis */
    nwamui_daemon_wifi_start_scan(self->prv->daemon);
}

static void 
nwam_wifi_selection_changed(GtkTreeSelection *selection, gpointer data)
{
	NwamWirelessChooser *self = NWAM_WIRELESS_CHOOSER(data);
    NwamWirelessChooserPrivate* prv = self->prv;
	
	GtkTreeIter iter;
	
    if (prv->selected_wifi) {
        g_object_unref(prv->selected_wifi);
        prv->selected_wifi = NULL;
    }

	if (gtk_tree_selection_get_selected(selection, NULL, &iter)) {
		GtkTreeModel   *model = gtk_tree_view_get_model(GTK_TREE_VIEW(prv->wifi_tv));
		NwamuiWifiNet  *wifi_net;
		gchar          *txt = NULL;

		gtk_tree_model_get (model, &iter, 0, &wifi_net, -1);

		prv->selected_wifi = g_object_ref(G_OBJECT(wifi_net));
		
        gtk_widget_set_sensitive (GTK_WIDGET(self->prv->connect_wireless_connect_btn), TRUE);

		g_object_unref(wifi_net);
		return;
	}

    gtk_widget_set_sensitive (GTK_WIDGET(self->prv->connect_wireless_connect_btn), FALSE);
}

static void
response_cb(GtkWidget* widget, gint responseid, gpointer data)
{
	NwamWirelessChooser *self = NWAM_WIRELESS_CHOOSER(data);
    NwamWirelessChooserPrivate* prv = self->prv;
    gboolean             stop_emission = FALSE;

	switch (responseid) {
		case GTK_RESPONSE_NONE:
			g_debug("GTK_RESPONSE_NONE");
			break;
		case GTK_RESPONSE_DELETE_EVENT:
			g_debug("GTK_RESPONSE_DELETE_EVENT");
			break;
		case GTK_RESPONSE_APPLY: /* Join Unlisted Network */
			g_debug("GTK_RESPONSE_APPLY");
            join_wireless( NULL, TRUE );
            stop_emission = TRUE;
			break;
		case GTK_RESPONSE_OK:  /* Join selected network, if any */
			g_debug("GTK_RESPONSE_OK");
            stop_emission = !nwam_pref_apply(NWAM_PREF_IFACE(data), NULL);
			break;
		case GTK_RESPONSE_CANCEL:
			g_debug("GTK_RESPONSE_CANCEL");
            gtk_widget_hide( GTK_WIDGET(prv->wireless_chooser) );
            stop_emission = FALSE;
			break;
		case GTK_RESPONSE_HELP:
            nwam_pref_help (NWAM_PREF_IFACE(data), NULL);
            stop_emission = TRUE;
			break;
	}
    if ( stop_emission ) {
        g_signal_stop_emission_by_name(widget, "response" );
    }
    else {
		gtk_widget_hide( GTK_WIDGET(self->prv->wireless_chooser) );
    }
    nwamui_util_restore_default_cursor(GTK_WIDGET(self->prv->wireless_chooser) );
}

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
	g_debug("NwamWirelessChooser: notify %s changed", arg1->name);
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
    NwamuiDaemon        *daemon;
    NwamuiObject        *ncp;
    gboolean             has_many_wifi;

    daemon = nwamui_daemon_get_instance();
    ncp = nwamui_daemon_get_active_ncp( daemon );
    has_many_wifi = nwamui_ncp_get_wireless_link_num(NWAMUI_NCP(ncp)) > 1;
    g_object_unref(G_OBJECT(ncp));
    g_object_unref(G_OBJECT(daemon));

    gtk_tree_model_get(GTK_TREE_MODEL(model), iter, 0, &wifi_info, -1);
    
    g_return_if_fail(NWAMUI_IS_WIFI_NET(wifi_info));
    
    switch (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (renderer), "nwam_wifi_fav_column_id"))) {
    case WIFI_FAV_ESSID:
    {   
        gchar*  essid = nwamui_wifi_net_get_display_string(wifi_info, has_many_wifi );

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
        status_icon = nwamui_util_get_wireless_strength_icon_with_size(signal, NWAMUI_WIRELESS_ICON_TYPE_BARS, 16);
        g_object_set (G_OBJECT(renderer),
          "pixbuf", status_icon,
          NULL);
        g_object_unref(G_OBJECT(status_icon));
    }
    break;
    default:
        g_assert_not_reached ();
    }

    g_object_unref(wifi_info);
}

static void
presistant_cb(GtkToggleButton* widget, gpointer data)
{
    /* Update preference. */
    g_object_set(nwamui_prof_get_instance_noref(),
      "add_any_new_wifi_to_fav", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)),
      NULL);
}

static void
daemon_status_changed(NwamuiDaemon *daemon, GParamSpec *arg1, gpointer user_data)
{
    NwamWirelessChooser        *self  = NWAM_WIRELESS_CHOOSER(user_data);
    NwamWirelessChooserPrivate *prv   = self->prv;

    switch(nwamui_daemon_get_status(daemon)) {
    case NWAMUI_DAEMON_STATUS_ALL_OK:
        break;
    case NWAMUI_DAEMON_STATUS_ERROR:
        daemon_active_ncp_changed(self->prv->daemon, NULL, (gpointer)self);
        break;
    default:
        break;
    }
}

static void
daemon_active_ncp_changed(NwamuiDaemon *daemon, GParamSpec *arg1, gpointer data)
{
    NwamWirelessChooser        *self = NWAM_WIRELESS_CHOOSER(data);

    /* Re create wlan list */
    nwam_pref_refresh(NWAM_PREF_IFACE(self), NULL, TRUE);
}

static void
daemon_add_object(NwamuiDaemon *daemon, NwamuiObject* object, gpointer user_data)
{
    NwamWirelessChooser        *self  = NWAM_WIRELESS_CHOOSER(user_data);
    NwamWirelessChooserPrivate *prv   = self->prv;
    GtkTreeModel               *model = gtk_tree_view_get_model(prv->wifi_tv);
    GtkTreeIter                 iter;

    if (NWAMUI_IS_WIFI_NET(object) && !NWAMUI_IS_KNOWN_WLAN(object)) {
        if (!capplet_model_find_object(model, G_OBJECT(object), &iter)) {
            gtk_list_store_prepend(GTK_LIST_STORE(model), &iter);
            gtk_list_store_set(GTK_LIST_STORE(model), &iter, 0, object, -1);
        }
    }
}

static void
daemon_remove_object(NwamuiDaemon *daemon, NwamuiObject* object, gpointer user_data)
{
    NwamWirelessChooser        *self  = NWAM_WIRELESS_CHOOSER(user_data);
    NwamWirelessChooserPrivate *prv   = self->prv;
    GtkTreeModel               *model = gtk_tree_view_get_model(prv->wifi_tv);
    GtkTreeIter                 iter;

    if (NWAMUI_IS_WIFI_NET(object) && !NWAMUI_IS_KNOWN_WLAN(object)) {
        if (capplet_model_find_object(model, G_OBJECT(object), &iter)) {
            gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
        }
    }
}

static void
add_any_new_wifi_to_fav(GObject *gobject, GParamSpec *arg1, gpointer data)
{
    NwamWirelessChooser* self = NWAM_WIRELESS_CHOOSER(data);
    gboolean add_to_fav;

    g_signal_handlers_block_by_func(gobject, (gpointer)add_any_new_wifi_to_fav, data);

    g_object_get(gobject, "add_any_new_wifi_to_fav", &add_to_fav, NULL);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(self->prv->add_to_preferred_cbox), add_to_fav);

    g_signal_handlers_unblock_by_func(gobject, (gpointer)add_any_new_wifi_to_fav, data);
}
