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
	-DPACKAGE=\"$(PACKAGE)\" \
	-DNWAM_MANAGER_DATADIR=\""$(pkgdatadir)"\"			\
	-DNWAM_MANAGER_LOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"	\
	-DGLADEDIR=\""$(gladedir)"\"			\
	-DUIDATA=\""$(uidir)/$(ui_DATA)"\"			\
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
	$(KSTAT_LIBS)		\
	$(SECDB_LIBS)		\
	$(SCF_LIBS)		\
	$(KSTAT_LIBS)		\
	$(NWAM_MANAGER_LIBS)		\
	$(LIBNOTIFY_LIBS) \
	$(NULL)

noinst_PROGRAMS = test-nwam

test_nwam_SOURCES =		\
	main.c		\
	$(NULL)

test_nwam_LDADD =			\
	$(top_srcdir)/common/libnwamui.la \
	$(NWAM_MANAGER_LIBS)

install-data-local:

EXTRA_DIST = 		\
	$(NULL)

