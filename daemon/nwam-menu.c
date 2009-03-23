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
#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include "nwam-menu.h"
#include "nwam-obj-proxy-iface.h"

#define DEBUG()	g_debug ("[[ %20s : %-4d ]]", __func__, __LINE__)

typedef struct _NwamMenuPrivate NwamMenuPrivate;
#define NWAM_MENU_GET_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), NWAM_TYPE_MENU, NwamMenuPrivate))
#define MENU_REMOVE_ITEM(menu, item)                            \
    {                                                           \
        g_assert(GTK_IS_MENU(menu) && GTK_IS_MENU_ITEM(item));  \
        gtk_container_remove(GTK_CONTAINER(menu), item);        \
    }

enum {
	PROP_ZERO,
	PROP_SECTIONS,
};

typedef struct _ForeachData {
	GtkTreeModelForeachFunc foreach_func;
	gpointer user_data;
	gpointer ret_data;
} ForeachData;

struct _NwamMenuPrivate {

	/* Each section is began with GtkSeparatorMenuItem, NULL means 0. We'd
     * better record every separators even though we don't really use some of
     * them. Must update it if Menu is re-design.
     */
    GtkWidget **section;
};

static void nwam_menu_finalize (NwamMenu *self);
static void nwam_menu_set_property(GObject         *object,
  guint            prop_id,
  const GValue    *value,
  GParamSpec      *pspec);
static void nwam_menu_get_property(GObject         *object,
  guint            prop_id,
  GValue          *value,
  GParamSpec      *pspec);

/* Utils */
static gint nwam_menu_item_compare(GtkWidget *a, GtkWidget *b);

/* GtkMenu utils */
static void menu_foreach_by_name(GtkWidget *widget, gpointer user_data);
static void menu_foreach_by_proxy(GtkWidget *widget, gpointer user_data);
static GtkMenuItem* menu_get_item_by_name(GtkMenu *menu, const gchar* name);
static GtkMenuItem* menu_get_item_by_proxy(GtkMenu *menu, GObject* proxy);
static void menu_section_get_positions(GtkMenu *menu,
  GtkWidget *start_sep, GtkWidget *end_sep,
  gint *ret_start_pos, gint *ret_end_pos);
static void menu_section_get_children(GtkMenu *menu,
  GtkWidget *start_sep, GtkWidget *end_sep,
  GList **ret_children, gint *ret_start_pos);

/* NwamMenu section utils */
static void nwam_menu_section_get_section_info(NwamMenu *self, gint sec_id,
  GtkWidget **menu, GtkWidget **start_sep, GtkWidget **end_sep);

G_DEFINE_TYPE (NwamMenu, nwam_menu, GTK_TYPE_MENU)

static void
nwam_menu_class_init (NwamMenuClass *klass)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->finalize = (void (*)(GObject*)) nwam_menu_finalize;
	gobject_class->set_property = nwam_menu_set_property;
	gobject_class->get_property = nwam_menu_get_property;

	g_type_class_add_private (klass, sizeof (NwamMenuPrivate));

	g_object_class_install_property (gobject_class,
      PROP_SECTIONS,
      g_param_spec_int("sections",
        N_("nwam menu sections"),
        N_("nwam menu sections"),
        0,
        G_MAXINT,
        0,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}

static void
nwam_menu_init (NwamMenu *self)
{
/*     NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self); */
}

static void
nwam_menu_finalize (NwamMenu *self)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);

    g_free(prv->section);

	G_OBJECT_CLASS(nwam_menu_parent_class)->finalize(G_OBJECT(self));
}

static void
nwam_menu_set_property(GObject         *object,
  guint            prop_id,
  const GValue    *value,
  GParamSpec      *pspec)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(object);

	switch (prop_id) {
	case PROP_SECTIONS: {
        gint new_num = g_value_get_int(value);
        prv->section = g_realloc(prv->section, new_num * sizeof(GtkWidget*));
    }
        break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nwam_menu_get_property(GObject         *object,
  guint            prop_id,
  GValue          *value,
  GParamSpec      *pspec)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(object);

	switch (prop_id)
	{
	case PROP_SECTIONS:
		g_value_set_int(value, sizeof(prv->section) / sizeof(GtkWidget*));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static gint
nwam_menu_item_compare(GtkWidget *a, GtkWidget *b)
{
    return 0;
}

GtkWidget*
nwam_menu_new(gint n_sections)
{
	return g_object_new(NWAM_TYPE_MENU, "sections", n_sections, NULL);
}

static void
nwam_menu_section_get_section_info(NwamMenu *self, gint sec_id,
  GtkWidget **menu, GtkWidget **start_sep, GtkWidget **end_sep)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);

    g_assert(menu);
    g_assert(start_sep);

    *start_sep = prv->section[sec_id];

    g_assert(*start_sep);

    if (GTK_IS_SEPARATOR_MENU_ITEM(*start_sep)) {
        *menu = gtk_widget_get_parent(*start_sep);
    } else if (GTK_IS_MENU(*start_sep)) {
        *menu = *start_sep;
        *start_sep = NULL;
    } else
        g_assert_not_reached();

    if (end_sep)
        *end_sep = NULL;
}

void
nwam_menu_section_prepend(NwamMenu *self, gint sec_id, GtkWidget *item)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);
    GtkWidget *menu, *start_sep;
    gint pos;

    g_assert(GTK_IS_MENU_ITEM(item) && !GTK_IS_SEPARATOR_MENU_ITEM(item));

    nwam_menu_section_get_section_info(self, sec_id, &menu, &start_sep, NULL);

    menu_section_get_positions(GTK_MENU(menu),
      start_sep, NULL,
      &pos, NULL);

    gtk_menu_shell_insert(GTK_MENU_SHELL(menu), item, pos);
}

void
nwam_menu_section_append(NwamMenu *self, gint sec_id, GtkWidget *item)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);
    GtkWidget *menu, *start_sep;
    gint pos;

    g_assert(GTK_IS_MENU_ITEM(item) && !GTK_IS_SEPARATOR_MENU_ITEM(item));

    nwam_menu_section_get_section_info(self, sec_id, &menu, &start_sep, NULL);

    menu_section_get_positions(GTK_MENU(menu),
      start_sep, NULL,
      NULL, &pos);

    gtk_menu_shell_insert(GTK_MENU_SHELL(menu), item, pos + 1);
}

void
nwam_menu_section_insert_sort(NwamMenu *self, gint sec_id, GtkWidget *item)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);
    GtkWidget *menu, *start_sep;
    GList *sorted, *i;
    gint start_pos;

    g_assert(GTK_IS_MENU_ITEM(item) && !GTK_IS_SEPARATOR_MENU_ITEM(item));

    nwam_menu_section_get_section_info(self, sec_id, &menu, &start_sep, NULL);

    /* Sorting */
    menu_section_get_children(GTK_MENU(menu),
      start_sep, NULL, &sorted, &start_pos);

    if (sorted) {
        sorted = g_list_sort(sorted, (GCompareFunc)nwam_menu_item_compare);

        for (i = sorted; i; i = g_list_next(i)) {
            if (item && nwam_menu_item_compare(item, GTK_WIDGET(i->data)) < 0) {
                gtk_menu_shell_insert(GTK_MENU_SHELL(menu), item, start_pos++);
                item = NULL;
            }
            gtk_menu_reorder_child(GTK_MENU(menu), GTK_WIDGET(i->data), start_pos++);
        }
        g_list_free(sorted);

        if (item)
            gtk_menu_shell_insert(GTK_MENU_SHELL(menu), item, start_pos++);
    } else {
        g_assert(start_pos == 0);
        gtk_menu_shell_insert(GTK_MENU_SHELL(menu), item, start_pos);
    }
}

void
nwam_menu_section_remove_item(NwamMenu *self, gint sec_id, GtkWidget *item)
{
    GtkWidget *menu;
    if ((menu = gtk_widget_get_parent(item)) != NULL)
        MENU_REMOVE_ITEM(menu, item);
}

void
nwam_menu_section_set(NwamMenu *self, gint sec_id, GtkWidget *w)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);
    prv->section[sec_id] = GTK_WIDGET(w);
}

void
nwam_menu_section_sort(NwamMenu *self, gint sec_id)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);
    GtkWidget *menu, *start_sep;
    GList *sorted, *i;
    gint start_pos;

    nwam_menu_section_get_section_info(self, sec_id, &menu, &start_sep, NULL);

    /* Sorting */
    menu_section_get_children(GTK_MENU(menu),
      start_sep, NULL, &sorted, &start_pos);

    if (sorted) {
        sorted = g_list_sort(sorted, (GCompareFunc)nwam_menu_item_compare);

        for (i = sorted; i; i = g_list_next(i)) {
            gtk_menu_reorder_child(GTK_MENU(menu), GTK_WIDGET(i->data), start_pos++);
        }
        g_list_free(sorted);
    }
}

void
nwam_menu_get_section_positions(NwamMenu *self, gint sec_id,
  gint *ret_start_pos, gint *ret_end_pos)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);
    GtkWidget *menu, *start_sep;

    nwam_menu_section_get_section_info(self, sec_id, &menu, &start_sep, NULL);

    menu_section_get_positions(GTK_MENU(menu),
      start_sep, NULL,
      ret_start_pos, ret_end_pos);
}

void
nwam_menu_section_delete(NwamMenu *self, gint sec_id)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);
    GtkWidget *menu, *start_sep;
    GList *children;

    nwam_menu_section_get_section_info(self, sec_id, &menu, &start_sep, NULL);

    /* Sorting */
    menu_section_get_children(GTK_MENU(menu),
      start_sep, NULL, &children, NULL);

    while (children) {
        MENU_REMOVE_ITEM(menu, children->data);
        children = g_list_delete_link(children, children);
    }

/*     gtk_widget_hide(start_sep); */
}

GtkWidget*
nwam_menu_section_get_item_by_proxy(NwamMenu *self, gint sec_id, GObject* proxy)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);
    GtkWidget *menu, *start_sep;
    GList *children;
    GtkWidget *item = NULL;

    nwam_menu_section_get_section_info(self, sec_id, &menu, &start_sep, NULL);

    /* Sorting */
    menu_section_get_children(GTK_MENU(menu),
      start_sep, NULL, &children, NULL);

    while (children) {
        if (nwam_obj_proxy_get_proxy(NWAM_OBJ_PROXY_IFACE(children->data)) ==
          (gpointer)proxy)
            item = GTK_WIDGET(children->data);
        children = g_list_delete_link(children, children);
    }
    return item;
}

/* GtkMenu related */
static void
menu_section_get_children(GtkMenu *menu,
  GtkWidget *start_sep, GtkWidget *end_sep,
  GList **ret_children, gint *ret_start_pos)
{
    GList *children = gtk_container_get_children(GTK_CONTAINER(menu));
    GList *i;
    gint pos = 0;

    g_assert(ret_children);

    /* Delete left part. */
    if (start_sep) {

        for (i = NULL; children && i != children; pos++) {
            if (children->data == (gpointer)start_sep) {
                /* Point to the next to the start separator. */
                i = g_list_next(children);
            }
            children = g_list_delete_link(children, children);
        }
    }

    /* Return the start pos. */
    if (ret_start_pos)
        *ret_start_pos = pos;

    /* Find the end separator. */
    if (children) {

        /* Find the right separator if it isn't passed. */
        if (end_sep) {
            for (i = children; i && i->data != (gpointer)end_sep; i = g_list_next(i)) {}
        } else {
            for (i = children; i && !GTK_IS_SEPARATOR_MENU_ITEM(i->data); i = g_list_next(i)) {}
        }
        /* Delete right part, i point to the right one. */
        if (i) {
            if (i == children) {
                children = NULL;
                g_debug("No sortable elements.");
            } else {
                g_list_previous(i)->next = NULL;
            }
            g_list_free(i);
        }
    } else
        g_message("Sort menu out of range.");

    /* Return found children, or NULL. */
    *ret_children = children;
}

static void
menu_section_get_positions(GtkMenu *menu,
  GtkWidget *start_sep, GtkWidget *end_sep,
  gint *ret_start_pos, gint *ret_end_pos)
{
    GList *children = gtk_container_get_children(GTK_CONTAINER(menu));

    if (ret_start_pos) {
        if (start_sep)
            *ret_start_pos = g_list_index(children, start_sep);
        else
            *ret_start_pos = 0;
    }

    if (ret_end_pos) {
        if (end_sep)
            *ret_end_pos = g_list_index(children, end_sep);
        else
            *ret_end_pos = g_list_length(children) - 1;
    }

    g_list_free(children);
}

static void
menu_foreach_by_name(GtkWidget *widget, gpointer user_data)
{
    ForeachData *data = (ForeachData *)user_data;
    GtkWidget *label;

    label = gtk_bin_get_child(GTK_BIN(widget));

    g_assert(GTK_IS_LABEL(label));

    if (g_ascii_strcasecmp(gtk_label_get_text(GTK_LABEL(label)),
        (gchar*)data->user_data) == 0) {

        g_assert(!data->ret_data);

        data->ret_data = (gpointer)g_object_ref(widget);
    }
}

static void
menu_foreach_by_proxy(GtkWidget *widget, gpointer user_data)
{
    ForeachData *data = (ForeachData *)user_data;

    g_assert(NWAM_IS_OBJ_PROXY_IFACE(widget));

    if (nwam_obj_proxy_get_proxy(NWAM_OBJ_PROXY_IFACE(widget)) == (gpointer)data->user_data) {
        
        g_assert(!data->ret_data);

        data->ret_data = (gpointer)g_object_ref(widget);
    }
}

static GtkMenuItem*
menu_get_item_by_name(GtkMenu *menu, const gchar* name)
{
    ForeachData data;

    data.ret_data = NULL;
    data.user_data = (gpointer)name;

    gtk_container_foreach(GTK_CONTAINER(menu), menu_foreach_by_name, (gpointer)&data);

    return (GtkMenuItem *)data.ret_data;
}

static GtkMenuItem*
menu_get_item_by_proxy(GtkMenu *menu, GObject* proxy)
{
    ForeachData data;

    data.ret_data = NULL;
    data.user_data = (gpointer)proxy;

    gtk_container_foreach(GTK_CONTAINER(menu), menu_foreach_by_proxy, (gpointer)&data);

    return (GtkMenuItem *)data.ret_data;
}


