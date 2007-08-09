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

install-data-local:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		$(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  After install, run this:"; \
		echo "***   $(gtk_update_icon_cache)"; \
	fi

gtk_update_icon_cache = /usr/bin/gtk-update-icon-cache -f -t $(icondir)

icondir   = $(datadir)/$(PACKAGE)/icons
icon_DATA = \
	earth-info.png \
	earth-normal.png \
	network-idle.png \
	network-offline.png \
	network-wireless.png \
	wireless-strength-0-24.png \
	wireless-strength-25-49.png \
	wireless-strength-50-74.png \
	wireless-strength-75-100.png \
	wireless-fragile.png

EXTRA_DIST = 		\
	$(icon_DATA)
	$(NULL)
