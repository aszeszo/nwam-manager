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
        ncu_ref = nwamui_ncp_get_active_ncu(ncp);
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
    g_free(cstr);

    g_object_unref(ncp);
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
#if 0
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

    if (capplet_dialog_run(NWAM_PREF_IFACE(wifi_dialog), NULL) == GTK_RESPONSE_OK) {
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
#endif
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

static gboolean
init_wait_for_embedding(gpointer data)
{
    GtkStatusIcon* status_icon = GTK_STATUS_ICON(data);

    /* Wait for the Notification Area to be ready */
    while ( !notification_area_ready( status_icon  ) ) {
        sleep(2);
    }

    return( TRUE );
}

int
main( int argc, char* argv[] )
{
    GnomeProgram *program;
    GOptionContext *option_context;
    GnomeClient *client;
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

    status_icon = GTK_STATUS_ICON(nwam_status_icon_new());
    nwam_notification_init(GTK_STATUS_ICON(status_icon));

    /* Setup function to be called after the mainloop has been created
     * this is to avoid confusion when calling gtk_main_iteration to get to
     * the point where the status icon's embedded flag is correctly set
     */
    gtk_init_add(init_wait_for_embedding, (gpointer)status_icon);

    daemon = nwamui_daemon_get_instance ();
    status_icon = GTK_STATUS_ICON(nwam_status_icon_new());
    
    nwam_notification_init(status_icon);

    gtk_main();

    g_debug ("exiting...\n");

    nwam_notification_cleanup();
    g_object_unref(status_icon);
    g_object_unref (daemon);
    g_object_unref (G_OBJECT (program));
    
    return 0;
}
