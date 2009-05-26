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

#include "nwam-tooltip-widget.h"
#include "libnwamui.h"


typedef struct _NwamObjectTooltipWidgetPrivate NwamObjectTooltipWidgetPrivate;
#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj),   \
        NWAM_TYPE_OBJECT_TOOLTIP_WIDGET, NwamObjectTooltipWidgetPrivate))

struct _NwamObjectTooltipWidgetPrivate {
    gulong place_holder;
};

enum {
	PROP_ZERO,
};

static void nwam_object_tooltip_widget_set_property (GObject         *object,
  guint            prop_id,
  const GValue    *value,
  GParamSpec      *pspec);
static void nwam_object_tooltip_widget_get_property (GObject         *object,
  guint            prop_id,
  GValue          *value,
  GParamSpec      *pspec);
static void nwam_object_tooltip_widget_finalize (NwamObjectTooltipWidget *self);

/* nwamui object net signals */
static void connect_object(NwamObjectTooltipWidget *self, NwamuiObject *object);
static void disconnect_object(NwamObjectTooltipWidget *self, NwamuiObject *object);
static void sync_object(NwamObjectTooltipWidget *self, NwamuiObject *object, gpointer user_data);
static void nwam_object_notify(GObject *gobject, GParamSpec *arg1, gpointer user_data);
static void nwam_ncu_notify(GObject *gobject, GParamSpec *arg1, gpointer user_data);

G_DEFINE_TYPE(NwamObjectTooltipWidget, nwam_object_tooltip_widget, NWAM_TYPE_MENU_ITEM)

static void
nwam_object_tooltip_widget_class_init (NwamObjectTooltipWidgetClass *klass)
{
	GObjectClass *gobject_class;
    NwamMenuItemClass *nwam_menu_item_class;

	gobject_class = G_OBJECT_CLASS (klass);
/* 	gobject_class->set_property = nwam_object_tooltip_widget_set_property; */
/* 	gobject_class->get_property = nwam_object_tooltip_widget_get_property; */
	gobject_class->finalize = (void (*)(GObject*)) nwam_object_tooltip_widget_finalize;

    nwam_menu_item_class = NWAM_MENU_ITEM_CLASS(klass);
    nwam_menu_item_class->connect_object = connect_object;
    nwam_menu_item_class->disconnect_object = disconnect_object;
    nwam_menu_item_class->sync_object = sync_object;
	
	g_type_class_add_private (klass, sizeof (NwamObjectTooltipWidgetPrivate));
}

static void
nwam_object_tooltip_widget_init (NwamObjectTooltipWidget *self)
{
    NwamObjectTooltipWidgetPrivate *prv = GET_PRIVATE(self);
}

GtkWidget *
nwam_object_tooltip_widget_new(NwamuiObject *object)
{
    NwamObjectTooltipWidget *item;

    item = g_object_new (NWAM_TYPE_OBJECT_TOOLTIP_WIDGET,
      "proxy-object", object,
      NULL);

    return GTK_WIDGET(item);
}

static void
nwam_object_tooltip_widget_finalize (NwamObjectTooltipWidget *self)
{
    NwamObjectTooltipWidgetPrivate *prv = GET_PRIVATE(self);

	G_OBJECT_CLASS(nwam_object_tooltip_widget_parent_class)->finalize(G_OBJECT (self));
}

static void
nwam_object_tooltip_widget_set_property (GObject         *object,
			     guint            prop_id,
			     const GValue    *value,
			     GParamSpec      *pspec)
{
	NwamObjectTooltipWidget *self = NWAM_OBJECT_TOOLTIP_WIDGET (object);
    NwamObjectTooltipWidgetPrivate *prv = GET_PRIVATE(self);

	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nwam_object_tooltip_widget_get_property (GObject         *object,
  guint            prop_id,
  GValue          *value,
  GParamSpec      *pspec)
{
	NwamObjectTooltipWidget *self = NWAM_OBJECT_TOOLTIP_WIDGET (object);
    NwamObjectTooltipWidgetPrivate *prv = GET_PRIVATE(self);

	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
connect_object(NwamObjectTooltipWidget *self, NwamuiObject *object)
{
    GType type = G_OBJECT_TYPE(object);

	if (type == NWAMUI_TYPE_NCU) {
        g_signal_connect (G_OBJECT(object), "notify::vanity_name",
          G_CALLBACK(nwam_object_notify), (gpointer)self);
        g_signal_connect (G_OBJECT(object), "notify::active",
          G_CALLBACK(nwam_ncu_notify), (gpointer)self);

        g_signal_connect (G_OBJECT(object), "notify::wifi-info",
          G_CALLBACK(nwam_ncu_notify), (gpointer)self);

        /* Call once on initial connection */
        nwam_ncu_notify(G_OBJECT(object), NULL, (gpointer)self);
    } else {
        g_signal_connect (G_OBJECT(object), "notify::name",
          G_CALLBACK(nwam_object_notify), (gpointer)self);
    }
}

static void
disconnect_object(NwamObjectTooltipWidget *self, NwamuiObject *object)
{
    g_signal_handlers_disconnect_matched(object,
      G_SIGNAL_MATCH_DATA,
      0,
      NULL,
      NULL,
      NULL,
      (gpointer)self);
}

static void
sync_object(NwamObjectTooltipWidget *self, NwamuiObject *object, gpointer user_data)
{
    nwam_object_notify(G_OBJECT(object), NULL, (gpointer)self);
}

static void
nwam_object_notify(GObject *gobject, GParamSpec *arg1, gpointer user_data)
{
	NwamObjectTooltipWidget *self = NWAM_OBJECT_TOOLTIP_WIDGET (user_data);
    NwamObjectTooltipWidgetPrivate *prv = GET_PRIVATE(self);
    NwamuiObject *object = NWAMUI_OBJECT(gobject);
    GType         type;
    GtkWidget    *item;
	GString      *gstr;
    gchar        *name;

    gstr = g_string_new("");
    type = G_OBJECT_TYPE(object);

    name = nwamui_object_get_name(object);

	if (type == NWAMUI_TYPE_NCU) {
        gchar *state = nwamui_ncu_get_connection_state_string(NWAMUI_NCU(object));
        switch (nwamui_ncu_get_ncu_type(NWAMUI_NCU(object))) {
        case NWAMUI_NCU_TYPE_WIRELESS:
            g_string_append_printf(gstr, _("<b>Wireless (%s):</b> %s"),
              name, state);
            break;
        case NWAMUI_NCU_TYPE_WIRED:
        case NWAMUI_NCU_TYPE_TUNNEL:
            g_string_append_printf(gstr, _("<b>Wired (%s):</b> %s"),
              name, state);
            break;
        default:
            g_assert_not_reached ();
        }
        g_free(state);
	} else if (type == NWAMUI_TYPE_ENV) {
        g_string_append_printf(gstr, _("<b>Location:</b> %s"), name);
	} else if (type == NWAMUI_TYPE_NCP) {
        g_string_append_printf(gstr, _("<b>Network Profile:</b> %s"), name);
/* 	} else if (type == NWAMUI_TYPE_WIFI_NET) { */
/* 	} else if (type == NWAMUI_TYPE_ENM) { */
/* 	} else { */
	}
    menu_item_set_markup(GTK_MENU_ITEM(user_data), gstr->str);
    g_string_free(gstr, TRUE);
    g_free(name);
}

static void
nwam_ncu_notify(GObject *gobject, GParamSpec *arg1, gpointer user_data)
{
    GtkImage *img = gtk_image_new_from_pixbuf(nwamui_util_get_ncu_status_icon(NWAMUI_NCU(gobject)));

    nwam_menu_item_set_widget(NWAM_MENU_ITEM(user_data),
      0,
      GTK_WIDGET(img));
}

