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
	$(PACKAGE_CFLAGS)	\
	$(NOTIFY_CFLAGS)	\
	$(NWAM_MANAGER_CFLAGS)		\
	-DNWAM_MANAGER_DATADIR=\""$(datadir)"\"			\
	-DNWAM_MANAGER_LOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"	\
	-DGLADEDIR=\""$(gladedir)"\"			\
	-DSYSCONFDIR=\"$(sysconfdir)\"				\
	-DPRINTER_HELPER_PATH=\"$(libexecdir)\"			\
	$(NULL)

AM_LDFLAGS = \
	$(LDFLAGS)		\
	$(PACKAGE_LIBS)		\
	$(INTLLIBS)		\
	$(NWAM_MANAGER_LIBS)		\
	$(NWAM_LIBS)		\
	$(SCF_LIBS)		\
	$(NULL)

noinst_LTLIBRARIES = libnwamui.la

libnwamui_la_SOURCES =		\
	libnwamui.c		\
	nwamui_object.c \
	nwamui_ip.c \
	nwamui_wifi_net.c \
	nwamui_daemon.c \
	nwamui_enm.c \
	nwamui_ncp.c \
	nwamui_ncu.c \
	nwamui_env.c \
	nwamui_cond.c \
	nwamui_prof.c \
	nwam_pref_iface.c	\
	nwamui_known_wlan.c \
	nwam-scf.c	\
	$(NULL)

libnwamui_la_CPPFLAGS = \
	$(AM_CPPFLAGS)		\
	$(NULL)

libnwamui_la_LIBADD =                 \
        $(NWAM_MANAGER_LIBS)                    \
        $(NULL)


install-data-local:

gladedir   = $(datadir)/nwam-manager

EXTRA_DIST = 		\
	libnwamui.h \
	help_refs.h \
	nwam_pref_iface.h \
	nwamui_cond.h \
	nwamui_daemon.h \
	nwamui_enm.h \
	nwamui_env.h \
	nwamui_ip.h \
	nwamui_ncp.h \
	nwamui_ncu.h \
	nwamui_object.h \
	nwamui_prof.h \
	nwamui_svc.c \
	nwamui_svc.h \
	nwamui_wifi_net.h \
	nwamui_known_wlan.h \
	nwam-scf.h	\
	$(NULL)
