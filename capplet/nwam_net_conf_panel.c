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
#define CONNECTION_MOVE_UP_BTN          "connection_move_up_btn"
#define CONNECTION_MOVE_DOWN_BTN        "connection_move_down_btn"
#define CONNECTION_RENAME_BTN           "connection_rename_btn"
#define CONNECTION_GROUP_BTN            "connection_group_btn"
#define ACTIVATION_MODE_LBL             "activation_mode_lbl"
#define CONNECTION_ACTIVATION_COMBO     "connection_activation_combo"

#define TREEVIEW_COLUMN_NUM             "meta:column"
#define COND_NODE_INFO                  "ncp_group_index_info"
#define COND_MODEL_INFO                 "ncp_group_first_id"

/* Values picked to put always ON at top of list, and always off at bottom */
#define ALWAYS_ON_GROUP_ID              (0)
#define ALWAYS_OFF_GROUP_ID             (G_MAXINT)

struct _NwamNetConfPanelPrivate {
	/* Widget Pointers */
	GtkTreeView*	    net_conf_treeview;

    GtkButton*          connection_move_up_btn;	
    GtkButton*          connection_move_down_btn;	
    GtkButton*          connection_rename_btn;
    GtkButton*          connection_group_btn;

    GtkComboBox*        profile_name_combo;
    GtkComboBox*        edit_profile_name_combo;

    GtkLabel*           activation_mode_lbl;
    GtkComboBox*        connection_activation_combo;

	/* Other Data */
    NwamCappletDialog*  pref_dialog;
    NwamuiNcp*          selected_ncp;
    NwamuiNcu*          selected_ncu;			/* Use g_object_set for it */
    gboolean            update_inprogress;
    gboolean			manual_expander_flag;
    gboolean            connection_activation_combo_show_ncu_part;
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
static void update_widgets(NwamNetConfPanel *self, GtkTreeSelection *selection);
static void selection_changed(GtkTreeSelection *selection, gpointer user_data);
static void cursor_changed(GtkTreeView *treeview, gpointer user_data);

static void update_rules_from_ncu (NwamNetConfPanel* self,
  GParamSpec *arg1,
  gpointer ncu);                /* unused */
static void conditions_connected_toggled_cb(GtkToggleButton *togglebutton, gpointer user_data);
static void conditions_set_status_changed(GtkComboBox* combo, gpointer user_data );
static void conditions_connected_changed(GtkComboBox* combo, gpointer user_data );
static void on_activate_static_menuitems (GtkAction *action, gpointer data);

/* ncu group */
static gboolean ncp_build_group_index(GtkTreeModel *model, GHashTable *hash, GtkTreeIter *parent, GtkTreeIter *iter, NwamuiNcu *ncu);
static void ncp_build_group(GtkTreeModel *model);

static gint ncu_pri_group_get_index(NwamuiNcu *ncu);
static gboolean ncu_pri_group_get_iter(GtkTreeModel *model, gint group_id, GtkTreeIter *parent);
static void ncu_pri_treeview_add(GtkTreeView *treeview, GtkTreeModel *model, NwamuiObject *object);


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
      "show-expanders", FALSE,
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

/*     g_signal_connect(view, */
/*       "cursor-changed", */
/*       G_CALLBACK(cursor_changed), */
/*       (gpointer)self); */

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

    prv->connection_move_up_btn = GTK_BUTTON(nwamui_util_glade_get_widget(CONNECTION_MOVE_UP_BTN));	
    prv->connection_move_down_btn = GTK_BUTTON(nwamui_util_glade_get_widget(CONNECTION_MOVE_DOWN_BTN));	
    prv->connection_rename_btn = GTK_BUTTON(nwamui_util_glade_get_widget(CONNECTION_RENAME_BTN));	
    prv->connection_group_btn = GTK_BUTTON(nwamui_util_glade_get_widget(CONNECTION_GROUP_BTN));	

    prv->activation_mode_lbl = GTK_LABEL(nwamui_util_glade_get_widget(ACTIVATION_MODE_LBL));
    prv->connection_activation_combo = GTK_COMBO_BOX(nwamui_util_glade_get_widget(CONNECTION_ACTIVATION_COMBO));

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

            model = gtk_tree_view_get_model(prv->net_conf_treeview);

            gtk_widget_hide(GTK_WIDGET(prv->net_conf_treeview));

            gtk_tree_store_clear(GTK_TREE_STORE(model));
            /* Clear COND_MODEL_INFO */
            g_object_set_data(G_OBJECT(model), COND_MODEL_INFO,
              GINT_TO_POINTER(ALWAYS_OFF_GROUP_ID));

            {
                GList *obj_list = nwamui_ncp_get_ncu_list(prv->selected_ncp);

                while (obj_list) {
                    ncu_pri_treeview_add(prv->net_conf_treeview, model, NWAMUI_OBJECT(obj_list->data));
                    g_object_unref(obj_list->data);
                    obj_list = g_list_delete_link(obj_list, obj_list);
                }
                /* Check if all or none group have nodes. */
                ncu_pri_treeview_add(prv->net_conf_treeview, model, NULL);
            }
            gtk_widget_show(GTK_WIDGET(prv->net_conf_treeview));
            /* Update widgets */
            selection_changed(gtk_tree_view_get_selection(prv->net_conf_treeview), (gpointer)self);

        }
        prv->selected_ncp = NWAMUI_NCP(user_data);
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
    nwamui_util_show_help ("");
}

static void
nwam_net_conf_panel_finalize(NwamNetConfPanel *self)
{
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
    nwamui_ncu_type_t              ncu_type;
    gchar*                         ncu_text;
    gchar*                         ncu_markup;
    gboolean                       ncu_enabled;
    nwamui_cond_activation_mode_t  ncu_activation_mode;
    nwamui_cond_priority_group_mode_t  
      ncu_priority_group_mode;
    gboolean                       ncu_is_dhcp;
    gchar*                         ncu_ipv4_addr;
    gchar*                         ncu_phy_addr;
    GdkPixbuf                     *status_icon;
    gchar*                         info_string;
    gboolean                       is_group = FALSE;
    
    gtk_tree_model_get(model, iter, 0, &ncu, -1);

    if (ncu) {
        if (NWAMUI_IS_NCU(ncu)) {
            ncu_type = nwamui_ncu_get_ncu_type(ncu);
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
            ncu_text = nwamui_ncu_get_display_name(ncu);
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
            GtkTreeIter child_iter;
            GString *str = NULL;
            gint    group_id = 0;
            gint    first_group_id = 0;

            group_id = (gint)g_object_get_data(G_OBJECT(ncu), COND_NODE_INFO);
            first_group_id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(model), COND_MODEL_INFO));

            if (group_id == ALWAYS_ON_GROUP_ID) {
                str = g_string_new(_("Always enable these connections:"));
            } else if (group_id == ALWAYS_OFF_GROUP_ID) {
                str = g_string_new(_("Always disable these connections:"));
            } else if (gtk_tree_model_iter_children(model, &child_iter, iter)) {

                NwamuiNcu *child_ncu;

                gtk_tree_model_get(model, &child_iter, 0, &child_ncu, -1);

                if ( child_ncu && NWAMUI_IS_NCU( child_ncu ) ) {
                    ncu_enabled = nwamui_ncu_get_enabled(child_ncu);
                    ncu_activation_mode = nwamui_ncu_get_activation_mode(child_ncu);
                    ncu_priority_group_mode = nwamui_ncu_get_priority_group_mode(child_ncu);
                }
                    
                if ( child_ncu ) {
                    g_object_unref(child_ncu);
                }

                if ( str == NULL ) {
                    switch (ncu_activation_mode) {
                    case NWAMUI_COND_ACTIVATION_MODE_PRIORITIZED:
                        switch (ncu_priority_group_mode) {
                        case NWAMUI_COND_PRIORITY_GROUP_MODE_EXCLUSIVE:
                            if ( first_group_id == group_id ) {
                                str = g_string_new(
                                    _("Then enable exactly one of these connections:"));
                            }
                            else {
                                str = g_string_new(
                                    _("Else enable exactly one of these connections:"));
                            }
                            break;
                        case NWAMUI_COND_PRIORITY_GROUP_MODE_SHARED:
                            if ( first_group_id == group_id ) {
                                str = g_string_new(
                                    _("Then enable one or more of these connections:"));
                            }
                            else {
                                str = g_string_new(
                                    _("Else enable one or more of these connections:"));
                            }
                            break;
                        case NWAMUI_COND_PRIORITY_GROUP_MODE_ALL:
                            if ( first_group_id == group_id ) {
                                str = g_string_new(
                                    _("Then enable all of these connections:"));
                            }
                            else {
                                str = g_string_new(
                                    _("Else enable all of these connections:"));
                            }
                            break;
                        default:
                            g_assert_not_reached();
                            break;
                        }
                        break;
                    case NWAMUI_COND_ACTIVATION_MODE_MANUAL:
                        /* Should be already set above */
                        break;
                    case NWAMUI_COND_ACTIVATION_MODE_SYSTEM:
                        g_error("Unexpected activation mode SYSTEM for NCU");
                        return;
                    }
                }
            }
            else {
                /* Handle case where there are no children */
                str = g_string_new(_("<i>None</i>"));
            }

            g_object_set(G_OBJECT(renderer),
              "sensitive", TRUE,
              "markup", str->str,
              NULL);
            g_string_free(str, TRUE);
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

static void
on_button_clicked(GtkButton *button, gpointer user_data)
{
    NwamNetConfPanel*           self = NWAM_NET_CONF_PANEL(user_data);
    NwamNetConfPanelPrivate*    prv = GET_PRIVATE(self);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(prv->net_conf_treeview);
    GtkTreeModel*               model;
    GtkTreeIter                 iter;
    gint                        count_selected_rows;
    GList*                      rows;

    count_selected_rows = gtk_tree_selection_count_selected_rows(selection);
    rows = gtk_tree_selection_get_selected_rows(selection, &model);

    if (count_selected_rows >= 1) {
        GtkTreePath *path = (GtkTreePath *)rows->data;

        gtk_tree_model_get_iter(model, &iter, path);

        if (button == (gpointer)prv->connection_group_btn) {
            gboolean all_in_group = (gboolean)g_object_get_data(G_OBJECT(prv->connection_group_btn), "all_in_group");
            GList *i;
            GList *rowrefs = NULL;
            GtkTreeIter parent;

            if (!all_in_group) {
                /* Append a new group */
                NwamuiObject *pobj = g_object_new(NWAMUI_TYPE_OBJECT, NULL);
                gtk_tree_store_append(GTK_TREE_STORE(model), &parent, NULL);
                gtk_tree_store_set(GTK_TREE_STORE(model), &parent, 0, pobj, -1);
                g_object_unref(pobj);
            }

            for (i = rows; i; i = g_list_next(i)) {
                rowrefs = g_list_prepend(rowrefs, gtk_tree_row_reference_new(model, (GtkTreePath*)i->data));
            }
            for (i = rowrefs; i; i = g_list_next(i)) {
                GtkTreeIter iter;
                gtk_tree_model_get_iter(model, &iter,
                  gtk_tree_row_reference_get_path((GtkTreeRowReference*)i->data));

                /* TODO */
                if (all_in_group)
                    capplet_tree_store_exclude_children(GTK_TREE_STORE(model),
                      &iter,
                      NULL,
                      NULL);
                else {
                    capplet_tree_store_merge_children(GTK_TREE_STORE(model),
                      &parent,
                      &iter,
                      NULL,
                      NULL);
                    gtk_tree_selection_select_iter(gtk_tree_view_get_selection(prv->net_conf_treeview), &parent);
                }
            }
            g_list_foreach (rowrefs, (GFunc)gtk_tree_row_reference_free, NULL);
            g_list_free (rowrefs);
            
        } else if (count_selected_rows == 1) {
            NwamuiObject *object;
            gtk_tree_model_get(model, &iter, 0, &object, -1);

            if (button == (gpointer)prv->connection_move_up_btn) {
                GtkTreeIter prev_iter;
                GtkTreeIter parent_iter;

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
        }
    }
    g_list_foreach (rows, (GFunc)gtk_tree_path_free, NULL);
    g_list_free (rows);

}

static void
cursor_changed(GtkTreeView *treeview, gpointer user_data)
{
    NwamNetConfPanelPrivate*    prv = GET_PRIVATE(user_data);
    update_widgets(NWAM_NET_CONF_PANEL(user_data), gtk_tree_view_get_selection(prv->net_conf_treeview));
}

static void
selection_changed(GtkTreeSelection *selection, gpointer user_data)
{
    update_widgets(NWAM_NET_CONF_PANEL(user_data), selection);
}

static void
update_widgets(NwamNetConfPanel *self, GtkTreeSelection *selection)
{
    NwamNetConfPanelPrivate*    prv = GET_PRIVATE(self);
    GtkTreeModel*               model = gtk_tree_view_get_model(prv->net_conf_treeview);
    GtkTreeIter                 iter;
    GList*                      rows;
    gint                        count_selected_rows;
    gboolean                    sensitive;
    gint on_group_num = 0;
    gint off_group_num = 0;
    gint group_num = 0;
    gint on_child_num = 0;
    gint off_child_num = 0;
    gint child_num = 0;
    NwamuiObject *object;

    gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_move_up_btn), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_move_down_btn), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_rename_btn), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_group_btn), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_activation_combo), FALSE);
    gtk_button_set_label(prv->connection_group_btn, _("_Group"));

    if (!prv->selected_ncp || !nwamui_ncp_is_modifiable(prv->selected_ncp)) {
        return;
    }

    count_selected_rows = gtk_tree_selection_count_selected_rows(selection);
    rows = gtk_tree_selection_get_selected_rows(selection, &model);

    if (count_selected_rows > 0) {
        GList *i;
        for (i = rows; i; i = g_list_next(i)) {
            gboolean is_child;
            GtkTreeIter parent;
            GtkTreeIter iter;
            GtkTreeIter *group_iter;
            gint group_id;

            gtk_tree_model_get_iter(model, &iter, (GtkTreePath*)i->data);

            if (gtk_tree_model_iter_parent(model, &parent, &iter)) {

                /* Filter out the fake object. */
                gtk_tree_model_get(model, &iter, 0, &object, -1);
                if (object == (gpointer)fake_object_in_pri_group) {
                    g_object_unref(object);
                    /* Minus the fake selection. */
                    count_selected_rows--;
                    continue;
                }
                g_object_unref(object);

                is_child = TRUE;
                group_iter = &parent;
            } else {
                is_child = FALSE;
                group_iter = &iter;
            }

            gtk_tree_model_get(model, group_iter, 0, &object, -1);
            group_id = (gint)g_object_get_data(G_OBJECT(object), COND_NODE_INFO);
            g_object_unref(object);

            if (is_child) {
                if (group_id == ALWAYS_ON_GROUP_ID)
                    on_child_num++;
                else if (group_id == ALWAYS_OFF_GROUP_ID)
                    off_child_num++;
                else
                    child_num++;
            } else {
                if (group_id == ALWAYS_ON_GROUP_ID)
                    on_group_num++;
                else if (group_id == ALWAYS_OFF_GROUP_ID)
                    off_group_num++;
                else
                    group_num++;
            }
        }

        if (count_selected_rows > 0) {
            if (group_num == 0) {
                /* Enable New Group */
                gtk_button_set_label(prv->connection_group_btn, _("New_group"));
                gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_group_btn), TRUE);
            } else if (on_group_num + off_group_num + group_num > 1) {
                /* Enable Remove Group */
                gtk_button_set_label(prv->connection_group_btn, _("Remove_group"));
                gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_group_btn), TRUE);
            }

            if (on_child_num == 0 && off_child_num + child_num > 0) {
                /* Enable Enable */
            }

            if (off_child_num == 0 && on_child_num + child_num > 0) {
                /* Enable Disable */
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
                if (child_num == 1) {
                    /* Enable Rename */
                    gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_rename_btn), TRUE);
                }

                if (on_group_num == 1) {
                    gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_move_up_btn), FALSE);
                }

                if (off_group_num == 1) {
                    gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_move_down_btn), FALSE);
                }

                if (group_num == 1) {
                    GtkTreePath *path = (GtkTreePath *)rows->data;

                    gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_activation_combo), TRUE);

                    connection_activation_combo_changed_cb(prv->connection_activation_combo, (gpointer) self);

                    gtk_tree_model_get_iter(model, &iter, path);
                    if (!gtk_tree_model_iter_has_child(model, &iter)) {
                        gtk_label_set_markup_with_mnemonic(prv->activation_mode_lbl, _("_Selected connection:"));
                        prv->connection_activation_combo_show_ncu_part = TRUE;
                    } else {
                        gtk_label_set_markup_with_mnemonic(prv->activation_mode_lbl, _("_Selected group:"));
                        prv->connection_activation_combo_show_ncu_part = FALSE;
                    }
                    gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(
                          gtk_combo_box_get_model(GTK_COMBO_BOX(self->prv->connection_activation_combo))));
                    if (gtk_combo_box_get_active(self->prv->connection_activation_combo) == -1)
                        gtk_combo_box_set_active(self->prv->connection_activation_combo, 0);
                }
            }
        }
    }
    g_list_foreach (rows, (GFunc)gtk_tree_path_free, NULL);
    g_list_free (rows);

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

static gboolean
ncp_build_group_index(  GtkTreeModel *model, 
                        GHashTable *hash, 
                        GtkTreeIter *parent, 
                        GtkTreeIter *iter, 
                        NwamuiNcu *ncu)
{
    GtkTreeRowReference*            rowref;
    gint                            group_id;
    GtkTreePath*                    parent_path;
    GtkTreeIter                     parent_iter;
    gboolean                        need_merge = TRUE;
    nwamui_cond_activation_mode_t   activation_mode;
    gboolean                        moved = FALSE;
    
    /* Assume ncu should be in a group in this function */
    g_assert(iter);

    /* System activation group, and Manual activation group need a new group
     * id
     */
    activation_mode = nwamui_ncu_get_activation_mode(ncu);
    switch (activation_mode) {
        case NWAMUI_COND_ACTIVATION_MODE_PRIORITIZED:
            group_id = nwamui_ncu_get_priority_group(ncu);
            break;
        case NWAMUI_COND_ACTIVATION_MODE_MANUAL:
            if ( nwamui_ncu_get_enabled(ncu) ) {
                group_id = ALWAYS_ON_GROUP_ID;
            }
            else {
                group_id = ALWAYS_OFF_GROUP_ID;
            }
            break;
        case NWAMUI_COND_ACTIVATION_MODE_SYSTEM:
            g_error("Unexpected activation mode SYSTEM for NCU");
            return( FALSE );
    }

    if ( !parent ) {
        /* No parent, so we need to give it one */
        if ((rowref = (GtkTreeRowReference*)g_hash_table_lookup(hash, &group_id)) == NULL) {
            /* No existing group to match, so create one */
            GObject    *pobj;
            GtkTreeIter insert_iter;
            gboolean    found_insert_point = FALSE;
            gint        current_first_id;
            
            /* Look for appopriate location to insert this */
            if (gtk_tree_model_get_iter_first(model, &insert_iter)) {
                NwamuiObject    *obj;
                do {
                    obj = NULL;
                    gtk_tree_model_get(model, &insert_iter, 0, &obj, -1);

                    if ( obj && NWAMUI_IS_OBJECT( obj ) ) {
                        gint id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(obj), "ncp_group_id"));
                        if ( id > group_id ) {
                            found_insert_point = TRUE;
                            break;
                        }
                    }

                    if ( obj ) {
                        g_object_unref(obj);
                    }
                } while ( gtk_tree_model_iter_next(model, &insert_iter) );
            }

            current_first_id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(model), 
                                                                 COND_MODEL_INFO));

            if ( group_id < current_first_id ) {
                /* Try to remember first id */
                g_object_set_data_full(G_OBJECT(model), COND_MODEL_INFO, 
                                       GINT_TO_POINTER(group_id), NULL);
            }

            /* The ncu isn't in a group and there is no index for the group */
            if ( found_insert_point ) {
                gtk_tree_store_insert_before(GTK_TREE_STORE(model), &parent_iter, NULL, &insert_iter );
            }
            else {
                gtk_tree_store_append(GTK_TREE_STORE(model), &parent_iter, NULL);
            }
            pobj = g_object_new(NWAMUI_TYPE_OBJECT, NULL);
            g_object_set_data_full(G_OBJECT(pobj), "ncp_group_id", GINT_TO_POINTER(group_id), NULL);
            gtk_tree_store_set(GTK_TREE_STORE(model), &parent_iter, 0, pobj, -1);
            /* Point to the new group */
            parent = &parent_iter;
        }
        else {
            /* A suitable group already exists, so use it */
            GtkTreePath *path = gtk_tree_row_reference_get_path(rowref);

            if (gtk_tree_model_get_iter(model, &parent_iter, path)) {
                /* The ncu isn't in a group but we have an index, so we move
                 * it */
                parent = &parent_iter;
            } else {
                /* reference is invalid? goto beginning */
                g_assert_not_reached();
            }
        }
    }
    else if (0) {
        /* We have a parent pointer */
        parent_path = gtk_tree_model_get_path(model, parent);
        rowref = gtk_tree_row_reference_new(model, parent_path);
        g_hash_table_insert(hash, &group_id, rowref);
        gtk_tree_path_free(parent_path);
    }
    else {
        need_merge = FALSE;
    }

    if (need_merge) {
        capplet_tree_store_merge_children(GTK_TREE_STORE(model),
          parent,
          iter,
          NULL,
          NULL);
        moved = TRUE;
    }
    return (moved);
}

static gint
compare_int( gpointer a, gpointer b )
{
    return( GPOINTER_TO_INT(a) - GPOINTER_TO_INT(b) );
}

static void
ncp_build_group(GtkTreeModel *model)
{
    GHashTable *hash = (GHashTable*)g_object_get_data(G_OBJECT(model), "ncp_group_index");
    GtkTreeIter iter;

    if (hash == NULL) {
        gint                    group_id;
        GtkTreePath*            tree_path;
        GtkTreeIter             item_iter;
        GtkTreeIter             child_iter;
        GtkTreeRowReference*    rowref;
        NwamuiObject*           pobj;

        hash = g_hash_table_new_full(g_int_hash,
          g_int_equal,
          NULL,
          (GDestroyNotify)gtk_tree_row_reference_free);
        g_object_set_data_full(G_OBJECT(model), "ncp_group_index",
          (gpointer)hash, (GDestroyNotify)g_hash_table_destroy);
            
        /* Create Always On / Off groups, and one child "None"  */
        group_id = ALWAYS_ON_GROUP_ID;
        gtk_tree_store_append(GTK_TREE_STORE(model), &item_iter, NULL);
        pobj = g_object_new(NWAMUI_TYPE_OBJECT, NULL);
        g_object_set_data_full(G_OBJECT(pobj), "ncp_group_id", GINT_TO_POINTER(group_id), NULL);
        gtk_tree_store_set(GTK_TREE_STORE(model), &item_iter, 0, pobj, -1);
        g_object_unref(pobj);
        tree_path = gtk_tree_model_get_path(model, &item_iter);
        rowref = gtk_tree_row_reference_new(model, tree_path);
        g_hash_table_insert(hash, &group_id, rowref);
        gtk_tree_path_free(tree_path);

        /* Create "None" element */
        gtk_tree_store_append(GTK_TREE_STORE(model), &child_iter, &item_iter);
        pobj = g_object_new(NWAMUI_TYPE_OBJECT, NULL);
        gtk_tree_store_set(GTK_TREE_STORE(model), &child_iter, 0, pobj, -1);
        g_object_unref(pobj);

        group_id = ALWAYS_OFF_GROUP_ID;
        gtk_tree_store_append(GTK_TREE_STORE(model), &item_iter, NULL);
        pobj = g_object_new(NWAMUI_TYPE_OBJECT, NULL);
        g_object_set_data_full(G_OBJECT(pobj), "ncp_group_id", GINT_TO_POINTER(group_id), NULL);
        gtk_tree_store_set(GTK_TREE_STORE(model), &item_iter, 0, pobj, -1);
        g_object_unref(pobj);
        tree_path = gtk_tree_model_get_path(model, &item_iter);
        rowref = gtk_tree_row_reference_new(model, tree_path);
        g_hash_table_insert(hash, &group_id, rowref);
        gtk_tree_path_free(tree_path);

        /* Create "None" element */
        gtk_tree_store_append(GTK_TREE_STORE(model), &child_iter, &item_iter);
        pobj = g_object_new(NWAMUI_TYPE_OBJECT, NULL);
        gtk_tree_store_set(GTK_TREE_STORE(model), &child_iter, 0, pobj, -1);
        g_object_unref(pobj);
    }

    if (gtk_tree_model_get_iter_first(model, &iter)) {
        gboolean not_done = TRUE;

        /* Reset first_id so that we can find the lowest value */
        g_object_set_data_full(G_OBJECT(model), COND_MODEL_INFO,
          GINT_TO_POINTER(G_MAXINT), NULL);

        do {
            GtkTreeIter child_iter;
            GtkTreeIter *parent_iter = NULL;
            NwamuiNcu *ncu;

            /* Assume each fake group object must have childen */
            /* We only iterate the first level -- group */
            if (gtk_tree_model_iter_children(model, &child_iter, &iter)) {
                gtk_tree_model_get(model, &child_iter, 0, &ncu, -1);

                parent_iter = &iter;
            } else {
                gtk_tree_model_get(model, &iter, 0, &ncu, -1);
            }

            if ( ncu && NWAMUI_IS_NCU(ncu) ) {
                gboolean moved; 
                moved = ncp_build_group_index(model, hash,
                  parent_iter,
                  parent_iter ? &child_iter : &iter,
                  ncu);

                g_object_unref(ncu);

                if ( moved ) { /* Reset to first again */
                    not_done = gtk_tree_model_get_iter_first(model, &iter);
                }
                else {
                    not_done = gtk_tree_model_iter_next(model, &iter);
                }
            }
            else {
                not_done = gtk_tree_model_iter_next(model, &iter);
            }
        } while ( not_done );
    }
}

static gint
ncu_pri_group_get_index(NwamuiNcu *ncu)
{
    nwamui_cond_activation_mode_t act_mode = nwamui_ncu_get_activation_mode(ncu);
    nwamui_cond_priority_group_mode_t group_mode = nwamui_ncu_get_priority_group_mode(ncu);

    switch (act_mode) {
    case NWAMUI_COND_ACTIVATION_MODE_MANUAL:
        return nwamui_ncu_get_enabled(ncu) ? ALWAYS_ON_GROUP_ID : ALWAYS_OFF_GROUP_ID;
    case NWAMUI_COND_ACTIVATION_MODE_PRIORITIZED:
        return nwamui_ncu_get_priority_group(ncu) + ALWAYS_ON_GROUP_ID + 1;
/*         switch (group_mode) { */
/*         case NWAMUI_COND_PRIORITY_GROUP_MODE_EXCLUSIVE: */
/*             return  */
/*         case NWAMUI_COND_PRIORITY_GROUP_MODE_SHARED: */
/*         case NWAMUI_COND_PRIORITY_GROUP_MODE_ALL: */
/*         case NWAMUI_COND_PRIORITY_GROUP_MODE_LAST: */
/*         } */
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

    group_id = (gint)g_object_get_data(object, COND_NODE_INFO);

    g_object_unref(object);

    return group_id > (gint)user_data;
}

/**
 * ncu_pri_group_get_iter:
 * 
 * Return TRUE if the index is existed and valid, FALSE if it is created or
 * revisted. 
 */
static gboolean
ncu_pri_group_get_iter(GtkTreeModel *model, gint group_id, GtkTreeIter *parent)
{
    GHashTable *hash = (GHashTable*)g_object_get_data(model, "ncp_group_index");
    GtkTreeRowReference* rowref;
    GtkTreePath *parent_path;

    if (hash == NULL) {
        hash = g_hash_table_new_full(g_direct_hash,
          g_direct_equal,
          NULL,
          (GDestroyNotify)gtk_tree_row_reference_free);
        g_object_set_data_full(model, "ncp_group_index",
          (gpointer)hash, (GDestroyNotify)g_hash_table_destroy);
    }

    if ((rowref = (GtkTreeRowReference*)g_hash_table_lookup(hash, group_id)) == NULL) {
        NwamuiObject *fakeobj;
        GtkTreeIter   sibling;
        gint          current_first_id;

    l_beginning:
        fakeobj = g_object_new(NWAMUI_TYPE_OBJECT, NULL);
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
        g_object_set_data(fakeobj, COND_NODE_INFO, (gpointer)group_id);
        g_object_unref(fakeobj);
        /* Point to the new group */
        parent_path = gtk_tree_model_get_path(model, parent);
        rowref = gtk_tree_row_reference_new(model, parent_path);
        /* Reference could be invalid? */
        g_hash_table_replace(hash, group_id, rowref);
        gtk_tree_path_free(parent_path);

        current_first_id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(model), 
            COND_MODEL_INFO));
        if (group_id != ALWAYS_ON_GROUP_ID && group_id < current_first_id) {
            /* Try to remember first id */
            g_object_set_data(G_OBJECT(model), COND_MODEL_INFO, 
              GINT_TO_POINTER(group_id));
        }

        return FALSE;
    } else {
        /* Assume the old one is what we added is a temp group if there are two
         * or more groups have the same group id. so we merge all temp group to
         * this current one. */
        GtkTreePath *path = gtk_tree_row_reference_get_path(rowref);

        if (gtk_tree_row_reference_valid(rowref) &&
          gtk_tree_model_get_iter(model, parent, path)) {
            /* We already have a group/index for the group id. */
            parent_path = gtk_tree_model_get_path(model, parent);
            rowref = gtk_tree_row_reference_new(model, parent_path);
            g_hash_table_replace(hash, group_id, rowref);
            gtk_tree_path_free(parent_path);
        } else {
            /* reference is invalid, goto beginning */
            goto l_beginning;
        }
    }
    return TRUE;
}

static void
ncu_pri_treeview_add(GtkTreeView *treeview, GtkTreeModel *model, NwamuiObject *object)
{
    GtkTreePath *path;
    GtkTreeIter parent;
    GtkTreeIter iter;

    if (object && NWAMUI_IS_NCU(object)) {
        ncu_pri_group_get_iter(model, ncu_pri_group_get_index(NWAMUI_NCU(object)), &parent);
        CAPPLET_TREE_STORE_ADD(model, &parent, object);

        path = gtk_tree_model_get_path(model, &parent);
        gtk_tree_view_expand_row(treeview, path, TRUE);
        gtk_tree_path_free(path);

    } else {
        if (!fake_object_in_pri_group)
            fake_object_in_pri_group = g_object_new(NWAMUI_TYPE_OBJECT, NULL);

        ncu_pri_group_get_iter(model, ALWAYS_ON_GROUP_ID, &parent);
        if (!gtk_tree_model_iter_has_child(model, &parent)) {
            CAPPLET_TREE_STORE_ADD(model, &parent, fake_object_in_pri_group);

            path = gtk_tree_model_get_path(model, &parent);
            gtk_tree_view_expand_row(treeview, path, TRUE);
            gtk_tree_path_free(path);
        }

        ncu_pri_group_get_iter(model, ALWAYS_OFF_GROUP_ID, &parent);
        if (!gtk_tree_model_iter_has_child(model, &parent)) {
            CAPPLET_TREE_STORE_ADD(model, &parent, fake_object_in_pri_group);

            path = gtk_tree_model_get_path(model, &parent);
            gtk_tree_view_expand_row(treeview, path, TRUE);
            gtk_tree_path_free(path);
        }
    }
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
