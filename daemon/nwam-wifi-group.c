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
#include <glib/gprintf.h>
#include <glib/gi18n.h>
#include <stdlib.h>

#include "nwam-wifi-group.h"
#include "nwam-wifi-action.h"

#define FALSEWLAN	"No wireless networks detected"

typedef struct _NwamWifiGroupPrivate        NwamWifiGroupPrivate;
struct _NwamWifiGroupPrivate {
    NwamuiDaemon *daemon;
};

#define NWAM_WIFI_GROUP_GET_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), NWAM_TYPE_WIFI_GROUP, NwamWifiGroupPrivate))

static void nwam_wifi_group_finalize(NwamWifiGroup *self);

G_DEFINE_TYPE(NwamWifiGroup,
  nwam_wifi_group,
  NWAM_TYPE_MENU_ACTION_GROUP)

static void
nwam_wifi_group_class_init(NwamWifiGroupClass *klass)
{
	GObjectClass *gobject_class;
    NwamMenuGroupClass *menu_group_class;

	gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->finalize = (void (*)(GObject*)) nwam_wifi_group_finalize;

    menu_group_class = NWAM_MENU_GROUP_CLASS(klass);

    g_type_class_add_private(klass, sizeof(NwamWifiGroupPrivate));
}

static void
nwam_wifi_group_init(NwamWifiGroup *self)
{
    NwamWifiGroupPrivate *prv = NWAM_WIFI_GROUP_GET_PRIVATE(self);
}

static void
nwam_wifi_group_finalize(NwamWifiGroup *self)
{
    NwamWifiGroupPrivate *prv = NWAM_WIFI_GROUP_GET_PRIVATE(self);

	G_OBJECT_CLASS(nwam_wifi_group_parent_class)->finalize(G_OBJECT(self));
}

NwamWifiGroup*
nwam_wifi_group_new(GtkUIManager *ui_manager,
  const gchar *place_holder)
{
    NwamWifiGroup *self;
    GtkAction *action;

    self = g_object_new(NWAM_TYPE_WIFI_GROUP,
      "ui-manager", ui_manager,
      "ui-placeholder", place_holder,
      NULL);

    {
        /* default "false" wlan which is invisible */
        const gchar *name = FALSEWLAN;
        action = GTK_ACTION(gtk_radio_action_new (name,
            name,
            NULL,
            NULL,
            (gint)NULL));
        gtk_action_set_visible(GTK_ACTION(action), FALSE);

        gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action), TRUE);

        nwam_menu_group_add_item(NWAM_MENU_GROUP(self),
          G_OBJECT(action),
          TRUE);

        g_object_unref (action);
    }

    return self;
}
