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
 * Created on May 9, 2007, 11:01 AM
 *
 */

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n.h>

#include "libnwamui.h"
#include "nwam_pref_dialog.h"
#include "nwam_env_pref_dialog.h"
#include "nwam_location_dialog.h"
#include "nwam_vpn_pref_dialog.h"
#include "nwam_conn_stat_panel.h"
#include "nwam_net_conf_panel.h"
#include "nwam_pref_iface.h"
#include "capplet-utils.h"

/* Names of Widgets in Glade file */
#define CONN_STATUS_TREEVIEW             "connection_status_table"
#define CONN_STATUS_PROFILE_LABEL        "current_profile_lbl"
#define CONN_STATUS_ENV_LABEL            "current_env_lbl"
#define CONN_STATUS_ENV_BUTTON           "environments_btn"
#define CONN_STATUS_VPN_LABEL            "current_vpn_lbl"
#define CONN_STATUS_VPN_BUTTON           "vpn_btn"
#define CONN_STATUS_REPAIR_BUTTON        "repair_connection_btn"

struct _NwamConnStatusPanelPrivate {
	/* Widget Pointers */
	GtkTreeView*	conn_status_treeview;
	GtkLabel*	current_profile_lbl;
	GtkButton*	env_btn;
	GtkLabel*	current_env_lbl;
	GtkButton*	vpn_btn;
	GtkLabel*	current_vpn_lbl;

	/* Other Data */
    NwamCappletDialog*  pref_dialog;
    NwamuiNcp*          ncp;
    NwamuiDaemon*       daemon;
    NwamLocationDialog* location_dialog;
	NwamVPNPrefDialog*  vpn_dialog;
};

enum {
	CONNVIEW_ICON=0,
	CONNVIEW_INFO,
    CONNVIEW_STATUS
};

#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
	NWAM_TYPE_CONN_STATUS_PANEL, NwamConnStatusPanelPrivate)) 

static void nwam_pref_init (gpointer g_iface, gpointer iface_data);
static gboolean refresh(NwamPrefIFace *iface, gpointer user_data, gboolean force);
static gboolean apply(NwamPrefIFace *iface, gpointer user_data);
static gboolean help(NwamPrefIFace *iface, gpointer user_data);

static void nwam_conn_status_panel_finalize(NwamConnStatusPanel *self);
static void nwam_conn_status_update_status_cell_cb (GtkTreeViewColumn *col,
					     GtkCellRenderer   *renderer,
					     GtkTreeModel      *model,
					     GtkTreeIter       *iter,
					     gpointer           data);

/* Callbacks */
static void object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);
static void nwam_conn_status_conn_view_row_activated_cb (GtkTreeView *tree_view,
					GtkTreePath *path,
					GtkTreeViewColumn *column,
					gpointer data);
static void repair_clicked_cb( GtkButton *button, gpointer data );
static void env_clicked_cb( GtkButton *button, gpointer data );
static void vpn_clicked_cb( GtkButton *button, gpointer data );
static void on_nwam_env_notify_cb(GObject *gobject, GParamSpec *arg1, gpointer data);
static void on_nwam_enm_notify_cb(GObject *gobject, GParamSpec *arg1, gpointer data);

G_DEFINE_TYPE_EXTENDED (NwamConnStatusPanel,
                        nwam_conn_status_panel,
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
    iface->dialog_run = NULL;
}

static void
nwam_conn_status_panel_class_init(NwamConnStatusPanelClass *klass)
{
	/* Pointer to GObject Part of Class */
	GObjectClass *gobject_class = (GObjectClass*) klass;
	
	/* Override Some Function Pointers */
	gobject_class->finalize = (void (*)(GObject*)) nwam_conn_status_panel_finalize;
	g_type_class_add_private (klass, sizeof (NwamConnStatusPanelPrivate));
}

static void
nwam_compose_tree_view (NwamConnStatusPanel *self)
{
	GtkTreeViewColumn *col;
	GtkCellRenderer *cell;
	GtkTreeView *view = self->prv->conn_status_treeview;

	g_object_set (view,
      "headers-clickable", TRUE,
      "headers-visible", FALSE,
      "rules-hint", TRUE,
      "reorderable", FALSE,
      "enable-search", TRUE,
      "show-expanders", TRUE,
      NULL);

    {
        col = gtk_tree_view_column_new();
        gtk_tree_view_append_column (view, col);

        g_object_set(col,
          "title", _("Connection Icon"),
          "resizable", TRUE,
          "clickable", TRUE,
          "sort-indicator", TRUE,
          "reorderable", TRUE,
          NULL);
        gtk_tree_view_column_set_sort_column_id (col, CONNVIEW_ICON);	

        /* Add first "type" icon cell */
        cell = gtk_cell_renderer_pixbuf_new();
        gtk_tree_view_column_pack_start(col, cell, TRUE);

        gtk_tree_view_column_set_cell_data_func (col,
          cell,
          nwam_conn_status_update_status_cell_cb,
          (gpointer) 0,
          NULL);
        /* Add 2nd wireless strength icon */
        cell = gtk_cell_renderer_pixbuf_new();
        gtk_tree_view_column_pack_end(col, cell, FALSE);

        gtk_tree_view_column_set_cell_data_func (col,
          cell,
          nwam_conn_status_update_status_cell_cb,
          (gpointer) 1, /* Important to know this is wireless cell in 1st col */
          NULL);
    } /* column icon */
    {
        col = gtk_tree_view_column_new();
        gtk_tree_view_append_column (view, col);

        g_object_set(col,
          "title", _("Connection Information"),
          "expand", TRUE,
          "resizable", TRUE,
          "clickable", TRUE,
          "sort-indicator", TRUE,
          "reorderable", TRUE,
          NULL);
        gtk_tree_view_column_set_sort_column_id (col, CONNVIEW_INFO);	

        /* First cell */
        cell = gtk_cell_renderer_text_new();
        gtk_tree_view_column_pack_start(col, cell, TRUE);

        gtk_tree_view_column_set_cell_data_func (col,
          cell,
          nwam_conn_status_update_status_cell_cb,
          (gpointer) 0,
          NULL);
    } /* column info */
        
    {
        col = gtk_tree_view_column_new();
        gtk_tree_view_append_column (view, col);

        g_object_set(col,
          "title", _("Connection Status"),
          "resizable", TRUE,
          "clickable", TRUE,
          "sort-indicator", TRUE,
          "reorderable", TRUE,
          NULL);
        gtk_tree_view_column_set_sort_column_id (col, CONNVIEW_STATUS);	

        /* First cell */
        cell = gtk_cell_renderer_text_new();
        gtk_tree_view_column_pack_start(col, cell, TRUE);

        g_object_set (cell,
          "xalign", 1.0,
          "weight", PANGO_WEIGHT_BOLD,
          NULL );

        gtk_tree_view_column_set_cell_data_func (col,
          cell,
          nwam_conn_status_update_status_cell_cb,
          (gpointer) 0,
          NULL);
    } /* column status */
}

static void
nwam_conn_status_panel_init(NwamConnStatusPanel *self)
{
	NwamConnStatusPanelPrivate *prv = GET_PRIVATE(self);
    GtkButton *btn;

	self->prv = prv;

	/* Iniialise pointers to important widgets */
	prv->conn_status_treeview = GTK_TREE_VIEW(nwamui_util_glade_get_widget(CONN_STATUS_TREEVIEW));
	prv->current_profile_lbl = GTK_LABEL(nwamui_util_glade_get_widget(CONN_STATUS_PROFILE_LABEL));
	prv->current_env_lbl = GTK_LABEL(nwamui_util_glade_get_widget(CONN_STATUS_ENV_LABEL));
	prv->env_btn = GTK_BUTTON(nwamui_util_glade_get_widget(CONN_STATUS_ENV_BUTTON));
	prv->current_vpn_lbl = GTK_LABEL(nwamui_util_glade_get_widget(CONN_STATUS_VPN_LABEL));
	prv->vpn_btn = GTK_BUTTON(nwamui_util_glade_get_widget(CONN_STATUS_VPN_BUTTON));
    btn = GTK_BUTTON(nwamui_util_glade_get_widget(CONN_STATUS_REPAIR_BUTTON));

    prv->daemon = nwamui_daemon_get_instance();
        
	g_signal_connect(G_OBJECT(self), "notify", (GCallback)object_notify_cb, NULL);
	g_signal_connect(GTK_BUTTON(prv->env_btn), "clicked", (GCallback)env_clicked_cb, (gpointer)self);
	g_signal_connect(GTK_BUTTON(prv->vpn_btn), "clicked", (GCallback)vpn_clicked_cb, (gpointer)self);
	g_signal_connect(GTK_BUTTON(btn), "clicked", (GCallback)repair_clicked_cb, (gpointer)self);
    g_signal_connect (prv->daemon, "notify::active_env", G_CALLBACK(on_nwam_env_notify_cb), (gpointer)self);

    nwam_compose_tree_view(self);

	g_signal_connect(GTK_TREE_VIEW(prv->conn_status_treeview),
                     "row-activated",
                     (GCallback)nwam_conn_status_conn_view_row_activated_cb,
                     (gpointer)self);

    /* FIXME should connect daemon update enm list signal to get the latest enm list */
    GList*                  enm_elem;
    NwamuiEnm*              enm;
    for ( enm_elem = g_list_first(nwamui_daemon_get_enm_list(NWAMUI_DAEMON(self->prv->daemon)));
          enm_elem != NULL;
          enm_elem = g_list_next( enm_elem ) ) {
        enm = NWAMUI_ENM(enm_elem->data);
		g_signal_connect (enm_elem->data, "notify::active", G_CALLBACK(on_nwam_enm_notify_cb), (gpointer)self);
    }

    /* Initially refresh self */
    {
        NwamuiNcp *ncp = nwamui_daemon_get_active_ncp(prv->daemon);
        if (ncp) {
            nwam_pref_refresh(NWAM_PREF_IFACE(self), ncp, TRUE);
            g_object_unref(ncp);
        }
    }
}

/**
 * nwam_conn_status_panel_new:
 * @returns: a new #NwamConnStatusPanel.
 *
 * Creates a new #NwamConnStatusPanel with an empty ncu
 **/
NwamConnStatusPanel*
nwam_conn_status_panel_new(NwamCappletDialog *pref_dialog)
{
	NwamConnStatusPanel *self =  NWAM_CONN_STATUS_PANEL(g_object_new(NWAM_TYPE_CONN_STATUS_PANEL, NULL));
        
    /* FIXME - Use GOBJECT Properties */
    self->prv->pref_dialog = g_object_ref( G_OBJECT( pref_dialog ));

    return( self );
}

static void
nwam_conn_status_panel_finalize(NwamConnStatusPanel *self)
{
	G_OBJECT_CLASS(nwam_conn_status_panel_parent_class)->finalize(G_OBJECT(self));
}

/**
 * refresh:
 *
 * Refresh function for the NwamPrefIface Interface.
 **/
static gboolean
refresh(NwamPrefIFace *iface, gpointer user_data, gboolean force)
{
    GList*                  enm_elem;
    gchar*                  text = NULL;
    NwamConnStatusPanel*    self = NWAM_CONN_STATUS_PANEL( iface );

    g_assert(NWAM_IS_CONN_STATUS_PANEL(self));
    
	/* data could be null or ncp */
    if (user_data != NULL) {
        NwamuiNcp *ncp = NWAMUI_NCP(user_data);
        GtkTreeModel *model;
        model = GTK_TREE_MODEL(nwamui_ncp_get_ncu_list_store(ncp));
        gtk_widget_hide(GTK_WIDGET(self->prv->conn_status_treeview));
        gtk_tree_view_set_model(self->prv->conn_status_treeview, model);
        gtk_widget_show(GTK_WIDGET(self->prv->conn_status_treeview));
        g_object_unref(model);

        text = nwamui_daemon_get_active_ncp_name(NWAMUI_DAEMON(self->prv->daemon));
        if (text) {
            gtk_label_set_text(self->prv->current_profile_lbl, text);
            g_free (text);
        } else {
            gtk_label_set_text(self->prv->current_profile_lbl, _("Unknow profile"));
        }
    }

    text = nwamui_daemon_get_active_env_name(NWAMUI_DAEMON(self->prv->daemon));
    if (text) {
        gtk_label_set_text(self->prv->current_env_lbl, text);
        g_free (text);
    } else {
        gtk_label_set_text(self->prv->current_env_lbl, _("Unknow env"));
    }
    on_nwam_enm_notify_cb (NULL, NULL, (gpointer)self);
    return(TRUE);
}

static gboolean
apply(NwamPrefIFace *iface, gpointer user_data)
{
}

/**
 * help:
 **/
static gboolean
help(NwamPrefIFace *iface, gpointer user_data)
{
    g_debug ("NwamConnStatusPanel: Help");
    nwamui_util_show_help ("");
}

/* Callbacks */
static void
nwam_conn_status_update_status_cell_cb (GtkTreeViewColumn *col,
				 GtkCellRenderer   *renderer,
				 GtkTreeModel      *model,
				 GtkTreeIter       *iter,
				 gpointer           data)
{
    gint                            cell_num = (gint)data;  /* Number of cell in column */
	gchar                          *stockid = NULL;
    NwamuiNcu*                      ncu = NULL;
    nwamui_ncu_type_t               ncu_type;
    gchar*                          ncu_text = NULL;
    gchar*                          ncu_markup = NULL;
    gboolean                        ncu_status;
    gboolean                        ncu_is_dhcp;
    gchar*                          ncu_ipv4_addr = NULL;
    GdkPixbuf                      *status_icon;
    gchar*                          info_string = NULL;
    nwamui_wifi_signal_strength_t   strength = NWAMUI_WIFI_STRENGTH_NONE;
    gint                            icon_size, dummy;

    if ( !gtk_icon_size_lookup(GTK_ICON_SIZE_LARGE_TOOLBAR, &icon_size, &dummy) ) {
        icon_size=24;
    }

	gtk_tree_model_get(model, iter, 0, &ncu, -1);

    ncu_type = nwamui_ncu_get_ncu_type(ncu);

    /* Get REAL interface status rather than what NWAM says */
    switch( nwamui_ncu_get_connection_state( ncu ) ) {
        case NWAMUI_STATE_CONNECTED:
        case NWAMUI_STATE_CONNECTED_ESSID: 
            ncu_status = TRUE;
            break;
        default:
            ncu_status = FALSE;
            break;
    }
        
	switch (gtk_tree_view_column_get_sort_column_id (col)) {
	case CONNVIEW_ICON:
        if( cell_num == 0 && ncu_type != NWAMUI_NCU_TYPE_WIRELESS ) {
            status_icon = nwamui_util_get_network_status_icon(ncu_type, strength, 
                    ncu_status?NWAMUI_ENV_STATUS_CONNECTED : NWAMUI_ENV_STATUS_ERROR,
                    icon_size);

            g_object_set (G_OBJECT(renderer),
              "pixbuf", status_icon,
              NULL);
        }
        else if( cell_num == 0 && ncu_type == NWAMUI_NCU_TYPE_WIRELESS ) {
            status_icon = nwamui_util_get_network_status_icon(ncu_type, strength, 
                    ncu_status?NWAMUI_ENV_STATUS_CONNECTED : NWAMUI_ENV_STATUS_ERROR,
                    icon_size);

            g_object_set (G_OBJECT(renderer),
              "pixbuf", status_icon,
              NULL);
        }
        else {
            g_object_set (G_OBJECT(renderer),
              "pixbuf", NULL,
              NULL);

        }
                    
		break;
                
	case CONNVIEW_INFO: 
                ncu_text = nwamui_ncu_get_display_name(ncu);
                ncu_is_dhcp = nwamui_ncu_get_ipv4_auto_conf(ncu);
                ncu_ipv4_addr = nwamui_ncu_get_ipv4_address(ncu);
                info_string = nwamui_ncu_get_connection_state_detail_string( ncu );

                ncu_markup= g_strdup_printf(_("<b>%s</b>\n<small>%s</small>"), ncu_text, info_string );
                g_free (info_string);
        
		g_object_set (G_OBJECT(renderer),
			"markup", ncu_markup,
			NULL);

                if ( ncu_ipv4_addr != NULL )
                    g_free( ncu_ipv4_addr );
                
                g_free( ncu_markup );
                g_free( ncu_text );
		break;
                
    case CONNVIEW_STATUS:
        ncu_text = nwamui_ncu_get_connection_state_string( ncu );
		g_object_set (G_OBJECT(renderer),
			"text", ncu_text,
			NULL);
        g_free(ncu_text);
        /*
		g_object_set (G_OBJECT(renderer),
			"text", ncu_status?_("Enabled"):_("Disabled"),
			NULL);
            */
		break;
	default:
		g_assert_not_reached ();
	}

        if ( ncu != NULL ) 
            g_object_unref(G_OBJECT(ncu));

}

/*
 * Double-clicking a connection switches the status view to that connection's
 * properties view (p5)
 */
static void
nwam_conn_status_conn_view_row_activated_cb (GtkTreeView *tree_view,
			    GtkTreePath *path,
			    GtkTreeViewColumn *column,
			    gpointer data)
{
	NwamConnStatusPanel* self = NWAM_CONN_STATUS_PANEL(data);
    GtkTreeIter iter;
    GtkTreeModel *model = gtk_tree_view_get_model(tree_view);

    if (gtk_tree_model_get_iter (model, &iter, path))
    {
        gpointer    connection;
        NwamuiNcu*  ncu;
            
        gtk_tree_model_get(model, &iter, 0, &connection, -1);

        ncu  = NWAMUI_NCU( connection );
            
        nwam_capplet_dialog_select_ncu(self->prv->pref_dialog, ncu );

    }
}

static void
repair_clicked_cb( GtkButton *button, gpointer data )
{
	NwamConnStatusPanel* self = NWAM_CONN_STATUS_PANEL(data);
    gint                 response;
	GtkTreeIter iter;
    GtkTreeSelection *selection;
    GtkTreeModel *model;

    selection = gtk_tree_view_get_selection (self->prv->conn_status_treeview);
    model = gtk_tree_view_get_model (self->prv->conn_status_treeview);
	if (gtk_tree_selection_get_selected(selection,
          NULL, &iter)) {
        gpointer    connection;
        NwamuiNcu*  ncu;

        gtk_tree_model_get(model, &iter, 0, &connection, -1);
        ncu  = NWAMUI_NCU( connection );
        
        /* TODO : Repair/renew the NCU */
        nwamui_ncu_set_active (ncu, FALSE);
        nwamui_ncu_set_active (ncu, TRUE); 
    }
}

static void
env_clicked_cb( GtkButton *button, gpointer data )
{
	NwamConnStatusPanel* self = NWAM_CONN_STATUS_PANEL(data);
    gint                 response;

    if ( self->prv->location_dialog == NULL ) {
        self->prv->location_dialog = nwam_location_dialog_new();
    }
    if ( self->prv->location_dialog != NULL ) {
        nwam_pref_refresh(NWAM_PREF_IFACE(self->prv->location_dialog), NULL, TRUE);
        response = capplet_dialog_run(NWAM_PREF_IFACE(self->prv->location_dialog), GTK_WIDGET(button) );
        
        g_debug("location_dialog returned %d", response );
    }
}

static void
vpn_clicked_cb( GtkButton *button, gpointer data )
{
	NwamConnStatusPanel* self = NWAM_CONN_STATUS_PANEL(data);
	gint response;

	if ( self->prv->vpn_dialog == NULL ) {
            self->prv->vpn_dialog = nwam_vpn_pref_dialog_new();
    }

	response = capplet_dialog_run(NWAM_PREF_IFACE(self->prv->vpn_dialog), button);
	
	g_debug("VPN dialog returned %d", response);
}

static void
object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
	g_debug("NwamConnStatusPanel: notify %s changed\n", arg1->name);
}

static void
on_nwam_env_notify_cb(GObject *gobject, GParamSpec *arg1, gpointer data)
{
	NwamConnStatusPanelPrivate *prv = GET_PRIVATE(data);
    gchar *text;

    if ( g_ascii_strcasecmp(arg1->name, "active_env") == 0 ) {
        if (text) {
            text = nwamui_daemon_get_active_env_name(NWAMUI_DAEMON(prv->daemon));
            gtk_label_set_text(prv->current_env_lbl, text);
            g_free (text);
        } else {
            gtk_label_set_text(prv->current_env_lbl, _("Unknow env"));
        }
    }
}

static void
on_nwam_enm_notify_cb(GObject *gobject, GParamSpec *arg1, gpointer data)
{
	NwamConnStatusPanelPrivate *prv = GET_PRIVATE(data);
    GList*                  enm_elem;
    NwamuiEnm*              enm;
    int                     enm_active_count = 0;
    gchar* enm_str = NULL;

    for ( enm_elem = g_list_first(nwamui_daemon_get_enm_list(NWAMUI_DAEMON(prv->daemon)));
          enm_elem != NULL;
          enm_elem = g_list_next( enm_elem ) ) {
            
        enm = NWAMUI_ENM(enm_elem->data);
            
        if  ( nwamui_enm_get_active(enm) ) {
            if ( enm_str == NULL ) {
                gchar *name;
                name = nwamui_enm_get_name(enm);
                enm_str = g_strdup_printf("%s Active", name);
                g_free (name);
            }
            enm_active_count++;
        }
    }
        
    if ( enm_active_count == 0 ) {
        enm_str = g_strdup(_("None Active"));
    }
    else if (enm_active_count > 1 ) {
        if ( enm_str != NULL )
            g_free( enm_str );  
            
        enm_str = g_strdup_printf("%s Active", enm_active_count);
    }
        
    gtk_label_set_text(prv->current_vpn_lbl, enm_str );
        
    g_free( enm_str );
}
