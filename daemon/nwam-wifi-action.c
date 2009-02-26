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
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "nwam-menuitem.h"
#include "nwam-wifi-action.h"
#include "nwam-obj-proxy-iface.h"
#include "libnwamui.h"

#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj),   \
        NWAM_TYPE_WIFI_ACTION, NwamWifiActionPrivate))

struct _NwamWifiActionPrivate {
	GtkWidget *w[MAX_WIDGET_NUM];
    NwamuiWifiNet *wifi;
	NwamuiDaemon *daemon;
    gulong toggled_handler_id;
};

enum {
	PROP_ZERO,
	PROP_WIFI,
	PROP_LWIDGET,
	PROP_RWIDGET,
};

static void nwam_obj_proxy_init(NwamObjProxyInterface *iface);
static GObject* get_proxy(NwamWifiAction *self);

static GtkWidget *create_menu_item (GtkAction *action);
static void connect_proxy        (GtkAction       *action, 
                                  GtkWidget       *proxy); 
static void disconnect_proxy     (GtkAction       *action, 
                                 GtkWidget       *proxy); 
static void nwam_wifi_action_set_property (GObject         *object,
  guint            prop_id,
  const GValue    *value,
  GParamSpec      *pspec);
static void nwam_wifi_action_get_property (GObject         *object,
  guint            prop_id,
  GValue          *value,
  GParamSpec      *pspec);
static void nwam_wifi_action_finalize (NwamWifiAction *self);

/* nwamui daemon signals */
static void connect_daemon_signals(GObject *self, NwamuiDaemon *daemon);
static void disconnect_daemon_signals(GObject *self, NwamuiDaemon *daemon);
static void daemon_wifi_selection_needed (NwamuiDaemon* daemon, NwamuiNcu* ncu, gpointer user_data);

/* nwamui wifi net signals */
static void connect_wifi_net_signals(NwamWifiAction *self, NwamuiWifiNet *wifi);
static void disconnect_wifi_net_signals(NwamWifiAction *self, NwamuiWifiNet *wifi);
static void on_nwam_wifi_toggled (GtkAction *action, gpointer data);
static void wifi_net_notify( GObject *gobject, GParamSpec *arg1, gpointer user_data);

G_DEFINE_TYPE_EXTENDED(NwamWifiAction, nwam_wifi_action,
  GTK_TYPE_TOGGLE_ACTION, 0,
  G_IMPLEMENT_INTERFACE(NWAM_TYPE_OBJ_PROXY_IFACE, nwam_obj_proxy_init))

static void
nwam_obj_proxy_init(NwamObjProxyInterface *iface)
{
    iface->get_proxy = get_proxy;
    iface->delete_notify = NULL;
}

static void
nwam_wifi_action_class_init (NwamWifiActionClass *klass)
{
	GObjectClass *gobject_class;
	GtkActionClass *action_class;

	gobject_class = G_OBJECT_CLASS (klass);
	action_class = GTK_ACTION_CLASS (klass);

	gobject_class->set_property = nwam_wifi_action_set_property;
	gobject_class->get_property = nwam_wifi_action_get_property;
	gobject_class->finalize = (void (*)(GObject*)) nwam_wifi_action_finalize;
	
	action_class->menu_item_type = NWAM_TYPE_MENU_ITEM;
/* 	action_class->toolbar_item_type = GTK_TYPE_TOGGLE_TOOL_BUTTON; */
	action_class->create_menu_item = create_menu_item;
	action_class->connect_proxy = connect_proxy;
	action_class->disconnect_proxy = disconnect_proxy;

	g_object_class_install_property (gobject_class,
      PROP_WIFI,
      g_param_spec_object ("wifi",
        N_("NwamuiWifiNet instance"),
        N_("Wireless AP"),
        NWAMUI_TYPE_WIFI_NET,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (gobject_class,
      PROP_LWIDGET,
      g_param_spec_object ("left-widget",
        N_("Left widget, ref'd"),
        N_("Left widget, ref'd"),
        GTK_TYPE_WIDGET,
        G_PARAM_READWRITE));
	g_object_class_install_property (gobject_class,
      PROP_RWIDGET,
      g_param_spec_object ("right-widget",
        N_("Right widget, ref'd"),
        N_("Right widget, ref'd"),
        GTK_TYPE_WIDGET,
        G_PARAM_READWRITE));

	g_type_class_add_private (klass, sizeof (NwamWifiActionPrivate));
}

static void
nwam_wifi_action_init (NwamWifiAction *self)
{
    NwamWifiActionPrivate *prv = GET_PRIVATE(self);
	self->prv = prv;

    /* nwamui ncp signals */
    {
        NwamuiDaemon *daemon = nwamui_daemon_get_instance ();
        NwamuiNcp *ncp = nwamui_daemon_get_active_ncp(daemon);
        NwamuiNcu *ncu = nwamui_ncp_get_active_ncu(ncp);

        connect_daemon_signals(G_OBJECT(self), daemon);

        if (ncu) {
            g_object_unref(ncu);
        }

        g_object_unref(ncp);
        g_object_unref(daemon);
    }

    prv->toggled_handler_id = g_signal_connect(self,
      "toggled", G_CALLBACK(on_nwam_wifi_toggled), NULL);
}

NwamWifiAction *
nwam_wifi_action_new(NwamuiWifiNet *wifi)
{
  NwamWifiAction *action;
  gchar *path = NULL;
  gchar *menu_text = NULL;

  menu_text = nwamui_wifi_net_get_display_string(NWAMUI_WIFI_NET(wifi), TRUE);
  path = nwamui_wifi_net_get_unique_name(NWAMUI_WIFI_NET(wifi));

  action = g_object_new (NWAM_TYPE_WIFI_ACTION,
    "name", path,
    "label", menu_text,
    "tooltip", NULL,
    "stock-id", NULL,
    "wifi", wifi,
    NULL);

  g_free(menu_text);
  g_free(path);

  return action;
}

static GtkWidget *
create_menu_item (GtkAction *action)
{
	NwamWifiAction *toggle_wifi_action = NWAM_WIFI_ACTION (action);
	
	return g_object_new(NWAM_TYPE_MENU_ITEM,
                        "draw-as-radio", TRUE,
                        NULL);
}

static void
nwam_wifi_action_finalize (NwamWifiAction *self)
{
    NwamWifiActionPrivate *prv = GET_PRIVATE(self);
    int i;

    /* nwamui ncp signals */
    {
        NwamuiDaemon *daemon = nwamui_daemon_get_instance ();
        NwamuiNcp *ncp = nwamui_daemon_get_active_ncp(daemon);

        disconnect_daemon_signals(G_OBJECT(self), daemon);

        g_object_unref(ncp);
        g_object_unref(daemon);
    }

    if (prv->wifi) {
        disconnect_wifi_net_signals(self, prv->wifi);
        g_object_unref(prv->wifi);
    }

    for (i = 0; i < MAX_WIDGET_NUM; i++) {
        if (prv->w[i]) {
            g_object_unref(G_OBJECT(prv->w[i]));
        }
    }

	G_OBJECT_CLASS(nwam_wifi_action_parent_class)->finalize(G_OBJECT (self));
}

static void
nwam_wifi_action_set_property (GObject         *object,
			     guint            prop_id,
			     const GValue    *value,
			     GParamSpec      *pspec)
{
	NwamWifiAction *self = NWAM_WIFI_ACTION (object);
    NwamWifiActionPrivate *prv = GET_PRIVATE(self);
    GObject *obj = g_value_dup_object (value);

	switch (prop_id) {
	case PROP_WIFI:
        if (prv->wifi != NWAMUI_WIFI_NET(obj)) {
            if (prv->wifi) {
                /* remove signal callback */
                disconnect_wifi_net_signals(self, prv->wifi);

                g_object_unref(prv->wifi);
            }
            prv->wifi = NWAMUI_WIFI_NET(obj);

            /* connect signal callback */
            connect_wifi_net_signals(self, prv->wifi);

            /* initializing */
            wifi_net_notify(G_OBJECT(prv->wifi), NULL, (gpointer)self);
        }
        break;
	case PROP_LWIDGET:
        g_return_if_fail(obj && G_IS_OBJECT (obj));

        if (prv->w[0] != GTK_WIDGET(obj)) {
            if (prv->w[0]) {
                g_object_unref(prv->w[0]);
            }
            prv->w[0] = GTK_WIDGET(obj);
        }
        break;
	case PROP_RWIDGET:
        g_return_if_fail(obj && G_IS_OBJECT (obj));

        if (prv->w[1] != GTK_WIDGET(obj)) {
            if (prv->w[1]) {
                g_object_unref(prv->w[1]);
            }
            prv->w[1] = GTK_WIDGET(obj);
        }
        break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nwam_wifi_action_get_property (GObject         *object,
  guint            prop_id,
  GValue          *value,
  GParamSpec      *pspec)
{
	NwamWifiAction *self = NWAM_WIFI_ACTION (object);
    NwamWifiActionPrivate *prv = GET_PRIVATE(self);

	switch (prop_id)
	{
	case PROP_WIFI:
		g_value_set_object (value, (GObject*) prv->wifi);
		break;
	case PROP_LWIDGET:
		g_value_set_object (value, (GObject*) prv->w[0]);
		break;
	case PROP_RWIDGET:
		g_value_set_object (value, (GObject*) prv->w[1]);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void 
connect_proxy (GtkAction *action,  
               GtkWidget *proxy) 
{
	NwamWifiAction *self = NWAM_WIFI_ACTION (action);
    NwamWifiActionPrivate *prv = GET_PRIVATE(self);

	(* GTK_ACTION_CLASS(nwam_wifi_action_parent_class)->connect_proxy) (action, proxy);
    if (NWAM_IS_MENU_ITEM (proxy)) {
        int i;

        for (i = 0; i < MAX_WIDGET_NUM; i++) {
            if (TRUE) {
                nwam_menu_item_set_widget(NWAM_MENU_ITEM(proxy), prv->w[i], i);
            }
        }
#if 0
        GList *childs;
        GList *idx;

        childs = gtk_container_get_children (proxy);
        for (idx = childs; idx; idx = idx->next) {
            g_debug ("connect_proxy found widget %s", gtk_widget_get_name (idx->data));
        }
#endif
    }
}

static void 
disconnect_proxy (GtkAction *action,  
                  GtkWidget *proxy) 
{ 
    if (NWAM_IS_MENU_ITEM (proxy)) {
#if 0
        GList *childs;
        GList *idx;

        childs = gtk_container_get_children (proxy);
        for (idx = childs; idx; idx = idx->next) {
            g_debug ("found widget %s", gtk_widget_get_name (idx->data));
        }
#endif
    }
	(* GTK_ACTION_CLASS(nwam_wifi_action_parent_class)->disconnect_proxy) (action, proxy);
} 

static void
on_nwam_wifi_toggled (GtkAction *action, gpointer data)
{
    NwamWifiAction *self = NWAM_WIFI_ACTION(action);
    NwamWifiActionPrivate *prv = GET_PRIVATE(self);
    NwamuiNcp* ncp = nwamui_daemon_get_active_ncp (prv->daemon);
    NwamuiNcu *ncu = NULL;
    NwamuiWifiNet *wifi = prv->wifi;
    gchar *name;

    /* Should we temporary set active to false for self, and wait for
     * wifi_net_notify to update self? */
    g_signal_handler_block(self, prv->toggled_handler_id);
    gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(self), FALSE);
    g_signal_handler_block(self, prv->toggled_handler_id);

    g_debug("******** toggled %s ***** status %s ***** wifi 0x%p ****",
      gtk_action_get_name(action),
      gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(action))?"on":"off",
      wifi);

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
        g_debug("Using wifi_net's own NCU 0x%X\n\tENABLE\tWlan 0x%p - %s\n", ncu, wifi, name);
        nwamui_wifi_net_connect(wifi);
        g_object_unref(ncu);
    } else {
        g_warning("Orphan Wlan 0x%p - %s\n", wifi, name);
    }

    g_free(name);
    g_object_unref(ncp);
}

static void
nwam_wifi_action_set_label(NwamWifiAction *self, const gchar *label)
{
    g_object_set(self, "label", label, NULL);
}

static GObject*
get_proxy(NwamWifiAction *self)
{
    return G_OBJECT(self->prv->wifi);
}

/** 
 * nwam_wifi_action_set_widget:
 * @nwam_action: a #NwamWifiAction.
 * @nwam: a widget to set as the nwam for the menu item.
 * 
 * Sets the nwam of @nwam_action to the given widget.
 * Note that it depends on the show-menu-images setting whether
 * the nwam will be displayed or not.
 **/ 
void
nwam_wifi_action_set_widget (NwamWifiAction *self,
  GtkWidget *widget,
  gint pos)
{
    NwamWifiActionPrivate *prv = GET_PRIVATE(self);

	g_return_if_fail(NWAM_IS_WIFI_ACTION (self));
    g_return_if_fail(widget && GTK_IS_WIDGET (widget));
	g_assert(pos >= 0 && pos < MAX_WIDGET_NUM);

    if (widget != prv->w[pos]) {
        if (prv->w[pos]) {
            g_object_unref(prv->w[pos]);
        }
        prv->w[pos] = GTK_WIDGET(g_object_ref(widget));
    }
}

/**
 * nwam_action_get_widget:
 * @nwam_action: a #NwamWifiAction.
 * @returns: the widget set as nwam of @nwam_action.
 *
 * Gets the widget that is currently set as the nwam of @nwam_action.
 * See nwam_action_set_nwam().
 **/
GtkWidget*
nwam_action_get_widget (NwamWifiAction *self,
  gint pos)
{
    NwamWifiActionPrivate *prv = GET_PRIVATE(self);

	g_return_val_if_fail (NWAM_IS_WIFI_ACTION (self), NULL);
	g_assert (pos >= 0 && pos < MAX_WIDGET_NUM);

    return prv->w[pos];
}

NwamuiWifiNet *
nwam_action_get_wifi (NwamWifiAction *self)
{
    NwamuiWifiNet *wifi;

    g_object_get(self, "wifi", &wifi, NULL);

    return wifi;
}

void
nwam_action_set_wifi (NwamWifiAction *self, NwamuiWifiNet *wifi)
{
    g_return_if_fail(NWAMUI_IS_WIFI_NET(wifi));

    g_object_set(self, "wifi", wifi, NULL);
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
connect_wifi_net_signals(NwamWifiAction *self, NwamuiWifiNet *wifi)
{
}

static void 
disconnect_wifi_net_signals(NwamWifiAction *self, NwamuiWifiNet *wifi)
{
}

static void 
wifi_net_notify( GObject *gobject, GParamSpec *arg1, gpointer user_data)
{
}
