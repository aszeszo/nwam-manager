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
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libgnomeui/libgnomeui.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <auth_attr.h>
#include <secdb.h>


#include "status_icon.h"
#include "notify.h"
#include "libnwamui.h"
#include "nwam-menu.h"

#include "capplet/nwam_wireless_dialog.h"

/*
 * Security Auth needed for NWAM to work. Console User and Network Management
 * profiles will have this.
 */
#define	NET_AUTOCONF_AUTH	"solaris.network.autoconf"

/* Command-line options */
static gboolean debug = FALSE;

static GOptionEntry option_entries[] = {
    {"debug", 0, 0, G_OPTION_ARG_NONE, &debug, N_("Enable debugging messages"), NULL },
    {NULL}
};


static GtkWidget *my_menu = NULL;
static GtkStatusIcon *status_icon = NULL;

#define NWAM_MANAGER_LOCK	"nwam-manager-lock"
gint prof_action_if_no_fav_networks = NULL;
gboolean prof_ask_join_open_network;
gboolean prof_ask_join_fav_network;
gboolean prof_ask_add_to_fav;

/* nwamui utilies */
extern void join_wireless(NwamuiWifiNet *wifi);
extern void change_ncu_priority();
extern void manage_known_ap();

/* nwamui preference signal */
static void join_wifi_not_in_fav (NwamuiProf* prof, gboolean val, gpointer data);
static void join_any_fav_wifi (NwamuiProf* prof, gboolean val, gpointer data);
static void add_any_new_wifi_to_fav (NwamuiProf* prof, gboolean val, gpointer data);
static void action_on_no_fav_networks (NwamuiProf* prof, gint val, gpointer data);

/* nwamui daemon events */
static void event_daemon_status_changed(NwamuiDaemon *daemon, nwamui_daemon_status_t status, gpointer user_data);

static void event_daemon_info(NwamuiDaemon *daemon, gint type, GObject *obj, gpointer data, gpointer user_data);

static void event_wifi_key_needed (NwamuiDaemon* daemon, NwamuiWifiNet* wifi, gint sicon_id);

static void event_wifi_selection_needed (NwamuiDaemon* daemon, NwamuiNcu* ncu, gint sicon_id);

static void event_ncu_up (NwamuiDaemon* daemon, NwamuiNcu* ncu, gpointer data);

static void event_ncu_down (NwamuiDaemon* daemon, NwamuiNcu* ncu, gpointer data);

/* nwamui ncp signals */
static void ncp_activate_ncu (NwamuiNcp *ncp, NwamuiNcu* ncu, gpointer user_data);
static void ncp_deactivate_ncu (NwamuiNcp *ncp, NwamuiNcu* ncu, gpointer user_data);

/* notification event */
static void on_notifyaction_wifi_key_need (NotifyNotification *n,
  gchar *action,
  gpointer user_data);

static void on_notifyaction_popup_menus(NotifyNotification *n,
  gchar *action,
  gpointer user_data);

/* tooltip related */
static void on_trigger_network_changed (NwamuiDaemon* daemon, NwamuiNcu* ncu, gint sicon_id);

/* others */
static gboolean find_wireless_interface(GtkTreeModel *model,
  GtkTreePath *path,
  GtkTreeIter *iter,
  gpointer data);

gboolean
detect_and_set_singleton ()
{
    enum { NORMAL_SESSION = 1, SUNRAY_SESSION };
    static gint session_type = 0;
    gchar *lockfile = NULL;
    int fd;
    
    if (session_type == 0) {
        const gchar *sunray_token;
        g_assert (lockfile == NULL);
        if ((sunray_token = g_getenv ("SUN_SUNRAY_TOKEN")) == NULL) {
            session_type = NORMAL_SESSION;
        } else {
            g_debug ("$SUN_SUNRAY_TOKEN = %s\n", sunray_token);
            session_type = SUNRAY_SESSION;
        }
    }
    
    switch (session_type) {
    case NORMAL_SESSION:
        lockfile = g_build_filename ("/tmp", NWAM_MANAGER_LOCK, NULL);
        break;
    case SUNRAY_SESSION:
        lockfile = g_build_filename ("/tmp", g_get_user_name (),
          NWAM_MANAGER_LOCK, NULL);
        break;
    default:
        g_debug ("Unknown session type.");
        exit (1);
    }

    fd = open (lockfile, O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        g_debug ("open lock file %s error", lockfile);
        exit (1);
    } else {
        struct flock pidlock;
        pidlock.l_type = F_WRLCK;
        pidlock.l_whence = SEEK_SET;
        pidlock.l_start = 0;
        pidlock.l_len = 0;
        if (fcntl (fd, F_SETLK, &pidlock) == -1) {
            if (errno == EAGAIN || errno == EACCES) {
                g_free (lockfile);
                return FALSE;
            } else {
                g_debug ("obtain lock file %s error", lockfile);
                exit (1);
            }
        }
    }
    g_free (lockfile);
    return TRUE;
}

static void 
cleanup_and_exit(int sig, siginfo_t *sip, void *data)
{
    gtk_main_quit ();
}

static void
on_blink_change(GtkStatusIcon *widget, 
        gpointer data)
{
    gboolean blink = GPOINTER_TO_UINT(data);
    g_debug("Set blinking %s", (blink) ? "on" : "off");
    gtk_status_icon_set_blinking(GTK_STATUS_ICON(status_icon), blink);
}

static gboolean
get_state(  GtkTreeModel *model,
            GtkTreePath *path,
            GtkTreeIter *iter,
            gpointer data)
{
	GString *str = (GString *)data;
	NwamuiNcu *ncu = NULL;
	gchar *format = NULL;
	gchar *name = NULL;
	
  	gtk_tree_model_get(model, iter, 0, &ncu, -1);

    g_assert( NWAMUI_IS_NCU(ncu) );
    
	switch (nwamui_ncu_get_ncu_type (ncu)) {
		case NWAMUI_NCU_TYPE_WIRED:
			format = _("\nWired (%s): %s");
			break;
		case NWAMUI_NCU_TYPE_WIRELESS:
			format = _("\nWireless (%s): %s");
			break;
	}
	name = nwamui_ncu_get_device_name (ncu);
	g_string_append_printf (str, format, name, "test again");
    g_free (name);
    
    g_object_unref( ncu );
    
    return( TRUE );
}

static void
update_tooltip(NwamuiDaemon* daemon, gint sicon_id, NwamuiNcu* ncu)
{
	GString        *gstr = g_string_new("");
	gchar          *name = NULL;
	gchar          *cstr = NULL;
    const gchar    *status_msg = NULL;
    NwamuiNcp      *ncp = NULL;
    NwamuiNcu      *ncu_ref = NULL;

    g_debug("Update tooltip called");
    if ( daemon != NULL ) {
        ncp = nwamui_daemon_get_active_ncp(daemon);
    }
    else {
        NwamuiDaemon* d = nwamui_daemon_get_instance();
        ncp = nwamui_daemon_get_active_ncp(daemon);
        g_object_unref(d);
    }

    if ( ncu == NULL ) {
        /* TODO: Handle active NCUs */
        /* ncu_ref = nwamui_ncp_get_active_ncu(ncp); */
    }
    else {
        ncu_ref = g_object_ref(ncu);
    }

    if ( ncu_ref != NULL ) {
        name = nwamui_ncu_get_device_name (ncu_ref);

        if ( nwamui_ncu_get_ncu_type( ncu_ref ) == NWAMUI_NCU_TYPE_WIRELESS ) {
            g_string_append_printf( gstr, _("Wireless (%s) network interface is active."), name);
        }
        else {
            g_string_append_printf( gstr, _("Wired (%s) network interface is active."), name);
        }
        g_free (name);
        g_object_unref(ncu_ref);
    }
    else {
        g_string_append_printf( gstr, _("No network interface is active."), name);
    }

    cstr = g_string_free(gstr, FALSE);
    g_debug("Update tooltip setting tooltip to '%s'", cstr);
	nwam_status_icon_set_tooltip (sicon_id, cstr);
    g_free(cstr);

    g_object_unref(ncp);
}

static void
on_trigger_network_changed (NwamuiDaemon* daemon, NwamuiNcu* ncu, gint sicon_id)
{
    update_tooltip( daemon, sicon_id, ncu );
}

static void
event_daemon_status_changed(NwamuiDaemon *daemon, nwamui_daemon_status_t status, gpointer user_data)
{
    const gchar* status_str;
    const gchar *body_str;
    gint status_icon_index = (gint)user_data;
    static gboolean need_report_daemon_error = FALSE;

	/* should repopulate data here */

    switch( status) {
    case NWAMUI_DAEMON_STATUS_ACTIVE:
        nwam_status_icon_set_visible(status_icon_index, TRUE);

		nwam_status_icon_set_status(status_icon_index, NWAMUI_ENV_STATUS_CONNECTED, NULL );

        if (need_report_daemon_error) {
            /* CFB Think this notification message is probably unhelpful
               in practice, commenting out for now... */
            /*status_str=_("Automatic network detection active");
            nwam_notification_show_message(status_str, NULL, NWAM_ICON_EARTH_CONNECTED);
            */
            need_report_daemon_error = FALSE;

            update_tooltip( daemon, status_icon_index, NULL );

        }
        break;
    case NWAMUI_DAEMON_STATUS_INACTIVE:
        if ( nwam_status_icon_get_visible(status_icon_index) ) {
            /* On intial run with NWAM disabled the icon won't be visible */
            nwam_status_icon_set_status(status_icon_index, NWAMUI_ENV_STATUS_ERROR, 
                                        _("Automatic network configuration daemon is unavailable.") );
		
            status_str=_("Automatic network configuration daemon is unavailable.");
            body_str=  _("For further information please run\n\"svcs -xv nwam\" in a terminal.");
            nwam_notification_show_message(status_str, body_str, NWAM_ICON_EARTH_ERROR, NOTIFY_EXPIRES_DEFAULT);
            update_tooltip( daemon, status_icon_index, NULL );
            need_report_daemon_error = TRUE;
        }
        break;
    default:
        g_assert_not_reached();
        break;
    }
}

static gboolean
ncu_is_higher_priority_than_active_ncu( NwamuiNcu* ncu, gboolean *is_active_ptr )
{
    NwamuiDaemon*  daemon  = nwamui_daemon_get_instance();
    NwamuiNcp*     active_ncp  = nwamui_daemon_get_active_ncp( daemon );
    NwamuiNcu*     active_ncu  = NULL;
    /* gint           ncu_prio    = nwamui_ncu_get_priority(ncu); */
    gint           active_ncu_prio = -1;
    gboolean       is_active_ncu = FALSE;
    gboolean       retval = FALSE;

    g_object_unref(daemon);
    
    return (TRUE);

    /* TODO: How to get the acitve ncu and priorities */
#if 0 
    if ( active_ncp != NULL && NWAMUI_NCP( active_ncp ) ) {
        active_ncu = nwamui_ncp_get_active_ncu( active_ncp );
        g_object_unref(active_ncp);
    }

    if ( active_ncu != NULL ) {
        active_ncu_prio = nwamui_ncu_get_priority(active_ncu);
        /* Return TRUE only if it's the same or a higher
         * priority ncu that the active ncu 
         * (lower value = higher prio)
         */
        is_active_ncu = (active_ncu == ncu);
        retval = (is_active_ncu || ncu_prio < active_ncu_prio);
        
        g_object_unref(active_ncu);
    }
    else {
        /* If there is no active_ncu, return TRUE */
        retval = TRUE;
    }

    if ( is_active_ptr != NULL ) {
        *is_active_ptr = is_active_ncu;
    }

    return( retval );
#endif /* 0 */
}

static void
event_daemon_info(NwamuiDaemon *daemon, gint type, GObject *obj, gpointer data, gpointer user_data)
{
    gint    sicon_id = (gint)user_data;
    gchar *msg = NULL;
    switch (type) {
    case NWAMUI_DAEMON_INFO_WLAN_CONNECTED: {
        if ( NWAMUI_IS_WIFI_NET( obj ) ) {
            gchar *essid = nwamui_wifi_net_get_display_string(NWAMUI_WIFI_NET(obj), FALSE);
            msg = g_strdup_printf(_("Connected to wireless network '%s'"), essid, NULL);
            nwam_status_icon_set_status(sicon_id, NWAMUI_ENV_STATUS_CONNECTED, msg );
            nwam_notification_show_message (msg, essid, NULL, NOTIFY_EXPIRES_DEFAULT );
            g_free(essid);
            g_free(msg);
            nwam_status_icon_set_activate_callback( sicon_id, NULL, NULL, NULL );
        }
    }
        break;
    case NWAMUI_DAEMON_INFO_WLAN_DISCONNECTED: {
            gboolean        show_message = FALSE;
            gboolean        is_active_ncu = FALSE;
            NwamuiWifiNet*  wifi = NULL;

            if ( obj != NULL && NWAMUI_IS_WIFI_NET( obj ) ) {
                wifi = NWAMUI_WIFI_NET(obj);
            }
            if ( wifi != NULL ) {
                NwamuiNcu*  ncu = nwamui_wifi_net_get_ncu( wifi );
                show_message = ncu_is_higher_priority_than_active_ncu( ncu, &is_active_ncu );
                g_object_unref(ncu);
            }

            if ( show_message && nwamui_daemon_get_event_cause(daemon) != NWAMUI_DAEMON_EVENT_CAUSE_NONE ) {
                gchar *essid    = nwamui_wifi_net_get_display_string( wifi , FALSE);
                gchar *summary  = g_strdup_printf (_("Disconnected from Wireless Network '%s'"), essid);
                gchar *body     = g_strdup_printf(_("%s\nClick here to join another wireless network."),
                                         nwamui_daemon_get_event_cause_string(daemon) );
                nwam_notification_show_message_with_action(summary, body,
                  NWAM_ICON_EARTH_WARNING,
                  NULL,	/* action */
                  NULL,	/* label */
                  on_notifyaction_popup_menus,
                  NULL,
                  NULL,
                  NOTIFY_EXPIRES_DEFAULT);

                /* Only change status if it's the active_ncu */
                if ( is_active_ncu ) {
                    nwam_status_icon_set_status(sicon_id, NWAMUI_ENV_STATUS_WARNING, summary );
                }

                g_free(summary);
                g_free(body);
                g_free(essid);
            }
        }
        break;
    case NWAMUI_DAEMON_INFO_NCU_SELECTED:
    case NWAMUI_DAEMON_INFO_NCU_UNSELECTED:
        break;
    case NWAMUI_DAEMON_INFO_WLAN_CHANGED: {
            gboolean show_message = FALSE;

            if ( obj != NULL && NWAMUI_IS_NCU(obj) ) {
                NwamuiNcu*  ncu         = NWAMUI_NCU(obj);

                show_message = ncu_is_higher_priority_than_active_ncu( ncu, NULL );
            }

            if ( show_message ) {
                nwam_notification_show_message (_("Automatic Network Configuration Daemon"), 
                                                 (gchar *)data, NULL, NOTIFY_EXPIRES_DEFAULT);
            }
        }
        break;
    case NWAMUI_DAEMON_INFO_WLAN_CONNECT_FAILED: {
            gboolean        show_message = FALSE;
            gboolean        is_active_ncu = FALSE;
            NwamuiWifiNet*  wifi = NULL;

            if ( obj != NULL && NWAMUI_IS_WIFI_NET( obj ) ) {
                wifi = NWAMUI_WIFI_NET(obj);
            }
            if ( wifi != NULL ) {
                NwamuiNcu*  ncu = nwamui_wifi_net_get_ncu( wifi );
                show_message = ncu_is_higher_priority_than_active_ncu( ncu, &is_active_ncu );
                g_object_unref(ncu);
            }

            if ( show_message ) {
                nwam_status_icon_set_status(sicon_id, NWAMUI_ENV_STATUS_WARNING, (gchar*)data );
            }
        }
        break;
    default:
        nwam_notification_show_message (_("Automatic Network Configuration Daemon"), (gchar *)data, NULL, NOTIFY_EXPIRES_DEFAULT);
        break;
    }
}

static void
on_status_activate_wifi_key_needed( GObject* object )
{
    join_wireless(NWAMUI_WIFI_NET(object));
}

static void
event_wifi_key_needed (NwamuiDaemon* daemon, NwamuiWifiNet* wifi, gint sicon_id)
{
	gchar *summary = NULL;
	gchar *body = NULL;

	if (wifi) {
        NwamuiNcu * ncu = nwamui_wifi_net_get_ncu( wifi );
        gchar *name;
        name = nwamui_wifi_net_get_display_string(wifi, FALSE);
        summary = g_strdup_printf (_("'%s' requires authentication"), name);
        g_free(name);
        body = g_strdup_printf (_("Click here to enter the wireless network key."));

        nwam_status_icon_set_activate_callback( sicon_id, body, on_status_activate_wifi_key_needed, G_OBJECT(wifi) );
        update_tooltip( daemon, sicon_id, ncu );

        nwam_notification_show_message_with_action(summary, body,
          NWAM_ICON_NETWORK_WIRELESS,
          NULL,	/* action */
          NULL,	/* label */
          on_notifyaction_wifi_key_need,
          (gpointer)g_object_ref(wifi),
          (GFreeFunc)g_object_unref,
          NOTIFY_EXPIRES_NEVER);
		
          g_object_unref(ncu);
    } else {
        g_assert_not_reached();
    }
    g_free(body);
    g_free(summary);
}

static void
event_wifi_selection_needed (NwamuiDaemon* daemon, NwamuiNcu* ncu, gint sicon_id)
{
	gchar      *summary = NULL;
	gchar      *body = NULL;
    gboolean    active_ncu = FALSE;

    /* Only show the message when it's relevant */
	if (ncu && ncu_is_higher_priority_than_active_ncu( ncu, &active_ncu )) {
        gchar *name = nwamui_ncu_get_vanity_name (ncu);
        summary = g_strdup_printf (_("Wireless networks available"));
        body = g_strdup_printf (_("Right-click the icon to connect \n"
                                   "%s network interface to an available network."), name);
        g_free(name);

        if ( active_ncu ) {
            nwam_status_icon_set_status(sicon_id, NWAMUI_ENV_STATUS_WARNING, 
                                        _("Right-click to select wireless network") );
        }

        nwam_notification_show_message_with_action(summary, body,
          NWAM_ICON_EARTH_WARNING,
          NULL,	/* action */
          NULL,	/* label */
          on_notifyaction_popup_menus,
          (gpointer)sicon_id,
          NULL,
          NOTIFY_EXPIRES_DEFAULT);

        g_free(summary);
        g_free(body);
    } 
}

static void
event_ncu_up (NwamuiDaemon* daemon, NwamuiNcu* ncu, gpointer data)
{
    gint status_icon_index = (gint)data;
    gchar *sname = NULL;
    gchar *ifname = NULL;
    gchar *addr = NULL;
    gchar *summary = NULL, *body = NULL;

	/* generic info */
    ifname = nwamui_ncu_get_device_name (ncu);
    if (!nwamui_ncu_get_ipv6_active (ncu)) {
        addr = nwamui_ncu_get_ipv4_address (ncu);
    } else {
        addr = nwamui_ncu_get_ipv6_address (ncu);
    }

    update_tooltip( daemon, status_icon_index, ncu );

    switch (nwamui_ncu_get_ncu_type (ncu)) {
    case NWAMUI_NCU_TYPE_WIRED: {
        GString *gstr = g_string_new("");
        sname = nwamui_ncu_get_vanity_name (ncu);
        summary = g_strdup_printf (_("%s network interface is connected."), sname);
        g_string_append_printf( gstr, _("%s: %s"), _("Network Interface"), ifname );
        if ( addr != NULL && strlen(addr) > 0 ) {
            g_string_append_printf( gstr, _("\n%s: %s"), _("Network Address"), addr );
        }
        body = g_string_free(gstr, FALSE);

        nwam_notification_show_message (summary, body, NWAM_ICON_NETWORK_IDLE, NOTIFY_EXPIRES_DEFAULT);
        break;
    }
    case NWAMUI_NCU_TYPE_WIRELESS: {
        gint signal = -1;
        GString *gstr = g_string_new("");
        NwamuiWifiNet *wifi = nwamui_ncu_get_wifi_info (ncu);
        if ( wifi != NULL ) { /* Can happen on initial run when NWAM already active */
            sname = nwamui_wifi_net_get_display_string(wifi, FALSE);
            summary = g_strdup_printf (_("Connected to wireless network '%s'"), sname);
            signal = (gint) ((nwamui_wifi_net_get_signal_strength (wifi) * 100) / NWAMUI_WIFI_STRENGTH_LAST);
        }
        else {
            sname = nwamui_ncu_get_vanity_name (ncu);
            summary = g_strdup_printf (_("%s network interface is connected."), sname);
        }

        g_string_append_printf( gstr, _("%s: %s"), _("Interface"), ifname );
        if ( addr != NULL && strlen(addr) > 0 ) {
            g_string_append_printf( gstr, _("\n%s: %s"), _("Address"), addr );
        }
        if ( signal != -1 ) {
            g_string_append_printf( gstr, _("\n%s: %d"),  _("Signal Strength"), signal );
        }

        body = g_string_free(gstr, FALSE);

        nwam_notification_show_message (summary, body, NWAM_ICON_NETWORK_WIRELESS, NOTIFY_EXPIRES_DEFAULT);
        if ( wifi ) {
            g_object_unref(wifi);
        }
		break;
    }
    default:
        g_assert_not_reached ();
    }

	/* set status to "CONNECTED" */
	nwam_status_icon_set_status(status_icon_index, NWAMUI_ENV_STATUS_CONNECTED, summary );

    g_free (sname);
    g_free (ifname);
    g_free (addr);
    g_free (summary);
    g_free (body);
}

static void
event_ncu_down (NwamuiDaemon* daemon, NwamuiNcu* ncu, gpointer data)
{
    gint status_icon_index = (gint)data;
    gchar *sname = NULL;
    gchar *summary = NULL;
    NwamuiNcp*  active_ncp = nwamui_daemon_get_active_ncp( daemon );
    NwamuiNcu*  active_ncu = NULL;
    
    if ( active_ncp != NULL && NWAMUI_NCP( active_ncp ) ) {
        active_ncu = nwamui_ncp_get_active_ncu( active_ncp );
        g_object_unref(active_ncp);
    }

    /* Only show this information, if it's the active_ncu */
    if ( ncu == active_ncu ) {
        switch (nwamui_ncu_get_ncu_type (ncu)) {
        case NWAMUI_NCU_TYPE_WIRED: {
                gchar *body = NULL;
                sname = nwamui_ncu_get_vanity_name (ncu);
                summary = g_strdup_printf (_("%s disconnected from network."), sname);
                if ( nwamui_daemon_get_event_cause(daemon) != NWAMUI_DAEMON_EVENT_CAUSE_NONE ) {
                    body = g_strdup_printf("Reason: %s",nwamui_daemon_get_event_cause_string(daemon));
                }
                nwam_notification_show_message (summary, body, NWAM_ICON_EARTH_WARNING, NOTIFY_EXPIRES_DEFAULT);
                g_free(body);
            }
            break;
        case NWAMUI_NCU_TYPE_WIRELESS: {
                gchar *body = NULL;
                sname = nwamui_ncu_get_vanity_name (ncu);
                summary = g_strdup_printf (_("%s disconnected from network."), sname);
                if ( nwamui_daemon_get_event_cause(daemon) != NWAMUI_DAEMON_EVENT_CAUSE_NONE ) {
                    body = g_strdup_printf("Reason: %s",nwamui_daemon_get_event_cause_string(daemon));
                }
                nwam_notification_show_message (summary, body, NWAM_ICON_EARTH_WARNING, NOTIFY_EXPIRES_DEFAULT);
                g_free(body);
            }
            break;
        default:
            g_assert_not_reached ();
        }

        /* set status to "WARNING", only relevant if it's the active ncu */
        nwam_status_icon_set_status(status_icon_index, NWAMUI_ENV_STATUS_WARNING, summary );
        update_tooltip( daemon, status_icon_index, ncu );
    }

    if ( active_ncu != NULL ) {
        g_object_unref(active_ncu);
    }

    g_free (sname);
    g_free (summary);
}

static void
ncp_activate_ncu (NwamuiNcp *ncp, NwamuiNcu* ncu, gpointer user_data)
{
    gint status_icon_index = (gint)user_data;

    if ( ncu != NULL ) { /* Can be NULL if nothing is active, e.g. NWAM brought down */
        gchar *summary = NULL;
        gchar *body = NULL;
        gchar *name;

        name = nwamui_ncu_get_vanity_name(ncu);
        summary = g_strdup_printf(_("Activating %s network interface..."), name);
        nwam_notification_show_message(summary, NULL, NWAM_ICON_EARTH_WARNING, NOTIFY_EXPIRES_DEFAULT);
        nwam_status_icon_set_status(status_icon_index, NWAMUI_ENV_STATUS_WARNING, NULL );
        update_tooltip( NULL, status_icon_index, ncu );

        g_free(name);
        g_free(summary);
        g_free(body);
    }
}

static void
ncp_deactivate_ncu (NwamuiNcp *ncp, NwamuiNcu* ncu, gpointer user_data)
{
    gint status_icon_index = (gint)user_data;
    NwamuiDaemon *daemon = nwamui_daemon_get_instance ();

    if (nwamui_ncu_get_ncu_type(ncu) == NWAMUI_NCU_TYPE_WIRELESS) {
        NwamuiWifiNet* wifi;

        wifi = nwamui_ncu_get_wifi_info(ncu);

        if (wifi) {
            switch (nwamui_wifi_net_get_security(wifi)) {
            case NWAMUI_WIFI_SEC_WEP_HEX:
            case NWAMUI_WIFI_SEC_WEP_ASCII: {
                gchar *name;
                name = nwamui_wifi_net_get_display_string(wifi, FALSE);

                switch (nwamui_daemon_get_event_cause(daemon)) {
                case NWAMUI_DAEMON_EVENT_CAUSE_DHCP_DOWN:
                case NWAMUI_DAEMON_EVENT_CAUSE_DHCP_TIMER: {
                    gchar *summary = NULL;
                    gchar *body = NULL;

                    summary = g_strdup_printf(_("Failed to connect to wireless network '%s'. %s"), name,
                                                 nwamui_daemon_get_event_cause_string(daemon) );
                    body = g_strdup(_("The DHCP server may not be responding, or\n"
                                      "you may have entered the wrong wireless key.\n\n"
                                      "Click here to re-enter the wireless key."));

                    nwam_status_icon_set_activate_callback( status_icon_index, body,
                                                            on_status_activate_wifi_key_needed, G_OBJECT(wifi) );

                    nwam_notification_show_message_with_action(summary, body,
                      NWAM_ICON_NETWORK_WIRELESS,
                      NULL,	/* action */
                      NULL,	/* label */
                      on_notifyaction_wifi_key_need,
                      (gpointer)g_object_ref(wifi),
                      (GFreeFunc)g_object_unref,
                      NOTIFY_EXPIRES_DEFAULT);

                    g_free(summary);
                    g_free(body);
                }
                    break;
                default:
                    break;
                }
                g_free(name);
            }
                break;
            default:
                break;
            }
            g_object_unref(wifi);
        }
    }
    update_tooltip( NULL, status_icon_index, NULL );
}

static void
on_notifyaction_wifi_key_need (NotifyNotification *n,
  gchar *action,
  gpointer user_data)
{
    join_wireless(user_data ? NWAMUI_WIFI_NET(user_data) : NULL);
}

static void
on_notifyaction_popup_menus(NotifyNotification *n,
  gchar *action,
  gpointer user_data)
{
    NwamuiDaemon *daemon = nwamui_daemon_get_instance();
    /* Trigger a re-scan of the networks while we're at it */
    nwamui_daemon_wifi_start_scan(daemon);
    g_object_unref(daemon);

    nwam_status_icon_show_menu((gint)user_data, 0, gtk_get_current_event_time());
}

static void
activate_cb ()
{
    /*
    nwam_notification_show_message("This is the summary", "This is the body...\nPara1", NULL );
    nwam_exec ("-e");
    */
}

static void
join_wifi_not_in_fav (NwamuiProf* prof, gboolean val, gpointer data)
{
    NwamuiDaemon* daemon = NWAMUI_DAEMON (data);
    gchar *body;
    
    prof_ask_join_open_network = val;
    body = g_strdup_printf ("prof_ask_join_open_network = %d", prof_ask_join_open_network);
    nwam_notification_show_message("Prof signal", body, NULL, NOTIFY_EXPIRES_DEFAULT);
    g_free (body);
}

static void
join_any_fav_wifi (NwamuiProf* prof, gboolean val, gpointer data)
{
    NwamuiDaemon* daemon = NWAMUI_DAEMON (data);
    gchar *body;

    prof_ask_join_fav_network = val;
    body = g_strdup_printf ("prof_ask_join_fav_network = %d", prof_ask_join_fav_network);
    nwam_notification_show_message("Prof signal", body, NULL, NOTIFY_EXPIRES_DEFAULT);
    g_free (body);
}

static void
add_any_new_wifi_to_fav (NwamuiProf* prof, gboolean val, gpointer data)
{
    NwamuiDaemon* daemon = NWAMUI_DAEMON (data);
    gchar *body;

    prof_ask_add_to_fav = val;
    body = g_strdup_printf ("prof_ask_add_to_fav = %d", prof_ask_add_to_fav);
    nwam_notification_show_message("Prof signal", body, NULL, NOTIFY_EXPIRES_DEFAULT);
    g_free (body);
}

static void
action_on_no_fav_networks (NwamuiProf* prof, gint val, gpointer data)
{
    NwamuiDaemon* daemon = NWAMUI_DAEMON (data);
    gchar *body;

    prof_action_if_no_fav_networks = val;
    body = g_strdup_printf ("prof_action_if_no_fav_networks = %d", prof_action_if_no_fav_networks);
    nwam_notification_show_message("Prof signal", body, NULL, NOTIFY_EXPIRES_DEFAULT);
    g_free (body);
}

/**
 * find_wireless_interface:
 *
 * Return a wireless ncu instance.
 */
static gboolean
find_wireless_interface(GtkTreeModel *model,
  GtkTreePath *path,
  GtkTreeIter *iter,
  gpointer data)
{
    NwamuiNcu     **ncu_p = (NwamuiNcu **)data;
    NwamuiNcu      *ncu;

    gtk_tree_model_get(model, iter, 0, &ncu, -1);
    g_assert(NWAMUI_IS_NCU(ncu));

    if (nwamui_ncu_get_ncu_type(ncu) == NWAMUI_NCU_TYPE_WIRELESS) {
        *ncu_p = ncu;
        return TRUE;
    }
    g_object_unref(ncu);
    return FALSE;
}

extern void
join_wireless(NwamuiWifiNet *wifi)
{
    static NwamWirelessDialog *wifi_dialog = NULL;
    NwamuiDaemon *daemon = nwamui_daemon_get_instance();
    NwamuiNcp *ncp = nwamui_daemon_get_active_ncp(daemon);
    NwamuiNcu *ncu = NULL;

    /* TODO popup key dialog */
    if (wifi_dialog == NULL) {
        wifi_dialog = nwam_wireless_dialog_new();
    }

    /* ncu could be NULL due to daemon may not know the active llp */
    if ((ncu = nwamui_ncp_get_active_ncu(ncp)) == NULL ||
      nwamui_ncu_get_ncu_type(ncu) != NWAMUI_NCU_TYPE_WIRELESS) {
        /* we need find a wireless interface */
        nwamui_ncp_foreach_ncu(ncp,
          (GtkTreeModelForeachFunc)find_wireless_interface,
          (gpointer)&ncu);
    }

    g_object_unref(ncp);
    g_object_unref(daemon);

    if (ncu && nwamui_ncu_get_ncu_type(ncu) == NWAMUI_NCU_TYPE_WIRELESS) {
        gchar *device = nwamui_ncu_get_device_name(ncu);
        nwam_wireless_dialog_set_ncu(wifi_dialog, device);
        g_free(device);
    } else {
        if (ncu) {
            g_object_unref(ncu);
        }
        nwam_notification_show_message (_("There are no wireless network interfaces"), NULL, NWAM_ICON_EARTH_ERROR, NOTIFY_EXPIRES_DEFAULT);
        return;
    }

    /* wifi may be NULL -- join a wireless */
    {
        gchar *name = NULL;
        if (wifi) {
            name = nwamui_wifi_net_get_unique_name(wifi);
        }
        g_debug("%s ## wifi 0x%p %s", __func__, wifi, name ? name : "nil");
        g_free(name);
    }
    nwam_wireless_dialog_set_wifi_net(wifi_dialog, wifi);

    if (nwam_wireless_dialog_run(wifi_dialog) == GTK_RESPONSE_OK) {
        gchar *name = NULL;
        gchar *passwd = NULL;

        /* get new wifi */
        wifi = nwam_wireless_dialog_get_wifi_net(wifi_dialog);
        g_assert(wifi);

        switch (nwam_wireless_dialog_get_security(wifi_dialog)) {
        case NWAMUI_WIFI_SEC_NONE:
            break;
        case NWAMUI_WIFI_SEC_WEP_HEX:
        case NWAMUI_WIFI_SEC_WEP_ASCII:
            passwd = nwam_wireless_dialog_get_key(wifi_dialog);
            nwamui_wifi_net_set_wep_password(wifi, passwd);
            g_debug("\nWIRELESS WEP '%s'\n", passwd);
            break;
        case NWAMUI_WIFI_SEC_WPA_PERSONAL:
        case NWAMUI_WIFI_SEC_WPA_ENTERPRISE:
            name = nwam_wireless_dialog_get_wpa_username(wifi_dialog);
            passwd = nwam_wireless_dialog_get_wpa_password(wifi_dialog);
            nwamui_wifi_net_set_wpa_username(wifi, name);
            nwamui_wifi_net_set_wpa_password(wifi, passwd);
            g_debug("\nWIRELESS WAP '%s : %s'\n", name, passwd);
            break;
        default:
            g_assert_not_reached();
            break;
        }

        g_free(name);
        g_free(passwd);

        /* try to connect */
        if (nwamui_wifi_net_get_status(wifi) != NWAMUI_WIFI_STATUS_CONNECTED) {
            nwamui_ncu_set_wifi_info(ncu, wifi);
            nwamui_wifi_net_connect(wifi);
        } else {
            g_debug("wireless is connected, just need a key!!!");
        }
    }

    g_object_unref(ncu);
}

extern void
manage_known_ap()
{
#if 0
    static NwamFavNetworksDialog *fav_ap_dialog = NULL;
    NwamuiDaemon *daemon = nwamui_daemon_get_instance();
    NwamuiNcp *ncp = nwamui_daemon_get_active_ncp(daemon);
    NwamuiNcu *ncu = NULL;

    /* ncu could be NULL due to daemon may not know the active llp */
    if ((ncu = nwamui_ncp_get_active_ncu(ncp)) == NULL ||
      nwamui_ncu_get_ncu_type(ncu) != NWAMUI_NCU_TYPE_WIRELESS) {
        /* we need find a wireless interface */
        nwamui_ncp_foreach_ncu(ncp,
          (GtkTreeModelForeachFunc)find_wireless_interface,
          (gpointer)&ncu);
    }

    g_object_unref(ncp);
    g_object_unref(daemon);

    if (fav_ap_dialog == NULL) {
        fav_ap_dialog = nwam_fav_networks_dialog_new();
    }

    if (ncu && nwamui_ncu_get_ncu_type(ncu) == NWAMUI_NCU_TYPE_WIRELESS) {
        nwam_fav_networks_dialog_set_ncu(fav_ap_dialog, ncu);
        g_object_unref(ncu);
    } else {
        if (ncu) {
            g_object_unref(ncu);
        }
        nwam_notification_show_message (_("No wireless network interfaces available"), NULL, NWAM_ICON_EARTH_ERROR, NOTIFY_EXPIRES_DEFAULT);
        return;
    }

    if (nwam_fav_networks_dialog_run(fav_ap_dialog) == GTK_RESPONSE_OK) {
    }
#endif
}

extern void
change_ncu_priority()
{
#if 0
    static NwamNetPriorityDialog *net_priority_dialog = NULL;
    NwamuiDaemon *daemon = nwamui_daemon_get_instance();
    NwamuiNcp *ncp = nwamui_daemon_get_active_ncp(daemon);
    if (net_priority_dialog == NULL) {
        net_priority_dialog = nwam_net_priority_dialog_new(ncp);
    }

    g_object_unref(ncp);
    g_object_unref(daemon);

    nwam_net_priority_dialog_run(net_priority_dialog);
#endif
}

static void
default_log_handler( const gchar    *log_domain,
                     GLogLevelFlags  log_level,
                     const gchar    *message,
                     gpointer        unused_data)
{
    /* If "debug" not set then filter out debug messages. */
    if ((log_level & G_LOG_LEVEL_MASK) == G_LOG_LEVEL_DEBUG && !debug) {
        /* Do nothing */
        return;
    }

    /* Otherwise just log as usual */
    g_log_default_handler (log_domain, log_level, message, unused_data);
}

static gboolean
user_has_autoconf_auth( void )
{
	struct passwd *pwd;
	gboolean retval = FALSE;

	if ((pwd = getpwuid(getuid())) == NULL) {
		g_debug("Unable to get users password entry");
	} else if (chkauthattr(NET_AUTOCONF_AUTH, pwd->pw_name) == 0) {
		g_debug("User %s does not have %s", pwd->pw_name, NET_AUTOCONF_AUTH);
	} else {
		retval = TRUE;
	}
	return (retval);
}

static void
on_ncp_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
    gint      status_icon_index = (gint)data;

    g_assert(NWAMUI_IS_NCP(gobject));

    if (g_ascii_strcasecmp(arg1->name, "ncu-list-store") == 0) {
        update_tooltip( NULL, status_icon_index, NULL );
    }
}

int
main( int argc, char* argv[] )
{
    GnomeProgram *program;
    GOptionContext *option_context;
    GnomeClient *client;
    gint    status_icon_index = 0;
    NwamMenu *menu;
    NwamuiDaemon* daemon;
    struct sigaction act;
    NwamuiProf* prof;
    
    option_context = g_option_context_new (PACKAGE);

#ifdef ENABLE_NLS
    bindtextdomain (GETTEXT_PACKAGE, NWAM_MANAGER_LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);

    g_option_context_add_main_entries (option_context, option_entries, GETTEXT_PACKAGE );
#else
    g_option_context_add_main_entries (option_context, option_entries, NULL );
#endif

    program = gnome_program_init (PACKAGE, VERSION, LIBGNOMEUI_MODULE,
                                  argc, argv,
                                  GNOME_PARAM_APP_DATADIR, NWAM_MANAGER_DATADIR,
                                  GNOME_PARAM_GOPTION_CONTEXT, option_context,
                                  GNOME_PARAM_NONE);

    gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
                           NWAM_MANAGER_DATADIR G_DIR_SEPARATOR_S "icons");


    /* Setup log handler to trap debug messages */
    g_log_set_default_handler( default_log_handler, NULL );

    act.sa_sigaction = cleanup_and_exit;
    sigemptyset (&act.sa_mask);
    act.sa_flags = SA_SIGINFO;
    sigaction (SIGINT, &act, NULL);
    sigaction (SIGPIPE, &act, NULL);
    sigaction (SIGKILL, &act, NULL);
    sigaction (SIGTERM, &act, NULL);

    if ( ! user_has_autoconf_auth() ) {
        g_warning("User doesn't have the authorisation (%s) to run nwam-manager.", NET_AUTOCONF_AUTH );
        exit(0);
    }

    if ( !debug ) {
        client = gnome_master_client ();
        gnome_client_set_restart_command (client, argc, argv);
        gnome_client_set_restart_style (client, GNOME_RESTART_IMMEDIATELY);
    }
    else {
        g_debug("Auto restart disabled while in debug mode.");
    }

    gtk_init( &argc, &argv );

    /* Initialise Thread Support */
    if (!g_thread_supported()) {
        g_thread_init(NULL);
    }

    daemon = nwamui_daemon_get_instance ();
    status_icon_index = nwam_status_icon_create();
    
    nwam_notification_init( nwam_status_icon_get_widget(status_icon_index) );

    /* TODO - For Phase 1 */
    /* nwam_status_icon_set_activate_callback(status_icon_index, G_CALLBACK(activate_cb)); */
    menu = get_nwam_menu_instance();

    /* show status icon only nwamd is online */
    if (nwamui_daemon_get_status(daemon) == NWAMUI_DAEMON_STATUS_ACTIVE) {
        nwam_status_icon_set_visible(status_icon_index, TRUE );
    }
    else {
        nwam_status_icon_set_visible(status_icon_index, FALSE );
    }

    /* nwamui daemon signals */
    g_signal_connect(daemon, "status_changed",
	    G_CALLBACK(event_daemon_status_changed),
	    (gpointer)status_icon_index);

    g_signal_connect(daemon, "daemon_info",
	    G_CALLBACK(event_daemon_info),
	    (gpointer)status_icon_index);

    g_signal_connect(daemon, "wifi_key_needed",
	    G_CALLBACK(event_wifi_key_needed),
	    (gpointer)status_icon_index);

    g_signal_connect(daemon, "wifi_selection_needed",
	    G_CALLBACK(event_wifi_selection_needed),
	    (gpointer)status_icon_index);

    g_signal_connect(daemon, "ncu_up",
      G_CALLBACK(event_ncu_up), (gpointer)status_icon_index);
	
    g_signal_connect(daemon, "ncu_down",
      G_CALLBACK(event_ncu_down), (gpointer)status_icon_index);

    /* nwamui ncp signals */
    {
        NwamuiNcp* ncp = nwamui_daemon_get_active_ncp(daemon);

        g_signal_connect(ncp, "activate_ncu",
          G_CALLBACK(ncp_activate_ncu),
          (gpointer)status_icon_index);

        g_signal_connect(ncp, "deactivate_ncu",
          G_CALLBACK(ncp_deactivate_ncu),
          (gpointer)status_icon_index);

        g_signal_connect(ncp, "notify::ncu-list-store",
          G_CALLBACK(on_ncp_notify_cb),
          (gpointer)status_icon_index);

        g_object_unref(ncp);
    }

    /* nwamui preference signals */
    {
        NwamuiProf* prof;

        prof = nwamui_prof_get_instance ();
        g_object_get (prof,
          "action_on_no_fav_networks", &prof_action_if_no_fav_networks,
          "join_wifi_not_in_fav", &prof_ask_join_open_network,
          "join_any_fav_wifi", &prof_ask_join_fav_network,
          "add_any_new_wifi_to_fav", &prof_ask_add_to_fav,
          NULL);
        g_signal_connect(prof, "join_wifi_not_in_fav",
          G_CALLBACK(join_wifi_not_in_fav), (gpointer) daemon);
        g_signal_connect(prof, "join_any_fav_wifi",
          G_CALLBACK(join_any_fav_wifi), (gpointer) daemon);
        g_signal_connect(prof, "add_any_new_wifi_to_fav",
          G_CALLBACK(add_any_new_wifi_to_fav), (gpointer) daemon);
        g_signal_connect(prof, "action_on_no_fav_networks",
          G_CALLBACK(action_on_no_fav_networks), (gpointer) daemon);

        nwamui_prof_notify_begin (prof);
        g_object_unref (prof);

    }

    gtk_main();

    g_debug ("exiting...\n");

    nwam_notification_cleanup();
    g_object_unref (daemon);
    g_object_unref (G_OBJECT (menu));
    g_object_unref (G_OBJECT (program));
    
    return 0;
}
