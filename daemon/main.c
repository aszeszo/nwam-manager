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


#include "libnwamui.h"
#include "nwam_tree_view.h"

#include "status_icon.h"

/*
 * Security Auth needed for NWAM to work. Console User and Network Management
 * profiles will have this.
 */
#define	NET_AUTOCONF_AUTH	"solaris.network.autoconf"

/* Command-line options */
static gboolean debug = FALSE;

static GOptionEntry option_entries[] = {
    {"debug", 'D', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &debug, N_("Enable debugging messages"), NULL },
    {NULL}
};

#define NWAM_MANAGER_LOCK	"nwam-manager-lock"
gint prof_action_if_no_fav_networks = NULL;
gboolean prof_ask_join_open_network;
gboolean prof_ask_join_fav_network;
gboolean prof_ask_add_to_fav;

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
            g_debug ("$SUN_SUNRAY_TOKEN = %s", sunray_token);
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
join_wifi_not_in_fav (NwamuiProf* prof, gboolean val, gpointer data)
{
    NwamuiDaemon* daemon = NWAMUI_DAEMON (data);
    gchar *body;
    
    prof_ask_join_open_network = val;
    body = g_strdup_printf ("prof_ask_join_open_network = %d", prof_ask_join_open_network);
/*     nwam_notification_show_message("Prof signal", body, NULL, NOTIFY_EXPIRES_DEFAULT); */
    g_free (body);
}

static void
join_any_fav_wifi (NwamuiProf* prof, gboolean val, gpointer data)
{
    NwamuiDaemon* daemon = NWAMUI_DAEMON (data);
    gchar *body;

    prof_ask_join_fav_network = val;
    body = g_strdup_printf ("prof_ask_join_fav_network = %d", prof_ask_join_fav_network);
/*     nwam_notification_show_message("Prof signal", body, NULL, NOTIFY_EXPIRES_DEFAULT); */
    g_free (body);
}

static void
add_any_new_wifi_to_fav (NwamuiProf* prof, gboolean val, gpointer data)
{
    NwamuiDaemon* daemon = NWAMUI_DAEMON (data);
    gchar *body;

    prof_ask_add_to_fav = val;
    body = g_strdup_printf ("prof_ask_add_to_fav = %d", prof_ask_add_to_fav);
/*     nwam_notification_show_message("Prof signal", body, NULL, NOTIFY_EXPIRES_DEFAULT); */
    g_free (body);
}

static void
action_on_no_fav_networks (NwamuiProf* prof, gint val, gpointer data)
{
    NwamuiDaemon* daemon = NWAMUI_DAEMON (data);
    gchar *body;

    prof_action_if_no_fav_networks = val;
    body = g_strdup_printf ("prof_action_if_no_fav_networks = %d", prof_action_if_no_fav_networks);
/*     nwam_notification_show_message("Prof signal", body, NULL, NOTIFY_EXPIRES_DEFAULT); */
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
init_wait_for_embedding_idle(gpointer data)
{
    GtkStatusIcon* status_icon = GTK_STATUS_ICON(data);

    if ( gtk_status_icon_is_embedded( status_icon ) ) {
        nwam_status_icon_run(NWAM_STATUS_ICON(status_icon));

        return FALSE;
    }
    return TRUE;
}

static gboolean
init_wait_for_embedding(gpointer data)
{
#if 0
    GtkStatusIcon* status_icon = GTK_STATUS_ICON(data);

    /* Wait for the Notification Area to be ready */
    while ( !notification_area_ready( status_icon  ) ) {
        sleep(2);
    }
    /* We make sure icon is available, then we get everything work */
    nwam_status_icon_run(NWAM_STATUS_ICON(status_icon));

    return( TRUE );
#else
    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
      init_wait_for_embedding_idle,
      g_object_ref(data),
      g_object_unref);
    return( TRUE );
#endif
}

int
main( int argc, char* argv[] )
{
    GnomeProgram *program;
    GOptionContext *option_context;
    GnomeClient *client;
    struct sigaction act;
    NwamuiProf* prof;
    GtkStatusIcon *status_icon = NULL;

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
    nwamui_util_default_log_handler_init();

    nwamui_util_set_debug_mode( debug );

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

    if ( !nwamui_util_is_debug_mode() ) {
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

    NWAM_TYPE_TREE_VIEW; /* Dummy to cause NwamTreeView to be loaded for GtkBuilder to find symbol */

    
    /* nwamui preference signals */
    {
        NwamuiDaemon* daemon;
        NwamuiProf* prof;

        daemon = nwamui_daemon_get_instance ();
        prof = nwamui_prof_get_instance ();

        g_signal_connect(prof, "join_wifi_not_in_fav",
          G_CALLBACK(join_wifi_not_in_fav), (gpointer) daemon);
        g_signal_connect(prof, "join_any_fav_wifi",
          G_CALLBACK(join_any_fav_wifi), (gpointer) daemon);
        g_signal_connect(prof, "add_any_new_wifi_to_fav",
          G_CALLBACK(add_any_new_wifi_to_fav), (gpointer) daemon);
        g_signal_connect(prof, "action_on_no_fav_networks",
          G_CALLBACK(action_on_no_fav_networks), (gpointer) daemon);

        g_object_get (prof,
          "action_on_no_fav_networks", &prof_action_if_no_fav_networks,
          "join_wifi_not_in_fav", &prof_ask_join_open_network,
          "join_any_fav_wifi", &prof_ask_join_fav_network,
          "add_any_new_wifi_to_fav", &prof_ask_add_to_fav,
          NULL);

        nwamui_prof_notify_begin (prof);
        g_object_unref (prof);
        g_object_unref (daemon);
    }

    /* Setup function to be called after the mainloop has been created
     * this is to avoid confusion when calling gtk_main_iteration to get to
     * the point where the status icon's embedded flag is correctly set
     */
    status_icon = nwam_status_icon_new();
    gtk_init_add(init_wait_for_embedding, (gpointer)status_icon);
    if ( nwamui_util_is_debug_mode() ) {
        g_message("Show status icon initially on debug mode.");
    }

    gtk_main();

    g_debug ("exiting...");

    g_object_unref(status_icon);
    g_object_unref (G_OBJECT (program));
    
    return 0;
}
