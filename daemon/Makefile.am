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
	-DGCONF_CONFIG_SOURCE=\"$(GCONF_SCHEMA_CONFIG_SOURCE)\"	\
	-DPRINTER_HELPER_PATH=\"$(libexecdir)\"			\
	$(NULL)

AM_LDFLAGS = \
	$(LDFLAGS)		\
	$(PACKAGE_LIBS)		\
	$(INTLLIBS)		\
	$(NWAM_LIBS)		\
	$(DLADM_LIBS)		\
	$(INETCFG_LIBS)		\
	$(KSTAT_LIBS)		\
	$(SECDB_LIBS)		\
	$(SCF_LIBS)		\
	$(KSTAT_LIBS)		\
	$(NWAM_MANAGER_LIBS)		\
	$(LIBNOTIFY_LIBS) \
	$(NULL)

libexec_PROGRAMS = nwam-manager

nwam_manager_SOURCES =		\
	main.c			\
	notify.c	\
	status_icon.c	\
	status_icon_tooltip.c	\
	nwam-tooltip-widget.c	\
	nwam-menu.c	\
	nwam-menuitem.c	\
	nwam-wifi-item.c	\
	nwam-env-item.c	\
	nwam-enm-item.c	\
	nwam-ncu-item.c	\
	nwam-obj-proxy-iface.c	\
	$(NULL)

nwam_manager_CPPFLAGS = \
	$(AM_CPPFLAGS)		\
	-I../capplet		\
	$(NULL)

nwam_manager_LDADD =				\
	$(top_srcdir)/common/libnwamui.la \
	$(top_srcdir)/capplet/libnwamuicapplet.la \
	$(NWAM_MANAGER_LIBS)			\
	$(NULL)

EXTRA_DIST = 		\
	notify.h	\
	status_icon.h	\
	status_icon_tooltip.h	\
	nwam-tooltip-widget.h	\
	nwam-menu.h	\
	nwam-menuitem.h	\
	nwam-wifi-item.h	\
	nwam-env-item.h	\
	nwam-enm-item.h	\
	nwam-ncu-item.h	\
	nwam-obj-proxy-iface.h	\
	$(NULL)
