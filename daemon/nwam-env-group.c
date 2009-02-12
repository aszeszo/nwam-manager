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

#include "nwam-env-group.h"
#include "nwam-env-action.h"
#include "nwam-obj-proxy-iface.h"
#include "libnwamui.h"

#define DEBUG()	g_debug ("[[ %20s : %-4d ]]", __func__, __LINE__)

typedef struct _NwamEnvGroupPrivate NwamEnvGroupPrivate;
struct _NwamEnvGroupPrivate {
    NwamuiDaemon *daemon;
/*     GtkRadioAction *automatic; */
};

#define NWAM_ENV_GROUP_GET_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), NWAM_TYPE_ENV_GROUP, NwamEnvGroupPrivate))

static void nwam_env_group_finalize(NwamEnvGroup *self);

G_DEFINE_TYPE(NwamEnvGroup,
  nwam_env_group,
  NWAM_TYPE_MENU_RADIO_ACTION_GROUP)

static void
nwam_env_group_class_init(NwamEnvGroupClass *klass)
{
	GObjectClass *gobject_class;
    NwamMenuGroupClass *menu_group_class;

	gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->finalize = (void (*)(GObject*)) nwam_env_group_finalize;

    menu_group_class = NWAM_MENU_GROUP_CLASS(klass);

    g_type_class_add_private(klass, sizeof(NwamEnvGroupPrivate));
}

static void
nwam_env_group_init(NwamEnvGroup *self)
{
    NwamEnvGroupPrivate *prv = NWAM_ENV_GROUP_GET_PRIVATE(self);

    prv->daemon = nwamui_daemon_get_instance();
}

static void
nwam_env_group_finalize(NwamEnvGroup *self)
{
    NwamEnvGroupPrivate *prv = NWAM_ENV_GROUP_GET_PRIVATE(self);

    g_object_unref(prv->daemon);
/*     g_object_unref(prv->automatic); */
	
	G_OBJECT_CLASS(nwam_env_group_parent_class)->finalize(G_OBJECT(self));
}

NwamEnvGroup*
nwam_env_group_new(GtkUIManager *ui_manager,
  const gchar *place_holder)
{
    NwamEnvGroup *self;
    NwamEnvGroupPrivate *prv;
	GList *idx = NULL;
	GtkAction* action = NULL;


    self = g_object_new(NWAM_TYPE_ENV_GROUP,
      "ui-manager", ui_manager,
      "ui-placeholder", place_holder,
      NULL);
    prv = NWAM_ENV_GROUP_GET_PRIVATE(self);

/*     prv->automatic = gtk_radio_action_new("automatic", */
/*       _("Automatic"), */
/*       NULL, */
/*       NULL, */
/*       0); */

/*     nwam_menu_group_add_item(NWAM_MENU_GROUP(self), */
/*       prv->automatic, */
/*       TRUE); */


	for (idx = nwamui_daemon_get_env_list(prv->daemon); idx; idx = g_list_next(idx)) {
		action = GTK_ACTION(nwam_env_action_new(NWAMUI_ENV(idx->data)));

        nwam_menu_group_add_item(NWAM_MENU_GROUP(self),
          G_OBJECT(action),
          TRUE);

        //nwam_menuitem_proxy_new(action, env_group);
		g_object_unref(action);
	}

    return self;
}

