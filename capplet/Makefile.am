# CDDL HEADER START
# 
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
# 
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
# 
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
# 
# CDDL HEADER END
#

NULL =

AM_CPPFLAGS = \
	-I$(top_srcdir)/common \
	$(PACKAGE_CFLAGS)	\
	$(NOTIFY_CFLAGS)	\
	$(NWAM_MANAGER_CFLAGS)		\
	-DNWAM_MANAGER_DATADIR=\""$(pkgdatadir)"\"			\
	-DNWAM_MANAGER_LOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"	\
	-DGLADEDIR=\""$(gladedir)"\"			\
	-DSYSCONFDIR=\"$(sysconfdir)\"				\
	-DPRINTER_HELPER_PATH=\"$(libexecdir)\"			\
	$(NULL)

AM_LDFLAGS = \
	$(LDFLAGS)		\
	$(PACKAGE_LIBS)		\
	$(INTLLIBS)		\
	$(NWAM_LIBS)		\
	$(DLADM_LIBS)		\
	$(SCF_LIBS)		\
	$(NWAM_MANAGER_LIBS)		\
	$(NULL)

bin_PROGRAMS = nwam-manager-properties

nwam_manager_properties_SOURCES =		\
	main.c			\
	nwam_wireless_dialog.c	\
	nwam_pref_dialog.c	\
	nwam_net_conf_panel.c	\
	nwam_conn_stat_panel.c	\
	nwam_env_pref_dialog.c	\
	nwam_vpn_pref_dialog.c	\
	nwam_conn_conf_ip_panel.c	\
	nwam_proxy_password_dialog.c	\
	nwam_env_svc.c	\
	nwam_wireless_chooser.c	\
	nwam_condition_vbox.c	\
	nwam_tree_view.c	\
	capplet-utils.c	\
	$(NULL)

nwam_manager_properties_CPPFLAGS = \
	$(AM_CPPFLAGS)		\
	-I../applet		\
	$(NULL)

nwam_manager_properties_LDADD =			\
    $(top_srcdir)/common/libnwamui.la \
	$(NWAM_MANAGER_LIBS)			\
	$(NULL)

noinst_LTLIBRARIES = libnwamuicapplet.la

libnwamuicapplet_la_SOURCES =		\
	nwam_wireless_dialog.c	\
	$(NULL)

libnwamuicapplet_la_CPPFLAGS = \
	$(AM_CPPFLAGS)		\
	-I../applet		\
	$(NULL)

libnwamuicapplet_la_LIBADD =			\
	$(top_srcdir)/common/libnwamui.la \
	$(NWAM_MANAGER_LIBS)			\
	$(NULL)

install-data-local:

gladedir   = $(pkgdatadir)
glade_DATA = nwam-manager-properties.glade

EXTRA_DIST = 		\
	nwam_wireless_dialog.h	\
	nwam_pref_dialog.h	\
	nwam_net_conf_panel.h	\
	nwam_conn_stat_panel.h	\
	nwam_env_pref_dialog.h	\
	nwam_vpn_pref_dialog.h	\
	nwam_conn_conf_ip_panel.h	\
	nwam_env_svc.h	\
	nwam_proxy_password_dialog.h	\
	nwam_wireless_chooser.h	\
	nwam_condition_vbox.h	\
	nwam_tree_view.h	\
	capplet-utils.h	\
	$(glade_DATA)	\
	$(NULL)

DISTCLEANFILES =	\
        $(NULL)

dist-hook:
	cd $(distdir) ; rm -f $(DISTCLEANFILES)
