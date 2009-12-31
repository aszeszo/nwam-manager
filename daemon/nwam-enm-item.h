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
#ifndef NWAM_ENM_ITEM_H
#define NWAM_ENM_ITEM_H

#include <gtk/gtk.h>
#include "nwam-menuitem.h"
#include "libnwamui.h"

G_BEGIN_DECLS

#define NWAM_TYPE_ENM_ITEM            (nwam_enm_item_get_type ())
#define NWAM_ENM_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NWAM_TYPE_ENM_ITEM, NwamEnmItem))
#define NWAM_ENM_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NWAM_TYPE_ENM_ITEM, NwamEnmItemClass))
#define NWAM_IS_ENM_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NWAM_TYPE_ENM_ITEM))
#define NWAM_IS_ENM_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NWAM_TYPE_ENM_ITEM))
#define NWAM_ENM_ITEM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), NWAM_TYPE_ENM_ITEM, NwamEnmItemClass))

typedef struct _NwamEnmItem        NwamEnmItem;
typedef struct _NwamEnmItemClass   NwamEnmItemClass;

struct _NwamEnmItem
{
    NwamMenuItem parent;
};

struct _NwamEnmItemClass
{
    NwamMenuItemClass parent_class;
};

GType            nwam_enm_item_get_type          (void) G_GNUC_CONST;

extern GtkWidget *nwam_enm_item_new(NwamuiObject *enm);

NwamuiEnm *nwam_enm_item_get_enm (NwamEnmItem *self);

void nwam_enm_item_set_enm (NwamEnmItem *self, NwamuiEnm *enm);


G_END_DECLS

#endif  /* NWAM_ENM_ITEM_H */


