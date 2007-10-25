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
#include <glade/glade.h>
#include <glib/gi18n.h>

#include "libnwamui.h"
#include "nwam_pref_dialog.h"
#include "nwam_env_pref_dialog.h"
#include "nwam_net_conf_panel.h"
#include "nwam_pref_iface.h"

/* Names of Widgets in Glade file */
#define NET_CONF_TREEVIEW             "network_profile_table"
#define CONNECTION_MOVE_UP_BTN        "connection_move_up_btn"
#define CONNECTION_MOVE_DOWN_BTN      "connection_move_down_btn"
#define CONNECTION_RENAME_BTN         "connection_rename_btn"

static void nwam_pref_init (gpointer g_iface, gpointer iface_data);

struct _NwamNetConfPanelPrivate {
	/* Widget Pointers */
	GtkTreeView*	net_conf_treeview;
        GtkTreeModel*   model;
        GtkButton*      connection_move_up_btn;	
        GtkButton*      connection_move_down_btn;	
        GtkButton*      connection_rename_btn;	


	/* Other Data */
        NwamCappletDialog*  pref_dialog;
        NwamuiNcp*          ncp;
};

enum {
	CONNVIEW_CHECK_BOX=0,
	CONNVIEW_INFO,
        CONNVIEW_STATUS
};

static void nwam_net_conf_panel_finalize(NwamNetConfPanel *self);
static void nwam_net_conf_update_status_cell_cb (GtkTreeViewColumn *col,
					     GtkCellRenderer   *renderer,
					     GtkTreeModel      *model,
					     GtkTreeIter       *iter,
					     gpointer           data);

static void nwam_net_conf_add (NwamNetConfPanel *self, NwamuiNcu* connection);

static gboolean nwam_net_conf_panel_refresh(NwamPrefIFace *pref_iface, gpointer data);

/* Callbacks */
static void object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);
static gint nwam_net_conf_connection_compare_cb (GtkTreeModel *model,
			      GtkTreeIter *a,
			      GtkTreeIter *b,
			      gpointer user_data);
static void nwam_net_pref_connection_view_row_activated_cb (GtkTreeView *tree_view,
					GtkTreePath *path,
					GtkTreeViewColumn *column,
					gpointer data);
static void vanity_name_editing_started (GtkCellRenderer *cell,
                                         GtkCellEditable *editable,
                                         const gchar     *path,
                                         gpointer         data);
static void vanity_name_edited ( GtkCellRendererText *cell,
                                 const gchar         *path_string,
                                 const gchar         *new_text,
                                 gpointer             data);
static void nwam_net_pref_connection_enabled_toggled_cb(    GtkCellRendererToggle *cell_renderer,
                                                            gchar                 *path,
                                                            gpointer               user_data);
static void connection_move_up_btn_cb( GtkButton*, gpointer user_data );
static void connection_move_down_btn_cb( GtkButton*, gpointer user_data );	
static void connection_rename_btn_cb( GtkButton*, gpointer user_data );
static void env_clicked_cb( GtkButton *button, gpointer data );
static void vpn_clicked_cb( GtkButton *button, gpointer data );
static gboolean refresh (NwamPrefIFace *self, gpointer data);
static gboolean apply (NwamPrefIFace *self, gpointer data);

G_DEFINE_TYPE_EXTENDED (NwamNetConfPanel,
                        nwam_net_conf_panel,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (NWAM_TYPE_PREF_IFACE, nwam_pref_init))

static void
nwam_pref_init (gpointer g_iface, gpointer iface_data)
{
	NwamPrefInterface *iface = (NwamPrefInterface *)g_iface;
	iface->refresh = nwam_net_conf_panel_refresh;
	iface->apply = NULL;
}

static void
nwam_net_conf_panel_class_init(NwamNetConfPanelClass *klass)
{
	/* Pointer to GObject Part of Class */
	GObjectClass *gobject_class = (GObjectClass*) klass;
	
	/* Override Some Function Pointers */
	gobject_class->finalize = (void (*)(GObject*)) nwam_net_conf_panel_finalize;
}

static void
row_deleted_cb (GtkTreeModel *tree_model, GtkTreePath *path, gpointer user_data) 
{
    g_debug("Row Deleted: %s", gtk_tree_path_to_string(path));
}

static void
row_inserted_cb (GtkTreeModel *tree_model, GtkTreePath *path, GtkTreeIter *iter, gpointer user_data)
{
    g_debug("Row Inserted: %s", gtk_tree_path_to_string(path));
}

static void
rows_reordered_cb(GtkTreeModel *tree_model, GtkTreePath *path, GtkTreeIter *iter, gpointer arg3, gpointer user_data)   
{
    gchar*  path_str = gtk_tree_path_to_string(path);
    g_debug("Rows Reordered: %s", path_str?path_str:"NULL");
}


static GtkTreeModel*
nwam_compose_tree_view (NwamNetConfPanel *self)
{
	GtkTreeViewColumn *col;
	GtkCellRenderer *cell;
	GtkTreeView *view = self->prv->net_conf_treeview;
	GtkTreeModel *model = NULL;

    model = GTK_TREE_MODEL(nwamui_ncp_get_ncu_list_store(self->prv->ncp));
        
    g_assert( GTK_IS_LIST_STORE( model ) );
    gtk_tree_view_set_model(GTK_TREE_VIEW(view), model);
        
	g_object_set (G_OBJECT(view),
		      "headers-clickable", FALSE,
		      "reorderable", TRUE,
		      NULL);
	
	// column enabled
	col = gtk_tree_view_column_new();
	cell = gtk_cell_renderer_toggle_new();
	gtk_tree_view_column_set_title(col, _("Enabled"));
	gtk_tree_view_column_pack_start(col, cell, FALSE);
	gtk_tree_view_column_set_cell_data_func (col,
						 cell,
						 nwam_net_conf_update_status_cell_cb,
						 (gpointer) 0,
						 NULL
		);
    g_object_set_data (G_OBJECT (cell), "column", GINT_TO_POINTER (CONNVIEW_CHECK_BOX));
	g_object_set (cell,
                      "activatable", TRUE,
		      NULL);
    g_signal_connect(G_OBJECT(cell), "toggled", G_CALLBACK(nwam_net_pref_connection_enabled_toggled_cb), (gpointer)model);
	g_object_set (col,
		      "resizable", TRUE,
		      "clickable", FALSE,
		      "sort-indicator", FALSE,
		      "reorderable", FALSE,
		      NULL);
	gtk_tree_view_append_column (view, col);

	// column info
	col = gtk_tree_view_column_new();
	cell = gtk_cell_renderer_text_new();
	gtk_tree_view_column_set_title(col, _("Connection Information"));
	gtk_tree_view_column_pack_start(col, cell, TRUE);
	gtk_tree_view_column_set_cell_data_func (col,
						 cell,
						 nwam_net_conf_update_status_cell_cb,
						 (gpointer) 0,
						 NULL
		);
    g_object_set_data (G_OBJECT (cell), "column", GINT_TO_POINTER (CONNVIEW_INFO));
	g_object_set (cell,
                      "editable", TRUE,
		      NULL);
    g_signal_connect (cell, "edited", G_CALLBACK(vanity_name_edited), model);
    g_signal_connect (cell, "editing-started", G_CALLBACK (vanity_name_editing_started), (gpointer)model);
	g_object_set (col,
		      "expand", TRUE,
		      "resizable", TRUE,
		      "clickable", FALSE,
		      "sort-indicator", FALSE,
		      "reorderable", FALSE,
		      NULL);
	gtk_tree_view_append_column (view, col);
        
        // column status
	col = gtk_tree_view_column_new();
	cell = gtk_cell_renderer_text_new();
	gtk_tree_view_column_set_title(col, _("Connection Status"));
	gtk_tree_view_column_pack_end(col, cell, FALSE);
	gtk_tree_view_column_set_cell_data_func (col,
						 cell,
						 nwam_net_conf_update_status_cell_cb,
						 (gpointer) 0,
						 NULL
		);
    g_object_set_data (G_OBJECT (cell), "column", GINT_TO_POINTER (CONNVIEW_STATUS));
	g_object_set (cell,
		      "weight", PANGO_WEIGHT_BOLD,
                      NULL );
	g_object_set (col,
		      "resizable", TRUE,
		      "clickable", FALSE,
		      "sort-indicator", FALSE,
		      "reorderable", FALSE,
		      NULL);
	gtk_tree_view_append_column (view, col);
        
    return( model );
}

static void
nnwam_net_conf_panel_set_ncp(NwamNetConfPanel *self, NwamuiNcp* ncp )
{
	NwamNetConfPanelPrivate *prv = self->prv;

    prv->ncp = NWAMUI_NCP(g_object_ref(ncp));

    prv->model = nwam_compose_tree_view (self);

    nwam_pref_refresh(NWAM_PREF_IFACE(self), NULL);
}

static void
nwam_net_conf_panel_init(NwamNetConfPanel *self)
{
	self->prv = g_new0(NwamNetConfPanelPrivate, 1);
	/* Iniialise pointers to important widgets */
	self->prv->net_conf_treeview = GTK_TREE_VIEW(nwamui_util_glade_get_widget(NET_CONF_TREEVIEW));
        self->prv->connection_move_up_btn = GTK_BUTTON(nwamui_util_glade_get_widget(CONNECTION_MOVE_UP_BTN));	
        g_signal_connect(self->prv->connection_move_up_btn, "clicked", G_CALLBACK(connection_move_up_btn_cb), (gpointer)self);	
        self->prv->connection_move_down_btn = GTK_BUTTON(nwamui_util_glade_get_widget(CONNECTION_MOVE_DOWN_BTN));	
        g_signal_connect(self->prv->connection_move_down_btn, "clicked", G_CALLBACK(connection_move_down_btn_cb), (gpointer)self);	
        self->prv->connection_rename_btn = GTK_BUTTON(nwamui_util_glade_get_widget(CONNECTION_RENAME_BTN));	
        g_signal_connect(self->prv->connection_rename_btn, "clicked", G_CALLBACK(connection_rename_btn_cb), (gpointer)self);	

        self->prv->model = NULL;
        self->prv->pref_dialog = NULL;
        self->prv->ncp = NULL;
        
	g_signal_connect(G_OBJECT(self), "notify", (GCallback)object_notify_cb, NULL);

	g_signal_connect(GTK_TREE_VIEW(self->prv->net_conf_treeview),
			 "row-activated",
			 (GCallback)nwam_net_pref_connection_view_row_activated_cb,
			 (gpointer)self);
}

/**
 * nwam_net_conf_panel_new:
 * @returns: a new #NwamNetConfPanel.
 *
 * Creates a new #NwamNetConfPanel with an empty ncu
 **/
NwamNetConfPanel*
nwam_net_conf_panel_new(NwamCappletDialog *pref_dialog, NwamuiNcp* ncp)
{
	NwamNetConfPanel *self =  NWAM_NET_CONF_PANEL(g_object_new(NWAM_TYPE_NET_CONF_PANEL, NULL));
        
        /* FIXME - Use GOBJECT Properties */
        self->prv->pref_dialog = g_object_ref( G_OBJECT( pref_dialog ));
        
        nnwam_net_conf_panel_set_ncp(self, ncp);
        
        return( self );
}

/**
 * nwam_net_conf_panel_refresh:
 *
 * Refresh #NwamNetConfPanel with the new connections.
 **/
static gboolean
nwam_net_conf_panel_refresh(NwamPrefIFace *pref_iface, gpointer data)
{
    NwamNetConfPanel*    self = NWAM_NET_CONF_PANEL( pref_iface );
    
    g_assert(NWAM_IS_NET_CONF_PANEL(self));
    
    if ( self->prv->ncp != NULL ) {
/*
        gtk_list_store_clear( GTK_LIST_STORE( self->prv->model ) );

        nwamui_ncp_foreach_ncu( self->prv->ncp, add_ncu_element, (gpointer)self );
*/
        g_debug("NwamNetConfPanel: Refresh");
    }
    
    return( TRUE );
}

/**
 * nwam_net_conf_add:
 * @self,
 * @connection: FIXME should be a specific type.
 * 
 * Add connections to tree view.
 */
static void
nwam_net_conf_add (NwamNetConfPanel *self, NwamuiNcu* connection)
{
	GtkTreeIter iter;
	gtk_list_store_append (GTK_LIST_STORE(self->prv->model), &iter);
	gtk_list_store_set (GTK_LIST_STORE(self->prv->model), &iter,
			    0, connection,
			    -1);
}

void
nwam_net_conf_clear (NwamNetConfPanel *self)
{
	gtk_list_store_clear (GTK_LIST_STORE(self->prv->model));
}

static void
nwam_net_conf_panel_finalize(NwamNetConfPanel *self)
{
        g_object_unref(G_OBJECT(self->prv->connection_move_up_btn));	
        g_object_unref(G_OBJECT(self->prv->connection_move_down_btn));	
        g_object_unref(G_OBJECT(self->prv->connection_rename_btn));	
        
        g_free(self->prv);
	self->prv = NULL;
	
	(*G_OBJECT_CLASS(nwam_net_conf_panel_parent_class)->finalize) (G_OBJECT(self));
}

/* Callbacks */

/*
 * We don't need a comp here actually due to requirements
 */
static gint
nwam_net_conf_connection_compare_cb (GtkTreeModel *model,
			 GtkTreeIter *a,
			 GtkTreeIter *b,
			 gpointer user_data)
{
	return 0;
}


static void
nwam_net_conf_update_status_cell_cb (GtkTreeViewColumn *col,
				 GtkCellRenderer   *renderer,
				 GtkTreeModel      *model,
				 GtkTreeIter       *iter,
				 gpointer           data)
{
    gint                    cell_num = (gint)data;  /* Number of cell in column */
    gpointer                connection = NULL;
    gchar                  *stockid = NULL;
    GString                *infobuf;
    NwamuiNcu*              ncu = NULL;
    nwamui_ncu_type_t       ncu_type;
    gchar*                  ncu_text;
    gchar*                  ncu_markup;
    gboolean                ncu_status;
    gboolean                ncu_is_dhcp;
    gchar*                  ncu_ipv4_addr;
    GdkPixbuf              *status_icon;
    gchar*                  info_string;
    
    gtk_tree_model_get(model, iter, 0, &connection, -1);
    
    ncu  = NWAMUI_NCU( connection );
    ncu_type = nwamui_ncu_get_ncu_type(ncu);
    ncu_status = nwamui_ncu_get_active(ncu);
    
/*
    gtk_tree_view_column_get_sort_column_id(col)
*/
    switch (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (renderer), "column"))) {
        case CONNVIEW_CHECK_BOX: {
                gboolean enabled = nwamui_ncu_get_active( ncu );

                /* TODO - handle case of conditions, and set "inconsistent" */
                g_object_set(G_OBJECT(renderer),
                        "active", enabled,
                        NULL); 
            }
            break;
        
        case CONNVIEW_INFO: {
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

                g_object_set(G_OBJECT(renderer),
                        "markup", ncu_markup,
                        NULL);

                if ( ncu_ipv4_addr != NULL )
                    g_free( ncu_ipv4_addr );

                g_free( ncu_markup );
                g_free( ncu_text );
            }
            break;
            
        case CONNVIEW_STATUS: {
                g_object_set(G_OBJECT(renderer),
                        "text", ncu_status?_("Enabled"):_("Disabled"),
                                NULL);
            }
            break;
        default:
            g_assert_not_reached();
    }
    
    if ( ncu != NULL )
        g_object_unref(G_OBJECT(ncu));

}

static void
vanity_name_editing_started (GtkCellRenderer *cell,
                             GtkCellEditable *editable,
                             const gchar     *path,
                             gpointer         data)
{
    g_debug("Editing Started");
    if (GTK_IS_ENTRY (editable)) {
        GtkEntry *entry = GTK_ENTRY (editable);
        GtkTreeIter iter;
        GtkTreeModel *model = GTK_TREE_MODEL(data);
        GtkTreePath  *tpath;

        if ( (tpath = gtk_tree_path_new_from_string(path)) != NULL 
              && gtk_tree_model_get_iter (model, &iter, tpath))
        {
            gpointer    connection;
            NwamuiNcu*  ncu;
            gchar*      vname;

            gtk_tree_model_get(model, &iter, 0, &connection, -1);

            ncu  = NWAMUI_NCU( connection );

            vname = nwamui_ncu_get_vanity_name(ncu);
            
            gtk_entry_set_text( entry, vname?vname:"" );

            g_free( vname );
            
            g_object_unref(G_OBJECT(connection));

            gtk_tree_path_free(tpath);
        }
    }
}

static void
vanity_name_edited ( GtkCellRendererText *cell,
                     const gchar         *path_string,
                     const gchar         *new_text,
                     gpointer             data)
{
    GtkTreeModel *model = (GtkTreeModel *)data;
    GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
    GtkTreeIter iter;

    gint column = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (cell), "column"));


    if ( gtk_tree_model_get_iter (model, &iter, path))
    switch (column) {
        case CONNVIEW_INFO:      {
            gpointer    connection;
            NwamuiNcu*  ncu;
            gtk_tree_model_get(model, &iter, 0, &connection, -1);

            ncu  = NWAMUI_NCU( connection );

            nwamui_ncu_set_vanity_name(ncu, new_text);

            g_object_unref(G_OBJECT(connection));

          }
          break;
        default:
            g_assert_not_reached();
    }

    gtk_tree_path_free (path);
}

/*
 * Double-clicking a connection switches the status view to that connection's
 * properties view (p5)
 */
static void
nwam_net_pref_connection_view_row_activated_cb (GtkTreeView *tree_view,
			    GtkTreePath *path,
			    GtkTreeViewColumn *column,
			    gpointer data)
{
	NwamNetConfPanel* self = NWAM_NET_CONF_PANEL(data);
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

/*
 * Enabled Toggle Button was toggled
 */
static void
nwam_net_pref_connection_enabled_toggled_cb(    GtkCellRendererToggle *cell_renderer,
                                                gchar                 *path,
                                                gpointer               user_data) 
{
    GtkTreeIter iter;
    GtkTreeModel *model = GTK_TREE_MODEL(user_data);
    GtkTreePath  *tpath;

    if ( (tpath = gtk_tree_path_new_from_string(path)) != NULL 
          && gtk_tree_model_get_iter (model, &iter, tpath))
    {
        gpointer    connection;
        NwamuiNcu*  ncu;

        gtk_tree_model_get(model, &iter, 0, &connection, -1);

        ncu  = NWAMUI_NCU( connection );

        nwamui_ncu_set_active(ncu, !gtk_cell_renderer_toggle_get_active( cell_renderer) );

        g_object_unref(G_OBJECT(connection));
        
        gtk_tree_path_free(tpath);
    }
}

static void
connection_move_up_btn_cb( GtkButton* button, gpointer user_data )
{
    NwamNetConfPanel*           self = NWAM_NET_CONF_PANEL(user_data);
    NwamNetConfPanelPrivate*    prv = self->prv;
    GtkTreeModel*               model;
    GtkTreeIter                 iter;
    GtkTreeSelection*           selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(prv->net_conf_treeview));

    if ( gtk_tree_selection_get_selected( selection, &model, &iter ) ) {
        GtkTreePath*    path = gtk_tree_model_get_path(model, &iter);
        
        if ( gtk_tree_path_prev(path) ) { /* See if we have somewhere to move up to... */
            GtkTreeIter prev_iter;
            if ( gtk_tree_model_get_iter(model, &prev_iter, path) ) {
                gtk_list_store_move_before(GTK_LIST_STORE(model), &iter, &prev_iter );
            }
        }
        gtk_tree_path_free(path);
    }
}

static void
connection_move_down_btn_cb( GtkButton* button, gpointer user_data )
{
    NwamNetConfPanel*           self = NWAM_NET_CONF_PANEL(user_data);
    NwamNetConfPanelPrivate*    prv = self->prv;
    GtkTreeModel*               model;
    GtkTreeIter                 iter;
    GtkTreeSelection*           selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(prv->net_conf_treeview));

    if ( gtk_tree_selection_get_selected( selection, &model, &iter ) ) {
        GtkTreeIter *next_iter = gtk_tree_iter_copy(&iter);
        
        if ( gtk_tree_model_iter_next(GTK_TREE_MODEL(model), next_iter) ) { /* See if we have somewhere to move down to... */
            gtk_list_store_move_after(GTK_LIST_STORE(model), &iter, next_iter );
        }
        
        gtk_tree_iter_free(next_iter);
    }
}

static void
connection_rename_btn_cb( GtkButton* button, gpointer user_data )
{
    NwamNetConfPanel*           self = NWAM_NET_CONF_PANEL(user_data);
    NwamNetConfPanelPrivate*    prv = self->prv;
    GtkTreeModel*               model;
    GtkTreeIter                 iter;
    GtkCellRendererText*        txt;
    GtkTreeSelection*           selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(prv->net_conf_treeview));

    if ( gtk_tree_selection_get_selected( selection, &model, &iter ) ) {
        GtkTreeViewColumn*  info_col = gtk_tree_view_get_column( GTK_TREE_VIEW(self->prv->net_conf_treeview), CONNVIEW_INFO );
        GList*              renderers = gtk_tree_view_column_get_cell_renderers( info_col );
        
        /* Should be only one renderer */
        g_assert( g_list_next( renderers ) == NULL );
        
        if ((txt = GTK_CELL_RENDERER_TEXT(g_list_first( renderers )->data)) != NULL) {
            GtkTreePath*    tpath = gtk_tree_model_get_path(model, &iter);

            gtk_tree_view_set_cursor (GTK_TREE_VIEW(self->prv->net_conf_treeview), tpath, info_col, TRUE);

            gtk_tree_path_free(tpath);
        }
    }
}

static void
object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
    g_debug("NwamNetConfPanel: notify %s changed\n", arg1->name);
}

