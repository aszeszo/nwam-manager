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
    void (*get_section_index)(NwamMenu *self, GtkWidget *w, gint *index);
};

GType            nwam_menu_get_type          (void) G_GNUC_CONST;

/* GtkMenu MACROs, must define ITEM_VARNAME before use them. */
#define MENU_CREATE_ITEM_BASE(menu, type, label, callback, user_data)   \
    g_assert(GTK_IS_MENU_SHELL(menu));                                  \
    ITEM_VARNAME = g_object_new(type, NULL);                            \
    if (!g_type_is_a(type, GTK_TYPE_SEPARATOR_MENU_ITEM)) {             \
        if (label)                                                      \
            menu_item_set_label(GTK_MENU_ITEM(ITEM_VARNAME), label);    \
        if (callback)                                                   \
            g_signal_connect((ITEM_VARNAME),                            \
              "activate", G_CALLBACK(callback), (gpointer)user_data);   \
    }
#define menu_append_item(menu, type, label, callback, user_data)        \
    menu_append_item_with_code(menu, type, label, callback, user_data, {})
#define menu_append_item_with_code(menu, type, label, callback, user_data, _C_) \
    {                                                                   \
        MENU_CREATE_ITEM_BASE(menu, type, label, callback, user_data);  \
        { _C_; }                                                        \
        ADD_MENU_ITEM(menu, GTK_WIDGET(ITEM_VARNAME));                  \
    }
#define menu_append_image_item(menu, label, img, callback, user_data)   \
    {                                                                   \
        MENU_CREATE_ITEM_BASE(menu, GTK_TYPE_IMAGE_MENU_ITEM,           \
          label, callback, user_data);                                  \
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(ITEM_VARNAME), \
          GTK_WIDGET(GTK_IMAGE(img)));                                  \
        ADD_MENU_ITEM(menu, GTK_WIDGET(ITEM_VARNAME));                  \
    }
#define menu_append_stock_item(menu, label, stock, callback, user_data) \
    {                                                                   \
        MENU_CREATE_ITEM_BASE(menu, GTK_TYPE_IMAGE_MENU_ITEM,           \
          label, callback, user_data);                                  \
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(ITEM_VARNAME), \
          gtk_image_new_from_stock(stock, GTK_ICON_SIZE_MENU));         \
        ADD_MENU_ITEM(menu, GTK_WIDGET(ITEM_VARNAME));                  \
    }
#define menu_append_item_with_submenu(menu, type, label, submenu)   \
    {                                                               \
        g_assert(!g_type_is_a(type, GTK_TYPE_SEPARATOR_MENU_ITEM)); \
        MENU_CREATE_ITEM_BASE(menu, type, label, NULL, NULL);       \
        ADD_MENU_ITEM(menu, GTK_WIDGET(ITEM_VARNAME));              \
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(ITEM_VARNAME),      \
          GTK_WIDGET(submenu));                                     \
    }
#define menu_append_separator(menu)                                 \
    {                                                               \
        MENU_CREATE_ITEM_BASE(menu, GTK_TYPE_SEPARATOR_MENU_ITEM,   \
          NULL, NULL, NULL);                                        \
        ADD_MENU_ITEM(menu, GTK_WIDGET(ITEM_VARNAME));              \
    }
#define START_MENU_SECTION_SEPARATOR(nwam_menu, sec_id, has_sep)        \
    {                                                                   \
        GtkWidget *sec_widget;                                          \
        g_assert(NWAM_IS_MENU(nwam_menu));                              \
        if (has_sep) {                                                  \
            MENU_CREATE_ITEM_BASE(nwam_menu, GTK_TYPE_SEPARATOR_MENU_ITEM, \
              NULL, NULL, NULL);                                        \
            ADD_MENU_ITEM(nwam_menu, GTK_WIDGET(ITEM_VARNAME));              \
            sec_widget = GTK_WIDGET(ITEM_VARNAME);                      \
        } else {                                                        \
            sec_widget = GTK_WIDGET(nwam_menu);                         \
        }                                                               \
        nwam_menu_section_set_left(NWAM_MENU(nwam_menu), sec_id,        \
          GTK_WIDGET(sec_widget));                                      \
    }
#define END_MENU_SECTION_SEPARATOR(nwam_menu, sec_id, has_sep)          \
    {                                                                   \
        GtkWidget *sec_widget;                                          \
        g_assert(NWAM_IS_MENU(nwam_menu));                              \
        if (has_sep) {                                                  \
            MENU_CREATE_ITEM_BASE(nwam_menu, GTK_TYPE_SEPARATOR_MENU_ITEM, \
              NULL, NULL, NULL);                                        \
            ADD_MENU_ITEM(nwam_menu, GTK_WIDGET(ITEM_VARNAME));              \
            sec_widget = GTK_WIDGET(ITEM_VARNAME);                      \
        } else {                                                        \
            sec_widget = NULL;                                          \
        }                                                               \
        nwam_menu_section_set_right(NWAM_MENU(nwam_menu), sec_id,       \
          GTK_WIDGET(sec_widget));                                      \
    }
#define ADD_MENU_ITEM(menu, item)                               \
    {                                                           \
        g_assert(GTK_IS_MENU(menu) && GTK_IS_MENU_ITEM(item));  \
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);      \
        gtk_widget_show(GTK_WIDGET(item));                      \
    }
#define REMOVE_MENU_ITEM(menu, item)                            \
    {                                                           \
        GtkWidget *parent;                                      \
        g_assert(GTK_IS_MENU(menu) && GTK_IS_MENU_ITEM(item));  \
        if ((parent = gtk_widget_get_parent(item)) != NULL)     \
            gtk_container_remove(GTK_CONTAINER(parent), item);  \
    }

extern GtkWidget* nwam_menu_new(gint n_sections);

/* NwamMenu section utils */
extern void nwam_menu_section_set_left(NwamMenu *self, gint sec_id, GtkWidget *w);
extern void nwam_menu_section_set_right(NwamMenu *self, gint sec_id, GtkWidget *w);
extern void nwam_menu_section_set_visible(NwamMenu *self, gint sec_id, gboolean visible);
extern void nwam_menu_section_set_sensitive(NwamMenu *self, gint sec_id, gboolean sensitive);
extern void nwam_menu_section_sort(NwamMenu *self, gint sec_id);
extern void nwam_menu_section_delete(NwamMenu *self, gint sec_id);
extern void nwam_menu_section_foreach(NwamMenu *self, gint sec_id, GFunc func, gpointer user_data);

extern GtkWidget *nwam_menu_section_get_item_by_proxy(NwamMenu *self, gint sec_id, GObject* proxy);

G_END_DECLS

#endif  /* NWAM_MENU_H */


