/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set expandtab ts=4 shiftwidth=4: */
/*
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
 */

/*
 * Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
 */

#include <libscf.h>
#include <strings.h>

#ifndef nwam_scf_H
#define	nwam_scf_H

// The below functions should be used directly from the
// minilander "libnwam.h", but it's not yet available. 
#define	NWAMUI_FMRI	"svc:/network/physical:default"
#define	NWAM_NETCFG_PG		"netcfg"
#define	NWAM_NETCFG_ACTIVE_NCP_PROP	"active_ncp"
#define	NWAM_NCP_NAME_DEF_FIXED	"DefaultFixed"
#define	NWAM_NCP_DEF_FIXED(name)	\
			(strcasecmp(name, NWAM_NCP_NAME_DEF_FIXED) == 0)



typedef struct scf_resources {
	scf_handle_t		*sr_handle;
	scf_instance_t		*sr_inst;
	scf_snapshot_t		*sr_snap;
	scf_propertygroup_t	*sr_pg;
	scf_property_t		*sr_prop;
	scf_value_t		*sr_val;
	scf_transaction_t	*sr_tx;
	scf_transaction_entry_t	*sr_ent;
} scf_resources_t;

int get_active_ncp(char *namestr, size_t namelen, scf_error_t *serr);

#endif	/* nwam_scf_H */

