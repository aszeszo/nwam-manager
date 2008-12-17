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
#include "capplet-utils.h"

/* Names of Widgets in Glade file */
#define NET_CONF_TREEVIEW               "network_profile_table"
#define CONNECTION_MOVE_UP_BTN          "connection_move_up_btn"
#define CONNECTION_MOVE_DOWN_BTN        "connection_move_down_btn"
#define CONNECTION_RENAME_BTN           "connection_rename_btn"
#define CONDITIONS_CONNECTED_CBOX       "conditions_connected_cbox"
#define CONDITIONS_CONNECTED_COMBO      "conditions_connected_combo"
#define CONDITIONS_CONNECTED_TABLE      "conditions_connected_table"
#define SET_STATUS_LBL                  "set_status_lbl"
#define CONDITIONS_SET_STATUS_COMBO     "conditions_set_status_combo"
#define CONDITIONS_EXPANDER				"conditions_expander"
#define ACTIVE_PROFILE_CBOX             "profile_name_combo1"

#define TREEVIEW_COLUMN_NUM             "meta:colume"

static void nwam_pref_init (gpointer g_iface, gpointer iface_data);

struct _NwamNetConfPanelPrivate {
	/* Widget Pointers */
	GtkTreeView*	    net_conf_treeview;
    GtkTreeModel*       model;
    GtkTreeModel*       rules_model;
    GtkButton*          connection_move_up_btn;	
    GtkButton*          connection_move_down_btn;	
    GtkButton*          connection_rename_btn;	

    GtkComboBox*        active_profile_combo;

    GtkCheckButton*     conditions_connected_cbox;
    GtkComboBox*        conditions_connected_combo;
    GtkTreeView*        conditions_connected_table;
    GtkLabel*           set_status_lbl;
    GtkComboBox*        conditions_set_status_combo;

    GtkExpander*	    conditions_expander;
    GtkComboBox*	    set_status_combo;

	/* Other Data */
    NwamCappletDialog*  pref_dialog;
    NwamuiNcp*          ncp;
    NwamuiNcu*          selected_ncu;			/* Use g_object_set for it */
    gboolean            update_inprogress;
    gboolean			manual_expander_flag;
};

enum {
    PROP_SELECTED_NCU = 1,
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

static void nwam_net_conf_rules_update_cell_cb (GtkTreeViewColumn *col,
					     GtkCellRenderer   *renderer,
					     GtkTreeModel      *model,
					     GtkTreeIter       *iter,
					     gpointer           data);

static void nwam_net_conf_add (NwamNetConfPanel *self, NwamuiNcu* connection);

static gboolean nwam_net_conf_panel_refresh(NwamPrefIFace *pref_iface, gpointer data, gboolean force);
static void net_conf_set_property (GObject         *object,
  guint            prop_id,
  const GValue    *value,
  GParamSpec      *pspec);

static void net_conf_get_property (GObject         *object,
  guint            prop_id,
  GValue          *value,
  GParamSpec      *pspec);

/* Callbacks */
static void object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);
static void ncu_notify_cb (NwamuiNcu *ncu, GParamSpec *arg1, gpointer data);
static gint nwam_net_conf_connection_compare_cb (GtkTreeModel *model,
			      GtkTreeIter *a,
			      GtkTreeIter *b,
			      gpointer user_data);
static void nwam_net_pref_connection_view_row_activated_cb (GtkTreeView *tree_view,
					GtkTreePath *path,
					GtkTreeViewColumn *column,
					gpointer data);
static void nwam_net_pref_connection_view_row_selected_cb (GtkTreeView *tree_view,
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

static void nwam_net_pref_rule_ncu_enabled_toggled_cb(      GtkCellRendererToggle *cell_renderer,
                                                            gchar                 *path,
                                                            gpointer               user_data);

static void connection_move_up_btn_cb( GtkButton*, gpointer user_data );
static void connection_move_down_btn_cb( GtkButton*, gpointer user_data );	
static void connection_rename_btn_cb( GtkButton*, gpointer user_data );
static void env_clicked_cb( GtkButton *button, gpointer data );
static void vpn_clicked_cb( GtkButton *button, gpointer data );
static gboolean help (NwamPrefIFace *self, gpointer data);
static gboolean apply (NwamPrefIFace *self, gpointer data);

static void update_rules_from_ncu (NwamNetConfPanel* self,
  GParamSpec *arg1,
  gpointer ncu);
static void conditions_connected_toggled_cb(GtkToggleButton *togglebutton, gpointer user_data);
static void conditions_set_status_changed(GtkComboBox* combo, gpointer user_data );
static void conditions_connected_changed(GtkComboBox* combo, gpointer user_data );
static void active_profile_changed(GtkComboBox* combo, gpointer user_data);

static void on_activate_static_menuitems (GtkAction *action, gpointer data);

G_DEFINE_TYPE_EXTENDED (NwamNetConfPanel,
  nwam_net_conf_panel,
  G_TYPE_OBJECT,
  0,
  G_IMPLEMENT_INTERFACE (NWAM_TYPE_PREF_IFACE, nwam_pref_init))

static const gchar *ui_spec =
  "<ui>"
  "	<popup name='NetPrefPanelPM_NCU'>"
  "		<menuitem action='0_enable'/>"
  "		<menuitem action='1_cond'/>"
  "		<menuitem action='2_disable'/>"
  "	</popup>"
  "	<popup name='NetPrefPanelPM_NCU_Group'>"
  "		<menuitem action='3_only_one'/>"
  "		<menuitem action='4_one_more'/>"
  "		<menuitem action='5_all'/>"
  "	</popup>"
  "</ui>";

static GtkActionEntry entries[] = {
    { "0_enable",
      NULL, N_("Always active when available"), 
      NULL, NULL, G_CALLBACK(on_activate_static_menuitems) },
    { "1_cond",
      NULL, N_("Conditionally active when available"),
      NULL, NULL, G_CALLBACK(on_activate_static_menuitems) },
    { "2_diable",
      NULL, N_("Never active"),
      NULL, NULL, G_CALLBACK(on_activate_static_menuitems) },
    { "3_only_one",
      NULL, N_("Only one connection may be active"),
      NULL, NULL, G_CALLBACK(on_activate_static_menuitems) },
    { "4_one_more",
      NULL, N_("One or more connections may be active"),
      NULL, NULL, G_CALLBACK(on_activate_static_menuitems) },
    { "5_all",
      NULL, N_("All connections must be active"),
      NULL, NULL, G_CALLBACK(on_activate_static_menuitems) },
};

static void
nwam_pref_init (gpointer g_iface, gpointer iface_data)
{
	NwamPrefInterface *iface = (NwamPrefInterface *)g_iface;
	iface->refresh = nwam_net_conf_panel_refresh;
	iface->apply = NULL;
	iface->help = help;
}

static void
nwam_net_conf_panel_class_init(NwamNetConfPanelClass *klass)
{
	/* Pointer to GObject Part of Class */
	GObjectClass *gobject_class = (GObjectClass*) klass;
    gobject_class->set_property = net_conf_set_property;
    gobject_class->get_property = net_conf_get_property;

	/* Override Some Function Pointers */
	gobject_class->finalize = (void (*)(GObject*)) nwam_net_conf_panel_finalize;
    /* Create some properties */
    g_object_class_install_property (gobject_class,
      PROP_SELECTED_NCU,
      g_param_spec_object ("selected_ncu",
        _("selected_ncu"),
        _("selected_ncu"),
        NWAMUI_TYPE_NCU,
        G_PARAM_READWRITE));
}

static void net_conf_set_property ( GObject         *object,
  guint            prop_id,
  const GValue    *value,
  GParamSpec      *pspec)
{
    NwamNetConfPanel*       self = NWAM_NET_CONF_PANEL(object);

    switch (prop_id) {
    case PROP_SELECTED_NCU: {
        if (self->prv->selected_ncu) {
            g_signal_handlers_disconnect_by_func (self->prv->selected_ncu,
              (GCallback)ncu_notify_cb,
              (gpointer)self);
            g_object_unref (self->prv->selected_ncu);
        }
        self->prv->selected_ncu = g_value_dup_object (value);
        if (self->prv->selected_ncu) {
            g_signal_connect (self->prv->selected_ncu,
              "notify",
              (GCallback)ncu_notify_cb,
              (gpointer)self);
        }
    }
        break;
    default: 
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void net_conf_get_property ( GObject         *object,
  guint            prop_id,
  GValue          *value,
  GParamSpec      *pspec)
{
    NwamNetConfPanel*       self = NWAM_NET_CONF_PANEL(object);

    switch (prop_id) {
    case PROP_SELECTED_NCU: {
        g_value_take_object (value, self->prv->selected_ncu);
    }
        break;
    default: 
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
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

    if ( self->prv->ncp == NULL ) {
        return NULL;
    }

    model = GTK_TREE_MODEL(nwamui_ncp_get_ncu_list_store(self->prv->ncp));
        
    g_assert( GTK_IS_LIST_STORE( model ) );
    gtk_tree_view_set_model(GTK_TREE_VIEW(view), model);
        
	g_object_set (G_OBJECT(view),
		      "headers-clickable", FALSE,
		      "reorderable", TRUE,
		      NULL);
	
	// column enabled
	cell = gtk_cell_renderer_toggle_new();
	col = gtk_tree_view_column_new_with_attributes(_("Enabled"),
      cell,
      "resizable", TRUE,
      "clickable", FALSE,
      "sort-indicator", FALSE,
      "reorderable", FALSE,
      NULL);
	gtk_tree_view_append_column (view, col);
    g_object_set_data (G_OBJECT (col), TREEVIEW_COLUMN_NUM, GINT_TO_POINTER (CONNVIEW_CHECK_BOX));

	g_object_set (cell,
      "xalign", 0.5,
      NULL );
	gtk_tree_view_column_set_cell_data_func (col, cell,
      nwam_net_conf_update_status_cell_cb, (gpointer) 0,
      NULL);
    g_object_set_data (G_OBJECT (cell), TREEVIEW_COLUMN_NUM, GINT_TO_POINTER (CONNVIEW_CHECK_BOX));
	g_object_set (cell, "active", TRUE, NULL);
    g_signal_connect(G_OBJECT(cell), "toggled", G_CALLBACK(nwam_net_pref_connection_enabled_toggled_cb), (gpointer)model);

	// column info
    // First cell
	cell = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes(_("Details"),
      cell,
      "expand", TRUE,
      "resizable", TRUE,
      "clickable", TRUE,
      "sort-indicator", TRUE,
      "reorderable", TRUE,
      NULL);
	gtk_tree_view_append_column (view, col);
    g_object_set_data (G_OBJECT (col), TREEVIEW_COLUMN_NUM, GINT_TO_POINTER (CONNVIEW_INFO));

    g_object_set_data (G_OBJECT (cell), TREEVIEW_COLUMN_NUM, GINT_TO_POINTER (CONNVIEW_INFO));
	g_object_set (cell, "editable", TRUE, NULL);
    g_signal_connect (cell, "edited", G_CALLBACK(vanity_name_edited), model);
    g_signal_connect (cell, "editing-started", G_CALLBACK (vanity_name_editing_started), (gpointer)model);
	gtk_tree_view_column_set_cell_data_func (col, cell,
      nwam_net_conf_update_status_cell_cb, (gpointer) 0,
      NULL);
    // Second cell
	cell = gtk_cell_renderer_text_new();
	g_object_set (cell,
      "xalign", 1.0,
      "weight", PANGO_WEIGHT_BOLD,
      NULL );
	g_object_set_data (G_OBJECT (cell), TREEVIEW_COLUMN_NUM, GINT_TO_POINTER (CONNVIEW_STATUS));
	gtk_tree_view_column_pack_end(col, cell, FALSE);
	gtk_tree_view_column_set_cell_data_func (col,
      cell,
      nwam_net_conf_update_status_cell_cb,
      (gpointer) 0,
      NULL);
        
    return( model );
}

/*
 * Filter function to decide if a given row is visible or not
 */
gboolean            
nwam_rules_tree_view_filter(GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
    NwamNetConfPanel*       self = NWAM_NET_CONF_PANEL(data);
    gpointer                connection = NULL;
    NwamuiNcu*              ncu = NULL;
    gchar*                  ncu_text;
    gboolean                show_row = TRUE;
    
    gtk_tree_model_get(model, iter, 0, &connection, -1);
    
    ncu  = NWAMUI_NCU( connection );
    
    if ( ncu != NULL && ncu == self->prv->selected_ncu ) {
        show_row = FALSE;
    }
    
    if ( ncu != NULL ) {
        g_object_unref(ncu);
    }
    
    return( show_row ); 
}

static void
nwam_compose_rules_tree_view (NwamNetConfPanel *self, GtkTreeView* view )
{
	GtkTreeViewColumn *col;
	GtkCellRenderer *cell;
	GtkTreeModel *model = NULL;

    if ( self->prv->ncp == NULL ) {
        return;
    }
    model = GTK_TREE_MODEL(nwamui_ncp_get_ncu_list_store(self->prv->ncp));
        
    /* Create a filter model */
    model = gtk_tree_model_filter_new(model, NULL ); 
    gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(model), 
                            nwam_rules_tree_view_filter, (gpointer)self, NULL );
    
    gtk_tree_view_set_model(GTK_TREE_VIEW(view), model);
    
    self->prv->rules_model = model;
        
	g_object_set (G_OBJECT(view),
		      "headers-clickable", FALSE,
		      "reorderable", FALSE,
		      NULL);
	
	// column enabled
	cell = gtk_cell_renderer_toggle_new();
	col = gtk_tree_view_column_new_with_attributes(_("Select"),
      cell,
      "resizable", TRUE,
      "clickable", FALSE,
      "sort-indicator", FALSE,
      "reorderable", FALSE,
      NULL);
	gtk_tree_view_column_set_cell_data_func (col,
						 cell,
						 nwam_net_conf_rules_update_cell_cb,
						 (gpointer) self,
						 NULL
		);
    g_object_set_data (G_OBJECT (cell), TREEVIEW_COLUMN_NUM, GINT_TO_POINTER (CONNVIEW_CHECK_BOX));
	g_object_set (cell,
      "activatable", TRUE,
      NULL);
    g_signal_connect(G_OBJECT(cell), "toggled", G_CALLBACK(nwam_net_pref_rule_ncu_enabled_toggled_cb), (gpointer)self);
	gtk_tree_view_append_column (view, col);

	// column info
	cell = gtk_cell_renderer_combo_new();
	col = gtk_tree_view_column_new_with_attributes(_("Connection Information"),
      cell,
      "expand", TRUE,
      "resizable", TRUE,
      "clickable", FALSE,
      "sort-indicator", FALSE,
      "reorderable", FALSE,
      NULL);
	gtk_tree_view_column_set_cell_data_func (col,
						 cell,
						 nwam_net_conf_rules_update_cell_cb,
						 (gpointer) self,
						 NULL
		);
    g_object_set_data (G_OBJECT (cell), TREEVIEW_COLUMN_NUM, GINT_TO_POINTER (CONNVIEW_INFO));
	gtk_tree_view_append_column (view, col);
        
}

static void
nwam_net_conf_panel_set_ncp(NwamNetConfPanel *self, NwamuiNcp* ncp )
{
	NwamNetConfPanelPrivate *prv = self->prv;

    if ( ncp != NULL ) {
        prv->ncp = NWAMUI_NCP(g_object_ref(ncp));
    }

    prv->model = nwam_compose_tree_view (self);
    
    nwam_compose_rules_tree_view(self, self->prv->conditions_connected_table);
    
    nwam_pref_refresh(NWAM_PREF_IFACE(self), NULL, TRUE);
}

static void
nwam_net_conf_panel_init(NwamNetConfPanel *self)
{
    NwamuiDaemon *daemon = nwamui_daemon_get_instance();
	self->prv = g_new0(NwamNetConfPanelPrivate, 1);
	/* Iniialise pointers to important widgets */
	self->prv->net_conf_treeview = GTK_TREE_VIEW(nwamui_util_glade_get_widget(NET_CONF_TREEVIEW));
    self->prv->connection_move_up_btn = GTK_BUTTON(nwamui_util_glade_get_widget(CONNECTION_MOVE_UP_BTN));	
    g_signal_connect(self->prv->connection_move_up_btn, "clicked", G_CALLBACK(connection_move_up_btn_cb), (gpointer)self);	
    self->prv->connection_move_down_btn = GTK_BUTTON(nwamui_util_glade_get_widget(CONNECTION_MOVE_DOWN_BTN));	
    g_signal_connect(self->prv->connection_move_down_btn, "clicked", G_CALLBACK(connection_move_down_btn_cb), (gpointer)self);	
    self->prv->connection_rename_btn = GTK_BUTTON(nwamui_util_glade_get_widget(CONNECTION_RENAME_BTN));	
    g_signal_connect(self->prv->connection_rename_btn, "clicked", G_CALLBACK(connection_rename_btn_cb), (gpointer)self);	
    self->prv->active_profile_combo = GTK_COMBO_BOX(nwamui_util_glade_get_widget(ACTIVE_PROFILE_CBOX));
    g_signal_connect(self->prv->active_profile_combo, "changed", G_CALLBACK(active_profile_changed),(gpointer)self);
    /* FIXME: How about zero ncp? */
    daemon_compose_ncp_combo(GTK_COMBO_BOX(self->prv->active_profile_combo),
      daemon,
      NWAM_PREF_IFACE(self));
    gtk_combo_box_set_active(GTK_COMBO_BOX(self->prv->active_profile_combo), 1);

    self->prv->conditions_connected_cbox = GTK_CHECK_BUTTON(nwamui_util_glade_get_widget(CONDITIONS_CONNECTED_CBOX));
    g_signal_connect(self->prv->conditions_connected_cbox, "toggled", G_CALLBACK(conditions_connected_toggled_cb), (gpointer)self);
    self->prv->conditions_connected_combo = GTK_COMBO_BOX(nwamui_util_glade_get_widget(CONDITIONS_CONNECTED_COMBO));
    g_signal_connect(self->prv->conditions_connected_combo, "changed", G_CALLBACK(conditions_connected_changed),(gpointer)self);
    self->prv->conditions_connected_table = GTK_TREE_VIEW(nwamui_util_glade_get_widget(CONDITIONS_CONNECTED_TABLE));
    self->prv->set_status_lbl = GTK_LABEL(nwamui_util_glade_get_widget(SET_STATUS_LBL));
    self->prv->conditions_set_status_combo = GTK_COMBO_BOX(nwamui_util_glade_get_widget(CONDITIONS_SET_STATUS_COMBO));
    g_signal_connect(self->prv->conditions_set_status_combo, "changed", G_CALLBACK(conditions_set_status_changed),(gpointer)self);
    self->prv->conditions_expander = GTK_EXPANDER(nwamui_util_glade_get_widget(CONDITIONS_EXPANDER));

    self->prv->model = NULL;
    self->prv->rules_model = NULL;
    self->prv->pref_dialog = NULL;
    self->prv->ncp = NULL;
    self->prv->selected_ncu = NULL;
    self->prv->update_inprogress = FALSE;
    
    update_rules_from_ncu (self, NULL, NULL);

	g_signal_connect(G_OBJECT(self), "notify", (GCallback)object_notify_cb, NULL);
	g_signal_connect(G_OBJECT(self),
      "notify::selected-ncu",
      (GCallback)update_rules_from_ncu,
      NULL);

	g_signal_connect(GTK_TREE_VIEW(self->prv->net_conf_treeview),
      "row-activated",
      (GCallback)nwam_net_pref_connection_view_row_activated_cb,
      (gpointer)self);
	g_signal_connect(GTK_TREE_VIEW(self->prv->net_conf_treeview),
      "cursor-changed",
      (GCallback)nwam_net_pref_connection_view_row_selected_cb,
      (gpointer)self);

    g_object_unref(daemon);
}

/**
 * nwam_net_conf_panel_new:
 * @returns: a new #NwamNetConfPanel.
 *
 * Creates a new #NwamNetConfPanel with an empty ncu
 **/
NwamNetConfPanel*
nwam_net_conf_panel_new(NwamCappletDialog *pref_dialog)
{
	NwamNetConfPanel *self =  NWAM_NET_CONF_PANEL(g_object_new(NWAM_TYPE_NET_CONF_PANEL, NULL));
        
    /* FIXME - Use GOBJECT Properties */
    self->prv->pref_dialog = g_object_ref( G_OBJECT( pref_dialog ));

    return( self );
}

/**
 * nwam_net_conf_panel_refresh:
 *
 * Refresh #NwamNetConfPanel with the new connections.
 **/
static gboolean
nwam_net_conf_panel_refresh(NwamPrefIFace *pref_iface, gpointer data, gboolean force)
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

static gboolean
help (NwamPrefIFace *self, gpointer data)
{
    g_debug("NwamNetConfPanel: Help");
    nwamui_util_show_help ("");
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

    if (self->prv->selected_ncu) {
        g_object_unref (self->prv->selected_ncu);
    }

    g_free(self->prv);
	self->prv = NULL;
	
	G_OBJECT_CLASS(nwam_net_conf_panel_parent_class)->finalize(G_OBJECT(self));
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
nwam_net_conf_rules_update_cell_cb (GtkTreeViewColumn *col,
					     GtkCellRenderer   *renderer,
					     GtkTreeModel      *model,
					     GtkTreeIter       *iter,
					     gpointer           data)
{
#if 0
  	NwamNetConfPanel*       self = NWAM_NET_CONF_PANEL(data);
    gpointer                connection = NULL;
    NwamuiNcu*              ncu = NULL;
    gchar*                  ncu_text;
    
    gtk_tree_model_get(model, iter, 0, &connection, -1);
    
    ncu  = NWAMUI_NCU( connection );
    
    switch (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (renderer), TREEVIEW_COLUMN_NUM))) {
        case CONNVIEW_CHECK_BOX: {
                gboolean enabled = FALSE;
                
                if ( self->prv->selected_ncu != NULL ) {
                    enabled = nwamui_ncu_selection_rule_ncus_contains( self->prv->selected_ncu, ncu );
                }

                g_object_set(G_OBJECT(renderer),
                        "active", enabled,
                        NULL); 
            }
            break;
        
        case CONNVIEW_INFO: {
                ncu_text = nwamui_ncu_get_display_name(ncu);

                g_object_set(G_OBJECT(renderer),
                        "text", ncu_text,
                        NULL);

                g_free( ncu_text );
            }
            break;

        default:
            g_assert_not_reached();
    }
    
    if ( ncu != NULL )
        g_object_unref(G_OBJECT(ncu));

#endif
}

static void
nwam_net_conf_update_status_cell_cb (GtkTreeViewColumn *col,
				 GtkCellRenderer   *renderer,
				 GtkTreeModel      *model,
				 GtkTreeIter       *iter,
				 gpointer           data)
{
#if 0
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
    gchar*                  ncu_phy_addr;
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

                g_object_set(G_OBJECT(renderer),
                        "active", enabled,
                        NULL); 
                if (enabled && nwamui_ncu_get_selection_rules_enabled (ncu)) {
                    g_object_set (renderer, "inconsistent", TRUE, NULL);
                } else {
                    g_object_set (renderer, "inconsistent", FALSE, NULL);
                }
            }
            break;
        
        case CONNVIEW_INFO: {
                ncu_text = nwamui_ncu_get_display_name(ncu);
                ncu_is_dhcp = nwamui_ncu_get_ipv4_auto_conf(ncu);
                ncu_ipv4_addr = nwamui_ncu_get_ipv4_address(ncu);
                ncu_phy_addr = nwamui_ncu_get_phy_address(ncu);
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

                if (ncu_phy_addr) {
                    ncu_markup= g_strdup_printf(_("<b>%s</b>\n<small>%s, %s</small>"), ncu_text, info_string, ncu_phy_addr );
                } else {
                    ncu_markup= g_strdup_printf(_("<b>%s</b>\n<small>%s</small>"), ncu_text, info_string );
                }
                g_object_set(G_OBJECT(renderer),
                        "markup", ncu_markup,
                        NULL);

                if ( ncu_ipv4_addr != NULL )
                    g_free( ncu_ipv4_addr );

                if ( ncu_phy_addr != NULL )
                    g_free( ncu_phy_addr );

                g_free (info_string);
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

#endif
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

    tpath = gtk_tree_path_new_from_string(path);
    if (tpath != NULL && gtk_tree_model_get_iter (model, &iter, tpath))
    {
        gpointer    connection;
        NwamuiNcu*  ncu;
        gboolean	inconsistent;

        gtk_tree_model_get(model, &iter, 0, &connection, -1);

        ncu  = NWAMUI_NCU( connection );

        g_object_get (cell_renderer, "inconsistent", &inconsistent, NULL);
        if (gtk_cell_renderer_toggle_get_active(cell_renderer)) {
            if (inconsistent) {
                nwamui_ncu_set_active(ncu, FALSE);
                //nwamui_ncu_set_selection_rules_enabled (ncu, FALSE);
            } else {
                //nwamui_ncu_set_selection_rules_enabled (ncu, TRUE);
            }
        } else {
            nwamui_ncu_set_active(ncu, TRUE);
        }

        g_object_unref(G_OBJECT(connection));
        
    }
    if (tpath) {
        gtk_tree_path_free(tpath);
    }
}

static void
vanity_name_editing_started (GtkCellRenderer *cell,
                             GtkCellEditable *editable,
                             const gchar     *path,
                             gpointer         data)
{
    g_debug("Editing Started");
    if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT(cell), TREEVIEW_COLUMN_NUM)) !=
      CONNVIEW_INFO) {
        return;
    }
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

    gint column = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (cell), TREEVIEW_COLUMN_NUM));


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

    gint columnid = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (column), TREEVIEW_COLUMN_NUM));

    /* skip the toggle coolumn */
    if (columnid != CONNVIEW_CHECK_BOX) {
        if (gtk_tree_model_get_iter (model, &iter, path)) {
            gpointer    connection;
            NwamuiNcu*  ncu;

            gtk_tree_model_get(model, &iter, 0, &connection, -1);

            ncu  = NWAMUI_NCU( connection );

            nwam_capplet_dialog_select_ncu(self->prv->pref_dialog, ncu );
        }
    }
}

static void
update_rules_from_ncu (NwamNetConfPanel* self,
  GParamSpec *arg1,
  gpointer data)
{
    NwamNetConfPanelPrivate*    prv = self->prv;
    GList*                      ncu_list = NULL;
/*     nwamui_ncu_rule_action_t    action; */
/*     nwamui_ncu_rule_state_t     ncu_state; */
    GtkTreeModel*               model = NULL;
    NwamuiNcu*					ncu;

    ncu = prv->selected_ncu;

    if ( prv->update_inprogress ) {
        /* Already in an update, prevent recursion */
        return;
    } else {
        prv->update_inprogress = TRUE;
    }
    
    ncu_notify_cb (ncu, NULL, self);

    gtk_widget_set_sensitive(GTK_WIDGET(prv->conditions_connected_cbox), ncu != NULL );

    if ( ncu != NULL/*  &&  */
/*          nwamui_ncu_get_selection_rule(ncu, &ncu_list, &ncu_state, &action) */ ) {
        /* Rules are Enabled */
        gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(prv->conditions_connected_cbox), TRUE);

        gtk_widget_set_sensitive(GTK_WIDGET(prv->conditions_connected_combo), TRUE);
        gtk_widget_set_sensitive(GTK_WIDGET(prv->conditions_connected_table), TRUE);
        gtk_widget_set_sensitive(GTK_WIDGET(prv->conditions_set_status_combo), TRUE);
        gtk_widget_set_sensitive(GTK_WIDGET(prv->set_status_lbl), TRUE);
        
        gtk_combo_box_set_active(GTK_COMBO_BOX(prv->conditions_set_status_combo), 
                                 /* action == NWAMUI_NCU_RULE_ACTION_DISABLE?1: */0 );

        gtk_combo_box_set_active(GTK_COMBO_BOX(prv->conditions_connected_combo), 
                                 /* ncu_state == NWAMUI_NCU_RULE_STATE_IS_NOT_CONNECTED?1: */0 );

    }
    else {
        gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(prv->conditions_connected_cbox), FALSE);
        
        model = gtk_tree_view_get_model(prv->conditions_connected_table);
        
        gtk_widget_set_sensitive(GTK_WIDGET(prv->conditions_connected_combo), FALSE);;
        gtk_widget_set_sensitive(GTK_WIDGET(prv->conditions_connected_table), FALSE);;
        gtk_widget_set_sensitive(GTK_WIDGET(prv->conditions_set_status_combo), FALSE);;
        gtk_widget_set_sensitive(GTK_WIDGET(prv->set_status_lbl), FALSE);;
    }

    if ( prv->rules_model != NULL ) {
        gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(prv->rules_model));
    }

    prv->update_inprogress = FALSE;

}

/*
 * Selecting (using keys or mouse) a connection needs to have the conditions
 * updated.
 */
static void
nwam_net_pref_connection_view_row_selected_cb (GtkTreeView *tree_view,
                                               gpointer data)
{
	NwamNetConfPanel*   self = NWAM_NET_CONF_PANEL(data);
    GtkTreePath*        path = NULL;
    GtkTreeViewColumn*  focus_column = NULL;;
    GtkTreeIter         iter;
    GtkTreeModel*       model = gtk_tree_view_get_model(tree_view);

    gtk_tree_view_get_cursor ( tree_view, &path, &focus_column );

    if (path != NULL && gtk_tree_model_get_iter (model, &iter, path))
    {
        gpointer    connection;
        NwamuiNcu*  ncu;

        gtk_tree_model_get(model, &iter, 0, &connection, -1);

        ncu  = NWAMUI_NCU( connection );
        
        if (self->prv->selected_ncu != ncu) {
            g_object_set (self, "selected_ncu", ncu, NULL);
        }
    }
    gtk_tree_path_free (path);
}



static void
nwam_net_pref_rule_ncu_enabled_toggled_cb  (    GtkCellRendererToggle *cell_renderer,
                                                gchar                 *path,
                                                gpointer               user_data) 
{
#if 0
	NwamNetConfPanel*           self = NWAM_NET_CONF_PANEL(user_data);
    NwamNetConfPanelPrivate*    prv = self->prv;
    GtkTreeIter                 iter;
    GtkTreeModel*               model = GTK_TREE_MODEL(prv->rules_model);
    GtkTreePath*                tpath;
    gboolean                    active = !gtk_cell_renderer_toggle_get_active( cell_renderer);

    if ( prv->selected_ncu != NULL 
         && prv->update_inprogress != TRUE
         && (tpath = gtk_tree_path_new_from_string(path)) != NULL 
         && gtk_tree_model_get_iter (model, &iter, tpath) )
    {
        gpointer    connection;
        NwamuiNcu*  ncu;

        gtk_tree_model_get(model, &iter, 0, &connection, -1);

        ncu  = NWAMUI_NCU( connection );

        if ( active ) {
            nwamui_ncu_selection_rule_ncus_add( prv->selected_ncu, ncu );
        }
        else {
            nwamui_ncu_selection_rule_ncus_remove( prv->selected_ncu, ncu );
        }

        g_object_unref(G_OBJECT(connection));
        
        gtk_tree_path_free(tpath);
    }
#endif
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
active_profile_changed(GtkComboBox* combo, gpointer user_data)
{

    /* TODO : Handle profile changing */

    g_debug("Active Profile changed");
    return;
}

static void 
conditions_connected_toggled_cb(GtkToggleButton *togglebutton, gpointer user_data)
{
    NwamNetConfPanel*           self = NWAM_NET_CONF_PANEL(user_data);
    NwamNetConfPanelPrivate*    prv = self->prv;

    if ( prv->selected_ncu != NULL && prv->update_inprogress != TRUE )
    {
        //nwamui_ncu_set_selection_rules_enabled(prv->selected_ncu, gtk_toggle_button_get_active(togglebutton));
        
        update_rules_from_ncu(self, NULL, NULL);
    }
}

static void 
conditions_set_status_changed(GtkComboBox* combo, gpointer user_data )
{
    NwamNetConfPanel*           self = NWAM_NET_CONF_PANEL(user_data);
    NwamNetConfPanelPrivate*    prv = self->prv;

    if ( prv->selected_ncu != NULL && prv->update_inprogress != TRUE )
    {
        /* nwamui_ncu_rule_action_t action = NWAMUI_NCU_RULE_ACTION_DISABLE; */
        
        switch( gtk_combo_box_get_active(combo)) {
            case 0:
                /* action = NWAMUI_NCU_RULE_ACTION_ENABLE; */
                break;
            case 1:
                /* action = NWAMUI_NCU_RULE_ACTION_DISABLE; */
                break;
            default:
                g_assert_not_reached();
                break;
        }
        
        /* nwamui_ncu_set_selection_rule_action(prv->selected_ncu, action ); */
    }
}

static void 
conditions_connected_changed(GtkComboBox* combo, gpointer user_data )
{
#if 0
    NwamNetConfPanel*           self = NWAM_NET_CONF_PANEL(user_data);
    NwamNetConfPanelPrivate*    prv = self->prv;

    if ( prv->selected_ncu != NULL && prv->update_inprogress != TRUE )
    {
        nwamui_ncu_rule_state_t state = NWAMUI_NCU_RULE_STATE_IS_CONNECTED;
        
        switch( gtk_combo_box_get_active(combo)) {
            case 0:
                state = NWAMUI_NCU_RULE_STATE_IS_CONNECTED;;
                break;
            case 1:
                state = NWAMUI_NCU_RULE_STATE_IS_NOT_CONNECTED;;
                break;
            default:
                g_assert_not_reached();
                break;
        }
        
        nwamui_ncu_set_selection_rule_state(prv->selected_ncu, state );
    }
#endif
}

static void
object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
    g_debug("NwamNetConfPanel: notify %s changed\n", arg1->name);
}

static void
ncu_notify_cb (NwamuiNcu *ncu, GParamSpec *arg1, gpointer user_data)
{
    NwamNetConfPanel*           self = NWAM_NET_CONF_PANEL(user_data);
    NwamNetConfPanelPrivate*    prv = self->prv;
    gchar*                      display_name = NULL;
    gchar*                      status_string = NULL;

    if (ncu != prv->selected_ncu) {
        return;
    }

    if (ncu != NULL ) {
        display_name = nwamui_ncu_get_display_name(NWAMUI_NCU(ncu));
    }

    status_string = g_strdup_printf(_("Then set status of '%s' to: "),
      display_name != NULL ? display_name : "");

    gtk_label_set_text(GTK_LABEL(prv->set_status_lbl), status_string );

    if ( status_string != NULL ) {
        g_free(status_string);
    }
    if ( display_name != NULL ) {
        g_free(display_name);
    }
}

static void
on_activate_static_menuitems (GtkAction *action, gpointer data)
{
    NwamNetConfPanel*           self = NWAM_NET_CONF_PANEL(data);
    gchar first_char = *gtk_action_get_name(action);

    switch(first_char) {
    case '0':
    case '1':
    default:;
    }
}

