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
 * File:   nwamui_daemon.h
 *
 */

#ifndef _NWAMUI_DAEMON_H
#define	_NWAMUI_DAEMON_H

#ifndef _libnwamui_H
#error "Please include libnwamui.h header instead."
#endif

G_BEGIN_DECLS

#define NWAMUI_TYPE_DAEMON               (nwamui_daemon_get_type ())
#define NWAMUI_DAEMON(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), NWAMUI_TYPE_DAEMON, NwamuiDaemon))
#define NWAMUI_DAEMON_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), NWAMUI_TYPE_DAEMON, NwamuiDaemonClass))
#define NWAMUI_IS_DAEMON(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NWAMUI_TYPE_DAEMON))
#define NWAMUI_IS_DAEMON_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), NWAMUI_TYPE_DAEMON))
#define NWAMUI_DAEMON_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), NWAMUI_TYPE_DAEMON, NwamuiDaemonClass))


typedef struct _NwamuiDaemon		      NwamuiDaemon;
typedef struct _NwamuiDaemonClass        NwamuiDaemonClass;
typedef struct _NwamuiDaemonPrivate	  NwamuiDaemonPrivate;

struct _NwamuiDaemon
{
	GObject                      object;

	/*< private >*/
	NwamuiDaemonPrivate    *prv;
};

struct _NwamuiDaemonClass
{
	GObjectClass                parent_class;
        
    void (*wifi_scan_result)    (NwamuiDaemon *self, GObject *data, gpointer user_data);
    void (*active_env_changed)  (NwamuiDaemon *self, GObject *data, gpointer user_data);
	void (*ncu_create)  (NwamuiDaemon *self, GObject *data, gpointer user_data);
	void (*ncu_destroy)  (NwamuiDaemon *self, GObject *data, gpointer user_data);
	void (*ncu_up)  (NwamuiDaemon *self, GObject *data, gpointer user_data);
	void (*ncu_down)  (NwamuiDaemon *self, GObject *data, gpointer user_data);
	void (*daemon_info)  (NwamuiDaemon *self, gpointer data, gpointer user_data);
    void (*add_wifi_fav) (NwamuiDaemon *self, NwamuiWifiNet* new_wifi, gpointer user_data);
    void (*remove_wifi_fav) (NwamuiDaemon *self, NwamuiWifiNet* new_wifi, gpointer user_data);
};

extern  GType                       nwamui_daemon_get_type (void) G_GNUC_CONST;

extern NwamuiDaemon*                nwamui_daemon_get_instance (void);

extern NwamuiNcp*                   nwamui_daemon_get_active_ncp(NwamuiDaemon *self);

extern gchar*                       nwamui_daemon_get_active_ncp_name(NwamuiDaemon *self);

extern gboolean                     nwamui_daemon_is_active_env(NwamuiDaemon *self, NwamuiEnv* env ) ;

extern NwamuiEnv*                   nwamui_daemon_get_active_env(NwamuiDaemon *self);

extern void                         nwamui_daemon_set_active_env( NwamuiDaemon* self, NwamuiEnv* env );

extern gchar*                       nwamui_daemon_get_active_env_name(NwamuiDaemon *self);

extern GList*                       nwamui_daemon_get_env_list(NwamuiDaemon *self);

extern void                         nwamui_daemon_env_append(NwamuiDaemon *self, NwamuiEnv* new_env );

extern gboolean                     nwamui_daemon_env_remove(NwamuiDaemon *self, NwamuiEnv* env );

extern GtkTreeModel *               nwamui_get_default_svcs (NwamuiDaemon *self);

extern GList*                       nwamui_daemon_get_enm_list(NwamuiDaemon *self);

extern void                         nwamui_daemon_enm_append(NwamuiDaemon *self, NwamuiEnm* new_enm );

extern gboolean                     nwamui_daemon_enm_remove(NwamuiDaemon *self, NwamuiEnm* enm );

extern void                         nwamui_daemon_wifi_start_scan(NwamuiDaemon *self);

extern GList*                       nwamui_daemon_get_fav_wifi_networks(NwamuiDaemon *self);

extern gboolean                     nwamui_daemon_set_fav_wifi_networks(NwamuiDaemon *self, GList *new_list );

extern void                         nwamui_daemon_add_wifi_fav(NwamuiDaemon *self, NwamuiWifiNet* new_wifi );

extern void                         nwamui_daemon_remove_wifi_fav(NwamuiDaemon *self, NwamuiWifiNet* wifi );


G_END_DECLS

#endif	/* _NWAMUI_DAEMON_H */

