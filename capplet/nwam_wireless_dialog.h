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
 * File:   nwam_wireless_dialog.h
 *
 * Created on May 9, 2007, 11:13 AM 
 * 
 */

#ifndef _NWAM_WIRELESS_DIALOG_H
#define	_NWAM_WIRELESS_DIALOG_H

#ifndef _libnwamui_H
#include <libnwamui.h>
#endif

G_BEGIN_DECLS

#define NWAM_TYPE_WIRELESS_DIALOG               (nwam_wireless_dialog_get_type ())
#define NWAM_WIRELESS_DIALOG(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), NWAM_TYPE_WIRELESS_DIALOG, NwamWirelessDialog))
#define NWAM_WIRELESS_DIALOG_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), NWAM_TYPE_WIRELESS_DIALOG, NwamWirelessDialogClass))
#define NWAM_IS_WIRELESS_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NWAM_TYPE_WIRELESS_DIALOG))
#define NWAM_IS_WIRELESS_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), NWAM_TYPE_WIRELESS_DIALOG))
#define NWAM_WIRELESS_DIALOG_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), NWAM_TYPE_WIRELESS_DIALOG, NwamWirelessDialogClass))


typedef struct _NwamWirelessDialog		NwamWirelessDialog;
typedef struct _NwamWirelessDialogClass         NwamWirelessDialogClass;
typedef struct _NwamWirelessDialogPrivate	NwamWirelessDialogPrivate;

struct _NwamWirelessDialog
{
	GObject                      object;

	/*< private >*/
	NwamWirelessDialogPrivate    *prv;
};

struct _NwamWirelessDialogClass
{
	GObjectClass                parent_class;
};


typedef enum {
    NWAMUI_WIRELESS_DIALOG_TITLE_ADD,
    NWAMUI_WIRELESS_DIALOG_TITLE_JOIN,
    NWAMUI_WIRELESS_DIALOG_TITLE_EDIT,
    NWAMUI_WIRELESS_DIALOG_TITLE_LAST /* Not to be used directly */
} nwamui_wireless_dialog_title_t;


extern  GType                   nwam_wireless_dialog_get_type (void) G_GNUC_CONST;

extern  NwamWirelessDialog*     nwam_wireless_dialog_new (void);

extern  NwamWirelessDialog*     nwam_wireless_dialog_new_with_ncu (const gchar *ncu);

extern  void                    nwam_wireless_dialog_set_title( NwamWirelessDialog  *self, nwamui_wireless_dialog_title_t title );

extern  void                    nwam_wireless_dialog_show (NwamWirelessDialog  *self);
extern  void                    nwam_wireless_dialog_hide (NwamWirelessDialog  *self);

extern NwamuiWifiNet*           nwam_wireless_dialog_get_wifi_net (NwamWirelessDialog *self);

extern void                     nwam_wireless_dialog_set_wifi_net (NwamWirelessDialog *self, NwamuiWifiNet* wifi_net );

extern  void                    nwam_wireless_dialog_set_ncu (NwamWirelessDialog  *wireless_dialog,
                                                              NwamuiNcu           *ncu_name );

extern  NwamuiNcu*              nwam_wireless_dialog_get_ncu (NwamWirelessDialog *wireless_dialog );

extern  void                    nwam_wireless_dialog_set_essid (NwamWirelessDialog  *wireless_dialog,
                                                                const gchar         *essid );

extern  gchar*                  nwam_wireless_dialog_get_essid (NwamWirelessDialog *wireless_dialog );

extern  void                    nwam_wireless_dialog_set_security (NwamWirelessDialog           *wireless_dialog,
                                                                   nwamui_wifi_security_t   security );

extern  nwamui_wifi_security_t      
                                nwam_wireless_dialog_get_security (NwamWirelessDialog *wireless_dialog );

extern  void                    nwam_wireless_dialog_set_key (NwamWirelessDialog  *wireless_dialog,
                                                              const gchar         *security_key );

extern  gchar*                  nwam_wireless_dialog_get_key (NwamWirelessDialog *wireless_dialog );

extern  void                    nwam_wireless_dialog_set_key_index (NwamWirelessDialog  *wireless_dialog,
                                                              guint key_index );

extern  guint                   nwam_wireless_dialog_get_key_index (NwamWirelessDialog *wireless_dialog );

extern  void                    nwam_wireless_dialog_set_bssid_list (NwamWirelessDialog  *wireless_dialog,
                                                                     GList               *bssid_list );

extern  GList*                  nwam_wireless_dialog_get_bssid_list (NwamWirelessDialog *wireless_dialog );

extern  void                    nwam_wireless_dialog_set_persistant (NwamWirelessDialog  *wireless_dialog,
                                                                    gboolean             persistant);

extern  gboolean                nwam_wireless_dialog_get_persistant (NwamWirelessDialog *wireless_dialog );

extern  void                    nwam_wireless_dialog_set_do_connect (NwamWirelessDialog  *wireless_dialog,
                                                                    gboolean             do_connect);

extern  gboolean                nwam_wireless_dialog_get_do_connect (NwamWirelessDialog *wireless_dialog );

extern void                     nwam_wireless_dialog_set_wpa_username (NwamWirelessDialog  *self,
                                                                       const gchar         *wpa_username );

extern gchar*                  nwam_wireless_dialog_get_wpa_username (NwamWirelessDialog *self );

extern void                    nwam_wireless_dialog_set_wpa_password (NwamWirelessDialog  *self,
                                                                      const gchar         *wpa_password );

extern gchar*                  nwam_wireless_dialog_get_wpa_password (NwamWirelessDialog *self );

extern void                    nwam_wireless_dialog_set_wpa_config_type (NwamWirelessDialog   *self,
                                                                         nwamui_wifi_wpa_config_t  wpa_config_type );

extern nwamui_wifi_wpa_config_t    nwam_wireless_dialog_get_wpa_config_type (NwamWirelessDialog *self );


extern void                    nwam_wireless_dialog_set_leap_username (NwamWirelessDialog  *self,
                                                                       const gchar         *leap_username );


extern gchar*                  nwam_wireless_dialog_get_leap_username (NwamWirelessDialog *self );


extern void                    nwam_wireless_dialog_set_leap_password (NwamWirelessDialog  *self,
                                                                       const gchar         *leap_password );

extern gchar*                  nwam_wireless_dialog_get_leap_password (NwamWirelessDialog *self );

G_END_DECLS

#endif	/* _NWAM_WIRELESS_DIALOG_H */

