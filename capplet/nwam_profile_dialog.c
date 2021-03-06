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
#include <glib/gi18n.h>

#include "libnwamui.h"
#include "nwam_pref_dialog.h"
#include "nwam_env_pref_dialog.h"
#include "nwam_profile_dialog.h"
#include "nwam_pref_iface.h"
#include "capplet-utils.h"
#include "nwam_tree_view.h"

#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
	NWAM_TYPE_PROFILE_DIALOG, NwamProfileDialogPrivate)) 

/* Names of Widgets in Glade file */
#define EDIT_NETWORK_PROFILE_DLG    "edit_network_profile_dlg"
#define NET_CONF_TREEVIEW           "network_profile_table"
#define PROFILE_NAME_ENTRY          "profile_name_entry"
#define EDIT_PROFILE_NAME_ENTRY     "edit_profile_name_entry"
#define PROFILE_OK_BTN              "profile_ok_btn"
#define CONNECTIONS_EDIT_BTN        "connections_edit_btn"
#define CONNECTION_ENABLE_BTN       "connection_enable_btn"
#define CONNECTION_DISABLE_BTN      "connection_disable_btn"
#define CONNECTION_MOVE_UP_BTN      "connection_move_up_btn"
#define CONNECTION_MOVE_DOWN_BTN    "connection_move_down_btn"
#define CONNECTION_RENAME_BTN       "connection_rename_btn"
#define CONNECTION_GROUP_BTN        "connection_group_btn"
#define ACTIVATION_MODE_LBL         "activation_mode_lbl"
#define CONNECTION_ACTIVATION_COMBO "connection_activation_combo"

#define TREEVIEW_COLUMN_NUM             "meta:column"
#define BUTTON_IS_GROUP_NEW             "group_button@is_new"
#define COND_MODEL_INFO                 "ncp_group_first_id"
#define NWAM_COND_NCU_GROUP_ID          "ncu_group_id"
#define NWAM_COND_NCU_GROUP_MODE        "ncu_group_mode"
#define NWAM_COND_NCU_ACT_MODE          "ncu_group_activation_mode"

/* Edit Connection Dialog */
#define EDIT_CONNECTIONS_DIALOG         "edit_connections_dialog"
#define CONNECTION_TREEVIEW             "connection_treeview"

struct _NwamProfileDialogPrivate {
	/* Widget Pointers */
    GtkDialog*          edit_network_profile_dlg;
	GtkTreeView*	    net_conf_treeview;

    GtkButton*          profile_ok_btn;
    GtkButton*          connections_edit_btn;
    GtkButton*          connection_enable_btn;	
    GtkButton*          connection_disable_btn;	
    GtkButton*          connection_move_up_btn;	
    GtkButton*          connection_move_down_btn;	
    GtkButton*          connection_rename_btn;
    GtkButton*          connection_group_btn;

    GtkWidget*          profile_name_entry;

    GtkLabel*           activation_mode_lbl;
    GtkComboBox*        connection_activation_combo;

    /* Edit Connections dialog */
    GtkDialog*          edit_connections_dialog;
	GtkTreeView*	    connection_treeview;

    GList*              ncu_list;
    GList*              ncu_rm_list;

	/* Other Data */
    NwamuiDaemon            *daemon;
    NwamCappletDialog*       pref_dialog;
    NwamuiObject*            selected_ncp;
    NwamuiNcu*               selected_ncu; /* Use g_object_set for it */
    nwamui_dialog_purpose_t  purpose;
    gboolean                 update_inprogress;
    gboolean                 manual_expander_flag;
    gboolean                 connection_activation_combo_show_ncu_part;

    gpointer            foreachdata[1];

    gint on_group_num;
    gint off_group_num;
    gint group_num;
    gint on_child_num;
    gint off_child_num;
    gint child_num;
};

enum {
    PROP_SELECTED_NCP = 1,
    PROP_SELECTED_NCU,
};

enum {
	CONNVIEW_INFO = 0
};

static const nwamui_cond_priority_group_mode_t combo_contents_mode_map[] = {
    NWAMUI_COND_PRIORITY_GROUP_MODE_LAST,
    NWAMUI_COND_PRIORITY_GROUP_MODE_LAST,
    NWAMUI_COND_PRIORITY_GROUP_MODE_LAST,
	NWAMUI_COND_PRIORITY_GROUP_MODE_EXCLUSIVE,
	NWAMUI_COND_PRIORITY_GROUP_MODE_SHARED,
	NWAMUI_COND_PRIORITY_GROUP_MODE_ALL,
	NWAMUI_COND_PRIORITY_GROUP_MODE_LAST /* Not to be used directly */
};

static const gchar *combo_contents[] = {
    N_("Always active"),
    N_("Activated by rules"),
    N_("Never active"),
    N_("Exactly one connection is enabled"),
    N_("One or more connections may be enabled"),
    N_("All connections must be enabled"),
    NULL
};

static NwamuiObject *fake_object_in_pri_group = NULL;

static void nwam_pref_init (gpointer g_iface, gpointer iface_data);
static gboolean refresh(NwamPrefIFace *iface, gpointer user_data, gboolean force);
static gboolean apply(NwamPrefIFace *iface, gpointer user_data);
static gboolean cancel(NwamPrefIFace *iface, gpointer user_data);
static gboolean help(NwamPrefIFace *iface, gpointer user_data);
static GtkWindow* dialog_get_window(NwamPrefIFace *iface);
static void set_purpose(NwamPrefIFace *iface, nwamui_dialog_purpose_t purpose);

static void nwam_profile_dialog_finalize(NwamProfileDialog *self);

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
static void nwam_connection_cell_toggled(GtkCellRendererToggle *cell_renderer,
  gchar                 *path,
  gpointer               user_data);

static void net_conf_set_property (GObject         *object,
  guint            prop_id,
  const GValue    *value,
  GParamSpec      *pspec);

static void net_conf_get_property (GObject         *object,
  guint            prop_id,
  GValue          *value,
  GParamSpec      *pspec);
static void edit_profile_changed(NwamProfileDialog *self, NwamuiObject *ncp);

/* Callbacks */
static void object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);
static void ncu_notify_cb (NwamuiNcu *ncu, GParamSpec *arg1, gpointer data);
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
static void condition_view_drag_data_get(GtkWidget        *widget,
  GdkDragContext   *drag_context,
  GtkSelectionData *data,
  guint             info,
  guint             time,
  gpointer          user_data);
static gboolean condition_view_drag_motion(GtkWidget      *widget,
  GdkDragContext *drag_context,
  gint            x,
  gint            y,
  guint           time,
  gpointer        user_data);
static void condition_view_drag_data_received(GtkWidget        *widget,
  GdkDragContext   *drag_context,
  gint              x,
  gint              y,
  GtkSelectionData *data,
  guint             info,
  guint             time,
  gpointer          user_data);

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
static void connections_edit_btn_clicked(GtkButton *button, gpointer user_data );
static void update_widgets(NwamProfileDialog *self, GtkTreeSelection *selection);
static void selection_changed(GtkTreeSelection *selection, gpointer user_data);
static gboolean selection_func(GtkTreeSelection *selection,
  GtkTreeModel *model,
  GtkTreePath *path,
  gboolean path_currently_selected,
  gpointer data);

static void update_rules_from_ncu (NwamProfileDialog* self,
  GParamSpec *arg1,
  gpointer ncu);                /* unused */
static void conditions_connected_toggled_cb(GtkToggleButton *togglebutton, gpointer user_data);
static void conditions_set_status_changed(GtkComboBox* combo, gpointer user_data );
static void conditions_connected_changed(GtkComboBox* combo, gpointer user_data );
static void on_activate_static_menuitems (GtkAction *action, gpointer data);
static void ncp_add_ncu(NwamuiNcp *ncp, NwamuiNcu* ncu, gpointer data);
static void ncp_remove_ncu(NwamuiNcp *ncp, NwamuiNcu* ncu, gpointer data);
static void response_cb(GtkWidget* widget, gint repsonseid, gpointer data);

/* ncu group */
static NwamuiObject* create_group_object( gint group_id,
  nwamui_cond_priority_group_mode_t  group_mode,
  nwamui_cond_activation_mode_t      act_mode );
#define NCU_PRI_GROUP_GET_PATH(model, group_id)         \
    ncu_pri_group_get_path(model, group_id, 0, NULL)
static GtkTreePath* ncu_pri_group_get_path(GtkTreeModel *model, gint group_id, gint group_mode, GtkTreeIter *parent);
static gboolean ncu_pri_group_get_iter(GtkTreeModel *model, gint group_id, gint group_mode, GtkTreeIter *parent);
static void ncu_pri_group_update(GtkTreeModel *model);
static void ncu_pri_treeview_add(NwamuiObject *object, GtkTreeView *treeview);
static void ncu_pri_treeview_remove(GtkTreeView *treeview, NwamuiObject *object);
static void capplet_tree_store_move_children(GtkTreeStore *model,
  GtkTreePath *target_path,
  GtkTreePath *source_path,
  GtkTreeModelForeachFunc func,
  gpointer user_data);
static GtkTreeRowReference *ref_ncu_group_node(GtkTreeModel *model, GtkTreePath *path);
static void handle_ncu_group_node(GtkTreeModel *model, GtkTreeRowReference *group_rr);
static gboolean foreach_set_group_mode(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer user_data);
static void set_group_object_group_mode(GtkTreeModel *model, GtkTreePath *path, nwamui_cond_priority_group_mode_t mode);

G_DEFINE_TYPE_EXTENDED (NwamProfileDialog,
  nwam_profile_dialog,
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
    iface->dialog_get_window = dialog_get_window;
    iface->set_purpose = set_purpose;
}

static void
nwam_profile_dialog_class_init(NwamProfileDialogClass *klass)
{
	/* Pointer to GObject Part of Class */
	GObjectClass *gobject_class = (GObjectClass*) klass;
    gobject_class->set_property = net_conf_set_property;
    gobject_class->get_property = net_conf_get_property;

    g_type_class_add_private (klass, sizeof(NwamProfileDialogPrivate));

	/* Override Some Function Pointers */
	gobject_class->finalize = (void (*)(GObject*)) nwam_profile_dialog_finalize;

    /* Create some properties */
    g_object_class_install_property (gobject_class,
      PROP_SELECTED_NCP,
      g_param_spec_object ("selected_ncp",
        _("selected_ncp"),
        _("selected_ncp"),
        NWAMUI_TYPE_OBJECT,
        G_PARAM_READWRITE));

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
    NwamProfileDialogPrivate* prv  = GET_PRIVATE(object);
    NwamProfileDialog*        self = NWAM_PROFILE_DIALOG(object);

    switch (prop_id) {
    case PROP_SELECTED_NCP: {
        NwamuiObject *new_ncp = NWAMUI_OBJECT(g_value_get_object(value));

        if (new_ncp != prv->selected_ncp) {

            if (prv->selected_ncp) {
                g_signal_handlers_disconnect_matched(prv->selected_ncp,
                  G_SIGNAL_MATCH_DATA,
                  0,
                  NULL,
                  NULL,
                  NULL,
                  (gpointer)self);

                g_object_unref (prv->selected_ncp);
            }

            prv->selected_ncp = new_ncp;

            if (prv->selected_ncp) {
                g_object_ref(prv->selected_ncp);

                g_signal_connect(prv->selected_ncp, "add",
                  G_CALLBACK(ncp_add_ncu), (gpointer) self);

                g_signal_connect(prv->selected_ncp, "remove",
                  G_CALLBACK(ncp_remove_ncu), (gpointer) self);

                /* Update edit ncp combo */
                edit_profile_changed(self, prv->selected_ncp);
            }
        }
    }
        break;
    case PROP_SELECTED_NCU: {
        if (prv->selected_ncu) {
            g_signal_handlers_disconnect_by_func (prv->selected_ncu,
              (gpointer)ncu_notify_cb,
              (gpointer)self);
            g_object_unref (prv->selected_ncu);
        }
        prv->selected_ncu = g_value_dup_object (value);
        if (prv->selected_ncu) {
            g_signal_connect (prv->selected_ncu,
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
    NwamProfileDialog*       self = NWAM_PROFILE_DIALOG(object);

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
nwam_compose_tree_view (NwamProfileDialog *self)
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
      "level-indentation", 24,
      NULL);

    if (nwamui_util_is_debug_mode()) {
        g_object_set (view,
          "show-expanders", TRUE,
          NULL);
    }

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

    /* DnD */
    static GtkTargetEntry targets[] = {
        { "ROW", GTK_TARGET_SAME_WIDGET, 0 }
    };

    gtk_tree_view_enable_model_drag_source(view,
      GDK_BUTTON1_MASK,
      targets,
      G_N_ELEMENTS(targets),
      GDK_ACTION_DEFAULT);

    gtk_tree_view_enable_model_drag_dest(view,
      targets,
      G_N_ELEMENTS(targets),
      GDK_ACTION_DEFAULT | GDK_ACTION_COPY);

    g_signal_connect(view,
      "drag-data-get",
      G_CALLBACK(condition_view_drag_data_get),
      (gpointer)self);

    g_signal_connect(view,
      "drag-motion",
      G_CALLBACK(condition_view_drag_motion),
      (gpointer)self);

    g_signal_connect(view,
      "drag-data-received",
      G_CALLBACK(condition_view_drag_data_received),
      (gpointer)self);


	/* column info */
    col = capplet_column_new(view,
      "title",_("Details"),
      "expand", TRUE,
      "resizable", FALSE,
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
    NwamProfileDialog*       self = NWAM_PROFILE_DIALOG(data);
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
nwam_profile_dialog_init(NwamProfileDialog *self)
{
    NwamProfileDialogPrivate*    prv = GET_PRIVATE(self);
	self->prv = prv;

    prv->daemon = nwamui_daemon_get_instance();

    if (!fake_object_in_pri_group)
        fake_object_in_pri_group = g_object_new(NWAMUI_TYPE_OBJECT, NULL);

	/* Iniialise pointers to important widgets */
	prv->edit_network_profile_dlg = GTK_DIALOG(nwamui_util_glade_get_widget(EDIT_NETWORK_PROFILE_DLG));
	prv->net_conf_treeview = GTK_TREE_VIEW(nwamui_util_glade_get_widget(NET_CONF_TREEVIEW));

    prv->profile_name_entry = nwamui_util_glade_get_widget(PROFILE_NAME_ENTRY);

    prv->profile_ok_btn = GTK_BUTTON(nwamui_util_glade_get_widget(PROFILE_OK_BTN));	
    prv->connections_edit_btn = GTK_BUTTON(nwamui_util_glade_get_widget(CONNECTIONS_EDIT_BTN));	
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

    g_signal_connect(prv->edit_network_profile_dlg,
      "response", (GCallback)response_cb, (gpointer)self);

    g_signal_connect(prv->connections_edit_btn,
      "clicked", G_CALLBACK(connections_edit_btn_clicked), (gpointer)self);
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
    /* { */
    /*     NwamuiObject *ncp = nwamui_daemon_get_active_ncp(prv->daemon); */
    /*     if (ncp) { */
    /*         nwam_pref_refresh(NWAM_PREF_IFACE(self), ncp, TRUE); */
    /*         g_object_unref(ncp); */
    /*     } */
    /* } */
}

/**
 * nwam_profile_dialog_new:
 * @returns: a new #NwamProfileDialog.
 *
 * Creates a new #NwamProfileDialog with an empty ncu
 **/
NwamProfileDialog*
nwam_profile_dialog_new(NwamCappletDialog *pref_dialog)
{
	NwamProfileDialog *self =  NWAM_PROFILE_DIALOG(g_object_new(NWAM_TYPE_PROFILE_DIALOG, NULL));
        
    /* FIXME - Use GOBJECT Properties */
    self->prv->pref_dialog = g_object_ref( G_OBJECT( pref_dialog ));

    return( self );
}

/**
 * refresh:
 *
 * Refresh #NwamProfileDialog with the new connections.
 **/
static gboolean
refresh(NwamPrefIFace *iface, gpointer user_data, gboolean force)
{
    NwamProfileDialog*         self   = NWAM_PROFILE_DIALOG( iface );
    NwamProfileDialogPrivate*  prv    = GET_PRIVATE(iface);
    NwamuiObject              *refresh_ncp;

    refresh_ncp = user_data ? g_object_ref(NWAMUI_NCP(user_data)) : nwamui_daemon_get_active_ncp(prv->daemon);

    if (force) {
        gtk_entry_set_text(GTK_ENTRY(prv->profile_name_entry), nwamui_object_get_name(refresh_ncp));
        gtk_widget_set_sensitive(prv->profile_name_entry, nwamui_object_can_rename(refresh_ncp));
    }

    g_object_set(self, "selected_ncp", refresh_ncp, NULL);
    g_object_unref(refresh_ncp);
    
    return( TRUE );
}

static gboolean
apply(NwamPrefIFace *iface, gpointer user_data)
{
    NwamProfileDialog        *self   = NWAM_PROFILE_DIALOG( iface );
    NwamProfileDialogPrivate *prv    = GET_PRIVATE(iface);
    GtkTreeIter               iter;

    if (prv->selected_ncp && nwamui_object_is_modifiable(prv->selected_ncp)) {
        capplet_model_1_level_foreach(gtk_tree_view_get_model(prv->net_conf_treeview),
          NULL,
          foreach_set_group_mode,
          self,
          &iter);

        /* rename */
        if (GTK_WIDGET_IS_SENSITIVE(prv->profile_name_entry)) {
            nwamui_object_set_name(prv->selected_ncp, gtk_entry_get_text(GTK_ENTRY(prv->profile_name_entry)));
        }
        /* This will commit all NCU children. */
        nwamui_object_commit(prv->selected_ncp);
    }

    g_object_set(self, "selected_ncp", NULL, NULL);

    return(TRUE);
}

static gboolean
cancel(NwamPrefIFace *iface, gpointer user_data)
{
    NwamProfileDialogPrivate *prv  = GET_PRIVATE(iface);
    NwamProfileDialog        *self = NWAM_PROFILE_DIALOG( iface );

    nwamui_object_reload(prv->selected_ncp);
    g_object_set(self, "selected_ncp", NULL, NULL);

    return(TRUE);
}

static void
set_purpose(NwamPrefIFace *iface, nwamui_dialog_purpose_t purpose)
{
    NwamProfileDialogPrivate* prv         = GET_PRIVATE(iface);
    const gchar*              purpose_str = NULL;

    switch( purpose ) {
    case NWAMUI_DIALOG_PURPOSE_ADD:
        purpose_str = _("Add Network Profile dialog");
        break;
    case NWAMUI_DIALOG_PURPOSE_EDIT:
        purpose_str = _("Edit Network Profile dialog");
        break;
    default:
        g_assert_not_reached();
        break;
    }
    
    prv->purpose = purpose;

    if ( purpose_str != NULL ) {
        gtk_window_set_title(GTK_WINDOW(prv->edit_network_profile_dlg), purpose_str);
    }
}

static gboolean
help(NwamPrefIFace *iface, gpointer user_data)
{
    g_debug("NwamProfileDialog: Help");
    nwamui_util_show_help (HELP_REF_PROFILES_EDITING);
}

static GtkWindow*
dialog_get_window(NwamPrefIFace *iface)
{
    NwamProfileDialogPrivate* prv         = GET_PRIVATE(iface);

    return GTK_WINDOW(prv->edit_network_profile_dlg);
}

static void
nwam_profile_dialog_finalize(NwamProfileDialog *self)
{
    NwamProfileDialogPrivate    *prv = GET_PRIVATE(self);
    g_object_unref(G_OBJECT(self->prv->connections_edit_btn));	
    g_object_unref(G_OBJECT(self->prv->connection_enable_btn));	
    g_object_unref(G_OBJECT(self->prv->connection_disable_btn));	
    g_object_unref(G_OBJECT(self->prv->connection_move_up_btn));	
    g_object_unref(G_OBJECT(self->prv->connection_move_down_btn));	
    g_object_unref(G_OBJECT(self->prv->connection_rename_btn));	
    g_object_unref(G_OBJECT(self->prv->connection_group_btn));	

    if (prv->pref_dialog) {
        g_object_unref(prv->pref_dialog);
    }

    if (prv->selected_ncu) {
        g_object_unref(prv->selected_ncu);
    }

    if (prv->selected_ncp) {
        g_object_unref(prv->selected_ncp);
    }

    g_object_unref(prv->daemon);

	G_OBJECT_CLASS(nwam_profile_dialog_parent_class)->finalize(G_OBJECT(self));
}

/* Callbacks */

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
            const gchar* ncu_text;
            gchar*       ncu_markup;
            gchar*       ncu_phy_addr;
            gchar*       info_string;

            ncu_text = nwamui_ncu_get_display_name(ncu);
            ncu_phy_addr = nwamui_ncu_get_phy_address(ncu);

            info_string = nwamui_ncu_get_configuration_summary_string( ncu );

            if (ncu_phy_addr) {
                ncu_markup= g_strdup_printf(_("<b>%s</b>\n<small>%s, %s</small>"), ncu_text, info_string, ncu_phy_addr );
            } else {
                ncu_markup= g_strdup_printf(_("<b>%s</b>\n<small>%s</small>"), ncu_text, info_string );
            }
            g_object_set(G_OBJECT(renderer),
              "sensitive", TRUE,
              "markup", ncu_markup,
              NULL);

            if ( ncu_phy_addr != NULL )
                g_free( ncu_phy_addr );

            g_free (info_string);
            g_free( ncu_markup );
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

                first_path = NCU_PRI_GROUP_GET_PATH(model, ALWAYS_ON_GROUP_ID);

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

            if ( nwamui_util_is_debug_mode()  ) { /* Append the group_id */
                gchar *id;
                gint    group_id = 0;
                group_id = (gint)g_object_get_data(G_OBJECT(ncu), NWAM_COND_NCU_GROUP_ID);
                id = g_strdup_printf("%s: %d", str ? str:"<i>Unknown</i>", group_id);
                g_object_set(G_OBJECT(renderer),
                  "sensitive", TRUE,
                  "markup", id,
                  NULL);
                g_free(id);
            }
            else {
                g_object_set(G_OBJECT(renderer),
                  "sensitive", TRUE,
                  "markup", str ? str:"<i>Unknown</i>",
                  NULL);
            }
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
    NwamProfileDialogPrivate   *prv = GET_PRIVATE(data);
    NwamuiObject               *obj;
    const gchar                *name;
    gboolean                    is_active = FALSE;
    gboolean                    is_in_ncp;
    NwamuiObject               *found_object = NULL;

    if (prv->selected_ncp) {

        gtk_tree_model_get(model, iter, 0, &obj, -1);

        /* We are using the NCU list in Automatic NCP. */
        if (g_list_find(prv->ncu_list, obj)) {
            is_in_ncp = TRUE;
        } else {
            is_in_ncp = FALSE;
        }

        /* prv->ncu_list only contains ncus of automaic ncp, so we have to find the
         * real NCU. */
        name = nwamui_object_get_name(obj);
        found_object = NWAMUI_OBJECT(nwamui_ncp_get_ncu_by_device_name(NWAMUI_NCP(prv->selected_ncp), name));
        if ( found_object != NULL ) {
            /* Now we change to interface state */
            if (nwamui_object_get_nwam_state(found_object, NULL, NULL) == NWAM_STATE_ONLINE) {
                is_active = TRUE;
            }
            /* Safely unref though we still use this pointer. */
            g_object_unref(found_object);
        }

        /* Set sensitivity based on whether it's active or not */
        g_object_set( G_OBJECT(renderer), "sensitive", !is_active, NULL );

        if (GTK_IS_CELL_RENDERER_TOGGLE(renderer)) {
            g_object_set(renderer, "active", is_in_ncp, NULL);
            g_object_set( G_OBJECT(renderer), "activatable", !is_active, NULL );
        } else {
            gchar       *disp_text;
            const gchar *disp_name = nwamui_ncu_get_display_name(NWAMUI_NCU(obj));

            if ( is_active ) {
                disp_text = g_strdup_printf(_("%s  (active)"), disp_name);
            }
            else {
                disp_text = g_strdup(disp_name);
            }

            g_object_set(renderer, "text", disp_text, NULL);

            g_free(disp_text);
        }
        g_object_unref(obj);
    }
}

static void
nwam_connection_cell_toggled(GtkCellRendererToggle *cell_renderer,
  gchar                 *path_str,
  gpointer               user_data)
{
    NwamProfileDialogPrivate*    prv = GET_PRIVATE(user_data);
    GtkTreeIter iter;
    GtkTreeModel *model;
    GtkTreePath  *path;
    NwamuiNcu*  ncu;
    const gchar *name;
    const gchar *ncu_name;
    GList *hit = NULL;

    model = gtk_tree_view_get_model(prv->connection_treeview);
    path = gtk_tree_path_new_from_string(path_str);
    gtk_tree_model_get_iter(model, &iter, path);
    gtk_tree_model_get(model, &iter, 0, &ncu, -1);

    name = nwamui_object_get_name(NWAMUI_OBJECT(ncu));

    if (gtk_cell_renderer_toggle_get_active(cell_renderer)) {
        prv->ncu_list = g_list_remove(prv->ncu_list, ncu);
        prv->ncu_rm_list = g_list_prepend(prv->ncu_rm_list, ncu);
    } else {
        if (g_list_find(prv->ncu_list, ncu) == NULL) {
            prv->ncu_rm_list = g_list_remove(prv->ncu_rm_list, ncu);
            prv->ncu_list = g_list_prepend(prv->ncu_list, ncu);
        }
    }
    gtk_tree_model_row_changed(model, path, &iter);
    g_object_unref(ncu);
    gtk_tree_path_free(path);
}

/*
 * Enabled Toggle Button was toggled
 */
static void
nwam_net_pref_connection_enabled_toggled_cb(    GtkCellRendererToggle *cell_renderer,
                                                gchar                 *path,
                                                gpointer               user_data) 
{
  	NwamProfileDialog*       self = NWAM_PROFILE_DIALOG(user_data);
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
                    nwamui_object_set_active(NWAMUI_OBJECT(ncu), FALSE);
                    //nwamui_ncu_set_selection_rules_enabled (ncu, FALSE);
                } else {
                    //nwamui_ncu_set_selection_rules_enabled (ncu, TRUE);
                }
            } else {
                nwamui_object_set_active(NWAMUI_OBJECT(ncu), TRUE);
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
  	NwamProfileDialog*       self = NWAM_PROFILE_DIALOG(data);

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

                gtk_entry_set_text( entry, nwamui_object_get_name(NWAMUI_OBJECT(ncu)));
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
    NwamProfileDialogPrivate *prv  = GET_PRIVATE(data);
    GtkTreeModel             *model;
    GtkTreePath              *path = gtk_tree_path_new_from_string (path_string);
    GtkTreeIter               iter;

    gint column = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (cell), TREEVIEW_COLUMN_NUM));

	model = gtk_tree_view_get_model(prv->net_conf_treeview);
    if ( gtk_tree_model_get_iter (model, &iter, path))
    switch (column) {
        case CONNVIEW_INFO:      {
            gpointer    connection;
            NwamuiNcu*  ncu;
            gtk_tree_model_get(model, &iter, 0, &connection, -1);

            if (NWAMUI_IS_NCU( connection )) {
                ncu  = NWAMUI_NCU( connection );

                nwamui_object_set_name(NWAMUI_OBJECT(ncu), new_text);
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

static gboolean
static_group_handle_fake_node(GtkTreeModel *model, GtkTreePath *path, gboolean creating)
{
    static GtkTreeRowReference *on_fake_node_rr = NULL;
    static GtkTreeRowReference *off_fake_node_rr = NULL;
    GtkTreeRowReference **fake_node_rrp = NULL;
    GtkTreePath *fake_path;

    g_assert(GTK_IS_TREE_STORE(model));

    if (gtk_tree_path_compare(path,
        NCU_PRI_GROUP_GET_PATH(model, ALWAYS_ON_GROUP_ID)) == 0) {
        fake_node_rrp = &on_fake_node_rr;
    } else if (gtk_tree_path_compare(path,
        NCU_PRI_GROUP_GET_PATH(model, ALWAYS_OFF_GROUP_ID)) == 0) {
        fake_node_rrp = &off_fake_node_rr;
    } else {
        return FALSE;
    }

    if ((*fake_node_rrp == NULL || !gtk_tree_row_reference_valid(*fake_node_rrp)) && creating) {
        /* Add fake node */
        GtkTreeIter parent_iter;
        GtkTreeIter temp;
        gtk_tree_model_get_iter(model, &parent_iter, path);

        if (!gtk_tree_model_iter_has_child(model, &parent_iter)) {
            CAPPLET_TREE_STORE_ADD(model, &parent_iter, fake_object_in_pri_group, &temp);
            fake_path = gtk_tree_model_get_path(model, &temp);
            *fake_node_rrp = gtk_tree_row_reference_new(model, fake_path);
            gtk_tree_path_free(fake_path);
        }
    } else if (*fake_node_rrp && !creating){
        GtkTreeIter temp;
        /* Remove fake node */
        fake_path = gtk_tree_row_reference_get_path(*fake_node_rrp);
        if (fake_path) {
            gtk_tree_model_get_iter(model, &temp, fake_path);
            gtk_tree_store_remove(GTK_TREE_STORE(model), &temp);
        }
        gtk_tree_row_reference_free(*fake_node_rrp);
        *fake_node_rrp = NULL;
    }
    return TRUE;
}

static void condition_view_drag_data_get(GtkWidget        *widget,
  GdkDragContext   *drag_context,
  GtkSelectionData *data,
  guint             info,
  guint             time,
  gpointer          user_data)
{
    /* Do nothing */
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));

    gtk_selection_data_set(data, GDK_NONE,
      sizeof(GtkTreeModel*), (guchar*)&model,
      sizeof(GtkTreeModel*));
}

static gboolean
condition_view_drag_motion(GtkWidget      *widget,
  GdkDragContext *drag_context,
  gint            x,
  gint            y,
  guint           time,
  gpointer        user_data)
{
    GtkTreePath  *path      = NULL;

    if (gtk_tree_selection_count_selected_rows(gtk_tree_view_get_selection(GTK_TREE_VIEW(widget))) > 0) {
        if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), x, y, &path, NULL, NULL, NULL)) {

            gint depth = gtk_tree_path_get_depth(path);

            gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(widget), path, NULL, FALSE, 0, 0);

            if (depth >= 1) {
                if (depth > 1) {
                    gtk_tree_path_up(path);
                }

                gtk_tree_view_set_drag_dest_row(GTK_TREE_VIEW(widget), path, 
                  GTK_TREE_VIEW_DROP_INTO_OR_AFTER);

                gtk_tree_path_free(path);
                gdk_drag_status(drag_context, GDK_ACTION_COPY, time);
                return TRUE;
            }
            gtk_tree_path_free(path);
        }
    }
    gdk_drag_status(drag_context, 0, time);
    return FALSE;
}

static void
condition_view_drag_data_received(GtkWidget        *widget,
  GdkDragContext   *drag_context,
  gint              x,
  gint              y,
  GtkSelectionData *data,
  guint             info,
  guint             time,
  gpointer          user_data)
{
    GtkTreePath  *path      = NULL;
    GtkTreePath  *src_path  = NULL;
    GtkTreeModel *model;
    GtkTreeModel *src_model = NULL;
    gboolean      flag      = FALSE;

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
    if (GDK_NONE == gtk_selection_data_get_data_type(data) &&
      data->format == sizeof(GtkTreeModel*) &&
      data->length == sizeof(GtkTreeModel*)) {
        src_model = *(GtkTreeModel**)data->data;
    } else {
        return;
    }

    if (src_model == (gpointer)model) {
/*         gtk_tree_view_get_drag_dest_row(GTK_TREE_VIEW(widget), */
/*           &path, */
/*           NULL); */
/*         if (path) { */
        if (gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(widget), x, y, &path, NULL)) {
            gint depth = gtk_tree_path_get_depth(path);

            if (depth >= 1) {
                GtkTreeRowReference *group_rr = NULL;
                GList*               rows     = NULL;
                GList*               rowrefs  = NULL;

                rows = gtk_tree_selection_get_selected_rows(gtk_tree_view_get_selection(GTK_TREE_VIEW(widget)), &model);

                for (; rows; rows = g_list_delete_link(rows, rows)) {
                    rowrefs = g_list_prepend(rowrefs, gtk_tree_row_reference_new(model, (GtkTreePath*)rows->data));
                    gtk_tree_path_free((GtkTreePath*)rows->data);
                }

                if (depth > 1)
                    gtk_tree_path_up(path);

                static_group_handle_fake_node(model, path, FALSE);

                for (; rowrefs; rowrefs = g_list_delete_link(rowrefs, rowrefs)) {
                    src_path = gtk_tree_row_reference_get_path((GtkTreeRowReference*)rowrefs->data);
                    /* Monitor the group node. */
                    group_rr = ref_ncu_group_node(model, src_path);

                    /* Only copy non-group nodes. */
                    capplet_tree_store_move_children(GTK_TREE_STORE(model),
                      path,
                      src_path,
                      NULL,
                      NULL);

                    /* Handle group node. */
                    handle_ncu_group_node(model, group_rr);
                    gtk_tree_row_reference_free(group_rr);
                    gtk_tree_row_reference_free((GtkTreeRowReference*)rowrefs->data);
                }
                flag = TRUE;
                static_group_handle_fake_node(model, path, TRUE);
                gtk_tree_view_expand_all(GTK_TREE_VIEW(widget));
            }
            gtk_tree_path_free(path);
        }
    }
    gtk_drag_finish(drag_context, flag, FALSE, time);
    return;
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
	NwamProfileDialog* self = NWAM_PROFILE_DIALOG(data);
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
update_rules_from_ncu (NwamProfileDialog* self,
  GParamSpec *arg1,
  gpointer data)
{
    NwamProfileDialogPrivate*    prv = GET_PRIVATE(self);
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
	NwamProfileDialog*           self = NWAM_PROFILE_DIALOG(user_data);
    NwamProfileDialogPrivate*    prv = GET_PRIVATE(self);
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

static gboolean
ncu_find_gt_name(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer user_data)
{
    NwamuiObject *ins_object = NWAMUI_OBJECT(user_data);
    NwamuiObject *object;
    nwamui_ncu_type_t ins_type;
    nwamui_ncu_type_t type;
    const gchar *ins_name;
    const gchar *name;
    gint ret;

    gtk_tree_model_get(model, iter, 0, &object, -1);

    ins_type = nwamui_ncu_get_ncu_type(NWAMUI_NCU(ins_object));
    type = nwamui_ncu_get_ncu_type(NWAMUI_NCU(object));
    if (ins_type < type) {
        g_object_unref(object);
        return TRUE;
    } else if(ins_type > type) {
        g_object_unref(object);
        return FALSE;
    }

    name = nwamui_object_get_name(object);
    ins_name = nwamui_object_get_name(ins_object);

    ret = g_ascii_strcasecmp(name, (gchar*)user_data);

    g_object_unref(object);

    return ret >= 0;
}

/**
 * capplet_tree_store_move_children:
 * Note only move children. If source has children, then move it, else move source.
 */
void
capplet_tree_store_move_children(GtkTreeStore *model,
  GtkTreePath *target_path,
  GtkTreePath *source_path,
  GtkTreeModelForeachFunc func,
  gpointer user_data)
{
	GtkTreeIter t_iter;
	GtkTreeIter s_iter;
    GtkTreeIter target;
    GtkTreeIter source;
	GtkTreeIter sibling;
    gint source_depth;
    NwamuiObject *object;

    gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &source, source_path);
    gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &target, target_path);
    source_depth = gtk_tree_path_get_depth(source_path);

    g_debug("%s source depth is %d", __func__, source_depth);
    g_assert(source_depth > 0 && source_depth <= 2);

    /* Consider the position. */
	if (gtk_tree_model_iter_children(GTK_TREE_MODEL(model), &s_iter, &source)) {
		do {

            gtk_tree_model_get(GTK_TREE_MODEL(model), &s_iter, 0, &object, -1);

#if 0
            if (capplet_model_1_level_foreach(GTK_TREE_MODEL(model), &target,
                ncu_find_gt_name, (gpointer)object, &sibling)) {
                TREE_STORE_CP_OBJECT_BEFORE(model, &s_iter, &target, &sibling, &t_iter);
            } else {
                TREE_STORE_CP_OBJECT_BEFORE(model, &s_iter, &target, NULL, &t_iter);
            }
#else
            TREE_STORE_CP_OBJECT_BEFORE(model, &s_iter, &target, NULL, &t_iter);
#endif
            g_object_unref(object);

		} while (gtk_tree_store_remove(GTK_TREE_STORE(model), &s_iter));
	} else if (source_depth > 1) {
        gtk_tree_model_get(GTK_TREE_MODEL(model), &source, 0, &object, -1);

#if 0
        if (capplet_model_1_level_foreach(GTK_TREE_MODEL(model), &target,
            ncu_find_gt_name, (gpointer)object, &sibling)) {
            TREE_STORE_CP_OBJECT_BEFORE(model, &source, &target, &sibling, &t_iter);
        } else {
            TREE_STORE_CP_OBJECT_BEFORE(model, &source, &target, NULL, &t_iter);
        }
#else
        TREE_STORE_CP_OBJECT_BEFORE(model, &source, &target, NULL, &t_iter);
#endif
        g_object_unref(object);
        gtk_tree_store_remove(GTK_TREE_STORE(model), &source);
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

    if (!static_group_handle_fake_node(model, parent_path, TRUE)) {
        gtk_tree_model_get_iter(model, &parent_iter, parent_path);
        if (!gtk_tree_model_iter_has_child(model, &parent_iter)) {
            gtk_tree_store_remove(GTK_TREE_STORE(model), &parent_iter);
            ncu_pri_group_update(model);
        }
    }
}


static gboolean
foreach_select_object(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer user_data)
{
    NwamProfileDialogPrivate*    prv = GET_PRIVATE(user_data);
    NwamuiObject *obj;

    gtk_tree_model_get(model, iter, 0, &obj, -1);
    g_assert(obj);
    if (obj == prv->foreachdata[0]) {
        gtk_tree_view_scroll_to_cell(prv->net_conf_treeview, path, NULL, FALSE, 0, 0);
        gtk_tree_selection_select_path(gtk_tree_view_get_selection(prv->net_conf_treeview), path);
        prv->foreachdata[0] = NULL;
    }
    g_object_unref(obj);
    return prv->foreachdata[0] == NULL;
}

/*
 * for_each_set_group_mode:
 * @path: must be NULL.
 * @iter: the group object.
 *
 * Set @mode to all the group members.
 */
static gboolean
foreach_set_group_mode(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer user_data)
{
    gboolean valid;
    GtkTreeIter child_iter;
    gint group_id;
    nwamui_cond_priority_group_mode_t mode;
    NwamuiObject *obj;

    gtk_tree_model_get(model, iter, 0, &obj, -1);
    g_assert(obj);

    group_id = (gint)g_object_get_data(G_OBJECT(obj), NWAM_COND_NCU_GROUP_ID);
    mode = (nwamui_cond_priority_group_mode_t)g_object_get_data(G_OBJECT(obj), NWAM_COND_NCU_GROUP_MODE);
    g_assert(mode < NWAMUI_COND_PRIORITY_GROUP_MODE_LAST && mode >= NWAMUI_COND_PRIORITY_GROUP_MODE_EXCLUSIVE);

    g_object_unref(obj);

    for (valid = gtk_tree_model_iter_children(model, &child_iter, iter);
         valid;
         valid = gtk_tree_model_iter_next(model, &child_iter)) {
        gtk_tree_model_get(model, &child_iter, 0, &obj, -1);
        if (obj) {
            if (obj != fake_object_in_pri_group) {
                switch (group_id) {
                case ALWAYS_ON_GROUP_ID:
                case ALWAYS_OFF_GROUP_ID:
                    nwamui_object_set_activation_mode(obj, NWAMUI_COND_ACTIVATION_MODE_MANUAL);
                    nwamui_object_set_enabled(NWAMUI_OBJECT(obj), group_id == ALWAYS_ON_GROUP_ID);
                    break;
                default:
                    nwamui_ncu_set_priority_group(NWAMUI_NCU(obj), group_id);
                    nwamui_object_set_activation_mode(obj, NWAMUI_COND_ACTIVATION_MODE_PRIORITIZED);
                    nwamui_ncu_set_priority_group_mode(NWAMUI_NCU(obj), mode);
                    break;
                }
            }
            g_object_unref(obj);
        }
    }
    return FALSE;
}

/*
 * set_group_object_group_mode:
 * @path: either a group node or a child node.
 *
 * Set @mode to the group object. The ncu members are not touched.
 */
static void
set_group_object_group_mode(GtkTreeModel *model, GtkTreePath *path, nwamui_cond_priority_group_mode_t mode)
{
    GtkTreeIter iter;
    GtkTreeIter child_iter;

    g_assert(mode < NWAMUI_COND_PRIORITY_GROUP_MODE_LAST && mode >= NWAMUI_COND_PRIORITY_GROUP_MODE_EXCLUSIVE);

    if (gtk_tree_path_get_depth(path) == 2) {
        gtk_tree_path_up(path);
    }
    if (gtk_tree_model_get_iter(model, &iter, path)) {
        NwamuiObject *obj;
        gtk_tree_model_get(model, &iter, 0, &obj, -1);
        if (obj) {
            g_object_set_data_full(G_OBJECT(obj), NWAM_COND_NCU_GROUP_MODE, 
              GINT_TO_POINTER(mode), NULL);
            g_object_unref(obj);
        }
        gtk_tree_model_row_changed(model, path, &iter);
    }
}

static void
on_button_clicked(GtkButton *button, gpointer user_data)
{
    NwamProfileDialog*         self          = NWAM_PROFILE_DIALOG(user_data);
    NwamProfileDialogPrivate*  prv           = GET_PRIVATE(self);
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
        GtkTreePath *parent_path;
        gboolean is_new_group;

        is_new_group = (gboolean)g_object_get_data(G_OBJECT(prv->connection_group_btn), BUTTON_IS_GROUP_NEW);

        for (; rows; rows = g_list_delete_link(rows, rows)) {
            rowrefs = g_list_prepend(rowrefs, gtk_tree_row_reference_new(model, (GtkTreePath*)rows->data));
            gtk_tree_path_free((GtkTreePath*)rows->data);
        }

        if (is_new_group) {
            /* New group */
            parent_path = NCU_PRI_GROUP_GET_PATH(model, ALWAYS_OFF_GROUP_ID - 1);
            ncu_pri_group_update(model);
        } else {
            /* Delete group */
            parent_path = NCU_PRI_GROUP_GET_PATH(model, ALWAYS_OFF_GROUP_ID);
            static_group_handle_fake_node(model, parent_path, FALSE);
        }

        for (; rowrefs; rowrefs = g_list_delete_link(rowrefs, rowrefs)) {
            GtkTreePath *path;
            GtkTreeRowReference *group_rr = NULL;

            path = gtk_tree_row_reference_get_path((GtkTreeRowReference*)rowrefs->data);
            /* Monitor the group node. */
            group_rr = ref_ncu_group_node(model, path);

            /* Only copy non-group nodes. */
            capplet_tree_store_move_children(GTK_TREE_STORE(model),
              parent_path,
              path,
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

            GtkTreePath *parent_path = gtk_tree_path_copy(path);
            GtkTreeRowReference *group_rr = NULL;

            /* Monitor the group node. */
            if (gtk_tree_path_get_depth(parent_path) > 1)
                gtk_tree_path_up(parent_path);
            group_rr = ref_ncu_group_node(model, parent_path);

            /* Find the previous parent path. */
            gtk_tree_path_prev(parent_path);
            static_group_handle_fake_node(model, parent_path, FALSE);

            /* Only copy non-group nodes. */
            capplet_tree_store_move_children(GTK_TREE_STORE(model),
              parent_path,
              path,
              NULL,
              NULL);

            /* Handle group node. */
            handle_ncu_group_node(model, group_rr);
            gtk_tree_row_reference_free(group_rr);
            static_group_handle_fake_node(model, parent_path, TRUE);

            {
                GtkTreeIter parent;
                gtk_tree_model_get_iter(model, &parent, parent_path);
                prv->foreachdata[0] = (gpointer)object;
                capplet_model_1_level_foreach(model,
                  &parent,
                  foreach_select_object,
                  self,
                  &iter);
            }
            gtk_tree_path_free(parent_path);

            update_widgets(self, gtk_tree_view_get_selection(prv->net_conf_treeview));
            gtk_tree_view_expand_all(prv->net_conf_treeview);

        } else if (button == (gpointer)prv->connection_move_down_btn) {
            GtkTreePath *parent_path = gtk_tree_path_copy(path);
            GtkTreeRowReference *group_rr = NULL;
            GtkTreeIter    parent;

            /* Monitor the group node. */
            if (gtk_tree_path_get_depth(parent_path) > 1)
                gtk_tree_path_up(parent_path);
            group_rr = ref_ncu_group_node(model, parent_path);

            /* Find the next parent path. */
            gtk_tree_path_next(parent_path);
            gtk_tree_model_get_iter(model, &parent, parent_path);
            static_group_handle_fake_node(model, parent_path, FALSE);

            /* Only copy non-group nodes. */
            capplet_tree_store_move_children(GTK_TREE_STORE(model),
              parent_path,
              path,
              NULL,
              NULL);

            /* Handle group node. */
            handle_ncu_group_node(model, group_rr);
            gtk_tree_row_reference_free(group_rr);
            static_group_handle_fake_node(model, parent_path, TRUE);

            {
                GtkTreeIter parent;
                gtk_tree_model_get_iter(model, &parent, parent_path);
                prv->foreachdata[0] = (gpointer)object;
                capplet_model_1_level_foreach(model,
                  &parent,
                  foreach_select_object,
                  self,
                  &iter);
            }
            gtk_tree_path_free(parent_path);

            update_widgets(self, gtk_tree_view_get_selection(prv->net_conf_treeview));
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
        } else if (button == (gpointer)prv->connection_enable_btn ||
          button == (gpointer)prv->connection_disable_btn) {
            GtkTreeIter parent;
            GtkTreeIter sibling;
            GtkTreeIter temp;
            GtkTreeRowReference *group_rr;
            gint group_id = (button == (gpointer)prv->connection_enable_btn?ALWAYS_ON_GROUP_ID:ALWAYS_OFF_GROUP_ID);

            gtk_tree_selection_unselect_path(selection, path);

            group_rr = ref_ncu_group_node(model, path);

            ncu_pri_group_get_path(model, group_id, 0, &parent);

            static_group_handle_fake_node(model,
              NCU_PRI_GROUP_GET_PATH(model, group_id), FALSE);

            if (capplet_model_1_level_foreach(model, &parent,
                ncu_find_gt_name, (gpointer)object, &sibling)) {
                TREE_STORE_CP_OBJECT_BEFORE(model, &iter, &parent, &sibling, &temp);
            } else {
                TREE_STORE_CP_OBJECT_BEFORE(model, &iter, &parent, NULL, &temp);
            }

            gtk_tree_selection_select_iter(selection, &temp);
            gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);

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
    }

    g_list_foreach (rows, (GFunc)gtk_tree_path_free, NULL);
    g_list_free (rows);

}

static void
foreach_ncu_dup_and_append_to_list_if_in_selected_ncp(NwamuiObject *obj, gpointer self)
{
    NwamProfileDialogPrivate*  prv    = GET_PRIVATE(self);

    g_return_if_fail(NWAMUI_IS_OBJECT(obj));

    if (nwamui_ncp_find_ncu(NWAMUI_NCP(prv->selected_ncp),
        nwamui_util_find_nwamui_object_by_name,
        nwamui_object_get_name(obj))) {
        prv->ncu_list = g_list_prepend(prv->ncu_list, g_object_ref(obj));
    }
}

static void
connections_edit_btn_clicked(GtkButton *button, gpointer user_data )
{
    NwamProfileDialog*         self   = NWAM_PROFILE_DIALOG(user_data);
    NwamProfileDialogPrivate*  prv    = GET_PRIVATE(user_data);
    NwamuiDaemon              *daemon = nwamui_daemon_get_instance();
    NwamuiObject              *auto_ncp;
    GtkTreeModel              *model;
    gint                       result;

    auto_ncp = nwamui_daemon_get_ncp_by_name(daemon, NWAM_NCP_NAME_AUTOMATIC);
    model = GTK_TREE_MODEL(nwamui_ncp_get_ncu_list_store(NWAMUI_NCP(auto_ncp)));
    /* Selected ncus used for show toggle cells. */
    g_assert(prv->ncu_list == NULL);
    g_assert(prv->ncu_rm_list == NULL);
    nwamui_ncp_foreach_ncu(NWAMUI_NCP(auto_ncp),
      (GFunc)foreach_ncu_dup_and_append_to_list_if_in_selected_ncp,
      (gpointer)self);

    gtk_tree_view_set_model(prv->connection_treeview, model);

    g_object_unref(model);
    g_object_unref(auto_ncp);

    result = gtk_dialog_run (GTK_DIALOG (prv->edit_connections_dialog));
    switch (result)
    {
    case GTK_RESPONSE_OK: {
        /* Handle the updated prv->ncu_selection */
        for (GList *elem = g_list_first(prv->ncu_rm_list);
             elem != NULL;
             elem = g_list_delete_link(elem, elem)) {
            NwamuiObject *ncu      = NULL;
            gchar        *dev_name = NULL;

            dev_name = nwamui_ncu_get_device_name(NWAMUI_NCU(elem->data));
            /* Find the real NCU. */
            ncu = nwamui_ncp_get_ncu_by_device_name(NWAMUI_NCP(prv->selected_ncp), dev_name);
            /* Manually remove it. */
            /* Needn't remove, since we have monitor ::add/remove signals. */
            nwamui_object_destroy(ncu);
            /* nwamui_object_remove(prv->selected_ncp, ncu); */
            /* Need unref. */
            g_object_unref(elem->data);
            g_object_unref(ncu);
            g_free(dev_name);
        }
        prv->ncu_rm_list = NULL;

        /* Need to add new ncus to this */
        for (GList *selected = g_list_first(prv->ncu_list);
             selected;
             selected = g_list_delete_link(selected, selected)) {

            if (nwamui_ncp_find_ncu(NWAMUI_NCP(prv->selected_ncp),
                nwamui_util_find_nwamui_object_by_name,
                nwamui_object_get_name(NWAMUI_OBJECT(selected->data))) == NULL) {

                NwamuiObject *new_ncu;
                /* Should fire events to get it added to UI */
                new_ncu = nwamui_object_clone(NWAMUI_OBJECT(selected->data), NULL, prv->selected_ncp);
                if (new_ncu != NULL) {
                    /* Needn't add, since we have monitor ::add/remove signals. */
                    if (nwamui_object_commit(new_ncu)) {
                        /* nwamui_object_add(prv->selected_ncp, new_ncu); */
                    }
                    g_object_unref(new_ncu);
                } else {
                    nwamui_warning("Clone NCP %s NCU %s failed",
                      nwamui_object_get_name(prv->selected_ncp),
                      nwamui_object_get_name(NWAMUI_OBJECT(selected->data)));
                }
            }
            /* Need unref. */
            g_object_unref(selected->data);
        }
        prv->ncu_list = NULL;

        edit_profile_changed(self, prv->selected_ncp);
    }
        break;
    default:
        if (prv->ncu_list != NULL) {
            g_list_foreach(prv->ncu_list, nwamui_util_obj_unref, NULL);
            g_list_free(prv->ncu_list);
            prv->ncu_list = NULL;
        }
        if (prv->ncu_rm_list != NULL) {
            g_list_foreach(prv->ncu_rm_list, nwamui_util_obj_unref, NULL);
            g_list_free(prv->ncu_rm_list);
            prv->ncu_rm_list = NULL;
        }
        break;
    }
    gtk_widget_hide(GTK_WIDGET(prv->edit_connections_dialog));
}

static void
selection_changed(GtkTreeSelection *selection, gpointer user_data)
{
    update_widgets(NWAM_PROFILE_DIALOG(user_data), selection);
}

static gboolean
selection_func(GtkTreeSelection *selection,
  GtkTreeModel *model,
  GtkTreePath *path,
  gboolean path_currently_selected,
  gpointer data)
{
    NwamProfileDialogPrivate*    prv = GET_PRIVATE(data);
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

    g_debug("<on,off,normal> c< %d %d %d > g< %d %d %d >",
      prv->on_child_num,
      prv->off_child_num,
      prv->child_num,
      prv->on_group_num,
      prv->off_group_num,
      prv->group_num);

    return TRUE;
}

static void
update_widgets(NwamProfileDialog *self, GtkTreeSelection *selection)
{
    NwamProfileDialogPrivate* prv           = GET_PRIVATE(self);
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
    ncu_is_active = nwamui_object_get_active( object );
    ncu_is_manual = ( nwamui_object_get_activation_mode( object ) 
      == NWAMUI_COND_ACTIVATION_MODE_MANUAL );
#endif
    gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_enable_btn), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_disable_btn), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_move_up_btn), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_move_down_btn), FALSE);
#ifdef ENABLE_NCU_RENAME
    gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_rename_btn), FALSE);
#else
    gtk_widget_hide(GTK_WIDGET(prv->connection_rename_btn));
#endif
    gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_group_btn), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_activation_combo), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(prv->activation_mode_lbl), FALSE);

    if (!prv->selected_ncp || !nwamui_object_is_modifiable(prv->selected_ncp)) {
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

#if 0
        if (on_child_num == 0 && on_group_num == 0) {
            /* Enable Up */
            gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_move_up_btn), TRUE);
        }

        if (off_child_num == 0 && off_group_num == 0) {
            /* Enable Down */
            gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_move_down_btn), TRUE);
        }
#endif
        if (count_selected_rows == 1) {
            if (on_child_num + child_num > 0) {
#ifdef ENABLE_NCU_RENAME
                /* Enable Rename */
                gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_rename_btn), TRUE);
#endif
                /* Enable Disable */
                gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_disable_btn), TRUE);
            }


            if (off_child_num + child_num > 0) {
#ifdef ENABLE_NCU_RENAME
                /* Enable Rename */
                gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_rename_btn), TRUE);
                /* Enable Enable */
#endif
                gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_enable_btn), TRUE);
            }

            gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_move_up_btn), TRUE);
            gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_move_down_btn), TRUE);
            if (on_group_num == 1 || on_child_num == 1) {
                gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_move_up_btn), FALSE);
            }

            if (off_group_num == 1 || off_child_num == 1) {
                gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_move_down_btn), FALSE);
            }

            if (group_num == 1 || child_num == 1) {
                GList*                             rows;
                GtkTreeIter                        iter;
                GtkTreeIter                        child_iter;
                GtkTreePath                       *path;
/*                 gint                               group_id; */
                nwamui_cond_priority_group_mode_t  group_mode;
                NwamuiObject                      *object;
/*                 gint                               selected_group_id; */
/*                 gint                               last_group_id; */

                rows = gtk_tree_selection_get_selected_rows(selection, NULL);

                path = gtk_tree_path_copy((GtkTreePath *)rows->data);
                /* Found its parent. */
                if (gtk_tree_path_get_depth(path) == 2) {
                    gtk_tree_path_up(path);
                }

                gtk_tree_model_get_iter(model, &iter, path);
                gtk_tree_model_get(model, &iter, 0, &object, -1);

/*                 gtk_tree_model_get_iter(model, &iter, (GtkTreePath *)rows->data); */

/*                 selected_group_id = *gtk_tree_path_get_indices((GtkTreePath *)rows->data); */
/*                 last_group_id = *gtk_tree_path_get_indices(NCU_PRI_GROUP_GET_PATH(model, ALWAYS_OFF_GROUP_ID)); */

/*                 if (selected_group_id == 1) { */
/*                     gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_move_up_btn), FALSE); */
/*                 } else if (selected_group_id == last_group_id - 1) { */
/*                     gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_move_down_btn), FALSE); */
/*                 } */

/*                 if (gtk_tree_model_iter_children(model, &child_iter, &iter)) { */
/*                     gtk_tree_model_get(model, &child_iter, 0, &object, -1); */
/*                 } else { */
/*                     gtk_tree_model_get(model, &iter, 0, &object, -1); */
/*                 } */


                gtk_widget_set_sensitive(GTK_WIDGET(prv->connection_activation_combo), TRUE);
                gtk_widget_set_sensitive(GTK_WIDGET(prv->activation_mode_lbl), TRUE);

                g_signal_handlers_block_by_func(G_OBJECT(prv->connection_activation_combo),
                  (gpointer)connection_activation_combo_changed_cb, (gpointer)self);

                prv->connection_activation_combo_show_ncu_part = FALSE;

                gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(
                      gtk_combo_box_get_model(GTK_COMBO_BOX(self->prv->connection_activation_combo))));
/*                 group_id = (gint)g_object_get_data(G_OBJECT(object), NWAM_COND_NCU_GROUP_ID); */
                group_mode = (gint)g_object_get_data(G_OBJECT(object), NWAM_COND_NCU_GROUP_MODE);

/*                 gtk_combo_box_set_active(self->prv->connection_activation_combo, group_id); */
                gtk_combo_box_set_active(self->prv->connection_activation_combo, group_mode);

                g_signal_handlers_unblock_by_func(G_OBJECT(prv->connection_activation_combo),
                  (gpointer)connection_activation_combo_changed_cb, (gpointer)self);

                g_object_unref(object);
                gtk_tree_path_free(path);

                g_list_foreach (rows, (GFunc)gtk_tree_path_free, NULL);
                g_list_free (rows);
            }
        }
    }
}

static void 
conditions_connected_toggled_cb(GtkToggleButton *togglebutton, gpointer user_data)
{
    NwamProfileDialog*           self = NWAM_PROFILE_DIALOG(user_data);
    NwamProfileDialogPrivate*    prv = GET_PRIVATE(self);

    if ( prv->selected_ncu != NULL && prv->update_inprogress != TRUE )
    {
        //nwamui_ncu_set_selection_rules_enabled(prv->selected_ncu, gtk_toggle_button_get_active(togglebutton));
        
        update_rules_from_ncu(self, NULL, NULL);
    }
}

static void 
conditions_set_status_changed(GtkComboBox* combo, gpointer user_data )
{
    NwamProfileDialog*           self = NWAM_PROFILE_DIALOG(user_data);
    NwamProfileDialogPrivate*    prv = GET_PRIVATE(self);

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
    NwamProfileDialog*           self = NWAM_PROFILE_DIALOG(user_data);
    NwamProfileDialogPrivate*    prv = GET_PRIVATE(self);

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
    g_debug("NwamProfileDialog: notify %s changed", arg1->name);
}

static void
ncu_notify_cb (NwamuiNcu *ncu, GParamSpec *arg1, gpointer user_data)
{
    NwamProfileDialog*        self          = NWAM_PROFILE_DIALOG(user_data);
    NwamProfileDialogPrivate* prv           = GET_PRIVATE(self);
    const gchar*             display_name  = NULL;
    gchar*                   status_string = NULL;

    if (ncu != prv->selected_ncu) {
        return;
    }

    if (ncu != NULL ) {
        display_name = nwamui_ncu_get_display_name(NWAMUI_NCU(ncu));
    }

    status_string = g_strdup_printf(_("Then set status of '%s' to: "), display_name);

    if ( status_string != NULL ) {
        g_free(status_string);
    }
}

static void
on_activate_static_menuitems (GtkAction *action, gpointer data)
{
    NwamProfileDialog*           self = NWAM_PROFILE_DIALOG(data);
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
    NwamProfileDialogPrivate*    prv = GET_PRIVATE(data);

    ncu_pri_treeview_add(NWAMUI_OBJECT(ncu), prv->net_conf_treeview);
    gtk_tree_view_expand_all(prv->net_conf_treeview);
}

static void
ncp_remove_ncu(NwamuiNcp *ncp, NwamuiNcu* ncu, gpointer data)
{
    NwamProfileDialogPrivate*    prv = GET_PRIVATE(data);

    ncu_pri_treeview_remove(prv->net_conf_treeview, NWAMUI_OBJECT(ncu));
    gtk_tree_view_expand_all(prv->net_conf_treeview);
}

static void
edit_profile_changed(NwamProfileDialog *self, NwamuiObject *ncp)
{
    NwamProfileDialogPrivate *prv     = GET_PRIVATE(self);
    GtkTreeModel             *model;

    g_assert(NWAMUI_IS_NCP(ncp));

    /* Update priority tree view */
    model = gtk_tree_view_get_model(prv->net_conf_treeview);

    gtk_widget_hide(GTK_WIDGET(prv->net_conf_treeview));

    gtk_tree_store_clear(GTK_TREE_STORE(model));

    gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(model),
      0,
      nwam_ncu_compare_cb,
      NULL,
      NULL);

    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model),
      0,
      GTK_SORT_ASCENDING);

    nwamui_ncp_foreach_ncu(NWAMUI_NCP(prv->selected_ncp), (GFunc)ncu_pri_treeview_add, (gpointer)prv->net_conf_treeview);

    /* Check if all or none group have nodes. */
    ncu_pri_treeview_add(NULL, prv->net_conf_treeview);
    /* Update hash, collapse group id. */
    ncu_pri_group_update(model);

    gtk_widget_show(GTK_WIDGET(prv->net_conf_treeview));

/*             prv->on_child_num = 0; */
/*             prv->off_child_num = 0; */
/*             prv->child_num = 0; */
/*             prv->on_group_num = 0; */
/*             prv->off_group_num = 0; */
/*             prv->group_num = 0; */

    /* Update widgets */
    selection_changed(gtk_tree_view_get_selection(prv->net_conf_treeview), (gpointer)self);

    gtk_widget_set_sensitive(prv->profile_name_entry, nwamui_object_can_rename(prv->selected_ncp));

    if (nwamui_object_is_modifiable(prv->selected_ncp)) {
        /* gtk_widget_set_sensitive(prv->profile_name_entry, TRUE); */
        gtk_widget_set_sensitive(GTK_WIDGET(prv->net_conf_treeview), TRUE);
        gtk_widget_set_sensitive(GTK_WIDGET(prv->connections_edit_btn), TRUE);
    } else {
        /* gtk_widget_set_sensitive(prv->profile_name_entry, FALSE); */
        gtk_widget_set_sensitive(GTK_WIDGET(prv->net_conf_treeview), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(prv->connections_edit_btn), FALSE);
    }
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
        nwamui_object_set_activation_mode(NWAMUI_OBJECT(ncu), ncu_act_mode);
    }
}

static gint
compare_int( gpointer a, gpointer b )
{
    return( GPOINTER_TO_INT(a) - GPOINTER_TO_INT(b) );
}

static gboolean
ncu_pri_group_find_iter_gt(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer user_data)
{
    NwamuiObject *object;
    gint group_id;

    gtk_tree_model_get(model, iter, 0, &object, -1);

    group_id = (gint)g_object_get_data(G_OBJECT(object), NWAM_COND_NCU_GROUP_ID);

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
ncu_pri_group_get_path(GtkTreeModel *model, gint group_id, gint group_mode, GtkTreeIter *parent)
{
    GHashTable *hash = ncu_pri_group_get_hash();
    GtkTreeRowReference* rowref;
    GtkTreePath *parent_path;
    GtkTreePath *path = NULL;

    if ((rowref = (GtkTreeRowReference*)g_hash_table_lookup(hash, (gpointer)group_id)) == NULL ||
      (path = gtk_tree_row_reference_get_path(rowref)) == NULL) {
        NwamuiObject *fakeobj;
        GtkTreeIter   sibling;
        GtkTreeIter *sibling_p;
        GtkTreeIter parent_iter;

        fakeobj = create_group_object(group_id, group_mode, NWAMUI_COND_ACTIVATION_MODE_LAST);
        /* The ncu isn't in a group and there is no index for the group.
         * Find a group which has a low pri, then insert before it.
         */
        if (!parent) {parent = &parent_iter;}

        if (capplet_model_1_level_foreach(model, NULL,
            ncu_pri_group_find_iter_gt, (gpointer)group_id, &sibling)) {
            sibling_p = &sibling;
        } else {
            sibling_p = NULL;
        }
        gtk_tree_store_insert_before(GTK_TREE_STORE(model), parent, NULL, sibling_p);
        gtk_tree_store_set(GTK_TREE_STORE(model), parent, 0, fakeobj, -1);

        g_object_unref(fakeobj);
        /* Point to the new group */
        parent_path = gtk_tree_model_get_path(model, parent);
        rowref = gtk_tree_row_reference_new(model, parent_path);
        /* Reference could be invalid? */
        g_hash_table_replace(hash, (gpointer)group_id, rowref);
        gtk_tree_path_free(parent_path);

        path = gtk_tree_row_reference_get_path(rowref);
    } else if (parent) {
        gtk_tree_model_get_iter(model, parent, path);
    }
    return path;
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
    GtkTreePath *path = NULL;
    gboolean ret;

    if ((rowref = (GtkTreeRowReference*)g_hash_table_lookup(hash, (gpointer)group_id)) == NULL ||
      (path = gtk_tree_row_reference_get_path(rowref)) == NULL) {
        path = ncu_pri_group_get_path(model, group_id, group_mode, parent);
        ret = FALSE;
    } else {
        gtk_tree_model_get_iter(model, parent, path);
        ret = TRUE;
    }
    return ret;
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
    keys = g_list_sort(keys, (GCompareFunc)compare_int);

    while (keys) {
        gint group_id = GPOINTER_TO_INT(keys->data);
        keys = g_list_delete_link(keys, keys);

        if (group_id != ALWAYS_ON_GROUP_ID && group_id != ALWAYS_OFF_GROUP_ID) {
            rowref = (GtkTreeRowReference*)g_hash_table_lookup(hash, (gpointer)group_id);
            if (rowref) {
                path = gtk_tree_row_reference_get_path(rowref);
                if (path) {
                    i++;
                    gtk_tree_model_get_iter(model, &iter, path);
                    gtk_tree_model_get(model, &iter, 0, &obj, -1);

                    g_hash_table_steal(hash, (gpointer)group_id);
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
ncu_pri_treeview_add(NwamuiObject *object, GtkTreeView *treeview)
{
    GtkTreeModel *model = gtk_tree_view_get_model(treeview);
    GtkTreePath  *path;
    GtkTreeIter   parent;
    GtkTreeIter   iter;

    if (object && NWAMUI_IS_NCU(object)) {
        gboolean ret;
        ret = ncu_pri_group_get_iter(model,
          nwamui_ncu_get_priority_group_for_view(NWAMUI_NCU(object)),
          nwamui_ncu_get_priority_group_mode(NWAMUI_NCU(object)),
          &parent);
        if (!ret) {
            g_assert(!gtk_tree_model_iter_has_child(model, &parent));
        }
        CAPPLET_TREE_STORE_ADD(model, &parent, object, &iter);
        /* If add an always on/off NCU, make sure the related fake node is
         * removed.
         */
        if (nwamui_ncu_get_priority_group_for_view(NWAMUI_NCU(object)) == ALWAYS_ON_GROUP_ID) {
            path = NCU_PRI_GROUP_GET_PATH(model, ALWAYS_ON_GROUP_ID);
            static_group_handle_fake_node(model, path, FALSE);
        } else if (nwamui_ncu_get_priority_group_for_view(NWAMUI_NCU(object)) == ALWAYS_OFF_GROUP_ID) {
            path = NCU_PRI_GROUP_GET_PATH(model, ALWAYS_OFF_GROUP_ID);
            static_group_handle_fake_node(model, path, FALSE);
        }
        path = gtk_tree_model_get_path(model, &parent);
        gtk_tree_view_expand_row(treeview, path, TRUE);
        gtk_tree_path_free(path);

    } else {
        path = NCU_PRI_GROUP_GET_PATH(model, ALWAYS_ON_GROUP_ID);
        static_group_handle_fake_node(model, path, TRUE);
        gtk_tree_view_expand_row(treeview, path, TRUE);
        path = NCU_PRI_GROUP_GET_PATH(model, ALWAYS_OFF_GROUP_ID);
        static_group_handle_fake_node(model, path, TRUE);
        gtk_tree_view_expand_row(treeview, path, TRUE);
    }
}

static void
ncu_pri_treeview_remove(GtkTreeView *treeview, NwamuiObject *object)
{
    GtkTreeModel        *model = gtk_tree_view_get_model(treeview);
    GtkTreeIter          parent;
    GtkTreeIter          temp;
    GtkTreePath         *path;
    NwamuiObject        *obj;
    GtkTreeRowReference *group_rr;

    g_return_if_fail(object && NWAMUI_IS_NCU(object));

    path = NCU_PRI_GROUP_GET_PATH(model,
      nwamui_ncu_get_priority_group_for_view(NWAMUI_NCU(object)));

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
    NwamProfileDialogPrivate*    prv = GET_PRIVATE(user_data);
    GtkTreeModel *model = gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER(gtk_combo_box_get_model(combo)));
	GtkTreeIter iter;
	GtkTreeIter child_iter;

	if (gtk_combo_box_get_active_iter(combo, &iter)) {
        gint row_data;
        gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(gtk_combo_box_get_model(combo)), &child_iter, &iter);
		gtk_tree_model_get(model, &child_iter, 0, &row_data, -1);
		switch (row_data) {
        case 3:
        case 4:
        case 5: {
            GList *rows;
            GtkTreePath *path;
            rows = gtk_tree_selection_get_selected_rows(gtk_tree_view_get_selection(prv->net_conf_treeview), NULL);
            g_assert(rows->next == NULL);
            set_group_object_group_mode(gtk_tree_view_get_model(prv->net_conf_treeview),
              rows->data,
              combo_contents_mode_map[row_data]);
            g_list_foreach (rows, (GFunc)gtk_tree_path_free, NULL);
            g_list_free (rows);
        }
            break;
        default:
            break;
        }
	}
}

static gboolean
connection_activation_combo_filter(GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
    NwamProfileDialogPrivate*    prv = GET_PRIVATE(user_data);
    gint row_data;

    gtk_tree_model_get(model, iter, 0, &row_data, -1);

    return (prv->connection_activation_combo_show_ncu_part && row_data < 3) ||
      (!prv->connection_activation_combo_show_ncu_part && row_data >= 3);
}

static void
response_cb(GtkWidget* widget, gint responseid, gpointer data)
{
    NwamProfileDialogPrivate* prv           = GET_PRIVATE(data);
    NwamProfileDialog*        self          = NWAM_PROFILE_DIALOG(data);
    gboolean                  stop_emission = FALSE;
	
	switch (responseid) {
		case GTK_RESPONSE_NONE:
            stop_emission = TRUE;
			break;
		case GTK_RESPONSE_OK:
            if (nwam_pref_apply(NWAM_PREF_IFACE(self), NULL)) {
                gtk_widget_hide(widget);
            } else {
                /* TODO - report error to user */
                stop_emission = TRUE;
            }
			break;
		case GTK_RESPONSE_REJECT: /* Generated by Referesh Button */
            nwam_pref_refresh(NWAM_PREF_IFACE(self), NULL, TRUE);
            stop_emission = TRUE;
			break;
		case GTK_RESPONSE_DELETE_EVENT:
            /* Fall through */
		case GTK_RESPONSE_CANCEL:
            if (nwam_pref_cancel (NWAM_PREF_IFACE(self), NULL)) {
                gtk_widget_hide(widget);
            } else {
                stop_emission = TRUE;
            }
			break;
		case GTK_RESPONSE_HELP:
            nwam_pref_help(NWAM_PREF_IFACE(self), NULL);
            stop_emission = TRUE;
			break;
	}
    if ( stop_emission ) {
        g_signal_stop_emission_by_name(widget, "response" );
    }
}
