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
 * Created on May 9, 2007, 11:01 AM
 *
 */

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n.h>

#include "libnwamui.h"
#include "nwam_pref_dialog.h"
#include "nwam_env_pref_dialog.h"
#include "nwam_vpn_pref_dialog.h"
#include "nwam_conn_stat_panel.h"
#include "nwam_net_conf_panel.h"
#include "nwam_pref_iface.h"

/* Names of Widgets in Glade file */
#define CONN_STATUS_TREEVIEW             "connection_status_table"
#define CONN_STATUS_ENV_NAME_LABEL       "current_env_lbl"
#define CONN_STATUS_ENV_BUTTON           "environments_btn"
#define CONN_STATUS_VPN_NAME_LABEL       "current_vpn_lbl"
#define CONN_STATUS_VPN_BUTTON           "vpn_btn"
#define CONN_STATUS_REPAIR_BUTTON        "repair_connection_btn"

static void nwam_pref_init (gpointer g_iface, gpointer iface_data);

struct _NwamConnStatusPanelPrivate {
	/* Widget Pointers */
	GtkTreeView*	conn_status_treeview;
	GtkButton*	env_btn;
	GtkLabel*	env_name_lbl;
	GtkButton*	vpn_btn;
	GtkLabel*	vpn_name_lbl;
    GtkTreeModel*   model;

	/* Other Data */
    NwamCappletDialog*  pref_dialog;
    NwamuiNcp*          ncp;
    NwamuiDaemon*       daemon;
    NwamEnvPrefDialog*  env_dialog;
	NwamVPNPrefDialog*  vpn_dialog;
};

enum {
	CONNVIEW_ICON=0,
	CONNVIEW_INFO,
    CONNVIEW_STATUS
};

#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
	NWAM_TYPE_CONN_STATUS_PANEL, NwamConnStatusPanelPrivate)) 

static void nwam_conn_status_panel_finalize(NwamConnStatusPanel *self);
static void nwam_conn_status_update_status_cell_cb (GtkTreeViewColumn *col,
					     GtkCellRenderer   *renderer,
					     GtkTreeModel      *model,
					     GtkTreeIter       *iter,
					     gpointer           data);

static gboolean  nwam_conn_status_panel_refresh(NwamPrefIFace *pref_iface, gpointer data );

/* Callbacks */
static void object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);
static void nwam_conn_status_conn_view_row_activated_cb (GtkTreeView *tree_view,
					GtkTreePath *path,
					GtkTreeViewColumn *column,
					gpointer data);
static void repair_clicked_cb( GtkButton *button, gpointer data );
static void env_clicked_cb( GtkButton *button, gpointer data );
static void vpn_clicked_cb( GtkButton *button, gpointer data );
static gboolean help (NwamConnStatusPanel *self, gpointer data);
static gboolean apply (NwamPrefIFace *self, gpointer data);
static void on_nwam_env_notify_cb(GObject *gobject, GParamSpec *arg1, gpointer data);
static void on_nwam_enm_notify_cb(GObject *gobject, GParamSpec *arg1, gpointer data);
static void on_nwam_ncu_notify_cb(GObject *gobject, GParamSpec *arg1, gpointer data);

G_DEFINE_TYPE_EXTENDED (NwamConnStatusPanel,
                        nwam_conn_status_panel,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (NWAM_TYPE_PREF_IFACE, nwam_pref_init))

static void
nwam_pref_init (gpointer g_iface, gpointer iface_data)
{
	NwamPrefInterface *iface = (NwamPrefInterface *)g_iface;
	iface->refresh = nwam_conn_status_panel_refresh;
	iface->apply = NULL;
	iface->help = help;
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

static GtkTreeModel*
nwam_compose_tree_view (NwamConnStatusPanel *self)
{
	GtkTreeViewColumn *col;
	GtkCellRenderer *cell;
	GtkTreeView *view = self->prv->conn_status_treeview;
	GtkTreeModel *model = NULL;

    model = GTK_TREE_MODEL(nwamui_ncp_get_ncu_list_store(self->prv->ncp));

    g_assert( GTK_IS_LIST_STORE( model ) );
    gtk_tree_view_set_model(GTK_TREE_VIEW(view), model);
        
	g_object_set (G_OBJECT(view),
		      "headers-clickable", FALSE,
		      NULL);
	
	// column icon
        /* Add first "type" icon cell */
	col = gtk_tree_view_column_new();
	cell = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_set_title(col, _("Connection Icon"));
	gtk_tree_view_column_pack_start(col, cell, FALSE);
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
	gtk_tree_view_column_set_sort_column_id (col, CONNVIEW_ICON);	
	g_object_set (col,
		      "resizable", TRUE,
		      "clickable", TRUE,
		      "sort-indicator", TRUE,
		      "reorderable", TRUE,
		      NULL);
	gtk_tree_view_append_column (view, col);

	// column info
	col = gtk_tree_view_column_new();
	cell = gtk_cell_renderer_text_new();
	gtk_tree_view_column_set_title(col, _("Connection Information"));
	gtk_tree_view_column_pack_start(col, cell, TRUE);
	gtk_tree_view_column_set_cell_data_func (col,
						 cell,
						 nwam_conn_status_update_status_cell_cb,
						 (gpointer) 0,
						 NULL);
	gtk_tree_view_column_set_sort_column_id (col, CONNVIEW_INFO);	
	g_object_set (col,
		      "expand", TRUE,
		      "resizable", TRUE,
		      "clickable", TRUE,
		      "sort-indicator", TRUE,
		      "reorderable", TRUE,
		      NULL);
	gtk_tree_view_append_column (view, col);
        
    // column status
	col = gtk_tree_view_column_new();
	cell = gtk_cell_renderer_text_new();
	gtk_tree_view_column_set_title(col, _("Connection Status"));
	gtk_tree_view_column_pack_end(col, cell, FALSE);
	gtk_tree_view_column_set_cell_data_func (col,
						 cell,
						 nwam_conn_status_update_status_cell_cb,
						 (gpointer) 0,
						 NULL);
	g_object_set (cell,
      "xalign", 1.0,
      "weight", PANGO_WEIGHT_BOLD,
      NULL );
	gtk_tree_view_column_set_sort_column_id (col, CONNVIEW_STATUS);	
	g_object_set (col,
		      "resizable", TRUE,
		      "clickable", TRUE,
		      "sort-indicator", TRUE,
		      "reorderable", TRUE,
		      NULL);
	gtk_tree_view_append_column (view, col);
        
    g_object_unref(model); /* Unref here since combobox will have ref-ed it */

    return( model );
}

static void
nwam_conn_status_panel_set_ncp(NwamConnStatusPanel *self, NwamuiNcp* ncp )
{
	NwamConnStatusPanelPrivate *prv = GET_PRIVATE(self);

    prv->ncp = NWAMUI_NCP(g_object_ref(ncp));

    prv->model = nwam_compose_tree_view (self);

    nwam_pref_refresh(NWAM_PREF_IFACE(self), NULL);
}

static void
nwam_conn_status_panel_init(NwamConnStatusPanel *self)
{
	NwamConnStatusPanelPrivate *prv = GET_PRIVATE(self);
    GtkButton *btn;

	self->prv = prv;

	/* Iniialise pointers to important widgets */
	prv->conn_status_treeview = GTK_TREE_VIEW(nwamui_util_glade_get_widget(CONN_STATUS_TREEVIEW));
	prv->env_name_lbl = GTK_LABEL(nwamui_util_glade_get_widget(CONN_STATUS_ENV_NAME_LABEL));
	prv->env_btn = GTK_BUTTON(nwamui_util_glade_get_widget(CONN_STATUS_ENV_BUTTON));
	prv->vpn_name_lbl = GTK_LABEL(nwamui_util_glade_get_widget(CONN_STATUS_VPN_NAME_LABEL));
	prv->vpn_btn = GTK_BUTTON(nwamui_util_glade_get_widget(CONN_STATUS_VPN_BUTTON));
    btn = GTK_BUTTON(nwamui_util_glade_get_widget(CONN_STATUS_REPAIR_BUTTON));

    prv->daemon = nwamui_daemon_get_instance();
    prv->pref_dialog = NULL;
    prv->ncp = NULL;
    prv->env_dialog = NULL;
    prv->model = NULL;
        
	g_signal_connect(G_OBJECT(self), "notify", (GCallback)object_notify_cb, NULL);
	g_signal_connect(GTK_BUTTON(prv->env_btn), "clicked", (GCallback)env_clicked_cb, (gpointer)self);
	g_signal_connect(GTK_BUTTON(prv->vpn_btn), "clicked", (GCallback)vpn_clicked_cb, (gpointer)self);
	g_signal_connect(GTK_BUTTON(btn), "clicked", (GCallback)repair_clicked_cb, (gpointer)self);
    g_signal_connect (prv->daemon, "notify::active_env", G_CALLBACK(on_nwam_env_notify_cb), (gpointer)self);

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
}

/**
 * nwam_conn_status_panel_new:
 * @returns: a new #NwamConnStatusPanel.
 *
 * Creates a new #NwamConnStatusPanel with an empty ncu
 **/
NwamConnStatusPanel*
nwam_conn_status_panel_new(NwamCappletDialog *pref_dialog, NwamuiNcp* ncp)
{
	NwamConnStatusPanel *self =  NWAM_CONN_STATUS_PANEL(g_object_new(NWAM_TYPE_CONN_STATUS_PANEL, NULL));
        
    /* FIXME - Use GOBJECT Properties */
    self->prv->pref_dialog = g_object_ref( G_OBJECT( pref_dialog ));
    
    nwam_conn_status_panel_set_ncp(self, ncp );

    return( self );
}

/**
 * nwam_conn_status_panel_refresh:
 *
 * Refresh function for the NwamPrefIface Interface.
 **/
static gboolean
nwam_conn_status_panel_refresh(NwamPrefIFace *pref_iface, gpointer data)
{
        GList*                  enm_elem;
        gchar*                  text = NULL;
        NwamConnStatusPanel*    self = NWAM_CONN_STATUS_PANEL( pref_iface );

        g_assert(NWAM_IS_CONN_STATUS_PANEL(self));
        
        if ( self->prv->ncp != NULL ) {
            g_debug("NwamConnStatusPanel: Refresh Called");
        }
        
        text = nwamui_daemon_get_active_env_name(NWAMUI_DAEMON(self->prv->daemon));
        if (text) {
            gtk_label_set_text(self->prv->env_name_lbl, text);
            g_free (text);
        } else {
            gtk_label_set_text(self->prv->env_name_lbl, _("Unknow env"));
        }
        on_nwam_enm_notify_cb (NULL, NULL, (gpointer)self);
        return(TRUE);
}

/**
 * nwam_conn_status_add:
 * @self,
 * @connection: FIXME should be a specific type.
 * 
 * Add connections to tree view.
 */
void
nwam_conn_status_add (NwamConnStatusPanel *self, NwamuiNcu* connection)
{
	GtkTreeIter iter;
	gtk_tree_store_append (GTK_TREE_STORE(self->prv->model), &iter, NULL);
	gtk_tree_store_set (GTK_TREE_STORE(self->prv->model), &iter,
			    0, connection,
			    -1);
    g_signal_connect (connection, "notify", G_CALLBACK(on_nwam_ncu_notify_cb), (gpointer)self);
}

static void
nwam_conn_status_panel_finalize(NwamConnStatusPanel *self)
{
	(*G_OBJECT_CLASS(nwam_conn_status_panel_parent_class)->finalize) (G_OBJECT(self));
}

/* Callbacks */

/**
 * help:
 **/
static gboolean
help (NwamConnStatusPanel *self, gpointer data)
{
    g_debug ("NwamConnStatusPanel: Help");
    nwamui_util_show_help ("");
}

static void
nwam_conn_status_update_status_cell_cb (GtkTreeViewColumn *col,
				 GtkCellRenderer   *renderer,
				 GtkTreeModel      *model,
				 GtkTreeIter       *iter,
				 gpointer           data)
{
        gint                    cell_num = (gint)data;  /* Number of cell in column */
	gpointer                connection = NULL;
	gchar                  *stockid = NULL;
        NwamuiNcu*              ncu = NULL;
        nwamui_ncu_type_t       ncu_type;
        gchar*                  ncu_text = NULL;
        gchar*                  ncu_markup = NULL;
        gboolean                ncu_status;
        gboolean                ncu_is_dhcp;
        gchar*                  ncu_ipv4_addr = NULL;
        GdkPixbuf              *status_icon;
        gchar*                  info_string = NULL;
        
	gtk_tree_model_get(model, iter, 0, &connection, -1);

        ncu  = NWAMUI_NCU( connection );
        ncu_type = nwamui_ncu_get_ncu_type(ncu);
        ncu_status = nwamui_ncu_get_active(ncu);
        
	switch (gtk_tree_view_column_get_sort_column_id (col)) {
	case CONNVIEW_ICON:
                if ( cell_num == 0 ) {
                    status_icon = nwamui_util_get_network_status_icon( ncu );
                    g_object_set (G_OBJECT(renderer),
                            "pixbuf", status_icon,
                            NULL);
                    g_object_unref(G_OBJECT(status_icon));
                }
                else if( cell_num > 0 && ncu_type == NWAMUI_NCU_TYPE_WIRELESS ) {
                    status_icon = nwamui_util_get_wireless_strength_icon( 
                                            nwamui_ncu_get_wifi_signal_strength(ncu), FALSE );
                    g_object_set (G_OBJECT(renderer),
                            "pixbuf", status_icon,
                            NULL);
                    g_object_unref(G_OBJECT(status_icon));
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
                if ( ! ncu_status ) {
                    if ( ncu_is_dhcp ) {
                        info_string = g_strdup_printf(_("DHCP"));
                    }
                    else {
                        info_string = g_strdup_printf(_("Static: %s"), ncu_ipv4_addr?ncu_ipv4_addr:_("<i>No IP Address</i>") );
                    }
                }
                else {
                    if ( ncu_is_dhcp ) {
                        info_string = g_strdup_printf(_("DHCP, %s"), ncu_ipv4_addr?ncu_ipv4_addr:_("<i>No IP Address</i>") );
                    }
                    else {
                        info_string = g_strdup_printf(_("%s"), ncu_ipv4_addr?ncu_ipv4_addr:_("<i>No IP Address</i>") );
                    }
                }

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
		g_object_set (G_OBJECT(renderer),
			"text", ncu_status?_("Enabled"):_("Disabled"),
			NULL);
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
	GtkWidget *toplevel;
	
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET(button));
	if (!GTK_WIDGET_TOPLEVEL(toplevel)) {
		toplevel = NULL;
	}
	
        if ( self->prv->env_dialog == NULL ) {
            self->prv->env_dialog = nwam_env_pref_dialog_new();
        }
        
        response = nwam_env_pref_dialog_run( self->prv->env_dialog, GTK_WINDOW(toplevel) );
        
        g_debug("env dialog returned %d", response );
}

static void
vpn_clicked_cb( GtkButton *button, gpointer data )
{
	NwamConnStatusPanel* self = NWAM_CONN_STATUS_PANEL(data);
	gint response;
	GtkWidget *toplevel;
	
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET(button));
        
	if (!GTK_WIDGET_TOPLEVEL(toplevel)) {
		toplevel = NULL;
	}
	
	if ( self->prv->vpn_dialog == NULL ) {
            self->prv->vpn_dialog = nwam_vpn_pref_dialog_new();
        }

	response = nwam_vpn_pref_dialog_run( self->prv->vpn_dialog, GTK_WINDOW(toplevel));
	
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
            gtk_label_set_text(prv->env_name_lbl, text);
            g_free (text);
        } else {
            gtk_label_set_text(prv->env_name_lbl, _("Unknow env"));
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
        
    gtk_label_set_text(prv->vpn_name_lbl, enm_str );
        
    g_free( enm_str );
}

static void
on_nwam_ncu_notify_cb(GObject *gobject, GParamSpec *arg1, gpointer data)
{
	NwamConnStatusPanelPrivate *prv = GET_PRIVATE(data);
	GtkTreeIter iter;
    GtkTreeSelection *selection;

    /* FIXME, should filter some notify::signals out */
    selection = gtk_tree_view_get_selection (prv->conn_status_treeview);
	if (gtk_tree_selection_get_selected(selection,
                                        NULL, &iter)) {
        GtkTreePath* path;
        path = gtk_tree_model_get_path (prv->model, &iter);
        gtk_tree_model_row_changed (prv->model, path, &iter);
        gtk_tree_path_free (path);
    } else {
/*
        g_assert_not_reached();
*/
        g_debug("Unexpected notification received.");
    }
}
