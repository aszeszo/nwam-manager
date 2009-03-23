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
#ifndef NWAM_MENU_H
#define NWAM_MENU_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NWAM_TYPE_MENU            (nwam_menu_get_type ())
#define NWAM_MENU(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NWAM_TYPE_MENU, NwamMenu))
#define NWAM_MENU_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NWAM_TYPE_MENU, NwamMenuClass))
#define NWAM_IS_MENU(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NWAM_TYPE_MENU))
#define NWAM_IS_MENU_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NWAM_TYPE_MENU))
#define NWAM_MENU_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), NWAM_TYPE_MENU, NwamMenuClass))

typedef struct _NwamMenu        NwamMenu;
typedef struct _NwamMenuClass   NwamMenuClass;

struct _NwamMenu
{
	GtkMenu parent;
};

struct _NwamMenuClass {
	GtkMenuClass parent_class;	
};

GType            nwam_menu_get_type          (void) G_GNUC_CONST;

extern GtkWidget* nwam_menu_new(gint n_sections);

/* GtkMenu MACROs, must define ITEM_VARNAME before use them. */
#define MENU_APPEND_ITEM_BASE(menu, type, label, callback, user_data)   \
    g_assert(GTK_IS_MENU_SHELL(menu));                                  \
    ITEM_VARNAME = g_object_new(type, NULL);                            \
    if (!g_type_is_a(type, GTK_TYPE_SEPARATOR_MENU_ITEM)) {             \
        if (label)                                                      \
            menu_item_set_label(GTK_MENU_ITEM(ITEM_VARNAME), label);    \
        if (callback)                                                   \
            g_signal_connect((ITEM_VARNAME),                            \
              "activate", G_CALLBACK(callback), (gpointer)user_data);   \
    }                                                                   \
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), GTK_WIDGET(ITEM_VARNAME));
#define menu_append_item(menu, type, label, callback, user_data)        \
    {                                                                   \
        MENU_APPEND_ITEM_BASE(menu, type, label, callback, user_data);  \
    }
#define menu_append_image_item(menu, label, img, callback, user_data)   \
    {                                                                   \
        MENU_APPEND_ITEM_BASE(menu, GTK_TYPE_IMAGE_MENU_ITEM,           \
          label, callback, user_data);                                  \
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(ITEM_VARNAME), \
          GTK_WIDGET(GTK_IMAGE(img)));                                  \
    }
#define menu_append_stock_item(menu, label, stock, callback, user_data) \
    {                                                                   \
        MENU_APPEND_ITEM_BASE(menu, GTK_TYPE_IMAGE_MENU_ITEM,           \
          label, callback, user_data);                                  \
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(ITEM_VARNAME), \
          gtk_image_new_from_stock(stock, GTK_ICON_SIZE_MENU));         \
    }
#define menu_append_item_with_submenu(menu, type, label, submenu)   \
    {                                                               \
        g_assert(!g_type_is_a(type, GTK_TYPE_SEPARATOR_MENU_ITEM)); \
        MENU_APPEND_ITEM_BASE(menu, type, label, NULL, NULL);       \
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(ITEM_VARNAME),      \
          GTK_WIDGET(submenu));                                     \
    }
#define menu_append_separator(menu)                                 \
    {                                                               \
        MENU_APPEND_ITEM_BASE(menu, GTK_TYPE_SEPARATOR_MENU_ITEM,   \
          NULL, NULL, NULL);                                        \
    }
#define menu_append_section_separator(nwam_menu, menu, sec_id, is_first) \
    {                                                                   \
        GtkWidget *sec_widget;                                          \
        g_assert(NWAM_IS_MENU(nwam_menu));                              \
        g_assert(GTK_IS_MENU_SHELL(menu));                              \
        if (is_first)                                                   \
            sec_widget = GTK_WIDGET(menu);                              \
        else {                                                          \
            MENU_APPEND_ITEM_BASE(menu, GTK_TYPE_SEPARATOR_MENU_ITEM,   \
              NULL, NULL, NULL);                                        \
            sec_widget = GTK_WIDGET(ITEM_VARNAME);                      \
        }                                                               \
        nwam_menu_section_set(NWAM_MENU(nwam_menu), sec_id,             \
          GTK_WIDGET(sec_widget));                                      \
    }

/* NwamMenu section utils */
extern void nwam_menu_section_set(NwamMenu *self, gint sec_id, GtkWidget *w);
extern void nwam_menu_section_sort(NwamMenu *self, gint sec_id);
extern void nwam_menu_section_insert_sort(NwamMenu *self, gint sec_id, GtkWidget *item);
extern void nwam_menu_section_append(NwamMenu *self, gint sec_id, GtkWidget *item);
extern void nwam_menu_section_prepend(NwamMenu *self, gint sec_id, GtkWidget *item);
extern void nwam_menu_section_remove_item(NwamMenu *self, gint sec_id, GtkWidget *item);
extern void nwam_menu_section_delete(NwamMenu *self, gint sec_id);
extern GtkWidget *nwam_menu_section_get_item_by_proxy(NwamMenu *self, gint sec_id, GObject* proxy);

G_END_DECLS

#endif  /* NWAM_MENU_H */


