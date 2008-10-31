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

#include "nwam-menu-radio-action-group.h"

typedef struct _NwamMenuRadioActionGroupPrivate NwamMenuRadioActionGroupPrivate;
struct _NwamMenuRadioActionGroupPrivate {
    GSList *radio_group;
};

static void nwam_menu_radio_action_group_finalize(NwamMenuRadioActionGroup *self);
static void nwam_menu_radio_action_group_add_item(NwamMenuRadioActionGroup *self,
  GtkAction *action,
  gboolean top);

G_DEFINE_TYPE(NwamMenuRadioActionGroup,
  nwam_menu_radio_action_group,
  NWAM_TYPE_MENU_GROUP)
#define NWAM_MENU_RADION_ACTION_GROUP_GET_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), NWAM_TYPE_MENU_RADIO_ACTION_GROUP, NwamMenuRadioActionGroupPrivate))

static void
nwam_menu_radio_action_group_class_init(NwamMenuRadioActionGroupClass *klass)
{
	GObjectClass *gobject_class;
    NwamMenuGroupClass *menu_group_class;

	gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->finalize = (void (*)(GObject*)) nwam_menu_radio_action_group_finalize;

    menu_group_class = NWAM_MENU_GROUP_CLASS(klass);
    menu_group_class->add_item = nwam_menu_radio_action_group_add_item;
}

static void
nwam_menu_radio_action_group_init(NwamMenuRadioActionGroup *self)
{
    NwamMenuRadioActionGroupPrivate *prv = NWAM_MENU_RADIO_ACTION_GROUP_GET_PRIVATE(self);
}

static void
nwam_menu_radio_action_group_finalize(NwamMenuRadioActionGroup *self)
{
    NwamMenuRadioActionGroupPrivate *prv = NWAM_MENU_RADIO_ACTION_GROUP_GET_PRIVATE(self);

	G_OBJECT_CLASS(nwam_menu_radio_action_group_parent_class)->finalize(G_OBJECT(self));
}

static void
nwam_menu_radio_action_group_add_item(NwamMenuRadioActionGroup *self,
  GtkAction *action,
  gboolean top)
{
    NwamMenuRadioActionGroupPrivate *prv = NWAM_MENU_RADIO_ACTION_GROUP_GET_PRIVATE(self);

    NWAM_MENU_GROUP_CLASS(nwam_menu_radio_action_group_parent_class)->add_item(self, action, top);
    gtk_radio_action_set_group (action, prv->radio_group);
    prv->radio_group = gtk_radio_action_get_group (action);
}
