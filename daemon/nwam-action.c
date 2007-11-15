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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "nwam-menuitem.h"
#include "nwam-action.h"

static GtkWidget *create_menu_item (GtkAction *action);
static void connect_proxy        (GtkAction       *action, 
                                  GtkWidget       *proxy); 
static void disconnect_proxy     (GtkAction       *action, 
                                 GtkWidget       *proxy); 

struct _NwamActionPrivate {
	guint img_num;
	GtkWidget *image;
	GtkWidget *label;
	GtkWidget *widgetl;
	GtkWidget *widgetr;
};

G_DEFINE_TYPE (NwamAction, nwam_action, GTK_TYPE_RADIO_ACTION)

static void
nwam_action_class_init (NwamActionClass *klass)
{
	GObjectClass *gobject_class;
	GtkActionClass *action_class;

	gobject_class = G_OBJECT_CLASS (klass);
	action_class = GTK_ACTION_CLASS (klass);
	
	action_class->menu_item_type = NWAM_TYPE_MENU_ITEM;
	action_class->toolbar_item_type = GTK_TYPE_TOGGLE_TOOL_BUTTON;
	action_class->create_menu_item = create_menu_item;
	//action_class->connect_proxy = connect_proxy;
	//action_class->disconnect_proxy = disconnect_proxy;
}

static void
nwam_action_init (NwamAction *self)
{
}

NwamAction *
nwam_action_new (const gchar *name,
		       const gchar *label,
		       const gchar *tooltip,
		       const gchar *stock_id)
{
  NwamAction *action;

  action = g_object_new (NWAM_TYPE_ACTION,
			 "name", name,
			 "label", label,
			 "tooltip", tooltip,
			 "stock-id", stock_id,
			 NULL);

  return action;
}

static GtkWidget *
create_menu_item (GtkAction *action)
{
	NwamAction *toggle_action = NWAM_ACTION (action);
	
	return g_object_new(NWAM_TYPE_MENU_ITEM,
                        //"draw-as-radio", gtk_toggle_action_get_draw_as_radio (GTK_TOGGLE_ACTION (action)),
                        "draw-as-radio", TRUE,
                        NULL);
}

static void 
connect_proxy (GtkAction *action,  
               GtkWidget *proxy) 
{
	(* GTK_ACTION_CLASS(nwam_action_parent_class)->connect_proxy) (action, proxy);
    if (NWAM_IS_MENU_ITEM (proxy)) {
#if 0
        if (nwam_menu_item_get_widget (NWAM_MENU_ITEM(proxy), 0) == NULL)
            nwam_menu_item_set_widget (NWAM_MENU_ITEM(proxy),
                                       gtk_image_new (), 0);
        if (nwam_menu_item_get_widget (NWAM_MENU_ITEM(proxy), 1) == NULL)
            nwam_menu_item_set_widget (NWAM_MENU_ITEM(proxy),
                                       gtk_image_new (), 1);
        GList *childs;
        GList *idx;

        childs = gtk_container_get_children (proxy);
        for (idx = childs; idx; idx = idx->next) {
            g_debug ("connect_proxy found widget %s", gtk_widget_get_name (idx->data));
        }
#endif
    }
}

static void 
disconnect_proxy (GtkAction *action,  
                  GtkWidget *proxy) 
{ 
    if (NWAM_IS_MENU_ITEM (proxy)) {
#if 0
        GList *childs;
        GList *idx;

        childs = gtk_container_get_children (proxy);
        for (idx = childs; idx; idx = idx->next) {
            g_debug ("found widget %s", gtk_widget_get_name (idx->data));
        }
#endif
    }
	(* GTK_ACTION_CLASS(nwam_action_parent_class)->disconnect_proxy) (action, proxy);
} 
