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
 * File:   nwam_proxy_password_dialog.c
 *
 * Created on May 9, 2007, 11:01 AM
 * 
 */

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <strings.h>

#include "nwam_proxy_password_dialog.h"

/* Names of Widgets in Glade file */
#define PROXY_PASSWORD_DIALOG_NAME              "proxy_password_dialog"
#define PROXY_PASSWORD_DIALOG_USERNAME_ENTRY    "proxy_username_entry"
#define PROXY_PASSWORD_DIALOG_PASSWORD_ENTRY    "proxy_password_entry"
#define PROXY_PASSWORD_DIALOG_OK_BUTTON         "proxy_password_dialog_ok_btn"
#define PROXY_PASSWORD_DIALOG_CANCEL_BUTTON     "proxy_password_dialog_cancel_btn"
#define PROXY_PASSWORD_DIALOG_HELP_BUTTON       "proxy_password_dialog_help_btn"

static GObjectClass *parent_class = NULL;

enum {
    PROP_USERNAME = 1,
    PROP_PASSWORD,
};

struct _NwamProxyPasswordDialogPrivate {
    /* Widget Pointers */
    GtkDialog*   proxy_password_dialog;
    GtkEntry*    username_entry; 
    GtkEntry*	 password_entry;
    GtkButton*   ok_button;
    GtkButton*   cancel_button;
    GtkButton*   help_button;
};

static void nwam_proxy_password_dialog_set_property ( GObject         *object,
                                                guint            prop_id,
                                                const GValue    *value,
                                                GParamSpec      *pspec);

static void nwam_proxy_password_dialog_get_property ( GObject         *object,
                                                guint            prop_id,
                                                GValue          *value,
                                                GParamSpec      *pspec);

static void nwam_proxy_password_dialog_finalize (     NwamProxyPasswordDialog *self);


/* Callbacks */
static void object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);
static void response_cb( GtkWidget* widget, gint repsonseid, gpointer data );
static void username_changed_cb( GtkWidget* widget, gpointer data );
static void password_changed_cb( GtkWidget* widget, gpointer data );

G_DEFINE_TYPE (NwamProxyPasswordDialog, nwam_proxy_password_dialog, G_TYPE_OBJECT)

static void
nwam_proxy_password_dialog_class_init (NwamProxyPasswordDialogClass *klass)
{
    /* Pointer to GObject Part of Class */
    GObjectClass *gobject_class = (GObjectClass*) klass;
        
    /* Initialise Static Parent Class pointer */
    parent_class = g_type_class_peek_parent (klass);

    /* Override Some Function Pointers */
    gobject_class->set_property = nwam_proxy_password_dialog_set_property;
    gobject_class->get_property = nwam_proxy_password_dialog_get_property;
    gobject_class->finalize = (void (*)(GObject*)) nwam_proxy_password_dialog_finalize;

    /* Create some properties */
    g_object_class_install_property (gobject_class,
                                     PROP_USERNAME,
                                     g_param_spec_string ("username",
                                                          _("Proxy Username"),
                                                          _("The Proxy Username to use for authenticating proxies"),
                                                          NULL,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_PASSWORD,
                                     g_param_spec_string ("password",
                                                          _("Proxy Password"),
                                                          _("The Proxy Password to use for authenticating proxies"),
                                                          NULL,
                                                          G_PARAM_READWRITE));

}


static void
nwam_proxy_password_dialog_init (NwamProxyPasswordDialog *self)
{
    GtkTreeModel    *model = NULL;
    
    self->prv = g_new0 (NwamProxyPasswordDialogPrivate, 1);
    
    /* Iniialise pointers to important widgets */
    self->prv->proxy_password_dialog =  GTK_DIALOG(nwamui_util_glade_get_widget(PROXY_PASSWORD_DIALOG_NAME));
    self->prv->username_entry =         GTK_ENTRY(nwamui_util_glade_get_widget(PROXY_PASSWORD_DIALOG_USERNAME_ENTRY));
    self->prv->password_entry =         GTK_ENTRY(nwamui_util_glade_get_widget(PROXY_PASSWORD_DIALOG_PASSWORD_ENTRY));
    self->prv->ok_button =              GTK_BUTTON(nwamui_util_glade_get_widget(PROXY_PASSWORD_DIALOG_OK_BUTTON));
    self->prv->cancel_button =          GTK_BUTTON(nwamui_util_glade_get_widget(PROXY_PASSWORD_DIALOG_CANCEL_BUTTON));
    self->prv->help_button =            GTK_BUTTON(nwamui_util_glade_get_widget(PROXY_PASSWORD_DIALOG_HELP_BUTTON));
    
    g_signal_connect(G_OBJECT(self), "notify", (GCallback)object_notify_cb, (gpointer)self);
    g_signal_connect(GTK_ENTRY(self->prv->username_entry), "changed", (GCallback)username_changed_cb, (gpointer)self);
    g_signal_connect(GTK_ENTRY(self->prv->password_entry), "changed", (GCallback)password_changed_cb, (gpointer)self);
    g_signal_connect(GTK_DIALOG(self->prv->proxy_password_dialog), "response", (GCallback)response_cb, (gpointer)self);
}


static void
nwam_proxy_password_dialog_set_property ( GObject         *object,
                                    guint            prop_id,
                                    const GValue    *value,
                                    GParamSpec      *pspec)
{
    NwamProxyPasswordDialog *self = NWAM_PROXY_PASSWORD_DIALOG(object);
    gchar*      tmpstr = NULL;
    gint        tmpint = 0;

    switch (prop_id) {
        case PROP_USERNAME:
            tmpstr = (gchar *) g_value_get_string (value);

            if (self->prv->username_entry != NULL) {
                gtk_entry_set_text(GTK_ENTRY(self->prv->username_entry), tmpstr?tmpstr:"");
            }
            break;
        case PROP_PASSWORD:
            tmpstr = (gchar *) g_value_get_string (value);

            if (self->prv->password_entry != NULL) {
                gtk_entry_set_text(GTK_ENTRY(self->prv->password_entry), tmpstr?tmpstr:"");
            }
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
nwam_proxy_password_dialog_get_property (GObject         *object,
                                  guint            prop_id,
                                  GValue          *value,
                                  GParamSpec      *pspec)
{
    NwamProxyPasswordDialog *self = NWAM_PROXY_PASSWORD_DIALOG(object);

    switch (prop_id) {
	case PROP_USERNAME:
            if (self->prv->username_entry != NULL) {
                g_value_set_string( value, gtk_entry_get_text(GTK_ENTRY(self->prv->username_entry)) );
            }
            break;
	case PROP_PASSWORD:
            if (self->prv->password_entry != NULL) {
                g_value_set_string( value, gtk_entry_get_text(GTK_ENTRY(self->prv->password_entry)) );
            }
            break;
    default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
    }
}


/**
 * nwam_proxy_password_dialog_new:
 * @returns: a new #NwamProxyPasswordDialog.
 *
 * Creates a new #NwamProxyPasswordDialog with an empty ncu
 **/
NwamProxyPasswordDialog*
nwam_proxy_password_dialog_new (void)
{
	return NWAM_PROXY_PASSWORD_DIALOG(g_object_new (NWAM_TYPE_PROXY_PASSWORD_DIALOG, NULL));
}

void                    
nwam_proxy_password_dialog_set_username (NwamProxyPasswordDialog  *self,
                                const gchar         *username )
{
    g_return_if_fail (NWAM_IS_PROXY_PASSWORD_DIALOG (self));
    
    if ( username != NULL ) {
        g_object_set (G_OBJECT (self),
                      "username", username?username:"",
                      NULL);
    }
}

gchar*                  
nwam_proxy_password_dialog_get_username (NwamProxyPasswordDialog *self )
{
    gchar*  username = NULL;
    
    g_return_val_if_fail (NWAM_IS_PROXY_PASSWORD_DIALOG (self), username);

    g_object_get (G_OBJECT (self),
                  "username", &username,
                  NULL);
    return( username );
}

void                    
nwam_proxy_password_dialog_set_password (NwamProxyPasswordDialog  *self,
                                const gchar         *password )
{
    g_return_if_fail (NWAM_IS_PROXY_PASSWORD_DIALOG (self));

    if ( password != NULL ) {
        g_object_set (G_OBJECT (self),
                      "password", password?password:"",
                      NULL);
    }
}

gchar*                  
nwam_proxy_password_dialog_get_password (NwamProxyPasswordDialog *self )
{
    gchar*  password = NULL;
    
    g_return_val_if_fail (NWAM_IS_PROXY_PASSWORD_DIALOG (self), password);

    g_object_get (G_OBJECT (self),
                  "password", &password,
                  NULL);
    return( password );
}

/**
 * nwam_proxy_password_dialog_run:
 * @nwam_proxy_password_dialog: a #NwamProxyPasswordDialog.
 * @returns: a GTK_DIALOG Response ID
 *
 * 
 * Blocks in a recursive main loop until the dialog either emits the response signal, or is destroyed.
 **/
gint       
nwam_proxy_password_dialog_run (NwamProxyPasswordDialog  *self)
{
    gint            response = GTK_RESPONSE_NONE;

    g_assert(NWAM_IS_PROXY_PASSWORD_DIALOG (self));
    
    if ( self->prv->proxy_password_dialog != NULL ) {
        response = gtk_dialog_run(GTK_DIALOG(self->prv->proxy_password_dialog));
        
        switch( response ) { 
            case GTK_RESPONSE_OK:
                break;
            default:
                break;
        }
        
        gtk_widget_hide( GTK_WIDGET(self->prv->proxy_password_dialog) );
    }
    
    return( response );
}

static void
nwam_proxy_password_dialog_finalize (NwamProxyPasswordDialog *self)
{
    gtk_widget_unref(GTK_WIDGET(self->prv->proxy_password_dialog ));
    gtk_widget_unref(GTK_WIDGET(self->prv->username_entry ));
    gtk_widget_unref(GTK_WIDGET(self->prv->password_entry ));
    gtk_widget_unref(GTK_WIDGET(self->prv->ok_button ));
    gtk_widget_unref(GTK_WIDGET(self->prv->cancel_button ));
    gtk_widget_unref(GTK_WIDGET(self->prv->help_button ));
 
    g_free (self->prv); 
    self->prv = NULL;

    parent_class->finalize (G_OBJECT (self));
}

/* Callbacks */

static void
response_cb( GtkWidget* widget, gint responseid, gpointer data )
{
    NwamProxyPasswordDialog* self = NWAM_PROXY_PASSWORD_DIALOG(data);
    gboolean            stop_emission = FALSE;
    
    switch (responseid) {
        case GTK_RESPONSE_NONE:
            g_debug("GTK_RESPONSE_NONE");
            break;
        case GTK_RESPONSE_OK:
            g_debug("GTK_RESPONSE_OK");
            break;
		case GTK_RESPONSE_DELETE_EVENT:
			g_debug("GTK_RESPONSE_DELETE_EVENT");
            /* Fall through */
        case GTK_RESPONSE_CANCEL:
            g_debug("GTK_RESPONSE_CANCEL");
            gtk_widget_hide(GTK_WIDGET(self->prv->proxy_password_dialog));
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

/* Username Changed */
static void
username_changed_cb( GtkWidget* widget, gpointer data )
{
    NwamProxyPasswordDialog *self = NWAM_PROXY_PASSWORD_DIALOG(data);
}

/* Password changed */
static void
password_changed_cb( GtkWidget* widget, gpointer data )
{
    NwamProxyPasswordDialog *self = NWAM_PROXY_PASSWORD_DIALOG(data);
}

static void
object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
    NwamProxyPasswordDialog* self = NWAM_PROXY_PASSWORD_DIALOG(data);

    g_debug("NwamProxyPasswordDialog: notify %s changed", arg1->name);
}

