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

/* Those functions are taken from the minilander project.
 * 
 * Functions that really should be in a common library somewhere,
 * to get and set SMF service property values.
 */

#include <libnwam.h>
#include "nwam-scf.h"


static void
release_scf_resources(scf_resources_t *res)
{
	scf_entry_destroy(res->sr_ent);
	scf_transaction_destroy(res->sr_tx);
	scf_value_destroy(res->sr_val);
	scf_property_destroy(res->sr_prop);
	scf_pg_destroy(res->sr_pg);
	scf_snapshot_destroy(res->sr_snap);
	scf_instance_destroy(res->sr_inst);
	(void) scf_handle_unbind(res->sr_handle);
	scf_handle_destroy(res->sr_handle);
}

static int
create_scf_resources(const char *fmri, scf_resources_t *res, scf_error_t *serr)
{
	*serr = SCF_ERROR_NONE;
	bzero(res, sizeof (scf_resources_t));

	if ((res->sr_handle = scf_handle_create(SCF_VERSION)) == NULL) {
		*serr = scf_error();
		return (-1);
	}

	if (scf_handle_bind(res->sr_handle) != 0) {
		*serr = scf_error();
		scf_handle_destroy(res->sr_handle);
		return (-1);
	}

	if ((res->sr_inst = scf_instance_create(res->sr_handle)) == NULL)
		goto cleanup;

	if (scf_handle_decode_fmri(res->sr_handle, fmri, NULL, NULL,
	    res->sr_inst, NULL, NULL, SCF_DECODE_FMRI_REQUIRE_INSTANCE) != 0)
		goto cleanup;

	if ((res->sr_snap = scf_snapshot_create(res->sr_handle)) == NULL)
		goto cleanup;

	if (scf_instance_get_snapshot(res->sr_inst, "running", res->sr_snap)
	    != 0)
		goto cleanup;

	if ((res->sr_pg = scf_pg_create(res->sr_handle)) == NULL)
		goto cleanup;

	if ((res->sr_prop = scf_property_create(res->sr_handle)) == NULL)
		goto cleanup;

	if ((res->sr_val = scf_value_create(res->sr_handle)) == NULL)
		goto cleanup;

	if ((res->sr_tx = scf_transaction_create(res->sr_handle)) == NULL)
		goto cleanup;

	if ((res->sr_ent = scf_entry_create(res->sr_handle)) == NULL)
		goto cleanup;

	return (0);

cleanup:
	*serr =  scf_error();
	release_scf_resources(res);
	return (-1);
}

static int
get_property_value(const char *fmri, const char *pg, const char *prop,
    boolean_t running, scf_resources_t *res, scf_error_t *serr)
{
	*serr = SCF_ERROR_NONE;
	if (create_scf_resources(fmri, res, serr) != 0)
		return (-1);

	if (scf_instance_get_pg_composed(res->sr_inst,
	    running ? res->sr_snap : NULL, pg, res->sr_pg) != 0)
		goto cleanup;

	if (scf_pg_get_property(res->sr_pg, prop, res->sr_prop) != 0)
		goto cleanup;

	if (scf_property_get_value(res->sr_prop, res->sr_val) != 0)
		goto cleanup;

	return (0);

cleanup:
	*serr = scf_error();
	release_scf_resources(res);
	return (-1);
}

int
get_active_ncp(char *namestr, size_t namelen, scf_error_t *serr)
{
	int		rtn = -1;
	scf_resources_t	res;

	if (get_property_value(NP_DEFAULT_FMRI, NETCFG_PG,
	    NETCFG_ACTIVE_NCP_PROP, B_TRUE, &res, serr) != 0)
		return (-1);

	if (scf_value_get_astring(res.sr_val, namestr, namelen) == 0)
		goto cleanup;

	rtn = 0;

cleanup:
	*serr = (rtn == -1) ? scf_error() : SCF_ERROR_NONE;
	release_scf_resources(&res);
	return (rtn);
}