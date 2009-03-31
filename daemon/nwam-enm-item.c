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

#include "nwam-enm-item.h"
#include "nwam-obj-proxy-iface.h"
#include "libnwamui.h"

typedef struct _NwamEnmItemPrivate NwamEnmItemPrivate;
#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj),   \
        NWAM_TYPE_ENM_ITEM, NwamEnmItemPrivate))

struct _NwamEnmItemPrivate {
    NwamuiEnm *enm;
    gulong toggled_handler_id;
};

enum {
	PROP_ZERO,
	PROP_ENM,
	PROP_LWIDGET,
	PROP_RWIDGET,
};

static void nwam_obj_proxy_init(NwamObjProxyInterface *iface);
static GObject* get_proxy(NwamObjProxyIFace *iface);

static void nwam_enm_item_set_property (GObject         *object,
  guint            prop_id,
  const GValue    *value,
  GParamSpec      *pspec);
static void nwam_enm_item_get_property (GObject         *object,
  guint            prop_id,
  GValue          *value,
  GParamSpec      *pspec);
static void nwam_enm_item_finalize (NwamEnmItem *self);

/* nwamui daemon signals */
static void connect_daemon_signals(GObject *self, NwamuiDaemon *daemon);
static void disconnect_daemon_signals(GObject *self, NwamuiDaemon *daemon);
static void daemon_enm_selection_needed (NwamuiDaemon* daemon, NwamuiNcu* ncu, gpointer user_data);

/* nwamui enm net signals */
static void connect_enm_net_signals(NwamEnmItem *self, NwamuiEnm *enm);
static void disconnect_enm_net_signals(NwamEnmItem *self, NwamuiEnm *enm);
static void on_nwam_enm_toggled (GtkCheckMenuItem *item, gpointer data);
static void on_nwam_enm_notify( GObject *gobject, GParamSpec *arg1, gpointer data);

G_DEFINE_TYPE_EXTENDED(NwamEnmItem, nwam_enm_item,
  NWAM_TYPE_MENU_ITEM, 0,
  G_IMPLEMENT_INTERFACE(NWAM_TYPE_OBJ_PROXY_IFACE, nwam_obj_proxy_init))

static void
nwam_obj_proxy_init(NwamObjProxyInterface *iface)
{
    iface->get_proxy = get_proxy;
    iface->delete_notify = NULL;
}

static void
nwam_enm_item_class_init (NwamEnmItemClass *klass)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->set_property = nwam_enm_item_set_property;
	gobject_class->get_property = nwam_enm_item_get_property;
	gobject_class->finalize = (void (*)(GObject*)) nwam_enm_item_finalize;
	
	g_object_class_install_property (gobject_class,
      PROP_ENM,
      g_param_spec_object ("enm",
        N_("NwamuiEnm instance"),
        N_("Wireless AP"),
        NWAMUI_TYPE_ENM,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_type_class_add_private (klass, sizeof (NwamEnmItemPrivate));
}

static void
nwam_enm_item_init (NwamEnmItem *self)
{
    NwamEnmItemPrivate *prv = GET_PRIVATE(self);

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
      "toggled", G_CALLBACK(on_nwam_enm_toggled), NULL);

    /* Must set it initially, because there are no check elsewhere. */
    nwam_menu_item_set_widget(NWAM_MENU_ITEM(self), 0, gtk_label_new(""));
}

GtkWidget*
nwam_enm_item_new(NwamuiEnm *enm)
{
    GtkWidget *item;

    item = g_object_new (NWAM_TYPE_ENM_ITEM,
      "enm", enm,
      NULL);

    return item;
}

static void
nwam_enm_item_finalize (NwamEnmItem *self)
{
    NwamEnmItemPrivate *prv = GET_PRIVATE(self);
    int i;

    /* nwamui ncp signals */
    {
        NwamuiDaemon *daemon = nwamui_daemon_get_instance ();
        NwamuiNcp *ncp = nwamui_daemon_get_active_ncp(daemon);

        disconnect_daemon_signals(G_OBJECT(self), daemon);

        g_object_unref(ncp);
        g_object_unref(daemon);
    }

    if (prv->enm) {
        disconnect_enm_net_signals(self, prv->enm);
        g_object_unref(prv->enm);
    }

	G_OBJECT_CLASS(nwam_enm_item_parent_class)->finalize(G_OBJECT (self));
}

static void
nwam_enm_item_set_property (GObject         *object,
  guint            prop_id,
  const GValue    *value,
  GParamSpec      *pspec)
{
	NwamEnmItem *self = NWAM_ENM_ITEM (object);
    NwamEnmItemPrivate *prv = GET_PRIVATE(self);
    GObject *obj = g_value_dup_object (value);

	switch (prop_id) {
	case PROP_ENM:
        if (prv->enm != NWAMUI_ENM(obj)) {
            if (prv->enm) {
                /* remove signal callback */
                disconnect_enm_net_signals(self, prv->enm);

                g_object_unref(prv->enm);
            }
            prv->enm = NWAMUI_ENM(obj);

            /* connect signal callback */
            connect_enm_net_signals(self, prv->enm);

            /* initializing */
            on_nwam_enm_notify(G_OBJECT(prv->enm), NULL, (gpointer)self);
        }
        break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nwam_enm_item_get_property (GObject         *object,
  guint            prop_id,
  GValue          *value,
  GParamSpec      *pspec)
{
	NwamEnmItem *self = NWAM_ENM_ITEM (object);
    NwamEnmItemPrivate *prv = GET_PRIVATE(self);

	switch (prop_id)
	{
	case PROP_ENM:
		g_value_set_object (value, (GObject*) prv->enm);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
connect_enm_net_signals(NwamEnmItem *self, NwamuiEnm *enm)
{
    g_signal_connect (enm, "notify::active",
      G_CALLBACK(on_nwam_enm_notify), (gpointer)self);
}

static void
disconnect_enm_net_signals(NwamEnmItem *self, NwamuiEnm *enm)
{
    g_signal_handlers_disconnect_matched(enm,
      G_SIGNAL_MATCH_DATA,
      0,
      NULL,
      NULL,
      NULL,
      (gpointer)self);
}

static void
on_nwam_enm_toggled (GtkCheckMenuItem *item, gpointer data)
{
	NwamEnmItem *self = NWAM_ENM_ITEM (item);
    NwamEnmItemPrivate *prv = GET_PRIVATE(self);

    g_signal_handler_block(self, prv->toggled_handler_id);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(self), FALSE);
    g_signal_handler_unblock(self, prv->toggled_handler_id);

	if (nwamui_enm_get_active(prv->enm)) {
		nwamui_enm_set_active(prv->enm, FALSE);
	} else {
		nwamui_enm_set_active (prv->enm, TRUE);
    }
}

static void
on_nwam_enm_notify( GObject *gobject, GParamSpec *arg1, gpointer data)
{
	NwamEnmItem *self = NWAM_ENM_ITEM (data);
    NwamEnmItemPrivate *prv = GET_PRIVATE(self);
    NwamuiObject *object = NWAMUI_OBJECT(gobject);

    g_debug("menuitem get enm notify %s changed\n", (arg1 && arg1->name)?arg1->name:"NULL");
    g_assert(NWAMUI_IS_ENM(object));

    if (!arg1 ||
      g_ascii_strcasecmp(arg1->name, "active") == 0 ||
      g_ascii_strcasecmp(arg1->name, "name") == 0) {
        gchar *m_name;
        gchar *new_text;

        m_name = nwamui_object_get_name(object);

        if (nwamui_object_get_active(object)) {
            new_text = g_strconcat(_("Stop "), m_name, NULL);
        } else {
            new_text = g_strconcat(_("Start "), m_name, NULL);
        }

        /* If there is any underscores we need to replace them with two since
         * otherwise it's interpreted as a mnemonic
         */
        new_text = nwamui_util_encode_menu_label( &new_text );
        menu_item_set_label(GTK_MENU_ITEM(self), new_text);

        g_free (new_text);
        g_free (m_name);

    }

    if (!arg1 || g_ascii_strcasecmp(arg1->name, "activation_mode") == 0) {
        gchar *object_markup;
        GtkWidget *label;

        switch (nwamui_object_get_activation_mode(object)) {
        case NWAMUI_COND_ACTIVATION_MODE_MANUAL:
            object_markup = _("<b>M</b>");
            break;
        case NWAMUI_COND_ACTIVATION_MODE_SYSTEM:
            object_markup = _("<b>S</b>");
            break;
        case NWAMUI_COND_ACTIVATION_MODE_PRIORITIZED:
            object_markup = _("<b>P</b>");
            break;
        case NWAMUI_COND_ACTIVATION_MODE_CONDITIONAL_ANY:
            object_markup = _("<b>C</b>");
            break;
        case NWAMUI_COND_ACTIVATION_MODE_CONDITIONAL_ALL:
            object_markup = _("<b>A</b>");
            break;
        default:
            g_assert_not_reached();
            break;
        }

        label = nwam_menu_item_get_widget(NWAM_MENU_ITEM(self), 0);
        g_assert(GTK_IS_LABEL(label));
        gtk_label_set_markup(GTK_LABEL(label), object_markup);
    }
}

static GObject*
get_proxy(NwamObjProxyIFace *iface)
{
    NwamEnmItemPrivate *prv = GET_PRIVATE(iface);
    return G_OBJECT(prv->enm);
}

NwamuiEnm *
nwam_enm_item_get_enm (NwamEnmItem *self)
{
    NwamuiEnm *enm;

    g_object_get(self, "enm", &enm, NULL);

    return enm;
}

void
nwam_enm_item_set_enm (NwamEnmItem *self, NwamuiEnm *enm)
{
    g_return_if_fail(NWAMUI_IS_ENM(enm));

    g_object_set(self, "enm", enm, NULL);
}

static void 
connect_daemon_signals(GObject *self, NwamuiDaemon *daemon)
{
}

static void 
disconnect_daemon_signals(GObject *self, NwamuiDaemon *daemon)
{
}

