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

#ifndef _status_icon_H
#define	_status_icon_H

G_BEGIN_DECLS

#define NWAM_TYPE_STATUS_ICON            (nwam_status_icon_get_type ())
#define NWAM_STATUS_ICON(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NWAM_TYPE_STATUS_ICON, NwamStatusIcon))
#define NWAM_STATUS_ICON_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NWAM_TYPE_STATUS_ICON, NwamStatusIconClass))
#define NWAM_IS_STATUS_ICON(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NWAM_TYPE_STATUS_ICON))
#define NWAM_IS_STATUS_ICON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NWAM_TYPE_STATUS_ICON))
#define NWAM_STATUS_ICON_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), NWAM_TYPE_STATUS_ICON, NwamStatusIconClass))

typedef struct _NwamStatusIcon        NwamStatusIcon;
typedef struct _NwamStatusIconClass   NwamStatusIconClass;

struct _NwamStatusIcon {
	GtkStatusIcon parent;
};

struct _NwamStatusIconClass {
	GtkStatusIconClass parent_class;	
};

GType nwam_status_icon_get_type(void) G_GNUC_CONST;

GtkStatusIcon* nwam_status_icon_new( void );

void nwam_status_icon_run(NwamStatusIcon *self);

void nwam_status_icon_set_activate_callback(NwamStatusIcon *self,
  GCallback activate_cb, gpointer user_data);

void nwam_status_icon_set_status(NwamStatusIcon *self, NwamuiNcu* wireless_ncu );

void nwam_status_icon_show_menu(NwamStatusIcon *self);

/* Others */
extern void nwam_exec (const gchar **nwam_arg);

G_END_DECLS

#endif	/* _status_icon_H */

