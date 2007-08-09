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
#include <gtk/gtkstatusicon.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "nwam-menuitem.h"

static void nwam_menu_item_set_property (GObject         *object,
					 guint            prop_id,
					 const GValue    *value,
					 GParamSpec      *pspec);
static void nwam_menu_item_get_property (GObject         *object,
					 guint            prop_id,
					 GValue          *value,
					 GParamSpec      *pspec);
static void nwam_menu_item_draw_indicator  (NwamMenuItem      *self,
					GdkRectangle          *area);

static void nwam_menu_item_finalize (NwamMenuItem *self);

enum {
	PROP_ZERO,
	PROP_LABEL,
	PROP_LWIDGET,
	PROP_RWIDGET,
};

struct _NwamMenuItemPrivate {
	GtkWidget *hbox;
	guint img_num;
	GtkWidget *image;
	GtkWidget *label;
	GtkWidget *widgetl;
	GtkWidget *widgetr;
};

G_DEFINE_TYPE (NwamMenuItem, nwam_menu_item, GTK_TYPE_CHECK_MENU_ITEM)

static void
nwam_menu_item_class_init (NwamMenuItemClass *klass)
{
	GObjectClass *gobject_class;
	GtkWidgetClass *widget_class;
	GtkCheckMenuItemClass *check_menu_item_class;
	GtkContainerClass *container_class;

	gobject_class = G_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);
	check_menu_item_class = GTK_CHECK_MENU_ITEM_CLASS (klass);
	container_class = GTK_CONTAINER_CLASS (klass);

//	container_class->forall = nwam_menu_item_forall;
//	container_class->remove = nwam_menu_item_remove;
  
	gobject_class->set_property = nwam_menu_item_set_property;
	gobject_class->get_property = nwam_menu_item_get_property;
	gobject_class->finalize = (void (*)(GObject*)) nwam_menu_item_finalize;
	
	check_menu_item_class->draw_indicator = nwam_menu_item_draw_indicator;
  
	g_object_class_install_property (gobject_class,
					 PROP_LABEL,
					 g_param_spec_string ("label",
							      N_("Label widget"),
							      N_("Child widget to appear next to the menu text"),
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class,
					 PROP_LWIDGET,
					 g_param_spec_object ("left-widget",
							      N_("Left widget, ref'd"),
							      N_("Left widget, ref'd"),
							      GTK_TYPE_WIDGET,
							      G_PARAM_READWRITE));
	g_object_class_install_property (gobject_class,
					 PROP_RWIDGET,
					 g_param_spec_object ("right-widget",
							      N_("Right widget, ref'd"),
							      N_("Right widget, ref'd"),
							      GTK_TYPE_WIDGET,
							      G_PARAM_READWRITE));
}

static void
nwam_menu_item_init (NwamMenuItem *self)
{
	GtkWidget* hbox = NULL;

	/* must keep a ref, due to gtkaction will unref gtkbox->child
	 * (due to setting a label as its child) since firstly gtk_container_add
	 * will set hbox as gtkbox->child
	 */
	hbox = GTK_WIDGET(g_object_ref(G_OBJECT(gtk_hbox_new (FALSE, 0))));
	gtk_container_add (GTK_CONTAINER (self), hbox);

	self->prv = g_new0 (NwamMenuItemPrivate, 1);
	self->prv->hbox = hbox;
}

static void
nwam_menu_item_set_property (GObject         *object,
			     guint            prop_id,
			     const GValue    *value,
			     GParamSpec      *pspec)
{
	NwamMenuItem *self = NWAM_MENU_ITEM (object);
	GtkWidget *hbox = self->prv->hbox;
	gchar *label = NULL;

	switch (prop_id)
	{
	case PROP_LABEL:
	{
		label = (gchar *) g_value_get_string (value);
		if (self->prv->label != NULL) {
			gtk_label_set_text (GTK_LABEL (self->prv->label), label);
		} else {
			self->prv->label = gtk_label_new (label);
			gtk_box_pack_start (GTK_BOX (hbox),
					    self->prv->label, TRUE, TRUE, 2);
			if (self->prv->widgetl || self->prv->widgetr) {
				gtk_box_reorder_child (GTK_BOX (hbox),
						       self->prv->label,
						       0);
			}
		}
		gtk_misc_set_alignment (GTK_MISC (self->prv->label),
					0.0, 0.5);
	}
	gtk_widget_show (self->prv->label);
	break;
	case PROP_LWIDGET:
	{
		g_assert (GTK_IS_WIDGET (g_value_get_object (value)));
		if (self->prv->widgetl == g_value_get_object (value))
			return;
		if (self->prv->widgetl != NULL ) {
			gtk_container_remove (GTK_CONTAINER (hbox),
					      self->prv->widgetl);
		}
		self->prv->widgetl = GTK_WIDGET (GTK_WIDGET (g_value_get_object(value)));
		gtk_box_pack_start (GTK_BOX (hbox),
				    self->prv->widgetl, FALSE, FALSE, 2);
		if (self->prv->label) {
			gtk_box_reorder_child (GTK_BOX (hbox),
					       self->prv->widgetl,
					       1);
		} else {
			gtk_box_reorder_child (GTK_BOX (hbox),
					       self->prv->widgetl,
					       0);
		}
		gtk_widget_show (self->prv->widgetl);
	}
	break;
	case PROP_RWIDGET:
	{
		g_assert (GTK_IS_WIDGET (g_value_get_object (value)));
		if (self->prv->widgetr == g_value_get_object (value))
			return;
		if (self->prv->widgetr != NULL) {
			gtk_container_remove (GTK_CONTAINER (hbox),
					      self->prv->widgetr);
		}
		self->prv->widgetr = GTK_WIDGET (GTK_WIDGET (g_value_get_object(value)));
		gtk_box_pack_start (GTK_BOX (hbox),
				    self->prv->widgetr, FALSE, FALSE, 2);
		gtk_widget_show (self->prv->widgetr);
	}
	break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nwam_menu_item_get_property (GObject         *object,
                                  guint            prop_id,
                                  GValue          *value,
                                  GParamSpec      *pspec)
{
	NwamMenuItem *self = NWAM_MENU_ITEM (object);
	GtkWidget *hbox = self->prv->hbox;

	switch (prop_id)
	{
	case PROP_LABEL:
		if (self->prv->label != NULL) {
			g_value_set_string (value, gtk_label_get_text (GTK_LABEL (self->prv->label)));
		} else {
			g_value_set_string (value, NULL);
		}
		break;
	case PROP_LWIDGET:
		g_value_set_object (value, (GObject*) self->prv->widgetl);
		break;
	case PROP_RWIDGET:
		g_value_set_object (value, (GObject*) self->prv->widgetr);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nwam_menu_item_forall (GtkContainer   *container,
                            gboolean    include_internals,
                            GtkCallback     callback,
                            gpointer        callback_data)
{
	NwamMenuItem *nwam_menu_item = NWAM_MENU_ITEM (container);
	(* GTK_CONTAINER_CLASS (nwam_menu_item_parent_class)->forall) (container,
									   include_internals,
									   callback,
									   callback_data);

}

/**
 * nwam_menu_item_new:
 * @returns: a new #NwamMenuItem.
 *
 * Creates a new #NwamMenuItem with an empty label.
 **/
GtkWidget*
nwam_menu_item_new (void)
{
	return g_object_new (NWAM_TYPE_MENU_ITEM, NULL);
}

/**
 * nwam_menu_item_new_with_label:
 * @label: the text of the menu item.
 * @returns: a new #NwamMenuItem.
 *
 * Creates a new #NwamMenuItem containing a label. 
 **/
GtkWidget*
nwam_menu_item_new_with_label (const gchar *label)
{
	NwamMenuItem *nwam_menu_item;

	nwam_menu_item = g_object_new (NWAM_TYPE_MENU_ITEM, NULL);

	g_object_set (G_OBJECT (nwam_menu_item),
		      "label", label,
		      NULL);
	return GTK_WIDGET(nwam_menu_item);
}

/** 
 * nwam_menu_item_set_widget:
 * @nwam_menu_item: a #NwamMenuItem.
 * @nwam: a widget to set as the nwam for the menu item.
 * 
 * Sets the nwam of @nwam_menu_item to the given widget.
 * Note that it depends on the show-menu-images setting whether
 * the nwam will be displayed or not.
 **/ 
void
nwam_menu_item_set_widget (NwamMenuItem *self,
			  GtkWidget *widget,
			  gint pos)
{
	g_return_if_fail (NWAM_IS_MENU_ITEM (self));
	g_assert (pos > 0);

	if (pos == 1) {
		g_object_set (G_OBJECT (self),
			      "left-widget", widget,
			      NULL);
		g_object_notify (G_OBJECT (self), "left-widget");
	}
	else if (pos == 2) {
		g_object_set (G_OBJECT (self),
			      "right-widget", widget,
			      NULL);
		g_object_notify (G_OBJECT (self), "right-widget");
	}
	else
		// extend ?
		return;
}

/**
 * nwam_menu_item_get_widget:
 * @nwam_menu_item: a #NwamMenuItem.
 * @returns: the widget set as nwam of @nwam_menu_item.
 *
 * Gets the widget that is currently set as the nwam of @nwam_menu_item.
 * See nwam_menu_item_set_nwam().
 **/
GtkWidget*
nwam_menu_item_get_widget (NwamMenuItem *self,
			  gint pos)
{
	GtkWidget *widget = NULL;

	g_return_val_if_fail (NWAM_IS_MENU_ITEM (self), NULL);
	g_assert (pos > 0);

	if (pos == 1)
		return self->prv->widgetl;
	else if (pos == 2)
		return self->prv->widgetr;
	else
		return NULL;
}

static void
nwam_menu_item_finalize (NwamMenuItem *self)
{
	g_object_unref (G_OBJECT(self->prv->hbox));
	g_free (self->prv);
	self->prv = NULL;

	(*G_OBJECT_CLASS(nwam_menu_item_parent_class)->finalize) (G_OBJECT (self));
}

static void
nwam_menu_item_draw_indicator  (NwamMenuItem      *self,
				GdkRectangle          *area)
{
	GtkWidget *widget = NULL;

	widget = gtk_bin_get_child(GTK_BIN(self));
	if (widget && widget != self->prv->hbox) {
		if (GTK_IS_LABEL(widget)) {
			g_object_set (G_OBJECT (self),
			      "label", gtk_label_get_text(GTK_LABEL(widget)),
			      NULL);
			gtk_container_remove (GTK_CONTAINER(self), widget);
			gtk_container_add (GTK_CONTAINER(self),
				GTK_WIDGET(g_object_ref(G_OBJECT(self->prv->hbox))));
			//gtk_widget_queue_resize (GTK_WIDGET(self));
			gtk_widget_show (GTK_WIDGET(self->prv->hbox));
		} else {
			g_assert_not_reached ();
		}
	}
	(*GTK_CHECK_MENU_ITEM_CLASS(nwam_menu_item_parent_class)->draw_indicator) (self, area);
}
