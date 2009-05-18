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
 * File:   help_refs.h
 *
 */

#ifndef _HELP_REFS_H
#define	_HELP_REFS_H

G_BEGIN_DECLS

#define HELP_REF_INTRO                           "intro"                         /* Introduction */
#define HELP_REF_NWAM_COMPONENTS                 "nwam-components"               /* About the Network Auto-Magic Process */
#define HELP_REF_NWSTATUS_ICON                   "nwstatus-icon"                 /* About the Network Status Notification Icon */
#define HELP_REF_CONNECTION_NAMES                "connection-names"              /* How Connection Names Are Displayed */
#define HELP_REF_STATUS_CONTXTMENU               "status-contxtmenu"             /* Working With the Network Status Notification Icon Menu */
#define HELP_REF_VIEWSTATUS_INFO                 "viewstatus-info"               /* Obtaining Information About Network Status */
#define HELP_REF_OBTAINSTATUS_CURRENT            "obtainstatus-current"          /* To Show Details of Enabled Network Connections */
#define HELP_REF_CONFIGURE_NETWORK               "configure-network"             /* Controlling Network Connections */
#define HELP_REF_CONNECTWIRELESS_STARTUP         "connectwireless-startup"       /* To Modify the Default Connection Priority */
#define HELP_REF_CHGWIRED_TOWIRELESS             "chgwired-towireless"           /* To Switch From a Wired Network to a Wireless Network */
#define HELP_REF_CHGWIRELESS_TOWIRED             "chgwireless-towired"           /* To Switch From a Wireless Network to a Wired Network */
#define HELP_REF_CHWIRELESS_TOWIRELESS           "chwireless-towireless"         /* To Switch From One Wireless Network to Another Wireless Network */
#define HELP_REF_CHGWIRED_TOWIRED                "chgwired-towired"              /* To Switch From One Wired Network Connection to Another Wired Network Connection */
#define HELP_REF_ADDJOINEDIT_DIALOG              "addjoinedit-dialog"            /* About Joining Wireless Networks */
#define HELP_REF_JOIN_NETWORK                    "join-network"                  /* Joining a Wireless Network */
#define HELP_REF_WIRELESS_CHOOSER                "wireless-chooser"              /* Working With the Wireless Chooser Dialog */
#define HELP_REF_FAVORITES_OVERVIEW              "favorites-overview"            /* Working With Favorite Networks */
#define HELP_REF_NWPREFS_DIALOG                  "nwprefs-dialog"                /* About the Network Preferences Dialog */
#define HELP_REF_CONNECTSTATUS_VIEW              "connectstatus-view"            /*  Connection Status View */
#define HELP_REF_NETWORKPROFILE_VIEW             "networkprofile-view"           /* Network Profile View */
#define HELP_REF_CONNECTPROP_VIEW                "connectprop-view"              /* Connection Properties View */
#define HELP_REF_NWPROFILES_CONFIGURING          "nwprofiles-configuring"        /* Working With Network Profiles */
#define HELP_REF_PROFILE_VIEWINFO                "profile-viewinfo"              /* Viewing Information About Your Network Profile */
#define HELP_REF_PROFILES_SWITCHING              "profiles-switching"            /* To Switch From One Network Profile to Another Network Profile */
#define HELP_REF_GIKAR                           "gikar"                         /* To Add or Remove Connections from the User Network Profile */
#define HELP_REF_ENABLE_DISABLE_CONNECTION       "enable-disable-connection"     /* To Enable and Disable Connections in the User Network Profile */
#define HELP_REF_RENAME_CONNECTION               "rename-connection"             /* To Rename Connections in the User Network Profile */
#define HELP_REF_PRIORITYGRP_TASKS               "prioritygrp-tasks"             /* Working With Priority Groups */
#define HELP_REF_CONNECTPROPS_CONFIG             "connectprops-config"           /* Configuring Network Connection Properties */
#define HELP_REF_CONNECTPROPS_SETUPIPV4          "connectprops-setupipv4"        /* To Set Up IPv4 Addresses */
#define HELP_REF_CONNECTPROPS_SETUPIPV6          "connectprops-setupipv6"        /* To Set Up IPv6 Addresses */
#define HELP_REF_FAVORITE_NETWORKS               "favorite-networks"             /* To Manage a List of Favorite Wireless Networks */
#define HELP_REF_WIRELESSPROPS_CONFIGURE         "wirelessprops-configure"       /* To Configure Wireless Connection Preferences */
#define HELP_REF_NWLOCATIONS_OVERVIEW            "nwlocations-overview"          /* Working With Locations */
#define HELP_REF_LOCATIONPREFS_DIALOG            "locationprefs-dialog"          /* About the Network Locations Dialog */
#define HELP_REF_ACTIVATIONMODE_CHG              "activationmode-chg"            /* To Change a Location's Activation Mode */
#define HELP_REF_LOCATIONS_SWITCHING             "locations-switching"           /* To Switch From One Location to Another Location */
#define HELP_REF_LOCATION_EDIT                   "location-edit"                 /* Editing Locations */
#define HELP_REF_LOCATION_CONFIGNEW              "location-confignew"            /* To Add a Location */
#define HELP_REF_LOCATION_REMOVE                 "location-remove"               /* To Remove a Location */
#define HELP_REF_LOCATION_RENAME                 "location-rename"               /* To Rename a Location */
#define HELP_REF_LOCATION_DUPLICATE              "location-duplicate"            /* To Duplicate a Location  */
#define HELP_REF_EDITLOCATIONS_NAMESVC           "editlocations-namesvc"         /* To Configure Name Services For a Location */
#define HELP_REF_EDITLOCATIONS_SECURITY          "editlocations-security"        /* To Configure Security Features for a Location */
#define HELP_REF_EDITLOCATIONS_SMFSVC            "editlocations-smfsvc"          /* Enabling and Disabling SMF Services for a Location */
#define HELP_REF_VPN_CONFIGURING                 "vpn-configuring"               /* Configuring VPN Applications */
#define HELP_REF_VPNPREFS_DIALOG                 "vpnprefs-dialog"               /* About the VPN Applications Dialog */
#define HELP_REF_VPNAPP_CLICONTROLLED            "vpnapp-clicontrolled"          /* To Add a Command-Line VPN Application */
#define HELP_REF_VPN_ADDSERVICE                  "vpn-addservice"                /* To Add a VPN Application Service */
#define HELP_REF_VPNCONFIG_STARTSTOP             "vpnconfig-startstop"           /* To Start and Stop a VPN Application */
#define HELP_REF_VPN_SETRULES                    "vpn-setrules"                  /* To Set Rules for a VPN Application */
#define HELP_REF_VPNAPPLICATION_REMOVE           "vpnapplication-remove"         /* To Remove a VPN Application */
#define HELP_REF_GIMFA                           "gimfa"                         /* Reference */
#define HELP_REF_EDITING_TABLES                  "editing-tables"                /* Working With Editable Tables */
#define HELP_REF_RULES_DIALOG                    "rules-dialog"                  /* Working With the Rules Dialog */

G_END_DECLS

#endif	/* _HELP_REFS_H */

