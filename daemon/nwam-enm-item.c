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
#include "libnwamui.h"

typedef struct _NwamEnmItemPrivate NwamEnmItemPrivate;
#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj),   \
        NWAM_TYPE_ENM_ITEM, NwamEnmItemPrivate))

struct _NwamEnmItemPrivate {
    NwamuiDaemon *daemon;
};

enum {
	PROP_ZERO,
};

static void nwam_enm_item_set_property (GObject         *object,
  guint            prop_id,
  const GValue    *value,
  GParamSpec      *pspec);
static void nwam_enm_item_get_property (GObject         *object,
  guint            prop_id,
  GValue          *value,
  GParamSpec      *pspec);
static void nwam_enm_item_finalize (NwamEnmItem *self);

/* nwamui enm net signals */
static void connect_enm_signals(NwamEnmItem *self, NwamuiEnm *enm);
static void disconnect_enm_signals(NwamEnmItem *self, NwamuiEnm *enm);
static void sync_enm(NwamEnmItem *self, NwamuiEnm *enm, gpointer user_data);
static void on_nwam_enm_toggled (GtkCheckMenuItem *item, gpointer data);
static void on_nwam_enm_notify( GObject *gobject, GParamSpec *arg1, gpointer data);

G_DEFINE_TYPE(NwamEnmItem, nwam_enm_item, NWAM_TYPE_MENU_ITEM)

static void
nwam_enm_item_class_init (NwamEnmItemClass *klass)
{
	GObjectClass *gobject_class;
    NwamMenuItemClass *nwam_menu_item_class;

	gobject_class = G_OBJECT_CLASS (klass);
/* 	gobject_class->set_property = nwam_enm_item_set_property; */
/* 	gobject_class->get_property = nwam_enm_item_get_property; */
	gobject_class->finalize = (void (*)(GObject*)) nwam_enm_item_finalize;

    nwam_menu_item_class = NWAM_MENU_ITEM_CLASS(klass);
    nwam_menu_item_class->connect_object = (nwam_menuitem_connect_object_t)connect_enm_signals;
    nwam_menu_item_class->disconnect_object = (nwam_menuitem_disconnect_object_t)disconnect_enm_signals;
    nwam_menu_item_class->sync_object = (nwam_menuitem_sync_object_t)sync_enm;
	
	g_type_class_add_private (klass, sizeof (NwamEnmItemPrivate));
}

static void
nwam_enm_item_init (NwamEnmItem *self)
{
    NwamEnmItemPrivate *prv = GET_PRIVATE(self);

    /* nwamui ncp signals */
    prv->daemon = nwamui_daemon_get_instance ();

    g_signal_connect(self, "toggled",
      G_CALLBACK(on_nwam_enm_toggled), NULL);

    /* Must set it initially, because there are no check elsewhere. */
    nwam_menu_item_set_widget(NWAM_MENU_ITEM(self), 0, gtk_image_new());
}

GtkWidget*
nwam_enm_item_new(NwamuiObject *enm)
{
    GtkWidget *item;

    g_assert(NWAMUI_IS_ENM(enm));

    item = g_object_new (NWAM_TYPE_ENM_ITEM,
      "proxy-object", enm,
      "draw-indicator", FALSE,
      NULL);

    return item;
}

static void
nwam_enm_item_finalize (NwamEnmItem *self)
{
    NwamEnmItemPrivate *prv = GET_PRIVATE(self);

    g_object_unref(prv->daemon);

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

	switch (prop_id) {
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

	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
connect_enm_signals(NwamEnmItem *self, NwamuiEnm *enm)
{
    g_signal_connect (G_OBJECT(enm), "notify::name",
      G_CALLBACK(on_nwam_enm_notify), (gpointer)self);
    g_signal_connect (enm, "notify::active",
      G_CALLBACK(on_nwam_enm_notify), (gpointer)self);
    g_signal_connect (G_OBJECT(enm), "notify::activation-mode",
      G_CALLBACK(on_nwam_enm_notify), (gpointer)self);
}

static void
disconnect_enm_signals(NwamEnmItem *self, NwamuiEnm *enm)
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
sync_enm(NwamEnmItem *self, NwamuiEnm *enm, gpointer user_data)
{
    on_nwam_enm_notify(G_OBJECT(enm), NULL, (gpointer)self);
}

static void
on_nwam_enm_toggled (GtkCheckMenuItem *item, gpointer data)
{
    NwamEnmItemPrivate *prv = GET_PRIVATE(item);
    NwamuiObject *object = NWAMUI_OBJECT(nwam_obj_proxy_get_proxy(NWAM_OBJ_PROXY_IFACE(item)));

	if (nwamui_object_get_active(object)) {
		nwamui_object_set_active(object, FALSE);
	} else {
        g_signal_handlers_block_by_func(item, (gpointer)on_nwam_enm_toggled, data);
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), FALSE);
        g_signal_handlers_unblock_by_func(item, (gpointer)on_nwam_enm_toggled, data);

		nwamui_object_set_active (object, TRUE);
    }
}

static void
on_nwam_enm_notify( GObject *gobject, GParamSpec *arg1, gpointer data)
{
	NwamEnmItem *self = NWAM_ENM_ITEM (data);
    NwamEnmItemPrivate *prv = GET_PRIVATE(self);
    NwamuiObject *object = NWAMUI_OBJECT(gobject);

    g_assert(NWAMUI_IS_ENM(object));

    if (!arg1 ||
      g_ascii_strcasecmp(arg1->name, "active") == 0 ||
      g_ascii_strcasecmp(arg1->name, "name") == 0) {
        const gchar *m_name;
        gchar *new_text;

        if (nwamui_object_get_active(object)) {
            m_name = _("Stop ");
        } else {
            m_name = _("Start ");
        }

        new_text = g_strconcat(m_name, nwamui_object_get_name(object), NULL);

        /* If there is any underscores we need to replace them with two since
         * otherwise it's interpreted as a mnemonic
         */
        new_text = nwamui_util_encode_menu_label( &new_text );
        menu_item_set_label(GTK_MENU_ITEM(self), new_text);

        g_free (new_text);
    }

    if (!arg1 || g_ascii_strcasecmp(arg1->name, "activation-mode") == 0) {
        const gchar    *icon_name = nwamui_util_get_active_mode_icon( object );
        GtkWidget      *image = nwam_menu_item_get_widget(NWAM_MENU_ITEM(self), 0);

        gtk_image_set_from_icon_name( GTK_IMAGE(image), icon_name, GTK_ICON_SIZE_MENU );

        switch (nwamui_object_get_activation_mode(object)) {
        case NWAMUI_COND_ACTIVATION_MODE_MANUAL:
            gtk_widget_set_sensitive(GTK_WIDGET(self), TRUE);
            break;
/*         case NWAMUI_COND_ACTIVATION_MODE_SYSTEM: */
/*         case NWAMUI_COND_ACTIVATION_MODE_PRIORITIZED: */
/*         case NWAMUI_COND_ACTIVATION_MODE_CONDITIONAL_ANY: */
/*         case NWAMUI_COND_ACTIVATION_MODE_CONDITIONAL_ALL: */
        default:
            gtk_widget_set_sensitive(GTK_WIDGET(self), FALSE);
            break;
        }
    }
}

NwamuiEnm *
nwam_enm_item_get_enm (NwamEnmItem *self)
{
    NwamuiEnm *enm;

    g_object_get(self, "proxy-object", &enm, NULL);

    return enm;
}

void
nwam_enm_item_set_enm (NwamEnmItem *self, NwamuiEnm *enm)
{
    g_return_if_fail(NWAMUI_IS_ENM(enm));

    g_object_set(self, "proxy-object", enm, NULL);
}
