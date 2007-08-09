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
#include <strings.h>

#include "nwam_wireless_dialog.h"

/* Names of Widgets in Glade file */
#define WIRELESS_DIALOG_NAME                    "add_wireless_network"
#define WIRELESS_DIALOG_ESSID_CBENTRY           "ssid_combo_entry"
#define WIRELESS_DIALOG_SEC_COMBO               "security_combo"
#define WIRELESS_PASSWORD_NOTEBOOK              "password_notebook"
#define WIRELESS_DIALOG_WEP_KEY_ENTRY           "wep_password_entry"
#define WIRELESS_DIALOG_WEP_KEY_CONF_ENTRY      "wep_confirm_password_entry"
#define WIRELESS_DIALOG_BSSID_ENTRY             "bssid_entry"
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
        PROP_WPA_USERNAME,
        PROP_WPA_PASSWORD,
        PROP_WPA_CONFIG_TYPE,
        PROP_WPA_CERT_FILE,
        PROP_BSSID,
        PROP_TRANSIENT,
};

struct _NwamWirelessDialogPrivate {
        /* Widget Pointers */
	GtkDialog*              wireless_dialog;
        GtkComboBoxEntry*       essid_combo;
        GtkEntry*               essid_cbentry; /* Convienience for essid_combo */
        GtkComboBox*            security_combo;
        GtkNotebook*            password_notebook;
        GtkCheckButton*         transient_cbutton;
        GtkCheckButton*         show_password_cbutton;
        /* WEP Settings */
        GtkEntry*               key_entry;
        GtkEntry*               key_conf_entry;
        GtkEntry*               bssid_entry;
        /* WPA Settings */
        GtkComboBox*            wpa_config_combo;
        GtkEntry*		wpa_username_entry;
        GtkEntry*		wpa_password_entry;
        /* TODO - Handle CERT FILE */

        /* Other Data */
        gchar*          ncu;
        NwamuiWifiNet*  wifi_net;
};

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
static void security_selection_cb( GtkWidget* widget, gpointer data );
static void show_password_cb( GtkToggleButton* widget, gpointer data );



G_DEFINE_TYPE (NwamWirelessDialog, nwam_wireless_dialog, G_TYPE_OBJECT)

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
                                     g_param_spec_string ("ncu",
                                                          _("Wireless NCU"),
                                                          _("The Wireless NCU that the dialog is to work with."),
                                                          NULL,
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
                                     PROP_BSSID,
                                     g_param_spec_string ("bssid",
                                                          _("Wireless BSSID"),
                                                          _("The Wireless AP BSSID to connect to."),
                                                          NULL,
                                                          G_PARAM_READWRITE));
    g_object_class_install_property (gobject_class,
                                     PROP_TRANSIENT,
                                     g_param_spec_boolean ("transient",
                                                          _("Wireless Network is Transient"),
                                                          _("This Wireless Network shouldn't be remembered on permanently"),
                                                          NULL,
                                                          G_PARAM_READWRITE));
    
}


static void
nwam_wireless_dialog_init (NwamWirelessDialog *self)
{
    GtkTreeModel    *model = NULL;
    
    self->prv = g_new0 (NwamWirelessDialogPrivate, 1);
    
    /* Iniialise pointers to important widgets */
    self->prv->wireless_dialog =        GTK_DIALOG(nwamui_util_glade_get_widget(WIRELESS_DIALOG_NAME));
    self->prv->essid_combo =            GTK_COMBO_BOX_ENTRY(nwamui_util_glade_get_widget(WIRELESS_DIALOG_ESSID_CBENTRY));
    self->prv->essid_cbentry =          GTK_ENTRY(gtk_bin_get_child(GTK_BIN(self->prv->essid_combo)));
    self->prv->security_combo =         GTK_COMBO_BOX(nwamui_util_glade_get_widget(WIRELESS_DIALOG_SEC_COMBO));
    self->prv->password_notebook =      GTK_NOTEBOOK(nwamui_util_glade_get_widget(WIRELESS_PASSWORD_NOTEBOOK));
    self->prv->key_entry =              GTK_ENTRY(nwamui_util_glade_get_widget(WIRELESS_DIALOG_WEP_KEY_ENTRY));
    self->prv->key_conf_entry =         GTK_ENTRY(nwamui_util_glade_get_widget(WIRELESS_DIALOG_WEP_KEY_CONF_ENTRY));
    self->prv->bssid_entry =            GTK_ENTRY(nwamui_util_glade_get_widget(WIRELESS_DIALOG_BSSID_ENTRY));
    self->prv->wpa_config_combo =       GTK_COMBO_BOX(nwamui_util_glade_get_widget(WIRELESS_DIALOG_WPA_CONFIG_COMBO));
    self->prv->wpa_username_entry =     GTK_ENTRY(nwamui_util_glade_get_widget(WIRELESS_DIALOG_WPA_USERNAME_ENTRY));
    self->prv->wpa_password_entry =     GTK_ENTRY(nwamui_util_glade_get_widget(WIRELESS_DIALOG_WPA_PASSWORD_ENTRY));
    self->prv->transient_cbutton =      GTK_CHECK_BUTTON(nwamui_util_glade_get_widget(WIRELESS_DIALOG_PERSIST_CBUTTON));
    self->prv->show_password_cbutton =  GTK_CHECK_BUTTON(nwamui_util_glade_get_widget(WIRELESS_DIALOG_SHOW_PASSWORD_CBUTTON));
    
    /* Change the ESSID comboboxentry to use Text and an an image (for Secure/Open) */
    change_essid_cbentry_model(GTK_COMBO_BOX_ENTRY(self->prv->essid_combo));

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
    g_signal_connect(GTK_COMBO_BOX(self->prv->security_combo), "changed", (GCallback)security_selection_cb, (gpointer)self);
    g_signal_connect(GTK_TOGGLE_BUTTON(self->prv->show_password_cbutton), "toggled", (GCallback)show_password_cb, (gpointer)self);
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
    GdkPixbuf              *encrypted_icon, *open_icon, *used_pix;
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
    
    /* TODO - get REAL icons for drop-downlist, these are borrowed */
    icon_theme = gtk_icon_theme_get_default();
    encrypted_icon = gtk_icon_theme_load_icon (GTK_ICON_THEME(icon_theme),
				     "gnome-dev-wavelan-encrypted", 16, 0, NULL);
    open_icon = gtk_icon_theme_load_icon (GTK_ICON_THEME(icon_theme),
				       "gnome-dev-wavelan", 16, 0, NULL);
    
    essid = nwamui_wifi_net_get_essid(wifi_elem);
    sec = nwamui_wifi_net_get_security(wifi_elem);
    
    used_pix = ( sec != NWAMUI_WIFI_SEC_NONE) ? g_object_ref (encrypted_icon) : g_object_ref (open_icon);
    gtk_list_store_append (GTK_LIST_STORE (model), &iter);
    gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                        0, essid, 1, used_pix, 2, (gint)sec, -1);
    g_object_unref (used_pix);
    
    /* g_free(essid); - TODO - should we free this here or not?? */
    g_object_unref (encrypted_icon);
    g_object_unref (open_icon);

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
    
    nwamui_daemon_wifi_start_scan(daemon);

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
        case PROP_NCU:
            tmpstr = (gchar *) g_value_get_string (value);

            g_assert( tmpstr != NULL );

            if (self->prv->ncu != NULL) {
                g_free( self->prv->ncu );
            }
            if ( tmpstr != NULL ) {
                self->prv->ncu = g_strdup(tmpstr);
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

            if (self->prv->key_entry != NULL && self->prv->key_conf_entry != NULL) {
                gtk_entry_set_text(GTK_ENTRY(self->prv->key_entry), tmpstr?tmpstr:"");
                gtk_entry_set_text(GTK_ENTRY(self->prv->key_conf_entry), tmpstr?tmpstr:"");
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
            /* TODO - Handle CERT FILE
            tmpstr = (gchar *) g_value_get_string (value);

            g_assert( tmpstr != NULL );

            if (self->prv->key_entry != NULL && self->prv->leap_password_entry != NULL) {
                if ( tmpstr != NULL ) {
                    gtk_entry_set_text(GTK_ENTRY(self->prv->leap_password_entry), tmpstr);
                }
            }
             */
            break;
        case PROP_BSSID:
            tmpstr = (gchar *) g_value_get_string (value);

            if (self->prv->bssid_entry != NULL) {
                gtk_entry_set_text(GTK_ENTRY(self->prv->bssid_entry), tmpstr?tmpstr:"");
            }
            break;
        case PROP_TRANSIENT:
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(self->prv->transient_cbutton), g_value_get_boolean (value));
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
            if (self->prv->ncu != NULL) {
                    g_value_set_string (value, self->prv->ncu);
            } else {
                    g_value_set_string (value, NULL);
            }
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
            /* TODO - Handle CERT FILE
            if (self->prv->leap_password_entry != NULL) {
                g_value_set_string( value, g_strdup(gtk_entry_get_text(GTK_ENTRY(self->prv->leap_password_entry))) );
            }
             */
            break;
        case PROP_BSSID:
            if (self->prv->bssid_entry != NULL) {
                g_value_set_string( value, g_strdup(gtk_entry_get_text(GTK_ENTRY(self->prv->bssid_entry))) );
            }
            break;
	case PROP_TRANSIENT:
            if (self->prv->transient_cbutton != NULL) {
                g_value_set_boolean (value, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(self->prv->transient_cbutton)) ) ;
            }
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
nwam_wireless_dialog_new (void)
{
	return NWAM_WIRELESS_DIALOG(g_object_new (NWAM_TYPE_WIRELESS_DIALOG, NULL));
}

/**
 * nwam_wireless_dialog_new_with_ncu:
 * @ncu: The Wireless NCU the dialog is to configure
 * @returns: a new #NwamWirelessDialog.
 *
 * Creates a new #NwamWirelessDialog to configure the given Wireless NCU. 
 **/
NwamWirelessDialog*
nwam_wireless_dialog_new_with_ncu (const gchar *ncu)
{
	NwamWirelessDialog *nwam_wireless_dialog;

	nwam_wireless_dialog = NWAM_WIRELESS_DIALOG(g_object_new (NWAM_TYPE_WIRELESS_DIALOG, NULL));

	g_object_set (G_OBJECT (nwam_wireless_dialog),
		      "ncu", ncu,
		      NULL);
	return NWAM_WIRELESS_DIALOG(nwam_wireless_dialog);
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
                              const gchar        *ncu )
{
    g_return_if_fail (NWAM_IS_WIRELESS_DIALOG (self));
    g_assert (ncu != NULL );

    if ( ncu != NULL ) {
        g_object_set (G_OBJECT (self),
                      "ncu", ncu,
                      NULL);
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
    self->prv->wifi_net = wifi_net;

    if ( wifi_net != NULL ) {
        g_object_set(G_OBJECT (self),
                "essid", nwamui_wifi_net_get_essid(wifi_net),
                "bssid", nwamui_wifi_net_get_bssid(wifi_net),
                "security", nwamui_wifi_net_get_security(wifi_net),
                "key", nwamui_wifi_net_get_wep_password(wifi_net),
                "wpa_config_type", nwamui_wifi_net_get_wpa_config(wifi_net),
                "wpa_username", nwamui_wifi_net_get_wpa_username(wifi_net),
                "wpa_password", nwamui_wifi_net_get_wpa_password(wifi_net),
                "wpa_cert_file", nwamui_wifi_net_get_wpa_cert_file(wifi_net),
                NULL);
    }
}


/**
 * nwam_wireless_dialog_get_ncu:
 * @nwam_wireless_dialog: a #NwamWirelessDialog.
 * @returns: the NCU currently being configured by the @nwam_wireless_dialog.
 *
 * Gets the Wireless NCU currently being configured by the NWAM Wireless Dialog.
 **/
gchar*
nwam_wireless_dialog_get_ncu (NwamWirelessDialog *self)
{
    g_return_val_if_fail (NWAM_IS_WIRELESS_DIALOG (self), NULL);

    if ( self->prv->ncu != NULL )
        return g_strdup(self->prv->ncu);
    else
        return NULL;
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
nwam_wireless_dialog_set_bssid (NwamWirelessDialog  *self,
                                const gchar         *bssid )
{
    g_return_if_fail (NWAM_IS_WIRELESS_DIALOG (self));
    g_assert (bssid != NULL );

    if ( bssid != NULL ) {
        g_object_set (G_OBJECT (self),
                      "bssid", bssid,
                      NULL);
    }
}
                                
gchar*                  
nwam_wireless_dialog_get_bssid (NwamWirelessDialog *self )
{
    gchar*  bssid = NULL;
    
    g_return_val_if_fail (NWAM_IS_WIRELESS_DIALOG (self), bssid);

    g_object_get (G_OBJECT (self),
                  "bssid", &bssid,
                  NULL);
    return( bssid );
}

void                    
nwam_wireless_dialog_set_transient (NwamWirelessDialog  *self,
                                    gboolean             transient)
{
    g_return_if_fail (NWAM_IS_WIRELESS_DIALOG (self));

    g_object_set (G_OBJECT (self),
                  "transient", transient,
                  NULL);
}
                                    
gboolean                
nwam_wireless_dialog_get_transient (NwamWirelessDialog *self )
{
    gboolean transient = FALSE;
    
    g_return_val_if_fail (NWAM_IS_WIRELESS_DIALOG (self), transient);

    g_object_get (G_OBJECT (self),
                  "transient", &transient,
                  NULL);
    return( transient );
}

/**
 * nwam_wireless_dialog_run:
 * @nwam_wireless_dialog: a #NwamWirelessDialog.
 * @returns: a GTK_DIALOG Response ID
 *
 * 
 * Blocks in a recursive main loop until the dialog either emits the response signal, or is destroyed.
 **/
gint       
nwam_wireless_dialog_run (NwamWirelessDialog  *self)
{
    gint            response = GTK_RESPONSE_NONE;

    g_assert(NWAM_IS_WIRELESS_DIALOG (self));
    
    if ( self->prv->wireless_dialog != NULL ) {
        response = gtk_dialog_run(GTK_DIALOG(self->prv->wireless_dialog));
        
        switch( response ) { 
            case GTK_RESPONSE_OK:
                if ( self->prv->wifi_net != NULL ) {
                    g_object_set(G_OBJECT(self->prv->wifi_net),
                                "essid", nwam_wireless_dialog_get_essid(self),
                                "security", nwam_wireless_dialog_get_security(self),
                                "bssid", nwam_wireless_dialog_get_bssid(self),
                                "wpa_config", nwam_wireless_dialog_get_wpa_config_type(self),
                                NULL);
                }
                else {
                    self->prv->wifi_net = nwamui_wifi_net_new(
                                nwam_wireless_dialog_get_essid(self),
                                nwam_wireless_dialog_get_security(self),
                                nwam_wireless_dialog_get_bssid(self),
                                "g",
                                (gint)g_random_int_range(0, 56), /* Random speed */
                                (nwamui_wifi_signal_strength_t)g_random_int_range((gint32)NWAMUI_WIFI_STRENGTH_NONE, (gint32)NWAMUI_WIFI_STRENGTH_EXCELLENT),
                                nwam_wireless_dialog_get_wpa_config_type(self)
                            );
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
        g_free (self->prv->ncu );
    
    gtk_widget_unref(GTK_WIDGET(self->prv->wireless_dialog ));
    gtk_widget_unref(GTK_WIDGET(self->prv->essid_combo ));
    gtk_widget_unref(GTK_WIDGET(self->prv->essid_cbentry ));
    gtk_widget_unref(GTK_WIDGET(self->prv->security_combo ));
    gtk_widget_unref(GTK_WIDGET(self->prv->password_notebook ));
    gtk_widget_unref(GTK_WIDGET(self->prv->key_entry ));
    gtk_widget_unref(GTK_WIDGET(self->prv->key_conf_entry ));
    gtk_widget_unref(GTK_WIDGET(self->prv->bssid_entry ));
    gtk_widget_unref(GTK_WIDGET(self->prv->wpa_config_combo));
    gtk_widget_unref(GTK_WIDGET(self->prv->wpa_username_entry));
    gtk_widget_unref(GTK_WIDGET(self->prv->wpa_password_entry));
    /* gtk_widget_unref(GTK_WIDGET(self->prv->wpa_cert_file_entry)); TODO - Handle CERT FILE */
    gtk_widget_unref(GTK_WIDGET(self->prv->transient_cbutton ));
    gtk_widget_unref(GTK_WIDGET(self->prv->show_password_cbutton ));
 
    g_free (self->prv); 
    self->prv = NULL;

    parent_class->finalize (G_OBJECT (self));
}

/* Verify data */
static gboolean
validate_information( NwamWirelessDialog* self )
{
    switch( nwam_wireless_dialog_get_security(self) ) {
        case NWAMUI_WIFI_SEC_NONE: 
            return( TRUE );

        case NWAMUI_WIFI_SEC_WEP_HEX:
        case NWAMUI_WIFI_SEC_WEP_ASCII:
            {
                const gchar* key =        gtk_entry_get_text(GTK_ENTRY(self->prv->key_entry));
                const gchar *key_conf =   gtk_entry_get_text(GTK_ENTRY(self->prv->key_conf_entry));
                
                if ( key == NULL && key_conf == NULL ) {    /* Same, so TRUE */
                    return( TRUE );
                }
                else if( key == NULL || key_conf == NULL ) { /* Different, so FALSE */
                    return( FALSE );
                }
                if ( g_ascii_strcasecmp( key, key_conf ) == 0 ) { /* Strings Match, so TRUE */
                    return( TRUE );
                }
            }
            break;
        case NWAMUI_WIFI_SEC_WPA_PERSONAL:
        case NWAMUI_WIFI_SEC_WPA_ENTERPRISE:
            {
                const gchar* username =   gtk_entry_get_text(GTK_ENTRY(self->prv->wpa_username_entry));
                const gchar* password =   gtk_entry_get_text(GTK_ENTRY(self->prv->wpa_password_entry));  
                
                if ( username != NULL && strlen(username) > 0  && password != NULL ) {
                    return( TRUE );
                }
                /* TODO - Determine more WPA Validation criteria */
            }
            break;
    }   
    
    /* All other conditions, FALSE */
    return( FALSE );
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
    
    g_assert( self != NULL );
    
    switch( gtk_combo_box_get_active(GTK_COMBO_BOX(widget) ) ) {
        case NWAMUI_WIFI_SEC_NONE:
            gtk_notebook_set_current_page( GTK_NOTEBOOK( self->prv->password_notebook), WIRELESS_NOTEBOOK_WEP_PAGE);
            gtk_widget_set_sensitive( GTK_WIDGET(self->prv->key_entry), FALSE);
            gtk_widget_set_sensitive( GTK_WIDGET(self->prv->key_conf_entry), FALSE);
            break;
        case NWAMUI_WIFI_SEC_WEP_HEX:
        case NWAMUI_WIFI_SEC_WEP_ASCII:
            gtk_notebook_set_current_page( GTK_NOTEBOOK( self->prv->password_notebook), WIRELESS_NOTEBOOK_WEP_PAGE);
            gtk_widget_set_sensitive( GTK_WIDGET(self->prv->key_entry), TRUE);
            gtk_widget_set_sensitive( GTK_WIDGET(self->prv->key_conf_entry), TRUE);
            break;
        case NWAMUI_WIFI_SEC_WPA_PERSONAL:
        case NWAMUI_WIFI_SEC_WPA_ENTERPRISE:
            gtk_notebook_set_current_page( GTK_NOTEBOOK( self->prv->password_notebook), WIRELESS_NOTEBOOK_WPA_PAGE);
            gtk_widget_set_sensitive( GTK_WIDGET(self->prv->key_entry), TRUE);
            gtk_widget_set_sensitive( GTK_WIDGET(self->prv->key_conf_entry), TRUE);
            break;
        default:
            break;
    }
}

static void
response_cb( GtkWidget* widget, gint responseid, gpointer data )
{
    NwamWirelessDialog* self = NWAM_WIRELESS_DIALOG(data);
    gboolean            stop_emission = FALSE;
    
    switch (responseid) {
        case GTK_RESPONSE_NONE:
            g_debug("GTK_RESPONSE_NONE");
            break;
        case GTK_RESPONSE_DELETE_EVENT:
            g_debug("GTK_RESPONSE_DELETE_EVENT");
            break;
        case GTK_RESPONSE_OK:
            g_debug("GTK_RESPONSE_OK");
            stop_emission = !validate_information(self);
            if ( stop_emission ) {
                /* TODO - Be more specific about WHY validation fails */
                nwamui_util_show_message(GTK_WINDOW(gtk_widget_get_toplevel(widget)), GTK_MESSAGE_ERROR, 
                                                    _("Validation Failed."), 
                                                    _("The configuration you specified is not valid"));
            }
            g_debug("Validation = %s", (!stop_emission)?"TRUE":"FALSE");
            break;
        case GTK_RESPONSE_CANCEL:
            g_debug("GTK_RESPONSE_CANCEL");
            gtk_widget_hide(GTK_WIDGET(self->prv->wireless_dialog));
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

static void
object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
    NwamWirelessDialog* self = NWAM_WIRELESS_DIALOG(data);

    g_debug("NwamWirelessDialog: notify %s changed\n", arg1->name);

    if ( g_ascii_strcasecmp(arg1->name, "ncu") == 0 ) {
        populate_essid_combo(self, GTK_COMBO_BOX_ENTRY(self->prv->essid_combo));
    }

}

