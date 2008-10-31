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

#include "nwam-enm-action.h"
#include "nwam-obj-proxy-iface.h"
#include "libnwamui.h"

#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj),   \
        NWAM_TYPE_ENM_ACTION, NwamEnmActionPrivate))

struct _NwamEnmActionPrivate {
    NwamuiEnm *enm;
};

enum {
	PROP_ZERO,
	PROP_ENM,
	PROP_LWIDGET,
	PROP_RWIDGET,
};

static void nwam_obj_proxy_init(NwamObjProxyInterface *iface);
static GObject* get_proxy(NwamEnmAction *self);

static void nwam_enm_action_set_property (GObject         *object,
  guint            prop_id,
  const GValue    *value,
  GParamSpec      *pspec);
static void nwam_enm_action_get_property (GObject         *object,
  guint            prop_id,
  GValue          *value,
  GParamSpec      *pspec);
static void nwam_enm_action_finalize (NwamEnmAction *self);

/* nwamui daemon signals */
static void connect_daemon_signals(GObject *self, NwamuiDaemon *daemon);
static void disconnect_daemon_signals(GObject *self, NwamuiDaemon *daemon);
static void daemon_enm_selection_needed (NwamuiDaemon* daemon, NwamuiNcu* ncu, gpointer user_data);

/* nwamui enm net signals */
static void connect_enm_net_signals(NwamEnmAction *self, NwamuiEnm *enm);
static void disconnect_enm_net_signals(NwamEnmAction *self, NwamuiEnm *enm);
static void on_nwam_enm_toggled (GtkAction *action, gpointer data);
static void on_nwam_enm_notify( GObject *gobject, GParamSpec *arg1, gpointer data);

G_DEFINE_TYPE_EXTENDED(NwamEnmAction, nwam_enm_action,
  GTK_TYPE_RADIO_ACTION, 0,
  G_IMPLEMENT_INTERFACE(NWAM_TYPE_OBJ_PROXY_IFACE, nwam_obj_proxy_init))

static void
nwam_obj_proxy_init(NwamObjProxyInterface *iface)
{
    iface->get_proxy = get_proxy;
    iface->delete_notify = NULL;
}

static void
nwam_enm_action_class_init (NwamEnmActionClass *klass)
{
	GObjectClass *gobject_class;
	GtkActionClass *action_class;

	gobject_class = G_OBJECT_CLASS (klass);
	action_class = GTK_ENM_ACTION_CLASS (klass);

	gobject_class->set_property = nwam_enm_action_set_property;
	gobject_class->get_property = nwam_enm_action_get_property;
	gobject_class->finalize = (void (*)(GObject*)) nwam_enm_action_finalize;
	
	g_object_class_install_property (gobject_class,
      PROP_ENM,
      g_param_spec_object ("enm",
        N_("NwamuiEnm instance"),
        N_("Wireless AP"),
        NWAMUI_TYPE_ENM,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_type_class_add_private (klass, sizeof (NwamEnmActionPrivate));
}

static void
nwam_enm_action_init (NwamEnmAction *self)
{
    NwamEnmActionPrivate *prv = GET_PRIVATE(self);
    GSList *group = NULL;

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
}

NwamEnmAction *
nwam_enm_action_new(NwamuiEnm *enm)
{
    NwamEnmAction *action;
    gchar *menu_text = NULL;

    menu_text = nwamui_enm_get_name(NWAMUI_ENM(enm));

    action = g_object_new (NWAM_TYPE_ENM_ACTION,
      "name", menu_text,
      "label", menu_text,
      "tooltip", NULL,
      "stock-id", NULL,
      "enm", enm,
      NULL);

    g_signal_connect (G_OBJECT(action), "activate",
      G_CALLBACK(on_nwam_enm_toggled), (gpointer)action);

    g_free(menu_text);

    return action;
}

static void
nwam_enm_action_finalize (NwamEnmAction *self)
{
    NwamEnmActionPrivate *prv = GET_PRIVATE(self);
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

	G_OBJECT_CLASS(nwam_enm_action_parent_class)->finalize(G_OBJECT (self));
}

static void
nwam_enm_action_set_property (GObject         *object,
  guint            prop_id,
  const GValue    *value,
  GParamSpec      *pspec)
{
	NwamEnmAction *self = NWAM_ENM_ACTION (object);
    NwamEnmActionPrivate *prv = GET_PRIVATE(self);
    GObject *obj = g_value_dup_object (value);

	switch (prop_id) {
	case PROP_ENM:
        if (prv->enm != NWAMUI_ENM_NET(obj)) {
            if (prv->enm) {
                /* remove signal callback */
                disconnect_enm_net_signals(self, prv->enm);

                g_object_unref(prv->enm);
            }
            prv->enm = NWAMUI_ENM_NET(obj);

            /* connect signal callback */
            connect_enm_net_signals(self, prv->enm);

            /* initializing */
            on_nwam_enm_notify(G_OBJECT(prv->enm), NULL, (gpointer)self);

            g_object_set (object, "value", (gint)prv->enm, NULL);
        }
        break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nwam_enm_action_get_property (GObject         *object,
  guint            prop_id,
  GValue          *value,
  GParamSpec      *pspec)
{
	NwamEnmAction *self = NWAM_ENM_ACTION (object);
    NwamEnmActionPrivate *prv = GET_PRIVATE(self);

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
connect_enm_net_signals(NwamEnmAction *self, NwamuiEnm *enm)
{
    g_signal_connect (enm, "notify::active",
      G_CALLBACK(on_nwam_enm_notify), (gpointer)self);
}

static void
disconnect_enm_net_signals(NwamEnmAction *self, NwamuiEnm *enm)
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
on_nwam_enm_toggled (GtkAction *action, gpointer data)
{
	NwamEnmAction *self = NWAM_ENM_ACTION (action);
    NwamEnmActionPrivate *prv = GET_PRIVATE(self);

	if (nwamui_enm_get_active(prv->enm)) {
		nwamui_enm_set_active(prv->enm, FALSE);
	} else {
		nwamui_enm_set_active (prv->enm, TRUE);
    }
}

static void
on_nwam_enm_notify( GObject *gobject, GParamSpec *arg1, gpointer data)
{
	NwamEnmAction *self = NWAM_ENM_ACTION (data);
    NwamEnmActionPrivate *prv = GET_PRIVATE(self);
    gchar *m_name;
    gchar *new_text;

    m_name = nwamui_enm_get_name (prv->enm);

    if (nwamui_enm_get_active (prv->enm)) {
        new_text = g_strconcat (_("Stop "), m_name, NULL);
    } else {
        new_text = g_strconcat (_("Start "), m_name, NULL);
    }
    g_object_set(self, "label", new_text, NULL);

    g_free (new_text);
    g_free (m_name);
}

static void
nwam_enm_action_set_label(NwamEnmAction *self, const gchar *label)
{
    g_object_set(self, "label", label, NULL);
}

static GObject*
get_proxy(NwamEnmAction *self)
{
    return G_OBJECT(self->prv->enm);
}

NwamuiEnm *
nwam_enm_action_get_enm (NwamEnmAction *self)
{
    NwamuiEnm *enm;

    g_object_get(self, "enm", &enm, NULL);

    return enm;
}

void
nwam_enm_action_set_enm (NwamEnmAction *self, NwamuiEnm *enm)
{
    g_return_if_fail(NWAMUI_IS_ENM_NET(enm));

    g_object_set(self, "enm", enm, NULL);
}

