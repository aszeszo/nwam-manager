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

#include "nwam-ncu-item.h"
#include "nwam-obj-proxy-iface.h"
#include "libnwamui.h"


typedef struct _NwamNcuItemPrivate NwamNcuItemPrivate;
#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj),   \
        NWAM_TYPE_NCU_ITEM, NwamNcuItemPrivate))

struct _NwamNcuItemPrivate {
    NwamuiNcu *ncu;
    gulong toggled_handler_id;
};

enum {
	PROP_ZERO,
	PROP_NCU,
	PROP_LWIDGET,
	PROP_RWIDGET,
};

static void nwam_obj_proxy_init(NwamObjProxyInterface *iface);
static GObject* get_proxy(NwamObjProxyIFace *iface);

static void nwam_ncu_item_set_property (GObject         *object,
  guint            prop_id,
  const GValue    *value,
  GParamSpec      *pspec);
static void nwam_ncu_item_get_property (GObject         *object,
  guint            prop_id,
  GValue          *value,
  GParamSpec      *pspec);
static void nwam_ncu_item_finalize (NwamNcuItem *self);

/* nwamui daemon signals */
static void connect_daemon_signals(GObject *self, NwamuiDaemon *daemon);
static void disconnect_daemon_signals(GObject *self, NwamuiDaemon *daemon);
static void daemon_ncu_selection_needed (NwamuiDaemon* daemon, NwamuiNcu* ncu, gpointer user_data);

/* nwamui ncu net signals */
static void connect_ncu_signals(NwamNcuItem *self, NwamuiNcu *ncu);
static void disconnect_ncu_signals(NwamNcuItem *self, NwamuiNcu *ncu);
static void on_nwam_ncu_toggled (GtkCheckMenuItem *item, gpointer data);
static void on_nwam_ncu_notify( GObject *gobject, GParamSpec *arg1, gpointer data);

G_DEFINE_TYPE_EXTENDED(NwamNcuItem, nwam_ncu_item,
  GTK_TYPE_CHECK_MENU_ITEM, 0,
  G_IMPLEMENT_INTERFACE(NWAM_TYPE_OBJ_PROXY_IFACE, nwam_obj_proxy_init))

static void
nwam_obj_proxy_init(NwamObjProxyInterface *iface)
{
    iface->get_proxy = get_proxy;
    iface->delete_notify = NULL;
}

static void
nwam_ncu_item_class_init (NwamNcuItemClass *klass)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->set_property = nwam_ncu_item_set_property;
	gobject_class->get_property = nwam_ncu_item_get_property;
	gobject_class->finalize = (void (*)(GObject*)) nwam_ncu_item_finalize;
	
	g_object_class_install_property (gobject_class,
      PROP_NCU,
      g_param_spec_object ("ncu",
        N_("NwamuiNcu instance"),
        N_("NwamuiNcu instance"),
        NWAMUI_TYPE_NCU,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_type_class_add_private (klass, sizeof (NwamNcuItemPrivate));
}

static void
nwam_ncu_item_init (NwamNcuItem *self)
{
    NwamNcuItemPrivate *prv = GET_PRIVATE(self);

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
      "toggled", G_CALLBACK(on_nwam_ncu_toggled), NULL);
}

GtkWidget *
nwam_ncu_item_new(NwamuiNcu *ncu)
{
    NwamNcuItem *item;

    item = g_object_new (NWAM_TYPE_NCU_ITEM,
      "ncu", ncu,
      NULL);

    return GTK_WIDGET(item);
}

static void
nwam_ncu_item_finalize (NwamNcuItem *self)
{
    NwamNcuItemPrivate *prv = GET_PRIVATE(self);
    int i;

    /* nwamui ncp signals */
    {
        NwamuiDaemon *daemon = nwamui_daemon_get_instance ();
        NwamuiNcp *ncp = nwamui_daemon_get_active_ncp(daemon);

        disconnect_daemon_signals(G_OBJECT(self), daemon);

        g_object_unref(ncp);
        g_object_unref(daemon);
    }

    if (prv->ncu) {
        disconnect_ncu_signals(self, prv->ncu);
        g_object_unref(prv->ncu);
    }

	G_OBJECT_CLASS(nwam_ncu_item_parent_class)->finalize(G_OBJECT (self));
}

static void
nwam_ncu_item_set_property (GObject         *object,
			     guint            prop_id,
			     const GValue    *value,
			     GParamSpec      *pspec)
{
	NwamNcuItem *self = NWAM_NCU_ITEM (object);
    NwamNcuItemPrivate *prv = GET_PRIVATE(self);
    GObject *obj = g_value_dup_object (value);

	switch (prop_id) {
	case PROP_NCU:
        if (prv->ncu != NWAMUI_NCU(obj)) {
            if (prv->ncu) {
                /* remove signal callback */
                disconnect_ncu_signals(self, prv->ncu);

                g_object_unref(prv->ncu);
            }
            prv->ncu = NWAMUI_NCU(obj);

            /* connect signal callback */
            connect_ncu_signals(self, prv->ncu);

            /* initializing */
            on_nwam_ncu_notify(G_OBJECT(prv->ncu), NULL, (gpointer)self);
        }
        break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nwam_ncu_item_get_property (GObject         *object,
  guint            prop_id,
  GValue          *value,
  GParamSpec      *pspec)
{
	NwamNcuItem *self = NWAM_NCU_ITEM (object);
    NwamNcuItemPrivate *prv = GET_PRIVATE(self);

	switch (prop_id)
	{
	case PROP_NCU:
		g_value_set_object (value, (GObject*) prv->ncu);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
connect_ncu_signals(NwamNcuItem *self, NwamuiNcu *ncu)
{
    g_signal_connect (G_OBJECT(ncu), "notify::active",
      G_CALLBACK(on_nwam_ncu_notify), (gpointer)self);
}

static void
disconnect_ncu_signals(NwamNcuItem *self, NwamuiNcu *ncu)
{
    g_signal_handlers_disconnect_matched(ncu,
      G_SIGNAL_MATCH_DATA,
      0,
      NULL,
      NULL,
      NULL,
      (gpointer)self);
}

static void
on_nwam_ncu_toggled (GtkCheckMenuItem *item, gpointer data)
{
	NwamNcuItem *self = NWAM_NCU_ITEM (item);
    NwamNcuItemPrivate *prv = GET_PRIVATE(self);
    NwamuiDaemon *daemon = nwamui_daemon_get_instance ();
    NwamuiNcp* ncp = nwamui_daemon_get_active_ncp (daemon);
    NwamuiNcu* ncu;

    g_signal_handler_block(self, prv->toggled_handler_id);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(self), FALSE);
    g_signal_handler_unblock(self, prv->toggled_handler_id);

    /* If the toggle is generated by the daemon we don't want to proceed any
     * more since it will tell the daemon to reconnected unnecessarily
     */
    if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item))) {
        if (ncu) {
            gchar *name = nwamui_ncu_get_device_name(ncu);
            g_debug("\n\tENABLE\tNcu\t 0x%p\t%s\n", ncu, name);
            g_free(name);
            nwamui_ncp_set_manual_ncu_selection(ncp, ncu);
        } else {
            nwamui_ncp_set_automatic_ncu_selection(ncp);
        }
    }
    g_object_unref(daemon);
    g_object_unref(ncp);
}

static void
on_nwam_ncu_notify( GObject *gobject, GParamSpec *arg1, gpointer data)
{
	NwamNcuItem *self = NWAM_NCU_ITEM (data);
    NwamNcuItemPrivate *prv = GET_PRIVATE(self);
    NwamuiObject *object = NWAMUI_OBJECT(gobject);

    g_debug("menuitem get ncu notify %s changed\n", (arg1 && arg1->name)?arg1->name:"NULL");
    g_assert(NWAMUI_IS_NCU(object));

/*     switch (nwamui_ncu_get_ncu_type (ncu)) { */
/*     case NWAMUI_NCU_TYPE_WIRELESS: */
/*         break; */
/*     case NWAMUI_NCU_TYPE_WIRED: */
/*         break; */
/*     default: */
/*         g_assert_not_reached (); */
/*     } */

    if (!arg1 || g_ascii_strcasecmp(arg1->name, "active") == 0) {

        g_signal_handler_block(self, prv->toggled_handler_id);
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(self),
          nwamui_object_get_active(object));
        g_signal_handler_unblock(self, prv->toggled_handler_id);

    } else if (!arg1 || g_ascii_strcasecmp(arg1->name, "enabled") == 0) {

        gtk_widget_set_sensitive(GTK_WIDGET(self),
          nwamui_ncu_get_enabled(NWAMUI_NCU(object)));

    } else if (!arg1 || g_ascii_strcasecmp(arg1->name, "vanity_name") == 0) {
		gchar *name = NULL;
        gchar *lbl_name = NULL;
        gchar *type_str = NULL;
		
		name = nwamui_object_get_name(object);
		if ( nwamui_ncu_get_ncu_type(NWAMUI_NCU(object)) == NWAMUI_NCU_TYPE_WIRELESS ){
            type_str = _("Wireless");
        }
        else {
            type_str = _("Wired");
        }
        lbl_name = g_strdup_printf(_("%s (%s)"), type_str, name);

        /* If there is any underscores we need to replace them with two since
         * otherwise it's interpreted as a mnemonic
         */
        lbl_name = nwamui_util_encode_menu_label( &lbl_name );
        menu_item_set_label(GTK_MENU_ITEM(self), lbl_name);

		g_free(name);
		g_free(lbl_name);
    }
}

static GObject*
get_proxy(NwamObjProxyIFace *iface)
{
    NwamNcuItemPrivate *prv = GET_PRIVATE(iface);
    return G_OBJECT(prv->ncu);
}

NwamuiNcu *
nwam_ncu_item_get_ncu (NwamNcuItem *self)
{
    NwamuiNcu *ncu;

    g_object_get(self, "ncu", &ncu, NULL);

    return ncu;
}

void
nwam_ncu_item_set_ncu (NwamNcuItem *self, NwamuiNcu *ncu)
{
    g_return_if_fail(NWAMUI_IS_NCU(ncu));

    g_object_set(self, "ncu", ncu, NULL);
}

static void 
connect_daemon_signals(GObject *self, NwamuiDaemon *daemon)
{
}

static void 
disconnect_daemon_signals(GObject *self, NwamuiDaemon *daemon)
{
}

