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
#include "nwam_conn_conf_ip_panel.h"
#include "nwam_pref_iface.h"
#include "nwam_wireless_dialog.h"
#include "capplet-utils.h"

/* Names of Widgets in Glade file */
#define IP_PANEL_IFACE_NOTEBOOK	"interface_notebook"
#define IP_PANEL_IPV4_COMBO	"ipv4_combo"
#define IP_PANEL_IPV6_COMBO	"ipv6_combo"
#define IP_PANEL_IPV4_NOTEBOOK	"ipv4_notebook"
#define IP_PANEL_IPV6_NOTEBOOK	"ipv6_notebook"
#define IP_PANEL_IPV6_EXPANDER	"ipv6_expander"
#define IP_PANEL_IPV6_EXP_LBL	"label34"

#define IP_PANEL_WIRELESS_TAB                   "wireless_tab_vbox"
#define IP_PANEL_WIRELESS_TABLE                 "wireless_treeview"
#define IP_PANEL_WIRELESS_TAB_ADD_BUTTON        "wireless_tab_add_button"
#define IP_PANEL_WIRELESS_TAB_REMOVE_BUTTON     "wireless_tab_remove_button"
#define IP_PANEL_WIRELESS_TAB_EDIT_BUTTON       "wireless_tab_edit_button"
#define IP_PANEL_WIRELESS_TAB_UP_BUTTON         "wireless_tab_up_button"
#define IP_PANEL_WIRELESS_TAB_DOWN_BUTTON       "wireless_tab_down_button"
#define IP_PANEL_WIRELESS_TAB_WIRELESS_OPT_EXPANDER    "wireless_options_expander"
#define IP_PANEL_WIRELESS_TAB_JOIN_OPEN_CBOX    "ask_before_join_open_cbox"
#define IP_PANEL_WIRELESS_TAB_JOIN_PREF_CBOX    "ask_before_join_preferred_cbox"
#define IP_PANEL_WIRELESS_TAB_ADD_CBOX          "ask_before_add_cbox"
#define IP_PANEL_WIRELESS_TAB_PREF_NET_COMBO    "no_preferred_networks_combo"

#define IP_DHCP_PANEL_IPV4_ADDRESS_LBL              "dhcp_address_lbl"
#define IP_DHCP_PANEL_IPV4_SUBNET_LBL               "dhcp_subnet_lbl"
#define IP_DHCP_PANEL_IPV4_RENEW_BTN                "renew_lease_btn"
#define IP_MANUAL_PANEL_IPV4_ADDRESS_ENTRY          "manual_address_entry"
#define IP_MANUAL_PANEL_IPV4_SUBNET_ENTRY           "manual_subnet_entry"
#define IP_MULTI_PANEL_IPV4_ADDR_TABLE_TVIEW        "address_table"
#define IP_MULTI_PANEL_IPV4_ADD_BTN                 "multiple_add_btn"
#define IP_MULTI_PANEL_IPV4_DEL_BTN                 "multiple_delete_btn"

#define IP_DHCP_PANEL_IPV6_ADDRESS_LBL              "ipv6_address_lbl"
#define IP_DHCP_PANEL_IPV6_ROUTER_LBL               "ipv6_router_lbl"
#define IP_DHCP_PANEL_IPV6_PREFIX_LBL               "ipv6_prefix_lbl"
#define IP_MANUAL_PANEL_IPV6_ADDRESS_ENTRY          "ipv6_address_entry"
#define IP_MANUAL_PANEL_IPV6_ROUTER_ENTRY           "ipv6_router_entry"
#define IP_MANUAL_PANEL_IPV6_PREFIX_ENTRY           "ipv6_prefix_entry"
#define IP_MULTI_PANEL_IPV6_ADDR_TABLE_TVIEW        "ipv6_address_table"
#define IP_MULTI_PANEL_IPV6_ADD_BTN                 "ipv6_add_btn"
#define IP_MULTI_PANEL_IPV6_DEL_BTN                 "ipv6_delete_btn"
#define IP_MULTI_PANEL_IPV6_ACCEPT_STATEFUL_CBTN    "accept_ipv6_stateful_cbox"
#define IP_MULTI_PANEL_IPV6_ACCEPT_STATELESS_CBTN   "accept_ipv6_stateless_cbox"

#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
	NWAM_TYPE_CONN_CONF_IP_PANEL, NwamConnConfIPPanelPrivate)) 

struct _NwamConnConfIPPanelPrivate {
	/* Widget Pointers */
	GtkNotebook *iface_nb;
	GtkComboBox *ipv4_combo;
	GtkComboBox *ipv6_combo;
	GtkNotebook *ipv4_nb;
	GtkNotebook *ipv6_nb;
	GtkExpander *ipv6_expander;
	GtkLabel    *ipv6_lbl;

	GtkLabel    *ipv4_addr_lbl;
	GtkLabel    *ipv4_subnet_lbl;
	GtkEntry    *ipv4_addr_entry;
	GtkEntry    *ipv4_subnet_entry;
	GtkButton   *ipv4_renew_btn;
	GtkButton   *ipv4_mutli_add_btn;
	GtkButton   *ipv4_mutli_del_btn;
	GtkTreeView *ipv4_tv;
        
    GtkWidget   *wireless_tab;
    GtkTreeView *wifi_fav_tv;
    GtkButton   *wireless_tab_add_button;
    GtkButton   *wireless_tab_remove_button;
    GtkButton   *wireless_tab_edit_button;
    GtkButton   *wireless_tab_up_button;
    GtkButton   *wireless_tab_down_button;
	GtkComboBox *no_preferred_networks_combo;
    GtkCheckButton *join_open_cbox;
    GtkCheckButton *join_preferred_cbox;
    GtkCheckButton *add_cbox;

/*
 *      Currently not relevant.
 *
	GtkLabel *ipv6_addr_lbl;
	GtkLabel *ipv6_prefix_lbl;
	GtkEntry *ipv6_addr_entry;
	GtkEntry *ipv6_prefix_entry;
	GtkCheckButton *ipv6_accept_stateful_cb;
	GtkCheckButton *ipv6_accept_stateless_cb;
*/      
 	GtkButton *ipv6_add_btn;
	GtkButton *ipv6_del_btn;
	GtkTreeView *ipv6_tv;

	/* Other Data */
    NwamuiDaemon*       daemon;
    NwamuiNcu*          ncu;
    gboolean            join_open;
    gboolean            join_preferred;
    gboolean            add_any_wifi;
    gint                action_if_no_fav;
    gulong              ncu_handler_id;
    NwamWirelessDialog* wifi_dialog;
};

enum {
	/* IP_VIEW_HOSTNAME=0, */
	IP_VIEW_ADDR = 0,
	IP_VIEW_MASK,
};

enum {
	WIFI_FAV_ESSID=0,
	WIFI_FAV_SECURITY,
	WIFI_FAV_SPEED,
    WIFI_CHANNEL
};

static void nwam_pref_init (gpointer g_iface, gpointer iface_data);
static gboolean refresh(NwamPrefIFace *iface, gpointer user_data, gboolean force);
static gboolean apply(NwamPrefIFace *iface, gpointer user_data);
static gboolean cancel(NwamPrefIFace *iface, gpointer user_data);
static gboolean help(NwamPrefIFace *iface, gpointer user_data);

static void nwam_conf_ip_panel_finalize(NwamConnConfIPPanel *self);

/* Callbacks */
static void object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);

static void daemon_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);

static void ncu_changed_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);

static void nwam_conn_multi_ip_cell_cb (GtkTreeViewColumn *col,
					     GtkCellRenderer   *renderer,
					     GtkTreeModel      *model,
					     GtkTreeIter       *iter,
					     gpointer           data);
static void nwam_conn_wifi_fav_cell_cb (GtkTreeViewColumn *col,
					     GtkCellRenderer   *renderer,
					     GtkTreeModel      *model,
					     GtkTreeIter       *iter,
					     gpointer           data);
static void nwam_conn_multi_cell_editing_started_cb(GtkCellRenderer *cell,
                                        GtkCellEditable *editable,
                                        const gchar     *path,
                                        gpointer         data);
static void nwam_conn_multi_ipv4_cell_edited_cb (GtkCellRendererText *renderer,
  gchar               *path,
  gchar               *new_text,
  gpointer             user_data);
static void nwam_conn_multi_ipv6_cell_edited_cb (GtkCellRendererText *renderer,
  gchar               *path,
  gchar               *new_text,
  gpointer             user_data);

static gint nwam_conn_multi_ip_comp_cb (GtkTreeModel *model,
			      GtkTreeIter *a,
			      GtkTreeIter *b,
			      gpointer user_data);
static gint nwam_conn_wifi_fav_comp_cb (GtkTreeModel *model,
			      GtkTreeIter *a,
			      GtkTreeIter *b,
			      gpointer user_data);
static void conn_view_row_activated_cb (GtkTreeView *tree_view,
					GtkTreePath *path,
					GtkTreeViewColumn *column,
					gpointer data);
static void multi_line_add_cb( GtkButton *button, gpointer data );
static void multi_line_del_cb( GtkButton *button, gpointer data );
static void show_changed_cb( GtkComboBox *combo, gpointer data );
/* static void renew_cb( GtkButton *button, gpointer data ); */
static void wireless_tab_add_button_clicked_cb( GtkButton *button, gpointer data );
static void wireless_tab_remove_button_clicked_cb( GtkButton *button, gpointer data );
static void wireless_tab_edit_button_clicked_cb( GtkButton *button, gpointer data );
static void wireless_tab_up_button_clicked_cb( GtkButton *button, gpointer data );
static void wireless_tab_down_button_clicked_cb( GtkButton *button, gpointer data );
static void refresh_clicked_cb( GtkButton *button, gpointer data );
static void ipv4_addr_changed_cb( GtkEditable* editable, gpointer user_data );
static void ipv4_subnet_changed_cb( GtkEditable* editable, gpointer user_data );

G_DEFINE_TYPE_EXTENDED (NwamConnConfIPPanel,
                        nwam_conf_ip_panel,
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
    iface->dialog_run = NULL;
}

static void
nwam_conf_ip_panel_class_init(NwamConnConfIPPanelClass *klass)
{
	/* Pointer to GObject Part of Class */
	GObjectClass *gobject_class = (GObjectClass*) klass;
    
    g_type_class_add_private (klass, sizeof(NwamConnConfIPPanelPrivate));

	/* Override Some Function Pointers */
	gobject_class->finalize = (void (*)(GObject*)) nwam_conf_ip_panel_finalize;
}

/*
 * The IPv4 mutli addr and IPv6 mutli addr have the similar view
 */
static void
nwam_compose_wifi_fav_view (NwamConnConfIPPanel *self, GtkTreeView *view)
{
    GtkTreeViewColumn *col;
    GtkCellRenderer *renderer;
    GtkTreeModel *model;

    model = GTK_TREE_MODEL(gtk_list_store_new (1, G_TYPE_OBJECT /* NwamuiWifiNet Object */));
    gtk_tree_view_set_model (view, model);

    g_object_set (G_OBJECT(view),
      "headers-visible", TRUE,
      "headers-clickable", TRUE,
      "reorderable", FALSE,
      "rules-hint", TRUE,
      "enable-search", FALSE,
      "show-expanders", TRUE,
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
	renderer = capplet_column_append_cell(col,
      gtk_cell_renderer_pixbuf_new(), FALSE,
      (GtkTreeCellDataFunc)nwam_conn_wifi_fav_cell_cb, (gpointer)0, NULL);
    g_object_set_data(G_OBJECT(renderer), "nwam_wifi_fav_column_id", GUINT_TO_POINTER(WIFI_FAV_ESSID));
	renderer = capplet_column_append_cell(col,
      gtk_cell_renderer_text_new(), FALSE,
      (GtkTreeCellDataFunc)nwam_conn_wifi_fav_cell_cb, (gpointer)1, NULL);
    g_object_set_data(G_OBJECT(renderer), "nwam_wifi_fav_column_id", GUINT_TO_POINTER(WIFI_FAV_ESSID));
    
    // Column:	WIFI_CHANNEL
    col = capplet_column_new(view,
      "title", _("Channel"),
      "expand", TRUE,
      "resizable", TRUE,
      "clickable", FALSE,
      "sort-indicator", FALSE,
      "reorderable", FALSE,
      NULL);
	renderer = capplet_column_append_cell(col,
      gtk_cell_renderer_text_new(), FALSE,
      (GtkTreeCellDataFunc)nwam_conn_wifi_fav_cell_cb, (gpointer)0, NULL);
    g_object_set_data(G_OBJECT(renderer), "nwam_wifi_fav_column_id", GUINT_TO_POINTER(WIFI_CHANNEL));
    
    // Column:	WIFI_FAV_SPEED
    col = capplet_column_new(view,
      "title", _("Mb/s"),
      "expand", TRUE,
      "resizable", TRUE,
      "clickable", FALSE,
      "sort-indicator", FALSE,
      "reorderable", FALSE,
      NULL);
	renderer = capplet_column_append_cell(col,
      gtk_cell_renderer_text_new(), FALSE,
      (GtkTreeCellDataFunc)nwam_conn_wifi_fav_cell_cb, (gpointer)0, NULL);
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
	renderer = capplet_column_append_cell(col,
      gtk_cell_renderer_text_new(), TRUE,
      (GtkTreeCellDataFunc)nwam_conn_wifi_fav_cell_cb, (gpointer)0, NULL);
    g_object_set_data(G_OBJECT(renderer), "nwam_wifi_fav_column_id", GUINT_TO_POINTER(WIFI_FAV_SECURITY));
	renderer = capplet_column_append_cell(col,
      gtk_cell_renderer_pixbuf_new(), FALSE,
      (GtkTreeCellDataFunc)nwam_conn_wifi_fav_cell_cb, (gpointer)1, NULL);
    /* gtk_tree_view_column_pack_end( col, renderer, FALSE ); */
    g_object_set_data(G_OBJECT(renderer), "nwam_wifi_fav_column_id", GUINT_TO_POINTER(WIFI_FAV_SECURITY));
    
}
/*
 * The IPv4 mutli addr and IPv6 mutli addr have the similar view
 */
static void
nwam_compose_multi_ip_tree_view (NwamConnConfIPPanel *self, GtkTreeView *view)
{
	GtkTreeViewColumn *col;
	GtkCellRenderer *renderer;

	g_object_set (G_OBJECT(view),
		      "headers-clickable", FALSE,
		      NULL);
	
	// column IP_VIEW_HOSTNAME
/*
	col = gtk_tree_view_column_new_with_attributes();
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_set_title(col, _("Hostname"));
	g_object_set (G_OBJECT(col),
                      "expand", TRUE,
		      "resizable", TRUE,
		      "clickable", TRUE,
		      "sort-indicator", TRUE,
		      "reorderable", TRUE,
		      NULL);
	gtk_tree_view_append_column (view, col);
	gtk_tree_view_column_set_sort_column_id (col, IP_VIEW_HOSTNAME);
	gtk_tree_view_column_pack_start(col, renderer, TRUE);
	gtk_tree_view_column_set_cell_data_func (col,
						 renderer,
						 nwam_conn_multi_ip_cell_cb,
						 (gpointer) view,
						 NULL);
	g_object_set(G_OBJECT(renderer), "editable", TRUE, NULL);
	g_signal_connect(G_OBJECT(renderer), "edited",
		(GCallback) nwam_conn_multi_ip_cell_edited_cb, (gpointer) view);
	g_object_set_data(G_OBJECT(renderer), "nwam_multi_ip_column_id", GUINT_TO_POINTER(IP_VIEW_HOSTNAME));
*/
	// column IP_VIEW_ADDR
    col = capplet_column_new(view,
      "title", _("Address"),
      "resizable", TRUE,
      "clickable", TRUE,
      "sort-indicator", TRUE,
      "reorderable", TRUE,
      NULL);
	renderer = capplet_column_append_cell(col,
      gtk_cell_renderer_text_new(), FALSE,
      (GtkTreeCellDataFunc)nwam_conn_multi_ip_cell_cb, (gpointer)view, NULL);

	gtk_tree_view_column_set_sort_column_id (col, IP_VIEW_ADDR);	
	g_object_set(G_OBJECT(renderer), "editable", TRUE, NULL);
	if (view == self->prv->ipv4_tv) {
        g_signal_connect(G_OBJECT(renderer), "edited",
          (GCallback) nwam_conn_multi_ipv4_cell_edited_cb, (gpointer)self);
        g_signal_connect(G_OBJECT(renderer), "editing-started",
          (GCallback) nwam_conn_multi_cell_editing_started_cb, (gpointer)FALSE);
    } else {
        g_signal_connect(G_OBJECT(renderer), "edited",
          (GCallback) nwam_conn_multi_ipv6_cell_edited_cb, (gpointer)self);
        g_signal_connect(G_OBJECT(renderer), "editing-started",
          (GCallback) nwam_conn_multi_cell_editing_started_cb, (gpointer)TRUE);
    }
	g_object_set_data(G_OBJECT(renderer), "nwam_multi_ip_column_id", GUINT_TO_POINTER(IP_VIEW_ADDR));

	// column IP_VIEW_MASK
    col = capplet_column_new(view,
      "resizable", TRUE,
      "clickable", TRUE,
      "sort-indicator", TRUE,
      "reorderable", TRUE,
      NULL);

	gtk_tree_view_column_set_sort_column_id (col, IP_VIEW_MASK);	

	renderer = capplet_column_append_cell(col,
      gtk_cell_renderer_text_new(), FALSE,
      (GtkTreeCellDataFunc)nwam_conn_multi_ip_cell_cb, (gpointer)view, NULL);

	if (view == self->prv->ipv4_tv) {
		gtk_tree_view_column_set_title(col, _("Subnet Mask"));
        g_signal_connect(G_OBJECT(renderer), "edited",
          (GCallback) nwam_conn_multi_ipv4_cell_edited_cb, (gpointer)self);
	} else {
		gtk_tree_view_column_set_title(col, _("Prefix Length"));
        g_signal_connect(G_OBJECT(renderer), "edited",
          (GCallback) nwam_conn_multi_ipv6_cell_edited_cb, (gpointer)self);
	}

	g_object_set(G_OBJECT(renderer), "editable", TRUE, NULL);
	g_object_set_data(G_OBJECT(renderer), "nwam_multi_ip_column_id", GUINT_TO_POINTER(IP_VIEW_MASK));
}

static void
nwam_conf_ip_panel_init(NwamConnConfIPPanel *self)
{
	GtkTreeModel *model;
	
	self->prv = GET_PRIVATE(self);
	
    self->prv->ncu = NULL;
    self->prv->ncu_handler_id = 0;
    
    self->prv->daemon = nwamui_daemon_get_instance();
    
    self->prv->wifi_dialog = nwam_wireless_dialog_new();
    
	/* Iniialise pointers to important widgets */
    self->prv->iface_nb = GTK_NOTEBOOK(nwamui_util_glade_get_widget(IP_PANEL_IFACE_NOTEBOOK));
    self->prv->wireless_tab = GTK_WIDGET(nwamui_util_glade_get_widget(IP_PANEL_WIRELESS_TAB));
    self->prv->wifi_fav_tv  = GTK_TREE_VIEW(nwamui_util_glade_get_widget(IP_PANEL_WIRELESS_TABLE));
    
    nwam_compose_wifi_fav_view( self, self->prv->wifi_fav_tv );

    self->prv->wireless_tab_add_button = GTK_BUTTON(nwamui_util_glade_get_widget(IP_PANEL_WIRELESS_TAB_ADD_BUTTON));
    self->prv->wireless_tab_remove_button = GTK_BUTTON(nwamui_util_glade_get_widget(IP_PANEL_WIRELESS_TAB_REMOVE_BUTTON));
    self->prv->wireless_tab_edit_button = GTK_BUTTON(nwamui_util_glade_get_widget(IP_PANEL_WIRELESS_TAB_EDIT_BUTTON));
    self->prv->wireless_tab_up_button = GTK_BUTTON(nwamui_util_glade_get_widget(IP_PANEL_WIRELESS_TAB_UP_BUTTON));
    self->prv->wireless_tab_down_button = GTK_BUTTON(nwamui_util_glade_get_widget(IP_PANEL_WIRELESS_TAB_DOWN_BUTTON));

    g_signal_connect(G_OBJECT(self->prv->wireless_tab_add_button ), "clicked", (GCallback)wireless_tab_add_button_clicked_cb, (gpointer)self);
    g_signal_connect(G_OBJECT(self->prv->wireless_tab_remove_button), "clicked", (GCallback)wireless_tab_remove_button_clicked_cb, (gpointer)self);
    g_signal_connect(G_OBJECT(self->prv->wireless_tab_edit_button), "clicked", (GCallback)wireless_tab_edit_button_clicked_cb, (gpointer)self);
    g_signal_connect(G_OBJECT(self->prv->wireless_tab_up_button), "clicked", (GCallback)wireless_tab_up_button_clicked_cb, (gpointer)self);
    g_signal_connect(G_OBJECT(self->prv->wireless_tab_down_button), "clicked", (GCallback)wireless_tab_down_button_clicked_cb, (gpointer)self);

	self->prv->no_preferred_networks_combo = GTK_COMBO_BOX(nwamui_util_glade_get_widget(IP_PANEL_WIRELESS_TAB_PREF_NET_COMBO));
    self->prv->join_open_cbox = GTK_CHECK_BUTTON(nwamui_util_glade_get_widget(IP_PANEL_WIRELESS_TAB_JOIN_OPEN_CBOX));
    self->prv->join_preferred_cbox = GTK_CHECK_BUTTON(nwamui_util_glade_get_widget(IP_PANEL_WIRELESS_TAB_JOIN_PREF_CBOX));
    self->prv->add_cbox = GTK_CHECK_BUTTON(nwamui_util_glade_get_widget(IP_PANEL_WIRELESS_TAB_ADD_CBOX));

	self->prv->ipv4_combo = GTK_COMBO_BOX(nwamui_util_glade_get_widget(IP_PANEL_IPV4_COMBO));
	g_signal_connect(G_OBJECT(self->prv->ipv4_combo), "changed", (GCallback)show_changed_cb, (gpointer)self);
	self->prv->ipv6_combo = GTK_COMBO_BOX(nwamui_util_glade_get_widget(IP_PANEL_IPV6_COMBO));
	g_signal_connect(G_OBJECT(self->prv->ipv6_combo), "changed", (GCallback)show_changed_cb, (gpointer)self);

	self->prv->ipv4_nb = GTK_NOTEBOOK(nwamui_util_glade_get_widget(IP_PANEL_IPV4_NOTEBOOK));
	self->prv->ipv6_nb = GTK_NOTEBOOK(nwamui_util_glade_get_widget(IP_PANEL_IPV6_NOTEBOOK));
	self->prv->ipv6_lbl = GTK_LABEL(nwamui_util_glade_get_widget(IP_PANEL_IPV6_EXP_LBL));
	self->prv->ipv6_expander = GTK_EXPANDER(nwamui_util_glade_get_widget(IP_PANEL_IPV6_EXPANDER));

	self->prv->ipv4_addr_lbl = GTK_LABEL(nwamui_util_glade_get_widget(IP_DHCP_PANEL_IPV4_ADDRESS_LBL));
	self->prv->ipv4_subnet_lbl = GTK_LABEL(nwamui_util_glade_get_widget(IP_DHCP_PANEL_IPV4_SUBNET_LBL));

/*	self->prv->ipv4_renew_btn = GTK_BUTTON(nwamui_util_glade_get_widget(IP_DHCP_PANEL_IPV4_RENEW_BTN));
	g_signal_connect(G_OBJECT(self->prv->ipv4_renew_btn), "clicked", (GCallback)renew_cb, (gpointer)self);
*/	
	self->prv->ipv4_addr_entry = GTK_ENTRY(nwamui_util_glade_get_widget(IP_MANUAL_PANEL_IPV4_ADDRESS_ENTRY));
    g_signal_connect(G_OBJECT(self->prv->ipv4_addr_entry ), "changed",
            (GCallback)ipv4_addr_changed_cb, (gpointer)self);
    nwamui_util_set_entry_ip_address_only( self->prv->ipv4_addr_entry, FALSE, TRUE );

	self->prv->ipv4_subnet_entry = GTK_ENTRY(nwamui_util_glade_get_widget(IP_MANUAL_PANEL_IPV4_SUBNET_ENTRY));
    g_signal_connect(G_OBJECT(self->prv->ipv4_subnet_entry ), "changed",
            (GCallback)ipv4_subnet_changed_cb, (gpointer)self);
    nwamui_util_set_entry_ip_address_only( self->prv->ipv4_subnet_entry, FALSE, TRUE );

	self->prv->ipv4_tv = GTK_TREE_VIEW(nwamui_util_glade_get_widget(IP_MULTI_PANEL_IPV4_ADDR_TABLE_TVIEW));

	self->prv->ipv4_mutli_add_btn = GTK_BUTTON(nwamui_util_glade_get_widget(IP_MULTI_PANEL_IPV4_ADD_BTN));
	self->prv->ipv4_mutli_del_btn = GTK_BUTTON(nwamui_util_glade_get_widget(IP_MULTI_PANEL_IPV4_DEL_BTN));
	g_signal_connect(G_OBJECT(self->prv->ipv4_mutli_add_btn), "clicked", (GCallback)multi_line_add_cb, (gpointer)self);
	g_signal_connect(G_OBJECT(self->prv->ipv4_mutli_del_btn), "clicked", (GCallback)multi_line_del_cb, (gpointer)self);

/*
 *      Currently not relevant
 *
	self->prv->ipv6_addr_lbl = GTK_LABEL(nwamui_util_glade_get_widget(IP_DHCP_PANEL_IPV6_ADDRESS_LBL));
	self->prv->ipv6_prefix_lbl = GTK_LABEL(nwamui_util_glade_get_widget(IP_DHCP_PANEL_IPV6_PREFIX_LBL));

	self->prv->ipv6_addr_entry = GTK_ENTRY(nwamui_util_glade_get_widget(IP_MANUAL_PANEL_IPV6_ADDRESS_ENTRY));
	self->prv->ipv6_prefix_entry = GTK_ENTRY(nwamui_util_glade_get_widget(IP_MANUAL_PANEL_IPV6_PREFIX_ENTRY));

        self->prv->ipv6_mutli_del_btn = GTK_BUTTON(nwamui_util_glade_get_widget(IP_MULTI_PANEL_IPV6_DEL_BTN));
	g_signal_connect(G_OBJECT(self->prv->ipv6_mutli_del_btn), "clicked", (GCallback)multi_line_del_cb, (gpointer)self);
	
	self->prv->ipv6_accept_stateful_cb = GTK_CHECK_BUTTON(nwamui_util_glade_get_widget(IP_MULTI_PANEL_IPV6_ACCEPT_STATEFUL_CBTN));
	self->prv->ipv6_accept_stateless_cb = GTK_CHECK_BUTTON(nwamui_util_glade_get_widget(IP_MULTI_PANEL_IPV6_ACCEPT_STATELESS_CBTN));
*/

	self->prv->ipv6_tv = GTK_TREE_VIEW(nwamui_util_glade_get_widget(IP_MULTI_PANEL_IPV6_ADDR_TABLE_TVIEW));

	self->prv->ipv6_add_btn = GTK_BUTTON(nwamui_util_glade_get_widget(IP_MULTI_PANEL_IPV6_ADD_BTN));
	self->prv->ipv6_del_btn = GTK_BUTTON(nwamui_util_glade_get_widget(IP_MULTI_PANEL_IPV6_DEL_BTN));
	g_signal_connect(G_OBJECT(self->prv->ipv6_add_btn), "clicked", (GCallback)multi_line_add_cb, (gpointer)self);
	g_signal_connect(G_OBJECT(self->prv->ipv6_del_btn), "clicked", (GCallback)multi_line_del_cb, (gpointer)self);

    nwam_compose_multi_ip_tree_view (self, self->prv->ipv4_tv);

    nwam_compose_multi_ip_tree_view (self, self->prv->ipv6_tv);

	g_signal_connect(G_OBJECT(self), "notify", (GCallback)object_notify_cb, NULL);

	g_signal_connect(G_OBJECT(self->prv->daemon), "notify", (GCallback)daemon_notify_cb, self );
}

static void
populate_wifi_fav( NwamConnConfIPPanel* self, gboolean set_initial_state )
{
    NwamConnConfIPPanelPrivate* prv;
    GtkTreeModel*   model;
    GList*          fav_list;
    GtkTreeIter     iter;
    
    prv = self->prv;

    gtk_widget_show_all( GTK_WIDGET(self->prv->wireless_tab) );
    
    /* Populate WiFi Favourites */
    model = GTK_TREE_MODEL( gtk_tree_view_get_model(GTK_TREE_VIEW(prv->wifi_fav_tv)));
    gtk_list_store_clear(GTK_LIST_STORE(model));
    
    fav_list = nwamui_daemon_get_fav_wifi_networks( NWAMUI_DAEMON(prv->daemon) );
    for( GList* elem = g_list_first(fav_list); elem != NULL; elem = g_list_next( elem ) ) {
        NwamuiWifiNet*  wifi_net = NWAMUI_WIFI_NET(elem->data);
        
        gtk_list_store_append(GTK_LIST_STORE( model ), &iter );
        
        gtk_list_store_set( GTK_LIST_STORE( model ), &iter, 0, wifi_net, -1 );
    }
    if (fav_list) {
        nwamui_util_free_obj_list( fav_list );
    }

    /* Populate WiFi conditions */
    {
        NwamuiProf*     prof;

        prof = nwamui_prof_get_instance ();
        g_object_get (prof,
          "action_on_no_fav_networks", &prv->action_if_no_fav,
          "join_wifi_not_in_fav", &prv->join_open,
          "join_any_fav_wifi", &prv->join_preferred,
          "add_any_new_wifi_to_fav", &prv->add_any_wifi,
          NULL);
        
        gtk_combo_box_set_active (prv->no_preferred_networks_combo,
          prv->action_if_no_fav);

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prv->join_open_cbox), prv->join_open);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prv->join_preferred_cbox), prv->join_preferred);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prv->add_cbox), prv->add_any_wifi);

        g_object_unref (prof);
    }
}

static void
populate_panel( NwamConnConfIPPanel* self, gboolean set_initial_state )
{        
    NwamConnConfIPPanelPrivate* prv;
    nwamui_ncu_type_t   ncu_type;
    gboolean            modifiable;
    gboolean            ipv4_dhcp;
    gchar*              ipv4_address;
    gchar*              ipv4_subnet;
    gboolean            ipv6_active;
    gboolean            ipv6_dhcp;
    gchar*              ipv6_address;
    gchar*              ipv6_prefix;

    g_assert( NWAM_IS_CONN_CONF_IP_PANEL(self));
    
    g_return_if_fail( NWAMUI_IS_NCU(self->prv->ncu) );

    prv = self->prv;
    
    ncu_type = nwamui_ncu_get_ncu_type( NWAMUI_NCU(prv->ncu) );
    modifiable = nwamui_ncu_is_modifiable( NWAMUI_NCU(prv->ncu) );
    ipv4_dhcp = nwamui_ncu_get_ipv4_dhcp( NWAMUI_NCU(prv->ncu) );
    ipv4_address = nwamui_ncu_get_ipv4_address( NWAMUI_NCU(prv->ncu) );
    ipv4_subnet = nwamui_ncu_get_ipv4_subnet( NWAMUI_NCU(prv->ncu) );
    ipv6_active = nwamui_ncu_get_ipv6_active( NWAMUI_NCU(prv->ncu) );
    ipv6_dhcp = nwamui_ncu_get_ipv6_dhcp( NWAMUI_NCU(prv->ncu) );
    ipv6_address = nwamui_ncu_get_ipv6_address( NWAMUI_NCU(prv->ncu) );
    ipv6_prefix = nwamui_ncu_get_ipv6_prefix( NWAMUI_NCU(prv->ncu) );
    
    /* Update TreeView Models to be associated with NCU */
	gtk_tree_view_set_model (prv->ipv4_tv, GTK_TREE_MODEL(nwamui_ncu_get_v4addresses(NWAMUI_NCU(prv->ncu))));
	gtk_tree_view_set_model (prv->ipv6_tv, GTK_TREE_MODEL(nwamui_ncu_get_v6addresses(NWAMUI_NCU(prv->ncu))));
	
    if ( ncu_type == NWAMUI_NCU_TYPE_WIRELESS) {
        populate_wifi_fav( self, set_initial_state );
    }
    else {
        gtk_widget_hide_all( GTK_WIDGET(self->prv->wireless_tab) );
    }

    if ( set_initial_state ) {
        if ( ipv4_dhcp ) { 
            gtk_combo_box_set_active(GTK_COMBO_BOX(prv->ipv4_combo), 1); /* Select DHCP on Combo Box */
        }
        else {
            gtk_combo_box_set_active(GTK_COMBO_BOX(prv->ipv4_combo), 2); /* Select Static IP on Combo Box */
            /* TODO - Check for Multiple IP Addresses  and update combo to reflect */
        }
        gtk_widget_set_sensitive(GTK_WIDGET(prv->ipv4_combo), modifiable);
    }
    
    g_signal_handlers_block_by_func(G_OBJECT(prv->ipv4_addr_entry ), (gpointer)ipv4_addr_changed_cb, (gpointer)self);
    if ( ipv4_address != NULL ) { 
          gtk_label_set_text(GTK_LABEL(prv->ipv4_addr_lbl), ipv4_address);
          gtk_entry_set_text(GTK_ENTRY(prv->ipv4_addr_entry), ipv4_address);
    }
    else {
          gtk_label_set_text(GTK_LABEL(prv->ipv4_addr_lbl), "");
          gtk_entry_set_text(GTK_ENTRY(prv->ipv4_addr_entry), "");
    }
    gtk_widget_set_sensitive(GTK_WIDGET(prv->ipv4_addr_lbl), modifiable);
    gtk_widget_set_sensitive(GTK_WIDGET(prv->ipv4_addr_entry), modifiable);

    g_signal_handlers_unblock_by_func(G_OBJECT(prv->ipv4_addr_entry ), (gpointer)ipv4_addr_changed_cb, (gpointer)self);
    
    g_signal_handlers_block_by_func(G_OBJECT(prv->ipv4_subnet_entry ), (gpointer)ipv4_subnet_changed_cb, (gpointer)self);
    if ( ipv4_subnet != NULL ) { 
          gtk_label_set_text(GTK_LABEL(prv->ipv4_subnet_lbl), ipv4_subnet);
          gtk_entry_set_text(GTK_ENTRY(prv->ipv4_subnet_entry), ipv4_subnet);
    }
    else {
          gtk_label_set_text(GTK_LABEL(prv->ipv4_subnet_lbl), "");
          gtk_entry_set_text(GTK_ENTRY(prv->ipv4_subnet_entry), "");
    }
    gtk_widget_set_sensitive(GTK_WIDGET(prv->ipv4_subnet_lbl), modifiable);
    gtk_widget_set_sensitive(GTK_WIDGET(prv->ipv4_subnet_entry), modifiable);

    g_signal_handlers_unblock_by_func(G_OBJECT(prv->ipv4_subnet_entry ), (gpointer)ipv4_subnet_changed_cb, (gpointer)self);

    if ( set_initial_state ) {
        if ( ! ipv6_active ) { 
            gtk_combo_box_set_active(GTK_COMBO_BOX(prv->ipv6_combo), 0); /* Select None On Combo Box */
        }
        else {
            gtk_combo_box_set_active(GTK_COMBO_BOX(prv->ipv6_combo), 1); /* Select Enabled On Combo Box */
            /* TODO - Check for Multiple IP Addresses  and update combo to reflect */
        }
        gtk_widget_set_sensitive(GTK_WIDGET(prv->ipv6_combo), modifiable);
    }


/*
 *  TODO - Handle IPv6 Accept buttons 
 *
	GtkCheckButton *ipv6_accept_stateful_cb;
	GtkCheckButton *ipv6_accept_stateless_cb;
*/
    g_free (ipv4_address);
    g_free (ipv4_subnet);
    g_free (ipv6_address);
    g_free (ipv6_prefix);
}

/*
 * Initiliaze all ncu if possible
 */
static void
nwam_connstatus_elements_init(NwamConnConfIPPanel *self)
{
	
}

/**
 * nwam_conf_ip_panel_new:
 * @returns: a new #NwamConnConfIPPanel.
 *
 * Creates a new #NwamConnConfIPPanel
 **/
NwamConnConfIPPanel*
nwam_conf_ip_panel_new(void)
{
	return NWAM_CONN_CONF_IP_PANEL(g_object_new(NWAM_TYPE_CONN_CONF_IP_PANEL, NULL));
}

/**
 * refresh:
 *
 * Refresh #NwamConnConfIPPanel
 **/
static gboolean
refresh(NwamPrefIFace *iface, gpointer user_data, gboolean force)
{
    NwamConnConfIPPanel *self = NWAM_CONN_CONF_IP_PANEL(iface);
    gboolean init_refresh = FALSE;
    NwamuiNcu* ncu = NWAMUI_NCU(user_data);

    g_return_val_if_fail( NWAM_IS_CONN_CONF_IP_PANEL(self), init_refresh );
    
    if ( ! NWAMUI_IS_NCU(ncu) ) {
        return( init_refresh );
    }

    if (self->prv->ncu != ncu) {
        init_refresh = TRUE;

        if ( self->prv->ncu != NULL ) {
            g_signal_handler_disconnect(G_OBJECT(self->prv->ncu),
              self->prv->ncu_handler_id );
            g_object_unref(self->prv->ncu );
        }
        self->prv->ncu = g_object_ref(ncu);

        self->prv->ncu_handler_id =     
          g_signal_connect(G_OBJECT(self->prv->ncu),
            "notify", (GCallback)ncu_changed_notify_cb, self);
    }
    
    populate_panel(NWAM_CONN_CONF_IP_PANEL(self), init_refresh | force);

    return TRUE;
}

static gboolean
cancel(NwamPrefIFace *iface, gpointer user_data)
{
}

static gboolean
apply(NwamPrefIFace *iface, gpointer user_data)
{
    NwamConnConfIPPanel *self = NWAM_CONN_CONF_IP_PANEL(iface);
    NwamConnConfIPPanelPrivate* prv = self->prv;
    
    nwamui_ncu_type_t   ncu_type;
    gboolean            ipv4_dhcp;
    const gchar*        ipv4_address;
    const gchar*        ipv4_subnet;
    gboolean            ipv6_active;
    gboolean            ipv6_dhcp;
    gchar*              ipv6_address;
    gchar*              ipv6_prefix;
    NwamuiWifiNet*      wifi_info = NULL; 
    GtkTreeModel*   model;

    if ( ! NWAMUI_IS_NCU(self->prv->ncu) ) {
        return( TRUE );
    }

    prv = self->prv;
    
    ncu_type = nwamui_ncu_get_ncu_type( NWAMUI_NCU(prv->ncu) );
    
    if ( ncu_type == NWAMUI_NCU_TYPE_WIRELESS) {
        GList*          fav_list;
        GtkTreeIter     iter;
        
        wifi_info = nwamui_ncu_get_wifi_info( NWAMUI_NCU(prv->ncu) );
        
        /* Populate WiFi Favourites */
        model = GTK_TREE_MODEL( gtk_tree_view_get_model(GTK_TREE_VIEW(prv->wifi_fav_tv)));
        fav_list = capplet_model_to_list(model);
        nwamui_daemon_set_fav_wifi_networks( NWAMUI_DAEMON(prv->daemon),  fav_list);
        if (fav_list) {
            nwamui_util_free_obj_list( fav_list );
        }

        /* Apply WiFi conditions */
        {
            NwamuiProf*     prof;
            gboolean join_open, join_preferred, add_any_wifi;
            gint action_if_no_fav;
        
            prof = nwamui_prof_get_instance ();
            
            action_if_no_fav = gtk_combo_box_get_active (prv->no_preferred_networks_combo);
            join_open = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (prv->join_open_cbox));
            join_preferred = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (prv->join_preferred_cbox));
            add_any_wifi = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (prv->add_cbox));
        
            if (prv->action_if_no_fav != action_if_no_fav) {
                prv->action_if_no_fav = action_if_no_fav;
                g_object_set (prof,
                  "action_on_no_fav_networks", prv->action_if_no_fav,
                  NULL);
            }
            
            if (prv->join_open != join_open) {
                prv->join_open = join_open;
                g_object_set (prof,
                  "join_wifi_not_in_fav", prv->join_open,
                  NULL);
            }
            if (prv->join_preferred != join_preferred) {
                prv->join_preferred = join_preferred;
                g_object_set (prof,
                  "join_any_fav_wifi", prv->join_preferred,
                  NULL);
            }
            if (prv->add_any_wifi != add_any_wifi) {
                prv->add_any_wifi = add_any_wifi;
                g_object_set (prof,
                  "add_any_new_wifi_to_fav", prv->add_any_wifi,
                  NULL);
            }

            g_object_unref (prof);
        }
    }

    if ( nwamui_ncu_is_modifiable( prv->ncu ) ) {
        switch (gtk_combo_box_get_active(GTK_COMBO_BOX(prv->ipv4_combo))) {
        case 1:
            nwamui_ncu_set_ipv4_dhcp( NWAMUI_NCU(prv->ncu), TRUE);
            break;
        case 2:
            ipv4_address = gtk_entry_get_text(GTK_ENTRY(prv->ipv4_addr_entry));
            ipv4_subnet = gtk_entry_get_text(GTK_ENTRY(prv->ipv4_subnet_entry));
            nwamui_ncu_set_ipv4_dhcp( NWAMUI_NCU(prv->ncu), FALSE);
            nwamui_ncu_set_ipv4_address( NWAMUI_NCU(prv->ncu),
              ipv4_address ? ipv4_address : "");
            nwamui_ncu_set_ipv4_subnet( NWAMUI_NCU(prv->ncu),
              ipv4_subnet ? ipv4_subnet : "");
            break;
        case 3:
            model = GTK_TREE_MODEL( gtk_tree_view_get_model(GTK_TREE_VIEW(prv->ipv4_tv)));
            nwamui_ncu_set_v4addresses(NWAMUI_NCU(prv->ncu), GTK_LIST_STORE(model));
            break;
        default:
            /* Disable ipv4 dhcp */
            nwamui_ncu_set_ipv4_dhcp( NWAMUI_NCU(prv->ncu), FALSE);
            break;
        }
        
        if ( nwamui_ncu_get_ipv6_active( NWAMUI_NCU(prv->ncu) ) ) { 
            switch (gtk_combo_box_get_active(GTK_COMBO_BOX(prv->ipv6_combo))) {
            case 0:
                nwamui_ncu_set_ipv6_dhcp( NWAMUI_NCU(prv->ncu), FALSE);
                break;
            case 1:
                model = GTK_TREE_MODEL( gtk_tree_view_get_model(GTK_TREE_VIEW(prv->ipv6_tv)));
                nwamui_ncu_set_v6addresses(NWAMUI_NCU(prv->ncu), GTK_LIST_STORE(model));
                break;
            default:
                /* Disable ipv6 dhcp */
                break;
            }
        }
    }
    
    return( TRUE );
}

/**
 * help:
 *
 * Help #NwamConnConfIPPanel
 **/
static gboolean
help(NwamPrefIFace *iface, gpointer user_data)
{
    g_debug ("NwamConnConfIPPanel: Help");
    nwamui_util_show_help (HELP_REF_CONNECTPROPS_CONFIG);
}

/**
 * nwam_multi_ip_add:
 * @self,
 * @connection: FIXME should be a specific type.
 * 
 * Add an ip to tree view.
 */
static void
nwam_multi_ip_add (NwamConnConfIPPanel *self, gpointer connection)
{
	/* FIXME
	GtkTreeIter iter;
	gtk_tree_store_append (GTK_TREE_STORE(self->prv->model), &iter, NULL);
	gtk_tree_store_set (GTK_TREE_STORE(self->prv->model), &iter,
			    0, connection,
			    -1);
	*/
}

static void
nwam_multi_ip_clear (NwamConnConfIPPanel *self)
{
	/* FIXME
	gtk_tree_store_clear (GTK_TREE_STORE(self->prv->model));
	*/
}

static void
nwam_conf_ip_panel_finalize(NwamConnConfIPPanel *self)
{
    if ( self->prv->ncu != NULL ) {
        g_signal_handler_disconnect(G_OBJECT(self->prv->ncu), self->prv->ncu_handler_id );
        g_object_unref( self->prv->ncu );
    }
        
    if ( self->prv->daemon != NULL ) {
        g_object_unref( self->prv->daemon );
    }

    self->prv = NULL;

    G_OBJECT_CLASS(nwam_conf_ip_panel_parent_class)->finalize(G_OBJECT(self));
}

/* Callbacks */

/*
 * We don't need a comp here actually due to requirements
 */
static gint
nwam_conn_multi_ip_comp_cb (GtkTreeModel *model,
			 GtkTreeIter *a,
			 GtkTreeIter *b,
			 gpointer user_data)
{
	return 0;
}

/*
 * We don't need a comp here actually due to requirements
 */
static gint
nwam_conn_wifi_fav_comp_cb (GtkTreeModel *model,
			 GtkTreeIter *a,
			 GtkTreeIter *b,
			 gpointer user_data)
{
	return 0;
}

/*
static void
renew_cb( GtkButton *button, gpointer data )
{
	NwamConnConfIPPanel* self = NWAM_CONN_CONF_IP_PANEL(data);
	g_assert (self->prv->ncu);
	if (button == self->prv->ipv4_renew_btn) {
		nwamui_ncu_set_ipv4_dhcp (self->prv->ncu, TRUE);
	} else {
		nwamui_ncu_set_ipv6_dhcp (self->prv->ncu, TRUE);
	}
}
*/

static void
multi_line_add_cb( GtkButton *button, gpointer data )
{
	NwamConnConfIPPanel* self = NWAM_CONN_CONF_IP_PANEL(data);
	GtkTreeIter         iter;
	GtkTreeModel*       model;
	GtkTreeView*        view;
    NwamuiIp*           ip = NULL;
    gboolean            is_v6 = FALSE;
	
	if (button == self->prv->ipv4_mutli_add_btn) {
		view = self->prv->ipv4_tv;
	} else {
		view = self->prv->ipv6_tv;
        is_v6 = TRUE;
	}
    g_signal_handlers_block_by_func(G_OBJECT(self->prv->ncu), (gpointer)ncu_changed_notify_cb, (gpointer)self);

    ip = nwamui_ip_new(self->prv->ncu, "", "", is_v6, FALSE, FALSE, FALSE );

	model = gtk_tree_view_get_model (view);
	gtk_list_store_append(GTK_LIST_STORE(model), &iter );
	gtk_list_store_set(GTK_LIST_STORE(model), &iter, 0, ip, -1);
	gtk_tree_selection_select_iter (gtk_tree_view_get_selection(view),
		&iter);

    g_signal_handlers_unblock_by_func(G_OBJECT(self->prv->ncu), (gpointer)ncu_changed_notify_cb, self);
}

static void
multi_line_del_cb( GtkButton *button, gpointer data )
{
	NwamConnConfIPPanel* self = NWAM_CONN_CONF_IP_PANEL(data);
	GtkTreeSelection *selection;
	GList *list, *idx;
	GtkTreeModel *model;
	
    g_signal_handlers_block_by_func(G_OBJECT(self->prv->ncu), (gpointer)ncu_changed_notify_cb, self);

	if (button == self->prv->ipv4_mutli_del_btn) {
		selection = gtk_tree_view_get_selection (self->prv->ipv4_tv);
		model = gtk_tree_view_get_model (self->prv->ipv4_tv);
	} else {
		selection = gtk_tree_view_get_selection (self->prv->ipv6_tv);
		model = gtk_tree_view_get_model (self->prv->ipv6_tv);
	}
	list = gtk_tree_selection_get_selected_rows (selection, NULL);
	
	for (idx=list; idx; idx = g_list_next(idx)) {
		GtkTreeIter  iter;
		g_assert (idx->data);
		if (gtk_tree_model_get_iter(model, &iter, (GtkTreePath *)idx->data)) {
			gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
		} else {
			g_assert_not_reached ();
		}
		gtk_tree_path_free ((GtkTreePath *)idx->data);
	}
	g_list_free (list);

    g_signal_handlers_unblock_by_func(G_OBJECT(self->prv->ncu), (gpointer)ncu_changed_notify_cb, self);
}

static void
object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
	g_debug("NwamConnConfIPPanel: notify %s changed", arg1->name);
}

static void
daemon_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
    NwamConnConfIPPanel*self = NWAM_CONN_CONF_IP_PANEL(data);

	g_debug("NwamConnConfIPPanel: daemon notify %s changed", arg1->name);


    if (arg1 && g_ascii_strcasecmp(arg1->name, "wifi_fav") == 0) {
        populate_wifi_fav( self, TRUE );
    }
}

static void 
ncu_changed_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
    g_debug("NwamConnConfIPPanel: ncu_changed " );

    nwam_pref_refresh(NWAM_PREF_IFACE(data), NWAMUI_NCU(gobject), TRUE); /* Refresh IP Data */
}

static void
show_changed_cb( GtkComboBox* widget, gpointer data )
{
	NwamConnConfIPPanel *self = NWAM_CONN_CONF_IP_PANEL(data);
	GtkNotebook         *cur_nb = NULL;
	GtkLabel            *lbl = NULL;
	gchar               *ipv6_lbl = NULL;
    gboolean             is_v4 = FALSE;

	/* detect which combo is operated by the user before go further */
	if ( is_v4 = (widget == self->prv->ipv4_combo) ) {
		cur_nb = self->prv->ipv4_nb;
	} else {
		cur_nb = self->prv->ipv6_nb;
		lbl = self->prv->ipv6_lbl;
	}
	
	gint actid = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));

	if (lbl) {
		switch (actid) {
            case 0: 
                /* ipv6_lbl = _("None"); */
                ipv6_lbl = "";
                break;
            case 1: 
                ipv6_lbl = _("Enabled");
                break;
            default: 
                g_assert_not_reached();
		}
        if (*ipv6_lbl != '\0') {
            ipv6_lbl = g_strdup_printf(_("Configure IPv_6 (%s)"), ipv6_lbl);
        } else {
            ipv6_lbl = g_strdup_printf(_("Configure IPv_6"));
        }
        gtk_label_set_text_with_mnemonic (lbl, ipv6_lbl);
		g_free (ipv6_lbl);
	}


	/* update the notetab according to the selected entry */
	if (actid == 0) {
		gtk_widget_hide (GTK_WIDGET(cur_nb));
	} else if (actid > 0 && actid <=3) {
		gtk_widget_show (GTK_WIDGET(cur_nb));
		gtk_notebook_set_current_page(cur_nb, actid - 1);
	} else {
		g_assert_not_reached ();
	}

    g_signal_handlers_block_by_func(G_OBJECT(self->prv->ncu), (gpointer)ncu_changed_notify_cb, self);
    if ( nwamui_ncu_is_modifiable( self->prv->ncu ) ) {
        if (actid == 0) {
            if ( is_v4 ) {
                nwamui_ncu_set_ipv4_active( self->prv->ncu, FALSE );
            }
            else {
                nwamui_ncu_set_ipv6_active( self->prv->ncu, FALSE );
            }
        } 
        else if ( actid == 1 ) {
            if ( is_v4 ) {
                nwamui_ncu_set_ipv4_dhcp( self->prv->ncu, TRUE );
            }
        }
        else if (actid > 1 && actid <=3) {
            if ( is_v4 ) {
                nwamui_ncu_set_ipv4_dhcp( self->prv->ncu, FALSE );
            }
        }
    }
    g_signal_handlers_unblock_by_func(G_OBJECT(self->prv->ncu), (gpointer)ncu_changed_notify_cb, self);
}

static void
nwam_conn_wifi_fav_cell_cb (    GtkTreeViewColumn *col,
				GtkCellRenderer   *renderer,
				GtkTreeModel      *model,
				GtkTreeIter       *iter,
				gpointer           data)
{
    guint                cell_num  = (guint)data;
    NwamuiWifiNet       *wifi_info  = NULL;

    gtk_tree_model_get(GTK_TREE_MODEL(model), iter, 0, &wifi_info, -1);
    
    g_assert(NWAMUI_IS_WIFI_NET(wifi_info));
    
    switch (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (renderer), "nwam_wifi_fav_column_id"))) {
    case WIFI_FAV_ESSID:
        {   
            if ( cell_num == 0 ) {
                GdkPixbuf   *status_icon;

                status_icon = nwamui_util_get_wireless_strength_icon( 
                    nwamui_wifi_net_get_signal_strength(wifi_info), NWAMUI_WIRELESS_ICON_TYPE_BARS, TRUE );
                g_object_set (G_OBJECT(renderer),
                  "pixbuf", status_icon,
                  NULL);
                g_object_unref(G_OBJECT(status_icon));
            }
            else if ( cell_num == 1 ) {
                gchar*  essid = nwamui_wifi_net_get_essid(wifi_info);
                
                g_object_set (G_OBJECT(renderer),
                        "text", essid?essid:"",
                        NULL);
                        
                g_free(essid);
            }
        }
        break;
    case WIFI_FAV_SECURITY:
        {   
            nwamui_wifi_security_t sec = nwamui_wifi_net_get_security(wifi_info);
            
            if ( cell_num == 0 ) {
                g_object_set (G_OBJECT(renderer),
                        "text", nwamui_util_wifi_sec_to_short_string(sec),
                        NULL);
            }
            else if ( cell_num == 1 ) {
                GdkPixbuf   *status_icon;

                status_icon = nwamui_util_get_network_security_icon( sec, TRUE );

                g_object_set (G_OBJECT(renderer),
                  "pixbuf", status_icon,
                  NULL);
                if (status_icon)
                    g_object_unref(G_OBJECT(status_icon));
            }
                    
        }
        break;
    case WIFI_FAV_SPEED:
        {   
            guint   speed = nwamui_wifi_net_get_speed( wifi_info );
            gchar*  str;

            if ( speed > 0 ) {
                str = g_strdup_printf(_("%uMb"), speed);
            }
            else {
                str = g_strdup(_("<i>Unknown</i>"));
            }
            
            g_object_set (G_OBJECT(renderer),
                    "markup", str,
                    NULL);
            
            g_free( str );
        }
        break;
    case WIFI_CHANNEL:
        {  
            guint   channel = nwamui_wifi_net_get_channel (wifi_info);
            gchar  *channel_str;
            
            if ( channel > 0 ) {
                channel_str = g_strdup_printf("%d", channel);
            }
            else {
                channel_str = g_strdup(_("<i>Unknown</i>"));
            }

            g_object_set (G_OBJECT(renderer),
                    "markup", channel_str,
                    /* "value", ((((gint)signal) * 100)/(((gint)NWAMUI_WIFI_STRENGTH_LAST) - 1)) % 101, */
                    NULL);
            g_free(channel_str);
                    
        }
        break;
    default:
        g_assert_not_reached ();
    }
}


static void
nwam_conn_multi_ip_cell_cb (    GtkTreeViewColumn *col,
                                GtkCellRenderer   *renderer,
                                GtkTreeModel      *model,
                                GtkTreeIter       *iter,
                                gpointer           data)
{
    GtkTreeView        *view = GTK_TREE_VIEW(data);
    gchar*              hostname;
    gchar*              addr;
    gchar*              subnet;
    NwamuiIp*           ip = NULL;

    gtk_tree_model_get(GTK_TREE_MODEL(model), iter, 0, &ip, -1);
    hostname = g_strdup(""); /* TODO - Remove hostname from IP address table */
    addr = nwamui_ip_get_address( ip );
    subnet = nwamui_ip_get_subnet_prefix( ip );

    switch (gtk_tree_view_column_get_sort_column_id (col)) {
/*
    case IP_VIEW_HOSTNAME:
        if ( hostname != NULL ) {
            g_object_set (G_OBJECT(renderer),
                    "text", hostname,
                    NULL);
        }
        break;
*/
    case IP_VIEW_ADDR:
        if ( addr != NULL ) {
            g_object_set (G_OBJECT(renderer),
                    "text", addr,
                    NULL);
        }
        break;
    case IP_VIEW_MASK:
        if ( subnet != NULL ) {
            g_object_set (G_OBJECT(renderer),
                    "text", subnet,
                    NULL);
        }
        break;
    default:
        g_assert_not_reached ();
    }
    g_free (hostname);
    g_free (addr);
    g_free (subnet);
}

static void
nwam_conn_multi_cell_editing_started_cb(GtkCellRenderer *cell,
                                        GtkCellEditable *editable,
                                        const gchar     *path,
                                        gpointer         data)
{
    if (GTK_IS_ENTRY (editable)) {
        GtkEntry *entry = GTK_ENTRY (editable);
        gboolean  is_v6 = (gboolean)data;
      
        nwamui_util_set_entry_ip_address_only(entry, is_v6, FALSE);
    }
}

static void
nwam_conn_multi_ipv4_cell_edited_cb ( GtkCellRendererText *renderer,
                                    gchar               *path,
                                    gchar               *new_text,
                                    gpointer             data)
{
    NwamConnConfIPPanel *self = NWAM_CONN_CONF_IP_PANEL(data);
    NwamConnConfIPPanelPrivate *prv = GET_PRIVATE(data);
    GtkTreeView        *view = prv->ipv4_tv;
	guint               col_id = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(renderer), "nwam_multi_ip_column_id"));
	GtkTreeModel*       model = GTK_TREE_MODEL (gtk_tree_view_get_model(view));
    NwamuiIp*           ip = NULL;
	GtkTreeIter         iter;
	
	gtk_tree_model_get_iter_from_string (model, &iter, path);
	gtk_tree_model_get(model, &iter, 0, &ip, -1);

    g_signal_handlers_block_by_func(G_OBJECT(self->prv->ncu), (gpointer)ncu_changed_notify_cb, self);

    /* TODO - Validate data in editing */
    if ( !nwamui_util_validate_ip_address( GTK_WIDGET(view), new_text, FALSE, TRUE ) ) {
        /* Invalid */
        GtkTreePath*    tpath = gtk_tree_model_get_path(model, &iter);
        GtkTreeViewColumn* col = gtk_tree_view_get_column(view, col_id );

        gtk_tree_view_set_cursor (GTK_TREE_VIEW(view), tpath, col, FALSE);

        gtk_tree_path_free(tpath);
        return;
    }

	switch (col_id) {
/*
	case IP_VIEW_HOSTNAME:
		break;
*/
	case IP_VIEW_ADDR:
        nwamui_ip_set_address(ip, new_text);
		break;
	case IP_VIEW_MASK:
        nwamui_ip_set_subnet_prefix(ip, new_text);
		break;
	default:
		g_assert_not_reached ();
	}
    g_signal_handlers_unblock_by_func(G_OBJECT(self->prv->ncu), (gpointer)ncu_changed_notify_cb, self);
}

static void
nwam_conn_multi_ipv6_cell_edited_cb ( GtkCellRendererText *renderer,
                                    gchar               *path,
                                    gchar               *new_text,
                                    gpointer             data)
{
    NwamConnConfIPPanel *self = NWAM_CONN_CONF_IP_PANEL(data);
    NwamConnConfIPPanelPrivate *prv = GET_PRIVATE(data);
    GtkTreeView        *view = prv->ipv6_tv;
	guint               col_id = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(renderer), "nwam_multi_ip_column_id"));
	GtkTreeModel*       model = GTK_TREE_MODEL (gtk_tree_view_get_model(view));
    NwamuiIp*           ip = NULL;
	GtkTreeIter         iter;
	
	gtk_tree_model_get_iter_from_string (model, &iter, path);
	gtk_tree_model_get(model, &iter, 0, &ip, -1);

    g_signal_handlers_block_by_func(G_OBJECT(self->prv->ncu), (gpointer)ncu_changed_notify_cb, self);

    /* TODO - Validate data in editing */
    if ( col_id == IP_VIEW_ADDR &&
         !nwamui_util_validate_ip_address( GTK_WIDGET(view), new_text, FALSE, TRUE ) ) {
        /* Invalid */
        GtkTreePath*    tpath = gtk_tree_model_get_path(model, &iter);
        GtkTreeViewColumn* col = gtk_tree_view_get_column(view, col_id );

        gtk_tree_view_set_cursor (GTK_TREE_VIEW(view), tpath, col, FALSE);

        gtk_tree_path_free(tpath);
        return;
    }
	switch (col_id) {
/*
	case IP_VIEW_HOSTNAME:
		break;
*/
	case IP_VIEW_ADDR:
        nwamui_ip_set_address(ip, new_text);
		break;
	case IP_VIEW_MASK:
        nwamui_ip_set_subnet_prefix(ip, new_text);
		break;
	default:
		g_assert_not_reached ();
	}
    g_signal_handlers_unblock_by_func(G_OBJECT(self->prv->ncu), (gpointer)ncu_changed_notify_cb, self);
}

static void 
wireless_tab_add_button_clicked_cb( GtkButton *button, gpointer data )
{
    NwamConnConfIPPanel*self = NWAM_CONN_CONF_IP_PANEL(data);
    NwamuiWifiNet*  new_wifi = NULL;
    
    g_debug("wireless_tab_add_button clicked");

    nwam_wireless_dialog_set_title( self->prv->wifi_dialog, NWAMUI_WIRELESS_DIALOG_TITLE_ADD);

    switch (capplet_dialog_run(NWAM_PREF_IFACE( self->prv->wifi_dialog ), GTK_WIDGET(button))) {
        case GTK_RESPONSE_OK:
                new_wifi = nwam_wireless_dialog_get_wifi_net( self->prv->wifi_dialog );
                if ( new_wifi != NULL ) {
                    /* Add to start of list, set prio to lowest (0). */
                    nwamui_wifi_net_set_priority(new_wifi, 0 );
                    nwamui_daemon_add_wifi_fav(self->prv->daemon, new_wifi);
                }
                nwam_wireless_dialog_set_wifi_net( self->prv->wifi_dialog, NULL ); 
                nwam_pref_refresh(NWAM_PREF_IFACE(self), self->prv->ncu, TRUE); /* Refresh IP Data */
            break;
        default:
            break;
    }
    
}

static void 
wireless_tab_remove_button_clicked_cb( GtkButton *button, gpointer data )
{
    NwamConnConfIPPanel *self = NWAM_CONN_CONF_IP_PANEL(data);
    GtkTreeSelection    *selection;
    GList *list,        *idx;
    GtkTreeModel        *model;
    NwamuiWifiNet       *wifi_net;
	
    g_debug("wireless_tab_remove_button clicked");

    selection = gtk_tree_view_get_selection (self->prv->wifi_fav_tv);
    model = gtk_tree_view_get_model (self->prv->wifi_fav_tv);

    list = gtk_tree_selection_get_selected_rows (selection, NULL);

    for (idx=list; idx; idx = g_list_next(idx)) {
            GtkTreeIter  iter;
            g_assert (idx->data);
            if (gtk_tree_model_get_iter(model, &iter, (GtkTreePath *)idx->data)) {
                gchar*  name;
                gchar*  message;

                gtk_tree_model_get( GTK_TREE_MODEL( model ), &iter, 0, &wifi_net, -1 );
                
                name = nwamui_wifi_net_get_essid(NWAMUI_WIFI_NET(wifi_net));
                message = g_strdup_printf(_("Remove favourite with ESSID '%s'?"), name?name:"" );
                if (nwamui_util_ask_yes_no( GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(button))), _("Remove Wireless Favourite?"), message )) {
                    g_debug("Removing wifi favourite: '%s'", name);

                    nwamui_daemon_remove_wifi_fav(self->prv->daemon, wifi_net );

                    nwam_pref_refresh(NWAM_PREF_IFACE(self), self->prv->ncu, TRUE); /* Refresh IP Data */
                }
            
                if (name)
                    g_free(name);

                g_free(message);
            } else {
                g_assert_not_reached ();
            }
            gtk_tree_path_free ((GtkTreePath *)idx->data);
    }
    g_list_free (list);

}

static void 
wireless_tab_edit_button_clicked_cb( GtkButton *button, gpointer data )
{
    NwamConnConfIPPanel *self = NWAM_CONN_CONF_IP_PANEL(data);
    GtkTreeSelection    *selection;
    GList *list,        *idx;
    GtkTreeModel        *model;
    NwamuiWifiNet       *wifi_net;
	
    g_debug("wireless_tab_edit_button clicked");

    selection = gtk_tree_view_get_selection (self->prv->wifi_fav_tv);
    model = gtk_tree_view_get_model (self->prv->wifi_fav_tv);

    list = gtk_tree_selection_get_selected_rows (selection, NULL);

    for (idx=list; idx; idx = g_list_next(idx)) {
            GtkTreeIter  iter;
            g_assert (idx->data);
            if (gtk_tree_model_get_iter(model, &iter, (GtkTreePath *)idx->data)) {
                gchar*          name;
                gchar*          message;
                NwamuiWifiNet*  new_wifi;

                gtk_tree_model_get( GTK_TREE_MODEL( model ), &iter, 0, &wifi_net, -1 );

                nwam_wireless_dialog_set_wifi_net( self->prv->wifi_dialog, wifi_net );
                nwam_wireless_dialog_set_title( self->prv->wifi_dialog, NWAMUI_WIRELESS_DIALOG_TITLE_EDIT);

                switch (capplet_dialog_run(NWAM_PREF_IFACE( self->prv->wifi_dialog ), GTK_WIDGET(button))) {
                    case GTK_RESPONSE_OK:
                        /* wifi_net object will be already updated, so only need to refresh row. */
                        gtk_tree_model_row_changed(GTK_TREE_MODEL(model), (GtkTreePath *)idx->data, &iter);
                        break;
                    default:
                        break;
                }

                /* Unset wifi_net to ensure that it's not accidently changed! */
                nwam_wireless_dialog_set_wifi_net( self->prv->wifi_dialog, wifi_net );

            } else {
                g_assert_not_reached ();
            }
            gtk_tree_path_free ((GtkTreePath *)idx->data);
    }
    g_list_free (list);
}

static void 
wireless_tab_up_button_clicked_cb( GtkButton *button, gpointer data )
{
    NwamConnConfIPPanel         *self = NWAM_CONN_CONF_IP_PANEL(data);
    NwamConnConfIPPanelPrivate* prv = self->prv;
    GtkTreeModel*               model;
    GtkTreeIter                 iter;
    GtkTreeSelection*           selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(prv->wifi_fav_tv));

    /* TODO - Feedback favourites order changes to the daemon */
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
wireless_tab_down_button_clicked_cb( GtkButton *button, gpointer data )
{
    NwamConnConfIPPanel        *self = NWAM_CONN_CONF_IP_PANEL(data);
    NwamConnConfIPPanelPrivate* prv = self->prv;
    GtkTreeModel*               model;
    GtkTreeIter                 iter;
    GtkTreeSelection*           selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(prv->wifi_fav_tv));

    if ( gtk_tree_selection_get_selected( selection, &model, &iter ) ) {
        GtkTreeIter *next_iter = gtk_tree_iter_copy(&iter);
        
        if ( gtk_tree_model_iter_next(GTK_TREE_MODEL(model), next_iter) ) { /* See if we have somewhere to move down to... */
            gtk_list_store_move_after(GTK_LIST_STORE(model), &iter, next_iter );
        }
        
        gtk_tree_iter_free(next_iter);
    }
}

static void
ipv4_addr_changed_cb( GtkEditable* editable, gpointer data )
{
    NwamConnConfIPPanel        *self = NWAM_CONN_CONF_IP_PANEL(data);
    NwamConnConfIPPanelPrivate* prv = self->prv;
    const gchar*                ipv4_address = NULL;

    ipv4_address = gtk_entry_get_text(GTK_ENTRY(editable));
    if ( ipv4_address != NULL ) {
        g_signal_handlers_block_by_func(G_OBJECT(self->prv->ncu), (gpointer)ncu_changed_notify_cb, self);
        nwamui_ncu_set_ipv4_address( NWAMUI_NCU(prv->ncu), ipv4_address );
        g_signal_handlers_unblock_by_func(G_OBJECT(self->prv->ncu), (gpointer)ncu_changed_notify_cb, self);
    }
}

static void
ipv4_subnet_changed_cb( GtkEditable* editable, gpointer data )
{
    NwamConnConfIPPanel        *self = NWAM_CONN_CONF_IP_PANEL(data);
    NwamConnConfIPPanelPrivate* prv = self->prv;
    const gchar*                ipv4_subnet = NULL;

    ipv4_subnet = gtk_entry_get_text(GTK_ENTRY(editable));
    if ( ipv4_subnet != NULL ) {
        g_signal_handlers_block_by_func(G_OBJECT(self->prv->ncu), (gpointer)ncu_changed_notify_cb, self);
        nwamui_ncu_set_ipv4_subnet( NWAMUI_NCU(prv->ncu), ipv4_subnet );
        g_signal_handlers_unblock_by_func(G_OBJECT(self->prv->ncu), (gpointer)ncu_changed_notify_cb, self);
    }
}


