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
typedef struct _NwamMenuPrivate NwamMenuPrivate;
typedef struct _NwamMenuClass   NwamMenuClass;

struct _NwamMenu
{
	GObject parent;
	
	/*< private >*/
	NwamMenuPrivate *prv;
};

struct _NwamMenuClass {
	GObjectClass parent_class;	
};

extern NwamMenu * get_nwam_menu_instance ();
extern GtkWidget* get_status_icon_menu( gint index );
extern void nwam_exec (const gchar *nwam_arg);

G_END_DECLS

#endif  /* NWAM_MENU_H */


