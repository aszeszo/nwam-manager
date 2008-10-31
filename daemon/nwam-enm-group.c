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

#include "nwam-enm-group.h"
#include "nwam-obj-proxy-iface.h"
#include "nwam-enm-action.h"
#include "libnwamui.h"

#define DEBUG()	g_debug ("[[ %20s : %-4d ]]", __func__, __LINE__)
typedef struct _NwamEnmGroupPrivate NwamEnmGroupPrivate;
struct _NwamEnmGroupPrivate {
    NwamuiDaemon *daemon;
};

#define NWAM_ENM_GROUP_GET_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), NWAM_TYPE_ENM_GROUP, NwamEnmGroupPrivate))

static void nwam_enm_group_finalize(NwamEnmGroup *self);
static void nwam_enm_group_attach(NwamEnmGroup *self);
static void nwam_enm_group_detach(NwamEnmGroup *self);
static void nwam_enm_group_add_item(NwamEnmGroup *self,
  GtkAction *action,
  gboolean top);
static void nwam_enm_group_remove_item(NwamEnmGroup *self,
  GtkAction *action);
static GObject* nwam_enm_group_get_item_by_name(NwamEnmGroup *self, const gchar* action_name);
static GObject* nwam_enm_group_get_item_by_proxy(NwamEnmGroup *self, GObject* proxy);

G_DEFINE_TYPE(NwamEnmGroup,
  nwam_enm_group,
  NWAM_TYPE_MENU_ACTION_GROUP)

static void
nwam_enm_group_class_init(NwamEnmGroupClass *klass)
{
	GObjectClass *gobject_class;
    NwamMenuGroupClass *menu_group_class;

	gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->finalize = (void (*)(GObject*)) nwam_enm_group_finalize;

    menu_group_class = NWAM_MENU_GROUP_CLASS(klass);
/*     menu_group_class->attach = nwam_enm_group_attach; */
/*     menu_group_class->detach = nwam_enm_group_detach; */
/*     menu_group_class->add_item = nwam_enm_group_add_item; */
/*     menu_group_class->remove_item = nwam_enm_group_remove_item; */
    menu_group_class->get_item_by_name = nwam_enm_group_get_item_by_name;
    menu_group_class->get_item_by_proxy = nwam_enm_group_get_item_by_proxy;
}

static void
nwam_enm_group_init(NwamEnmGroup *self)
{
    NwamEnmGroupPrivate *prv = NWAM_ENM_GROUP_GET_PRIVATE(self);
	GList *idx = NULL;
	GtkAction* action = NULL;

    prv->daemon = nwamui_daemon_get_instance();

	for (idx = nwamui_daemon_get_enm_list(prv->daemon); idx; idx = g_list_next(idx)) {
		action = GTK_ACTION(nwam_enm_action_new(NWAMUI_ENM(idx->data)));
        //nwam_menuitem_proxy_new(action, vpn_group);
        nwam_menu_group_add_item(NWAM_MENU_GROUP(self),
          action,
          TRUE);

		g_object_unref(action);
	}
}

static void
nwam_enm_group_finalize(NwamEnmGroup *self)
{
    NwamEnmGroupPrivate *prv = NWAM_ENM_GROUP_GET_PRIVATE(self);

    g_object_unref(prv->daemon);

	G_OBJECT_CLASS(nwam_enm_group_parent_class)->finalize(G_OBJECT(self));
}

static GObject*
nwam_enm_group_get_item_by_name(NwamEnmGroup *self, const gchar* action_name)
{
    if (action_name) {
        return NWAM_MENU_GROUP_CLASS(nwam_enm_group_parent_class)->get_item_by_name(NWAM_MENU_GROUP(self), action_name);
    } else {
        return NULL;
    }
}

static GObject*
nwam_enm_group_get_item_by_proxy(NwamEnmGroup *self, GObject* proxy)
{
    if (proxy) {
        return NWAM_MENU_GROUP_CLASS(nwam_enm_group_parent_class)->get_item_by_proxy(NWAM_MENU_GROUP(self), proxy);
    } else {
        return NULL;
    }
}

static void
nwam_enm_group_add_item(NwamEnmGroup *self, GtkAction *action, gboolean top)
{
    NWAM_MENU_GROUP_CLASS(nwam_enm_group_parent_class)->add_item(NWAM_MENU_GROUP(self), action, top);
}

static void
nwam_enm_group_remove_item(NwamEnmGroup *self, GtkAction *action)
{
    NWAM_MENU_GROUP_CLASS(nwam_enm_group_parent_class)->remove_item(NWAM_MENU_GROUP(self), action);
}

static void
nwam_enm_group_attach(NwamEnmGroup *self)
{
    NWAM_MENU_GROUP_CLASS(nwam_enm_group_parent_class)->attach(NWAM_MENU_GROUP(self));
}

static void
nwam_enm_group_detach(NwamEnmGroup *self)
{
    NWAM_MENU_GROUP_CLASS(nwam_enm_group_parent_class)->detach(NWAM_MENU_GROUP(self));
}

NwamEnmGroup*
nwam_enm_group_new(GtkUIManager *ui_manager,
  const gchar *place_holder)
{
    NwamEnmGroup *self;

    self = g_object_new(NWAM_TYPE_ENM_GROUP,
      "ui-manager", ui_manager,
      "ui-placeholder", place_holder,
      NULL);

    return self;
}

