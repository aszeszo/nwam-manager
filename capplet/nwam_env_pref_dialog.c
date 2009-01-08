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
 * File:   nwamui_env_preferences.c
 *
 * Created on May 29, 2007, 1:11 PM
 * 
 */

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n.h>

#include <libnwamui.h>
#include "nwam_pref_iface.h"
#include "nwam_env_pref_dialog.h"
#include "nwam_proxy_password_dialog.h"
#include <strings.h>

/* Names of Widgets in Glade file */
#define     ENV_PREF_DIALOG_NAME           "nwam_environment"
#define     ENVIRONMENT_NAME_COMBO         "environment_name_combo"
#define     ADD_ENVIRONMENT_BTN            "add_environment_btn"
#define     EDIT_ENVIRONMENT_BTN           "edit_environment_btn"
#define     DUP_ENVIRONMENT_BTN            "dup_environment_btn"
#define     DELETE_ENVIRONMENT_BTN         "delete_environment_btn"
#define     ENVIRONMENT_NOTEBOOK           "environment_notebook"
#define     PROXY_CONFIG_COMBO             "proxy_config_combo"
#define     PROXY_NOTEBOOK                 "proxy_notebook"
#define     HTTP_NAME                      "http_name"
#define     HTTP_PORT_SBOX                 "http_port_sbox"
#define     HTTP_PASSWORD_BTN              "http_password_btn"
#define     USE_FOR_ALL_CB                 "use_for_all_cb"
#define     HTTPS_NAME                     "https_name"
#define     HTTPS_PORT_SBOX                "https_port_sbox"
#define     FTP_NAME                       "ftp_name"
#define     FTP_PORT_SBOX                  "ftp_port_sbox"
#define     GOPHER_NAME                    "gopher_name"
#define     GOPHER_PORT_SBOX               "gopher_port_sbox"
#define     SOCKS_NAME                     "socks_name"
#define     SOCKS_PORT_SBOX                "socks_port_sbox"
#define     NO_PROXY_ENTRY                 "no_proxy_entry"
#define     HTTP_PROXY_LABEL               "http_proxy_label"            
#define     HTTPS_PROXY_LABEL              "https_proxy_label"           
#define     FTP_PROXY_LABEL                "ftp_proxy_label"         
#define     GOPHER_PROXY_LABEL             "gopher_proxy_label"          
#define     SOCKS_PROXY_LABEL              "socks_proxy_label"           
#define     GOPHER_PROXY_PORT_LABEL        "gopher_proxy_port_label"         
#define     SOCKS_PROXY_PORT_LABEL         "socks_proxy_port_label"          
#define     HTTPS_PROXY_PORT_LABEL         "https_proxy_port_label"          
#define     HTTP_PROXY_PORT_LABEL          "http_proxy_port_label"           
#define     FTP_PROXY_PORT_LABEL           "ftp_proxy_port_label"            
#define     DEFAULT_NETSERVICES_LIST       "default_netservices_list"
#define     ADDITIONAL_NETSERVICES_LIST    "additional_netservices_list"
#define     ADD_NETSERVICE_BTN             "add_netservice_btn"
#define     DELETE_NETSERVICE_BTN          "delete_netservice_btn"
#define     ONLY_ALLOW_LABEL               "only_allow_lbl"
#define     CONDITIONS_SCROLLWIN           "conditions_scrolledwindow"

#define     IPPOOL_CB                      "ippool_cb"
#define     IPNAT_CB                       "ipnat_cb"
#define     IPFILTER_V6_CB                 "ipfilter_v6_cb"
#define     IPFILTER_CB                    "ipfilter_cb"
#define     IPPOOL_FILE_CHOOSER            "ippool_file_chooser"
#define     IPNAT_FILE_CHOOSER             "ipnat_file_chooser"
#define     IPFILTER_V6_FILE_CHOOSER       "ipfilter_v6_file_chooser"
#define     IPFILTER_FILE_CHOOSER          "ipfilter_file_chooser"

#define     NAMESERVICES_CONFIG_COMBO      "nameservices_config_combo"
#define     NAMESERVICES_ADD_BTN           "nameservices_add_btn"
#define     NAMESERVICES_DELETE_BTN        "nameservices_delete_btn"
#define     NSSWITCH_FILE_BTN              "nsswitch_file_btn"

enum {
    PROXY_MANUAL_PAGE = 0, 
    PROXY_PAC_PAGE 
};

enum {
    SVC_CHECK_BOX = 0,
    SVC_INFO
};

#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
	NWAM_TYPE_ENV_PREF_DIALOG, NwamEnvPrefDialogPrivate)) 
	
struct _NwamEnvPrefDialogPrivate {

    /* Widget Pointers */
    GtkDialog*                  env_pref_dialog;
    GtkComboBox*                environment_name_combo;
    GtkButton*                  add_environment_btn;
    GtkButton*                  edit_environment_btn;
    GtkButton*                  dup_environment_btn;
    GtkButton*                  delete_environment_btn;
    GtkNotebook*                environment_notebook;
    GtkComboBox*                proxy_config_combo;
    GtkNotebook*                proxy_notebook;
    GtkEntry*                   http_name;
    GtkSpinButton*              http_port_sbox;
    GtkButton*                  http_password_btn;
    NwamProxyPasswordDialog*    proxy_password_dialog;
    GtkCheckButton*             use_for_all_cb;
    GtkEntry*                   https_name;
    GtkSpinButton*              https_port_sbox;
    GtkEntry*                   ftp_name;
    GtkSpinButton*              ftp_port_sbox;
    GtkEntry*                   gopher_name;
    GtkSpinButton*              gopher_port_sbox;
    GtkEntry*                   socks_name;
    GtkSpinButton*              socks_port_sbox;
    GtkEntry*                   no_proxy_entry;
    GtkLabel*                   http_proxy_label;
    GtkLabel*                   https_proxy_label;
    GtkLabel*                   ftp_proxy_label;
    GtkLabel*                   gopher_proxy_label;
    GtkLabel*                   socks_proxy_label;
    GtkLabel*                   gopher_proxy_port_label;
    GtkLabel*                   socks_proxy_port_label;
    GtkLabel*                   https_proxy_port_label;
    GtkLabel*                   http_proxy_port_label;
    GtkLabel*                   ftp_proxy_port_label;
    GtkLabel*                   only_allow_label;
    
    GtkTreeView*                default_netservices_list;
    GtkTreeView*                additional_netservices_list;
    GtkButton*                  add_netservice_btn;
    GtkButton*                  delete_netservice_btn;
    GtkBox*                     conditions_vbox;

    GtkCheckButton*             ippool_cb;
    GtkCheckButton*             ipnat_cb;
    GtkCheckButton*             ipfilter_cb;
    GtkCheckButton*             ipfilter_v6_cb;
    GtkFileChooserButton*       ippool_file_chooser;
    GtkFileChooserButton*       ipnat_file_chooser;
    GtkFileChooserButton*       ipfilter_file_chooser;
    GtkFileChooserButton*       ipfilter_v6_file_chooser;

    GtkComboBox*                nameservices_config_combo;
    GtkButton*                  nameservices_add_btn;
    GtkButton*                  nameservices_delete_btn;
    GtkFileChooserButton*       nsswitch_file_btn;

    /* Other Data */
    NwamuiDaemon*               daemon;
    NwamuiEnv*                  selected_env;
};

static void         nwam_env_pref_dialog_finalize (NwamEnvPrefDialog *self);
static void         nwam_pref_init (gpointer g_iface, gpointer iface_data);

static void         change_env_combo_model(NwamEnvPrefDialog *self);

static void         populate_dialog( NwamEnvPrefDialog* self );

static void         select_proxy_panel( NwamEnvPrefDialog* self, nwamui_env_proxy_type_t proxy_type );

static void nwam_compose_tree_view (NwamEnvPrefDialog *self);
static void response_cb( GtkWidget* widget, gint repsonseid, gpointer data );
static gboolean refresh (NwamEnvPrefDialog *self, gpointer data, gboolean force);
static gboolean apply (NwamPrefIFace *self, gpointer data);
static gboolean help (NwamPrefIFace *self, gpointer data);

/* Callbacks */
static void         env_selection_changed_cb( GtkWidget* widget, gpointer data );

static void         proxy_config_changed_cb( GtkWidget* widget, gpointer data );

static void         use_for_all_toggled(GtkToggleButton *togglebutton, gpointer user_data);

static void         server_text_changed_cb(GtkEntry *entry, gpointer  user_data);

static void         port_changed_cb(GtkSpinButton *sbutton, gpointer  user_data);

static void         env_add_button_clicked_cb(GtkButton *button, gpointer  user_data);

static void         env_rename_button_clicked_cb(GtkButton *button, gpointer  user_data);

static void         env_duplicate_button_clicked_cb(GtkButton *button, gpointer  user_data);

static void         env_delete_button_clicked_cb(GtkButton *button, gpointer  user_data);

static void         http_password_button_clicked_cb(GtkButton *button, gpointer  user_data);

static void default_svc_status_cb (GtkTreeViewColumn *tree_column,
                                   GtkCellRenderer *cell,
                                   GtkTreeModel *tree_model,
                                   GtkTreeIter *iter,
                                   gpointer data);
static void default_svc_toggled_cb (GtkCellRendererToggle *cell_renderer,
                                    gchar                 *path,
                                    gpointer               user_data);
static void additional_svc_status_cb (GtkTreeViewColumn *tree_column,
                                      GtkCellRenderer *cell,
                                      GtkTreeModel *tree_model,
                                      GtkTreeIter *iter,
                                      gpointer data);
static void additional_svc_toggled_cb (GtkCellRendererToggle *cell_renderer,
                                       gchar                 *path,
                                       gpointer               user_data);
static gboolean default_netservices_vfunc (GtkTreeModel *model,
                                           GtkTreeIter *iter,
                                           gpointer data);
static gboolean additional_netservices_vfunc (GtkTreeModel *model,
                                              GtkTreeIter *iter,
                                              gpointer data);

G_DEFINE_TYPE_EXTENDED (NwamEnvPrefDialog,
  nwam_env_pref_dialog,
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
}

static void
nwam_env_pref_dialog_class_init (NwamEnvPrefDialogClass *klass)
{
    /* Pointer to GObject Part of Class */
    GObjectClass *gobject_class = (GObjectClass*) klass;
        
    /* Override Some Function Pointers */
    gobject_class->finalize = (void (*)(GObject*)) nwam_env_pref_dialog_finalize;

    g_type_class_add_private (klass, sizeof (NwamEnvPrefDialogPrivate));
}

        
static void
nwam_env_pref_dialog_init (NwamEnvPrefDialog *self)
{
    GtkTreeModel    *model = NULL;
    
	NwamEnvPrefDialogPrivate *prv = GET_PRIVATE(self);
    GtkWidget *conditions_scrollwin;
	self->prv = prv;
    
    /* Iniialise pointers to important widgets */
    prv->env_pref_dialog = GTK_DIALOG(nwamui_util_glade_get_widget(ENV_PREF_DIALOG_NAME));
    prv->environment_name_combo = GTK_COMBO_BOX(nwamui_util_glade_get_widget( ENVIRONMENT_NAME_COMBO ));
    prv->add_environment_btn = GTK_BUTTON(nwamui_util_glade_get_widget( ADD_ENVIRONMENT_BTN ));
    prv->edit_environment_btn = GTK_BUTTON(nwamui_util_glade_get_widget( EDIT_ENVIRONMENT_BTN ));
    prv->dup_environment_btn = GTK_BUTTON(nwamui_util_glade_get_widget( DUP_ENVIRONMENT_BTN ));
    prv->delete_environment_btn = GTK_BUTTON(nwamui_util_glade_get_widget( DELETE_ENVIRONMENT_BTN ));
    prv->environment_notebook = GTK_NOTEBOOK(nwamui_util_glade_get_widget( ENVIRONMENT_NOTEBOOK ));
    prv->proxy_config_combo = GTK_COMBO_BOX(nwamui_util_glade_get_widget( PROXY_CONFIG_COMBO ));
    prv->proxy_notebook = GTK_NOTEBOOK(nwamui_util_glade_get_widget( PROXY_NOTEBOOK ));
    prv->http_name = GTK_ENTRY(nwamui_util_glade_get_widget( HTTP_NAME ));
    prv->http_port_sbox = GTK_SPIN_BUTTON(nwamui_util_glade_get_widget( HTTP_PORT_SBOX ));
    prv->http_password_btn = GTK_BUTTON(nwamui_util_glade_get_widget( HTTP_PASSWORD_BTN ));
    prv->use_for_all_cb = GTK_CHECK_BUTTON(nwamui_util_glade_get_widget( USE_FOR_ALL_CB ));
    prv->https_name = GTK_ENTRY(nwamui_util_glade_get_widget( HTTPS_NAME ));
    prv->https_port_sbox = GTK_SPIN_BUTTON(nwamui_util_glade_get_widget( HTTPS_PORT_SBOX ));
    prv->ftp_name = GTK_ENTRY(nwamui_util_glade_get_widget( FTP_NAME ));
    prv->ftp_port_sbox = GTK_SPIN_BUTTON(nwamui_util_glade_get_widget( FTP_PORT_SBOX ));
    prv->gopher_name = GTK_ENTRY(nwamui_util_glade_get_widget( GOPHER_NAME ));
    prv->gopher_port_sbox = GTK_SPIN_BUTTON(nwamui_util_glade_get_widget( GOPHER_PORT_SBOX ));
    prv->socks_name = GTK_ENTRY(nwamui_util_glade_get_widget( SOCKS_NAME ));
    prv->socks_port_sbox = GTK_SPIN_BUTTON(nwamui_util_glade_get_widget( SOCKS_PORT_SBOX ));
    prv->no_proxy_entry = GTK_ENTRY(nwamui_util_glade_get_widget( NO_PROXY_ENTRY ));
    prv->http_proxy_label = GTK_LABEL(nwamui_util_glade_get_widget( HTTP_PROXY_LABEL ));            
    prv->https_proxy_label = GTK_LABEL(nwamui_util_glade_get_widget( HTTPS_PROXY_LABEL ));           
    prv->ftp_proxy_label = GTK_LABEL(nwamui_util_glade_get_widget( FTP_PROXY_LABEL ));         
    prv->gopher_proxy_label = GTK_LABEL(nwamui_util_glade_get_widget( GOPHER_PROXY_LABEL ));          
    prv->socks_proxy_label = GTK_LABEL(nwamui_util_glade_get_widget( SOCKS_PROXY_LABEL ));           
    prv->gopher_proxy_port_label = GTK_LABEL(nwamui_util_glade_get_widget( GOPHER_PROXY_PORT_LABEL ));         
    prv->socks_proxy_port_label = GTK_LABEL(nwamui_util_glade_get_widget( SOCKS_PROXY_PORT_LABEL ));          
    prv->https_proxy_port_label = GTK_LABEL(nwamui_util_glade_get_widget( HTTPS_PROXY_PORT_LABEL ));          
    prv->http_proxy_port_label = GTK_LABEL(nwamui_util_glade_get_widget( HTTP_PROXY_PORT_LABEL ));           
    prv->ftp_proxy_port_label = GTK_LABEL(nwamui_util_glade_get_widget( FTP_PROXY_PORT_LABEL ));            
    prv->default_netservices_list = GTK_TREE_VIEW(nwamui_util_glade_get_widget( DEFAULT_NETSERVICES_LIST ));
    prv->additional_netservices_list = GTK_TREE_VIEW(nwamui_util_glade_get_widget( ADDITIONAL_NETSERVICES_LIST ));
    prv->add_netservice_btn = GTK_BUTTON(nwamui_util_glade_get_widget( ADD_NETSERVICE_BTN ));
    prv->delete_netservice_btn = GTK_BUTTON(nwamui_util_glade_get_widget( DELETE_NETSERVICE_BTN ));
    prv->only_allow_label = GTK_LABEL(nwamui_util_glade_get_widget( ONLY_ALLOW_LABEL ));     

    prv->ippool_cb = GTK_CHECK_BUTTON(nwamui_util_glade_get_widget( IPPOOL_CB ));
    prv->ipnat_cb = GTK_CHECK_BUTTON(nwamui_util_glade_get_widget( IPNAT_CB ));
    prv->ipfilter_v6_cb = GTK_CHECK_BUTTON(nwamui_util_glade_get_widget( IPFILTER_V6_CB ));
    prv->ipfilter_cb = GTK_CHECK_BUTTON(nwamui_util_glade_get_widget( IPFILTER_CB ));
    prv->ippool_file_chooser = GTK_FILE_CHOOSER_BUTTON(nwamui_util_glade_get_widget( IPPOOL_FILE_CHOOSER ));
    prv->ipnat_file_chooser = GTK_FILE_CHOOSER_BUTTON(nwamui_util_glade_get_widget( IPNAT_FILE_CHOOSER ));
    prv->ipfilter_v6_file_chooser = GTK_FILE_CHOOSER_BUTTON(nwamui_util_glade_get_widget( IPFILTER_V6_FILE_CHOOSER ));
    prv->ipfilter_file_chooser = GTK_FILE_CHOOSER_BUTTON(nwamui_util_glade_get_widget( IPFILTER_FILE_CHOOSER ));

    prv->nameservices_config_combo = GTK_COMBO_BOX(nwamui_util_glade_get_widget(NAMESERVICES_CONFIG_COMBO));
    prv->nameservices_add_btn = GTK_BUTTON(nwamui_util_glade_get_widget(NAMESERVICES_ADD_BTN));
    prv->nameservices_delete_btn = GTK_BUTTON(nwamui_util_glade_get_widget(NAMESERVICES_DELETE_BTN));
    prv->nsswitch_file_btn = GTK_FILE_CHOOSER_BUTTON(nwamui_util_glade_get_widget(NSSWITCH_FILE_BTN));
    {
        NwamuiDaemon *daemon = nwamui_daemon_get_instance();
        capplet_compose_nwamui_obj_combo(GTK_COMBO_BOX(prv->nameservices_config_combo), NWAM_PREF_IFACE(self));
        capplet_update_nwamui_obj_combo(GTK_COMBO_BOX(prv->nameservices_config_combo), daemon, NWAMUI_TYPE_ENV);
        gtk_combo_box_set_active(GTK_COMBO_BOX(prv->nameservices_config_combo), 0);
        g_object_unref(daemon);
    }

    conditions_scrollwin = nwamui_util_glade_get_widget(CONDITIONS_SCROLLWIN);
    prv->conditions_vbox = GTK_BOX(nwam_condition_vbox_new());
    gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW(conditions_scrollwin),
                                           GTK_WIDGET(prv->conditions_vbox));
    
    /* Other useful pointer */
    prv->selected_env = NULL;
    prv->daemon = NWAMUI_DAEMON(nwamui_daemon_get_instance());
    prv->proxy_password_dialog = NULL;
    
    change_env_combo_model(self);
    
    /* Add Signal Handlers */
	g_signal_connect(GTK_DIALOG(self->prv->env_pref_dialog), "response", (GCallback)response_cb, (gpointer)self);

    g_signal_connect(GTK_COMBO_BOX(prv->environment_name_combo), "changed", (GCallback)env_selection_changed_cb, (gpointer)self);
    
    g_signal_connect(GTK_BUTTON(prv->add_environment_btn), "clicked", G_CALLBACK(env_add_button_clicked_cb), (gpointer)self);
    g_signal_connect(GTK_BUTTON(prv->edit_environment_btn), "clicked", G_CALLBACK(env_rename_button_clicked_cb), (gpointer)self);
    g_signal_connect(GTK_BUTTON(prv->dup_environment_btn), "clicked", G_CALLBACK(env_duplicate_button_clicked_cb), (gpointer)self);
    g_signal_connect(GTK_BUTTON(prv->delete_environment_btn), "clicked", G_CALLBACK(env_delete_button_clicked_cb), (gpointer)self);
    g_signal_connect(GTK_BUTTON(prv->http_password_btn), "clicked", G_CALLBACK(http_password_button_clicked_cb), (gpointer)self);
    
    g_signal_connect(GTK_COMBO_BOX(prv->proxy_config_combo), "changed", (GCallback)proxy_config_changed_cb, (gpointer)self);
    g_signal_connect(GTK_CHECK_BUTTON(prv->use_for_all_cb), "toggled", (GCallback)use_for_all_toggled, (gpointer)self);
    
    g_signal_connect( prv->http_name, "changed", G_CALLBACK(server_text_changed_cb), (gpointer)self );
    g_signal_connect( prv->http_port_sbox, "value-changed", G_CALLBACK(port_changed_cb), (gpointer)self );

    g_signal_connect( prv->https_name, "changed", G_CALLBACK(server_text_changed_cb), (gpointer)self );
    g_signal_connect( prv->https_port_sbox, "value-changed", G_CALLBACK(port_changed_cb), (gpointer)self );

    g_signal_connect( prv->ftp_name, "changed", G_CALLBACK(server_text_changed_cb), (gpointer)self );
    g_signal_connect( prv->ftp_port_sbox, "value-changed", G_CALLBACK(port_changed_cb), (gpointer)self );

    g_signal_connect( prv->gopher_name, "changed", G_CALLBACK(server_text_changed_cb), (gpointer)self );
    g_signal_connect( prv->gopher_port_sbox, "value-changed", G_CALLBACK(port_changed_cb), (gpointer)self );

    g_signal_connect( prv->socks_name, "changed", G_CALLBACK(server_text_changed_cb), (gpointer)self );
    g_signal_connect( prv->socks_port_sbox, "value-changed", G_CALLBACK(port_changed_cb), (gpointer)self );

    g_signal_connect( prv->no_proxy_entry, "changed", G_CALLBACK(server_text_changed_cb), (gpointer)self );

    nwam_compose_tree_view (self);
}

static void
nwam_env_pref_dialog_finalize (NwamEnvPrefDialog *self)
{
    if ( self->prv->proxy_password_dialog != NULL ) {
        g_object_unref(G_OBJECT(self->prv->proxy_password_dialog));
    }
    
    g_object_unref(G_OBJECT(self->prv->env_pref_dialog ));
    g_object_unref(G_OBJECT(self->prv->environment_name_combo));
    g_object_unref(G_OBJECT(self->prv->add_environment_btn));
    g_object_unref(G_OBJECT(self->prv->edit_environment_btn));
    g_object_unref(G_OBJECT(self->prv->dup_environment_btn));
    g_object_unref(G_OBJECT(self->prv->delete_environment_btn));
    g_object_unref(G_OBJECT(self->prv->environment_notebook));
    g_object_unref(G_OBJECT(self->prv->proxy_config_combo));
    g_object_unref(G_OBJECT(self->prv->proxy_notebook));
    g_object_unref(G_OBJECT(self->prv->http_name));
    g_object_unref(G_OBJECT(self->prv->http_port_sbox));
    g_object_unref(G_OBJECT(self->prv->http_password_btn));
    g_object_unref(G_OBJECT(self->prv->use_for_all_cb));
    g_object_unref(G_OBJECT(self->prv->https_name));
    g_object_unref(G_OBJECT(self->prv->https_port_sbox));
    g_object_unref(G_OBJECT(self->prv->ftp_name));
    g_object_unref(G_OBJECT(self->prv->ftp_port_sbox));
    g_object_unref(G_OBJECT(self->prv->gopher_name));
    g_object_unref(G_OBJECT(self->prv->gopher_port_sbox));
    g_object_unref(G_OBJECT(self->prv->socks_name));
    g_object_unref(G_OBJECT(self->prv->socks_port_sbox));
    g_object_unref(G_OBJECT(self->prv->no_proxy_entry));
    g_object_unref(G_OBJECT(self->prv->http_proxy_label));            
    g_object_unref(G_OBJECT(self->prv->https_proxy_label));           
    g_object_unref(G_OBJECT(self->prv->ftp_proxy_label));         
    g_object_unref(G_OBJECT(self->prv->gopher_proxy_label));          
    g_object_unref(G_OBJECT(self->prv->socks_proxy_label));           
    g_object_unref(G_OBJECT(self->prv->gopher_proxy_port_label));         
    g_object_unref(G_OBJECT(self->prv->socks_proxy_port_label));          
    g_object_unref(G_OBJECT(self->prv->https_proxy_port_label));          
    g_object_unref(G_OBJECT(self->prv->http_proxy_port_label));           
    g_object_unref(G_OBJECT(self->prv->ftp_proxy_port_label));            
    g_object_unref(G_OBJECT(self->prv->default_netservices_list));
    g_object_unref(G_OBJECT(self->prv->additional_netservices_list));
    g_object_unref(G_OBJECT(self->prv->add_netservice_btn));
    g_object_unref(G_OBJECT(self->prv->delete_netservice_btn));
    g_object_unref(G_OBJECT(self->prv->only_allow_label));
    g_object_unref(G_OBJECT(self->prv->conditions_vbox));

    g_object_unref(G_OBJECT(self->prv->ippool_cb));
    g_object_unref(G_OBJECT(self->prv->ipnat_cb));
    g_object_unref(G_OBJECT(self->prv->ipfilter_v6_cb));
    g_object_unref(G_OBJECT(self->prv->ipfilter_cb));
    g_object_unref(G_OBJECT(self->prv->ippool_file_chooser));
    g_object_unref(G_OBJECT(self->prv->ipnat_file_chooser));
    g_object_unref(G_OBJECT(self->prv->ipfilter_v6_file_chooser));
    g_object_unref(G_OBJECT(self->prv->ipfilter_file_chooser));

    g_object_unref(G_OBJECT(self->prv->nameservices_config_combo));
    g_object_unref(G_OBJECT(self->prv->nameservices_add_btn));
    g_object_unref(G_OBJECT(self->prv->nameservices_delete_btn));
    g_object_unref(G_OBJECT(self->prv->nsswitch_file_btn));

    g_object_unref(G_OBJECT(self->prv->daemon));
    G_OBJECT_CLASS(nwam_env_pref_dialog_parent_class)->finalize(G_OBJECT(self));
}

/**
 * nwam_env_pref_dialog_new:
 * @returns: a new #NwamEnvPrefDialog.
 *
 * Creates a new #NwamEnvPrefDialog.
 **/
NwamEnvPrefDialog*
nwam_env_pref_dialog_new (void)
{
    return NWAM_ENV_PREF_DIALOG(g_object_new (NWAM_TYPE_ENV_PREF_DIALOG, NULL));
}


/**
 * nwam_env_pref_dialog_run:
 * @nnwam_env_pref_dialog: a #NwamEnvPrefDialog.
 * @returns: a GTK_DIALOG Response ID
 *
 * 
 * Blocks in a recursive main loop until the dialog either emits the response signal, or is destroyed.
 **/
gint       
nwam_env_pref_dialog_run (NwamEnvPrefDialog  *self, GtkWindow* parent)
{
    gint response = GTK_RESPONSE_NONE;
    
    g_assert(NWAM_IS_ENV_PREF_DIALOG (self));
    
    if ( self->prv->env_pref_dialog != NULL ) {
	if (parent) {
		gtk_window_set_transient_for (GTK_WINDOW(self->prv->env_pref_dialog), parent);
		gtk_window_set_modal (GTK_WINDOW(self->prv->env_pref_dialog), TRUE);
	} else {
		gtk_window_set_transient_for (GTK_WINDOW(self->prv->env_pref_dialog), NULL);
		gtk_window_set_modal (GTK_WINDOW(self->prv->env_pref_dialog), FALSE);		
	}
        populate_dialog( self );
        
        response =  gtk_dialog_run(GTK_DIALOG(self->prv->env_pref_dialog));

    }
    return(response);
    
    
}

static void
nwam_compose_tree_view (NwamEnvPrefDialog *self)
{
	GtkTreeViewColumn *col;
	GtkCellRenderer *cell;
	GtkTreeView *view;

    /*
     * compose the default netservices view
     */
    view = self->prv->default_netservices_list;

	g_object_set (G_OBJECT(view),
		      "headers-clickable", FALSE,
		      "reorderable", TRUE,
		      NULL);
	
	// column enabled
	cell = gtk_cell_renderer_toggle_new();
	col = gtk_tree_view_column_new_with_attributes(NULL,
      cell,
      "resizable", FALSE,
      "clickable", FALSE,
      "sort-indicator", FALSE,
      "reorderable", FALSE,
      NULL);
	gtk_tree_view_column_set_cell_data_func (col,
                                             cell,
                                             default_svc_status_cb,
                                             (gpointer) self,
                                             NULL);
    g_object_set_data (G_OBJECT (cell), "column",
                       GINT_TO_POINTER (SVC_CHECK_BOX));
	g_object_set (cell, "activatable", TRUE, NULL);
    g_signal_connect(G_OBJECT(cell), "toggled",
                     G_CALLBACK(default_svc_toggled_cb),
                     (gpointer)self);
	gtk_tree_view_append_column (view, col);

	// column name
	cell = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes(NULL,
      cell,
      "expand", TRUE,
      "resizable", FALSE,
      "clickable", FALSE,
      "sort-indicator", FALSE,
      "reorderable", FALSE,
      NULL);
	gtk_tree_view_column_set_cell_data_func (col,
                                             cell,
                                             default_svc_status_cb,
                                             (gpointer) self,
                                             NULL);
    g_object_set_data (G_OBJECT (cell), "column",
                       GINT_TO_POINTER (SVC_INFO));
	gtk_tree_view_append_column (view, col);
        
    /*
     * compose view for additional netservices view
     */
    view = self->prv->additional_netservices_list;

	g_object_set (G_OBJECT(view),
		      "headers-clickable", FALSE,
		      "reorderable", TRUE,
		      NULL);
	
	// column enabled
	cell = gtk_cell_renderer_toggle_new();
	col = gtk_tree_view_column_new_with_attributes(NULL,
      cell,
      "resizable", FALSE,
      "clickable", FALSE,
      "sort-indicator", FALSE,
      "reorderable", FALSE,
      NULL);
	gtk_tree_view_column_set_cell_data_func (col,
                                             cell,
                                             default_svc_status_cb,
                                             (gpointer) self,
                                             NULL);
    g_object_set_data (G_OBJECT (cell), "column",
                       GINT_TO_POINTER (SVC_CHECK_BOX));
	g_object_set (cell, "activatable", TRUE, NULL);
    g_signal_connect(G_OBJECT(cell), "toggled",
                     G_CALLBACK(additional_svc_toggled_cb),
                     (gpointer)self);
	gtk_tree_view_append_column (view, col);

	// column name
	cell = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes(NULL,
      cell,
      "expand", TRUE,
      "resizable", FALSE,
      "clickable", FALSE,
      "sort-indicator", FALSE,
      "reorderable", FALSE,
      NULL);
	gtk_tree_view_column_set_cell_data_func (col,
                                             cell,
                                             default_svc_status_cb,
                                             (gpointer) self,
                                             NULL);
    g_object_set_data (G_OBJECT (cell), "column",
                       GINT_TO_POINTER (SVC_INFO));
	gtk_tree_view_append_column (view, col);
}

static void
update_combo( gpointer obj, gpointer user_data )
{
    NwamEnvPrefDialog* self = NWAM_ENV_PREF_DIALOG(user_data);
    GtkComboBox*     combo = GTK_COMBO_BOX( self->prv->environment_name_combo );
    GtkTreeStore*    tree_store;
    NwamuiEnv*       env = NWAMUI_ENV(obj);
    GtkTreeIter      iter;
    
    g_return_if_fail( obj != NULL && user_data != NULL );
    
    tree_store = GTK_TREE_STORE(gtk_combo_box_get_model(combo));
    
    gtk_tree_store_append( tree_store, &iter, NULL );
    gtk_tree_store_set(tree_store, &iter, 0, env, -1 );
    
    if ( nwamui_daemon_is_active_env(self->prv->daemon, env ) ) {
        gtk_combo_box_set_active_iter(GTK_COMBO_BOX(combo), &iter);
    }
}

static void
populate_env_list_combo( NwamEnvPrefDialog* self ) 
{
    NwamEnvPrefDialogPrivate*   prv;
    GList*                      env_list;
    GtkTreeStore*               model;
    
    g_assert( NWAM_IS_ENV_PREF_DIALOG( self ) );
    
    prv = self->prv;
    
    env_list = nwamui_daemon_get_env_list(prv->daemon);
    
    model = GTK_TREE_STORE(gtk_combo_box_get_model(prv->environment_name_combo));
    gtk_tree_store_clear(GTK_TREE_STORE(model));
    g_list_foreach(env_list, update_combo, (gpointer)self);
}

static void
populate_panels_from_env( NwamEnvPrefDialog* self, NwamuiEnv* current_env)
{
    NwamEnvPrefDialogPrivate*   prv;
    gchar*      http_server;
    gint        http_port;
    gchar*      https_server;
    gint        https_port;
    gchar*      ftp_server;
    gint        ftp_port;
    gchar*      gopher_server;
    gint        gopher_port;
    gchar*      socks_server;
    gint        socks_port;
    gchar*      no_proxy;
    gboolean    use_for_all;
    gchar*      env_name;
    gchar*      new_rules_label;
    nwamui_env_proxy_type_t proxy_type;
    
    g_debug("populate_panels_from_env called");
    
    g_assert( current_env != NULL );
    
    prv = self->prv;
    
    /* TODO - Not as per UI Spec - should be using a copy on write mechanism, allowing edits */
    if ( nwamui_env_is_modifiable( current_env ) ) {
        gtk_widget_set_sensitive(GTK_WIDGET(prv->delete_environment_btn), TRUE);
        gtk_widget_set_sensitive(GTK_WIDGET(prv->edit_environment_btn), TRUE);
        gtk_container_foreach(GTK_CONTAINER(self->prv->environment_notebook), (GtkCallback)gtk_widget_set_sensitive, (gpointer)TRUE);
    }
    else {
        gtk_widget_set_sensitive(GTK_WIDGET(prv->delete_environment_btn), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(prv->edit_environment_btn), FALSE);
        gtk_container_foreach(GTK_CONTAINER(self->prv->environment_notebook), (GtkCallback)gtk_widget_set_sensitive, (gpointer)FALSE);
    }
    
    /*
     * Proxy Tab
     */
    proxy_type = nwamui_env_get_proxy_type(current_env);
    switch ( proxy_type ) {
        case NWAMUI_ENV_PROXY_TYPE_DIRECT:
            gtk_combo_box_set_active(GTK_COMBO_BOX(prv->proxy_config_combo), 0 );
            break;
        case NWAMUI_ENV_PROXY_TYPE_MANUAL:
            gtk_combo_box_set_active(GTK_COMBO_BOX(prv->proxy_config_combo), 1 );
            break;
        case NWAMUI_ENV_PROXY_TYPE_PAC_FILE:
            gtk_combo_box_set_active(GTK_COMBO_BOX(prv->proxy_config_combo), 2 );
            break;
    }
    
    http_server = nwamui_env_get_proxy_http_server( current_env );
    http_port = nwamui_env_get_proxy_http_port( current_env );
    https_server = nwamui_env_get_proxy_https_server( current_env );
    https_port = nwamui_env_get_proxy_https_port( current_env );
    ftp_server = nwamui_env_get_proxy_ftp_server( current_env );
    ftp_port = nwamui_env_get_proxy_ftp_port( current_env );
    gopher_server = nwamui_env_get_proxy_gopher_server( current_env );
    gopher_port = nwamui_env_get_proxy_gopher_port( current_env );
    socks_server = nwamui_env_get_proxy_socks_server( current_env );
    socks_port = nwamui_env_get_proxy_socks_port( current_env );
    no_proxy = nwamui_env_get_proxy_bypass_list( current_env );
    use_for_all = nwamui_env_get_use_http_proxy_for_all( current_env );
    
    gtk_entry_set_text(prv->http_name, http_server?http_server:"");
    gtk_spin_button_set_value(prv->http_port_sbox, (gdouble)http_port );
    
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prv->use_for_all_cb), use_for_all);
    
    if ( use_for_all ) {
        g_debug("Using HTTP proxy for all");
        
        gtk_entry_set_text(prv->https_name, http_server?http_server:"");
        gtk_spin_button_set_value(prv->https_port_sbox, (gdouble)http_port );

        gtk_entry_set_text(prv->ftp_name, http_server?http_server:"");
        gtk_spin_button_set_value(prv->ftp_port_sbox, (gdouble)http_port );

        gtk_entry_set_text(prv->gopher_name, http_server?http_server:"");
        gtk_spin_button_set_value(prv->gopher_port_sbox, (gdouble)http_port );

    } else {
        gtk_entry_set_text(prv->https_name, https_server?https_server:"");
        gtk_spin_button_set_value(prv->https_port_sbox, (gdouble)https_port );

        gtk_entry_set_text(prv->ftp_name, ftp_server?ftp_server:"");
        gtk_spin_button_set_value(prv->ftp_port_sbox, (gdouble)ftp_port );

        gtk_entry_set_text(prv->gopher_name, gopher_server?gopher_server:"");
        gtk_spin_button_set_value(prv->gopher_port_sbox, (gdouble)gopher_port );

    }

    gtk_entry_set_text(prv->socks_name, socks_server?socks_server:"");
    gtk_spin_button_set_value(prv->socks_port_sbox, (gdouble)socks_port );

    gtk_entry_set_text(prv->no_proxy_entry, no_proxy?no_proxy:"");
    
    /*
     * Rules Tab
     */
    env_name = nwamui_env_get_name(current_env);
    new_rules_label = g_strdup_printf(_("Only allow '%s' to become active if the following conditions apply:"), env_name );
    gtk_label_set_text(prv->only_allow_label, new_rules_label);
    g_free(env_name);
    g_free(new_rules_label);
    nwam_pref_refresh(NWAM_PREF_IFACE(prv->conditions_vbox), current_env, TRUE);

    /*
     * SVC Tab
     */
#if 0
    GtkTreeModel *model;
    GtkTreeModelFilter *filter;
    
    model = nwamui_env_get_svcs (current_env);

    /* default view */
    filter = GTK_TREE_MODEL_FILTER(gtk_tree_model_filter_new (model,
                                     NULL));
    gtk_tree_view_set_model (prv->default_netservices_list,
      GTK_TREE_MODEL(filter));
    gtk_tree_model_filter_set_visible_func (filter,
      default_netservices_vfunc,
      self,
      NULL);
    gtk_tree_model_filter_refilter (filter);
    g_object_unref (filter);

    /* additional view */
    filter = GTK_TREE_MODEL_FILTER(gtk_tree_model_filter_new (model,
                                     NULL));
    gtk_tree_view_set_model (prv->additional_netservices_list,
      GTK_TREE_MODEL(filter));
    gtk_tree_model_filter_set_visible_func (filter,
      additional_netservices_vfunc,
      self,
      NULL);
    gtk_tree_model_filter_refilter (filter);
    g_object_unref (filter);
    g_object_unref (model);
#endif
}

static void
populate_dialog( NwamEnvPrefDialog* self ) 
{
    NwamEnvPrefDialogPrivate*   prv;
    NwamuiEnv*                  current_env;
    GtkTreeStore*               model;
    GtkTreeIter                 iter;
    
    g_assert( NWAM_IS_ENV_PREF_DIALOG( self ) );
    
    prv = self->prv;
    
    model = GTK_TREE_STORE(gtk_combo_box_get_model(prv->environment_name_combo));

    populate_env_list_combo( self ); /* Will generate an event to handle population of tabs */

}

static void
select_proxy_panel( NwamEnvPrefDialog* self, nwamui_env_proxy_type_t proxy_type )
{
    switch ( proxy_type ) {
        case NWAMUI_ENV_PROXY_TYPE_DIRECT:
            /* No configuration, so hide panels */
            g_debug("Hiding proxy panels (NWAMUI_ENV_PROXY_TYPE_DIRECT)");
            if ( GTK_WIDGET_VISIBLE(self->prv->proxy_notebook) ) {
                gtk_widget_hide_all(GTK_WIDGET(self->prv->proxy_notebook));
            }
            break;
        case NWAMUI_ENV_PROXY_TYPE_MANUAL:
            g_debug("Switching to page NWAMUI_ENV_PROXY_TYPE_MANUAL");
            if ( ! GTK_WIDGET_VISIBLE(self->prv->proxy_notebook) ) {
                gtk_widget_show_all(GTK_WIDGET(self->prv->proxy_notebook));
            }
            gtk_notebook_set_current_page(GTK_NOTEBOOK(self->prv->proxy_notebook), PROXY_MANUAL_PAGE );
            break;
        case NWAMUI_ENV_PROXY_TYPE_PAC_FILE:
            g_debug("Switching to page NWAMUI_ENV_PROXY_TYPE_PAC_FILE");
            if ( ! GTK_WIDGET_VISIBLE(self->prv->proxy_notebook) ) {
                gtk_widget_show_all(GTK_WIDGET(self->prv->proxy_notebook));
            }
            gtk_notebook_set_current_page(GTK_NOTEBOOK(self->prv->proxy_notebook), PROXY_PAC_PAGE );
            break;
        default:
            g_assert_not_reached();
            break;
    }
}


static void
show_env_cell_cb (GtkCellLayout *cell_layout,
			GtkCellRenderer   *renderer,
			GtkTreeModel      *model,
			GtkTreeIter       *iter,
			gpointer           data)
{
	gpointer row_data = NULL;
	gchar *text = NULL;
	
	gtk_tree_model_get(model, iter, 0, &row_data, -1);
	
	if (row_data == NULL) {
		return;
	}
        
	g_assert (G_IS_OBJECT (row_data));
	
	if (NWAMUI_IS_ENV (row_data)) {
		text = nwamui_env_get_name(NWAMUI_ENV(row_data));
		g_object_set (G_OBJECT(renderer), "text", text, NULL);
		g_free (text);
		return;
	}
}

/*
 * Change the combo box model to allow for the inclusion of a reference to the Env to be stored.
 */
static void
change_env_combo_model(NwamEnvPrefDialog *self)
{
	GtkCellRenderer *renderer;
	GtkTreeModel      *model;
	GtkCellRenderer   *pix_renderer;
        
        g_assert(NWAM_IS_ENV_PREF_DIALOG(self));
        
	model = GTK_TREE_MODEL(gtk_tree_store_new(1, G_TYPE_OBJECT));    /* Object Pointer */
	
	gtk_combo_box_set_model(GTK_COMBO_BOX(self->prv->environment_name_combo), model);
	gtk_cell_layout_clear(GTK_CELL_LAYOUT(self->prv->environment_name_combo));
	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(self->prv->environment_name_combo), renderer, TRUE);
	gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(self->prv->environment_name_combo),
					renderer,
					show_env_cell_cb,
					NULL,
					NULL);
	g_object_unref(model);
}

static void
env_selection_changed_cb( GtkWidget* widget, gpointer data )
{
    NwamEnvPrefDialog* self = NWAM_ENV_PREF_DIALOG(data);
    NwamuiEnv*      current_env = NULL;
    GtkTreeModel   *model = NULL;
    GtkTreeIter     iter;

    /* TODO - check if changes were made before switchinh environments, maybe... */
    model = gtk_combo_box_get_model(GTK_COMBO_BOX(widget));

    if ( gtk_combo_box_get_active_iter(GTK_COMBO_BOX(widget), &iter ) ) {
        gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, 0, &current_env, -1);

        /* Maintain for quick access elsewhere */
        if (self->prv->selected_env != NULL ) {
            g_object_unref(self->prv->selected_env);
        }
        self->prv->selected_env = NWAMUI_ENV(g_object_ref(current_env)); 
        
        populate_panels_from_env(self, current_env);

        g_object_unref( G_OBJECT(current_env) );
    }
}

static void
proxy_config_changed_cb( GtkWidget* widget, gpointer data )
{
    NwamEnvPrefDialog*          self = NWAM_ENV_PREF_DIALOG(data);
    NwamuiEnv*                  current_env = NULL;
    GtkTreeModel*               model = NULL;
    GtkTreeIter                 iter;
    gint                        index = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
    nwamui_env_proxy_type_t     proxy_type;
    
    g_debug("proxy_config: index = %d ", index);
    switch ( index ) {
        case 0:
            proxy_type = NWAMUI_ENV_PROXY_TYPE_DIRECT;
            break;
        case 1:
            proxy_type = NWAMUI_ENV_PROXY_TYPE_MANUAL;
            break;
        case 2:
            proxy_type = NWAMUI_ENV_PROXY_TYPE_PAC_FILE;
            break;
        default:
            g_assert_not_reached();
            break;
    }

    /* TODO - check if changes were made before switchinh environments, maybe... */
    model = gtk_combo_box_get_model(GTK_COMBO_BOX(self->prv->environment_name_combo));

    if ( gtk_combo_box_get_active_iter(GTK_COMBO_BOX(self->prv->environment_name_combo), &iter ) ) {
        gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, 0, &current_env, -1);
        if ( proxy_type != nwamui_env_get_proxy_type( current_env ) ) {
            nwamui_env_set_proxy_type( current_env, proxy_type );
        }
        g_object_unref( G_OBJECT(current_env) );
    }

    select_proxy_panel( self, proxy_type );
}


static void
use_for_all_toggled(GtkToggleButton *togglebutton,
                    gpointer         user_data)
{
    NwamEnvPrefDialog*  self = NWAM_ENV_PREF_DIALOG(user_data);
    gboolean            use_for_all = gtk_toggle_button_get_active(togglebutton);
    
    gtk_widget_set_sensitive(GTK_WIDGET(self->prv->https_name), !use_for_all );
    gtk_widget_set_sensitive(GTK_WIDGET(self->prv->https_port_sbox), !use_for_all );
    gtk_widget_set_sensitive(GTK_WIDGET(self->prv->ftp_name), !use_for_all );
    gtk_widget_set_sensitive(GTK_WIDGET(self->prv->ftp_port_sbox), !use_for_all );
    gtk_widget_set_sensitive(GTK_WIDGET(self->prv->gopher_name), !use_for_all );
    gtk_widget_set_sensitive(GTK_WIDGET(self->prv->gopher_port_sbox), !use_for_all );
    gtk_widget_set_sensitive(GTK_WIDGET(self->prv->socks_name), !use_for_all );
    gtk_widget_set_sensitive(GTK_WIDGET(self->prv->socks_port_sbox), !use_for_all );
    gtk_widget_set_sensitive(GTK_WIDGET(self->prv->https_proxy_label), !use_for_all );           
    gtk_widget_set_sensitive(GTK_WIDGET(self->prv->ftp_proxy_label), !use_for_all );         
    gtk_widget_set_sensitive(GTK_WIDGET(self->prv->gopher_proxy_label), !use_for_all );          
    gtk_widget_set_sensitive(GTK_WIDGET(self->prv->socks_proxy_label), !use_for_all );           
    gtk_widget_set_sensitive(GTK_WIDGET(self->prv->gopher_proxy_port_label), !use_for_all );         
    gtk_widget_set_sensitive(GTK_WIDGET(self->prv->socks_proxy_port_label), !use_for_all );          
    gtk_widget_set_sensitive(GTK_WIDGET(self->prv->https_proxy_port_label), !use_for_all );          
    gtk_widget_set_sensitive(GTK_WIDGET(self->prv->ftp_proxy_port_label), !use_for_all );            
    
    nwamui_env_set_use_http_proxy_for_all(self->prv->selected_env, use_for_all );
}

static void
server_text_changed_cb   (GtkEntry *entry,
                        gpointer  user_data)
{
    NwamEnvPrefDialog*  self = NWAM_ENV_PREF_DIALOG(user_data);
    gboolean            use_for_all = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(self->prv->use_for_all_cb));
    const gchar*        text = gtk_entry_get_text(entry);

    if ( entry == self->prv->no_proxy_entry ) {
            nwamui_env_set_proxy_bypass_list(self->prv->selected_env, text);
    }
    else {
        if ( use_for_all ) {
            gtk_entry_set_text(self->prv->https_name, text);
            gtk_entry_set_text(self->prv->ftp_name, text);
            gtk_entry_set_text(self->prv->gopher_name, text);

            nwamui_env_set_proxy_http_server(self->prv->selected_env, text);
        }
        else {
            if ( entry == self->prv->http_name ) {
                nwamui_env_set_proxy_http_server(self->prv->selected_env, text);
            }
            else if ( entry == self->prv->https_name ) {
                nwamui_env_set_proxy_https_server(self->prv->selected_env, text);
            }
            else if ( entry == self->prv->ftp_name ) {
                nwamui_env_set_proxy_ftp_server(self->prv->selected_env, text);
            }
            else if ( entry == self->prv->gopher_name ) {
                nwamui_env_set_proxy_gopher_server(self->prv->selected_env, text);
            }
            else if ( entry == self->prv->socks_name ) {
                nwamui_env_set_proxy_socks_server(self->prv->selected_env, text);
            }
        }
    }
}

static void
port_changed_cb   (GtkSpinButton *sbutton,
                        gpointer  user_data)
{
    NwamEnvPrefDialog*  self = NWAM_ENV_PREF_DIALOG(user_data);
    gboolean            use_for_all = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(self->prv->use_for_all_cb));
    gdouble             value = gtk_spin_button_get_value(sbutton);
    
    if ( use_for_all ) {
        gtk_spin_button_set_value(self->prv->https_port_sbox, value );
        gtk_spin_button_set_value(self->prv->ftp_port_sbox, value );
        gtk_spin_button_set_value(self->prv->gopher_port_sbox, value );

        nwamui_env_set_proxy_http_port(self->prv->selected_env, (gint)value );
    }
    else {
        if ( sbutton == self->prv->http_port_sbox ) {
            nwamui_env_set_proxy_http_port(self->prv->selected_env, (gint)value );
        }
        else if( sbutton == self->prv->https_port_sbox ) {
            nwamui_env_set_proxy_https_port(self->prv->selected_env, (gint)value );
        }
        else if( sbutton == self->prv->ftp_port_sbox ) {
            nwamui_env_set_proxy_ftp_port(self->prv->selected_env, (gint)value );
        }
        else if( sbutton == self->prv->gopher_port_sbox ) {
            nwamui_env_set_proxy_gopher_port(self->prv->selected_env, (gint)value );
        }
        else if( sbutton == self->prv->socks_port_sbox ) {
            nwamui_env_set_proxy_socks_port(self->prv->selected_env, (gint)value );
        }
    }
}

static void
env_add_button_clicked_cb(GtkButton *button, gpointer  user_data)
{
    NwamEnvPrefDialog*          self = NWAM_ENV_PREF_DIALOG(user_data);
    NwamuiEnv*                  new_env = NULL;
    gchar*                      new_name;
    
    new_name = nwamui_util_rename_dialog_run(GTK_WINDOW(self->prv->env_pref_dialog), _("New Location"), "" );

    if ( new_name != NULL && strlen( new_name ) > 0 ) {
        new_env = nwamui_env_new( new_name );
        nwamui_daemon_env_append(self->prv->daemon, new_env );
        
        nwamui_daemon_set_active_env(self->prv->daemon, new_env );

        populate_env_list_combo(self);
        
        g_free(new_name);
        g_object_unref(new_env);
    }
}

static void
env_rename_button_clicked_cb(GtkButton *button, gpointer  user_data)
{
    NwamEnvPrefDialog*          self = NWAM_ENV_PREF_DIALOG(user_data);
    NwamuiEnv*                  current_env = NULL;
    GtkTreeModel*               model = NULL;
    GtkTreeIter                 iter;
    
    /* TODO - check if changes were made before switchinh environments, maybe... */
    model = gtk_combo_box_get_model(GTK_COMBO_BOX(self->prv->environment_name_combo));

    if ( gtk_combo_box_get_active_iter(GTK_COMBO_BOX(self->prv->environment_name_combo), &iter ) ) {
        gchar*  current_name;
        gchar*  new_name;
        
        gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, 0, &current_env, -1);
        
        current_name = nwamui_env_get_name(current_env);
        
        new_name = nwamui_util_rename_dialog_run(GTK_WINDOW(self->prv->env_pref_dialog), _("Rename Location"), current_name );
        
        if ( new_name != NULL ) {
            GtkTreePath*    path = gtk_tree_model_get_path(model, &iter);
            nwamui_env_set_name(current_env, new_name);
            gtk_tree_model_row_changed(model, path, &iter);
            gtk_tree_path_free(path);
            g_free(new_name);
        }

        g_free(current_name);
        g_object_unref( G_OBJECT(current_env) );
    }
    
}

static void
env_duplicate_button_clicked_cb(GtkButton *button, gpointer  user_data)
{
    NwamEnvPrefDialog*          self = NWAM_ENV_PREF_DIALOG(user_data);
    NwamuiEnv*                  current_env = NULL;
    NwamuiEnv*                  new_env;
    GtkTreeModel*               model = NULL;
    GtkTreeIter                 iter;
    
    /* TODO - check if changes were made before switchinh environments, maybe... */
    model = gtk_combo_box_get_model(GTK_COMBO_BOX(self->prv->environment_name_combo));

    if ( gtk_combo_box_get_active_iter(GTK_COMBO_BOX(self->prv->environment_name_combo), &iter ) ) {
        gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, 0, &current_env, -1);

        new_env = nwamui_env_clone( current_env );
        
        nwamui_daemon_env_append(self->prv->daemon, new_env );
        
        nwamui_daemon_set_active_env(self->prv->daemon, new_env );

        populate_env_list_combo(self);
        
        g_object_unref( G_OBJECT(new_env) );
        g_object_unref( G_OBJECT(current_env) );
    }
    

}

static void
env_delete_button_clicked_cb(GtkButton *button, gpointer  user_data)
{
    NwamEnvPrefDialog*          self = NWAM_ENV_PREF_DIALOG(user_data);
    NwamuiEnv*                  current_env = NULL;
    GtkTreeModel*               model = NULL;
    GtkTreeIter                 iter;
    
    /* TODO - check if changes were made before switchinh environments, maybe... */
    model = gtk_combo_box_get_model(GTK_COMBO_BOX(self->prv->environment_name_combo));

    if ( gtk_combo_box_get_active_iter(GTK_COMBO_BOX(self->prv->environment_name_combo), &iter ) ) {
        gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, 0, &current_env, -1);

        if ( nwamui_env_is_modifiable( current_env ) ) {
            /* TODO - Are you sure?? - if active, pick next best... ? */
            gchar*  name = nwamui_env_get_name( current_env );
            gchar*  message = g_strdup_printf(_("Remove location '%s'?"), name?name:"" );
            if (nwamui_util_ask_yes_no( GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(button))), _("Remove Location?"), message )) {
                g_debug("Removing location: '%s'", name);
                
                nwamui_daemon_env_remove(self->prv->daemon, current_env );
        
                populate_env_list_combo(self);
            }
            
            if (name)
                g_free(name);
            
            g_free(message);
        }
        
        g_object_unref( G_OBJECT(current_env) );
    }
        
}

static void
http_password_button_clicked_cb(GtkButton *button, gpointer  user_data)
{
    NwamEnvPrefDialog*  self = NWAM_ENV_PREF_DIALOG(user_data);
    gint                response;
    gchar*              username;
    gchar*              password;
    
    if ( self->prv->proxy_password_dialog == NULL ) {
        self->prv->proxy_password_dialog = nwam_proxy_password_dialog_new();
    }
    
    username = nwamui_env_get_proxy_username(self->prv->selected_env);
    nwam_proxy_password_dialog_set_username(self->prv->proxy_password_dialog, username );
    password = nwamui_env_get_proxy_password(self->prv->selected_env);
    nwam_proxy_password_dialog_set_password(self->prv->proxy_password_dialog, password );
    g_free(username);
    g_free(password);
    
    response = nwam_proxy_password_dialog_run(self->prv->proxy_password_dialog);
    switch( response ) {
        case GTK_RESPONSE_OK: {
                gchar *new_username = nwam_proxy_password_dialog_get_username(self->prv->proxy_password_dialog);
                gchar *new_password = nwam_proxy_password_dialog_get_password(self->prv->proxy_password_dialog);
                nwamui_env_set_proxy_username(self->prv->selected_env, new_username);
                nwamui_env_set_proxy_password(self->prv->selected_env, new_password);
                g_free(new_username);
                g_free(new_password);
            }
            break;
        default:
            break;
    }
}

static void
default_svc_status_cb (GtkTreeViewColumn *tree_column,
                       GtkCellRenderer *cell,
                       GtkTreeModel *tree_model,
                       GtkTreeIter *iter,
                       gpointer data)
{
	NwamEnvPrefDialogPrivate *prv = GET_PRIVATE(data);
    NwamuiSvc *svc;
    gint col;
    GtkTreeIter piter;
    
    col = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (cell), "column"));
    gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (tree_model),
      &piter,
      iter);
    /* svc = nwamui_env_get_svc (prv->selected_env, &piter); */
    
    switch (col) {
    case SVC_CHECK_BOX:
    {
        gboolean enabled = FALSE;
        /* gboolean enabled = nwamui_svc_get_status (svc); */

        g_object_set(G_OBJECT(cell),
          "active", enabled,
          NULL);
        break;
    }
    case SVC_INFO:
    {
        gchar *info = nwamui_svc_get_name (svc);

        g_assert (info);
        g_object_set(G_OBJECT(cell),
          "markup", info,
          NULL);
        g_free (info);
        break;
    }
    }
    /* g_object_unref (svc); */
}

static void
default_svc_toggled_cb (GtkCellRendererToggle *cell_renderer,
                        gchar                 *path,
                        gpointer               user_data)
{
	NwamEnvPrefDialogPrivate *prv = GET_PRIVATE(user_data);
    GtkTreeIter iter, piter;
    GtkTreeModel *model;
    GtkTreePath  *tpath;
    NwamuiSvc *svc;

    model = gtk_tree_view_get_model (prv->default_netservices_list);
    if ((tpath = gtk_tree_path_new_from_string(path)) != NULL
      && gtk_tree_model_get_iter (model, &iter, tpath)) {
        gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model), &piter, &iter);

        /*
        svc = nwamui_env_get_svc (prv->selected_env, &piter);
        nwamui_svc_set_status (svc,
          !gtk_cell_renderer_toggle_get_active(cell_renderer));
        g_object_unref (svc);
          */

        gtk_tree_path_free(tpath);
    }
}

static void
additional_svc_status_cb (GtkTreeViewColumn *tree_column,
                          GtkCellRenderer *cell,
                          GtkTreeModel *tree_model,
                          GtkTreeIter *iter,
                          gpointer data)
{
}

static void
additional_svc_toggled_cb (GtkCellRendererToggle *cell_renderer,
                           gchar                 *path,
                           gpointer               user_data)
{
	NwamEnvPrefDialogPrivate *prv = GET_PRIVATE(user_data);
    GtkTreeIter iter, piter;
    GtkTreeModel *model;
    GtkTreePath  *tpath;
    NwamuiSvc *svc;

    model = gtk_tree_view_get_model (prv->additional_netservices_list);
    if ((tpath = gtk_tree_path_new_from_string(path)) != NULL
      && gtk_tree_model_get_iter (model, &iter, tpath)) {
        gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model), &piter, &iter);

        /*
        svc = nwamui_env_get_svc (prv->selected_env, &piter);
        nwamui_svc_set_status (svc,
          !gtk_cell_renderer_toggle_get_active(cell_renderer));
        g_object_unref (svc);
          */

        gtk_tree_path_free(tpath);
    }
}

static gboolean
default_netservices_vfunc (GtkTreeModel *model,
                           GtkTreeIter *iter,
                           gpointer data)
{
#if 0
	NwamEnvPrefDialogPrivate *prv = GET_PRIVATE(data);
    NwamuiSvc *svc;
    gboolean ret;
    
    svc = nwamui_env_get_svc (prv->selected_env, iter);

    ret = nwamui_svc_is_default (svc);
    g_object_unref (svc);
    
    return ret;
#endif
}


static gboolean
additional_netservices_vfunc (GtkTreeModel *model,
                              GtkTreeIter *iter,
                              gpointer data)
{
#if 0
	NwamEnvPrefDialogPrivate *prv = GET_PRIVATE(data);
    NwamuiSvc *svc;
    gboolean ret;
    
    svc = nwamui_env_get_svc (prv->selected_env, iter);

    ret = nwamui_svc_is_default (svc);
    g_object_unref (svc);

    return !ret;
#endif /* 0 */
}

static void
response_cb( GtkWidget* widget, gint responseid, gpointer data )
{
    NwamEnvPrefDialog* self = NWAM_ENV_PREF_DIALOG(data);
    gboolean            stop_emission = FALSE;
    
    switch (responseid) {
    case GTK_RESPONSE_NONE:
        g_debug("GTK_RESPONSE_NONE");
        break;
    case GTK_RESPONSE_APPLY:
    case GTK_RESPONSE_OK:
        if (nwam_pref_apply (NWAM_PREF_IFACE(data), NULL)) {

            if (responseid == GTK_RESPONSE_OK) {
                gtk_widget_hide( GTK_WIDGET(self->prv->env_pref_dialog) );
            }
            stop_emission = TRUE;
        }
        break;
    case GTK_RESPONSE_CANCEL:
        g_debug("GTK_RESPONSE_CANCEL");
        gtk_widget_hide( GTK_WIDGET(self->prv->env_pref_dialog) );
        stop_emission = TRUE;
        break;
    case GTK_RESPONSE_HELP:
        g_debug("GTK_RESPONSE_HELP");
        nwam_pref_help (NWAM_PREF_IFACE(data), NULL);
        stop_emission = TRUE;
        break;
    }
    if ( stop_emission ) {
        g_signal_stop_emission_by_name(widget, "response" );
    }
}

static gboolean
refresh (NwamEnvPrefDialog *self, gpointer data, gboolean force)
{
    /* TODO nameservices_config_combo need it */
}

static gboolean
apply (NwamPrefIFace *self, gpointer data)
{
	NwamEnvPrefDialogPrivate *prv = GET_PRIVATE(self);
	GtkTreeIter iter;
    GtkTreeModel *model;
    GObject *obj;

    g_debug("NwamEnvPrefDialog apply");

	model = gtk_combo_box_get_model (GTK_COMBO_BOX(prv->environment_name_combo));
    
    /* FIXME, ok need call into separated panel/instance
     * apply all changes, if no errors, hide all
     */
    if (gtk_tree_model_get_iter_first (model, &iter)) {
        do {
            gtk_tree_model_get (model, &iter, 0, &obj, -1);
            if (!nwamui_env_commit (NWAMUI_ENV (obj))) {
                gchar *name = nwamui_env_get_name (NWAMUI_ENV (obj));
                gchar *msg = g_strdup_printf (_("Committing %s faild..."), name);
                nwamui_util_show_message (prv->env_pref_dialog,
                  GTK_MESSAGE_ERROR,
                  _("Commit ENV error"),
                  msg);
                g_free (msg);
                g_free (name);
                return FALSE;
            }
        } while (gtk_tree_model_iter_next (model, &iter));
    }
    return TRUE;
}

static gboolean
help (NwamPrefIFace *self, gpointer data)
{
	NwamEnvPrefDialogPrivate *prv = GET_PRIVATE(self);
    g_debug("NwamEnvPrefDialog help");
}
