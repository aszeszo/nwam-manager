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
#include "nwam_tree_view.h"

#define DEBUG()	g_debug ("[[ %20s : %-4d ]]", __func__, __LINE__)

#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
	NWAM_TYPE_NET_CONF_PANEL, NwamNetConfPanelPrivate)) 

/* Names of Widgets in Glade file */
#define NET_CONF_TREEVIEW               "network_profile_table"
#define PROFILE_NAME_COMBO              "profile_name_combo"
#define EDIT_PROFILE_NAME_COMBO         "edit_profile_name_combo"
#define PROFILE_EDIT_BTN                "profile_edit_btn"
#define CONNECTION_ENABLE_BTN           "connection_enable_btn"
#define CONNECTION_DISABLE_BTN          "connection_disable_btn"
#define CONNECTION_MOVE_UP_BTN          "connection_move_up_btn"
#define CONNECTION_MOVE_DOWN_BTN        "connection_move_down_btn"
#define CONNECTION_RENAME_BTN           "connection_rename_btn"
#define CONNECTION_GROUP_BTN            "connection_group_btn"
#define ACTIVATION_MODE_LBL             "activation_mode_lbl"
#define CONNECTION_ACTIVATION_COMBO     "connection_activation_combo"

#define TREEVIEW_COLUMN_NUM             "meta:column"
#define BUTTON_IS_GROUP_NEW             "group_button@is_new"
#define COND_MODEL_INFO                 "ncp_group_first_id"
#define NWAM_COND_NCU_GROUP_ID          "ncu_group_id"
#define NWAM_COND_NCU_GROUP_MODE        "ncu_group_mode"
#define NWAM_COND_NCU_ACT_MODE          "ncu_group_activation_mode"

/* Edit Connection Dialog */
#define EDIT_CONNECTIONS_DIALOG         "edit_connections_dialog"
#define CONNECTION_TREEVIEW             "connection_treeview"

/* Values picked to put always ON at top of list, and always off at bottom */
#define ALWAYS_ON_GROUP_ID              (0)
#define ALWAYS_OFF_GROUP_ID             (G_MAXINT)

struct _NwamNetConfPanelPrivate {
	/* Widget Pointers */
	GtkTreeView*	    net_conf_treeview;

    GtkButton*          profile_edit_btn;	
    GtkButton*          connection_enable_btn;	
    GtkButton*          connection_disable_btn;	
    GtkButton*          connection_move_up_btn;	
    GtkButton*          connection_move_down_btn;	
    GtkButton*          connection_rename_btn;
    GtkButton*          connection_group_btn;

    GtkComboBox*        profile_name_combo;
    GtkComboBox*        edit_profile_name_combo;

    GtkLabel*           activation_mode_lbl;
    GtkComboBox*        connection_activation_combo;

    /* Edit Connections dialog */
    GtkDialog*          edit_connections_dialog;
	GtkTreeView*	    connection_treeview;

    GList*              ncu_selection;

	/* Other Data */
    NwamCappletDialog*  pref_dialog;
    NwamuiNcp*          selected_ncp;
    NwamuiNcu*          selected_ncu;			/* Use g_object_set for it */
    gboolean            update_inprogress;
    gboolean			manual_expander_flag;
    gboolean            connection_activation_combo_show_ncu_part;


    gint on_group_num;
    gint off_group_num;
    gint group_num;
    gint on_child_num;
    gint off_child_num;
    gint child_num;
};

enum {
    PROP_SELECTED_NCU = 1,
};

enum {
	CONNVIEW_INFO = 0
};

static const gchar *combo_contents[] = {
    N_("Always active"),
    N_("Activated by rules"),
    N_("Never active"),
    N_("Only one connection may be active"),
    N_("One or more connections may be active"),
    N_("All connections must be active"),
    NULL};

static NwamuiObject *fake_object_in_pri_group = NULL;

static void nwam_pref_init (gpointer g_iface, gpointer iface_data);
static gboolean refresh(NwamPrefIFace *iface, gpointer user_data, gboolean force);
static gboolean apply(NwamPrefIFace *iface, gpointer user_data);
static gboolean help(NwamPrefIFace *iface, gpointer user_data);

static void nwam_net_conf_panel_finalize(NwamNetConfPanel *self);

static void nwam_net_conf_update_status_cell_cb (GtkTreeViewColumn *col,
  GtkCellRenderer   *renderer,
  GtkTreeModel      *model,
  GtkTreeIter       *iter,
  gpointer           data);

static void nwam_connection_cell_func (GtkTreeViewColumn *col,
  GtkCellRenderer   *renderer,
  GtkTreeModel      *model,
  GtkTreeIter       *iter,
  gpointer           data);

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
static void vanity_name_editing_started (GtkCellRenderer *cell,
                                         GtkCellEditable *editable,
                                         const gchar     *path,
                                         gpointer         data);
static void vanity_name_edited ( GtkCellRendererText *cell,
                                 const gchar         *path_string,
                                 const gchar         *new_text,
                                 gpointer             data);
static void nwam_connection_cell_toggled(GtkCellRendererToggle *cell_renderer,
  gchar                 *path,
  gpointer               user_data);
static void nwam_net_pref_connection_enabled_toggled_cb(    GtkCellRendererToggle *cell_renderer,
                                                            gchar                 *path,
                                                            gpointer               user_data); /* unused */

static void nwam_net_pref_rule_ncu_enabled_toggled_cb(      GtkCellRendererToggle *cell_renderer,
                                                            gchar                 *path,
                                                            gpointer               user_data); /* unused */

static void connection_activation_combo_cell_cb(GtkCellLayout *cell_layout,
  GtkCellRenderer   *renderer,
  GtkTreeModel      *model,
  GtkTreeIter       *iter,
  gpointer           data);
static void connection_activation_combo_changed_cb(GtkComboBox* combo, gpointer user_data );
static gboolean connection_activation_combo_filter(GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data);


static void on_button_clicked(GtkButton *button, gpointer user_data );
static void profile_edit_btn_clicked(GtkButton *button, gpointer user_data );
static void update_widgets(NwamNetConfPanel *self, GtkTreeSelection *selection);
static void selection_changed(GtkTreeSelection *selection, gpointer user_data);
static gboolean selection_func(GtkTreeSelection *selection,
  GtkTreeModel *model,
  GtkTreePath *path,
  gboolean path_currently_selected,
  gpointer data);

static void cursor_changed(GtkTreeView *treeview, gpointer user_data);

static void update_rules_from_ncu (NwamNetConfPanel* self,
  GParamSpec *arg1,
  gpointer ncu);                /* unused */
static void conditions_connected_toggled_cb(GtkToggleButton *togglebutton, gpointer user_data);
static void conditions_set_status_changed(GtkComboBox* combo, gpointer user_data );
static void conditions_connected_changed(GtkComboBox* combo, gpointer user_data );
static void on_activate_static_menuitems (GtkAction *action, gpointer data);
static void ncp_add_ncu(NwamuiNcp *ncp, NwamuiNcu* ncu, gpointer data);
static void ncp_remove_ncu(NwamuiNcp *ncp, NwamuiNcu* ncu, gpointer data);

/* ncu group */
static NwamuiObject* create_group_object( gint group_id,
  nwamui_cond_priority_group_mode_t  group_mode,
  nwamui_cond_activation_mode_t      act_mode );
static gint ncu_pri_group_get_index(NwamuiNcu *ncu);
static GtkTreePath* ncu_pri_group_get_path(GtkTreeModel *model, gint group_id);
static gboolean ncu_pri_group_get_iter(GtkTreeModel *model, gint group_id, gint group_mode, GtkTreeIter *parent);
static void ncu_pri_group_update(GtkTreeModel *model);
static void ncu_pri_treeview_add(GtkTreeView *treeview, GtkTreeModel *model, NwamuiObject *object);
static void ncu_pri_treeview_remove(GtkTreeView *treeview, GtkTreeModel *model, NwamuiObject *object);


G_DEFINE_TYPE_EXTENDED (NwamNetConfPanel,
  nwam_net_conf_panel,
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
nwam_net_conf_panel_class_init(NwamNetConfPanelClass *klass)
{
	/* Pointer to GObject Part of Class */
	GObjectClass *gobject_class = (GObjectClass*) klass;
    gobject_class->set_property = net_conf_set_property;
    gobject_class->get_property = net_conf_get_property;

    g_type_class_add_private (klass, sizeof(NwamNetConfPanelPrivate));

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

static void
net_conf_set_property ( GObject         *object,
  guint            prop_id,
  const GValue    *value,
  GParamSpec      *pspec)
{
    NwamNetConfPanel*       self = NWAM_NET_CONF_PANEL(object);

    switch (prop_id) {
    case PROP_SELECTED_NCU: {
        if (self->prv->selected_ncu) {
            g_signal_handlers_disconnect_by_func (self->prv->selected_ncu,
              (gpointer)ncu_notify_cb,
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

static void
net_conf_get_property ( GObject         *object,
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
nwam_compose_tree_view (NwamNetConfPanel *self)
{
	GtkTreeViewColumn *col;
	GtkCellRenderer *cell;
	GtkTreeView *view = self->prv->net_conf_treeview;

	g_object_set (view,
      "headers-clickable", FALSE,
      "headers-visible", FALSE,
      "rules-hint", FALSE,
      "reorderable", FALSE,
      "enable-search", FALSE,
      "show-expanders", TRUE,
      "level-indentation", 40,
      NULL);

	g_signal_connect(view,
      "row-activated",
      (GCallback)nwam_net_pref_connection_view_row_activated_cb,
      (gpointer)self);

    g_signal_connect(gtk_tree_view_get_selection(view),
      "changed",
      G_CALLBACK(selection_changed),
      (gpointer)self);

    gtk_tree_selection_set_select_function(gtk_tree_view_get_selection(view),
      selection_func,
      (gpointer)self,
      NULL);

	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(view),
      GTK_SELECTION_MULTIPLE);

	/* column info */
    col = capplet_column_new(view,
      "title",_("Details"),
      "expand", TRUE,
      "resizable", TRUE,
      "clickable", TRUE,
      "sort-indicator", FALSE,
      "reorderable", FALSE,
      NULL);
    g_object_set_data (G_OBJECT (col), TREEVIEW_COLUMN_NUM, GINT_TO_POINTER (CONNVIEW_INFO));

    cell = capplet_column_append_cell(col,
      gtk_cell_renderer_text_new(), TRUE,
      (GtkTreeCellDataFunc)nwam_net_conf_update_status_cell_cb, (gpointer)0,
      NULL);

    g_object_set_data (G_OBJECT (cell), TREEVIEW_COLUMN_NUM, GINT_TO_POINTER (CONNVIEW_INFO));
    g_signal_connect (cell, "edited", G_CALLBACK(vanity_name_edited), (gpointer)self);
    g_signal_connect (cell, "editing-started", G_CALLBACK(vanity_name_editing_started), (gpointer)self);

    /* Connection treeview of Edit Connections Dialog. */
    view = self->prv->connection_treeview;

	g_object_set (view,
      "headers-visible", FALSE,
      "reorderable", FALSE,
      "enable-search", FALSE,
      "show-expanders", FALSE,
      NULL);

	/* connection column */
    col = capplet_column_new(view,
      "title",_(""),
      "expand", TRUE,
      "resizable", FALSE,
      NULL);

    cell = capplet_column_append_cell(col,
      gtk_cell_renderer_toggle_new(), FALSE,
      nwam_connection_cell_func, (gpointer)self,
      NULL);
    g_signal_connect(cell, "toggled",
      G_CALLBACK(nwam_connection_cell_toggled), (gpointer)self);

    cell = capplet_column_append_cell(col,
      gtk_cell_renderer_text_new(), TRUE,
      nwam_connection_cell_func, (gpointer)self,
      NULL);
}

/*
 * Filter function to decide if a given row is visible or not. unused
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
nwam_net_conf_panel_init(NwamNetConfPanel *self)
{
    NwamNetConfPanelPrivate*    prv = GET_PRIVATE(self);
    NwamuiDaemon *daemon = nwamui_daemon_get_instance();
	self->prv = prv;
	/* Iniialise pointers to important widgets */
	prv->net_conf_treeview = GTK_TREE_VIEW(nwamui_util_glade_get_widget(NET_CONF_TREEVIEW));

    prv->profile_name_combo = GTK_COMBO_BOX(nwamui_util_glade_get_widget(PROFILE_NAME_COMBO));
    prv->edit_profile_name_combo = GTK_COMBO_BOX(nwamui_util_glade_get_widget(EDIT_PROFILE_NAME_COMBO));

    prv->profile_edit_btn = GTK_BUTTON(nwamui_util_glade_get_widget(PROFILE_EDIT_BTN));	
    prv->connection_enable_btn = GTK_BUTTON(nwamui_util_glade_get_widget(CONNECTION_ENABLE_BTN));	
    prv->connection_disable_btn = GTK_BUTTON(nwamui_util_glade_get_widget(CONNECTION_DISABLE_BTN));	
    prv->connection_move_up_btn = GTK_BUTTON(nwamui_util_glade_get_widget(CONNECTION_MOVE_UP_BTN));	
    prv->connection_move_down_btn = GTK_BUTTON(nwamui_util_glade_get_widget(CONNECTION_MOVE_DOWN_BTN));	
    prv->connection_rename_btn = GTK_BUTTON(nwamui_util_glade_get_widget(CONNECTION_RENAME_BTN));	
    prv->connection_group_btn = GTK_BUTTON(nwamui_util_glade_get_widget(CONNECTION_GROUP_BTN));	

    prv->activation_mode_lbl = GTK_LABEL(nwamui_util_glade_get_widget(ACTIVATION_MODE_LBL));
    prv->connection_activation_combo = GTK_COMBO_BOX(nwamui_util_glade_get_widget(CONNECTION_ACTIVATION_COMBO));

    /* Edit Connections dialog */
    prv->edit_connections_dialog = GTK_DIALOG(nwamui_util_glade_get_widget(EDIT_CONNECTIONS_DIALOG));
    prv->connection_treeview = GTK_TREE_VIEW(nwamui_util_glade_get_widget(CONNECTION_TREEVIEW));

    g_signal_connect(prv->profile_edit_btn,
      "clicked", G_CALLBACK(profile_edit_btn_clicked), (gpointer)self);
    g_signal_connect(prv->connection_enable_btn,
      "clicked", G_CALLBACK(on_button_clicked), (gpointer)self);
    g_signal_connect(prv->connection_disable_btn,
      "clicked", G_CALLBACK(on_button_clicked), (gpointer)self);
    g_signal_connect(prv->connection_move_up_btn,
      "clicked", G_CALLBACK(on_button_clicked), (gpointer)self);
    g_signal_connect(prv->connection_move_down_btn,
      "clicked", G_CALLBACK(on_button_clicked), (gpointer)self);
    g_signal_connect(prv->connection_rename_btn,
      "clicked", G_CALLBACK(on_button_clicked), (gpointer)self);
    g_signal_connect(prv->connection_group_btn,
      "clicked", G_CALLBACK(on_button_clicked), (gpointer)self);

    /* Active Profile combo. */
	capplet_compose_combo(GTK_COMBO_BOX(prv->profile_name_combo),
      GTK_TYPE_LIST_STORE,
      G_TYPE_OBJECT,
      nwamui_object_name_cell,
      NULL,
      NULL,
      (gpointer)self,
      NULL);

    gtk_widget_hide(GTK_WIDGET(prv->profile_name_combo));
    capplet_update_model_from_daemon(gtk_combo_box_get_model(GTK_COMBO_BOX(prv->profile_name_combo)), daemon, NWAMUI_TYPE_NCP);
    gtk_widget_show(GTK_WIDGET(prv->profile_name_combo));

    /* FIXME: How about zero ncp? */
    CAPPLET_COMPOSE_NWAMUI_OBJECT_COMBO(GTK_COMBO_BOX(prv->edit_profile_name_combo), NWAM_PREF_IFACE(self));

    gtk_widget_hide(GTK_WIDGET(prv->edit_profile_name_combo));
/*     capplet_update_model_from_daemon(gtk_combo_box_get_model(GTK_COMBO_BOX(prv->edit_profile_name_combo)), daemon, NWAMUI_TYPE_NCP); */
    /* Reuse active profile combo model */
    gtk_combo_box_set_model(GTK_COMBO_BOX(prv->edit_profile_name_combo),
      gtk_combo_box_get_model(GTK_COMBO_BOX(prv->profile_name_combo)));
    gtk_widget_show(GTK_WIDGET(prv->edit_profile_name_combo));

    capplet_compose_combo(prv->connection_activation_combo,
      GTK_TYPE_LIST_STORE,
      G_TYPE_INT,
      connection_activation_combo_cell_cb,
      NULL,
      (GCallback)connection_activation_combo_changed_cb,
      (gpointer)self,
      NULL);
    {
        GtkTreeModel      *model;
        GtkTreeModel *filter;
        GtkTreeIter   iter;
        int i;

        model = gtk_combo_box_get_model(GTK_COMBO_BOX(prv->connection_activation_combo));
        /* Clean all */
        gtk_list_store_clear(GTK_LIST_STORE(model));

        filter = gtk_tree_model_filter_new(model, NULL);

        gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(filter),
          connection_activation_combo_filter,
          (gpointer)self, NULL);

        gtk_widget_hide(GTK_WIDGET(prv->connection_activation_combo));
        gtk_combo_box_set_model(GTK_COMBO_BOX(prv->connection_activation_combo), filter);
        gtk_widget_show(GTK_WIDGET(prv->connection_activation_combo));
        g_object_unref(filter);

        for (i = 0; combo_contents[i]; i++) {
            gtk_list_store_append(GTK_LIST_STORE(model), &iter);
            gtk_list_store_set (GTK_LIST_STORE(model), &iter,
              0, i,
              -1);
        }
    }
    g_signal_connect(prv->connection_activation_combo, "changed", G_CALLBACK(conditions_connected_changed),(gpointer)self);

    nwam_compose_tree_view(self);

    CAPPLET_COMPOSE_NWAMUI_OBJECT_TREE_VIEW(prv->net_conf_treeview);

	g_signal_connect(G_OBJECT(self), "notify", (GCallback)object_notify_cb, NULL);

    /* Initially refresh self */
    {
        NwamuiNcp *ncp = nwamui_daemon_get_active_ncp(daemon);
        if (ncp) {
            nwam_pref_refresh(NWAM_PREF_IFACE(self), ncp, TRUE);
            g_object_unref(ncp);
        }
    }

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
 * refresh:
 *
 * Refresh #NwamNetConfPanel with the new connections.
 **/
static gboolean
refresh(NwamPrefIFace *iface, gpointer user_data, gboolean force)
{
    NwamNetConfPanel*    self = NWAM_NET_CONF_PANEL( iface );
    NwamNetConfPanelPrivate*    prv = GET_PRIVATE(iface);
    NwamuiObject *combo_object;
    NwamuiNcp *old_ncp = prv->selected_ncp;


	/* data could be null or prv->selected_ncp */
    if (user_data != NULL) {
        GtkTreeModel *model;

        {
            NwamuiDaemon *daemon = nwamui_daemon_get_instance();
            NwamuiNcp *active_ncp = nwamui_daemon_get_active_ncp(daemon);
            /* Update active ncp combo. */
            capplet_combo_set_active_object(prv->profile_name_combo, G_OBJECT(active_ncp));
            g_object_unref(active_ncp);
            g_object_unref(daemon);
        }

        /* Update ncp combo first if the selected ncp isn't the same to what we
         * are refreshing */
        combo_object = (NwamuiObject *)capplet_combo_get_active_object(prv->edit_profile_name_combo);
        /* Safely unref */
        if (combo_object)
            g_object_unref(combo_object);
        if (combo_object != user_data) {
            capplet_combo_set_active_object(prv->edit_profile_name_combo, G_OBJECT(user_data));
            /* Will trigger refresh again, so return */
            return(TRUE);
        }

        prv->selected_ncp = NWAMUI_NCP(user_data);
        if (old_ncp != NWAMUI_NCP(user_data) || force) {

            g_signal_handlers_disconnect_matched(old_ncp,
              G_SIGNAL_MATCH_DATA,
              0,
              NULL,
              NULL,
              NULL,
              (gpointer)self);

            model = gtk_tree_view_get_model(prv->net_conf_treeview);

            gtk_widget_hide(GTK_WIDGET(prv->net_conf_treeview));

            gtk_tree_store_clear(GTK_TREE_STORE(model));

            {
                GList *obj_list = nwamui_ncp_get_ncu_list(prv->selected_ncp);

                while (obj_list) {
                    ncu_pri_treeview_add(prv->net_conf_treeview, model, NWAMUI_OBJECT(obj_list->data));
                    g_object_unref(obj_list->data);
                    obj_list = g_list_delete_link(obj_list, obj_list);
                }
                /* Check if all or none group have nodes. */
                ncu_pri_treeview_add(prv->net_conf_treeview, model, NULL);
                /* Update hash, collapse group id. */
                ncu_pri_group_update(model);
            }

            gtk_widget_show(GTK_WIDGET(prv->net_conf_treeview));
/*             prv->on_child_num = 0; */
/*             prv->off_child_num = 0; */
/*             prv->child_num = 0; */
/*             prv->on_group_num = 0; */
/*             prv->off_group_num = 0; */
/*             prv->group_num = 0; */

            g_signal_connect(prv->selected_ncp, "add_ncu",
              G_CALLBACK(ncp_add_ncu), (gpointer) self);

            g_signal_connect(prv->selected_ncp, "remove_ncu",
              G_CALLBACK(ncp_remove_ncu), (gpointer) self);

            /* Update widgets */
            selection_changed(gtk_tree_view_get_selection(prv->net_conf_treeview), (gpointer)self);

            if (nwamui_ncp_is_modifiable(prv->selected_ncp)) {
                gtk_widget_set_sensitive(GTK_WIDGET(prv->net_conf_treeview), TRUE);
                gtk_widget_set_sensitive(GTK_WIDGET(prv->profile_edit_btn), TRUE);
            } else {
                gtk_widget_set_sensitive(GTK_WIDGET(prv->net_conf_treeview), FALSE);
                gtk_widget_set_sensitive(GTK_WIDGET(prv->profile_edit_btn), FALSE);
            }
        }
    }
    
    if (force) {
    }

    return( TRUE );
}

static gboolean
apply(NwamPrefIFace *iface, gpointer user_data)
{
    NwamNetConfPanel*    self = NWAM_NET_CONF_PANEL( iface );
    NwamNetConfPanelPrivate*    prv = GET_PRIVATE(iface);
    NwamuiObject *combo_object;

    {
        NwamuiDaemon *daemon = nwamui_daemon_get_instance();
        /* Set active ncp combo. */
        combo_object = (NwamuiObject *)capplet_combo_get_active_object(prv->profile_name_combo);
        nwamui_daemon_set_active_ncp(daemon, NWAMUI_NCP(combo_object));

        g_object_unref(combo_object);
        g_object_unref(daemon);
    }
}

static gboolean
help(NwamPrefIFace *iface, gpointer user_data)
{
    g_debug("NwamNetConfPanel: Help");
    nwamui_util_show_help (HELP_REF_NETWORKPROFILE_VIEW);
}

static void
nwam_net_conf_panel_finalize(NwamNetConfPanel *self)
{
    g_object_unref(G_OBJECT(self->prv->profile_edit_btn));	
    g_object_unref(G_OBJECT(self->prv->connection_enable_btn));	
    g_object_unref(G_OBJECT(self->prv->connection_disable_btn));	
    g_object_unref(G_OBJECT(self->prv->connection_move_up_btn));	
    g_object_unref(G_OBJECT(self->prv->connection_move_down_btn));	
    g_object_unref(G_OBJECT(self->prv->connection_rename_btn));	
    g_object_unref(G_OBJECT(self->prv->connection_group_btn));	

    if (self->prv->selected_ncu) {
        g_object_unref (self->prv->selected_ncu);
    }

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
nwam_net_conf_update_status_cell_cb (GtkTreeViewColumn *col,
  GtkCellRenderer   *renderer,
  GtkTreeModel      *model,
  GtkTreeIter       *iter,
  gpointer           data)
{
    NwamuiNcu*                     ncu      = NULL;
    gboolean                       is_group = FALSE;
    
    gtk_tree_model_get(model, iter, 0, &ncu, -1);

    if (ncu) {
        if (NWAMUI_IS_NCU(ncu)) {
            is_group = FALSE;
        } else if (ncu == (gpointer)fake_object_in_pri_group) {
            g_object_set(G_OBJECT(renderer),
              "sensitive", FALSE,
              "markup", _("<b><i>(None)</i></b>"),
              NULL);
            g_object_unref(ncu);
            return;
        } else if (NWAMUI_IS_OBJECT(ncu)) {
            is_group = TRUE;
        } else
            g_assert_not_reached();
    } else
        return;
    
    switch (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (renderer), TREEVIEW_COLUMN_NUM))) {
    case CONNVIEW_INFO: {
        if (!is_group) {
            gchar*                         ncu_text;
            gchar*                         ncu_markup;
            gboolean                       ncu_enabled;
            gboolean                       ncu_is_dhcp;
            gchar*                         ncu_ipv4_addr;
            gchar*                         ncu_phy_addr;
            gchar*                         info_string;

            ncu_text = nwamui_ncu_get_display_name(ncu);
            ncu_enabled = nwamui_ncu_get_enabled(ncu);
            ncu_is_dhcp = nwamui_ncu_get_ipv4_auto_conf(ncu);
            ncu_ipv4_addr = nwamui_ncu_get_ipv4_address(ncu);
            ncu_phy_addr = nwamui_ncu_get_phy_address(ncu);

            if ( ! ncu_enabled ) {
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
              "sensitive", TRUE,
              "markup", ncu_markup,
              NULL);

            if ( ncu_ipv4_addr != NULL )
                g_free( ncu_ipv4_addr );

            if ( ncu_phy_addr != NULL )
                g_free( ncu_phy_addr );

            g_free (info_string);
            g_free( ncu_markup );
            g_free( ncu_text );
        } else {
            gchar *str = NULL;
            gint    group_id = 0;

            group_id = (gint)g_object_get_data(G_OBJECT(ncu), NWAM_COND_NCU_GROUP_ID);

            if (group_id == ALWAYS_ON_GROUP_ID) {
                str = _("Always enable these connections:");
            } else if (group_id == ALWAYS_OFF_GROUP_ID) {
                str = _("Always disable these connections:");
            } else {
                GtkTreePath *first_path;
                GtkTreePath *second_path;
                gint    group_mode = 0;
                gboolean    first_group_id;

                group_mode = (gint)g_object_get_data(G_OBJECT(ncu), NWAM_COND_NCU_GROUP_MODE);

                first_path = ncu_pri_group_get_path(model, ALWAYS_ON_GROUP_ID);

                second_path = gtk_tree_model_get_path(model, iter);
                gtk_tree_path_prev(second_path);

                if (gtk_tree_path_compare(first_path, second_path) == 0) {
                    first_group_id = TRUE;
                } else {
                    first_group_id = FALSE;
                }

                switch (group_mode) {
                case NWAMUI_COND_PRIORITY_GROUP_MODE_EXCLUSIVE:
                    if ( first_group_id ) {
                        str = _("Then enable exactly one of these connections:");
                    }
                    else {
                        str = _("Else enable exactly one of these connections:");
                    }
                    break;
                case NWAMUI_COND_PRIORITY_GROUP_MODE_SHARED:
                    if ( first_group_id ) {
                        str = _("Then enable one or more of these connections:");
                    }
                    else {
                        str = _("Else enable one or more of these connections:");
                    }
                    break;
                case NWAMUI_COND_PRIORITY_GROUP_MODE_ALL:
                    if ( first_group_id ) {
                        str = _("Then enable all of these connections:");
                    }
                    else {
                        str = _("Else enable all of these connections:");
                    }
                    break;
                default:
                    g_assert_not_reached();
                    break;
                }
            }

#if 0
            g_object_set(G_OBJECT(renderer),
              "sensitive", TRUE,
              "markup", str ? str:"TODO",
              NULL);
#else
            {
                gchar *id;
                gint    group_id = 0;
                group_id = (gint)g_object_get_data(G_OBJECT(ncu), NWAM_COND_NCU_GROUP_ID);
                id = g_strdup_printf("%s: %d", str ? str:"TODO", group_id);
                g_object_set(G_OBJECT(renderer),
                  "sensitive", TRUE,
                  "markup", id,
                  NULL);
                g_free(id);
            }
#endif
        }
    }
        break;
            
    default:
        g_assert_not_reached();
    }
    
    if ( ncu != NULL ) {
        g_object_unref(G_OBJECT(ncu));
    }

}

static void
nwam_connection_cell_func (GtkTreeViewColumn *col,
  GtkCellRenderer   *renderer,
  GtkTreeModel      *model,
  GtkTreeIter       *iter,
  gpointer           data)
{
    NwamNetConfPanelPrivate*    prv = GET_PRIVATE(data);
    NwamuiObject *obj;
    gchar *name;

    gtk_tree_model_get(model, iter, 0, &obj, -1);

    name = nwamui_object_get_name(obj);

    if (GTK_IS_CELL_RENDERER_TOGGLE(renderer)) {
        GList *ncus = prv->ncu_selection;
        gchar *ncu_name;
        gboolean hit = FALSE;

        while (ncus && !hit) {
            ncu_name = nwamui_object_get_name(NWAMUI_OBJECT(ncus->data));

            if (g_ascii_strcasecmp(name, ncu_name) == 0) {
                hit = TRUE;
            }
            ncus = g_list_next(ncus);
            g_free(ncu_name);
        }
        g_object_set(renderer, "active", hit, NULL);
    } else {
        g_object_set(renderer, "text", name, NULL);

    }
    g_free(name);
    g_object_unref(obj);
}

/*
 * Enabled Toggle Button was toggled
 */
static void
nwam_net_pref_connection_enabled_toggled_cb(    GtkCellRendererToggle *cell_renderer,
                                                gchar                 *path,
                                                gpointer               user_data) 
{
  	NwamNetConfPanel*       self = NWAM_NET_CONF_PANEL(user_data);
    GtkTreeIter iter;
    GtkTreeModel *model;
    GtkTreePath  *tpath;

	model = gtk_tree_view_get_model(self->prv->net_conf_treeview);
    tpath = gtk_tree_path_new_from_string(path);
    if (tpath != NULL && gtk_tree_model_get_iter (model, &iter, tpath))
    {
        gpointer    connection;
        NwamuiNcu*  ncu;
        gboolean	inconsistent;

        gtk_tree_model_get(model, &iter, 0, &connection, -1);

        if (NWAMUI_IS_NCU( connection )) {
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
  	NwamNetConfPanel*       self = NWAM_NET_CONF_PANEL(data);

    g_debug("Editing Started");

    g_object_set (cell, "editable", FALSE, NULL);

    if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT(cell), TREEVIEW_COLUMN_NUM)) != CONNVIEW_INFO) {
        return;
    }
    if (GTK_IS_ENTRY (editable)) {
        GtkEntry *entry = GTK_ENTRY (editable);
        GtkTreeIter iter;
        GtkTreeModel *model = gtk_tree_view_get_model(self->prv->net_conf_treeview);
        GtkTreePath  *tpath;

        if ( (tpath = gtk_tree_path_new_from_string(path)) != NULL 
              && gtk_tree_model_get_iter (model, &iter, tpath))
        {
            gpointer    connection;
            NwamuiNcu*  ncu;
            gchar*      vname;

            gtk_tree_model_get(model, &iter, 0, &connection, -1);

            if (NWAMUI_IS_NCU( connection )) {
                ncu  = NWAMUI_NCU( connection );

                vname = nwamui_ncu_get_vanity_name(ncu);
            
                gtk_entry_set_text( entry, vname?vname:"" );

                g_free( vname );
            
            }
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
  	NwamNetConfPanel*       self = NWAM_NET_CONF_PANEL(data);
    GtkTreeModel *model;
    GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
    GtkTreeIter iter;

    gint column = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (cell), TREEVIEW_COLUMN_NUM));

	model = gtk_tree_view_get_model(self->prv->net_conf_treeview);
    if ( gtk_tree_model_get_iter (model, &iter, path))
    switch (column) {
        case CONNVIEW_INFO:      {
            gpointer    connection;
            NwamuiNcu*  ncu;
            gtk_tree_model_get(model, &iter, 0, &connection, -1);

            if (NWAMUI_IS_NCU( connection )) {
                ncu  = NWAMUI_NCU( connection );

                nwamui_ncu_set_vanity_name(ncu, new_text);
            }
            g_object_unref(G_OBJECT(connection));

          }
          break;
        default:
            g_assert_not_reached();
    }

    gtk_tree_path_free (path);

    g_object_set (cell, "editable", FALSE, NULL);
}

static void
nwam_connection_cell_toggled(GtkCellRendererToggle *cell_renderer,
  gchar                 *path_str,
  gpointer               user_data)
{
    NwamNetConfPanelPrivate*    prv = GET_PRIVATE(user_data);
    GtkTreeIter iter;
    GtkTreeModel *model;
    GtkTreePath  *path;
    GList *ncus = prv->ncu_selection;
    NwamuiNcu*  ncu;
    gchar *name;
    gchar *ncu_name;
    GList *hit = NULL;

    model = gtk_tree_view_get_model(prv->connection_treeview);
    path = gtk_tree_path_new_from_string(path_str);
    gtk_tree_model_get_iter (model, &iter, path);
    gtk_tree_model_get(model, &iter, 0, &ncu, -1);

    name = nwamui_object_get_name(NWAMUI_OBJECT(ncu));
    while (ncus && !hit) {
        ncu_name = nwamui_object_get_name(NWAMUI_OBJECT(ncus->data));
        if (g_ascii_strcasecmp(name, ncu_name) == 0) {
            hit = ncus;
        }
        ncus = g_list_next(ncus);
        g_free(ncu_name);
    }
    g_free(name);

    if (gtk_cell_renderer_toggle_get_active(cell_renderer)) {
        if (hit) {
            g_object_unref(hit->data);
            prv->ncu_selection = g_list_delete_link(prv->ncu_selection, hit);
        }
    } else {
        if (!hit) {
            /* TODO, maybe we need clone this ncu? */
            g_object_ref(ncu);
            prv->ncu_selection = g_list_prepend(prv->ncu_selection, ncu);
        }
    }
    gtk_tree_model_row_changed(model, path, &iter);
    g_object_unref(ncu);
    gtk_tree_path_free(path);
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

    if (columnid == CONNVIEW_INFO) {
        if (gtk_tree_model_get_iter (model, &iter, path)) {
            NwamuiNcu*  ncu;

            gtk_tree_model_get(model, &iter, 0, &ncu, -1);

            nwam_capplet_dialog_select_ncu(self->prv->pref_dialog, ncu );

            g_object_unref(ncu);
        }
    }
}

static void
update_rules_from_ncu (NwamNetConfPanel* self,
  GParamSpec *arg1,
  gpointer data)
{
    NwamNetConfPanelPrivate*    prv = GET_PRIVATE(self);
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

    if ( ncu != NULL/*  &&  */
/*          nwamui_ncu_get_selection_rule(ncu, &ncu_list, &ncu_state, &action) */ ) {
        /* Rules are Enabled */
        gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_activation_combo), TRUE);

        gtk_combo_box_set_active(GTK_COMBO_BOX(prv->connection_activation_combo), 
                                 /* ncu_state == NWAMUI_NCU_RULE_STATE_IS_NOT_CONNECTED?1: */0 );

    }
    else {
        gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_activation_combo), FALSE);;
    }

    prv->update_inprogress = FALSE;

}

static void
nwam_net_pref_rule_ncu_enabled_toggled_cb  (    GtkCellRendererToggle *cell_renderer,
                                                gchar                 *path,
                                                gpointer               user_data) 
{
#if 0
	NwamNetConfPanel*           self = NWAM_NET_CONF_PANEL(user_data);
    NwamNetConfPanelPrivate*    prv = GET_PRIVATE(self);
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

/**
 * capplet_tree_store_move_children:
 * Note only move children. If source has children, then move it, else move source.
 */
void
capplet_tree_store_move_children(GtkTreeStore *model,
    GtkTreeIter *target,
    GtkTreeIter *source,
    GtkTreeModelForeachFunc func,
    gpointer user_data)
{
	GtkTreeIter t_iter;
	GtkTreeIter s_iter;
	GtkTreeIter iter;
    GtkTreeIter temp_iter;

	if (gtk_tree_model_iter_children(GTK_TREE_MODEL(model), &s_iter, source)) {
		do {
			/* Take care if there are customer monitoring
			 * row-add/delete signals */
            /* TODO consider the position. */
            if (!gtk_tree_model_iter_children(GTK_TREE_MODEL(model), &t_iter, target)) {
                /* Think about the target position? */
                CAPPLET_TREE_STORE_ADD(model, target, NULL, &t_iter);
                TREE_STORE_CP_OBJECT(model, &t_iter, &s_iter);
            } else {

                TREE_STORE_CP_OBJECT_AFTER(model, &s_iter, target, &t_iter, &temp_iter);
            }
		} while (gtk_tree_store_remove(GTK_TREE_STORE(model), &s_iter));
	} else {
        if (!gtk_tree_model_iter_children(GTK_TREE_MODEL(model), &t_iter, target)) {
            /* Think about the target position? */
            CAPPLET_TREE_STORE_ADD(model, target, NULL, &t_iter);
            TREE_STORE_CP_OBJECT(model, &t_iter, source);
        } else {

            TREE_STORE_CP_OBJECT_AFTER(model, source, target, &t_iter, &temp_iter);
        }
        gtk_tree_store_remove(GTK_TREE_STORE(model), source);
    }
}

static GtkTreeRowReference*
ref_ncu_group_node(GtkTreeModel *model, GtkTreePath *path)
{
    GtkTreeRowReference *group_rr;
    GtkTreePath *parent_path;

    g_return_val_if_fail(model && path, NULL);

    /* Monitor the group node. */
    if (gtk_tree_path_get_depth(path) == 2) {
        parent_path = gtk_tree_path_copy(path);
        gtk_tree_path_up(parent_path);
    } else {
        parent_path = gtk_tree_path_copy(path);
    }
    group_rr = gtk_tree_row_reference_new(model, parent_path);
    gtk_tree_path_free(parent_path);
    return group_rr;
}

static void
handle_ncu_group_node(GtkTreeModel *model, GtkTreeRowReference *group_rr)
{
    GtkTreePath *parent_path;
    GtkTreeIter parent_iter;
    gboolean need_fake_node;

    g_assert(model && group_rr && gtk_tree_row_reference_valid(group_rr));

    parent_path = gtk_tree_row_reference_get_path(group_rr);

    if (gtk_tree_path_compare(parent_path,
        ncu_pri_group_get_path(model, ALWAYS_ON_GROUP_ID)) == 0 ||
      gtk_tree_path_compare(parent_path,
        ncu_pri_group_get_path(model, ALWAYS_OFF_GROUP_ID)) == 0) {
        need_fake_node = TRUE;
    } else {
        need_fake_node = FALSE;
    }
    gtk_tree_model_get_iter(model, &parent_iter, parent_path);
    if (!gtk_tree_model_iter_has_child(model, &parent_iter)) {
        if (need_fake_node) {
            GtkTreeIter temp;
            CAPPLET_TREE_STORE_ADD(model, &parent_iter, fake_object_in_pri_group, &temp);
        } else {
            gtk_tree_store_remove(GTK_TREE_STORE(model), &parent_iter);
        }
    }
}

static void
on_button_clicked(GtkButton *button, gpointer user_data)
{
    NwamNetConfPanel*         self          = NWAM_NET_CONF_PANEL(user_data);
    NwamNetConfPanelPrivate*  prv           = GET_PRIVATE(self);
    GtkTreeSelection         *selection     = gtk_tree_view_get_selection(prv->net_conf_treeview);
    GtkTreeModel*             model;
    gint                      count_selected_rows;
    GList*                    rows;
    gint                      on_group_num  = prv->on_group_num;
    gint                      off_group_num = prv->off_group_num;
    gint                      group_num     = prv->group_num;
    gint                      on_child_num  = prv->on_child_num;
    gint                      off_child_num = prv->off_child_num;
    gint                      child_num     = prv->child_num;

    count_selected_rows = gtk_tree_selection_count_selected_rows(selection);
    rows = gtk_tree_selection_get_selected_rows(selection, &model);

    if (count_selected_rows == 0) {
        return;
    }

    /* Group button */
    if (button == (gpointer)prv->connection_group_btn) {

        GList *rowrefs = NULL;
        GtkTreeIter parent;
        gboolean is_new_group;

        is_new_group = (gboolean)g_object_get_data(G_OBJECT(prv->connection_group_btn), BUTTON_IS_GROUP_NEW);

        for (; rows; rows = g_list_delete_link(rows, rows)) {
            rowrefs = g_list_prepend(rowrefs, gtk_tree_row_reference_new(model, (GtkTreePath*)rows->data));
            gtk_tree_path_free((GtkTreePath*)rows->data);
        }

        if (is_new_group) {
            gint group_id;

            /* New group */
            ncu_pri_group_get_iter(model, ALWAYS_OFF_GROUP_ID - 1, NWAMUI_COND_PRIORITY_GROUP_MODE_EXCLUSIVE, &parent);
            ncu_pri_group_update(model);
        } else {
            GtkTreeIter iter;
            NwamuiObject *obj;
            /* Delete group */
            ncu_pri_group_get_iter(model, ALWAYS_OFF_GROUP_ID, 0, &parent);
            if (gtk_tree_model_iter_children(GTK_TREE_MODEL(model), &iter, &parent)) {
                gtk_tree_model_get(model, &iter, 0, &obj, -1);
                if (obj == (gpointer)fake_object_in_pri_group) {
                    g_object_unref(obj);
                    gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);
                } else {
                    g_object_unref(obj);
                }
            }
        }

        for (; rowrefs; rowrefs = g_list_delete_link(rowrefs, rowrefs)) {
            GtkTreeIter iter;
            GtkTreePath *path;
            GtkTreeRowReference *group_rr = NULL;

            path = gtk_tree_row_reference_get_path((GtkTreeRowReference*)rowrefs->data);
            /* Monitor the group node. */
            group_rr = ref_ncu_group_node(model, path);

            gtk_tree_model_get_iter(model, &iter, path);

            /* Only copy non-group nodes. */
            capplet_tree_store_move_children(GTK_TREE_STORE(model),
              &parent,
              &iter,
              NULL,
              NULL);

            /* Handle group node. */
            handle_ncu_group_node(model, group_rr);
            gtk_tree_row_reference_free(group_rr);
            gtk_tree_row_reference_free((GtkTreeRowReference*)rowrefs->data);
        }

        gtk_tree_selection_unselect_all(selection);
        gtk_tree_view_expand_all(prv->net_conf_treeview);

        update_widgets(self, gtk_tree_view_get_selection(prv->net_conf_treeview));
        return;
        /* Group button end */

    }

    if (count_selected_rows == 1) {
        NwamuiObject   *object;
        GtkTreePath    *path = (GtkTreePath *)rows->data;
        GtkTreeIter    iter;

        gtk_tree_model_get_iter(model, &iter, path);
        gtk_tree_model_get(model, &iter, 0, &object, -1);
 
        if (button == (gpointer)prv->connection_move_up_btn) {
            GtkTreeIter new_group_iter;

            if ( gtk_tree_path_up(path) ) { 
                /* If it's got a parent, then we need to move up a group,
                 * and put the NCU there
                 */

                /* See if we can go up one */
                if ( gtk_tree_path_prev(path) ) { 
                    /* If so, we move the obect, and change it's settings */

                    gtk_tree_model_get_iter(model, &new_group_iter, path);

                    capplet_tree_store_merge_children(GTK_TREE_STORE(model),
                      &new_group_iter,
                      &iter,
                      NULL,
                      NULL);

                    gtk_tree_view_expand_row(prv->net_conf_treeview, path, TRUE);
                }
            }

            update_widgets(self, gtk_tree_view_get_selection(prv->net_conf_treeview));

        } else if (button == (gpointer)prv->connection_move_down_btn) {
            GtkTreeIter new_group_iter;

            if ( gtk_tree_path_up(path) ) { 
                /* If it's got a parent, then we need to move up a group,
                 * and put the NCU there
                 */

                /* See if we can go down one */
                gtk_tree_path_next(path);
                    
                /* If so, we move the obect, and change it's settings */

                gtk_tree_model_get_iter(model, &new_group_iter, path);

                capplet_tree_store_merge_children(GTK_TREE_STORE(model),
                  &new_group_iter,
                  &iter,
                  NULL,
                  NULL);

                gtk_tree_view_expand_row(prv->net_conf_treeview, path, TRUE);
            }

            update_widgets(self, gtk_tree_view_get_selection(prv->net_conf_treeview));
        } else if (button == (gpointer)prv->connection_rename_btn) {
            GtkCellRendererText*        txt;
            GtkTreeViewColumn*  info_col = gtk_tree_view_get_column( GTK_TREE_VIEW(self->prv->net_conf_treeview), CONNVIEW_INFO );
            GList*              renderers = gtk_tree_view_column_get_cell_renderers( info_col );
        
            /* Should be only one renderer */
            g_assert( g_list_next( renderers ) == NULL );
        
            if ((txt = GTK_CELL_RENDERER_TEXT(g_list_first( renderers )->data)) != NULL) {
                GtkTreePath*    tpath = gtk_tree_model_get_path(model, &iter);
                g_object_set (txt, "editable", TRUE, NULL);

                gtk_tree_view_set_cursor (GTK_TREE_VIEW(self->prv->net_conf_treeview), tpath, info_col, TRUE);

                gtk_tree_path_free(tpath);
            }
        } else if (button == (gpointer)prv->connection_enable_btn ||
          button == (gpointer)prv->connection_disable_btn) {
            GtkTreeIter parent;
            GtkTreeIter temp;
            NwamuiObject *obj;
            GtkTreeRowReference *group_rr;

            gtk_tree_selection_unselect_path(selection, path);

            group_rr = ref_ncu_group_node(model, path);

            ncu_pri_group_get_iter(model,
              button == (gpointer)prv->connection_enable_btn?ALWAYS_ON_GROUP_ID:ALWAYS_OFF_GROUP_ID, 0, &parent);

            /* Must have at least one node. */
            if (gtk_tree_model_iter_children(GTK_TREE_MODEL(model), &temp, &parent)) {
                gtk_tree_model_get(model, &temp, 0, &obj, -1);
                if (obj == (gpointer)fake_object_in_pri_group) {
                    /* Overwrite */
                    TREE_STORE_CP_OBJECT(model, &temp, &iter);

                    gtk_tree_selection_select_iter(selection, &temp);
                } else {
                    GtkTreeIter pos;
                    /* Insert, pos? */
                    TREE_STORE_CP_OBJECT_AFTER(model, &iter, &parent, &temp, &pos);
                    gtk_tree_selection_select_iter(selection, &pos);
                }
                g_object_unref(obj);

                gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);
            }
            handle_ncu_group_node(model, group_rr);
            gtk_tree_row_reference_free(group_rr);
            gtk_tree_view_expand_all(prv->net_conf_treeview);

        } else if (button == (gpointer)prv->connection_rename_btn) {
            GtkCellRendererText*        txt;
            GtkTreeViewColumn*  info_col = gtk_tree_view_get_column( GTK_TREE_VIEW(self->prv->net_conf_treeview), CONNVIEW_INFO );
            GList*              renderers = gtk_tree_view_column_get_cell_renderers( info_col );
        
            /* Should be only one renderer */
            g_assert( g_list_next( renderers ) == NULL );
        
            if ((txt = GTK_CELL_RENDERER_TEXT(g_list_first( renderers )->data)) != NULL) {
                GtkTreePath*    tpath = gtk_tree_model_get_path(model, &iter);
                g_object_set (txt, "editable", TRUE, NULL);

                gtk_tree_view_set_cursor (GTK_TREE_VIEW(self->prv->net_conf_treeview), tpath, info_col, TRUE);

                gtk_tree_path_free(tpath);
            }
        }

        g_object_unref(object);
    } else if ( 0 && count_selected_rows == 1) {
        NwamuiObject *object;
        GtkTreeIter    iter;
        GtkTreePath    *path = (GtkTreePath *)rows->data;

        gtk_tree_model_get_iter(model, &iter, path);
        gtk_tree_model_get(model, &iter, 0, &object, -1);
 

        if (button == (gpointer)prv->connection_move_up_btn) {
            GtkTreeIter prev_iter;
            GtkTreeIter parent_iter;
            GtkTreeIter    iter;
            GtkTreePath    *path = (GtkTreePath *)rows->data;

            gtk_tree_model_get_iter(model, &iter, path);

            if ( gtk_tree_path_prev(path) ) { /* See if we have somewhere to move up to... */
                gtk_tree_model_get_iter(model, &prev_iter, path);
                if (!gtk_tree_model_iter_children(GTK_TREE_MODEL(model), &parent_iter, &prev_iter)) {
                    gtk_tree_store_move_before(GTK_TREE_STORE(model), &iter, &prev_iter );
                    gtk_tree_selection_select_iter(selection, &iter);
                } else {
                    parent_iter = prev_iter;
                    gtk_tree_store_append(GTK_TREE_STORE(model),
                      &prev_iter,
                      &parent_iter);
                    capplet_tree_store_move_object(GTK_TREE_MODEL(model),
                      &prev_iter, &iter);
                    capplet_tree_view_expand_row(prv->net_conf_treeview,
                      &parent_iter, TRUE);
                    gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);
                    gtk_tree_selection_select_iter(selection, &prev_iter);
                }
            } else if (gtk_tree_model_iter_parent(GTK_TREE_MODEL(model), &parent_iter, &iter)) {
                gtk_tree_store_insert_before(GTK_TREE_STORE(model),
                  &prev_iter,
                  NULL,
                  &parent_iter);
                capplet_tree_store_move_object(GTK_TREE_MODEL(model),
                  &prev_iter, &iter);
                gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);
                if (!gtk_tree_model_iter_has_child(GTK_TREE_MODEL(model), &parent_iter))
                    gtk_tree_store_remove(GTK_TREE_STORE(model), &parent_iter);
                gtk_tree_selection_select_iter(selection, &prev_iter);
            }
            update_widgets(self, gtk_tree_view_get_selection(prv->net_conf_treeview));
        } else if (button == (gpointer)prv->connection_move_down_btn) {
            GtkTreeIter next_iter = iter;
            GtkTreeIter parent_iter;
        
            if ( gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &next_iter) ) { /* See if we have somewhere to move down to... */
                if (!gtk_tree_model_iter_has_child(GTK_TREE_MODEL(model), &next_iter)) {
                    gtk_tree_store_move_after(GTK_TREE_STORE(model), &iter, &next_iter );
                    gtk_tree_selection_select_iter(selection, &iter);
                } else {
                    parent_iter = next_iter;
                    gtk_tree_store_prepend(GTK_TREE_STORE(model),
                      &next_iter,
                      &parent_iter);
                    capplet_tree_store_move_object(GTK_TREE_MODEL(model),
                      &next_iter, &iter);
                    capplet_tree_view_expand_row(prv->net_conf_treeview,
                      &parent_iter, TRUE);
                    gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);
                    gtk_tree_selection_select_iter(selection, &next_iter);
                }
            } else if (gtk_tree_model_iter_parent(GTK_TREE_MODEL(model), &parent_iter, &iter)) {
                gtk_tree_store_insert_after(GTK_TREE_STORE(model),
                  &next_iter,
                  NULL,
                  &parent_iter);
                capplet_tree_store_move_object(GTK_TREE_MODEL(model),
                  &next_iter, &iter);
                gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);
                if (!gtk_tree_model_iter_has_child(GTK_TREE_MODEL(model), &parent_iter))
                    gtk_tree_store_remove(GTK_TREE_STORE(model), &parent_iter);
                gtk_tree_selection_select_iter(selection, &next_iter);
            }
            update_widgets(self, gtk_tree_view_get_selection(prv->net_conf_treeview));
        }
        g_object_unref(object);
    }

    g_list_foreach (rows, (GFunc)gtk_tree_path_free, NULL);
    g_list_free (rows);

}

static void
profile_edit_btn_clicked(GtkButton *button, gpointer user_data )
{
    NwamNetConfPanelPrivate*  prv           = GET_PRIVATE(user_data);
    NwamuiDaemon *daemon = nwamui_daemon_get_instance();
    NwamuiNcp *auto_ncp;
    GtkTreeModel *model;
    gint result;

    auto_ncp = nwamui_daemon_get_ncp_by_name(daemon, NWAM_NCP_NAME_AUTOMATIC);
    model = GTK_TREE_MODEL(nwamui_ncp_get_ncu_list_store(auto_ncp));
    /* Selected ncus used for show toggle cells. */
    prv->ncu_selection = nwamui_ncp_get_ncu_list(prv->selected_ncp);

    gtk_tree_view_set_model(prv->connection_treeview, model);

    g_object_unref(model);
    g_object_unref(auto_ncp);
    g_object_unref(daemon);

    result = gtk_dialog_run (GTK_DIALOG (prv->edit_connections_dialog));
    switch (result)
    {
    case GTK_RESPONSE_OK:
        /* Todo handle the updated prv->ncu_selection */
        break;
    default:
        break;
    }
    gtk_widget_hide (prv->edit_connections_dialog);

    g_list_foreach(prv->ncu_selection, nwamui_util_obj_unref, NULL);
    g_list_free(prv->ncu_selection);
    prv->ncu_selection = NULL;
}

static void
selection_changed(GtkTreeSelection *selection, gpointer user_data)
{
    update_widgets(NWAM_NET_CONF_PANEL(user_data), selection);
}

static gboolean
selection_func(GtkTreeSelection *selection,
  GtkTreeModel *model,
  GtkTreePath *path,
  gboolean path_currently_selected,
  gpointer data)
{
    NwamNetConfPanelPrivate*    prv = GET_PRIVATE(data);
    NwamuiObject *object;
    gboolean is_child;
    GtkTreeIter parent;
    GtkTreeIter iter;
    GtkTreeIter *group_iter;
    gint group_id;
    gint inc = path_currently_selected ? -1 : 1;

    /* Hack, only reset when no selections. */
    if (gtk_tree_selection_count_selected_rows(selection) == 0) {
        prv->on_child_num = 0;
        prv->off_child_num = 0;
        prv->child_num = 0;
        prv->on_group_num = 0;
        prv->off_group_num = 0;
        prv->group_num = 0;
    }

    gtk_tree_model_get_iter(model, &iter, path);

    if (gtk_tree_model_iter_parent(model, &parent, &iter)) {

        gtk_tree_model_get(model, &iter, 0, &object, -1);
        if (object == (gpointer)fake_object_in_pri_group) {
            g_object_unref(object);
            /* Filter out the fake object. */
            return path_currently_selected;
        }
        g_object_unref(object);

        is_child = TRUE;
        group_iter = &parent;
    } else {
        is_child = FALSE;
        group_iter = &iter;
    }

    gtk_tree_model_get(model, group_iter, 0, &object, -1);
    group_id = (gint)g_object_get_data(G_OBJECT(object), NWAM_COND_NCU_GROUP_ID);
    g_object_unref(object);

    if (group_id == ALWAYS_ON_GROUP_ID || group_id == ALWAYS_OFF_GROUP_ID) {
        GtkTreeIter child_iter;
        if (gtk_tree_model_iter_children(model, &child_iter, group_iter)) {
            gtk_tree_model_get(model, &child_iter, 0, &object, -1);
            if (object == (gpointer)fake_object_in_pri_group) {
                g_object_unref(object);
                /* Filter out the fake object. */
                return path_currently_selected;
            }
            g_object_unref(object);
        }
    }

    if (is_child) {
        if (group_id == ALWAYS_ON_GROUP_ID)
            prv->on_child_num += inc;
        else if (group_id == ALWAYS_OFF_GROUP_ID)
            prv->off_child_num += inc;
        else
            prv->child_num += inc;
    } else {
        if (group_id == ALWAYS_ON_GROUP_ID)
            prv->on_group_num += inc;
        else if (group_id == ALWAYS_OFF_GROUP_ID)
            prv->off_group_num += inc;
        else
            prv->group_num += inc;
    }

    g_debug("<on,off,normal> c< %d %d %d > g< %d %d %d >\n",
      prv->on_child_num,
      prv->off_child_num,
      prv->child_num,
      prv->on_group_num,
      prv->off_group_num,
      prv->group_num);

    return TRUE;
}

static void
update_widgets(NwamNetConfPanel *self, GtkTreeSelection *selection)
{
    NwamNetConfPanelPrivate* prv           = GET_PRIVATE(self);
    GtkTreeModel*            model         = gtk_tree_view_get_model(prv->net_conf_treeview);
    gint                     count_selected_rows;
    gint                     on_group_num  = prv->on_group_num;
    gint                     off_group_num = prv->off_group_num;
    gint                     group_num     = prv->group_num;
    gint                     on_child_num  = prv->on_child_num;
    gint                     off_child_num = prv->off_child_num;
    gint                     child_num     = prv->child_num;
#if 0
    gboolean                 ncu_is_active = FALSE;;
    gboolean                 ncu_is_manual = FALSE;;
    ncu_is_active = nwamui_ncu_get_active( NWAMUI_NCU(object) );
    ncu_is_manual = ( nwamui_ncu_get_activation_mode( NWAMUI_NCU(object) ) 
      == NWAMUI_COND_ACTIVATION_MODE_MANUAL );
#endif
    gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_enable_btn), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_disable_btn), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_move_up_btn), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_move_down_btn), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_rename_btn), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_group_btn), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_activation_combo), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(prv->activation_mode_lbl), FALSE);

    if (!prv->selected_ncp || !nwamui_ncp_is_modifiable(prv->selected_ncp)) {
        return;
    }

    count_selected_rows = gtk_tree_selection_count_selected_rows(selection);

    if (count_selected_rows > 0) {
        if ((on_group_num + off_group_num == 1 ||
            on_child_num + off_child_num + child_num > 0) && group_num == 0) {
            /* Enable New Group */
            gtk_button_set_label(prv->connection_group_btn, _("New _group"));
            gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_group_btn), TRUE);
            g_object_set_data(G_OBJECT(prv->connection_group_btn), BUTTON_IS_GROUP_NEW, (gpointer)TRUE);
        } else if (group_num == count_selected_rows) {
            /* Enable Remove Group */
            gtk_button_set_label(prv->connection_group_btn, _("Remove _group"));
            gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_group_btn), TRUE);
            g_object_set_data(G_OBJECT(prv->connection_group_btn), BUTTON_IS_GROUP_NEW, (gpointer)FALSE);
        }

        if (on_child_num == 0 && on_group_num == 0) {
            /* Enable Up */
            gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_move_up_btn), TRUE);
        }

        if (off_child_num == 0 && off_group_num == 0) {
            /* Enable Down */
            gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_move_down_btn), TRUE);
        }

        if (count_selected_rows == 1) {
            if (on_child_num + child_num > 0) {
                /* Enable Rename */
                gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_rename_btn), TRUE);
                /* Enable Disable */
                gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_disable_btn), TRUE);
            }


            if (off_child_num + child_num > 0) {
                /* Enable Rename */
                gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_rename_btn), TRUE);
                /* Enable Enable */
                gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_enable_btn), TRUE);
            }

            if (on_group_num == 1) {
                gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_move_up_btn), FALSE);
            }

            if (off_group_num == 1) {
                gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_move_down_btn), FALSE);
            }

#if 0
                if (child_num == 1) {
                    /* Enable Rename */
                    gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_rename_btn), TRUE);
                    if ( ncu_is_manual ) {
                        if ( ncu_is_active ) {
                            gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_disable_btn), TRUE);
                        }
                        else {
                            gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_enable_btn), TRUE);
                        }
                    }
                }

                /* Always on/off groups cannot be moved */
                if (on_group_num == 1 || off_group_num == 1) {
                    gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_move_up_btn), FALSE);
                    gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_move_down_btn), FALSE);
                }

#endif
            if (group_num == 1) {
                GList*                   rows;
                GtkTreeIter              iter;
                GtkTreeIter              child_iter;
                gint                     group_id;
                NwamuiObject             *object;

                rows = gtk_tree_selection_get_selected_rows(selection, NULL);

                gtk_tree_model_get_iter(model, &iter, (GtkTreePath *)rows->data);
                if (gtk_tree_model_iter_children(model, &child_iter, &iter)) {
                    gtk_tree_model_get(model, &child_iter, 0, &object, -1);

                    gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_activation_combo), TRUE);
                    gtk_widget_set_sensitive(GTK_WIDGET(prv->activation_mode_lbl), TRUE);

                    connection_activation_combo_changed_cb(prv->connection_activation_combo, (gpointer) self);

                    prv->connection_activation_combo_show_ncu_part = FALSE;

                    gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(
                          gtk_combo_box_get_model(GTK_COMBO_BOX(self->prv->connection_activation_combo))));
                    group_id = (gint)g_object_get_data(G_OBJECT(object), NWAM_COND_NCU_GROUP_ID);

                    gtk_combo_box_set_active(self->prv->connection_activation_combo, group_id);
                    g_object_unref(object);
                }

                g_list_foreach (rows, (GFunc)gtk_tree_path_free, NULL);
                g_list_free (rows);
            }
        }
    }
}

static void 
conditions_connected_toggled_cb(GtkToggleButton *togglebutton, gpointer user_data)
{
    NwamNetConfPanel*           self = NWAM_NET_CONF_PANEL(user_data);
    NwamNetConfPanelPrivate*    prv = GET_PRIVATE(self);

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
    NwamNetConfPanelPrivate*    prv = GET_PRIVATE(self);

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
    NwamNetConfPanelPrivate*    prv = GET_PRIVATE(self);

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
    NwamNetConfPanelPrivate*    prv = GET_PRIVATE(self);
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

static void
ncp_add_ncu(NwamuiNcp *ncp, NwamuiNcu* ncu, gpointer data)
{
    NwamNetConfPanelPrivate*    prv = GET_PRIVATE(data);

    ncu_pri_treeview_add(prv->net_conf_treeview,
      gtk_tree_view_get_model(prv->net_conf_treeview)
      , NWAMUI_OBJECT(ncu));
}

static void
ncp_remove_ncu(NwamuiNcp *ncp, NwamuiNcu* ncu, gpointer data)
{
    NwamNetConfPanelPrivate*    prv = GET_PRIVATE(data);

    ncu_pri_treeview_remove(prv->net_conf_treeview,
      gtk_tree_view_get_model(prv->net_conf_treeview)
      , NWAMUI_OBJECT(ncu));
}

static NwamuiObject*
create_group_object( gint                               group_id,
                     nwamui_cond_priority_group_mode_t  group_mode,
                     nwamui_cond_activation_mode_t      act_mode )
{
    NwamuiObject    *pobj;

    pobj = g_object_new(NWAMUI_TYPE_OBJECT, NULL);

    g_object_set_data_full(G_OBJECT(pobj), NWAM_COND_NCU_GROUP_ID, 
                                           GINT_TO_POINTER(group_id), NULL);
    g_object_set_data_full(G_OBJECT(pobj), NWAM_COND_NCU_GROUP_MODE, 
                                           GINT_TO_POINTER(group_mode), NULL);
    g_object_set_data_full(G_OBJECT(pobj), NWAM_COND_NCU_ACT_MODE, 
                                           GINT_TO_POINTER(act_mode), NULL);
    return pobj;
}

static void
update_ncu_like_group( NwamuiObject* group_obj, NwamuiNcu* ncu )
{
    gint                               group_id, ncu_group_id;
    nwamui_cond_priority_group_mode_t  group_mode, ncu_group_mode;
    nwamui_cond_activation_mode_t      act_mode, ncu_act_mode;


    group_id = (gint)g_object_get_data(
                    G_OBJECT(group_obj), NWAM_COND_NCU_GROUP_ID);
    group_mode = (nwamui_cond_priority_group_mode_t)g_object_get_data(
                    G_OBJECT(group_obj), NWAM_COND_NCU_GROUP_MODE);
    act_mode = (nwamui_cond_activation_mode_t)g_object_get_data(
                    G_OBJECT(group_obj), NWAM_COND_NCU_ACT_MODE);

    ncu_group_id = nwamui_ncu_get_priority_group( ncu ); 

    if ( group_id != ncu_group_id ) {
        nwamui_ncu_set_priority_group( ncu, ncu_group_id ); 
    }
    if ( group_mode != ncu_group_mode ) {
        nwamui_ncu_set_priority_group_mode( ncu, ncu_group_mode ); 
    }
    if ( act_mode != ncu_act_mode ) {
        nwamui_ncu_set_activation_mode( ncu, ncu_act_mode );
    }
}

static gint
compare_int( gpointer a, gpointer b )
{
    return( GPOINTER_TO_INT(a) - GPOINTER_TO_INT(b) );
}

static gint
ncu_pri_group_get_index(NwamuiNcu *ncu)
{
    nwamui_cond_activation_mode_t act_mode = nwamui_ncu_get_activation_mode(ncu);
    switch (act_mode) {
    case NWAMUI_COND_ACTIVATION_MODE_MANUAL:
        return nwamui_ncu_get_enabled(ncu) ? ALWAYS_ON_GROUP_ID : ALWAYS_OFF_GROUP_ID;
    case NWAMUI_COND_ACTIVATION_MODE_PRIORITIZED:
        return nwamui_ncu_get_priority_group(ncu) + ALWAYS_ON_GROUP_ID + 1;
    default:
        g_warning("%s: Not supported activation mode %d", __func__, act_mode);
        return ALWAYS_OFF_GROUP_ID;
    }
}

static gboolean
ncu_pri_group_find_iter_gt(GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
    NwamuiObject *object;
    gint group_id;

    gtk_tree_model_get(model, iter, 0, &object, -1);

    group_id = (gint)g_object_get_data(object, NWAM_COND_NCU_GROUP_ID);

    g_object_unref(object);

    return group_id > (gint)user_data;
}

static gboolean
ncu_pri_group_find_iter_by_obj(GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
    NwamuiObject *object;
    gint group_id;

    gtk_tree_model_get(model, iter, 0, &object, -1);

    group_id = (gint)g_object_get_data(object, NWAM_COND_NCU_GROUP_ID);

    g_object_unref(object);

    return group_id > (gint)user_data;
}

static GHashTable*
ncu_pri_group_get_hash()
{
    static GHashTable *hash = NULL;

    if (hash == NULL) {
        hash = g_hash_table_new_full(g_direct_hash,
          g_direct_equal,
          NULL,
          (GDestroyNotify)gtk_tree_row_reference_free);
    }
    return hash;
}

/**
 * ncu_pri_group_get_iter:
 * 
 * Return NULL if the index isn't existed or valid, shouldn't be modified. 
 */
static GtkTreePath*
ncu_pri_group_get_path(GtkTreeModel *model, gint group_id)
{
    GHashTable *hash = ncu_pri_group_get_hash();
    GtkTreeRowReference* rowref;
    GtkTreePath *parent_path;

    if ((rowref = (GtkTreeRowReference*)g_hash_table_lookup(hash, group_id)) != NULL) {
        /* Assume the old one is what we added is a temp group if there are two
         * or more groups have the same group id. so we merge all temp group to
         * this current one. */
        return gtk_tree_row_reference_get_path(rowref);
    }
    return NULL;
}

/**
 * ncu_pri_group_get_iter:
 * 
 * Return TRUE if the index is existed and valid, FALSE if it is created or
 * revisted. 
 */
static gboolean
ncu_pri_group_get_iter(GtkTreeModel *model, gint group_id, gint group_mode, GtkTreeIter *parent)
{
    GHashTable *hash = ncu_pri_group_get_hash();
    GtkTreeRowReference* rowref;
    GtkTreePath *parent_path;

    if ((rowref = (GtkTreeRowReference*)g_hash_table_lookup(hash, group_id)) == NULL) {
        NwamuiObject *fakeobj;
        GtkTreeIter   sibling;

    l_beginning:
        fakeobj = create_group_object(group_id, group_mode, NWAMUI_COND_ACTIVATION_MODE_LAST);
        /* The ncu isn't in a group and there is no index for the group.
         * Find a group which has a low pri, then insert before it.
         */
        if (capplet_model_1_level_foreach(model, NULL,
            ncu_pri_group_find_iter_gt, group_id, &sibling)) {
            gtk_tree_store_insert_before(GTK_TREE_STORE(model), parent, NULL, &sibling);
        } else {
            gtk_tree_store_append(GTK_TREE_STORE(model), parent, NULL);
        }
        gtk_tree_store_set(GTK_TREE_STORE(model), parent, 0, fakeobj, -1);

        g_object_unref(fakeobj);
        /* Point to the new group */
        parent_path = gtk_tree_model_get_path(model, parent);
        rowref = gtk_tree_row_reference_new(model, parent_path);
        /* Reference could be invalid? */
        g_hash_table_replace(hash, group_id, rowref);
        gtk_tree_path_free(parent_path);

        return FALSE;
    } else {
        /* Assume the old one is what we added is a temp group if there are two
         * or more groups have the same group id. so we merge all temp group to
         * this current one. */
        GtkTreePath *path = gtk_tree_row_reference_get_path(rowref);

        if (path && gtk_tree_model_get_iter(model, parent, path)) {
            /* We already have a group/index for the group id. */
        } else {
            /* reference is invalid, goto beginning */
            goto l_beginning;
        }
    }
    return TRUE;
}

/**
 * ncu_pri_group_update:
 */
static void
ncu_pri_group_update(GtkTreeModel *model)
{
    GHashTable *hash = ncu_pri_group_get_hash();
    GtkTreeRowReference* rowref;
    GtkTreePath *path;
    GList *keys;
    gint i = ALWAYS_ON_GROUP_ID;
    NwamuiObject    *obj;
    GtkTreeIter iter;

    keys = g_hash_table_get_keys(hash);
    keys = g_list_sort(keys, compare_int);

    while (keys) {
        gint group_id = GPOINTER_TO_INT(keys->data);
        keys = g_list_delete_link(keys, keys);

        if (group_id != ALWAYS_ON_GROUP_ID && group_id != ALWAYS_OFF_GROUP_ID) {
            rowref = (GtkTreeRowReference*)g_hash_table_lookup(hash, group_id);
            if (rowref) {
                path = gtk_tree_row_reference_get_path(rowref);
                if (path) {
                    i++;
                    gtk_tree_model_get_iter(model, &iter, path);
                    gtk_tree_model_get(model, &iter, 0, &obj, -1);

                    g_hash_table_steal(hash, group_id);
                    g_hash_table_insert(hash, GINT_TO_POINTER(i), rowref);

                    g_object_set_data(G_OBJECT(obj), NWAM_COND_NCU_GROUP_ID, 
                      GINT_TO_POINTER(i));
                    g_object_unref(obj);

/*                     gtk_tree_model_row_changed(model, path, &iter); */
                }
            }
        }
    }
}

static void
ncu_pri_treeview_add(GtkTreeView *treeview, GtkTreeModel *model, NwamuiObject *object)
{
    GtkTreePath *path;
    GtkTreeIter parent;
    GtkTreeIter iter;

    if (object && NWAMUI_IS_NCU(object)) {
        gboolean ret;
        ret = ncu_pri_group_get_iter(model,
          ncu_pri_group_get_index(NWAMUI_NCU(object)),
          nwamui_ncu_get_priority_group_mode(NWAMUI_NCU(object)),
          &parent);
        if (!ret) {
            g_assert(!gtk_tree_model_iter_has_child(model, &parent));
        }
        CAPPLET_TREE_STORE_ADD(model, &parent, object, &iter);

        path = gtk_tree_model_get_path(model, &parent);
        gtk_tree_view_expand_row(treeview, path, TRUE);
        gtk_tree_path_free(path);

    } else {
        if (!fake_object_in_pri_group)
            fake_object_in_pri_group = g_object_new(NWAMUI_TYPE_OBJECT, NULL);

        ncu_pri_group_get_iter(model, ALWAYS_ON_GROUP_ID, 0, &parent);
        if (!gtk_tree_model_iter_has_child(model, &parent)) {
            CAPPLET_TREE_STORE_ADD(model, &parent, fake_object_in_pri_group, &iter);

            path = gtk_tree_model_get_path(model, &parent);
            gtk_tree_view_expand_row(treeview, path, TRUE);
            gtk_tree_path_free(path);
        }

        ncu_pri_group_get_iter(model, ALWAYS_OFF_GROUP_ID, 0, &parent);
        if (!gtk_tree_model_iter_has_child(model, &parent)) {
            CAPPLET_TREE_STORE_ADD(model, &parent, fake_object_in_pri_group, &iter);

            path = gtk_tree_model_get_path(model, &parent);
            gtk_tree_view_expand_row(treeview, path, TRUE);
            gtk_tree_path_free(path);
        }
    }
}

static void
ncu_pri_treeview_remove(GtkTreeView *treeview, GtkTreeModel *model, NwamuiObject *object)
{
    GtkTreeIter parent;
    GtkTreeIter temp;
    GtkTreePath *path;
    NwamuiObject *obj;
    GtkTreeRowReference *group_rr;

    g_return_if_fail(object && NWAMUI_IS_NCU(object));

    path = ncu_pri_group_get_path(model,
      ncu_pri_group_get_index(NWAMUI_NCU(object)));

    group_rr = ref_ncu_group_node(model, path);
    gtk_tree_model_get_iter(model, &parent, path);
    gtk_tree_path_free(path);

    if (capplet_model_find_object_with_parent(model, &parent,
        G_OBJECT(object), &temp)) {
        gtk_tree_store_remove(GTK_TREE_STORE(model), &temp);

    }
    handle_ncu_group_node(model, group_rr);
    gtk_tree_row_reference_free(group_rr);
}

static void
connection_activation_combo_cell_cb(GtkCellLayout *cell_layout,
    GtkCellRenderer   *renderer,
    GtkTreeModel      *model,
    GtkTreeIter       *iter,
    gpointer           data)
{
    gint row_data;
	gtk_tree_model_get(model, iter, 0, &row_data, -1);
    g_object_set(G_OBJECT(renderer), "text", combo_contents[row_data], NULL);
}

static void 
connection_activation_combo_changed_cb(GtkComboBox* combo, gpointer user_data)
{
    NwamNetConfPanelPrivate*    prv = GET_PRIVATE(user_data);
    GtkTreeModel *model = gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER(gtk_combo_box_get_model(combo)));
	GtkTreeIter iter;
	GtkTreeIter child_iter;

	if (gtk_combo_box_get_active_iter(combo, &iter)) {
        gint row_data;
        gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(gtk_combo_box_get_model(combo)), &child_iter, &iter);
		gtk_tree_model_get(model, &child_iter, 0, &row_data, -1);
		switch (row_data) {
        case 1:
        case 3:
        case 4:
            break;
        default:
            break;
        }
	}
}

static gboolean
connection_activation_combo_filter(GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
    NwamNetConfPanelPrivate*    prv = GET_PRIVATE(user_data);
    gint row_data;

    gtk_tree_model_get(model, iter, 0, &row_data, -1);

    return (prv->connection_activation_combo_show_ncu_part && row_data < 3) ||
      (!prv->connection_activation_combo_show_ncu_part && row_data >= 3);
}
