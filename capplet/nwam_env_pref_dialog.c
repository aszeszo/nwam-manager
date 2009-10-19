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
 * File:   nwamui_env_preferences.c
 *
 * Created on May 29, 2007, 1:11 PM
 * 
 */

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <strings.h>
#include "libscf.h"

#include <libnwamui.h>
#include "nwam_pref_iface.h"
#include "nwam_env_pref_dialog.h"
#include "nwam_proxy_password_dialog.h"
#include "capplet-utils.h"

/* Nameservices default file strings */
#define  NSSWITCH_FILE_FILES    (SYSCONFDIR "/nsswitch.files")
#define  NSSWITCH_FILE_DNS      (SYSCONFDIR "/nsswitch.dns")
#define  NSSWITCH_FILE_NIS      (SYSCONFDIR "/nsswitch.nis")
#define  NSSWITCH_FILE_LDAP     (SYSCONFDIR "/nsswitch.ldap")

/* Names of Widgets in Glade file */
#define     ENV_PREF_DIALOG_NAME           "nwam_location_properties"
#define     ENVIRONMENT_NOTEBOOK           "environment_notebook"

#ifdef ENABLE_NETSERVICES
#define     ENABLED_NETSERVICES_LIST       "enabled_netservices_list"
#define     DISABLED_NETSERVICES_LIST      "disabled_netservices_list"
#define     ADD_ENABLED_NETSERVICE_BTN     "add_enabled_netservice_btn"
#define     DELETE_ENABLED_NETSERVICE_BTN  "delete_enabled_netservice_btn"
#define     ADD_DISABLED_NETSERVICE_BTN    "add_disabled_netservice_btn"
#define     DELETE_DISABLED_NETSERVICE_BTN "delete_disabled_netservice_btn"
#endif /* ENABLE_NETSERVICES */

#define     NAMESERVICE_FILES_CHECK_BUTTON              "files_service_cb"
#define     NAMESERVICE_DNS_CHECK_BUTTON                "dns_service_cb"
#define     NAMESERVICE_DNS_MANUAL_RB                   "dns_manual_rb"
#define     NAMESERVICE_DNS_DHCP_RB                     "dns_dhcp_rb"
#define     NAMESERVICE_DNS_DOMAIN_ENTRY                "dns_domain_entry"
#define     NAMESERVICE_DNS_DOMAIN_ENTRY_LABEL          "dns_domain_entry_label"
#define     NAMESERVICE_DNS_SEARCH_ENTRY                "dns_search_entry"
#define     NAMESERVICE_DNS_SEARCH_ENTRY_LABEL          "dns_search_entry_label"
#define     NAMESERVICE_DNS_SERVERS_ENTRY               "dns_servers_entry"
#define     NAMESERVICE_DNS_SERVERS_ENTRY_LABEL         "dns_servers_entry_label"
#define     NAMESERVICE_NIS_DOMAIN_LABEL                "nis_domain_label"
#define     NAMESERVICE_NIS_DOMAIN_LABEL_LABEL          "nis_domain_label_label"
#define     NAMESERVICE_NIS_CHECK_BUTTON                "nis_service_cb"
#define     NAMESERVICE_NIS_SERVERS_ENTRY               "nis_servers_entry"
#define     NAMESERVICE_NIS_SERVERS_ENTRY_LABEL         "nis_servers_entry_label"
#define     NAMESERVICE_NIS_MANUAL_RB                   "nis_manual_rb"
#define     NAMESERVICE_NIS_DHCP_RB                     "nis_dhcp_rb"
#define     NAMESERVICE_LDAP_CHECK_BUTTON               "ldap_service_cb"
#define     NAMESERVICE_LDAP_DOMAIN_LABEL                "ldap_domain_label"
#define     NAMESERVICE_LDAP_DOMAIN_LABEL_LABEL          "ldap_domain_label_label"
#define     NAMESERVICE_LDAP_SERVERS_ENTRY              "ldap_servers_entry"
#define     NAMESERVICE_LDAP_SERVERS_ENTRY_LABEL        "ldap_servers_entry_label"
#define     NAMMESERVICE_DEFAULT_DOMAIN                 "default_domain_entry"
#define     NAMMESERVICE_DEFAULT_DOMAIN_LABEL           "default_domain_entry_label"
#define     NSSWITCH_FILE_BTN                           "nsswitch_file_btn"
#define     NSSWITCH_DEFAULT_BTN                        "nsswitch_default_btn"

#ifdef ENABLE_PROXY
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
#endif /* ENABLE_PROXY */

#define     IPPOOL_CONFIG_CB               "ippool_config_cb"
#define     NAT_CONFIG_CB                  "nat_config_cb"
#define     IPF_V6_CONFIG_CB               "ipf_v6_config_cb"
#define     IPF_CONFIG_CB                  "ipf_config_cb"
#define     IPPOOL_FILE_CHOOSER            "ippool_file_chooser"
#define     NAT_FILE_CHOOSER               "nat_file_chooser"
#define     IPFILTER_V6_FILE_CHOOSER       "ipfilter_v6_file_chooser"
#define     IPFILTER_FILE_CHOOSER          "ipfilter_file_chooser"
#define     IKE_CONFIG_CB                  "ike_config_cb"
#define     IPSEC_POLICY_CB                "ipsec_policy_cb"
#define     IPSEC_POLICY_FILE_CHOOSER      "ipsec_policy_file_chooser"
#define     IKE_FILE_CHOOSER               "ike_file_chooser"


/* add fmri dialog */
#define     ENTER_SERVICE_FMRI_DIALOG      "enter_service_fmri_dialog"
#define     SMF_FMRI_ENTRY                 "smf_fmri_entry"

#define TREEVIEW_COLUMN_NUM             "meta:column"

enum {
    PROXY_MANUAL_PAGE = 0, 
    PROXY_PAC_PAGE 
};

enum {
    SVC_INFO = 0,
    NAMESERVICES_NAME,
    DEFAULT_DOMAINNAME,
    DNS_NAMESERVICE_DOMAIN,
    NAMESERVICES_ADDR,
};

#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
	NWAM_TYPE_ENV_PREF_DIALOG, NwamEnvPrefDialogPrivate)) 
	
struct _NwamEnvPrefDialogPrivate {

    /* Widget Pointers */
    GtkDialog*                  env_pref_dialog;
    GtkNotebook*                environment_notebook;

#ifdef ENABLE_PROXY
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
#endif /* ENABLE_PROXY */

    GtkCheckButton*             files_service_cb;
    GtkCheckButton*             dns_service_cb;                 
    GtkEntry*                   dns_servers_entry;                          
    GtkLabel*                   dns_servers_entry_label;                           
    GtkRadioButton*             dns_config_manual_rb;                           
    GtkRadioButton*             dns_config_dhcp_rb;                         
    GtkEntry*                   dns_search_entry;                           
    GtkLabel*                   dns_search_entry_label;                           
    GtkEntry*                   dns_domain_entry;                           
    GtkLabel*                   dns_domain_entry_label;                           
                    
    GtkCheckButton*             nis_service_cb;                 
    GtkEntry*                   nis_servers_entry;                          
    GtkLabel*                   nis_servers_entry_label;                          
    GtkLabel*                   nis_domain_label;                          
    GtkLabel*                   nis_domain_label_label;                          
    GtkRadioButton*             nis_config_manual_rb;                           
    GtkRadioButton*             nis_config_dhcp_rb;                         
                    
    GtkCheckButton*             ldap_service_cb;                    
    GtkLabel*                   ldap_domain_label;                          
    GtkLabel*                   ldap_domain_label_label;                          
    GtkEntry*                   ldap_servers_entry;                            
    GtkLabel*                   ldap_servers_entry_label;                            
                    
    GtkEntry*                   default_domain_entry;                           
    GtkLabel*                   default_domain_entry_label;                           

    guint                       num_nameservices; 
    GtkFileChooserButton*       nsswitch_file_btn;
    GtkButton*                  nsswitch_default_btn;

#ifdef ENABLE_NETSERVICES
    GtkTreeView*                enabled_netservices_list;
    GtkTreeView*                disabled_netservices_list;
    GtkButton*                  add_enabled_netservice_btn;
    GtkButton*                  delete_enabled_netservice_btn;
    GtkButton*                  add_disabled_netservice_btn;
    GtkButton*                  delete_disabled_netservice_btn;
#endif /* ENABLE_NETSERVICES */

    GtkCheckButton*             ippool_config_cb;
    GtkCheckButton*             nat_config_cb;
    GtkCheckButton*             ipf_config_cb;
    GtkCheckButton*             ipf_v6_config_cb;
    GtkFileChooserButton*       ippool_file_chooser;
    GtkFileChooserButton*       nat_file_chooser;
    GtkFileChooserButton*       ipfilter_file_chooser;
    GtkFileChooserButton*       ipfilter_v6_file_chooser;
    GtkCheckButton*             ike_config_cb;
    GtkCheckButton*             ipsec_policy_cb;
    GtkFileChooserButton*       ipsec_policy_file_chooser;
    GtkFileChooserButton*       ike_file_chooser;


    /* add fmri dialog */
    GtkDialog*                  enter_service_fmri_dialog;
    GtkEntry*                   smf_fmri_entry;

    /* Other Data */
    NwamuiDaemon*               daemon;
    NwamuiEnv*                  selected_env;
};

static void nwam_pref_init (gpointer g_iface, gpointer iface_data);
static gboolean refresh(NwamPrefIFace *iface, gpointer user_data, gboolean force);
static gboolean cancel(NwamPrefIFace *iface, gpointer user_data);
static gboolean apply(NwamPrefIFace *iface, gpointer user_data);
static gboolean help(NwamPrefIFace *iface, gpointer user_data);
static gint dialog_run(NwamPrefIFace *iface, GtkWindow *parent);
static GtkWindow* dialog_get_window(NwamPrefIFace *iface);

static void         nwam_env_pref_dialog_finalize (NwamEnvPrefDialog *self);

#ifdef ENABLE_PROXY
static void         select_proxy_panel( NwamEnvPrefDialog* self, nwamui_env_proxy_type_t proxy_type );
#endif /* ENABLE_PROXY */

#ifdef ENABLE_NETSERVICES
static void nwam_compose_tree_view (NwamEnvPrefDialog *self);
#endif /* ENABLE_NETSERVICES */

static void response_cb( GtkWidget* widget, gint repsonseid, gpointer data );

static void fmri_dialog_response_cb( GtkWidget* widget, gint repsonseid, gpointer data );

static void get_smf_service_name(const char *fmri, gchar **name_out, gboolean *valid_fmri );

#if 0
static void action_menu_group_set_visible(NwamMenuGroup *menugroup, const gchar *name, gboolean visible);
#endif

/* Callbacks */
#ifdef ENABLE_PROXY
static void         proxy_config_changed_cb( GtkWidget* widget, gpointer data );

static void         use_for_all_toggled(GtkToggleButton *togglebutton, gpointer user_data);

static void         server_text_changed_cb(GtkEntry *entry, gpointer  user_data);

static void         port_changed_cb(GtkSpinButton *sbutton, gpointer  user_data);

static void         http_password_button_clicked_cb(GtkButton *button, gpointer  user_data);

#endif /* ENABLE_PROXY */

static void         on_ns_source_toggled(GtkToggleButton *togglebutton, gpointer user_data);

static void         on_ns_selection_toggled(GtkToggleButton *togglebutton, gpointer user_data);

static void         on_ns_text_changed(GtkEntry *entry, gpointer  user_data);
    
static void         svc_button_clicked(GtkButton *button, gpointer  user_data);

static void         nsswitch_default_clicked_cb(GtkButton *button, gpointer  user_data);

static void vanity_name_edited ( GtkCellRendererText *cell,
  const gchar         *path_string,
  const gchar         *new_text,
  gpointer             data);

static void vanity_name_editing_started (GtkCellRenderer *cell,
  GtkCellEditable *editable,
  const gchar     *path,
  gpointer         data);

static void menu_pos_func(GtkMenu *menu,
  gint *x,
  gint *y,
  gboolean *push_in,
  gpointer user_data);

static void default_svc_status_cb (GtkTreeViewColumn *tree_column,
                                   GtkCellRenderer *cell,
                                   GtkTreeModel *tree_model,
                                   GtkTreeIter *iter,
                                   gpointer data);

static void nameservices_status_cb (GtkTreeViewColumn *tree_column,
                                   GtkCellRenderer *cell,
                                   GtkTreeModel *tree_model,
                                   GtkTreeIter *iter,
                                   gpointer data);

static void on_button_toggled(GtkToggleButton *button, gpointer user_data);

static void svc_list_merge_to_model(gpointer data, gpointer user_data);

static void on_activate_static_menuitems (GtkAction *action, gpointer data);

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
	iface->cancel = cancel;
    iface->help = help;
    iface->dialog_run = dialog_run;
    iface->dialog_get_window = dialog_get_window;
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
    prv->environment_notebook = GTK_NOTEBOOK(nwamui_util_glade_get_widget( ENVIRONMENT_NOTEBOOK ));

#ifdef ENABLE_PROXY
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
#endif /* ENABLE_PROXY */

    /* Name Services Page */

    /* Used to count number of name services, if > 1 then need to specify a nsswitch.conf file */
    prv->num_nameservices = 0; 

    prv->files_service_cb = GTK_CHECK_BUTTON(nwamui_util_glade_get_widget(NAMESERVICE_FILES_CHECK_BUTTON));
    prv->dns_service_cb = GTK_CHECK_BUTTON(nwamui_util_glade_get_widget(NAMESERVICE_DNS_CHECK_BUTTON));
    prv->dns_config_manual_rb = GTK_RADIO_BUTTON(nwamui_util_glade_get_widget(NAMESERVICE_DNS_MANUAL_RB));
    prv->dns_config_dhcp_rb = GTK_RADIO_BUTTON(nwamui_util_glade_get_widget(NAMESERVICE_DNS_DHCP_RB));
    prv->dns_domain_entry = GTK_ENTRY(nwamui_util_glade_get_widget(NAMESERVICE_DNS_DOMAIN_ENTRY));
    prv->dns_domain_entry_label = GTK_LABEL(nwamui_util_glade_get_widget(NAMESERVICE_DNS_DOMAIN_ENTRY_LABEL));
    prv->dns_search_entry = GTK_ENTRY(nwamui_util_glade_get_widget(NAMESERVICE_DNS_SEARCH_ENTRY));
    prv->dns_search_entry_label = GTK_LABEL(nwamui_util_glade_get_widget(NAMESERVICE_DNS_SEARCH_ENTRY_LABEL));
    prv->dns_servers_entry = GTK_ENTRY(nwamui_util_glade_get_widget(NAMESERVICE_DNS_SERVERS_ENTRY));
    prv->dns_servers_entry_label = GTK_LABEL(nwamui_util_glade_get_widget(NAMESERVICE_DNS_SERVERS_ENTRY_LABEL));
                                                          
    prv->nis_service_cb = GTK_CHECK_BUTTON(nwamui_util_glade_get_widget(NAMESERVICE_NIS_CHECK_BUTTON));
    prv->nis_servers_entry = GTK_ENTRY(nwamui_util_glade_get_widget(NAMESERVICE_NIS_SERVERS_ENTRY));
    prv->nis_servers_entry_label = GTK_LABEL(nwamui_util_glade_get_widget(NAMESERVICE_NIS_SERVERS_ENTRY_LABEL));
    prv->nis_domain_label = GTK_LABEL(nwamui_util_glade_get_widget(NAMESERVICE_NIS_DOMAIN_LABEL));
    prv->nis_domain_label_label = GTK_LABEL(nwamui_util_glade_get_widget(NAMESERVICE_NIS_DOMAIN_LABEL_LABEL));
    prv->nis_config_manual_rb = GTK_RADIO_BUTTON(nwamui_util_glade_get_widget(NAMESERVICE_NIS_MANUAL_RB));
    prv->nis_config_dhcp_rb = GTK_RADIO_BUTTON(nwamui_util_glade_get_widget(NAMESERVICE_NIS_DHCP_RB));
                                                          
    prv->ldap_service_cb = GTK_CHECK_BUTTON(nwamui_util_glade_get_widget(NAMESERVICE_LDAP_CHECK_BUTTON));
    prv->ldap_servers_entry = GTK_ENTRY(nwamui_util_glade_get_widget(NAMESERVICE_LDAP_SERVERS_ENTRY));
    prv->ldap_servers_entry_label = GTK_LABEL(nwamui_util_glade_get_widget(NAMESERVICE_LDAP_SERVERS_ENTRY_LABEL));
    prv->ldap_domain_label = GTK_LABEL(nwamui_util_glade_get_widget(NAMESERVICE_LDAP_DOMAIN_LABEL));
    prv->ldap_domain_label_label = GTK_LABEL(nwamui_util_glade_get_widget(NAMESERVICE_LDAP_DOMAIN_LABEL_LABEL));
                                                          
    prv->default_domain_entry = GTK_ENTRY(nwamui_util_glade_get_widget(NAMMESERVICE_DEFAULT_DOMAIN));
    prv->default_domain_entry_label = GTK_LABEL(nwamui_util_glade_get_widget(NAMMESERVICE_DEFAULT_DOMAIN_LABEL));

    prv->nsswitch_file_btn = GTK_FILE_CHOOSER_BUTTON(nwamui_util_glade_get_widget(NSSWITCH_FILE_BTN));
    prv->nsswitch_default_btn = GTK_BUTTON(nwamui_util_glade_get_widget(NSSWITCH_DEFAULT_BTN));

#ifdef ENABLE_NETSERVICES
    /* Newtwork Services Page */
    prv->enabled_netservices_list = GTK_TREE_VIEW(nwamui_util_glade_get_widget( ENABLED_NETSERVICES_LIST ));
    prv->disabled_netservices_list = GTK_TREE_VIEW(nwamui_util_glade_get_widget( DISABLED_NETSERVICES_LIST ));
    prv->add_enabled_netservice_btn = GTK_BUTTON(nwamui_util_glade_get_widget( ADD_ENABLED_NETSERVICE_BTN ));
    prv->delete_enabled_netservice_btn = GTK_BUTTON(nwamui_util_glade_get_widget( DELETE_ENABLED_NETSERVICE_BTN ));
    prv->add_disabled_netservice_btn = GTK_BUTTON(nwamui_util_glade_get_widget( ADD_DISABLED_NETSERVICE_BTN ));
    prv->delete_disabled_netservice_btn = GTK_BUTTON(nwamui_util_glade_get_widget( DELETE_DISABLED_NETSERVICE_BTN ));
#endif /* ENABLE_NETSERVICES */

    /* Security Page */
    prv->ippool_config_cb = GTK_CHECK_BUTTON(nwamui_util_glade_get_widget( IPPOOL_CONFIG_CB ));
    prv->nat_config_cb = GTK_CHECK_BUTTON(nwamui_util_glade_get_widget( NAT_CONFIG_CB ));
    prv->ipf_v6_config_cb = GTK_CHECK_BUTTON(nwamui_util_glade_get_widget( IPF_V6_CONFIG_CB ));
    prv->ipf_config_cb = GTK_CHECK_BUTTON(nwamui_util_glade_get_widget( IPF_CONFIG_CB ));
    prv->ippool_file_chooser = GTK_FILE_CHOOSER_BUTTON(nwamui_util_glade_get_widget( IPPOOL_FILE_CHOOSER ));
    prv->nat_file_chooser = GTK_FILE_CHOOSER_BUTTON(nwamui_util_glade_get_widget( NAT_FILE_CHOOSER ));
    prv->ipfilter_v6_file_chooser = GTK_FILE_CHOOSER_BUTTON(nwamui_util_glade_get_widget( IPFILTER_V6_FILE_CHOOSER ));
    prv->ipfilter_file_chooser = GTK_FILE_CHOOSER_BUTTON(nwamui_util_glade_get_widget( IPFILTER_FILE_CHOOSER ));
    prv->ike_config_cb = GTK_CHECK_BUTTON(nwamui_util_glade_get_widget( IKE_CONFIG_CB ));
    prv->ipsec_policy_cb = GTK_CHECK_BUTTON(nwamui_util_glade_get_widget( IPSEC_POLICY_CB ));
    prv->ipsec_policy_file_chooser = GTK_FILE_CHOOSER_BUTTON(nwamui_util_glade_get_widget( IPSEC_POLICY_FILE_CHOOSER ));
    prv->ike_file_chooser = GTK_FILE_CHOOSER_BUTTON(nwamui_util_glade_get_widget( IKE_FILE_CHOOSER ));


    /* Other useful pointer */
    prv->selected_env = NULL;
    prv->daemon = NWAMUI_DAEMON(nwamui_daemon_get_instance());
#ifdef ENABLE_PROXY
    prv->proxy_password_dialog = NULL;
#endif /* ENABLE_PROXY */
    
    /* add fmri dialog */
    prv->enter_service_fmri_dialog = GTK_DIALOG(nwamui_util_glade_get_widget(ENTER_SERVICE_FMRI_DIALOG));
    prv->smf_fmri_entry = GTK_ENTRY(nwamui_util_glade_get_widget(SMF_FMRI_ENTRY));

    nwamui_util_set_entry_smf_fmri_completion( prv->smf_fmri_entry );

    /* Add Signal Handlers */
	g_signal_connect(GTK_DIALOG(self->prv->env_pref_dialog), "response", (GCallback)response_cb, (gpointer)self);

#ifdef ENABLE_PROXY
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
#endif /* ENABLE_PROXY */

	g_signal_connect(prv->enter_service_fmri_dialog, "response", (GCallback)fmri_dialog_response_cb, (gpointer)self);

    /* Name Services Page Callbacks */
	g_signal_connect(prv->files_service_cb,
      "toggled", (GCallback)on_ns_selection_toggled, (gpointer)self);

	g_signal_connect(prv->dns_service_cb,
      "toggled", (GCallback)on_ns_selection_toggled, (gpointer)self);
	g_signal_connect(prv->dns_config_manual_rb,
      "toggled", (GCallback)on_ns_source_toggled, (gpointer)self);
	g_signal_connect(prv->dns_config_dhcp_rb,
      "toggled", (GCallback)on_ns_source_toggled, (gpointer)self);
	g_signal_connect(prv->dns_servers_entry,
      "changed", (GCallback)on_ns_text_changed, (gpointer)self);
	g_signal_connect(prv->dns_search_entry,
      "changed", (GCallback)on_ns_text_changed, (gpointer)self);
	g_signal_connect(prv->dns_domain_entry,
      "changed", (GCallback)on_ns_text_changed, (gpointer)self);

    nwamui_util_set_entry_validation(   prv->dns_servers_entry, 
                    NWAMUI_ENTRY_VALIDATION_IS_V4|NWAMUI_ENTRY_VALIDATION_IS_V6|NWAMUI_ENTRY_VALIDATION_ALLOW_LIST|NWAMUI_ENTRY_VALIDATION_ALLOW_EMPTY,
                    TRUE );

    nwamui_util_set_entry_validation(   prv->nis_servers_entry,
                    NWAMUI_ENTRY_VALIDATION_IS_V4|NWAMUI_ENTRY_VALIDATION_IS_V6|NWAMUI_ENTRY_VALIDATION_ALLOW_LIST|NWAMUI_ENTRY_VALIDATION_ALLOW_EMPTY,
                    TRUE );

    nwamui_util_set_entry_validation(   prv->ldap_servers_entry,
                    NWAMUI_ENTRY_VALIDATION_IS_V4|NWAMUI_ENTRY_VALIDATION_IS_V6|NWAMUI_ENTRY_VALIDATION_ALLOW_LIST|NWAMUI_ENTRY_VALIDATION_ALLOW_EMPTY,
                    TRUE );

	g_signal_connect(prv->nis_service_cb,
      "toggled", (GCallback)on_ns_selection_toggled, (gpointer)self);
	g_signal_connect(prv->nis_config_manual_rb,
      "toggled", (GCallback)on_ns_source_toggled, (gpointer)self);
	g_signal_connect(prv->nis_config_dhcp_rb,
      "toggled", (GCallback)on_ns_source_toggled, (gpointer)self);
	g_signal_connect(prv->nis_servers_entry,
      "changed", (GCallback)on_ns_text_changed, (gpointer)self);

	g_signal_connect(prv->ldap_service_cb,
      "toggled", (GCallback)on_ns_selection_toggled, (gpointer)self);
	g_signal_connect(prv->ldap_servers_entry,
      "changed", (GCallback)on_ns_text_changed, (gpointer)self);

	g_signal_connect(prv->default_domain_entry,
      "changed", (GCallback)on_ns_text_changed, (gpointer)self);
    
    g_signal_connect(prv->nsswitch_default_btn,
      "clicked", (GCallback)nsswitch_default_clicked_cb, (gpointer)self );

    /* Security Page Callbacks */
	g_signal_connect(prv->ippool_config_cb,
      "toggled", (GCallback)on_button_toggled, (gpointer)self);
	g_signal_connect(prv->nat_config_cb,
      "toggled", (GCallback)on_button_toggled, (gpointer)self);
	g_signal_connect(prv->ipf_v6_config_cb,
      "toggled", (GCallback)on_button_toggled, (gpointer)self);
	g_signal_connect(prv->ipf_config_cb,
      "toggled", (GCallback)on_button_toggled, (gpointer)self);
    g_signal_connect(prv->ike_config_cb,
      "toggled", (GCallback)on_button_toggled, (gpointer)self);
    g_signal_connect(prv->ipsec_policy_cb,
      "toggled", (GCallback)on_button_toggled, (gpointer)self);

#ifdef ENABLE_NETSERVICES
    {
        GtkTreeModel      *model;

        nwam_compose_tree_view (self);

        /* Svc tables */
		model = GTK_TREE_MODEL(gtk_list_store_new(1, G_TYPE_STRING));
        gtk_tree_view_set_model(prv->enabled_netservices_list, model);
		g_object_unref(model);

		model = GTK_TREE_MODEL(gtk_list_store_new(1, G_TYPE_STRING));
        gtk_tree_view_set_model(prv->disabled_netservices_list, model);
		g_object_unref(model);


        g_signal_connect(prv->add_enabled_netservice_btn,
          "clicked", G_CALLBACK(svc_button_clicked), (gpointer)self);
        g_signal_connect(prv->delete_enabled_netservice_btn,
          "clicked", G_CALLBACK(svc_button_clicked), (gpointer)self);
        g_signal_connect(prv->add_disabled_netservice_btn,
          "clicked", G_CALLBACK(svc_button_clicked), (gpointer)self);
        g_signal_connect(prv->delete_disabled_netservice_btn,
          "clicked", G_CALLBACK(svc_button_clicked), (gpointer)self);
    }
#endif /* ENABLE_NETSERVICES */
}

static void
nwam_env_pref_dialog_finalize (NwamEnvPrefDialog *self)
{
#ifdef ENABLE_PROXY
    if ( self->prv->proxy_password_dialog != NULL ) {
        g_object_unref(G_OBJECT(self->prv->proxy_password_dialog));
    }
#endif /* ENABLE_PROXY */

    g_object_unref(G_OBJECT(self->prv->env_pref_dialog ));

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
static gint
dialog_run(NwamPrefIFace *iface, GtkWindow *parent)
{
	NwamEnvPrefDialogPrivate *prv = GET_PRIVATE(iface);
    NwamEnvPrefDialog*          self = NWAM_ENV_PREF_DIALOG(iface);
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
    
        response =  gtk_dialog_run(GTK_DIALOG(self->prv->env_pref_dialog));

    }
    return(response);
    
    
}

static GtkWindow* 
dialog_get_window(NwamPrefIFace *iface)
{
    NwamEnvPrefDialog*          self = NWAM_ENV_PREF_DIALOG(iface);

    if ( self->prv->env_pref_dialog ) {
        return ( GTK_WINDOW(self->prv->env_pref_dialog) );
    }

    return(NULL);
}

static void
block_nameservices_signals( NwamEnvPrefDialog* self, gboolean block )
{
    NwamEnvPrefDialogPrivate*   prv = self?self->prv:NULL;

    if ( self == NULL ) {
        return;
    }

    if ( block ) {
        g_signal_handlers_block_by_func(prv->files_service_cb, (gpointer)on_ns_selection_toggled, (gpointer)self);

        g_signal_handlers_block_by_func(prv->dns_service_cb, (gpointer)on_ns_selection_toggled, (gpointer)self);
        g_signal_handlers_block_by_func(prv->dns_config_manual_rb, (gpointer)on_ns_source_toggled, (gpointer)self);
        g_signal_handlers_block_by_func(prv->dns_config_dhcp_rb, (gpointer)on_ns_source_toggled, (gpointer)self);
        g_signal_handlers_block_by_func(prv->dns_servers_entry, (gpointer)on_ns_text_changed, (gpointer)self);
        g_signal_handlers_block_by_func(prv->dns_search_entry, (gpointer)on_ns_text_changed, (gpointer)self);
        g_signal_handlers_block_by_func(prv->dns_domain_entry, (gpointer)on_ns_text_changed, (gpointer)self);

        g_signal_handlers_block_by_func(prv->nis_service_cb, (gpointer)on_ns_selection_toggled, (gpointer)self);
        g_signal_handlers_block_by_func(prv->nis_config_manual_rb, (gpointer)on_ns_source_toggled, (gpointer)self);
        g_signal_handlers_block_by_func(prv->nis_config_dhcp_rb, (gpointer)on_ns_source_toggled, (gpointer)self);
        g_signal_handlers_block_by_func(prv->nis_servers_entry, (gpointer)on_ns_text_changed, (gpointer)self);

        g_signal_handlers_block_by_func(prv->ldap_service_cb, (gpointer)on_ns_selection_toggled, (gpointer)self);
        g_signal_handlers_block_by_func(prv->ldap_servers_entry, (gpointer)on_ns_text_changed, (gpointer)self);

        g_signal_handlers_block_by_func(prv->default_domain_entry, (gpointer)on_ns_text_changed, (gpointer)self);
    }
    else {
        g_signal_handlers_unblock_by_func(prv->files_service_cb, (gpointer)on_ns_selection_toggled, (gpointer)self);

        g_signal_handlers_unblock_by_func(prv->dns_service_cb, (gpointer)on_ns_selection_toggled, (gpointer)self);
        g_signal_handlers_unblock_by_func(prv->dns_config_manual_rb, (gpointer)on_ns_source_toggled, (gpointer)self);
        g_signal_handlers_unblock_by_func(prv->dns_config_dhcp_rb, (gpointer)on_ns_source_toggled, (gpointer)self);
        g_signal_handlers_unblock_by_func(prv->dns_servers_entry, (gpointer)on_ns_text_changed, (gpointer)self);
        g_signal_handlers_unblock_by_func(prv->dns_search_entry, (gpointer)on_ns_text_changed, (gpointer)self);
        g_signal_handlers_unblock_by_func(prv->dns_domain_entry, (gpointer)on_ns_text_changed, (gpointer)self);

        g_signal_handlers_unblock_by_func(prv->nis_service_cb, (gpointer)on_ns_selection_toggled, (gpointer)self);
        g_signal_handlers_unblock_by_func(prv->nis_config_manual_rb, (gpointer)on_ns_source_toggled, (gpointer)self);
        g_signal_handlers_unblock_by_func(prv->nis_config_dhcp_rb, (gpointer)on_ns_source_toggled, (gpointer)self);
        g_signal_handlers_unblock_by_func(prv->nis_servers_entry, (gpointer)on_ns_text_changed, (gpointer)self);

        g_signal_handlers_unblock_by_func(prv->ldap_service_cb, (gpointer)on_ns_selection_toggled, (gpointer)self);
        g_signal_handlers_unblock_by_func(prv->ldap_servers_entry, (gpointer)on_ns_text_changed, (gpointer)self);

        g_signal_handlers_unblock_by_func(prv->default_domain_entry, (gpointer)on_ns_text_changed, (gpointer)self);
    }
}

#ifdef ENABLE_NETSERVICES
static void
nwam_compose_tree_view (NwamEnvPrefDialog *self)
{
	GtkTreeViewColumn *col;
	GtkCellRenderer *cell;
	GtkTreeView *view;

    /*
     * compose the enabled netservices view
     */
    view = self->prv->enabled_netservices_list;

	g_object_set (G_OBJECT(view),
		      "headers-clickable", FALSE,
		      "reorderable", TRUE,
		      NULL);

	/* column name */
	col = capplet_column_new(view,
      "title", NULL,
      "expand", TRUE,
      "resizable", FALSE,
      "clickable", FALSE,
      "sort-indicator", FALSE,
      "reorderable", FALSE,
      NULL);

    /* First cell */
	cell = capplet_column_append_cell(col,
      gtk_cell_renderer_text_new(), TRUE,
      default_svc_status_cb, (gpointer) self, NULL);

    g_object_set_data (G_OBJECT (cell), TREEVIEW_COLUMN_NUM,
                       GINT_TO_POINTER (SVC_INFO));

        
    /*
     * compose view for disabled netservices view
     */
    view = self->prv->disabled_netservices_list;

	g_object_set (G_OBJECT(view),
		      "headers-clickable", FALSE,
		      "reorderable", TRUE,
		      NULL);
	
	/* column name */
	col = capplet_column_new(view,
      "title", NULL,
      "expand", TRUE,
      "resizable", FALSE,
      "clickable", FALSE,
      "sort-indicator", FALSE,
      "reorderable", FALSE,
      NULL);

    /* First cell */
	cell = capplet_column_append_cell(col,
      gtk_cell_renderer_text_new(), TRUE,
      default_svc_status_cb, (gpointer) self, NULL);

    g_object_set_data (G_OBJECT (cell), TREEVIEW_COLUMN_NUM,
                       GINT_TO_POINTER (SVC_INFO));

}
#endif /* ENABLE_NETSERVICES */

static void
populate_panels_from_env( NwamEnvPrefDialog* self, NwamuiEnv* current_env)
{
    NwamEnvPrefDialogPrivate*   prv;
    gchar*                      env_name;
    GList*                      nameservices = NULL;
    gchar*                      default_domain = NULL;
    gboolean                    files = FALSE;
    gboolean                    dns = FALSE;
    gboolean                    nis = FALSE;
    gboolean                    ldap = FALSE;
    guint                       num_nameservices = 0;
    gchar                      *config_file;


#ifdef ENABLE_PROXY
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
    nwamui_env_proxy_type_t proxy_type;
#endif /* ENABLE_PROXY */
    
    g_debug("populate_panels_from_env called");
    
    g_assert( current_env != NULL );
    
    prv = self->prv;
    
    gtk_container_foreach(GTK_CONTAINER(self->prv->environment_notebook), (GtkCallback)gtk_widget_set_sensitive, (gpointer)TRUE);
    
    /*
     * Proxy Tab
     */
#ifdef ENABLE_PROXY
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
#endif /* ENABLE_PROXY */

    /*
     * Name Services Tab
     */
        
    block_nameservices_signals( self, TRUE );

    nameservices = nwamui_env_get_nameservices(current_env);

    for (GList* elem = g_list_first(nameservices);
         elem != NULL;
         elem = g_list_next(elem) ) {
        nwamui_env_nameservices_t service = (nwamui_env_nameservices_t)elem->data;

        switch( service ) {
            case NWAMUI_ENV_NAMESERVICES_DNS:
                dns = TRUE;
                break;
            case NWAMUI_ENV_NAMESERVICES_FILES:
                files = TRUE;
                break;
            case NWAMUI_ENV_NAMESERVICES_NIS:
                nis = TRUE;
                break;
            case NWAMUI_ENV_NAMESERVICES_LDAP:
                ldap = TRUE;
                break;
        }

        num_nameservices++;
    }

    prv->num_nameservices = num_nameservices;

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prv->files_service_cb), files );

    /* If active, then set senstivity based on the number of services,
     * otherwise it's always sensitive.
     */
    gtk_widget_set_sensitive(GTK_WIDGET(prv->files_service_cb), 
                            ((files)?(num_nameservices > 1):TRUE) );

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prv->dns_service_cb), dns );
    /* If active, then set senstivity based on the number of services,
     * otherwise it's always sensitive.
     */
    gtk_widget_set_sensitive(GTK_WIDGET(prv->dns_service_cb), 
                            ((dns)?(num_nameservices > 1):TRUE) );

    /* Pre-fill entries, and select text */
    gtk_entry_set_text(prv->dns_search_entry, _("(optional)") );
    gtk_entry_select_region(prv->dns_search_entry, 0, -1 );

    gtk_entry_set_text(prv->dns_servers_entry, _("(required)") );
    gtk_entry_select_region(prv->dns_servers_entry, 0, -1 );

    gtk_entry_set_text(prv->nis_servers_entry, _("(optional)") );
    gtk_entry_select_region(prv->nis_servers_entry, 0, -1 );

    gtk_entry_set_text(prv->ldap_servers_entry, _("(required)") );
    gtk_entry_select_region(prv->ldap_servers_entry, 0, -1 );

    if ( dns ) {
        gchar*                      dns_domain = NULL;
        GList*                      dns_servers = NULL;
        GList*                      dns_search = NULL;
        nwamui_env_config_source_t  dns_configsrc;
        gboolean                    sensitive;

        dns_configsrc = nwamui_env_get_dns_nameservice_config_source( current_env );
        dns_domain = nwamui_env_get_dns_nameservice_domain( current_env );
        dns_servers = nwamui_env_get_dns_nameservice_servers( current_env );
        dns_search = nwamui_env_get_dns_nameservice_search( current_env );

        if ( dns_configsrc == NWAMUI_ENV_CONFIG_SOURCE_MANUAL ) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prv->dns_config_manual_rb), TRUE );
            gtk_widget_set_sensitive(GTK_WIDGET(prv->dns_domain_entry), sensitive );
            gtk_widget_set_sensitive(GTK_WIDGET(prv->dns_domain_entry_label), sensitive );
            gtk_widget_set_sensitive(GTK_WIDGET(prv->dns_search_entry), sensitive );
            gtk_widget_set_sensitive(GTK_WIDGET(prv->dns_search_entry_label), sensitive );
            gtk_widget_set_sensitive(GTK_WIDGET(prv->dns_servers_entry), sensitive );
            gtk_widget_set_sensitive(GTK_WIDGET(prv->dns_servers_entry_label), sensitive );
            sensitive = TRUE;
        }
        else {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prv->dns_config_dhcp_rb), TRUE );
            sensitive = FALSE;
        }
        gtk_widget_set_sensitive(GTK_WIDGET(prv->dns_domain_entry), sensitive );
        gtk_widget_set_sensitive(GTK_WIDGET(prv->dns_domain_entry_label), sensitive );
        gtk_widget_set_sensitive(GTK_WIDGET(prv->dns_search_entry), sensitive );
        gtk_widget_set_sensitive(GTK_WIDGET(prv->dns_search_entry_label), sensitive );
        gtk_widget_set_sensitive(GTK_WIDGET(prv->dns_servers_entry), sensitive );
        gtk_widget_set_sensitive(GTK_WIDGET(prv->dns_servers_entry_label), sensitive );

        if ( dns_servers != NULL ) {
            gchar* str = nwamui_util_glist_to_comma_string( dns_servers );
            gtk_entry_set_text(prv->dns_servers_entry, str?str:"" );
            g_free(str);
        }

        if ( dns_search != NULL ) {
            gchar* str = nwamui_util_glist_to_comma_string( dns_search );
            gtk_entry_set_text(prv->dns_search_entry, str?str:"" );
            g_free(str);
        }

        gtk_entry_set_text(prv->dns_domain_entry, dns_domain?dns_domain:"" );

        g_free(dns_domain);
        g_list_foreach( dns_servers, (GFunc)g_free, NULL );
        g_list_free(dns_servers);
        g_list_foreach( dns_search, (GFunc)g_free, NULL );
        g_list_free(dns_search);
    }

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prv->nis_service_cb), nis );
    /* If active, then set senstivity based on the number of services,
     * otherwise it's always sensitive.
     */
    gtk_widget_set_sensitive(GTK_WIDGET(prv->nis_service_cb), 
                            ((nis)?(num_nameservices > 1):TRUE) );

    if ( nis ) {
        GList*                      nis_servers = NULL;
        nwamui_env_config_source_t  nis_configsrc;
        gboolean                    sensitive;

        nis_configsrc = nwamui_env_get_nis_nameservice_config_source( current_env );
        nis_servers = nwamui_env_get_nis_nameservice_servers( current_env );

        if ( nis_configsrc == NWAMUI_ENV_CONFIG_SOURCE_MANUAL ) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prv->nis_config_manual_rb), TRUE );
            sensitive = TRUE;
        }
        else {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prv->nis_config_dhcp_rb), TRUE );
            sensitive = FALSE;
        }
        gtk_widget_set_sensitive(GTK_WIDGET(prv->nis_domain_label), sensitive );
        gtk_widget_set_sensitive(GTK_WIDGET(prv->nis_domain_label_label), sensitive );
        gtk_widget_set_sensitive(GTK_WIDGET(prv->nis_servers_entry), sensitive );
        gtk_widget_set_sensitive(GTK_WIDGET(prv->nis_servers_entry_label), sensitive );

        if ( nis_servers != NULL ) {
            gchar* str = nwamui_util_glist_to_comma_string( nis_servers );
            gtk_entry_set_text(prv->nis_servers_entry, str?str:"" );
            g_free(str);
        }

        g_list_foreach( nis_servers, (GFunc)g_free, NULL );
        g_list_free(nis_servers);
    }

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prv->ldap_service_cb), ldap );
    /* If active, then set senstivity based on the number of services,
     * otherwise it's always sensitive.
     */
    gtk_widget_set_sensitive(GTK_WIDGET(prv->ldap_service_cb), 
                            ((ldap)?(num_nameservices > 1):TRUE) );

    if ( ldap ) {
        GList*                      ldap_servers = NULL;

        ldap_servers = nwamui_env_get_ldap_nameservice_servers( current_env );

        if ( ldap_servers != NULL ) {
            gchar* str = nwamui_util_glist_to_comma_string( ldap_servers );
            gtk_entry_set_text(prv->ldap_servers_entry, str?str:"" );
            g_free(str);
        }

        g_list_foreach( ldap_servers, (GFunc)g_free, NULL );
        g_list_free(ldap_servers);
    }

    default_domain = nwamui_env_get_default_domainname( current_env );

    if ( default_domain ) {
	    gtk_entry_set_text(prv->default_domain_entry, default_domain );
        g_free( default_domain );
    }

	gtk_widget_set_sensitive(GTK_WIDGET(prv->dns_config_manual_rb), dns );
	gtk_widget_set_sensitive(GTK_WIDGET(prv->dns_config_dhcp_rb), dns );
	gtk_widget_set_sensitive(GTK_WIDGET(prv->nis_config_manual_rb), nis );
	gtk_widget_set_sensitive(GTK_WIDGET(prv->nis_config_dhcp_rb), nis );
	gtk_widget_set_sensitive(GTK_WIDGET(prv->ldap_servers_entry), ldap );
	gtk_widget_set_sensitive(GTK_WIDGET(prv->ldap_servers_entry_label), ldap );
	gtk_widget_set_sensitive(GTK_WIDGET(prv->ldap_domain_label), ldap );
	gtk_widget_set_sensitive(GTK_WIDGET(prv->ldap_domain_label_label), ldap );
	gtk_widget_set_sensitive(GTK_WIDGET(prv->default_domain_entry_label), nis || ldap );
	gtk_widget_set_sensitive(GTK_WIDGET(prv->default_domain_entry), nis || ldap );
    
    config_file = nwamui_env_get_nameservices_config_file(current_env);
    if (config_file) {
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(prv->nsswitch_file_btn),
          config_file);
        g_free(config_file);
    }

    /* nsswitch default button is active only if exactly 1 NS selected */
	gtk_widget_set_sensitive(GTK_WIDGET(prv->nsswitch_default_btn), prv->num_nameservices == 1);

    block_nameservices_signals( self, FALSE );

    /*
     * Security Tab
     */
    {
        gchar *config_file;
        /* nat */
        config_file = nwamui_env_get_ipnat_config_file(current_env);
        if (config_file) {
            gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(prv->nat_file_chooser),
              config_file);
            g_free(config_file);
        }
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prv->nat_config_cb),
          config_file != NULL);
        gtk_toggle_button_toggled(GTK_TOGGLE_BUTTON(prv->nat_config_cb));
        /* ipfilter */
        config_file = nwamui_env_get_ipfilter_config_file(current_env);
        if (config_file) {
            gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(prv->ipfilter_file_chooser),
              config_file);
            g_free(config_file);
        }
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prv->ipf_config_cb),
          config_file != NULL);
        gtk_toggle_button_toggled(GTK_TOGGLE_BUTTON(prv->ipf_config_cb));
        /* ipfilter v6 */
        config_file = nwamui_env_get_ipfilter_v6_config_file(current_env);
        if (config_file) {
            gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(prv->ipfilter_v6_file_chooser),
              config_file);
            g_free(config_file);
        }
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prv->ipf_v6_config_cb),
          config_file != NULL);
        gtk_toggle_button_toggled(GTK_TOGGLE_BUTTON(prv->ipf_v6_config_cb));
        /* ippool */
        config_file = nwamui_env_get_ippool_config_file(current_env);
        if (config_file) {
            gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(prv->ippool_file_chooser),
              config_file);
            g_free(config_file);
        }
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prv->ippool_config_cb),
          config_file != NULL);
        gtk_toggle_button_toggled(GTK_TOGGLE_BUTTON(prv->ippool_config_cb));
        /* ike */
        config_file = nwamui_env_get_ike_config_file(current_env);
        if (config_file) {
            gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(prv->ike_file_chooser),
              config_file);
            g_free(config_file);
        }
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prv->ike_config_cb),
          config_file != NULL);
        gtk_toggle_button_toggled(GTK_TOGGLE_BUTTON(prv->ike_config_cb));
        /* ipsec policy */
        config_file = nwamui_env_get_ipsecpolicy_config_file(current_env);
        if (config_file) {
            gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(prv->ipsec_policy_file_chooser),
              config_file);
            g_free(config_file);
        }
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prv->ipsec_policy_cb),
          config_file != NULL);
        gtk_toggle_button_toggled(GTK_TOGGLE_BUTTON(prv->ipsec_policy_cb));
        /* ipsec key missing */

    }
    
    /*
     * SVC Tab
     */
#ifdef ENABLE_NETSERVICES
    {
        GtkTreeModel *model;
        GList *obj_list;

        /* Update enabled svcs */
        model = gtk_tree_view_get_model(prv->enabled_netservices_list);
        obj_list = nwamui_env_get_svcs_enable(current_env);
        gtk_widget_hide(GTK_WIDGET(prv->enabled_netservices_list));
        gtk_list_store_clear(GTK_LIST_STORE(model));
        g_list_foreach(obj_list, svc_list_merge_to_model, (gpointer)model);
        gtk_widget_show(GTK_WIDGET(prv->enabled_netservices_list));
        g_list_free(obj_list);

        /* Update diabled svcs */
        model = gtk_tree_view_get_model(prv->disabled_netservices_list);
        obj_list = nwamui_env_get_svcs_disable(current_env);
        gtk_widget_hide(GTK_WIDGET(prv->disabled_netservices_list));
        gtk_list_store_clear(GTK_LIST_STORE(model));
        g_list_foreach(obj_list, svc_list_merge_to_model, (gpointer)model);
        gtk_widget_show(GTK_WIDGET(prv->disabled_netservices_list));
        g_list_free(obj_list);
    }
#endif /* ENABLE_NETSERVICES */
}

#ifdef ENABLE_PROXY
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
#endif /* ENABLE_PROXY */


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

#ifdef ENABLE_PROXY
static void
proxy_config_changed_cb( GtkWidget* widget, gpointer data )
{
	NwamEnvPrefDialogPrivate *prv = GET_PRIVATE(data);
    NwamEnvPrefDialog*          self = NWAM_ENV_PREF_DIALOG(data);
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

    if ( proxy_type != nwamui_env_get_proxy_type( prv->selected_env ) ) {
        nwamui_env_set_proxy_type( prv->selected_env, proxy_type );
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
#endif /* ENABLE_PROXY */


static void
on_ns_source_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
    NwamEnvPrefDialog          *self = NWAM_ENV_PREF_DIALOG(user_data);
	NwamEnvPrefDialogPrivate   *prv = GET_PRIVATE(user_data);
    gboolean                    active = gtk_toggle_button_get_active(togglebutton);
    gboolean                    sensitive = FALSE;
    nwamui_env_config_source_t  config_source;

	if ( togglebutton == (GtkToggleButton*)prv->dns_config_manual_rb ||
	     togglebutton == (GtkToggleButton*)prv->dns_config_dhcp_rb ) {
        /* Only act upon the active selection */
        if (active) {
            if ( togglebutton == (GtkToggleButton*)prv->dns_config_manual_rb ) {
                config_source = NWAMUI_ENV_CONFIG_SOURCE_MANUAL;
            }
            else { /* togglebutton == prv->dns_config_dhcp_rb */
                config_source = NWAMUI_ENV_CONFIG_SOURCE_DHCP;
            }
            nwamui_env_set_dns_nameservice_config_source (  prv->selected_env, config_source );
        }
        sensitive = (active && config_source == NWAMUI_ENV_CONFIG_SOURCE_MANUAL);

        gtk_widget_set_sensitive(GTK_WIDGET(prv->dns_domain_entry), sensitive );
        gtk_widget_set_sensitive(GTK_WIDGET(prv->dns_domain_entry_label), sensitive );
        gtk_widget_set_sensitive(GTK_WIDGET(prv->dns_search_entry), sensitive );
        gtk_widget_set_sensitive(GTK_WIDGET(prv->dns_search_entry_label), sensitive );
        gtk_widget_set_sensitive(GTK_WIDGET(prv->dns_servers_entry), sensitive );
        gtk_widget_set_sensitive(GTK_WIDGET(prv->dns_servers_entry_label), sensitive );
    }
    else if ( togglebutton == (GtkToggleButton*)prv->nis_config_manual_rb ||
	          togglebutton == (GtkToggleButton*)prv->nis_config_dhcp_rb ) {
        /* Only act upon the active selection */
        if (active) {
            if ( togglebutton == (GtkToggleButton*)prv->nis_config_manual_rb ) {
                config_source = NWAMUI_ENV_CONFIG_SOURCE_MANUAL;
            }
            else { /* togglebutton == prv->nis_config_dhcp_rb */
                config_source = NWAMUI_ENV_CONFIG_SOURCE_DHCP;
            }
            nwamui_env_set_nis_nameservice_config_source (  prv->selected_env, config_source );
        }
        sensitive = (active && config_source == NWAMUI_ENV_CONFIG_SOURCE_MANUAL);

        gtk_widget_set_sensitive(GTK_WIDGET(prv->nis_domain_label), sensitive );
        gtk_widget_set_sensitive(GTK_WIDGET(prv->nis_domain_label_label), sensitive );
        gtk_widget_set_sensitive(GTK_WIDGET(prv->nis_servers_entry), sensitive );
        gtk_widget_set_sensitive(GTK_WIDGET(prv->nis_servers_entry_label), sensitive );
    }

}

static void
on_ns_selection_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
    NwamEnvPrefDialog          *self = NWAM_ENV_PREF_DIALOG(user_data);
	NwamEnvPrefDialogPrivate   *prv = GET_PRIVATE(user_data);
    gboolean                    active = gtk_toggle_button_get_active(togglebutton);
    GList                      *nameservices = nwamui_env_get_nameservices( prv->selected_env );
    gboolean                    files = FALSE; /* Which service is being actioned */
    gboolean                    dns = FALSE;
    gboolean                    nis = FALSE;
    gboolean                    ldap = FALSE;
    gboolean                    files_active = FALSE; /* Which services are active now */
    gboolean                    dns_active = FALSE;
    gboolean                    nis_active = FALSE;
    gboolean                    ldap_active = FALSE;
    gboolean                    entry_sensitive = FALSE;
    guint                       num_nameservices = 0;
    nwamui_env_nameservices_t   new_service = NWAMUI_ENV_NAMESERVICES_LAST;
    nwamui_env_config_source_t  dns_configsrc;
    nwamui_env_config_source_t  nis_configsrc;

	if ( togglebutton == (GtkToggleButton*)prv->files_service_cb ) {
        if ( active ) {
            new_service = NWAMUI_ENV_NAMESERVICES_FILES;
        }
        files = TRUE;
    }
    else if ( togglebutton == (GtkToggleButton*)prv->dns_service_cb ) {
        if ( active ) {
            new_service = NWAMUI_ENV_NAMESERVICES_DNS;
        }
        dns = TRUE;
    }
    else if ( togglebutton == (GtkToggleButton*)prv->nis_service_cb ) {
        if ( active ) {
            new_service = NWAMUI_ENV_NAMESERVICES_NIS;
        }
        nis = TRUE;
    }
    else if ( togglebutton == (GtkToggleButton*)prv->ldap_service_cb ) {
        if ( active ) {
            new_service = NWAMUI_ENV_NAMESERVICES_LDAP;
        }
        ldap = TRUE;
    }

    for (GList* elem = g_list_first(nameservices);
         elem != NULL; /* no increment here */) {
        nwamui_env_nameservices_t service = (nwamui_env_nameservices_t)elem->data;
        gboolean                  remove_element = FALSE;

        num_nameservices++;

        switch( service ) {
            case NWAMUI_ENV_NAMESERVICES_DNS:
                if ( dns && !active ) {
                    remove_element = TRUE;
                }
                break;
            case NWAMUI_ENV_NAMESERVICES_FILES:
                if ( files && !active ) {
                    remove_element = TRUE;
                }
                files = TRUE;
                break;
            case NWAMUI_ENV_NAMESERVICES_NIS:
                if ( nis && !active ) {
                    remove_element = TRUE;
                }
                nis = TRUE;
                break;
            case NWAMUI_ENV_NAMESERVICES_LDAP:
                if ( ldap && !active ) {
                    remove_element = TRUE;
                }
                break;
        }

        if ( active && service == new_service ) {
            /* Shouldn't really happen that we're adding a nameservice that's
             * already in the list, but just in case, let's not add it, if it
             * is already there.
             */
            new_service = NWAMUI_ENV_NAMESERVICES_LAST;
        }

        if ( remove_element ) {
            GList *to_remove = elem;

            /* Bump to next now */
            elem = g_list_next( elem );

            nameservices = g_list_delete_link(nameservices, to_remove);
            num_nameservices--;
        }
        else {
            elem = g_list_next(elem);
        }
    }

    dns_configsrc = nwamui_env_get_dns_nameservice_config_source( prv->selected_env );
    nis_configsrc = nwamui_env_get_nis_nameservice_config_source( prv->selected_env );

    if ( new_service != NWAMUI_ENV_NAMESERVICES_LAST ) {
        g_list_append( nameservices, (gpointer)new_service );

        /* If a new service is being added, ensure there is a sensibly initial
         * value for the config souce.
         */
        if ( dns && active ) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prv->dns_config_dhcp_rb), TRUE );
            gtk_toggle_button_toggled(GTK_TOGGLE_BUTTON(prv->dns_config_dhcp_rb));
        }
        else if ( nis && active ) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prv->nis_config_dhcp_rb), TRUE );
            gtk_toggle_button_toggled(GTK_TOGGLE_BUTTON(prv->nis_config_dhcp_rb));
        }

        num_nameservices++;
    }
    
    /* If active, then set senstivity based on the number of services,
     * otherwise it's always sensitive.
     */
    files_active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prv->files_service_cb));
    gtk_widget_set_sensitive(GTK_WIDGET(prv->files_service_cb), 
                            (files_active?(num_nameservices > 1):TRUE) );
    dns_active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prv->dns_service_cb));
    gtk_widget_set_sensitive(GTK_WIDGET(prv->dns_service_cb), 
                            (dns_active?(num_nameservices > 1):TRUE) );

    nis_active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prv->nis_service_cb));
    gtk_widget_set_sensitive(GTK_WIDGET(prv->nis_service_cb), 
                            (nis_active?(num_nameservices > 1):TRUE) );
    ldap_active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prv->ldap_service_cb));
    gtk_widget_set_sensitive(GTK_WIDGET(prv->ldap_service_cb), 
                            (ldap_active?(num_nameservices > 1):TRUE) );
    
    nwamui_env_set_nameservices( prv->selected_env, nameservices );

    g_list_free( nameservices );

    prv->num_nameservices = num_nameservices;

    /* DNS */
    entry_sensitive = (dns_active && dns_configsrc == NWAMUI_ENV_CONFIG_SOURCE_MANUAL);
    gtk_widget_set_sensitive(GTK_WIDGET(prv->dns_config_manual_rb), dns_active );
    gtk_widget_set_sensitive(GTK_WIDGET(prv->dns_config_dhcp_rb), dns_active );
    gtk_widget_set_sensitive(GTK_WIDGET(prv->dns_domain_entry), entry_sensitive );
    gtk_widget_set_sensitive(GTK_WIDGET(prv->dns_domain_entry_label), entry_sensitive );
    gtk_widget_set_sensitive(GTK_WIDGET(prv->dns_search_entry), entry_sensitive );
    gtk_widget_set_sensitive(GTK_WIDGET(prv->dns_search_entry_label), entry_sensitive );
    gtk_widget_set_sensitive(GTK_WIDGET(prv->dns_servers_entry), entry_sensitive );
    gtk_widget_set_sensitive(GTK_WIDGET(prv->dns_servers_entry_label), entry_sensitive );

    /*NIS */
    entry_sensitive = (nis_active && nis_configsrc == NWAMUI_ENV_CONFIG_SOURCE_MANUAL);
    gtk_widget_set_sensitive(GTK_WIDGET(prv->nis_config_manual_rb), nis_active );
    gtk_widget_set_sensitive(GTK_WIDGET(prv->nis_config_dhcp_rb), nis_active );
    gtk_widget_set_sensitive(GTK_WIDGET(prv->nis_servers_entry), entry_sensitive );
    gtk_widget_set_sensitive(GTK_WIDGET(prv->nis_servers_entry_label), entry_sensitive );
    gtk_widget_set_sensitive(GTK_WIDGET(prv->nis_domain_label), entry_sensitive );
    gtk_widget_set_sensitive(GTK_WIDGET(prv->nis_domain_label_label), entry_sensitive );

    /* LDAP */
    gtk_widget_set_sensitive(GTK_WIDGET(prv->ldap_servers_entry), ldap_active );
    gtk_widget_set_sensitive(GTK_WIDGET(prv->ldap_servers_entry_label), ldap_active );
    gtk_widget_set_sensitive(GTK_WIDGET(prv->ldap_domain_label), ldap_active );
    gtk_widget_set_sensitive(GTK_WIDGET(prv->ldap_domain_label_label), ldap_active );

	gtk_widget_set_sensitive(GTK_WIDGET(prv->default_domain_entry_label), (nis_active || ldap_active) );
	gtk_widget_set_sensitive(GTK_WIDGET(prv->default_domain_entry), (nis_active || ldap_active) );

    /* nsswitch default button is active only if exactly 1 NS selected */
	gtk_widget_set_sensitive(GTK_WIDGET(prv->nsswitch_default_btn), prv->num_nameservices == 1);
}

static void
on_ns_text_changed(GtkEntry *entry, gpointer  user_data)
{
    NwamEnvPrefDialog          *self = NWAM_ENV_PREF_DIALOG(user_data);
	NwamEnvPrefDialogPrivate   *prv = GET_PRIVATE(user_data);
    const gchar                *text = gtk_entry_get_text(entry);
    
    if ( entry == prv->dns_servers_entry ) {
        GList *servers = nwamui_util_parse_string_to_glist( text );
        nwamui_env_set_dns_nameservice_servers( prv->selected_env, servers );
        g_list_foreach( servers, (GFunc)g_free, NULL );
        g_list_free( servers );
    }
    else if ( entry == prv->dns_search_entry ) {
        GList *domains = nwamui_util_parse_string_to_glist( text );
        nwamui_env_set_dns_nameservice_search( prv->selected_env, domains );
        g_list_foreach( domains, (GFunc)g_free, NULL );
        g_list_free( domains );
    }
    else if ( entry == prv->dns_domain_entry ) {
        nwamui_env_set_dns_nameservice_domain( prv->selected_env, text );
    }
    else if ( entry == prv->nis_servers_entry ) {
        GList *servers = nwamui_util_parse_string_to_glist( text );
        nwamui_env_set_nis_nameservice_servers( prv->selected_env, servers );
        g_list_foreach( servers, (GFunc)g_free, NULL );
        g_list_free( servers );
    }
    else if ( entry == prv->ldap_servers_entry ) {
        GList *servers = nwamui_util_parse_string_to_glist( text );
        nwamui_env_set_ldap_nameservice_servers( prv->selected_env, servers );
        g_list_foreach( servers, (GFunc)g_free, NULL );
        g_list_free( servers );
    }
    else if ( entry == prv->default_domain_entry ) {
        nwamui_env_set_default_domainname( prv->selected_env, text );
    }
}

static void
nsswitch_default_clicked_cb(GtkButton *button, gpointer  user_data)
{
    NwamEnvPrefDialog          *self = NWAM_ENV_PREF_DIALOG(user_data);
	NwamEnvPrefDialogPrivate   *prv = GET_PRIVATE(user_data);
    GList                      *nameservices;
    guint                       num_nameservices = 0;
    const gchar*                nss_file = NULL;

    /* Assume if we got this far, then the button is active and we have only 1
     * nameservice selected.
     */
    nameservices = nwamui_env_get_nameservices( prv->selected_env );

    for (GList* elem = g_list_first(nameservices);
         elem != NULL;
         elem = g_list_next(elem) ) {
        nwamui_env_nameservices_t service = (nwamui_env_nameservices_t)elem->data;

        switch( service ) {
            case NWAMUI_ENV_NAMESERVICES_DNS:
                nss_file = NSSWITCH_FILE_DNS;
                break;
            case NWAMUI_ENV_NAMESERVICES_FILES:
                nss_file = NSSWITCH_FILE_FILES;
                break;
            case NWAMUI_ENV_NAMESERVICES_NIS:
                nss_file = NSSWITCH_FILE_NIS;
                break;
            case NWAMUI_ENV_NAMESERVICES_LDAP:
                nss_file = NSSWITCH_FILE_LDAP;
                break;
        }

        num_nameservices++;
    }

    if ( num_nameservices > 1 ) {
        nwamui_warning( "Unexpectedly got %d nameservces when expected 1", num_nameservices );
    }

    if (nss_file != NULL) {
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(prv->nsswitch_file_btn), nss_file);
    }
}

#ifdef ENABLE_NETSERVICES
static void
svc_button_clicked(GtkButton *button, gpointer  user_data)
{
	NwamEnvPrefDialogPrivate *prv = GET_PRIVATE(user_data);
    NwamEnvPrefDialog *self = NWAM_ENV_PREF_DIALOG(user_data);
    GtkTreeView *treeview;
    GtkTreeModel *model;
    gboolean is_adding = TRUE;

    if (button == (gpointer)prv->add_enabled_netservice_btn ||
      button == (gpointer)prv->delete_enabled_netservice_btn) {

        treeview = prv->enabled_netservices_list;
        is_adding = (button == (gpointer)prv->add_enabled_netservice_btn);

    } else if (button == (gpointer)prv->add_disabled_netservice_btn ||
      button == (gpointer)prv->delete_disabled_netservice_btn) {

        treeview = prv->disabled_netservices_list;
        is_adding = (button == (gpointer)prv->add_disabled_netservice_btn);

    } else {
        g_assert_not_reached();
    }

    model = gtk_tree_view_get_model(treeview);

    if (is_adding) {
        GtkWindow *parent = NULL;
        GtkWidget *w = GTK_WIDGET(button);

        if (w) {
            w = gtk_widget_get_toplevel(w);

            if (GTK_WIDGET_TOPLEVEL (w)) {
                parent = GTK_WINDOW(w);
            }
        }
        gtk_window_set_modal (GTK_WINDOW(prv->enter_service_fmri_dialog), TRUE);
        gtk_window_set_transient_for (GTK_WINDOW(prv->enter_service_fmri_dialog), parent);

        gtk_entry_set_text(prv->smf_fmri_entry, "" );

        if (GTK_RESPONSE_OK == gtk_dialog_run(GTK_DIALOG(prv->enter_service_fmri_dialog))) {
            GtkTreeIter iter;
            const gchar *svc;

            svc = gtk_entry_get_text(prv->smf_fmri_entry);

            gtk_list_store_append(GTK_LIST_STORE(model), &iter);
            gtk_list_store_set(GTK_LIST_STORE(model), &iter, 0, svc, -1);
        }
    } else {
        GtkTreeIter iter;
        GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);

        if (gtk_tree_selection_get_selected (selection, NULL, &iter)) {
            gtk_list_store_remove (GTK_LIST_STORE(model), &iter);
        }
    }
}
#endif /* ENABLE_NETSERVICES */

static void
default_svc_status_cb (GtkTreeViewColumn *tree_column,
                       GtkCellRenderer *cell,
                       GtkTreeModel *tree_model,
                       GtkTreeIter *iter,
                       gpointer data)
{
	NwamEnvPrefDialogPrivate *prv = GET_PRIVATE(data);
    gchar *svc;
    GtkTreeIter piter;
    
    gtk_tree_model_get(tree_model, iter, 0, &svc, -1);

    if (!svc)
        return;

    switch (GPOINTER_TO_INT(g_object_get_data (G_OBJECT (cell), TREEVIEW_COLUMN_NUM))) {
    case SVC_INFO:
    {
        gchar *info;
        gchar *name;
        gchar *fmri;

        get_smf_service_name(svc, &name, NULL );

        fmri = g_strstr_len(svc, sizeof("svc:/"), "/");

        if ( name != NULL ) {
            info = g_strdup_printf(_("<b>%s</b>\n%s"), name, fmri ? fmri + 1 : svc);
        }
        else {
            info = g_strdup_printf(_("%s"), fmri ? fmri + 1 : svc);
        }

        g_object_set(G_OBJECT(cell), "markup", info, NULL);

        g_free(name);
        g_free(info);
    }
    break;
    default:
        g_assert_not_reached();
        break;
    }

    g_free(svc);
}

static void
nameservices_status_cb (GtkTreeViewColumn *tree_column,
  GtkCellRenderer *cell,
  GtkTreeModel *tree_model,
  GtkTreeIter *iter,
  gpointer data)
{
	NwamEnvPrefDialogPrivate *prv = GET_PRIVATE(data);
    nwam_nameservices_t ns;
    
    gtk_tree_model_get(tree_model, iter, 0, &ns, -1);

    switch (GPOINTER_TO_INT(g_object_get_data (G_OBJECT (cell), TREEVIEW_COLUMN_NUM))) {
    case NAMESERVICES_NAME:
    {
        switch (ns) {
        case NWAM_NAMESERVICES_DNS:
        case NWAM_NAMESERVICES_FILES:
        case NWAM_NAMESERVICES_NIS:
        case NWAM_NAMESERVICES_LDAP:
            g_object_set(G_OBJECT(cell), "markup",
              nwam_nameservices_enum_to_string(ns), NULL);
            break;
        default:
            g_object_set(G_OBJECT(cell), "markup", NULL, NULL);
            break;
        }
    }
    break;
    case DEFAULT_DOMAINNAME:
    {
        gchar *name = nwamui_env_get_default_domainname(prv->selected_env);

        g_object_set(G_OBJECT(cell), "markup", name, NULL);

        g_free(name);
    }
    break;
    case DNS_NAMESERVICE_DOMAIN:
    {
        gchar *name = nwamui_env_get_dns_nameservice_domain(prv->selected_env);

        g_object_set(G_OBJECT(cell), "markup", name, NULL);

        g_free(name);
    }
    break;
    case NAMESERVICES_ADDR:
    {
        GList *list = NULL;
        GString *name = NULL;

        switch (ns) {
        case NWAM_NAMESERVICES_DNS:
            list = nwamui_env_get_dns_nameservice_servers(prv->selected_env);
            break;
        case NWAM_NAMESERVICES_FILES:
            break;
        case NWAM_NAMESERVICES_NIS:
            list = nwamui_env_get_nis_nameservice_servers(prv->selected_env);
            break;
        case NWAM_NAMESERVICES_LDAP:
            list = nwamui_env_get_ldap_nameservice_servers(prv->selected_env);
            break;
        default:
            g_object_set(G_OBJECT(cell), "markup", NULL, NULL);
            return;
        }

        if (list) {
            GList *i;
            name = g_string_new("");
            for (i = list; i; i = i->next) {
                g_string_append_printf(name, ", %s", (gchar*)i->data);
            }

            g_object_set(G_OBJECT(cell), "markup", name->str + 2, NULL);
            g_string_free(name, TRUE);
        }
    }
    break;
    default:
        g_assert_not_reached();
        break;
    }
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
        nwam_pref_cancel (NWAM_PREF_IFACE(data), NULL);
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
refresh(NwamPrefIFace *iface, gpointer user_data, gboolean force)
{
	NwamEnvPrefDialogPrivate *prv = GET_PRIVATE(iface);
    NwamEnvPrefDialog* self = NWAM_ENV_PREF_DIALOG(iface);

	if (prv->selected_env != user_data) {
		if (prv->selected_env) {
			g_object_unref(prv->selected_env);
		}
		if ((prv->selected_env = user_data) != NULL) {
			g_object_ref(prv->selected_env);
		}
		force = TRUE;
	}

	if (force) {
		if (prv->selected_env) {
			gchar *title;
            gchar *name;

            name = nwamui_object_get_name(NWAMUI_OBJECT(prv->selected_env));
			title = g_strdup_printf(_("Location Properties : %s"), name);
			g_object_set(prv->env_pref_dialog,
			    "title", title,
			    NULL);
			g_free(name);
			g_free(title);

            /* Set title to include hostname */
            nwamui_util_window_title_append_hostname( prv->env_pref_dialog );

            populate_panels_from_env(self, prv->selected_env);
		}
	}
	return TRUE;
}

static gboolean
cancel(NwamPrefIFace *iface, gpointer user_data)
{
	NwamEnvPrefDialogPrivate *prv = GET_PRIVATE(iface);
    NwamEnvPrefDialog* self = NWAM_ENV_PREF_DIALOG(iface);
    NwamuiEnv *current_env;

    g_debug("NwamEnvPrefDialog cancel");

    if (!prv->selected_env) {
        return TRUE;
    }
    else {
        nwamui_object_reload( NWAMUI_OBJECT(prv->selected_env) );
    }

    return TRUE;
}
static gboolean
apply(NwamPrefIFace *iface, gpointer user_data)
{
	NwamEnvPrefDialogPrivate *prv = GET_PRIVATE(iface);
    NwamEnvPrefDialog* self = NWAM_ENV_PREF_DIALOG(iface);
    NwamuiEnv *current_env;
    gchar     *prop_name = NULL;

    g_debug("NwamEnvPrefDialog apply");

    if (!prv->selected_env)
        return TRUE;
    else
        current_env = prv->selected_env;
    
    /*
     * Name Services Tab
     */
    {
        gchar  *config_file;
        gint    num_services = 0;
        gboolean    check_default_domain = FALSE;

        /* DNS Servers */
        for ( GList* elem =  nwamui_env_get_nameservices( current_env );
                elem != NULL; elem = g_list_next(elem) ) {
            switch ( (nwamui_env_nameservices_t)elem->data ) {
                case NWAMUI_ENV_NAMESERVICES_DNS:
                    /* Required */
                    if ( !nwamui_util_validate_text_entry( GTK_WIDGET(prv->dns_servers_entry), gtk_entry_get_text(prv->dns_servers_entry),
                                                NWAMUI_ENTRY_VALIDATION_IS_V4|NWAMUI_ENTRY_VALIDATION_IS_V6|NWAMUI_ENTRY_VALIDATION_ALLOW_LIST,
                                                TRUE, TRUE ) ) {
                        gtk_widget_grab_focus( GTK_WIDGET(prv->dns_servers_entry) );

                        return( FALSE );
                    }
                    break;
                case NWAMUI_ENV_NAMESERVICES_NIS:
                    /* Optional */
                    if ( !nwamui_util_validate_text_entry( GTK_WIDGET(prv->nis_servers_entry), gtk_entry_get_text(prv->nis_servers_entry),
                                                NWAMUI_ENTRY_VALIDATION_IS_V4|NWAMUI_ENTRY_VALIDATION_IS_V6|NWAMUI_ENTRY_VALIDATION_ALLOW_LIST|NWAMUI_ENTRY_VALIDATION_ALLOW_EMPTY,
                                                TRUE, TRUE ) ) {
                        gtk_widget_grab_focus( GTK_WIDGET(prv->nis_servers_entry) );

                        return( FALSE );
                    }
                    check_default_domain = TRUE;
                    break;
                case NWAMUI_ENV_NAMESERVICES_LDAP:
                    /* Required */
                    if ( !nwamui_util_validate_text_entry( GTK_WIDGET(prv->ldap_servers_entry), gtk_entry_get_text(prv->ldap_servers_entry),
                                                NWAMUI_ENTRY_VALIDATION_IS_V4|NWAMUI_ENTRY_VALIDATION_IS_V6|NWAMUI_ENTRY_VALIDATION_ALLOW_LIST,
                                                TRUE, TRUE ) ) {
                        gtk_widget_grab_focus( GTK_WIDGET(prv->ldap_servers_entry) );

                        return( FALSE );
                    }
                    check_default_domain = TRUE;
                    break;
                case NWAMUI_ENV_NAMESERVICES_FILES:
                    break;
            }
            num_services++;
        }

        if ( check_default_domain ) {
            const gchar *text = gtk_entry_get_text(GTK_ENTRY(prv->default_domain_entry));
            gchar       *stripped = NULL;

            if ( text != NULL ) {
                stripped = g_strdup( text );
                g_strstrip( stripped );
            }

            if ( text == NULL || stripped == NULL || strlen(stripped) ==0 ) {
                nwamui_util_show_message(GTK_WINDOW(prv->env_pref_dialog),
                        GTK_MESSAGE_ERROR, _("Default Domain Required"),
                        _("With NIS or LDAP a default domain must be specified.\nPlease provide one in the appropriate field."),
                        TRUE);
                gtk_widget_grab_focus(GTK_WIDGET(prv->default_domain_entry));
                return( FALSE );
            }
            g_free(stripped);
        }

        /* NSSWITCH configuration file */
        config_file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(prv->nsswitch_file_btn));
        if (config_file) {
            nwamui_env_set_nameservices_config_file(current_env, config_file);
            g_free(config_file);
        }
        else if ( num_services > 1 ) {
            nwamui_util_show_message(GTK_WINDOW(prv->env_pref_dialog),
                    GTK_MESSAGE_ERROR, _("Nameservice Switch File Required"),
                    _("A nameservice switch file should be specified\nif more than one name service is selected."),
                    TRUE);
            return( FALSE );
        }



    }


    /*
     * Security Tab
     */
    {
        gchar *config_file;
        /* nat */
        if (GTK_WIDGET_SENSITIVE(GTK_WIDGET(prv->nat_file_chooser)) &&
          (config_file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(prv->nat_file_chooser)))) {
            nwamui_env_set_ipnat_config_file(current_env, config_file);
            g_free(config_file);
        } else {
            nwamui_env_set_ipnat_config_file(current_env, NULL);
        }

        if (GTK_WIDGET_SENSITIVE(GTK_WIDGET(prv->ipfilter_file_chooser)) &&
          (config_file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(prv->ipfilter_file_chooser)))) {
            nwamui_env_set_ipfilter_config_file(current_env, config_file);
            g_free(config_file);
        } else {
            nwamui_env_set_ipfilter_config_file(current_env, NULL);
        }

        if (GTK_WIDGET_SENSITIVE(GTK_WIDGET(prv->ipfilter_v6_file_chooser)) &&
          (config_file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(prv->ipfilter_v6_file_chooser)))) {
            nwamui_env_set_ipfilter_v6_config_file(current_env, config_file);
            g_free(config_file);
        } else {
            nwamui_env_set_ipfilter_v6_config_file(current_env, NULL);
        }

        if (GTK_WIDGET_SENSITIVE(GTK_WIDGET(prv->ippool_file_chooser)) &&
          (config_file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(prv->ippool_file_chooser)))) {
            nwamui_env_set_ippool_config_file(current_env, config_file);
            g_free(config_file);
        } else {
            nwamui_env_set_ippool_config_file(current_env, NULL);
        }

        if (GTK_WIDGET_SENSITIVE(GTK_WIDGET(prv->ike_file_chooser)) &&
          (config_file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(prv->ike_file_chooser)))) {
            nwamui_env_set_ike_config_file(current_env, config_file);
            g_free(config_file);
        } else {
            nwamui_env_set_ike_config_file(current_env, NULL);
        }

        if (GTK_WIDGET_SENSITIVE(GTK_WIDGET(prv->ipsec_policy_file_chooser)) &&
          (config_file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(prv->ipsec_policy_file_chooser)))) {
            nwamui_env_set_ipsecpolicy_config_file(current_env, config_file);
            g_free(config_file);
        } else {
            nwamui_env_set_ipsecpolicy_config_file(current_env, NULL);
        }
    }
    
    /*
     * SVC Tab
     */
#ifdef ENABLE_NETSERVICES
    {
        GtkTreeModel *model;
        GList *nlist;

        /* Update enabled svcs */
        model = gtk_tree_view_get_model(prv->enabled_netservices_list);
        nlist = capplet_model_to_list(model);
        nwamui_env_set_svcs_enable(current_env, nlist);
        g_list_foreach(nlist, (GFunc)g_free, NULL);
        g_list_free(nlist);

        /* Update disabled svcs */
        model = gtk_tree_view_get_model(prv->disabled_netservices_list);
        nlist = capplet_model_to_list(model);
        nwamui_env_set_svcs_disable(current_env, nlist);
        g_list_foreach(nlist, (GFunc)g_free, NULL);
        g_list_free(nlist);
    }
#endif /* ENABLE_NETSERVICES */

    if (nwamui_env_validate (NWAMUI_ENV (prv->selected_env), &prop_name)) {
        if (!nwamui_env_commit (NWAMUI_ENV (prv->selected_env))) {
            gchar *name = nwamui_env_get_name (NWAMUI_ENV (prv->selected_env));
            gchar *msg = g_strdup_printf (_("Committing %s failed..."), name);
            nwamui_util_show_message (GTK_WINDOW(prv->env_pref_dialog),
              GTK_MESSAGE_ERROR,
              _("Commit Location Error"),
              msg, TRUE);
            g_free (msg);
            g_free (name);
            return FALSE;
        }
    }
    else {
        gchar *name = nwamui_env_get_name (NWAMUI_ENV (prv->selected_env));
        gchar *msg = g_strdup_printf (_("Validation of %s failed with the property %s"), name, prop_name);

        nwamui_util_show_message (GTK_WINDOW(prv->env_pref_dialog),
          GTK_MESSAGE_ERROR,
          _("Validation error"),
          msg, TRUE);
        g_free (msg);
        g_free (name);
        g_free (prop_name);
        return( FALSE );
    }
    return TRUE;
}

static gboolean
help(NwamPrefIFace *iface, gpointer user_data)
{
	NwamEnvPrefDialogPrivate *prv = GET_PRIVATE(iface);
    g_debug("NwamEnvPrefDialog help");

    nwamui_util_show_help (HELP_REF_LOCATION_EDIT);
}

static void
on_button_toggled(GtkToggleButton *button, gpointer user_data)
{
	NwamEnvPrefDialogPrivate *prv = GET_PRIVATE(user_data);
    gboolean toggled;
    GtkFileChooserButton *current_chooser;
    gchar *filename;

    g_assert(GTK_IS_TOGGLE_BUTTON(button));
    toggled = gtk_toggle_button_get_active(button);
	if (button == (gpointer)prv->ippool_config_cb)
        current_chooser = prv->ippool_file_chooser;
/*         gtk_widget_set_sensitive (GTK_WIDGET(prv->ippool_file_chooser), toggled); */
    else if (button == (gpointer)prv->nat_config_cb)
        current_chooser = prv->nat_file_chooser;
/*         gtk_widget_set_sensitive (GTK_WIDGET(prv->nat_file_chooser), toggled); */
    else if (button == (gpointer)prv->ipf_v6_config_cb)
        current_chooser = prv->ipfilter_v6_file_chooser;
/*         gtk_widget_set_sensitive (GTK_WIDGET(prv->ipfilter_v6_file_chooser), toggled); */
    else if (button == (gpointer)prv->ipf_config_cb)
        current_chooser = prv->ipfilter_file_chooser;
/*         gtk_widget_set_sensitive (GTK_WIDGET(prv->ipfilter_file_chooser), toggled); */
    else if (button == (gpointer)prv->ike_config_cb)
        current_chooser = prv->ike_file_chooser;
/*         gtk_widget_set_sensitive (GTK_WIDGET(prv->ike_file_chooser), toggled); */
    else if (button == (gpointer)prv->ipsec_policy_cb)
        current_chooser = prv->ipsec_policy_file_chooser;
/*         gtk_widget_set_sensitive (GTK_WIDGET(prv->ipsec_policy_file_chooser), toggled); */
    else
        g_assert_not_reached();

    gtk_widget_set_sensitive(GTK_WIDGET(current_chooser), toggled);

/*     if (toggled) { */
/*         filename = gtk_file_chooser_get_filename(current_chooser); */
/*         if (!filename) { */
/*             gtk_widget_activate(GTK_WIDGET(current_chooser)); */
/*             filename = gtk_file_chooser_get_filename(current_chooser); */
/*             if (!filename) { */
/*                 gtk_toggle_button_toggled(GTK_TOGGLE_BUTTON(button)); */
/*                 return; */
/*             } */
/*         } */
/*         g_free(filename); */
/*     } */
}

static void
svc_list_merge_to_model(gpointer data, gpointer user_data)
{
	GtkTreeModel *model = (GtkTreeModel *)user_data;
	GtkTreeIter   iter;

	gtk_list_store_append(GTK_LIST_STORE(model), &iter);
	gtk_list_store_set(GTK_LIST_STORE(model), &iter, 0, (gchar*)data, -1);

	g_free(data);
}

static void
get_smf_service_name(const char *fmri, gchar **name_out, gboolean* valid_fmri )
{
    static scf_handle_t        *handle = NULL;
	ssize_t         max_scf_name_length = scf_limit(SCF_LIMIT_MAX_NAME_LENGTH);
    scf_service_t *service;
    scf_instance_t *instance;
    scf_propertygroup_t    *pg;
    scf_property_t *prop;
    scf_value_t    *value;
    char           *name;
    char           *cname;
    char           *desc;

    if ( fmri == NULL ) {
        return;
    }

    if ( name_out != NULL ) {
        *name_out = NULL;
    }

    if ( valid_fmri != NULL ) {
        *valid_fmri = FALSE;
    }

    if ( handle == NULL ) {
        handle = scf_handle_create(SCF_VERSION);

        if (scf_handle_bind(handle) == -1 ) {
            g_error("couldn't bind to smf service: %s", scf_strerror(scf_error()) );
            return;
        }
    }

    service = scf_service_create( handle );
    instance = scf_instance_create( handle );

    if ( scf_handle_decode_fmri( handle, fmri , NULL, service, instance, NULL, NULL,
        SCF_DECODE_FMRI_REQUIRE_INSTANCE ) < 0 ) {
        g_debug("Problem decoding the fmri '%s' at line %d : %s", fmri, __LINE__, scf_strerror(scf_error()));
        scf_service_destroy( service );
        scf_instance_destroy( instance );
        return;
    }

    /* Got this far, so FMRI is valid */
    if ( valid_fmri != NULL ) {
        *valid_fmri = TRUE;
    }

    if ( name_out == NULL ) {
        /* May as well return now since we don't have anything to do if
         * name_out is NULL
         */
        return;
    }

    pg = scf_pg_create( handle );
    prop = scf_property_create( handle );
    value = scf_value_create( handle );
    name = g_malloc( max_scf_name_length + 1 );
    cname = g_malloc( max_scf_name_length + 1 );
    desc = g_malloc( max_scf_name_length + 1 );

    /* Check for the instance name first */
    if ( scf_instance_get_pg( instance, SCF_PG_TM_COMMON_NAME, pg) != -1 ) {
        if ( scf_pg_get_property(pg, "C", prop ) != -1 ) {
            if ( scf_property_get_value(prop, value ) != -1 ) {
                if ( scf_value_get_ustring(value, cname, max_scf_name_length + 1 ) != -1 ) {
                    g_debug("INSTANCE COMMON NAME : '%s'", cname );
                    if ( name_out != NULL ) {
                        *name_out = g_strdup(cname);
                    }
                }
            }
        }
    }

    /* Check the service name, if the instance name wasn't set */
    if ( *name_out != NULL ) {
        if ( scf_service_get_pg( service, SCF_PG_TM_COMMON_NAME, pg) != -1) {
            if ( scf_pg_get_property(pg, "C", prop ) != -1 ) {
                if ( scf_property_get_value(prop, value ) != -1 ) {
                    if ( scf_value_get_ustring(value, cname, max_scf_name_length + 1 ) != -1 ) {
                        g_debug("SERVICE COMMON NAME : '%s'", cname );
                        *name_out = g_strdup(cname);
                    }
                }
            }
        }
    }

#if 0
    /* Right now we don't need to get the description, but just in case we
     * need it in the future, we'll keep the code here.
     */
    if ( scf_service_get_pg( service, SCF_PG_TM_DESCRIPTION, pg) != -1 ) {
        if ( scf_pg_get_property(pg, "C", prop ) != -1 ) {
            if ( scf_property_get_value(prop, value ) != -1 ) {
                if ( scf_value_get_ustring(value, desc, max_scf_name_length + 1 ) != -1 ) {
                    g_debug("SERVICE DESCRIPTION : '%s'", desc );
                }
            }
        }
    }

    if ( scf_instance_get_pg( instance, SCF_PG_TM_DESCRIPTION, pg) != -1 ) {
        if ( scf_pg_get_property(pg, "C", prop ) != -1 ) {
            if ( scf_property_get_value(prop, value ) != -1 ) {
                if ( scf_value_get_ustring(value, desc, max_scf_name_length + 1 ) != -1 ) {
                    g_debug("INSTANCE DESCRIPTION : '%s'", desc );
                }
            }
        }
    }
#endif

/*     scf_handle_destroy( handle ); */
    scf_pg_destroy( pg );
    scf_property_destroy( prop );
    scf_value_destroy( value );

    g_free(name);
    g_free(cname);
    g_free(desc);
}

static void
fmri_dialog_response_cb( GtkWidget* widget, gint repsonseid, gpointer data )
{
	NwamEnvPrefDialogPrivate *prv = GET_PRIVATE(data);
    NwamEnvPrefDialog* self = NWAM_ENV_PREF_DIALOG(data);
    gboolean            stop_emission = FALSE;

    switch (repsonseid) {
    case GTK_RESPONSE_NONE:
        g_debug("GTK_RESPONSE_NONE");
        break;
    case GTK_RESPONSE_APPLY:
    case GTK_RESPONSE_OK: {
        const gchar *svc;
        gchar *svc_name;

        svc = gtk_entry_get_text(prv->smf_fmri_entry);

        if (svc) {
            gboolean valid_fmri = FALSE;

            get_smf_service_name(svc, &svc_name, &valid_fmri );

            /* Only fail if the FMRI isn't parseable, rather than based on
             * whether the name is valid, since some services don't set a
             * common name - a bug in the service, but we shoudn't fail.
             */
            if (valid_fmri) {
                gtk_widget_hide(widget);
                g_free(svc_name);
                break;
            }
        }
        nwamui_util_show_message(GTK_WINDOW(widget),
          GTK_MESSAGE_ERROR,
          _("Invalid Service FMRI"), 
          _("Either the service FMRI you entered was not in a valid\nformat, or the service does not exist. Please try again"), TRUE);
        gtk_widget_grab_focus(GTK_WIDGET(prv->smf_fmri_entry));
        gtk_editable_select_region(GTK_EDITABLE(prv->smf_fmri_entry), 0, -1);
        stop_emission = TRUE;
    }
        break;
    case GTK_RESPONSE_CANCEL:
        g_debug("GTK_RESPONSE_CANCEL");
        gtk_widget_hide(widget);
        stop_emission = TRUE;
        break;
    case GTK_RESPONSE_HELP:
        g_debug("GTK_RESPONSE_HELP");
        stop_emission = TRUE;
        break;
    }
    if ( stop_emission ) {
        g_signal_stop_emission_by_name(widget, "response" );
    }
}

static void

on_activate_static_menuitems (GtkAction *action, gpointer data)
{
	NwamEnvPrefDialogPrivate *prv = GET_PRIVATE(data);
    NwamEnvPrefDialog* self = NWAM_ENV_PREF_DIALOG(data);

    gchar first_char = *gtk_action_get_name(action);

    switch(first_char) {
    case '0':
    case '1':
    default:;
    }
}

static void
vanity_name_edited ( GtkCellRendererText *cell,
  const gchar         *path_string,
  const gchar         *new_text,
  gpointer             data)
{
}

#if 0
static void
vanity_name_editing_started (GtkCellRenderer *cell,
  GtkCellEditable *editable,
  const gchar     *path_string,
  gpointer         data)
{
	NwamEnvPrefDialogPrivate *prv = GET_PRIVATE(data);
    NwamEnvPrefDialog* self = NWAM_ENV_PREF_DIALOG(data);
    GtkTreeModel *model;
    GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
    GtkTreeIter iter;

	model = gtk_tree_view_get_model(prv->nameservices_table);
    if ( gtk_tree_model_get_iter (model, &iter, path))
        switch (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (cell), TREEVIEW_COLUMN_NUM))) {
        case NAMESERVICES_NAME:      {
            
            if (GTK_IS_ENTRY(editable)) {
                GtkWidget *menu;
                GtkTreeViewColumn *column;
                GtkAllocation a;
                GdkRectangle rect;

                menu = nwam_menu_group_get_menu(prv->menugroup);

                gtk_tree_view_get_cursor(prv->nameservices_table,
                  NULL,
                  &column);

                if (column && menu) {
                    gint ns;

                    gtk_tree_model_get(model, &iter, 0, &ns, -1);

                    gtk_tree_view_get_cell_area(prv->nameservices_table,
                      path,
                      column,
                      &rect);

                    gdk_window_get_origin(gtk_tree_view_get_bin_window(prv->nameservices_table),
                      &a.x,
                      &a.y);
                    a.x += rect.x;
                    a.y += rect.y;

                    /* Enable current menu */
                    action_menu_group_set_visible(prv->menugroup,
                      nwam_nameservices_enum_to_string(ns),
                      TRUE);

                    gtk_menu_popup(menu, NULL, NULL, menu_pos_func, &a, 0,
                      gtk_get_current_event_time());

                }
            }
        }
            break;
        default:
            g_assert_not_reached();
        }

    gtk_tree_path_free (path);
}
#endif /* 0 */

static void
menu_pos_func(GtkMenu *menu,
  gint *x,
  gint *y,
  gboolean *push_in,
  gpointer user_data)
{
    GtkAllocation *a = (GtkAllocation*)user_data;
    *x = a->x;
    *y = a->y;
    *push_in = FALSE;
}

#if 0
static void
action_menu_group_set_visible(NwamMenuGroup *menugroup, const gchar *name, gboolean visible)
{
    GtkAction *action;

    if (name) {
        action = (GtkAction*)nwam_menu_group_get_item_by_name(menugroup, name);

        if (action) {
            g_assert(GTK_IS_ACTION(action));

            gtk_action_set_visible(action, visible);
        }
    }
}
#endif
