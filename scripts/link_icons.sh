#/bin/bash
#
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


#
# Script to create symbolic links to NWAM icons in /usr/share/icons when the
# install is being done to an alternative basedir.
#
# Due to the missing entries for the "NxN/status" directories in the current
# 2.20 hicolor theme (it's fixed in 2.21) we need to patch the index.theme
# file to include these directories.
#
# Usage: link_icons.sh [<installdir>]
#
#        e.g. link_icons.sh /opt/nwam-manager/share/icons
#
# NOTE : This should only be used for development purposes, not for production
# installations.
#
# 
TMP_PATCH=/tmp/patch.$$

DEST_THEME=hicolor
DEST_THEME_BASE_DIR=/usr/share/icons
DEST_THEME_DIR=${DEST_THEME_BASE_DIR}/${DEST_THEME}

trap "rm -f $TMP_PATCH" 0 1 2 15

if [ ! -w ${DEST_THEME_BASE_DIR} ]; then
    echo "Requires more privileges to write to ${DEST_THEME_BASE_DIR}"
    exit 1
fi

if [ -n "$1" ]; then
    SRC_BASEDIR="$1"
    shift
else
    SRC_BASEDIR="$PWD"
fi

if [ ! -d "${SRC_BASEDIR}/${DEST_THEME}" ]; then
    echo "Please specify, or run from, icons directory"
fi

echo "Creating symbolic links for images..."
find $SRC_BASEDIR/${DEST_THEME} -name \*.png  | \
    sed -e "s@^\(.*\)\(${DEST_THEME}.*\)\(/.*\)@[ ! -d ${DEST_THEME_BASE_DIR}/\2 ] \&\& mkdir ${DEST_THEME_BASE_DIR}/\2; ln -s \1\2\3 ${DEST_THEME_BASE_DIR}/\2\3@" | \
    /bin/sh -s

STATUS_CNT=`grep -c "/status" ${DEST_THEME_DIR}/index.theme`
if [ "${STATUS_CNT}" -gt 0 ]; then
    echo "Patch for NxN/status dirs already applied, skipping...."
else
    sed -e '1,/PATCH_START/d' $0 > $TMP_PATCH

    gpatch --backup ${DEST_THEME_DIR}/index.theme $TMP_PATCH
fi

echo "Updating icon cache in ${DEST_THEME_DIR}"
gtk-update-icon-cache ${DEST_THEME_DIR}

exit 0

PATCH_START
--- /usr/share/icons/hicolor/index.theme	2007-11-05 08:55:11.000000000 +0000
+++ /mnt/usr/share/icons/hicolor/index.theme	2007-11-22 13:49:32.987480000 +0000
@@ -2,7 +2,7 @@
 Name=Hicolor
 Comment=Fallback icon theme
 Hidden=true
-Directories=192x192/apps,128x128/actions,128x128/apps,128x128/devices,128x128/filesystems,128x128/mimetypes,96x96/actions,96x96/apps,96x96/devices,96x96/filesystems,96x96/mimetypes,72x72/apps,64x64/actions,64x64/apps,64x64/devices,64x64/filesystems,64x64/mimetypes,48x48/actions,48x48/apps,48x48/devices,48x48/filesystems,48x48/mimetypes,36x36/apps,32x32/actions,32x32/apps,32x32/devices,32x32/filesystems,32x32/mimetypes,22x22/actions,22x22/apps,22x22/devices,22x22/filesystems,22x22/mimetypes,16x16/actions,16x16/apps,16x16/devices,16x16/filesystems,16x16/mimetypes,scalable/actions,scalable/apps,scalable/devices,scalable/filesystems,scalable/mimetypes,16x16/stock/chart,16x16/stock/code,16x16/stock/data,16x16/stock/document,16x16/stock/form,16x16/stock/generic,16x16/stock/image,16x16/stock/io,16x16/stock/media,16x16/stock/navigation,16x16/stock/net,16x16/stock/object,16x16/stock/table,16x16/stock/text,24x24/stock/chart,24x24/stock/code,24x24/stock/data,24x24/stock/document,24x24/stock/form,24x24/stock/generic,24x24/stock/image,24x24/stock/io,24x24/stock/media,24x24/stock/navigation,24x24/stock/net,24x24/stock/object,24x24/stock/table,24x24/stock/text,32x32/stock/chart,32x32/stock/code,32x32/stock/data,32x32/stock/document,32x32/stock/form,32x32/stock/generic,32x32/stock/image,32x32/stock/io,32x32/stock/media,32x32/stock/navigation,32x32/stock/net,32x32/stock/object,32x32/stock/table,32x32/stock/text,36x36/stock/chart,36x36/stock/code,36x36/stock/data,36x36/stock/document,36x36/stock/form,36x36/stock/generic,36x36/stock/image,36x36/stock/io,36x36/stock/media,36x36/stock/navigation,36x36/stock/net,36x36/stock/object,36x36/stock/table,36x36/stock/text,48x48/stock/chart,48x48/stock/code,48x48/stock/data,48x48/stock/document,48x48/stock/form,48x48/stock/generic,48x48/stock/image,48x48/stock/io,48x48/stock/media,48x48/stock/navigation,48x48/stock/net,48x48/stock/object,48x48/stock/table,48x48/stock/text
+Directories=192x192/apps,128x128/actions,128x128/apps,128x128/devices,128x128/filesystems,128x128/mimetypes,96x96/actions,96x96/apps,96x96/devices,96x96/filesystems,96x96/mimetypes,72x72/apps,64x64/actions,64x64/apps,64x64/devices,64x64/filesystems,64x64/mimetypes,48x48/actions,48x48/apps,48x48/devices,48x48/filesystems,48x48/mimetypes,36x36/apps,32x32/actions,32x32/apps,32x32/devices,32x32/filesystems,32x32/mimetypes,22x22/actions,22x22/apps,22x22/devices,22x22/filesystems,22x22/mimetypes,16x16/actions,16x16/apps,16x16/devices,16x16/filesystems,16x16/mimetypes,scalable/actions,scalable/apps,scalable/devices,scalable/filesystems,scalable/mimetypes,16x16/stock/chart,16x16/stock/code,16x16/stock/data,16x16/stock/document,16x16/stock/form,16x16/stock/generic,16x16/stock/image,16x16/stock/io,16x16/stock/media,16x16/stock/navigation,16x16/stock/net,16x16/stock/object,16x16/stock/table,16x16/stock/text,24x24/stock/chart,24x24/stock/code,24x24/stock/data,24x24/stock/document,24x24/stock/form,24x24/stock/generic,24x24/stock/image,24x24/stock/io,24x24/stock/media,24x24/stock/navigation,24x24/stock/net,24x24/stock/object,24x24/stock/table,24x24/stock/text,32x32/stock/chart,32x32/stock/code,32x32/stock/data,32x32/stock/document,32x32/stock/form,32x32/stock/generic,32x32/stock/image,32x32/stock/io,32x32/stock/media,32x32/stock/navigation,32x32/stock/net,32x32/stock/object,32x32/stock/table,32x32/stock/text,36x36/stock/chart,36x36/stock/code,36x36/stock/data,36x36/stock/document,36x36/stock/form,36x36/stock/generic,36x36/stock/image,36x36/stock/io,36x36/stock/media,36x36/stock/navigation,36x36/stock/net,36x36/stock/object,36x36/stock/table,36x36/stock/text,48x48/stock/chart,48x48/stock/code,48x48/stock/data,48x48/stock/document,48x48/stock/form,48x48/stock/generic,48x48/stock/image,48x48/stock/io,48x48/stock/media,48x48/stock/navigation,48x48/stock/net,48x48/stock/object,48x48/stock/table,48x48/stock/text,16x16/status,24x24/status,32x32/status,48x48/status
 
 [16x16/actions]
 Size=16
@@ -29,6 +29,12 @@
 Context=MimeTypes
 Type=Threshold
 
+[16x16/status]
+Size=16
+Context=Status
+Type=Threshold
+
+
 [22x22/actions]
 Size=22
 Context=Actions
@@ -54,6 +60,16 @@
 Context=MimeTypes
 Type=Threshold
 
+[24x24/status]
+Size=24
+Context=Status
+Type=Threshold
+
+[32x32/status]
+Size=32
+Context=Status
+Type=Threshold
+
 [32x32/actions]
 Size=32
 Context=Actions
@@ -84,6 +100,11 @@
 Context=Applications
 Type=Threshold
 
+[48x48/status]
+Size=48
+Context=Status
+Type=Threshold
+
 [48x48/actions]
 Size=48
 Context=Actions
