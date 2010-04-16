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

SUBDIRS = common tests capplet daemon po pixmaps ui data help

NWAM_CORE_INCS = \
		nwam_core/usr/include/libdlwlan.h

EXTRA_DIST =			\
		${NWAM_CORE_INCS} \
		m4 \
        intltool-extract.in	\
        intltool-merge.in	\
        intltool-update.in	\
		gnome-doc-utils.make \
		SUNWnwam-manager.spec	\
		nwam-manager.spec

DISTCLEANFILES =		\
        intltool-extract	\
        intltool-merge		\
        intltool-update \
		gnome-doc-utils.make

DISTCHECK_CONFIGURE_FLAGS= \
	--disable-scrollkeeper

