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
#ifndef NWAM_MENU_GROUP_H
#define NWAM_MENU_GROUP_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NWAM_TYPE_MENU_GROUP      (nwam_menu_group_get_type ())
#define NWAM_MENU_GROUP(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NWAM_TYPE_MENU_GROUP, NwamMenuGroup))
#define NWAM_MENU_GROUP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NWAM_TYPE_MENU_GROUP, NwamMenuGroupClass))
#define NWAM_IS_MENU_GROUP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NWAM_TYPE_MENU_GROUP))
#define NWAM_IS_MENU_GROUP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NWAM_TYPE_MENU_GROUP))
#define NWAM_MENU_GROUP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), NWAM_TYPE_MENU_GROUP, NwamMenuGroupClass))

typedef struct _NwamMenuGroup        NwamMenuGroup;
typedef struct _NwamMenuGroupClass   NwamMenuGroupClass;

struct _NwamMenuGroup {
	GObject parent;
};

struct _NwamMenuGroupClass {
	GObjectClass parent_class;

    void (*attach)(NwamMenuGroup *self);
    void (*detach)(NwamMenuGroup *self);
    void (*add_item)(NwamMenuGroup *self, GObject *item, gboolean top);
    void (*remove_item)(NwamMenuGroup *self, GObject *item);
    GObject *(*get_item_by_name)(NwamMenuGroup *self, const gchar *name);
    GObject *(*get_item_by_proxy)(NwamMenuGroup *self, GObject *proxy);
/*     void (*foreach)(NwamMenuGroup *self, GtkCallback callback, gpointer user_data); */
};

GType nwam_menu_group_get_type(void) G_GNUC_CONST;

void nwam_menu_group_attach(NwamMenuGroup *self);
void nwam_menu_group_detach(NwamMenuGroup *self);
void nwam_menu_group_add_item(NwamMenuGroup *self, GObject *item, gboolean top);
void nwam_menu_group_remove_item(NwamMenuGroup *self, GObject *item);
GObject* nwam_menu_group_get_item_by_name(NwamMenuGroup *self, const gchar *name);
GObject* nwam_menu_group_get_item_by_proxy(NwamMenuGroup *self, GObject *proxy);

G_END_DECLS

#endif  /* NWAM_MENU_GROUP_H */
