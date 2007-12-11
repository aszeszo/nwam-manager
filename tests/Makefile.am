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
	$(PACKAGE_LIBS)		\
	$(INTLLIBS)		\
	$(NWAM_MANAGER_LIBS)		\
	$(NWAM_LIBS)		\
	$(NULL)

noinst_PROGRAMS = test-nwam

test_nwam_SOURCES =		\
	main.c		\
	$(NULL)

install-data-local:

EXTRA_DIST = 		\
	$(NULL)

