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

#include "nwam-env-action.h"
#include "nwam-obj-proxy-iface.h"
#include "libnwamui.h"


#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj),   \
        NWAM_TYPE_ENV_ACTION, NwamEnvActionPrivate))

struct _NwamEnvActionPrivate {
    NwamuiEnv *env;
    gulong toggled_handler_id;
};

enum {
	PROP_ZERO,
	PROP_ENV,
	PROP_LWIDGET,
	PROP_RWIDGET,
};

static void nwam_obj_proxy_init(NwamObjProxyInterface *iface);
static GObject* get_proxy(NwamEnvAction *self);

static void nwam_env_action_set_property (GObject         *object,
  guint            prop_id,
  const GValue    *value,
  GParamSpec      *pspec);
static void nwam_env_action_get_property (GObject         *object,
  guint            prop_id,
  GValue          *value,
  GParamSpec      *pspec);
static void nwam_env_action_finalize (NwamEnvAction *self);

/* nwamui daemon signals */
static void connect_daemon_signals(GObject *self, NwamuiDaemon *daemon);
static void disconnect_daemon_signals(GObject *self, NwamuiDaemon *daemon);
static void daemon_env_selection_needed (NwamuiDaemon* daemon, NwamuiNcu* ncu, gpointer user_data);

/* nwamui env net signals */
static void connect_env_net_signals(NwamEnvAction *self, NwamuiEnv *env);
static void disconnect_env_net_signals(NwamEnvAction *self, NwamuiEnv *env);
static void on_nwam_env_toggled (GtkAction *action, gpointer data);
static void on_nwam_env_notify( GObject *gobject, GParamSpec *arg1, gpointer data);

G_DEFINE_TYPE_EXTENDED(NwamEnvAction, nwam_env_action,
  GTK_TYPE_TOGGLE_ACTION, 0,
  G_IMPLEMENT_INTERFACE(NWAM_TYPE_OBJ_PROXY_IFACE, nwam_obj_proxy_init))

static void
nwam_obj_proxy_init(NwamObjProxyInterface *iface)
{
    iface->get_proxy = get_proxy;
    iface->delete_notify = NULL;
}

static void
nwam_env_action_class_init (NwamEnvActionClass *klass)
{
	GObjectClass *gobject_class;
	GtkActionClass *action_class;

	gobject_class = G_OBJECT_CLASS (klass);
	action_class = GTK_ACTION_CLASS (klass);

	gobject_class->set_property = nwam_env_action_set_property;
	gobject_class->get_property = nwam_env_action_get_property;
	gobject_class->finalize = (void (*)(GObject*)) nwam_env_action_finalize;
	
	g_object_class_install_property (gobject_class,
      PROP_ENV,
      g_param_spec_object ("env",
        N_("NwamuiEnv instance"),
        N_("Wireless AP"),
        NWAMUI_TYPE_ENV,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_type_class_add_private (klass, sizeof (NwamEnvActionPrivate));
}

static void
nwam_env_action_init (NwamEnvAction *self)
{
    NwamEnvActionPrivate *prv = GET_PRIVATE(self);
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

    prv->toggled_handler_id = g_signal_connect(self,
      "toggled", G_CALLBACK(on_nwam_env_toggled), NULL);
}

NwamEnvAction *
nwam_env_action_new(NwamuiEnv *env)
{
    NwamEnvAction *action;
    gchar *menu_text = NULL;

    menu_text = nwamui_env_get_name(NWAMUI_ENV(env));

    action = g_object_new (NWAM_TYPE_ENV_ACTION,
      "name", menu_text,
      "label", menu_text,
      "tooltip", NULL,
      "stock-id", NULL,
      "env", env,
      NULL);

    g_free(menu_text);

    return action;
}

static void
nwam_env_action_finalize (NwamEnvAction *self)
{
    NwamEnvActionPrivate *prv = GET_PRIVATE(self);
    int i;

    /* nwamui ncp signals */
    {
        NwamuiDaemon *daemon = nwamui_daemon_get_instance ();
        NwamuiNcp *ncp = nwamui_daemon_get_active_ncp(daemon);

        disconnect_daemon_signals(G_OBJECT(self), daemon);

        g_object_unref(ncp);
        g_object_unref(daemon);
    }

    if (prv->env) {
        disconnect_env_net_signals(self, prv->env);
        g_object_unref(prv->env);
    }

	G_OBJECT_CLASS(nwam_env_action_parent_class)->finalize(G_OBJECT (self));
}

static void
nwam_env_action_set_property (GObject         *object,
			     guint            prop_id,
			     const GValue    *value,
			     GParamSpec      *pspec)
{
	NwamEnvAction *self = NWAM_ENV_ACTION (object);
    NwamEnvActionPrivate *prv = GET_PRIVATE(self);
    GObject *obj = g_value_dup_object (value);

	switch (prop_id) {
	case PROP_ENV:
        if (prv->env != NWAMUI_ENV(obj)) {
            if (prv->env) {
                /* remove signal callback */
                disconnect_env_net_signals(self, prv->env);

                g_object_unref(prv->env);
            }
            prv->env = NWAMUI_ENV(obj);

            /* connect signal callback */
            connect_env_net_signals(self, prv->env);

            /* initializing */
            on_nwam_env_notify(G_OBJECT(prv->env), NULL, (gpointer)self);
        }
        break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nwam_env_action_get_property (GObject         *object,
  guint            prop_id,
  GValue          *value,
  GParamSpec      *pspec)
{
	NwamEnvAction *self = NWAM_ENV_ACTION (object);
    NwamEnvActionPrivate *prv = GET_PRIVATE(self);

	switch (prop_id)
	{
	case PROP_ENV:
		g_value_set_object (value, (GObject*) prv->env);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
connect_env_net_signals(NwamEnvAction *self, NwamuiEnv *env)
{
    g_signal_connect (G_OBJECT(env), "notify::active",
      G_CALLBACK(on_nwam_env_notify), (gpointer)self);
}

static void
disconnect_env_net_signals(NwamEnvAction *self, NwamuiEnv *env)
{
    g_signal_handlers_disconnect_matched(env,
      G_SIGNAL_MATCH_DATA,
      0,
      NULL,
      NULL,
      NULL,
      (gpointer)self);
}

static void
on_nwam_env_toggled (GtkAction *action, gpointer data)
{
	NwamEnvAction *self = NWAM_ENV_ACTION (action);
    NwamEnvActionPrivate *prv = GET_PRIVATE(self);
    NwamuiDaemon *daemon = nwamui_daemon_get_instance ();

    g_signal_handler_block(self, prv->toggled_handler_id);
    gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(self), FALSE);
    g_signal_handler_unblock(self, prv->toggled_handler_id);

	if (!nwamui_daemon_is_active_env(daemon, prv->env)) {
		nwamui_daemon_set_active_env(daemon, prv->env);
	}
    g_object_unref(daemon);
}

static void
on_nwam_env_notify( GObject *gobject, GParamSpec *arg1, gpointer data)
{
	NwamEnvAction *self = NWAM_ENV_ACTION (data);
    NwamEnvActionPrivate *prv = GET_PRIVATE(self);

    g_debug("menuitem get env notify %s changed\n", (arg1 && arg1->name)?arg1->name:"NULL");
}

static void
nwam_env_action_set_label(NwamEnvAction *self, const gchar *label)
{
    g_object_set(self, "label", label, NULL);
}

static GObject*
get_proxy(NwamEnvAction *self)
{
    return G_OBJECT(self->prv->env);
}

NwamuiEnv *
nwam_env_action_get_env (NwamEnvAction *self)
{
    NwamuiEnv *env;

    g_object_get(self, "env", &env, NULL);

    return env;
}

void
nwam_env_action_set_env (NwamEnvAction *self, NwamuiEnv *env)
{
    g_return_if_fail(NWAMUI_IS_ENV(env));

    g_object_set(self, "env", env, NULL);
}

static void 
connect_daemon_signals(GObject *self, NwamuiDaemon *daemon)
{
}

static void 
disconnect_daemon_signals(GObject *self, NwamuiDaemon *daemon)
{
}

