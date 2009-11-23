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
#include "libnwamui.h"


typedef struct _NwamNcuItemPrivate NwamNcuItemPrivate;
#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj),   \
        NWAM_TYPE_NCU_ITEM, NwamNcuItemPrivate))

struct _NwamNcuItemPrivate {
    NwamuiDaemon *daemon;
    gboolean sensitive;
};

enum {
	PROP_ZERO,
};

static void nwam_ncu_item_set_property (GObject         *object,
  guint            prop_id,
  const GValue    *value,
  GParamSpec      *pspec);
static void nwam_ncu_item_get_property (GObject         *object,
  guint            prop_id,
  GValue          *value,
  GParamSpec      *pspec);
static void nwam_ncu_item_finalize (NwamNcuItem *self);

/* nwamui ncu net signals */
static gint menu_ncu_item_compare(NwamMenuItem *self, NwamMenuItem *other);
static void connect_ncu_signals(NwamNcuItem *self, NwamuiNcu *ncu);
static void disconnect_ncu_signals(NwamNcuItem *self, NwamuiNcu *ncu);
static void sync_ncu(NwamNcuItem *self, NwamuiNcu *ncu, gpointer user_data);
static void on_nwam_ncu_toggled (GtkCheckMenuItem *item, gpointer data);
static void on_nwam_ncu_notify( GObject *gobject, GParamSpec *arg1, gpointer data);

G_DEFINE_TYPE(NwamNcuItem, nwam_ncu_item, NWAM_TYPE_MENU_ITEM)

static void
nwam_ncu_item_class_init (NwamNcuItemClass *klass)
{
	GObjectClass *gobject_class;
    NwamMenuItemClass *nwam_menu_item_class;

	gobject_class = G_OBJECT_CLASS (klass);
/* 	gobject_class->set_property = nwam_ncu_item_set_property; */
/* 	gobject_class->get_property = nwam_ncu_item_get_property; */
	gobject_class->finalize = (void (*)(GObject*)) nwam_ncu_item_finalize;

    nwam_menu_item_class = NWAM_MENU_ITEM_CLASS(klass);
    nwam_menu_item_class->connect_object = (nwam_menuitem_connect_object_t)connect_ncu_signals;
    nwam_menu_item_class->disconnect_object = (nwam_menuitem_disconnect_object_t)disconnect_ncu_signals;
    nwam_menu_item_class->sync_object = (nwam_menuitem_sync_object_t)sync_ncu;
    nwam_menu_item_class->compare = (nwam_menuitem_compare_t)menu_ncu_item_compare;
	
	g_type_class_add_private (klass, sizeof (NwamNcuItemPrivate));
}

static void
nwam_ncu_item_init (NwamNcuItem *self)
{
    NwamNcuItemPrivate *prv = GET_PRIVATE(self);

    prv->daemon = nwamui_daemon_get_instance ();

    g_signal_connect(self, "toggled",
      G_CALLBACK(on_nwam_ncu_toggled), NULL);
}

GtkWidget *
nwam_ncu_item_new(NwamuiNcu *ncu)
{
    NwamNcuItem *item;

    item = g_object_new (NWAM_TYPE_NCU_ITEM,
      "proxy-object", ncu,
      NULL);

    return GTK_WIDGET(item);
}

static void
nwam_ncu_item_finalize (NwamNcuItem *self)
{
    NwamNcuItemPrivate *prv = GET_PRIVATE(self);

    g_object_unref(prv->daemon);

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

	switch (prop_id) {
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

	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static gint
menu_ncu_item_compare(NwamMenuItem *self, NwamMenuItem *other)
{
    return nwamui_ncu_get_priority_group(NWAMUI_NCU(nwam_obj_proxy_get_proxy(NWAM_OBJ_PROXY_IFACE(self)))) - nwamui_ncu_get_priority_group(NWAMUI_NCU(nwam_obj_proxy_get_proxy(NWAM_OBJ_PROXY_IFACE(other))));
}

static void
connect_ncu_signals(NwamNcuItem *self, NwamuiNcu *ncu)
{
    NwamNcuItemPrivate *prv = GET_PRIVATE(self);

    g_signal_connect (G_OBJECT(ncu), "notify::name",
      G_CALLBACK(on_nwam_ncu_notify), (gpointer)self);
    g_signal_connect (G_OBJECT(ncu), "notify::active",
      G_CALLBACK(on_nwam_ncu_notify), (gpointer)self);
/*     g_signal_connect (G_OBJECT(ncu), "notify::enabled", */
/*       G_CALLBACK(on_nwam_ncu_notify), (gpointer)self); */
    g_signal_connect (G_OBJECT(ncu), "notify::activation-mode",
      G_CALLBACK(on_nwam_ncu_notify), (gpointer)self);

    /* Now according to activation mode to set sensitive */
/*     prv->sensitive = nwamui_ncu_is_modifiable(NWAMUI_NCU(nwam_obj_proxy_get_proxy(NWAM_OBJ_PROXY_IFACE(self)))); */
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
sync_ncu(NwamNcuItem *self, NwamuiNcu *ncu, gpointer user_data)
{
    NwamNcuItemPrivate *prv = GET_PRIVATE(self);

    on_nwam_ncu_notify(G_OBJECT(ncu), NULL, (gpointer)self);

    /* Now according to activation mode to set sensitive */
/*     gtk_widget_set_sensitive(GTK_WIDGET(self), prv->sensitive); */
}

static void
on_nwam_ncu_toggled (GtkCheckMenuItem *item, gpointer data)
{
    NwamNcuItemPrivate *prv = GET_PRIVATE(item);
    NwamuiObject* ncu = NWAMUI_OBJECT(nwam_obj_proxy_get_proxy(NWAM_OBJ_PROXY_IFACE(item)));
    gboolean   active = nwamui_object_get_active(ncu);

    g_signal_handlers_block_by_func(G_OBJECT(item), (gpointer)on_nwam_ncu_toggled, data);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), active);
    g_signal_handlers_unblock_by_func(G_OBJECT(item), (gpointer)on_nwam_ncu_toggled, data);

    nwamui_object_set_active(ncu, !active);
}

static void
on_nwam_ncu_notify( GObject *gobject, GParamSpec *arg1, gpointer data)
{
	NwamNcuItem *self = NWAM_NCU_ITEM (data);
    NwamNcuItemPrivate *prv = GET_PRIVATE(self);
    NwamuiObject *object = NWAMUI_OBJECT(gobject);

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

        g_signal_handlers_block_by_func(G_OBJECT(self), (gpointer)on_nwam_ncu_toggled, NULL);
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(self),
          nwamui_object_get_active(object));
        g_signal_handlers_unblock_by_func(G_OBJECT(self), (gpointer)on_nwam_ncu_toggled, NULL);

    }

    if (!arg1 || g_ascii_strcasecmp(arg1->name, "activation-mode") == 0) {
        nwamui_cond_activation_mode_t mode = nwamui_object_get_activation_mode(object);

        switch (mode) {
        case NWAMUI_COND_ACTIVATION_MODE_PRIORITIZED:
            gtk_widget_set_sensitive(GTK_WIDGET(self), FALSE);
            break;
        case NWAMUI_COND_ACTIVATION_MODE_MANUAL:
            gtk_widget_set_sensitive(GTK_WIDGET(self), TRUE);
            break;
        default:
            g_assert_not_reached();
        }
    }

/*     if (!arg1 || g_ascii_strcasecmp(arg1->name, "enabled") == 0) { */

/*         gtk_widget_set_sensitive(GTK_WIDGET(self), */
/*           nwamui_object_get_enabled(NWAMUI_OBJECT(object))); */

/*     } */

    if (!arg1 || g_ascii_strcasecmp(arg1->name, "name") == 0) {
        gchar *lbl_name = NULL;
        gchar *type_str = NULL;
		
		if ( nwamui_ncu_get_ncu_type(NWAMUI_NCU(object)) == NWAMUI_NCU_TYPE_WIRELESS ){
            type_str = _("Wireless");
        }
        else {
            type_str = _("Wired");
        }
        lbl_name = g_strdup_printf(_("%s (%s)"), type_str, nwamui_object_get_name(object));

        /* If there is any underscores we need to replace them with two since
         * otherwise it's interpreted as a mnemonic
         */
        lbl_name = nwamui_util_encode_menu_label( &lbl_name );
        menu_item_set_label(GTK_MENU_ITEM(self), lbl_name);

		g_free(lbl_name);
    }
}

NwamuiNcu *
nwam_ncu_item_get_ncu (NwamNcuItem *self)
{
    NwamuiNcu *ncu;

    g_object_get(self, "proxy-object", &ncu, NULL);

    return ncu;
}

void
nwam_ncu_item_set_ncu (NwamNcuItem *self, NwamuiNcu *ncu)
{
    g_return_if_fail(NWAMUI_IS_NCU(ncu));

    g_object_set(self, "proxy-object", ncu, NULL);
}

