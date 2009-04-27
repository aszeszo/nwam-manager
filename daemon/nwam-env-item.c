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

#include "nwam-env-item.h"
#include "nwam-obj-proxy-iface.h"
#include "libnwamui.h"


typedef struct _NwamEnvItemPrivate NwamEnvItemPrivate;
#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj),   \
        NWAM_TYPE_ENV_ITEM, NwamEnvItemPrivate))

struct _NwamEnvItemPrivate {
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
static GObject* get_proxy(NwamObjProxyIFace *iface);
static void refresh(NwamObjProxyIFace *iface);

static void nwam_env_item_set_property (GObject         *object,
  guint            prop_id,
  const GValue    *value,
  GParamSpec      *pspec);
static void nwam_env_item_get_property (GObject         *object,
  guint            prop_id,
  GValue          *value,
  GParamSpec      *pspec);
static void nwam_env_item_finalize (NwamEnvItem *self);

/* nwamui daemon signals */
static void connect_daemon_signals(GObject *self, NwamuiDaemon *daemon);
static void disconnect_daemon_signals(GObject *self, NwamuiDaemon *daemon);
static void daemon_env_selection_needed (NwamuiDaemon* daemon, NwamuiNcu* ncu, gpointer user_data);

/* nwamui env net signals */
static void connect_env_net_signals(NwamEnvItem *self, NwamuiEnv *env);
static void disconnect_env_net_signals(NwamEnvItem *self, NwamuiEnv *env);
static void on_nwam_env_toggled (GtkCheckMenuItem *item, gpointer data);
static void on_nwam_env_notify( GObject *gobject, GParamSpec *arg1, gpointer data);

G_DEFINE_TYPE_EXTENDED(NwamEnvItem, nwam_env_item,
  NWAM_TYPE_MENU_ITEM, 0,
  G_IMPLEMENT_INTERFACE(NWAM_TYPE_OBJ_PROXY_IFACE, nwam_obj_proxy_init))

static void
nwam_obj_proxy_init(NwamObjProxyInterface *iface)
{
    iface->get_proxy = get_proxy;
    iface->refresh = refresh;
    iface->delete_notify = NULL;
}

static void
nwam_env_item_class_init (NwamEnvItemClass *klass)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->set_property = nwam_env_item_set_property;
	gobject_class->get_property = nwam_env_item_get_property;
	gobject_class->finalize = (void (*)(GObject*)) nwam_env_item_finalize;
	
	g_object_class_install_property (gobject_class,
      PROP_ENV,
      g_param_spec_object ("env",
        N_("NwamuiEnv instance"),
        N_("Wireless AP"),
        NWAMUI_TYPE_ENV,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_type_class_add_private (klass, sizeof (NwamEnvItemPrivate));
}

static void
nwam_env_item_init (NwamEnvItem *self)
{
    NwamEnvItemPrivate *prv = GET_PRIVATE(self);

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

    /* Must set it initially, because there are no check elsewhere. */
    nwam_menu_item_set_widget(NWAM_MENU_ITEM(self), 0, gtk_label_new(""));
}

GtkWidget*
nwam_env_item_new(NwamuiEnv *env)
{
    GtkWidget *item;

    item = g_object_new (NWAM_TYPE_ENV_ITEM,
      "env", env,
      NULL);

    return item;
}

static void
nwam_env_item_finalize (NwamEnvItem *self)
{
    NwamEnvItemPrivate *prv = GET_PRIVATE(self);
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

	G_OBJECT_CLASS(nwam_env_item_parent_class)->finalize(G_OBJECT (self));
}

static void
nwam_env_item_set_property (GObject         *object,
			     guint            prop_id,
			     const GValue    *value,
			     GParamSpec      *pspec)
{
	NwamEnvItem *self = NWAM_ENV_ITEM (object);
    NwamEnvItemPrivate *prv = GET_PRIVATE(self);
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
            nwam_obj_proxy_refresh(NWAM_OBJ_PROXY_IFACE(self));
        }
        break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nwam_env_item_get_property (GObject         *object,
  guint            prop_id,
  GValue          *value,
  GParamSpec      *pspec)
{
	NwamEnvItem *self = NWAM_ENV_ITEM (object);
    NwamEnvItemPrivate *prv = GET_PRIVATE(self);

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
connect_env_net_signals(NwamEnvItem *self, NwamuiEnv *env)
{
    g_signal_connect (G_OBJECT(env), "notify::active",
      G_CALLBACK(on_nwam_env_notify), (gpointer)self);
}

static void
disconnect_env_net_signals(NwamEnvItem *self, NwamuiEnv *env)
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
on_nwam_env_toggled (GtkCheckMenuItem *item, gpointer data)
{
	NwamEnvItem *self = NWAM_ENV_ITEM (item);
    NwamEnvItemPrivate *prv = GET_PRIVATE(self);
    NwamuiDaemon *daemon = nwamui_daemon_get_instance ();

    g_signal_handler_block(self, prv->toggled_handler_id);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(self), FALSE);
    g_signal_handler_unblock(self, prv->toggled_handler_id);

	if (!nwamui_daemon_is_active_env(daemon, prv->env)) {
		nwamui_daemon_set_active_env(daemon, prv->env);
	}
    g_object_unref(daemon);
}

static void
on_nwam_env_notify( GObject *gobject, GParamSpec *arg1, gpointer data)
{
	NwamEnvItem *self = NWAM_ENV_ITEM (data);
    NwamEnvItemPrivate *prv = GET_PRIVATE(self);
    NwamuiObject *object = NWAMUI_OBJECT(gobject);

    g_debug("menuitem get env notify %s changed\n", (arg1 && arg1->name)?arg1->name:"NULL");
    g_assert(NWAMUI_IS_ENV(object));

    if (!arg1 || g_ascii_strcasecmp(arg1->name, "active") == 0) {

        g_signal_handler_block(self, prv->toggled_handler_id);
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(self),
          nwamui_object_get_active(object));
        g_signal_handler_unblock(self, prv->toggled_handler_id);

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

    if (!arg1 || g_ascii_strcasecmp(arg1->name, "name") == 0) {

        gchar *menu_text = NULL;
        menu_text = nwamui_object_get_name(object);
        /* If there is any underscores we need to replace them with two since
         * otherwise it's interpreted as a mnemonic
         */
        menu_text = nwamui_util_encode_menu_label( &menu_text );
        menu_item_set_label(GTK_MENU_ITEM(self), menu_text);
        g_free(menu_text);
    }
}

static GObject*
get_proxy(NwamObjProxyIFace *iface)
{
    NwamEnvItemPrivate *prv = GET_PRIVATE(iface);
    return G_OBJECT(prv->env);
}

static void
refresh(NwamObjProxyIFace *iface)
{
    NwamEnvItem *self = NWAM_ENV_ITEM(iface);
    NwamEnvItemPrivate *prv = GET_PRIVATE(iface);

    on_nwam_env_notify(G_OBJECT(prv->env), NULL, (gpointer)self);
}

NwamuiEnv *
nwam_env_item_get_env (NwamEnvItem *self)
{
    NwamuiEnv *env;

    g_object_get(self, "env", &env, NULL);

    return env;
}

void
nwam_env_item_set_env (NwamEnvItem *self, NwamuiEnv *env)
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
