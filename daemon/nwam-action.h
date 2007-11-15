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
#ifndef NWAM_ACTION_H
#define NWAM_ACTION_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NWAM_TYPE_ACTION            (nwam_action_get_type ())
#define NWAM_ACTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NWAM_TYPE_ACTION, NwamAction))
#define NWAM_ACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NWAM_TYPE_ACTION, NwamActionClass))
#define NWAM_IS_ACTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NWAM_TYPE_ACTION))
#define NWAM_IS_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NWAM_TYPE_ACTION))
#define NWAM_ACTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), NWAM_TYPE_ACTION, NwamActionClass))

typedef struct _NwamAction        NwamAction;
typedef struct _NwamActionPrivate NwamActionPrivate;
typedef struct _NwamActionClass   NwamActionClass;

struct _NwamAction
{
  GtkRadioAction parent;

	/*< private >*/

  NwamActionPrivate *private_data;
};

struct _NwamActionClass
{
  GtkRadioActionClass parent_class;

  void (* nwamd) (NwamAction *action);

};

GType            nwam_action_get_type          (void) G_GNUC_CONST;
extern NwamAction *nwam_action_new               (const gchar     *name,
                                          const gchar     *label,
                                             const gchar     *tooltip,
                                           const gchar     *stock_id);

G_END_DECLS

#endif  /* NWAM_ACTION_H */

