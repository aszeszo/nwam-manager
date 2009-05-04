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

#include "nwam-menuitem.h"
#include "nwam-menu.h"
#include "libnwamui.h"

extern gboolean debug;
#define DEBUG()	g_debug ("[[ %20s : %-4d ]]", __func__, __LINE__)

typedef struct _NwamMenuPrivate NwamMenuPrivate;
#define NWAM_MENU_GET_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), NWAM_TYPE_MENU, NwamMenuPrivate))

enum {
    GET_SECTION_INDEX = 0,
    LAST_SIGNAL,
};

static guint nwam_menu_signals[LAST_SIGNAL] = { 0 };

enum {
	PROP_ZERO,
	PROP_SECTIONS,
};

typedef struct _ForeachData {
	GtkTreeModelForeachFunc foreach_func;
	gpointer user_data;
	gpointer ret_data;
} ForeachData;

typedef struct _MenuSection MenuSection;
struct _MenuSection {
	GtkWidget *lw;
	GtkWidget *rw;
    gint children_number;
};

/* Each section is began with GtkSeparatorMenuItem, NULL means 0. We'd
 * better record every separators even though we don't really use some of
 * them. Must update it if Menu is re-design.
 */
static MenuSection *prvsection = NULL;
static gint section_number = 0;

struct _NwamMenuPrivate {
    gpointer place_holder;
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
static void nwam_menu_real_add(GtkContainer      *container,
  GtkWidget         *widget);
static void nwam_menu_real_remove(GtkContainer      *container,
  GtkWidget         *widget);
static void nwam_menu_real_insert(GtkMenuShell *menu_shell,
  GtkWidget    *child,
  gint          position);
static void default_get_section_index(NwamMenu *self, GtkWidget *w, gint *index);

/* Utils */
static gint menu_item_compare(GtkWidget *a, GtkWidget *b);

/* GtkMenu utils */
static void menu_foreach_by_name(GtkWidget *widget, gpointer user_data);
static void menu_foreach_by_proxy(GtkWidget *widget, gpointer user_data);
static GtkMenuItem* menu_get_item_by_name(GtkMenu *menu, const gchar* name);
static GtkMenuItem* menu_get_item_by_proxy(GtkMenu *menu, GObject* proxy);
static GtkWidget* menu_section_get_menu_widget(MenuSection *sec);
static GtkWidget* menu_section_get_left_widget(MenuSection *sec);
static void menu_section_set_left_widget(MenuSection *sec, GtkWidget *w);
static GtkWidget* menu_section_get_right_widget(MenuSection *sec);
static void menu_section_set_right_widget(MenuSection *sec, GtkWidget *w);
static MenuSection* menu_section_new(GtkWidget *lw, GtkWidget *rw);
static void menu_section_delete(MenuSection *sec);
static gint menu_section_get_children_num(MenuSection *sec);
static gboolean menu_section_has_child(MenuSection *sec, GtkWidget *child);
static void menu_section_get_children(MenuSection *sec, GList **ret_children, gint *ret_start_pos);
static void menu_section_get_positions(MenuSection *sec, gint *ret_start_pos, gint *ret_end_pos);
static void menu_section_increase_children(MenuSection *sec);
static void menu_section_decrease_children(MenuSection *sec);

/* NwamMenu section utils */
static void nwam_menu_get_section_positions(NwamMenu *self, gint sec_id,
  gint *ret_start_pos, gint *ret_end_pos);


G_DEFINE_TYPE(NwamMenu, nwam_menu, GTK_TYPE_MENU)

static void
nwam_menu_class_init (NwamMenuClass *klass)
{
	GObjectClass *gobject_class;
    GtkContainerClass *container_class;
    GtkMenuShellClass *menu_shell_class;

	gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->finalize = (void (*)(GObject*)) nwam_menu_finalize;
	gobject_class->set_property = nwam_menu_set_property;
	gobject_class->get_property = nwam_menu_get_property;

    container_class = GTK_CONTAINER_CLASS(klass);
/*     container_class->add = nwam_menu_real_add; */
    container_class->remove = nwam_menu_real_remove;

    menu_shell_class = GTK_MENU_SHELL_CLASS(klass);
    menu_shell_class->insert = nwam_menu_real_insert;

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

    nwam_menu_signals[GET_SECTION_INDEX] =
      g_signal_new ("get_section_index",
        G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
        G_STRUCT_OFFSET (NwamMenuClass, get_section_index),
        NULL, NULL,
        marshal_VOID__OBJECT_POINTER,
        G_TYPE_NONE,                  /* Return Type */
        2,                            /* Number of Args */
        GTK_TYPE_WIDGET,
        G_TYPE_POINTER);               /* Types of Args */

    klass->get_section_index = default_get_section_index;
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

    /* Ref count is correct here, or we just leave this leak.  */
/*     g_free(prvsection); */

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
        gsize old_size = 0;

        if (section_number >= new_num)
            return;

        prvsection = g_realloc(prvsection, new_num * sizeof(MenuSection));
        /* Zero up to initilize. */
        memset(prvsection + section_number*sizeof(MenuSection), 0, (new_num-section_number)*sizeof(MenuSection));
        section_number = new_num;
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
		g_value_set_int(value, section_number);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nwam_menu_real_add(GtkContainer *container, GtkWidget *widget)
{
}

static void
nwam_menu_real_remove(GtkContainer *container, GtkWidget *widget)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(container);
    gint index = -1;

    g_signal_emit(container, nwam_menu_signals[GET_SECTION_INDEX], 0,
      widget, (gpointer)&index);

    if (debug) {
        if (NWAM_IS_OBJ_PROXY_IFACE(widget)) {
            NwamuiObject *object = NWAMUI_OBJECT(nwam_obj_proxy_get_proxy(NWAM_OBJ_PROXY_IFACE(widget)));
            gchar *name = nwamui_object_get_name(object);
            g_assert(!GTK_IS_SEPARATOR(widget));
            g_debug("%s: \"%s\" got index %d", __func__, name, index);
            g_free(name);
        }
    }

    if (index >= 0) {
        GtkWidget *menu = menu_section_get_menu_widget(&prvsection[index]);
        /* Section related, menu secion may have a different root menu. */
        g_assert(menu_section_has_child(&prvsection[index], widget));
        g_assert(NWAM_IS_MENU(menu));

        GTK_CONTAINER_CLASS(nwam_menu_parent_class)->remove(GTK_CONTAINER(menu), widget);
        menu_section_decrease_children(&prvsection[index]);
    } else {
        GTK_CONTAINER_CLASS(nwam_menu_parent_class)->remove(container, widget);
    }
}

static void
nwam_menu_real_insert(GtkMenuShell *menu_shell,
  GtkWidget *child,
  gint position)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(menu_shell);
    gint index = -1;

    g_signal_emit(menu_shell, nwam_menu_signals[GET_SECTION_INDEX], 0,
      child, (gpointer)&index);

    if (debug) {
        if (NWAM_IS_OBJ_PROXY_IFACE(child)) {
            NwamuiObject *object = NWAMUI_OBJECT(nwam_obj_proxy_get_proxy(NWAM_OBJ_PROXY_IFACE(child)));
            gchar *name = nwamui_object_get_name(object);
            g_assert(!GTK_IS_SEPARATOR(child));
            g_debug("%s: \"%s\" got index %d position %d", __func__, name, index, position);
            g_free(name);
        }
    }

    if (index >= 0) {
        GtkWidget *menu = menu_section_get_menu_widget(&prvsection[index]);
        GList *sorted, *i;
        gint start_pos;

        /* Section related */
        g_assert(NWAM_IS_MENU(menu));
        g_assert(GTK_IS_MENU_ITEM(child) && !GTK_IS_SEPARATOR_MENU_ITEM(child));

        /* Sorting */
        menu_section_get_children(&prvsection[index], &sorted, &start_pos);

        if (sorted) {
            sorted = g_list_sort(sorted, (GCompareFunc)menu_item_compare);

            for (i = sorted; i; i = g_list_next(i)) {
                if (child && menu_item_compare(child, GTK_WIDGET(i->data)) < 0) {
                    GTK_MENU_SHELL_CLASS(nwam_menu_parent_class)->insert(GTK_MENU_SHELL(menu), child, start_pos++);
                    child = NULL;
                }
                gtk_menu_reorder_child(GTK_MENU(menu), GTK_WIDGET(i->data), start_pos++);
            }
            g_list_free(sorted);

            if (child)
                GTK_MENU_SHELL_CLASS(nwam_menu_parent_class)->insert(GTK_MENU_SHELL(menu), child, start_pos++);
        } else {
            g_assert(start_pos == 0);
            GTK_MENU_SHELL_CLASS(nwam_menu_parent_class)->insert(GTK_MENU_SHELL(menu), child, start_pos);
        }
        menu_section_increase_children(&prvsection[index]);
    } else {
        GTK_MENU_SHELL_CLASS(nwam_menu_parent_class)->insert(menu_shell, child, position);
    }
}

static void
default_get_section_index(NwamMenu *self, GtkWidget *w, gint *index)
{
    *index = -1;
}

static gint
menu_item_compare(GtkWidget *a, GtkWidget *b)
{
    if (NWAM_IS_MENU_ITEM(a) && NWAM_IS_MENU_ITEM(b))
        return nwam_menu_item_compare(NWAM_MENU_ITEM(a), NWAM_MENU_ITEM(b));
    return 0;
}

GtkWidget*
nwam_menu_new(gint n_sections)
{
	return g_object_new(NWAM_TYPE_MENU, "sections", n_sections, NULL);
}

static GtkWidget*
menu_section_get_menu_widget(MenuSection *sec)
{
    g_return_val_if_fail(sec, NULL);
    if (!GTK_IS_MENU(sec->lw)) {
        return gtk_widget_get_parent(sec->lw);
    } else
        return GTK_WIDGET(sec->lw);
}

static GtkWidget*
menu_section_get_left_widget(MenuSection *sec)
{
    g_return_val_if_fail(sec, NULL);
    if (!GTK_IS_MENU(sec->lw)) {
        return (GtkWidget *)sec->lw;
    } else
        return NULL;
}

static void
menu_section_set_left_widget(MenuSection *sec, GtkWidget *w)
{
    /* Must has left side or parent menu. */
    g_assert(sec);
    g_assert(GTK_IS_MENU_ITEM(w) || NWAM_IS_MENU(w));

    sec->lw = (GtkWidget *)w;

    if (GTK_IS_MENU_ITEM(w)) {
        if (sec->children_number > 0)
            gtk_widget_show(w);
        else
            gtk_widget_hide(w);
    }
}

static GtkWidget*
menu_section_get_right_widget(MenuSection *sec)
{
    g_return_val_if_fail(sec, NULL);
    return (GtkWidget *)sec->rw;
}

static void
menu_section_set_right_widget(MenuSection *sec, GtkWidget *w)
{
    /* May not has right side. */
    g_assert(sec);
    g_assert(w == NULL || GTK_IS_MENU_ITEM(w));
    if ((sec->rw = (GtkWidget *)w) != NULL) {
        if (sec->children_number > 0)
            gtk_widget_show(w);
        else
            gtk_widget_hide(w);
    }
}

static MenuSection*
menu_section_new(GtkWidget *lw, GtkWidget *rw)
{
    MenuSection *sec = g_new(MenuSection, 1);
    menu_section_set_left_widget(sec, lw);
    menu_section_set_right_widget(sec, rw);
    sec->children_number = 0;
    return sec;
}

static void
menu_section_delete(MenuSection *sec)
{
    g_free(sec);
}

static gint
menu_section_get_children_num(MenuSection *sec)
{
    GtkWidget *menu = menu_section_get_menu_widget(sec);
    GtkWidget *start_sep = menu_section_get_left_widget(sec);
    GtkWidget *end_sep = menu_section_get_right_widget(sec);
    GList *children = gtk_container_get_children(GTK_CONTAINER(menu));
    gboolean start = FALSE;
    gint num = 0;

    for (; children;) {
        if (start)
            num++;
        if (children->data == (gpointer)start_sep) {
            start = TRUE;
        } else if ((end_sep && children->data == (gpointer)end_sep) ||
          GTK_IS_SEPARATOR_MENU_ITEM(children->data)) {
            break;
        }
        children = g_list_delete_link(children, children);
    }
    if (children)
        g_list_free(children);
    return num;
}

static gboolean
menu_section_has_child(MenuSection *sec, GtkWidget *child)
{
    GtkWidget *menu = menu_section_get_menu_widget(sec);
    GtkWidget *start_sep = menu_section_get_left_widget(sec);
    GtkWidget *end_sep = menu_section_get_right_widget(sec);
    GList *children = gtk_container_get_children(GTK_CONTAINER(menu));
    gboolean found = FALSE;
    gboolean failed = FALSE;

    g_assert(child != start_sep && child != end_sep);

    for (; children;) {
        if (children->data == (gpointer)child) {
            found = TRUE;
        } else if (children->data == (gpointer)start_sep) {
            if (found) { failed = TRUE; break; }
        } else if ((end_sep && children->data == (gpointer)end_sep) ||
          GTK_IS_SEPARATOR_MENU_ITEM(children->data)) {
            if (!found) failed = TRUE;
            break;
        }
        children = g_list_delete_link(children, children);
    }
    if (children)
        g_list_free(children);
    return (found && !failed);
}

static void
menu_section_get_children(MenuSection *sec, GList **ret_children, gint *ret_start_pos)
{
    GtkWidget *menu = menu_section_get_menu_widget(sec);
    GtkWidget *start_sep = menu_section_get_left_widget(sec);
    GtkWidget *end_sep = menu_section_get_right_widget(sec);
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
menu_section_get_positions(MenuSection *sec, gint *ret_start_pos, gint *ret_end_pos)
{
    GtkWidget *menu = menu_section_get_menu_widget(sec);
    GtkWidget *start_sep = menu_section_get_left_widget(sec);
    GtkWidget *end_sep = menu_section_get_right_widget(sec);
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
menu_section_increase_children(MenuSection *sec)
{
    g_assert(sec->children_number >= 0);
    if (sec->children_number++ == 0) {
        if (sec->lw && GTK_IS_MENU_ITEM(sec->lw))
            gtk_widget_show(sec->lw);
        if (sec->rw && GTK_IS_MENU_ITEM(sec->rw))
            gtk_widget_show(sec->rw);
    }
}

static void
menu_section_decrease_children(MenuSection *sec)
{
    g_assert(sec->children_number >= 0);
    if (--sec->children_number == 0) {
        if (sec->lw && GTK_IS_MENU_ITEM(sec->lw))
            gtk_widget_hide(sec->lw);
        if (sec->rw && GTK_IS_MENU_ITEM(sec->rw))
            gtk_widget_hide(sec->rw);
    }
}

void
nwam_menu_section_set_left(NwamMenu *self, gint sec_id, GtkWidget *w)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);

    menu_section_set_left_widget(&prvsection[sec_id], w);
}

void
nwam_menu_section_set_right(NwamMenu *self, gint sec_id, GtkWidget *w)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);

    menu_section_set_right_widget(&prvsection[sec_id], w);
}

void
nwam_menu_section_set_visible(NwamMenu *self, gint sec_id, gboolean visible)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);
    GtkWidget *menu = menu_section_get_menu_widget(&prvsection[sec_id]);
    GtkWidget *lw = menu_section_get_left_widget(&prvsection[sec_id]);
    GtkWidget *rw = menu_section_get_right_widget(&prvsection[sec_id]);
    void (*func)(GtkWidget*);

    if (visible)
        func = gtk_widget_show;
    else
        func = gtk_widget_hide;

    nwam_menu_section_foreach(self, sec_id, (GFunc)func, NULL);

    if (lw)
        func(lw);
    if (rw)
        func(rw);
}

void
nwam_menu_section_sort(NwamMenu *self, gint sec_id)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);
    GtkWidget *menu = menu_section_get_menu_widget(&prvsection[sec_id]);
    GList *sorted, *i;
    gint start_pos;

    /* Sorting */
    menu_section_get_children(&prvsection[sec_id], &sorted, &start_pos);

    if (sorted) {
        sorted = g_list_sort(sorted, (GCompareFunc)menu_item_compare);

        for (i = sorted; i; i = g_list_next(i)) {
            gtk_menu_reorder_child(GTK_MENU(menu), GTK_WIDGET(i->data), start_pos++);
        }
        g_list_free(sorted);
    }
}

static void
nwam_menu_get_section_positions(NwamMenu *self, gint sec_id,
  gint *ret_start_pos, gint *ret_end_pos)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);

    menu_section_get_positions(&prvsection[sec_id], ret_start_pos, ret_end_pos);
}

void
nwam_menu_section_delete(NwamMenu *self, gint sec_id)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);
    GtkWidget *menu = menu_section_get_menu_widget(&prvsection[sec_id]);
    GList *children;

    /* Sorting */
    menu_section_get_children(&prvsection[sec_id], &children, NULL);

    while (children) {
        REMOVE_MENU_ITEM(menu, children->data);
        children = g_list_delete_link(children, children);
    }
}

void
nwam_menu_section_foreach(NwamMenu *self, gint sec_id, GFunc func, gpointer user_data)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);
    GtkWidget *menu = menu_section_get_menu_widget(&prvsection[sec_id]);
    GList *children;

    if (func) {
        /* Sorting */
        menu_section_get_children(&prvsection[sec_id], &children, NULL);

        while (children) {
            func(children->data, user_data);
            children = g_list_delete_link(children, children);
        }
    }
}

GtkWidget*
nwam_menu_section_get_item_by_proxy(NwamMenu *self, gint sec_id, GObject* proxy)
{
    NwamMenuPrivate *prv = NWAM_MENU_GET_PRIVATE(self);
    GtkWidget *menu = menu_section_get_menu_widget(&prvsection[sec_id]);
    GList *children;
    GtkWidget *item = NULL;

    /* Sorting */
    menu_section_get_children(&prvsection[sec_id], &children, NULL);

    while (children) {
        if (nwam_obj_proxy_get_proxy(NWAM_OBJ_PROXY_IFACE(children->data)) ==
          (gpointer)proxy)
            item = GTK_WIDGET(children->data);
        children = g_list_delete_link(children, children);
    }
    return item;
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


