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
#ifndef NWAM_MENU_ITEM_H
#define NWAM_MENU_ITEM_H

#include <gtk/gtk.h>
#include "nwam-obj-proxy-iface.h"

G_BEGIN_DECLS

#define NWAM_TYPE_MENU_ITEM            (nwam_menu_item_get_type ())
#define NWAM_MENU_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NWAM_TYPE_MENU_ITEM, NwamMenuItem))
#define NWAM_MENU_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NWAM_TYPE_MENU_ITEM, NwamMenuItemClass))
#define NWAM_IS_MENU_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NWAM_TYPE_MENU_ITEM))
#define NWAM_IS_MENU_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NWAM_TYPE_MENU_ITEM))
#define NWAM_MENU_ITEM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), NWAM_TYPE_MENU_ITEM, NwamMenuItemClass))


typedef struct _NwamMenuItem			NwamMenuItem;
typedef struct _NwamMenuItemClass		NwamMenuItemClass;

struct _NwamMenuItem
{
    GtkCheckMenuItem menu_item;
};

typedef void (*nwam_menuitem_connect_object_t)(NwamMenuItem *self, GObject *object);
typedef void (*nwam_menuitem_disconnect_object_t)(NwamMenuItem *self, GObject *object);
typedef void (*nwam_menuitem_sync_object_t)(NwamMenuItem *self, GObject *object, gpointer user_data);
typedef gint (*nwam_menuitem_compare_t)(NwamMenuItem *self, NwamMenuItem *other);

struct _NwamMenuItemClass
{
    GtkCheckMenuItemClass             parent_class;
    nwam_menuitem_connect_object_t    connect_object;
    nwam_menuitem_disconnect_object_t disconnect_object;
    nwam_menuitem_sync_object_t       sync_object;
    nwam_menuitem_compare_t           compare;
};


GType nwam_menu_item_get_type (void) G_GNUC_CONST;
GtkWidget* nwam_menu_item_new (void);
GtkWidget* nwam_menu_item_new_with_object(GObject *object);
GtkWidget* nwam_menu_item_new_with_label (const gchar *label);

void nwam_menu_item_set_widget (NwamMenuItem *self, gint pos, GtkWidget *image);
GtkWidget* nwam_menu_item_get_widget (NwamMenuItem *self, gint pos);

/*
 * MAX_WIDGET_NUM:
 * max number of widget show on a nwam menuitem
 */
#ifndef MAX_WIDGET_NUM
#define MAX_WIDGET_NUM	2
#endif

/* Utils */
extern void menu_item_set_label(GtkMenuItem *item, const gchar *label);
extern void menu_item_set_markup(GtkMenuItem *item, const gchar *label);
extern gint nwam_menu_item_compare(NwamMenuItem *self, NwamMenuItem *other);

G_END_DECLS

#endif /* NWAM_MENU_ITEM_H */
