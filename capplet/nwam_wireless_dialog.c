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
#include <glib/gi18n.h>
#include <strings.h>
#include <sys/ethernet.h>

#include "nwam_pref_iface.h"
#include "nwam_wireless_dialog.h"

/* Names of Widgets in Glade file */
#define WIRELESS_DIALOG_NAME                    "add_wireless_network"
#define WIRELESS_DIALOG_ESSID_CBENTRY           "ssid_combo_entry"
#define WIRELESS_DIALOG_SEC_COMBO               "security_combo"
#define WIRELESS_PASSWORD_NOTEBOOK              "password_notebook"
#define WIRELESS_DIALOG_WEP_KEY_ENTRY           "wep_password_entry"
#define WIRELESS_DIALOG_WEP_KEY_CONF_ENTRY      "wep_confirm_password_entry"
#define WIRELESS_DIALOG_KEY_INDEX_SPINBTN       "key_index_spinbtn"
#define WIRELESS_DIALOG_KEY_INDEX_LBL           "key_index_lbl"
#define WIRELESS_DIALOG_BSSID_LIST              "bssid_list"
#define WIRELESS_DIALOG_BSSID_ADD_BUTTON        "bssid_add_btn"
#define WIRELESS_DIALOG_BSSID_REMOVE_BUTTON     "bssid_remove_btn"
#define WIRELESS_DIALOG_WPA_CONFIG_COMBO        "wpa_config_combo"
#define WIRELESS_DIALOG_WPA_USERNAME_ENTRY      "wpa_username_entry"
#define WIRELESS_DIALOG_WPA_PASSWORD_ENTRY      "wpa_password_entry"
#define WIRELESS_LEAP_USERNAME_ENTRY            "leap_username_entry"
#define WIRELESS_LEAP_PASSWORD_ENTRY            "leap_password_entry"
#define WIRELESS_DIALOG_PERSIST_CBUTTON         "add_to_preferred_cbox"
#define WIRELESS_DIALOG_SHOW_PASSWORD_CBUTTON   "show_password_cbox"


#define WIRELESS_NOTEBOOK_WEP_PAGE              (0)
#define WIRELESS_NOTEBOOK_WPA_PAGE              (1)

static GObjectClass *parent_class = NULL;

enum {
        PROP_ZERO,
        PROP_NCU,
        PROP_ESSID,
        PROP_SECURITY,
        PROP_WEP_KEY,
        PROP_WEP_KEY_INDEX,
        PROP_WPA_USERNAME,
        PROP_WPA_PASSWORD,
        PROP_WPA_CONFIG_TYPE,
        PROP_WPA_CERT_FILE,
        PROP_BSSID_LIST,
        PROP_PERSISTANT,
        PROP_DO_CONNECT,
};

struct _NwamWirelessDialogPrivate {
        /* Widget Pointers */
        GtkDialog*              wireless_dialog;
        GtkComboBoxEntry*       essid_combo;
        GtkEntry*               essid_cbentry; /* Convienience for essid_combo */
        GtkComboBox*            security_combo;
        GtkNotebook*            password_notebook;
        GtkCheckButton*         persistant_cbutton;
        GtkCheckButton*         show_password_cbutton;
        /* WEP Settings */
        GtkEntry*               key_entry;
        GtkEntry*               key_conf_entry;
        GtkTreeView*            bssid_list_tv;
        GtkButton*              bssid_add_btn;
        GtkButton*              bssid_remove_btn;
        GtkLabel*               key_index_lbl;
        GtkSpinButton*          key_index_spinbtn;
        /* WPA Settings */
        GtkComboBox*            wpa_config_combo;
        GtkEntry*		        wpa_username_entry;
        GtkEntry*		        wpa_password_entry;
        /* TODO - Handle CERT FILE when supported */

        /* Other Data */
        NwamuiNcu*              ncu;
        NwamuiWifiNet*          wifi_net;
        gboolean                do_connect;
        nwamui_wireless_dialog_title_t title;
        gboolean                key_entry_changed;
        gboolean                sec_mode_changed;
};

static void nwam_pref_init (gpointer g_iface, gpointer iface_data);
static gboolean refresh(NwamPrefIFace *iface, gpointer user_data, gboolean force);
static gboolean apply(NwamPrefIFace *iface, gpointer user_data);
static gboolean help(NwamPrefIFace *iface, gpointer user_data);
static gint dialog_run(NwamPrefIFace *iface, GtkWindow *parent);
static GtkWindow* dialog_get_window(NwamPrefIFace *iface);

static void nwam_wireless_dialog_set_property ( GObject         *object,
                                                guint            prop_id,
                                                const GValue    *value,
                                                GParamSpec      *pspec);

static void nwam_wireless_dialog_get_property ( GObject         *object,
                                                guint            prop_id,
                                                GValue          *value,
                                                GParamSpec      *pspec);

static void nwam_wireless_dialog_finalize (     NwamWirelessDialog *self);


static void change_essid_cbentry_model(GtkComboBoxEntry *cbentry);

static void populate_essid_combo(NwamWirelessDialog *self, GtkComboBoxEntry *cbentry);

/* Callbacks */
static void object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);
static void response_cb( GtkWidget* widget, gint repsonseid, gpointer data );
static void essid_changed_cb( GtkWidget* widget, gpointer data );
static void key_entry_changed_cb( GtkWidget* widget, gpointer data );
static void security_selection_cb( GtkWidget* widget, gpointer data );
static void show_password_cb( GtkToggleButton* widget, gpointer data );
static void bssid_add_cb(GtkWidget *button, gpointer data);
static void bssid_remove_cb(GtkWidget *widget, gpointer data);
static void bssid_edited_cb (GtkCellRendererText *renderer, gchar *path,
                             gchar *new_text, gpointer user_data);



G_DEFINE_TYPE_EXTENDED (NwamWirelessDialog,
  nwam_wireless_dialog,
  G_TYPE_OBJECT,
  0,
  G_IMPLEMENT_INTERFACE (NWAM_TYPE_PREF_IFACE, nwam_pref_init))

static void
nwam_pref_init (gpointer g_iface, gpointer iface_data)
{
	NwamPrefInterface *iface = (NwamPrefInterface *)g_iface;
/*     iface->refresh = refresh; */
/*     iface->apply = apply; */
    iface->help = help;
    iface->dialog_run = dialog_run;
    iface->dialog_get_window = dialog_get_window;
}

static void
nwam_wireless_dialog_class_init (NwamWirelessDialogClass *klass)
{
    /* Pointer to GObject Part of Class */
    GObjectClass *gobject_class = (GObjectClass*) klass;
        
    /* Initialise Static Parent Class pointer */
    parent_class = g_type_class_peek_parent (klass);

    /* Override Some Function Pointers */
    gobject_class->set_property = nwam_wireless_dialog_set_property;
    gobject_class->get_property = nwam_wireless_dialog_get_property;
    gobject_class->finalize = (void (*)(GObject*)) nwam_wireless_dialog_finalize;

    /* Create some properties */
    g_object_class_install_property (gobject_class,
                                     PROP_NCU,
                                     g_param_spec_object ("ncu",
                                                          _("Wireless NCU"),
                                                          _("The Wireless NCU that the dialog is to work with."),
                                                          NWAMUI_TYPE_NCU,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_ESSID,
                                     g_param_spec_string ("essid",
                                                          _("Wireless ESSID"),
                                                          _("The Wireless ESSID to connect to."),
                                                          NULL,
                                                          G_PARAM_READWRITE));
    g_object_class_install_property (gobject_class,
                                     PROP_SECURITY,
                                     g_param_spec_int    ("security",
                                                          _("Wireless Security Type"),
                                                          _("The Wireless security (None/WEP/WPA)."),
                                                          0, NWAMUI_WIFI_SEC_LAST,
                                                          NWAMUI_WIFI_SEC_NONE,
                                                          G_PARAM_READWRITE));
    g_object_class_install_property (gobject_class,
                                     PROP_WEP_KEY,
                                     g_param_spec_string ("key",
                                                          _("Wireless Security Key"),
                                                          _("The Wireless security key."),
                                                          NULL,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_WEP_KEY_INDEX,
                                     g_param_spec_int ("key_index",
                                                          _("Wireless Security Key Index"),
                                                          _("The Wireless security key index."),
                                                          1, /* Min */
                                                          4, /* Max */
                                                          1, /* Default */
                                                          G_PARAM_READWRITE));
    g_object_class_install_property (gobject_class,
                                     PROP_WPA_USERNAME,
                                     g_param_spec_string ("wpa_username",
                                                          _("WPA Username"),
                                                          _("The Username to use for a WPA connection."),
                                                          NULL,
                                                          G_PARAM_READWRITE));
    g_object_class_install_property (gobject_class,
                                     PROP_WPA_PASSWORD,
                                     g_param_spec_string ("wpa_password",
                                                          _("WPA Password"),
                                                          _("The Password to use for a WPA connection."),
                                                          NULL,
                                                          G_PARAM_READWRITE));
    g_object_class_install_property (gobject_class,
                                     PROP_WPA_CONFIG_TYPE,
                                     g_param_spec_int    ("wpa_config_type",
                                                          _("WPA Config Type"),
                                                          _("The type of WPA Connection (Automatic or TTLS-PAP)"),
                                                          0, NWAMUI_WIFI_WPA_CONFIG_LAST,
                                                          NWAMUI_WIFI_WPA_CONFIG_AUTOMATIC,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_WPA_CERT_FILE,
                                     g_param_spec_string ("wpa_cert_file",
                                                          _("WPA Cert File"),
                                                          _("The Certificate File to use for WPA."),
                                                          NULL,
                                                          G_PARAM_READWRITE));    
    g_object_class_install_property (gobject_class,
                                     PROP_BSSID_LIST,
                                     g_param_spec_pointer ("bssid_list",
                                                          _("Wireless BSSID list"),
                                                          _("The Wireless AP BSSIDs known."),
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_PERSISTANT,
                                     g_param_spec_boolean ("persistant",
                                                          _("Wireless Network is Persistant"),
                                                          _("This Wireless Network should be remembered, on successful connection"),
                                                          FALSE,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_DO_CONNECT,
                                     g_param_spec_boolean ("do_connect",
                                                          _("On success, connect to Wireless Network"),
                                                          _("On success (RESPONSE_OK), connect to Wireless Network"),
                                                          FALSE,
                                                          G_PARAM_READWRITE));
    
}


static void
nwam_wireless_dialog_init (NwamWirelessDialog *self)
{
    GtkTreeModel    *model = NULL;
    GtkListStore    *bssid_list_store = NULL;
    
    self->prv = g_new0 (NwamWirelessDialogPrivate, 1);
    
    /* Iniialise pointers to important widgets */
    self->prv->wireless_dialog =        
        GTK_DIALOG(nwamui_util_ui_get_widget_from(NWAMUI_UI_FILE_WIRELESS, WIRELESS_DIALOG_NAME));
    self->prv->essid_combo = 
        GTK_COMBO_BOX_ENTRY(nwamui_util_ui_get_widget_from(NWAMUI_UI_FILE_WIRELESS, WIRELESS_DIALOG_ESSID_CBENTRY));
    self->prv->essid_cbentry =
        GTK_ENTRY(gtk_bin_get_child(GTK_BIN(self->prv->essid_combo)));
    self->prv->security_combo =
        GTK_COMBO_BOX(nwamui_util_ui_get_widget_from(NWAMUI_UI_FILE_WIRELESS, WIRELESS_DIALOG_SEC_COMBO));
    self->prv->password_notebook =
        GTK_NOTEBOOK(nwamui_util_ui_get_widget_from(NWAMUI_UI_FILE_WIRELESS, WIRELESS_PASSWORD_NOTEBOOK));
    self->prv->key_entry =
        GTK_ENTRY(nwamui_util_ui_get_widget_from(NWAMUI_UI_FILE_WIRELESS, WIRELESS_DIALOG_WEP_KEY_ENTRY));
    self->prv->key_index_lbl =
        GTK_LABEL(nwamui_util_ui_get_widget_from(NWAMUI_UI_FILE_WIRELESS, WIRELESS_DIALOG_KEY_INDEX_LBL));
    self->prv->key_index_spinbtn =
        GTK_SPIN_BUTTON(nwamui_util_ui_get_widget_from(NWAMUI_UI_FILE_WIRELESS, WIRELESS_DIALOG_KEY_INDEX_SPINBTN));
    /* self->prv->key_conf_entry =
     * GTK_ENTRY(nwamui_util_ui_get_widget_from(NWAMUI_UI_FILE_WIRELESS, WIRELESS_DIALOG_WEP_KEY_CONF_ENTRY)); */
    self->prv->key_conf_entry =
        GTK_ENTRY(nwamui_util_ui_get_widget_from(NWAMUI_UI_FILE_WIRELESS, WIRELESS_DIALOG_WEP_KEY_ENTRY));
    self->prv->bssid_list_tv =
        GTK_TREE_VIEW(nwamui_util_ui_get_widget_from(NWAMUI_UI_FILE_WIRELESS, WIRELESS_DIALOG_BSSID_LIST));
    self->prv->bssid_add_btn =
        GTK_BUTTON(nwamui_util_ui_get_widget_from(NWAMUI_UI_FILE_WIRELESS, WIRELESS_DIALOG_BSSID_ADD_BUTTON));
    self->prv->bssid_remove_btn =
        GTK_BUTTON(nwamui_util_ui_get_widget_from(NWAMUI_UI_FILE_WIRELESS, WIRELESS_DIALOG_BSSID_REMOVE_BUTTON));
    self->prv->wpa_config_combo =
        GTK_COMBO_BOX(nwamui_util_ui_get_widget_from(NWAMUI_UI_FILE_WIRELESS, WIRELESS_DIALOG_WPA_CONFIG_COMBO));
    self->prv->wpa_username_entry =
        GTK_ENTRY(nwamui_util_ui_get_widget_from(NWAMUI_UI_FILE_WIRELESS, WIRELESS_DIALOG_WPA_USERNAME_ENTRY));
    self->prv->wpa_password_entry =
        GTK_ENTRY(nwamui_util_ui_get_widget_from(NWAMUI_UI_FILE_WIRELESS, WIRELESS_DIALOG_WPA_PASSWORD_ENTRY));
    self->prv->persistant_cbutton =
        GTK_CHECK_BUTTON(nwamui_util_ui_get_widget_from(NWAMUI_UI_FILE_WIRELESS, WIRELESS_DIALOG_PERSIST_CBUTTON));
    self->prv->show_password_cbutton =  
        GTK_CHECK_BUTTON(nwamui_util_ui_get_widget_from(NWAMUI_UI_FILE_WIRELESS, WIRELESS_DIALOG_SHOW_PASSWORD_CBUTTON));
    
    self->prv->ncu = NULL;
    self->prv->do_connect = FALSE;
    self->prv->title = NWAMUI_WIRELESS_DIALOG_TITLE_ADD;

    gtk_spin_button_set_range(self->prv->key_index_spinbtn, 1, 4);
    gtk_spin_button_set_increments(self->prv->key_index_spinbtn, 1.0, 1.0);
    /* Change the ESSID comboboxentry to use Text and an an image (for Secure/Open) */
    change_essid_cbentry_model(GTK_COMBO_BOX_ENTRY(self->prv->essid_combo));

    /* Create model for the bssid_list treeview */
    bssid_list_store = gtk_list_store_new( 1, G_TYPE_STRING );
    gtk_tree_view_set_model( self->prv->bssid_list_tv, GTK_TREE_MODEL( bssid_list_store ) );
    g_object_set(G_OBJECT(self->prv->bssid_list_tv), 
            "enable-search", FALSE, 
            NULL);
    {
        GtkCellRenderer*    renderer = gtk_cell_renderer_text_new ();
	    g_object_set(G_OBJECT(renderer), 
                "editable", TRUE, 
                NULL);
        g_signal_connect(G_OBJECT(renderer), "edited",
                         (GCallback) bssid_edited_cb, (gpointer) self->prv->bssid_list_tv);
        GtkTreeViewColumn*  column = gtk_tree_view_column_new_with_attributes (_("BSSID"),
                                   renderer,
                                   "text", 0,
                                   NULL);
        gtk_tree_view_column_set_sort_column_id (column, 0);
        gtk_tree_view_append_column(self->prv->bssid_list_tv, column);
    }
    g_object_unref( bssid_list_store );

    /* Initialise list of security types */
    model = gtk_combo_box_get_model(GTK_COMBO_BOX(self->prv->security_combo));
    gtk_list_store_clear (GTK_LIST_STORE (model)); /* Empry existing list */
    for (int i = 0; i < NWAMUI_WIFI_SEC_LAST; i++) {
        gtk_combo_box_append_text(GTK_COMBO_BOX(self->prv->security_combo), nwamui_util_wifi_sec_to_string( i ));
    }
    
    /* Initialise list of WPA Config Types */
    model = gtk_combo_box_get_model(GTK_COMBO_BOX(self->prv->wpa_config_combo));
    gtk_list_store_clear (GTK_LIST_STORE (model)); /* Empry existing list */
    for (int i = 0; i < NWAMUI_WIFI_WPA_CONFIG_LAST; i++) {
        gtk_combo_box_append_text(GTK_COMBO_BOX(self->prv->wpa_config_combo), nwamui_util_wifi_wpa_config_to_string( i ));
    }
    
    /* Fill in the essid list using a scan */
    populate_essid_combo(self, GTK_COMBO_BOX_ENTRY(self->prv->essid_combo));


    g_signal_connect(G_OBJECT(self), "notify", (GCallback)object_notify_cb, (gpointer)self);
    g_signal_connect(GTK_DIALOG(self->prv->wireless_dialog), "response", (GCallback)response_cb, (gpointer)self);
    g_signal_connect(GTK_ENTRY(self->prv->essid_cbentry), "changed", (GCallback)essid_changed_cb, (gpointer)self);
    g_signal_connect(GTK_ENTRY(self->prv->key_entry), "changed", (GCallback)key_entry_changed_cb, (gpointer)self);
    g_signal_connect(GTK_COMBO_BOX(self->prv->security_combo), "changed", (GCallback)security_selection_cb, (gpointer)self);
    g_signal_connect(GTK_TOGGLE_BUTTON(self->prv->show_password_cbutton), "toggled", (GCallback)show_password_cb, (gpointer)self);
    g_signal_connect(GTK_BUTTON(self->prv->bssid_add_btn), "clicked", (GCallback)bssid_add_cb, 
                               (gpointer)self->prv->bssid_list_tv);
    g_signal_connect(GTK_BUTTON(self->prv->bssid_remove_btn), "clicked", (GCallback)bssid_remove_cb, 
                               (gpointer)self->prv->bssid_list_tv);
}

static void
change_essid_cbentry_model(GtkComboBoxEntry *cbentry)
{
  GtkTreeModel      *model;
  GtkCellRenderer   *pix_renderer;

  model = GTK_TREE_MODEL(gtk_list_store_new (3, G_TYPE_STRING,      /* ESSID */
                                                GDK_TYPE_PIXBUF,    /* Pixmap - SECURRE/NOT */
                                                G_TYPE_INT));       /* INT : nwamui_wifi_security_t */
  gtk_combo_box_set_model(GTK_COMBO_BOX (cbentry), model);
  g_object_unref (model);

  gtk_combo_box_entry_set_text_column (cbentry, 0);

  /* Use a  pixbuf cell renderer for the icon, in column 1 */
  pix_renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_end(GTK_CELL_LAYOUT (cbentry), pix_renderer, FALSE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (cbentry), pix_renderer, "pixbuf", 1);
}

static void
add_essid_list_item_to_essid_liststore(GObject* daemon, GObject* wifi, gpointer user_data)
{
    NwamuiWifiNet          *wifi_elem = NULL;
    GtkTreeModel           *model = (GtkTreeModel*)user_data;
    GtkIconTheme           *icon_theme;
    GdkPixbuf              *icon;
    GtkTreeIter             iter;
    nwamui_wifi_security_t  sec;
    gchar*                  essid;

    g_debug("Got : WifiResult Signal");
    
    if ( wifi == NULL ) {
        /* End of Scan */
        g_debug("End Of Scan");
        
        /* De-register interest in events here */
        g_signal_handlers_disconnect_by_func(daemon, (gpointer)add_essid_list_item_to_essid_liststore, user_data);
        g_debug("Unregistered interest in WifiScanResults");
        return;
    }
    
    g_assert( NWAMUI_WIFI_NET(wifi) );
    
    wifi_elem = NWAMUI_WIFI_NET(wifi);
    
    essid = nwamui_wifi_net_get_essid(wifi_elem);
    sec = nwamui_wifi_net_get_security(wifi_elem);
    
    icon = nwamui_util_get_network_security_icon( sec, TRUE );
    
    gtk_list_store_append (GTK_LIST_STORE (model), &iter);
    gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                        0, essid, 1, icon, 2, (gint)sec, -1);
    g_object_unref (icon);
    
    g_free (essid);
}

static void 
populate_essid_combo(NwamWirelessDialog *self, GtkComboBoxEntry *cbentry)
{
    GtkTreeModel    *model;
    GList           *list = NULL;
    NwamuiDaemon    *daemon = nwamui_daemon_get_instance();
    
    model = gtk_combo_box_get_model(GTK_COMBO_BOX(cbentry));
    gtk_list_store_clear (GTK_LIST_STORE (model)); /* Empry list */

    g_signal_connect(daemon, "wifi_scan_result", (GCallback)add_essid_list_item_to_essid_liststore, (gpointer)model);
    
    nwamui_daemon_dispatch_wifi_scan_events_from_cache( daemon );

    g_object_unref(G_OBJECT(daemon));
}

static void
nwam_wireless_dialog_set_property ( GObject         *object,
                                    guint            prop_id,
                                    const GValue    *value,
                                    GParamSpec      *pspec)
{
    NwamWirelessDialog *self = NWAM_WIRELESS_DIALOG(object);
    gchar*      tmpstr = NULL;
    gint        tmpint = 0;

    switch (prop_id) {
        case PROP_NCU: {
                NwamuiNcu* ncu = NWAMUI_NCU( g_value_dup_object( value ) );

                if ( self->prv->ncu != NULL ) {
                        g_object_unref( self->prv->ncu );
                }
                self->prv->ncu = ncu;
            }
            break;
        case PROP_ESSID:
            tmpstr = (gchar *) g_value_get_string (value);

            if (self->prv->essid_cbentry != NULL) {
                gtk_entry_set_text(GTK_ENTRY(self->prv->essid_cbentry), tmpstr?tmpstr:"");
            }
            break;
        case PROP_SECURITY:
            tmpint = g_value_get_int (value);

            g_assert( tmpint >= 0 && tmpint <= NWAMUI_WIFI_SEC_LAST );

            gtk_combo_box_set_active(GTK_COMBO_BOX(self->prv->security_combo), tmpint);
           
            break;            
        case PROP_WEP_KEY:
            tmpstr = (gchar *) g_value_get_string (value);

            if (self->prv->key_entry != NULL) {
                gtk_entry_set_text(GTK_ENTRY(self->prv->key_entry), tmpstr?tmpstr:"");
            }
            if (self->prv->key_conf_entry != NULL) {
                gtk_entry_set_text(GTK_ENTRY(self->prv->key_conf_entry), tmpstr?tmpstr:"");
            }
            break;
        case PROP_WEP_KEY_INDEX:
            if (self->prv->key_index_spinbtn != NULL) {
                gtk_spin_button_set_value(GTK_SPIN_BUTTON(self->prv->key_index_spinbtn),
                  g_value_get_int(value));
            }
            break;
	case PROP_WPA_CONFIG_TYPE:
            tmpint = g_value_get_int (value);

            g_assert( tmpint >= 0 && tmpint <= NWAMUI_WIFI_WPA_CONFIG_LAST );

            gtk_combo_box_set_active(GTK_COMBO_BOX(self->prv->wpa_config_combo), tmpint);
           
            break;
	case PROP_WPA_USERNAME:
            tmpstr = (gchar *) g_value_get_string (value);

            if (self->prv->key_entry != NULL && self->prv->wpa_username_entry != NULL) {
                gtk_entry_set_text(GTK_ENTRY(self->prv->wpa_username_entry), tmpstr?tmpstr:"");
            }
            break;
	case PROP_WPA_PASSWORD:
            tmpstr = (gchar *) g_value_get_string (value);

            if (self->prv->key_entry != NULL && self->prv->wpa_password_entry != NULL) {
                gtk_entry_set_text(GTK_ENTRY(self->prv->wpa_password_entry), tmpstr?tmpstr:"");
            }
            break;
	case PROP_WPA_CERT_FILE:
            /* TODO - Handle CERT FILE when supported
            tmpstr = (gchar *) g_value_get_string (value);

            g_assert( tmpstr != NULL );

            if (self->prv->key_entry != NULL && self->prv->leap_password_entry != NULL) {
                if ( tmpstr != NULL ) {
                    gtk_entry_set_text(GTK_ENTRY(self->prv->leap_password_entry), tmpstr);
                }
            }
             */
            break;
        case PROP_BSSID_LIST: {
                GList          *blist = (GList *) g_value_get_pointer (value);
                GtkTreeModel   *model = NULL;
                GtkTreeIter     iter;

                model = gtk_tree_view_get_model (self->prv->bssid_list_tv);
                gtk_list_store_clear( GTK_LIST_STORE(model) );

                for ( GList* elem = g_list_first( blist );
                      elem != NULL;
                      elem = g_list_next(elem) ) {
                    gtk_list_store_append(GTK_LIST_STORE(model), &iter );
                    gtk_list_store_set(GTK_LIST_STORE(model), &iter, 0, elem->data, -1);
                }
            }
            break;
        case PROP_PERSISTANT:
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(self->prv->persistant_cbutton), g_value_get_boolean (value));
            break;
        case PROP_DO_CONNECT:
            self->prv->do_connect = g_value_get_boolean (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
nwam_wireless_dialog_get_property (GObject         *object,
                                  guint            prop_id,
                                  GValue          *value,
                                  GParamSpec      *pspec)
{
    NwamWirelessDialog *self = NWAM_WIRELESS_DIALOG(object);

    switch (prop_id) {
	case PROP_NCU:
            g_value_set_object( value, self->prv->ncu );
            break;
	case PROP_ESSID:
            if (self->prv->essid_cbentry != NULL) {
                g_value_set_string( value, g_strdup(gtk_entry_get_text(GTK_ENTRY(self->prv->essid_cbentry))) );
            }
            break;
	case PROP_SECURITY:
            if (self->prv->security_combo != NULL) {
                g_value_set_int( value, gtk_combo_box_get_active(GTK_COMBO_BOX(self->prv->security_combo)) );
            }
            break;
	case PROP_WEP_KEY:
            if (self->prv->key_entry != NULL) {
                g_value_set_string( value, g_strdup(gtk_entry_get_text(GTK_ENTRY(self->prv->key_entry))) );
            }
            break;            
	case PROP_WEP_KEY_INDEX:
            if (self->prv->key_index_spinbtn != NULL) {
                g_value_set_int(value,
                  gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(self->prv->key_index_spinbtn)));
            }
            break;            
	case PROP_WPA_CONFIG_TYPE:
            if (self->prv->wpa_config_combo != NULL) {
                g_value_set_int( value, gtk_combo_box_get_active(GTK_COMBO_BOX(self->prv->wpa_config_combo)) );
            }
            break;
	case PROP_WPA_USERNAME:
            if (self->prv->wpa_username_entry != NULL) {
                g_value_set_string( value, g_strdup(gtk_entry_get_text(GTK_ENTRY(self->prv->wpa_username_entry))) );
            }
            break;
	case PROP_WPA_PASSWORD:
            if (self->prv->wpa_password_entry != NULL) {
                g_value_set_string( value, g_strdup(gtk_entry_get_text(GTK_ENTRY(self->prv->wpa_password_entry))) );
            }
            break;
	case PROP_WPA_CERT_FILE:
            /* TODO - Handle CERT FILE when supported
            if (self->prv->leap_password_entry != NULL) {
                g_value_set_string( value, g_strdup(gtk_entry_get_text(GTK_ENTRY(self->prv->leap_password_entry))) );
            }
             */
            break;
        case PROP_BSSID_LIST: {
                GList          *blist = NULL;
                GtkTreeModel   *model = NULL;
                GtkTreeIter     iter;

                model = gtk_tree_view_get_model (self->prv->bssid_list_tv);

                if ( gtk_tree_model_get_iter_first( model, &iter ) ) {
                    do {
                        gchar*  string = NULL;
                        gtk_tree_model_get(model, &iter, 0, &string, -1);

                        if ( string != NULL ) {
                            blist = g_list_append( blist, string );
                        }
                    }
                    while ( gtk_tree_model_iter_next( model, &iter ) );
                }
                g_value_set_pointer( value, blist );
            }
            break;
	case PROP_PERSISTANT:
            if (self->prv->persistant_cbutton != NULL) {
                g_value_set_boolean (value, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(self->prv->persistant_cbutton)) ) ;
            }
            break;
	case PROP_DO_CONNECT:
            g_value_set_boolean(value, self->prv->do_connect) ;
            break;
        default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
    }
}


/**
 * nwam_wireless_dialog_new:
 * @returns: a new #NwamWirelessDialog.
 *
 * Creates a new #NwamWirelessDialog with an empty ncu
 **/
NwamWirelessDialog*
nwam_wireless_dialog_get_instance (void)
{
    static NwamWirelessDialog*  instance = NULL;

    if ( instance == NULL ) {
        instance = NWAM_WIRELESS_DIALOG(g_object_new (NWAM_TYPE_WIRELESS_DIALOG, NULL));
    }

	return NWAM_WIRELESS_DIALOG(g_object_ref(instance));
}

/** 
 * nwam_wireless_dialog_set_ncu:
 * @nwam_wireless_dialog: a #NwamWirelessDialog.
 * @ncu: The Wireless NCU the dialog is to configure
 * 
 * Sets the Wireless NCU of @nwam_wireless_dialog so that
 * the dialog can configure it.
 **/ 
void
nwam_wireless_dialog_set_ncu (NwamWirelessDialog *self,
                              NwamuiNcu          *ncu )
{
    g_return_if_fail (NWAM_IS_WIRELESS_DIALOG (self));
    g_assert (ncu != NULL );

    if ( ncu != NULL ) {
        g_object_set (G_OBJECT (self),
                      "ncu", ncu,
                      NULL);
    }
}

extern  void
nwam_wireless_dialog_set_title( NwamWirelessDialog  *self, nwamui_wireless_dialog_title_t title )
{
    const gchar*    title_str = NULL;

    g_return_if_fail (NWAM_IS_WIRELESS_DIALOG (self));

    g_assert( title >= NWAMUI_WIRELESS_DIALOG_TITLE_ADD && title < NWAMUI_WIRELESS_DIALOG_TITLE_LAST );

    switch( title ) {
        case NWAMUI_WIRELESS_DIALOG_TITLE_ADD:
            title_str = _("Add Wireless Network");
            break;
        case NWAMUI_WIRELESS_DIALOG_TITLE_JOIN:
            title_str = _("Join Wireless Network");
            break;
        case NWAMUI_WIRELESS_DIALOG_TITLE_EDIT:
            title_str = _("Edit Wireless Network");
            break;
    }
    
    if ( title_str != NULL ) {
        self->prv->title = title;
        gtk_window_set_title( GTK_WINDOW(self->prv->wireless_dialog), title_str );
    }
}

/**
 * nwam_wireless_dialog_get_wifi_net:
 * @nwam_wireless_dialog: a #NwamWirelessDialog.
 * @returns: a new NwamuiWifiNet object representing the configuration.
 *
 * Gets a new NwamuiWifiNet object based on the configuration entered in the dialog.
 *
 * Will return NULL is OK wasn't clicked.
 *
 **/
NwamuiWifiNet*
nwam_wireless_dialog_get_wifi_net (NwamWirelessDialog *self)
{
    g_return_val_if_fail (NWAM_IS_WIRELESS_DIALOG (self), NULL);

    return( self->prv->wifi_net );
}

/**
 * nwam_wireless_dialog_set_wifi_net:
 * @nwam_wireless_dialog: a #NwamWirelessDialog.
 * @wifi_net: a NwamuiWifiNet object representing the configuration to present
 *
 *
 **/
void 
nwam_wireless_dialog_set_wifi_net (NwamWirelessDialog *self, NwamuiWifiNet* wifi_net )
{
    g_return_if_fail (NWAM_IS_WIRELESS_DIALOG (self));

    if ( self->prv->wifi_net != NULL ) {
        g_object_unref(self->prv->wifi_net);
    }
    self->prv->wifi_net = NULL;

    if ( wifi_net != NULL ) {
        GList                   *bssid_list = NULL;
        nwamui_wifi_security_t   security = nwamui_wifi_net_get_security(wifi_net);

        self->prv->wifi_net = NWAMUI_WIFI_NET(g_object_ref(wifi_net));

        if ( self->prv->title == NWAMUI_WIRELESS_DIALOG_TITLE_EDIT &&
             self->prv->title == NWAMUI_WIRELESS_DIALOG_TITLE_ADD ) {
            bssid_list = nwamui_wifi_net_get_fav_bssid_list(wifi_net);
        }
        else {
            /* Want to see all knowns bssids */
            bssid_list = nwamui_wifi_net_get_bssid_list(wifi_net);
        }

        if ( self->prv->title == NWAMUI_WIRELESS_DIALOG_TITLE_EDIT ) {
            /* Don't allow user to edit the text if it's not possible */
            gboolean    editable = nwamui_object_can_rename( NWAMUI_OBJECT(wifi_net) );

            gtk_widget_set_sensitive(( GTK_WIDGET(self->prv->essid_combo) ), editable );
        }

        g_object_set(G_OBJECT (self),
                "essid", nwamui_wifi_net_get_essid(wifi_net),
                "bssid_list", bssid_list,
                "security", security,
                NULL );

        if ( bssid_list ) {
            g_list_foreach( bssid_list, (GFunc)g_free, NULL );
            g_list_free(bssid_list);
        }

        switch (security) {
            case NWAMUI_WIFI_SEC_NONE: 
                break;

#ifdef WEP_ASCII_EQ_HEX 
            case NWAMUI_WIFI_SEC_WEP:
#else
            case NWAMUI_WIFI_SEC_WEP_HEX:
            case NWAMUI_WIFI_SEC_WEP_ASCII:
#endif /* WEP_ASCII_EQ_HEX */
            case NWAMUI_WIFI_SEC_WPA_PERSONAL:
                g_object_set(G_OBJECT (self),
                        "key", nwamui_wifi_net_get_wep_password(wifi_net),
                        "key_index", nwamui_wifi_net_get_wep_key_index(wifi_net),
                        NULL);
                break;
#if 0
            /* Currently ENTERPRISE is not supported */
            case NWAMUI_WIFI_SEC_WPA_ENTERPRISE:
                nwamui_wifi_wpa_config_t wpa_config_type = nwamui_wifi_net_get_wpa_config(wifi_net);

                g_object_set(G_OBJECT (self),
                        "wpa_config_type", wpa_config_type,
                        "wpa_username", nwamui_wifi_net_get_wpa_username(wifi_net),
                        "wpa_password", nwamui_wifi_net_get_wpa_password(wifi_net),
                        NULL);

                switch( wpa_config_type ) {
                    case NWAMUI_WIFI_WPA_CONFIG_AUTOMATIC:
                        break;
                    case NWAMUI_WIFI_WPA_CONFIG_LEAP:
                    case NWAMUI_WIFI_WPA_CONFIG_PEAP:
                        g_object_set(G_OBJECT (self),
                                "wpa_cert_file", nwamui_wifi_net_get_wpa_cert_file(wifi_net),
                                NULL);
                        break;
                }
                break;
#endif
            default:
                break;
        }
    }
    else {
        g_object_set(G_OBJECT (self),
                "essid", "",
                "bssid_list", NULL,
                "security", NWAMUI_WIFI_SEC_NONE,
                NULL );
    }
}


/**
 * nwam_wireless_dialog_get_ncu:
 * @nwam_wireless_dialog: a #NwamWirelessDialog.
 * @returns: the NCU currently being configured by the @nwam_wireless_dialog.
 *
 * Gets the Wireless NCU currently being configured by the NWAM Wireless Dialog.
 **/
NwamuiNcu*
nwam_wireless_dialog_get_ncu (NwamWirelessDialog *self)
{
    NwamuiNcu   *ncu = NULL;

    g_return_val_if_fail (NWAM_IS_WIRELESS_DIALOG (self), NULL);
    
    g_object_get (G_OBJECT (self),
                  "ncu", &ncu,
                  NULL);

    return( ncu );
}

void                    
nwam_wireless_dialog_set_essid (NwamWirelessDialog  *self,
                                const gchar         *essid )
{
    g_return_if_fail (NWAM_IS_WIRELESS_DIALOG (self));
    g_assert (essid != NULL );

    if ( essid != NULL ) {
        g_object_set (G_OBJECT (self),
                      "essid", essid,
                      NULL);
    }
}

gchar*                  
nwam_wireless_dialog_get_essid (NwamWirelessDialog *self )
{
    gchar*  essid = NULL;
    
    g_return_val_if_fail (NWAM_IS_WIRELESS_DIALOG (self), essid);

    g_object_get (G_OBJECT (self),
                  "essid", &essid,
                  NULL);
    return( essid );
}

void                    
nwam_wireless_dialog_set_security (NwamWirelessDialog  *self,
                                   nwamui_wifi_security_t   security )
{
    g_return_if_fail (NWAM_IS_WIRELESS_DIALOG (self));
    g_assert (security >= 0 && security < NWAMUI_WIFI_SEC_LAST);

    g_object_set (G_OBJECT (self),
                  "security", (gint)security,
                   NULL);
}

nwamui_wifi_security_t                    
nwam_wireless_dialog_get_security (NwamWirelessDialog *self )
{
    gint security = NWAMUI_WIFI_SEC_NONE;
    
    g_return_val_if_fail (NWAM_IS_WIRELESS_DIALOG (self), security);

    g_object_get (G_OBJECT (self),
                  "security", &security,
                  NULL);
    return( (nwamui_wifi_security_t)security );
}

void                    
nwam_wireless_dialog_set_key (NwamWirelessDialog  *self,
                              const gchar         *key )
{
    g_return_if_fail (NWAM_IS_WIRELESS_DIALOG (self));
    g_assert (key != NULL );

    if ( key != NULL ) {
        g_object_set (G_OBJECT (self),
                      "key", key,
                      NULL);
    }
}

gchar*                  
nwam_wireless_dialog_get_key (NwamWirelessDialog *self )
{
    gchar*  key = NULL;
    
    g_return_val_if_fail (NWAM_IS_WIRELESS_DIALOG (self), key);

    g_object_get (G_OBJECT (self),
                  "key", &key,
                  NULL);
    return( key );
}

void
nwam_wireless_dialog_set_key_index (NwamWirelessDialog  *self,
  guint key_index )
{
    g_return_if_fail (NWAM_IS_WIRELESS_DIALOG (self));
    g_return_if_fail (key_index >= 1 && key_index <= 4);

    g_object_set (G_OBJECT (self),
                  "key_index", key_index,
                  NULL);

}

guint
nwam_wireless_dialog_get_key_index (NwamWirelessDialog *self )
{
    gint  key = 1;
    
    g_return_val_if_fail (NWAM_IS_WIRELESS_DIALOG (self), key);

    g_object_get (G_OBJECT (self),
                  "key_index", &key,
                  NULL);
    return( key );
}

void                    
nwam_wireless_dialog_set_wpa_username (NwamWirelessDialog  *self,
                                       const gchar         *wpa_username )
{
    g_return_if_fail (NWAM_IS_WIRELESS_DIALOG (self));
    g_assert (wpa_username != NULL );

    if ( wpa_username != NULL ) {
        g_object_set (G_OBJECT (self),
                      "wpa_username", wpa_username,
                      NULL);
    }
}

gchar*                  
nwam_wireless_dialog_get_wpa_username (NwamWirelessDialog *self )
{
    gchar*  wpa_username = NULL;
    
    g_return_val_if_fail (NWAM_IS_WIRELESS_DIALOG (self), wpa_username);

    g_object_get (G_OBJECT (self),
                  "wpa_username", &wpa_username,
                  NULL);
    return( wpa_username );
}

void                    
nwam_wireless_dialog_set_wpa_password (NwamWirelessDialog  *self,
                                const gchar         *wpa_password )
{
    g_return_if_fail (NWAM_IS_WIRELESS_DIALOG (self));
    g_assert (wpa_password != NULL );

    if ( wpa_password != NULL ) {
        g_object_set (G_OBJECT (self),
                      "wpa_password", wpa_password,
                      NULL);
    }
}

gchar*                  
nwam_wireless_dialog_get_wpa_password (NwamWirelessDialog *self )
{
    gchar*  wpa_password = NULL;
    
    g_return_val_if_fail (NWAM_IS_WIRELESS_DIALOG (self), wpa_password);

    g_object_get (G_OBJECT (self),
                  "wpa_password", &wpa_password,
                  NULL);
    return( wpa_password );
}

void                    
nwam_wireless_dialog_set_wpa_config_type (NwamWirelessDialog   *self,
                                          nwamui_wifi_wpa_config_t  wpa_config_type )
{
    g_return_if_fail (NWAM_IS_WIRELESS_DIALOG (self));
    g_assert (wpa_config_type >= 0 && wpa_config_type < NWAMUI_WIFI_WPA_CONFIG_LAST);
    
    g_object_set (G_OBJECT (self),
                  "wpa_config_type", (gint)wpa_config_type,
                  NULL);
}

nwamui_wifi_wpa_config_t                 
nwam_wireless_dialog_get_wpa_config_type (NwamWirelessDialog *self )
{
    gint wpa_config_type = NWAMUI_WIFI_WPA_CONFIG_AUTOMATIC;
    
    g_return_val_if_fail (NWAM_IS_WIRELESS_DIALOG (self), wpa_config_type);

    g_object_get (G_OBJECT (self),
                  "wpa_config_type", &wpa_config_type,
                  NULL);
    
    return( (nwamui_wifi_wpa_config_t)wpa_config_type );
}

void                    
nwam_wireless_dialog_set_leap_username (NwamWirelessDialog  *self,
                                        const gchar         *leap_username )
{
    g_return_if_fail (NWAM_IS_WIRELESS_DIALOG (self));
    g_assert (leap_username != NULL );

    if ( leap_username != NULL ) {
        g_object_set (G_OBJECT (self),
                      "leap_username", leap_username,
                      NULL);
    }
}

gchar*                  
nwam_wireless_dialog_get_leap_username (NwamWirelessDialog *self )
{
    gchar*  leap_username = NULL;
    
    g_return_val_if_fail (NWAM_IS_WIRELESS_DIALOG (self), leap_username);

    g_object_get (G_OBJECT (self),
                  "leap_username", &leap_username,
                  NULL);
    return( leap_username );
}

void                    
nwam_wireless_dialog_set_leap_password (NwamWirelessDialog  *self,
                                        const gchar         *leap_password )
{
    g_return_if_fail (NWAM_IS_WIRELESS_DIALOG (self));
    g_assert (leap_password != NULL );

    if ( leap_password != NULL ) {
        g_object_set (G_OBJECT (self),
                      "leap_password", leap_password,
                      NULL);
    }
}

gchar*                  
nwam_wireless_dialog_get_leap_password (NwamWirelessDialog *self )
{
    gchar*  leap_password = NULL;
    
    g_return_val_if_fail (NWAM_IS_WIRELESS_DIALOG (self), leap_password);

    g_object_get (G_OBJECT (self),
                  "leap_password", &leap_password,
                  NULL);
    return( leap_password );
}

void                    
nwam_wireless_dialog_set_bssid_list (NwamWirelessDialog  *self,
                                     GList               *bssid_list )
{
    g_return_if_fail (NWAM_IS_WIRELESS_DIALOG (self));
    g_assert (bssid_list != NULL );

    if ( bssid_list != NULL ) {
        g_object_set (G_OBJECT (self),
                      "bssid_list", bssid_list,
                      NULL);
    }
}
                                
GList*                  
nwam_wireless_dialog_get_bssid_list (NwamWirelessDialog *self )
{
    GList*  bssid_list = NULL;
    
    g_return_val_if_fail (NWAM_IS_WIRELESS_DIALOG (self), bssid_list);

    g_object_get (G_OBJECT (self),
                  "bssid_list", &bssid_list,
                  NULL);
    return( bssid_list );
}

void                    
nwam_wireless_dialog_set_persistant (NwamWirelessDialog  *self,
                                    gboolean             persistant)
{
    g_return_if_fail (NWAM_IS_WIRELESS_DIALOG (self));

    g_object_set (G_OBJECT (self),
                  "persistant", persistant,
                  NULL);
}
                                    
gboolean                
nwam_wireless_dialog_get_persistant (NwamWirelessDialog *self )
{
    gboolean persistant = FALSE;
    
    g_return_val_if_fail (NWAM_IS_WIRELESS_DIALOG (self), persistant);

    g_object_get (G_OBJECT (self),
                  "persistant", &persistant,
                  NULL);
    return( persistant );
}

void                    
nwam_wireless_dialog_set_do_connect (NwamWirelessDialog  *self,
                                    gboolean             do_connect)
{
    g_return_if_fail (NWAM_IS_WIRELESS_DIALOG (self));

    g_object_set (G_OBJECT (self),
                  "do_connect", do_connect,
                  NULL);
}
                                    
gboolean                
nwam_wireless_dialog_get_do_connect (NwamWirelessDialog *self )
{
    gboolean do_connect = FALSE;
    
    g_return_val_if_fail (NWAM_IS_WIRELESS_DIALOG (self), do_connect);

    g_object_get (G_OBJECT (self),
                  "do_connect", &do_connect,
                  NULL);
    return( do_connect );
}

static gboolean
help(NwamPrefIFace *iface, gpointer user_data)
{
    nwamui_util_show_help(HELP_REF_JOIN_NETWORK);
}

/**
 * nwam_wireless_dialog_run:
 * @nwam_wireless_dialog: a #NwamWirelessDialog.
 * @returns: a GTK_DIALOG Response ID
 *
 * 
 * Blocks in a recursive main loop until the dialog either emits the response signal, or is destroyed.
 **/
static gint
dialog_run(NwamPrefIFace *iface, GtkWindow *parent)
{
    NwamWirelessDialog *self = NWAM_WIRELESS_DIALOG(iface);
    gint            response = GTK_RESPONSE_NONE;

    g_assert(NWAM_IS_WIRELESS_DIALOG (self));
    
    if (self->prv->wifi_net == NULL ) {
        g_object_set(G_OBJECT (self),
                "essid", "",
                "bssid_list", NULL,
                "security", NWAMUI_WIFI_SEC_NONE,
                NULL );
    }
    
    if ( self->prv->wireless_dialog != NULL ) {
        self->prv->key_entry_changed = FALSE; /* Mark key entry as unchanged */
        self->prv->sec_mode_changed = FALSE; /* Mark sec_mode as unchanged */

        if ( self->prv->title == NWAMUI_WIRELESS_DIALOG_TITLE_EDIT &&
             self->prv->wifi_net != NULL ) {
            /* Don't allow user to edit the text if it's not possible */
            gboolean    editable = nwamui_object_can_rename( NWAMUI_OBJECT(self->prv->wifi_net) );

            gtk_widget_set_sensitive(( GTK_WIDGET(self->prv->essid_combo) ), editable );
        }
        else {
            gtk_widget_set_sensitive(( GTK_WIDGET(self->prv->essid_combo) ), TRUE );
        }



        /* Don't show the persist flag if we're not connecting, since it's
         * meaningless in that case. */
        if ( self->prv->do_connect ) {
            gtk_widget_show(GTK_WIDGET(self->prv->persistant_cbutton) );
        }
        else {
            gtk_widget_hide(GTK_WIDGET(self->prv->persistant_cbutton) );
        }

        response = gtk_dialog_run(GTK_DIALOG(self->prv->wireless_dialog));
        
        switch( response ) { 
            case GTK_RESPONSE_OK: {
                    gchar*                  essid = nwam_wireless_dialog_get_essid(self);
                    nwamui_wifi_security_t  security = nwam_wireless_dialog_get_security(self);
                    GList*                  bssid_list = nwam_wireless_dialog_get_bssid_list(self);

                    if ( self->prv->wifi_net != NULL ) {
                        g_object_set(G_OBJECT(self->prv->wifi_net),
                                    "essid", essid,
                                    "fav_bssid_list", bssid_list,
                                    "security", security,
                                    NULL);
                    }
                    else {
                        self->prv->wifi_net = nwamui_wifi_net_new(
                                    self->prv->ncu,
                                    essid,
                                    security,
                                    bssid_list,
                                    NWAMUI_WIFI_BSS_TYPE_AUTO);
                    }

                    switch ( nwam_wireless_dialog_get_security(self) ) {
                        case NWAMUI_WIFI_SEC_NONE: 
                            break;

#ifdef WEP_ASCII_EQ_HEX 
                        case NWAMUI_WIFI_SEC_WEP:
#else
                        case NWAMUI_WIFI_SEC_WEP_HEX:
                        case NWAMUI_WIFI_SEC_WEP_ASCII:
#endif /* WEP_ASCII_EQ_HEX */
                        case NWAMUI_WIFI_SEC_WPA_PERSONAL: {
                                /* Only set the key(s) if the security mode or the actual
                                 * key value was changed.
                                 */
                                if ( self->prv->sec_mode_changed || self->prv->key_entry_changed ) {
                                    gchar*  passwd = nwam_wireless_dialog_get_key(self);

                                    g_object_set(G_OBJECT (self->prv->wifi_net),
                                            "wep_password", passwd,
                                            NULL);

                                    nwamui_wifi_net_store_key(self->prv->wifi_net); 

                                    if ( passwd ) {
                                        g_free(passwd);
                                    }
                                }
#ifdef WEP_ASCII_EQ_HEX 
                                if (nwam_wireless_dialog_get_security(self) == NWAMUI_WIFI_SEC_WEP) {
#else
                                if (nwam_wireless_dialog_get_security(self) == NWAMUI_WIFI_SEC_WEP_HEX) ||
                                   (nwam_wireless_dialog_get_security(self) == NWAMUI_WIFI_SEC_WEP_ASCII) {
#endif /* WEP_ASCII_EQ_HEX */
                                    g_object_set(G_OBJECT (self->prv->wifi_net),
                                      "wep_key_index", nwam_wireless_dialog_get_key_index(self),
                                      NULL);
                                }
                            }
                            break;
#if 0
                        /* Currently ENTERPRISE is not supported */
                        case NWAMUI_WIFI_SEC_WPA_ENTERPRISE:
                            nwamui_wifi_wpa_config_t wpa_config_type = nwamui_wifi_net_get_wpa_config(wifi_net);

                            /* FIXME: Make WPA_ENTERPRISE work when supported */
                            switch( wpa_config_type ) {
                                case NWAMUI_WIFI_WPA_CONFIG_AUTOMATIC:
                                    break;
                                case NWAMUI_WIFI_WPA_CONFIG_LEAP:
                                    break;
                                case NWAMUI_WIFI_WPA_CONFIG_PEAP:
                                    break;
                            }
                            break;
#endif
                        default:
                            break;
                    }

                    if ( self->prv->wifi_net != NULL && self->prv->do_connect ) {
                        gboolean add_to_fav = TRUE;

                        add_to_fav = nwam_wireless_dialog_get_persistant(self);

                        if ( self->prv->ncu ) {
                            nwamui_ncu_set_wifi_info(self->prv->ncu, self->prv->wifi_net);
                        }
                        nwamui_wifi_net_connect(self->prv->wifi_net, add_to_fav );
                    }

                    if ( essid ) {
                        g_free(essid);
                    }

                    if ( bssid_list ) {
                        g_list_foreach( bssid_list, (GFunc)g_free, NULL );
                        g_list_free(bssid_list);
                    }
                }
                break;
            default:
                if ( self->prv->wifi_net != NULL ) {
                    g_object_unref(self->prv->wifi_net);
                    self->prv->wifi_net = NULL;
                }
                break;
        }
        
        gtk_widget_hide( GTK_WIDGET(self->prv->wireless_dialog) );
    }
    
    return( response );
}

static GtkWindow* 
dialog_get_window(NwamPrefIFace *iface)
{
    NwamWirelessDialog *self = NWAM_WIRELESS_DIALOG(iface);

    if ( self->prv->wireless_dialog ) {
        return ( GTK_WINDOW(self->prv->wireless_dialog) );
    }

    return(NULL);
}

/**
 * nwam_wireless_dialog_show
 * @nwam_wireless_dialog: a #NwamWirelessDialog.
 * 
 * Shows the dialog
 **/
void
nwam_wireless_dialog_show (NwamWirelessDialog  *self)
{
    g_assert(NWAM_IS_WIRELESS_DIALOG (self));
    
    if ( self->prv->wireless_dialog != NULL ) {
        gtk_widget_show_all(GTK_WIDGET(self->prv->wireless_dialog));
    }
}

/**
 * nwam_wireless_dialog_hide
 * @nwam_wireless_dialog: a #NwamWirelessDialog.
 *
 * 
 * Hides the dialog
 **/
void
nwam_wireless_dialog_hide (NwamWirelessDialog  *self)
{
    g_assert(NWAM_IS_WIRELESS_DIALOG (self));
    
    if ( self->prv->wireless_dialog != NULL ) {
        gtk_widget_hide_all(GTK_WIDGET(self->prv->wireless_dialog));
    }
}

static void
nwam_wireless_dialog_finalize (NwamWirelessDialog *self)
{
    if ( self->prv->ncu != NULL )
        g_object_unref( G_OBJECT(self->prv->ncu) );

    gtk_widget_unref(GTK_WIDGET(self->prv->wireless_dialog ));
    gtk_widget_unref(GTK_WIDGET(self->prv->essid_combo ));
    gtk_widget_unref(GTK_WIDGET(self->prv->essid_cbentry ));
    gtk_widget_unref(GTK_WIDGET(self->prv->security_combo ));
    gtk_widget_unref(GTK_WIDGET(self->prv->password_notebook ));
    gtk_widget_unref(GTK_WIDGET(self->prv->key_entry ));
    gtk_widget_unref(GTK_WIDGET(self->prv->key_conf_entry ));
    gtk_widget_unref(GTK_WIDGET(self->prv->key_index_lbl ));
    gtk_widget_unref(GTK_WIDGET(self->prv->key_index_spinbtn ));
    gtk_widget_unref(GTK_WIDGET(self->prv->bssid_list_tv ));
    gtk_widget_unref(GTK_WIDGET(self->prv->bssid_add_btn ));
    gtk_widget_unref(GTK_WIDGET(self->prv->bssid_remove_btn ));
    gtk_widget_unref(GTK_WIDGET(self->prv->wpa_config_combo));
    gtk_widget_unref(GTK_WIDGET(self->prv->wpa_username_entry));
    gtk_widget_unref(GTK_WIDGET(self->prv->wpa_password_entry));
    /* gtk_widget_unref(GTK_WIDGET(self->prv->wpa_cert_file_entry)); TODO - Handle CERT FILE when supported */
    gtk_widget_unref(GTK_WIDGET(self->prv->persistant_cbutton ));
    gtk_widget_unref(GTK_WIDGET(self->prv->show_password_cbutton ));
 
    g_free (self->prv); 
    self->prv = NULL;

    parent_class->finalize (G_OBJECT (self));
}

#define _MSGBUFSIZE     (2048)

/* Verify data */
static const gchar*
validate_information( NwamWirelessDialog* self )
{
    static gchar    ret_str[_MSGBUFSIZE];
    NwamuiWifiNet  *tmp_wifi = NULL;
    gchar          *error_prop = NULL;
    const char     *rval = NULL;
    nwamui_wifi_security_t security;
    gboolean        is_wep = FALSE;

    ret_str[0] = '\0'; /* For use where we need to printf things */

    if ( GTK_WIDGET_IS_SENSITIVE( GTK_WIDGET(self->prv->essid_cbentry) ) ) {
        const gchar * essid = gtk_entry_get_text(GTK_ENTRY(self->prv->essid_cbentry)); 

        if (essid != NULL && strlen(essid) > 0 ) {
            for ( const char *c = essid; c != NULL && *c != '\0'; c++ ) {
                if ( !g_ascii_isprint( *c ) ) {
                    return( _("The Wireless network name (ESSID) contains invalid (non-ASCII) characters.") );
                }
            }
        }
        else {
            return( _("The Wireless network name (ESSID) should not be blank.") );
        }
    }

    if ( GTK_WIDGET_IS_SENSITIVE( GTK_WIDGET(self->prv->bssid_list_tv) ) ) {
        GList          *blist = NULL;
        GtkTreeModel   *model = NULL;
        GtkTreeIter     iter;

        model = gtk_tree_view_get_model (self->prv->bssid_list_tv);

        if ( gtk_tree_model_get_iter_first( model, &iter ) ) {
            do {
                gchar*  string = NULL;
                gtk_tree_model_get(model, &iter, 0, &string, -1);

                if ( string != NULL ) {
                    struct ether_addr *ether = ether_aton( string );
                    
                    if ( ether == NULL ) {
                        snprintf(ret_str, _MSGBUFSIZE, _("Wireless AP (BSSID) - '%s' is not a valid address"), string  );

                        g_free(string);

                        return( ret_str );
                    }
                }
            }
            while ( gtk_tree_model_iter_next( model, &iter ) );
        }
    }

    /* Only validate the key if the security mode or the actual key value was
     * changed.
     */
    if ( self->prv->sec_mode_changed || self->prv->key_entry_changed ) {
        switch( security = nwam_wireless_dialog_get_security(self) ) {
            case NWAMUI_WIFI_SEC_NONE: 
                break;

#ifdef WEP_ASCII_EQ_HEX 
            case NWAMUI_WIFI_SEC_WEP:
#else
            case NWAMUI_WIFI_SEC_WEP_HEX:
            case NWAMUI_WIFI_SEC_WEP_ASCII:
#endif /* WEP_ASCII_EQ_HEX */
                is_wep = TRUE;
                /* Fall-through */
            case NWAMUI_WIFI_SEC_WPA_PERSONAL:
                {
                    const gchar* key =        gtk_entry_get_text(GTK_ENTRY(self->prv->key_entry));
                    const gchar *key_conf =   gtk_entry_get_text(GTK_ENTRY(self->prv->key_conf_entry));
                    gchar       *rval = NULL;
                    
                    if ( key == NULL && key_conf == NULL ) {    /* Same, so TRUE */
                        rval = NULL;
                    }
                    else if( key == NULL || key_conf == NULL ||
                             strcmp( key, key_conf ) != 0 ) { /* Different, so FALSE */
                        rval = _("Keys entered are different" );
                    }

                    if ( rval == NULL ) {
                        guint        len;
                        gboolean     is_all_hex;

                        len = strlen( key );
                        is_all_hex = TRUE;
                        for ( int i  = 0; i < len; i++ ) {
                            if ( !g_ascii_isxdigit( key[i] ) ) {
                                is_all_hex = FALSE;
                            }
                        }

                        if ( is_wep ) {
                            if ( len == 10 || len == 26 ) {
                                if ( !is_all_hex ) {
                                    rval = _("WEP keys of length 10 or 26 should be hexadecimal values only");
                                }
                            }
                            else if ( len != 5 && len != 13 ) {
                                rval = _("WEP keys should be either 5 or 13 ASCII characters,\nor a hexadecimal value of length 10 or 26");   
                            }
                        }
                        else { /* is WPA */
                            if ( len < 8 || len > 63 ) {
                                rval = _("WPA keys should be between 8 and 63 characters in length");
                            }
                        }
                    }

                    if ( rval != NULL ) {
                        return( rval );
                    }
                }
                break;
#if 0
            /* Currently ENTERPRISE is not supported */
            case NWAMUI_WIFI_SEC_WPA_ENTERPRISE:
                {
                    const gchar* username =   gtk_entry_get_text(GTK_ENTRY(self->prv->wpa_username_entry));
                    const gchar* password =   gtk_entry_get_text(GTK_ENTRY(self->prv->wpa_password_entry));  
                    
                    if ( username != NULL && strlen(username) > 0  && password != NULL ) {
                        return( NULL );
                    }
                    /* TODO - Determine more WPA Validation criteria when ENTERPRISE supported */
                }
                break;
#endif
        }   
    }

    if ( self->prv->wifi_net == NULL ) {
        NwamuiNcu *ncu;

        ncu = nwam_wireless_dialog_get_ncu(self);

        tmp_wifi = nwamui_wifi_net_new(
                ncu,
                nwam_wireless_dialog_get_essid(self),
                nwam_wireless_dialog_get_security(self),
                nwam_wireless_dialog_get_bssid_list(self),
                NWAMUI_WIFI_BSS_TYPE_AUTO );

        switch ( nwam_wireless_dialog_get_security(self) ) {
            case NWAMUI_WIFI_SEC_NONE: 
                break;

#ifdef WEP_ASCII_EQ_HEX 
            case NWAMUI_WIFI_SEC_WEP:
#else
            case NWAMUI_WIFI_SEC_WEP_HEX:
            case NWAMUI_WIFI_SEC_WEP_ASCII:
#endif /* WEP_ASCII_EQ_HEX */
            case NWAMUI_WIFI_SEC_WPA_PERSONAL:
                g_object_set(G_OBJECT (tmp_wifi),
                        "wep_password", nwam_wireless_dialog_get_key(self),
                        NULL);
                break;
#if 0
            /* Currently ENTERPRISE is not supported */
            case NWAMUI_WIFI_SEC_WPA_ENTERPRISE:
                nwamui_wifi_wpa_config_t wpa_config_type = nwamui_wifi_net_get_wpa_config(wifi_net);

                /* FIXME: Make this work when supported */
                switch( wpa_config_type ) {
                    case NWAMUI_WIFI_WPA_CONFIG_AUTOMATIC:
                        break;
                    case NWAMUI_WIFI_WPA_CONFIG_LEAP:
                        break;
                    case NWAMUI_WIFI_WPA_CONFIG_PEAP:
                        break;
                }
                break;
#endif
            default:
                break;
        }
    }
    else {
        tmp_wifi = NWAMUI_WIFI_NET(g_object_ref(self->prv->wifi_net));
    }

    if ( !nwamui_wifi_net_validate_favourite( tmp_wifi, &error_prop ) ) {
        snprintf(ret_str, _MSGBUFSIZE, _("Failed to validate wireless network due to an error with the property : %s"), error_prop );
        rval = ret_str;
    }
    else {
        rval = NULL;  /* All OK */
    }

    g_object_unref(tmp_wifi);

    return( rval );
}

/* Callbacks */
static void
show_password_cb( GtkToggleButton* widget, gpointer data )
{
    NwamWirelessDialog *self = NWAM_WIRELESS_DIALOG(data);
    gboolean active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
    
    g_assert( self != NULL );
    
    gtk_entry_set_visibility(GTK_ENTRY(self->prv->key_entry), active );
    gtk_entry_set_visibility(GTK_ENTRY(self->prv->key_conf_entry), active );
    gtk_entry_set_visibility(GTK_ENTRY(self->prv->wpa_password_entry), active );
}

static void
security_selection_cb( GtkWidget* widget, gpointer data )
{
    NwamWirelessDialog *self = NWAM_WIRELESS_DIALOG(data);
    gint active_index;
    
    g_assert( self != NULL );

    self->prv->sec_mode_changed = TRUE;

    active_index = gtk_combo_box_get_active(GTK_COMBO_BOX(widget) );
    switch( active_index ) {
        case NWAMUI_WIFI_SEC_NONE:
            gtk_notebook_set_current_page( GTK_NOTEBOOK( self->prv->password_notebook), WIRELESS_NOTEBOOK_WEP_PAGE);
            gtk_widget_set_sensitive( GTK_WIDGET(self->prv->key_entry), FALSE);
            gtk_widget_set_sensitive( GTK_WIDGET(self->prv->key_conf_entry), FALSE);
            gtk_widget_set_sensitive( GTK_WIDGET(self->prv->show_password_cbutton ), FALSE );
/*             gtk_widget_set_sensitive( GTK_WIDGET(self->prv->key_index_lbl ), FALSE ); */
/*             gtk_widget_set_sensitive( GTK_WIDGET(self->prv->key_index_spinbtn ), FALSE ); */
            gtk_widget_hide( GTK_WIDGET(self->prv->key_index_lbl ));
            gtk_widget_hide( GTK_WIDGET(self->prv->key_index_spinbtn ));
            break;
#ifdef WEP_ASCII_EQ_HEX 
#else
#endif /* WEP_ASCII_EQ_HEX */
#ifdef WEP_ASCII_EQ_HEX 
        case NWAMUI_WIFI_SEC_WEP:
#else
        case NWAMUI_WIFI_SEC_WEP_HEX:
        case NWAMUI_WIFI_SEC_WEP_ASCII:
#endif /* WEP_ASCII_EQ_HEX */
        case NWAMUI_WIFI_SEC_WPA_PERSONAL: {
            gtk_notebook_set_current_page( GTK_NOTEBOOK( self->prv->password_notebook), WIRELESS_NOTEBOOK_WEP_PAGE);
            gtk_widget_set_sensitive( GTK_WIDGET(self->prv->key_entry), TRUE);
            gtk_widget_set_sensitive( GTK_WIDGET(self->prv->key_conf_entry), TRUE);
            gtk_widget_set_sensitive( GTK_WIDGET(self->prv->show_password_cbutton ), TRUE );
#ifdef WEP_ASCII_EQ_HEX 
            if (active_index == NWAMUI_WIFI_SEC_WEP) {
#else
            if (active_index == NWAMUI_WIFI_SEC_WEP_ASCII ||
                active_index == NWAMUI_WIFI_SEC_WEP_HEX) {
#endif /* WEP_ASCII_EQ_HEX */
                gtk_widget_show( GTK_WIDGET(self->prv->key_index_lbl ));
                gtk_widget_show( GTK_WIDGET(self->prv->key_index_spinbtn ));
            } else {
                gtk_widget_hide( GTK_WIDGET(self->prv->key_index_lbl ));
                gtk_widget_hide( GTK_WIDGET(self->prv->key_index_spinbtn ));
            }
    }
            break;
#if 0
        /* Currently ENTERPRISE is not supported */
        case NWAMUI_WIFI_SEC_WPA_ENTERPRISE:
            gtk_notebook_set_current_page( GTK_NOTEBOOK( self->prv->password_notebook), WIRELESS_NOTEBOOK_WPA_PAGE);
            gtk_notebook_set_current_page( GTK_NOTEBOOK( self->prv->password_notebook), WIRELESS_NOTEBOOK_WPA_PAGE);
            gtk_widget_set_sensitive( GTK_WIDGET(self->prv->wpa_username_entry), TRUE);
            gtk_widget_set_sensitive( GTK_WIDGET(self->prv->wpa_password_entry), TRUE);
            /* default set to automatic */
            if (gtk_combo_box_get_active(self->prv->wpa_config_combo) == -1) {
                gtk_combo_box_set_active(self->prv->wpa_config_combo, 0);
            }
            break;
#endif
        default:
            break;
    }
}

static void
response_cb( GtkWidget* widget, gint responseid, gpointer data )
{
    NwamWirelessDialog* self = NWAM_WIRELESS_DIALOG(data);
    gboolean            stop_emission = FALSE;
    const gchar*        validation_error = NULL;
    
    switch (responseid) {
        case GTK_RESPONSE_NONE:
            g_debug("GTK_RESPONSE_NONE");
            break;
        case GTK_RESPONSE_DELETE_EVENT:
            g_debug("GTK_RESPONSE_DELETE_EVENT");
            break;
        case GTK_RESPONSE_OK:
            g_debug("GTK_RESPONSE_OK");
            validation_error = validate_information(self);
            stop_emission = (validation_error != NULL );
            if ( stop_emission ) {
                nwamui_util_show_message(GTK_WINDOW(gtk_widget_get_toplevel(widget)), GTK_MESSAGE_ERROR, 
                                                    _("Validation Failed."), 
                                                    validation_error, TRUE );
            }
            g_debug("Validation = %s", (!stop_emission)?"TRUE":"FALSE");
            break;
        case GTK_RESPONSE_CANCEL:
            g_debug("GTK_RESPONSE_CANCEL");
            gtk_widget_hide(GTK_WIDGET(self->prv->wireless_dialog));
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

static void
key_entry_changed_cb( GtkWidget* widget, gpointer data )
{
    NwamWirelessDialog *self = NWAM_WIRELESS_DIALOG(data);

    self->prv->key_entry_changed = TRUE;
}

/* ESSID Changed */
static void
essid_changed_cb( GtkWidget* widget, gpointer data )
{
    NwamWirelessDialog *self = NWAM_WIRELESS_DIALOG(data);
    GtkTreeModel       *model = NULL;
    GtkTreeIter         iter;
    gboolean            state = strlen(gtk_entry_get_text(GTK_ENTRY(widget))) != 0 ;
    gint                int_data;
    
    model = gtk_combo_box_get_model(GTK_COMBO_BOX(self->prv->essid_combo));
    if ( gtk_combo_box_get_active_iter(GTK_COMBO_BOX(self->prv->essid_combo), &iter ) ) {
        gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, 2, &int_data,  -1);
        nwam_wireless_dialog_set_security(self, (nwamui_wifi_security_t)int_data);
    }
    
    gtk_dialog_set_response_sensitive(GTK_DIALOG(self->prv->wireless_dialog), GTK_RESPONSE_OK, state );
}

/* Add BSSID Button */
static void
bssid_add_cb(GtkWidget *button, gpointer data)
{
  GtkTreeIter iter;
  GtkTreeView       *treeview = (GtkTreeView *)data;
  GtkTreeModel      *model = gtk_tree_view_get_model (treeview);
  GtkTreeSelection  *selection = gtk_tree_view_get_selection (treeview);
  GtkTreeViewColumn *col = gtk_tree_view_get_column(treeview, 0);
  GtkTreePath *path;

  gtk_list_store_append (GTK_LIST_STORE (model), &iter);
  gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, "", -1);

  /* Start editing it */
  path = gtk_tree_model_get_path (model, &iter);
  gtk_tree_view_set_cursor(treeview, path, col, TRUE);
  gtk_tree_path_free (path);
}

/* Remove BSSID Button */
static void
bssid_remove_cb(GtkWidget *widget, gpointer data)
{
  GtkTreeIter iter;
  GtkTreeView *treeview = (GtkTreeView *)data;
  GtkTreeModel *model = gtk_tree_view_get_model (treeview);
  GtkTreeSelection *selection = gtk_tree_view_get_selection (treeview);

  if (gtk_tree_selection_get_selected (selection, NULL, &iter)) {
      GtkTreePath *path;

      path = gtk_tree_model_get_path (model, &iter);
      gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
      gtk_tree_path_free (path);
    }
}

static void 
bssid_edited_cb (GtkCellRendererText *renderer, gchar *path,
                 gchar *new_text, gpointer user_data) 
{
    GtkTreeView        *view = GTK_TREE_VIEW(user_data);
	GtkTreeModel*       model = GTK_TREE_MODEL (gtk_tree_view_get_model(view));
	GtkTreeIter         iter;
	
	gtk_tree_model_get_iter_from_string (model, &iter, path);
	gtk_list_store_set(GTK_LIST_STORE(model), &iter, 0, new_text, -1);
}

static void
object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
    NwamWirelessDialog* self = NWAM_WIRELESS_DIALOG(data);

    g_debug("NwamWirelessDialog: notify %s changed", arg1->name);

    if ( g_ascii_strcasecmp(arg1->name, "ncu") == 0 ) {
        populate_essid_combo(self, GTK_COMBO_BOX_ENTRY(self->prv->essid_combo));
    }

}

