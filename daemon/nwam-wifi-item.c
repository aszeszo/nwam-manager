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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "nwam-menuitem.h"
#include "nwam-wifi-item.h"
#include "libnwamui.h"

typedef struct _NwamWifiItemPrivate NwamWifiItemPrivate;
#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj),   \
        NWAM_TYPE_WIFI_ITEM, NwamWifiItemPrivate))

struct _NwamWifiItemPrivate {
	NwamuiDaemon *daemon;
    gulong toggled_handler_id;
};

enum {
	PROP_ZERO,
};

static void nwam_wifi_item_set_property (GObject         *object,
  guint            prop_id,
  const GValue    *value,
  GParamSpec      *pspec);
static void nwam_wifi_item_get_property (GObject         *object,
  guint            prop_id,
  GValue          *value,
  GParamSpec      *pspec);
static void nwam_wifi_item_finalize (NwamWifiItem *self);

/* nwamui daemon signals */
static void connect_daemon_signals(GObject *self, NwamuiDaemon *daemon);
static void disconnect_daemon_signals(GObject *self, NwamuiDaemon *daemon);

/* nwamui wifi net signals */
static gint menu_wifi_item_compare(NwamMenuItem *self, NwamMenuItem *other);
static void connect_wifi_net_signals(NwamWifiItem *self, NwamuiWifiNet *wifi);
static void disconnect_wifi_net_signals(NwamWifiItem *self, NwamuiWifiNet *wifi);
static void sync_wifi_net(NwamWifiItem *self, NwamuiWifiNet *wifi_net, gpointer user_data);

static void on_nwam_wifi_toggled (GtkCheckMenuItem *item, gpointer data);
static void wifi_net_notify( GObject *gobject, GParamSpec *arg1, gpointer user_data);

G_DEFINE_TYPE(NwamWifiItem, nwam_wifi_item, NWAM_TYPE_MENU_ITEM)

static void
nwam_wifi_item_class_init (NwamWifiItemClass *klass)
{
	GObjectClass *gobject_class;
    NwamMenuItemClass *nwam_menu_item_class;

	gobject_class = G_OBJECT_CLASS (klass);
/* 	gobject_class->set_property = nwam_wifi_item_set_property; */
/* 	gobject_class->get_property = nwam_wifi_item_get_property; */
	gobject_class->finalize = (void (*)(GObject*)) nwam_wifi_item_finalize;
	
    nwam_menu_item_class = NWAM_MENU_ITEM_CLASS(klass);
    nwam_menu_item_class->connect_object = connect_wifi_net_signals;
    nwam_menu_item_class->disconnect_object = disconnect_wifi_net_signals;
    nwam_menu_item_class->sync_object = sync_wifi_net;
    nwam_menu_item_class->compare = menu_wifi_item_compare;

	g_type_class_add_private (klass, sizeof (NwamWifiItemPrivate));
}

static void
nwam_wifi_item_init (NwamWifiItem *self)
{
    NwamWifiItemPrivate *prv = GET_PRIVATE(self);

    /* nwamui ncp signals */
    {
        NwamuiDaemon *daemon = nwamui_daemon_get_instance ();
        NwamuiNcp *ncp = nwamui_daemon_get_active_ncp(daemon);

        connect_daemon_signals(G_OBJECT(self), daemon);

        g_object_unref(ncp);

        prv->daemon = daemon;

    }

    prv->toggled_handler_id = g_signal_connect(self,
      "toggled", G_CALLBACK(on_nwam_wifi_toggled), NULL);
}

GtkWidget *
nwam_wifi_item_new(NwamuiWifiNet *wifi)
{
  NwamWifiItem *item;
  gchar *path = NULL;

  item = g_object_new (NWAM_TYPE_WIFI_ITEM,
    "proxy-object", wifi,
    NULL);

  return GTK_WIDGET(item);
}

static void
nwam_wifi_item_finalize (NwamWifiItem *self)
{
    NwamWifiItemPrivate *prv = GET_PRIVATE(self);

    /* nwamui ncp signals */
    if ( prv->daemon ) {
        NwamuiNcp *ncp = nwamui_daemon_get_active_ncp(prv->daemon);

        disconnect_daemon_signals(G_OBJECT(self), prv->daemon);

        g_object_unref(ncp);

        g_object_unref(prv->daemon);
    }

	G_OBJECT_CLASS(nwam_wifi_item_parent_class)->finalize(G_OBJECT (self));
}

static void
nwam_wifi_item_set_property (GObject         *object,
			     guint            prop_id,
			     const GValue    *value,
			     GParamSpec      *pspec)
{
	NwamWifiItem *self = NWAM_WIFI_ITEM (object);
    NwamWifiItemPrivate *prv = GET_PRIVATE(self);

	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nwam_wifi_item_get_property (GObject         *object,
  guint            prop_id,
  GValue          *value,
  GParamSpec      *pspec)
{
	NwamWifiItem *self = NWAM_WIFI_ITEM (object);
    NwamWifiItemPrivate *prv = GET_PRIVATE(self);

	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
on_nwam_wifi_toggled (GtkCheckMenuItem *item, gpointer data)
{
    NwamWifiItem *self = NWAM_WIFI_ITEM(item);
    NwamWifiItemPrivate *prv = GET_PRIVATE(self);
    NwamuiNcp* ncp = nwamui_daemon_get_active_ncp (prv->daemon);
    NwamuiNcu *ncu = NULL;
    NwamuiWifiNet *wifi = NWAMUI_WIFI_NET(nwam_obj_proxy_get_proxy(NWAM_OBJ_PROXY_IFACE(self)));
    gchar *name;

    /* Should we temporary set active to false for self, and wait for
     * wifi_net_notify to update self? */
    g_signal_handler_block(self, prv->toggled_handler_id);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(self), FALSE);
    g_signal_handler_unblock(self, prv->toggled_handler_id);

/*     g_debug("******** toggled %s ***** status %s ***** wifi 0x%p ****", */
/*       gtk_action_get_name(item), */
/*       gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(action))?"on":"off", */
/*       wifi); */

    /* Always connect the wifi no matter if the action is active. */

    /* If the active ncu is NULL, it means that the daemon hasn't yet
     * selected anything, but that doesn't mean that it's not correct
     * to allow the user to select the wifi net, this would result in
     * the NCU selection by the daemon, but the wifi_net structure will
     * contain a pointer to it's originating NCU, so use that.
     *
     * If it is non-NULL, then make sure the NCU is up to date w.r.t. the
     * wifi_net.
     */
    ncu = nwamui_wifi_net_get_ncu ( wifi );
        
    name = nwamui_wifi_net_get_unique_name(wifi);

    if (ncu != NULL) {
        g_assert(nwamui_ncu_get_ncu_type(ncu) == NWAMUI_NCU_TYPE_WIRELESS);
        g_debug("Using wifi_net's own NCU 0x%X\n\tENABLE\tWlan 0x%p - %s", ncu, wifi, name);
        nwamui_wifi_net_connect(wifi, TRUE);
        g_object_unref(ncu);
    } else {
        g_warning("Orphan Wlan 0x%p - %s\n", wifi, name);
    }

    g_free(name);
    g_object_unref(ncp);
}

NwamuiWifiNet *
nwam_wifi_item_get_wifi (NwamWifiItem *self)
{
    NwamuiWifiNet *wifi;

    g_object_get(self, "proxy-object", &wifi, NULL);

    return wifi;
}

void
nwam_wifi_item_set_wifi (NwamWifiItem *self, NwamuiWifiNet *wifi)
{
    g_return_if_fail(NWAMUI_IS_WIFI_NET(wifi));

    g_object_set(self, "proxy-object", wifi, NULL);
}

static gint
menu_wifi_item_compare(NwamMenuItem *self, NwamMenuItem *other)
{
    gint ret;
    gint signal_self;
    gint signal_other;

    signal_self = (gint)nwamui_wifi_net_get_signal_strength(
            NWAMUI_WIFI_NET(nwam_obj_proxy_get_proxy(NWAM_OBJ_PROXY_IFACE(self))));
    signal_other = (gint)nwamui_wifi_net_get_signal_strength(
            NWAMUI_WIFI_NET(nwam_obj_proxy_get_proxy(NWAM_OBJ_PROXY_IFACE(other))));

    /* Reverse logic to sort descending, by signal strength. */
    ret = signal_other - signal_self;

    if (ret == 0)
        ret = nwamui_wifi_net_compare(NWAMUI_WIFI_NET(nwam_obj_proxy_get_proxy(NWAM_OBJ_PROXY_IFACE(self))), NWAMUI_WIFI_NET(nwam_obj_proxy_get_proxy(NWAM_OBJ_PROXY_IFACE(other))));

    return ret;
}

static void 
connect_daemon_signals(GObject *self, NwamuiDaemon *daemon)
{
}

static void 
disconnect_daemon_signals(GObject *self, NwamuiDaemon *daemon)
{
}

static void 
connect_wifi_net_signals(NwamWifiItem *self, NwamuiWifiNet *wifi)
{
    g_signal_connect (wifi, "notify",
      G_CALLBACK(wifi_net_notify), (gpointer)self);
}

static void 
disconnect_wifi_net_signals(NwamWifiItem *self, NwamuiWifiNet *wifi)
{
    g_signal_handlers_disconnect_matched(wifi,
      G_SIGNAL_MATCH_FUNC,
      0,
      NULL,
      NULL,
      (gpointer)wifi_net_notify,
      NULL);
}

static void
sync_wifi_net(NwamWifiItem *self, NwamuiWifiNet *wifi, gpointer user_data)
{
    wifi_net_notify(G_OBJECT(wifi), NULL, (gpointer)self);
}

static void 
wifi_net_notify( GObject *gobject, GParamSpec *arg1, gpointer user_data)
{
	NwamWifiItem *self = NWAM_WIFI_ITEM (user_data);
    NwamWifiItemPrivate *prv = GET_PRIVATE(self);
    NwamuiWifiNet *wifi = NWAMUI_WIFI_NET(gobject);
    GtkWidget *img = NULL;

    g_assert(self);

    {
        NwamuiDaemon* daemon = nwamui_daemon_get_instance();
        NwamuiNcp*    ncp = nwamui_daemon_get_active_ncp( daemon );

        gchar *label = nwamui_wifi_net_get_display_string(wifi, nwamui_ncp_get_wireless_link_num( ncp ) > 1);

        /* If there is any underscores we need to replace them with two since
         * otherwise it's interpreted as a mnemonic
         */
        label = nwamui_util_encode_menu_label( &label );

        menu_item_set_label(GTK_MENU_ITEM(self), label);
        g_free(label);
        g_object_unref(ncp);
        g_object_unref(daemon);
    }

    if (!arg1 || g_ascii_strcasecmp(arg1->name, "signal-strength") == 0) {

          img = gtk_image_new_from_pixbuf (nwamui_util_get_wireless_strength_icon(nwamui_wifi_net_get_signal_strength(wifi), NWAMUI_WIRELESS_ICON_TYPE_BARS, TRUE ));
          nwam_menu_item_set_widget(NWAM_MENU_ITEM(self), 0, img);

    }

    if (!arg1 || g_ascii_strcasecmp(arg1->name, "status") == 0) {

        g_signal_handler_block(self, prv->toggled_handler_id);
        switch (nwamui_wifi_net_get_status(wifi)) {
        case NWAMUI_WIFI_STATUS_CONNECTED:
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(self), TRUE);
            break;
        case NWAMUI_WIFI_STATUS_DISCONNECTED:
        case NWAMUI_WIFI_STATUS_FAILED:
        default:
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(self), FALSE);
            break;
        }
        g_signal_handler_unblock(self, prv->toggled_handler_id);

    }

    if (!arg1 || g_ascii_strcasecmp(arg1->name, "security") == 0) {

          img = gtk_image_new_from_pixbuf (nwamui_util_get_network_security_icon(nwamui_wifi_net_get_security (wifi), TRUE )); 
          nwam_menu_item_set_widget(NWAM_MENU_ITEM(self), 1, img);

    }

#if 0
    {
        gchar *name = nwamui_wifi_net_get_essid(wifi);
        g_debug("%s\n\tAction 0x%p update wifi 0x%p %s\n\t%s", __func__, self, wifi, gtk_action_get_name(GTK_ACTION(self)), nwamui_wifi_net_get_status(wifi) == NWAMUI_WIFI_STATUS_CONNECTED ? "Active" : "Deactive");
        g_free(name);
    }
#endif
}
