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

#ifndef _NWAM_NET_CONF_PANEL_H
#define	_NWAM_NET_CONF_PANEL_H

G_BEGIN_DECLS


#define NWAM_TYPE_NET_CONF_PANEL               (nwam_net_conf_panel_get_type ())
#define NWAM_NET_CONF_PANEL(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), NWAM_TYPE_NET_CONF_PANEL, NwamNetConfPanel))
#define NWAM_NET_CONF_PANEL_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), NWAM_TYPE_NET_CONF_PANEL, NwamNetConfPanelClass))
#define NWAM_IS_NET_CONF_PANEL(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NWAM_TYPE_NET_CONF_PANEL))
#define NWAM_IS_NET_CONF_PANEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), NWAM_TYPE_NET_CONF_PANEL))
#define NWAM_NET_CONF_PANEL_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), NWAM_TYPE_NET_CONF_PANEL, NwamNetConfPanelClass))


typedef struct _NwamNetConfPanel		NwamNetConfPanel;
typedef struct _NwamNetConfPanelClass	NwamNetConfPanelClass;
typedef struct _NwamNetConfPanelPrivate	NwamNetConfPanelPrivate;

struct _NwamNetConfPanel
{
	GObject                      object;

	/*< private >*/
	NwamNetConfPanelPrivate    *prv;
};

struct _NwamNetConfPanelClass
{
	GObjectClass                parent_class;
};

GType nwam_net_conf_panel_get_type (void) G_GNUC_CONST;

NwamNetConfPanel* nwam_net_conf_panel_new(NwamCappletDialog *pref_dialog, NwamuiNcp* ncp);

G_END_DECLS

#endif	/* _NWAM_NET_CONF_PANEL_H */
