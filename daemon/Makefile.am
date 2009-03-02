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
	nwam-menu.c	\
	nwam-menuitem.c	\
	nwam-wifi-action.c	\
	nwam-env-action.c	\
	nwam-enm-action.c	\
	nwam-wifi-group.c	\
	nwam-env-group.c	\
	nwam-enm-group.c	\
	nwam-obj-proxy-iface.c	\
	$(NULL)

nwam_manager_CPPFLAGS = \
	$(AM_CPPFLAGS)		\
	$(NULL)

nwam_manager_LDADD =				\
	$(top_srcdir)/common/libnwamui.la \
	$(top_srcdir)/capplet/libnwamuicapplet.la \
	$(NWAM_MANAGER_LIBS)			\
	$(NULL)

@INTLTOOL_SCHEMAS_RULE@
schemasdir       = $(GCONF_SCHEMA_FILE_DIR)
schemas_in_files = nwam-manager.schemas.in
schemas_DATA     = $(schemas_in_files:.schemas.in=.schemas)
SUFFIXES = .desktop.in .desktop

.desktop.in.desktop:
	LC_ALL=C sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|;s|\@PROGRAM\@|nwam-manager|" $< > $@

autosessiondir      = $(sysconfdir)/xdg/autostart
autosession_in_files = nwam-manager.desktop.in
autosession_DATA    = $(autosession_in_files:.desktop.in=.desktop)

if GCONF_SCHEMAS_INSTALL
install-data-local:
	if test -z "$(DESTDIR)" ; then \
		for p in $(schemas_DATA) ; do \
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/daemon/$$p ; \
		done ; \
	fi
else
install-data-local:
endif

uidir           = $(pkgdatadir)
ui_DATA         = nwam-ui.xml

EXTRA_DIST = 		\
	notify.h	\
	nwam-enm-action.h	\
	nwam-enm-group.h	\
	nwam-env-action.h	\
	nwam-env-group.h	\
	nwam-menu.h	\
	nwam-menuitem.h	\
	nwam-obj-proxy-iface.h	\
	nwam-wifi-action.h	\
	nwam-wifi-group.h	\
	status_icon.h	\
	$(ui_DATA)	\
	$(schemas_in_files)	\
	$(autosession_in_files)	\
	$(NULL)

DISTCLEANFILES =	\
	$(autosession_DATA)	\
        $(schemas_DATA) \
        $(NULL)

dist-hook:
	cd $(distdir) ; rm -f $(DISTCLEANFILES)
