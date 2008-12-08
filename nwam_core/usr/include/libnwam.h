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
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * This file contains data structures and APIs of libnwam.
 * Implementation is MT safe
 */
#ifndef _LIBNWAM_H
#define	_LIBNWAM_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>

/*
 * Common definitions
 */

/* nwam flags used for read/commit */
/* mask off the global vs. local portions of the flags value */
#define	NWAM_FLAG_GLOBAL_MASK		0xFFFFFFFF
#define	NWAM_FLAG_LOCAL_MASK		0xFFFFFFFFULL << 32
#define	NWAM_WALK_FILTER_MASK		NWAM_FLAG_LOCAL_MASK

/* Block waiting for commit if necessary */
#define	NWAM_FLAG_BLOCKING		0x00000001
/* Committed object must be new */
#define	NWAM_FLAG_CREATE		0x00000002
/* Tell destroy functions not to free handle */
#define	NWAM_FLAG_DO_NOT_FREE		0x00000004

/* nwam flags used for selecting ncu type for walk */
#define	NWAM_FLAG_NCU_TYPE_LINK		0x00000001ULL << 32
#define	NWAM_FLAG_NCU_TYPE_IP		0x00000002ULL << 32
#define	NWAM_FLAG_NCU_TYPE_ALL		(NWAM_FLAG_NCU_TYPE_LINK | \
					NWAM_FLAG_NCU_TYPE_IP)

/* nwam flags used for selecting ncu class for walk */
#define	NWAM_FLAG_NCU_CLASS_PHYS	0x00000100ULL << 32
#define	NWAM_FLAG_NCU_CLASS_IPTUN	0x00000200ULL << 32
#define	NWAM_FLAG_NCU_CLASS_IP		0x00010000ULL << 32
#define	NWAM_FLAG_NCU_CLASS_ALL_LINK	(NWAM_FLAG_NCU_CLASS_PHYS | \
					NWAM_FLAG_NCU_CLASS_IPTUN)
#define	NWAM_FLAG_NCU_CLASS_ALL_IP	NWAM_FLAG_NCU_CLASS_IP
#define	NWAM_FLAG_NCU_CLASS_ALL		(NWAM_FLAG_NCU_CLASS_ALL_IP | \
					NWAM_FLAG_NCU_CLASS_ALL_LINK)
#define	NWAM_FLAG_NCU_TYPE_CLASS_ALL	(NWAM_FLAG_NCU_CLASS_ALL | \
					NWAM_FLAG_NCU_TYPE_ALL)

/* flags used for selecting activation for walk */
#define	NWAM_FLAG_ACTIVATION_MODE_MANUAL		0x000000001ULL << 32
#define	NWAM_FLAG_ACTIVATION_MODE_SYSTEM		0x000000002ULL << 32
#define	NWAM_FLAG_ACTIVATION_MODE_PRIORITIZED		0x000000004ULL << 32
#define	NWAM_FLAG_ACTIVATION_MODE_CONDITIONAL_ANY	0x000000008ULL << 32
#define	NWAM_FLAG_ACTIVATION_MODE_CONDITIONAL_ALL	0x000000010ULL << 32
#define	NWAM_FLAG_ACTIVATION_MODE_ALL	(NWAM_FLAG_ACTIVATION_MODE_MANUAL |\
					NWAM_FLAG_ACTIVATION_MODE_SYSTEM |\
					NWAM_FLAG_ACTIVATION_MODE_PRIORITIZED |\
				NWAM_FLAG_ACTIVATION_MODE_CONDITIONAL_ANY |\
				NWAM_FLAG_ACTIVATION_MODE_CONDITIONAL_ALL)

/* nwam return codes */
typedef enum {
	NWAM_SUCCESS,			/* No error occured */
	NWAM_LIST_END,			/* End of list reached */
	NWAM_INVALID_HANDLE,		/* Entity handle is invalid */
	NWAM_HANDLE_UNBOUND,		/* Handle not bound to entity */
	NWAM_INVALID_ARG,		/* Argument is invalid */
	NWAM_PERMISSION_DENIED,		/* Insufficient privileges for action */
	NWAM_NO_MEMORY,			/* Out of memory */
	NWAM_ENTITY_EXISTS,		/* Entity already exists */
	NWAM_ENTITY_IN_USE,		/* Entity in use */
	NWAM_ENTITY_COMMITTED,		/* Entity already committed */
	NWAM_ENTITY_NOT_FOUND,		/* Entity not found */
	NWAM_ENTITY_TYPE_MISMATCH,	/* Entity type mismatch */
	NWAM_ENTITY_INVALID,		/* Validation of entity failed */
	NWAM_ENTITY_INVALID_MEMBER,	/* Entity member invalid */
	NWAM_ENTITY_INVALID_VALUE,	/* Validation of entity value failed */
	NWAM_ENTITY_MISSING_MEMBER,	/* Required member is missing */
	NWAM_ENTITY_NO_VALUE,		/* No value associated with entity */
	NWAM_ENTITY_MULTIPLE_VALUES,	/* Multiple values for entity */
	NWAM_WALK_HALTED,		/* Callback function returned nonzero */
	NWAM_ERROR_BIND,		/* Could not bind to backend */
	NWAM_ERROR_BACKEND_INIT,	/* Could not initialize backend */
	NWAM_ERROR_INTERNAL		/* Internal error */
} nwam_error_t;

#define	NWAM_MAX_NAME_LEN		128
#define	NWAM_MAX_VALUE_LEN		1024
#define	NWAM_MAX_FMRI_LEN		256
#define	NWAM_MAX_NUM_VALUES		32

/* used for getting and setting of properties */
typedef enum {
	NWAM_VALUE_TYPE_BOOLEAN,
	NWAM_VALUE_TYPE_INT64,
	NWAM_VALUE_TYPE_UINT64,
	NWAM_VALUE_TYPE_STRING,
	NWAM_VALUE_TYPE_UNKNOWN
} nwam_value_type_t;

/*
 * Definitions relevant to backend processing of NWAM data, as used
 * by netcfgd in processing libnwam backend door requests.
 */

/*
 * Functions needed to initialize/stop processing of libnwam backend data
 * (used in netcfgd).
 */
nwam_error_t nwam_backend_init(void);
void nwam_backend_fini(void);

/* Holds values of various types for getting and setting of properties */
/* Forward definition */
struct nwam_value;
typedef struct nwam_value *nwam_value_t;

/* Value-related functions. */
nwam_error_t nwam_value_create_boolean(boolean_t, nwam_value_t *);
nwam_error_t nwam_value_create_boolean_array(boolean_t *, uint_t,
    nwam_value_t *);
nwam_error_t nwam_value_create_int64(int64_t, nwam_value_t *);
nwam_error_t nwam_value_create_int64_array(int64_t *, uint_t, nwam_value_t *);
nwam_error_t nwam_value_create_uint64(uint64_t, nwam_value_t *);
nwam_error_t nwam_value_create_uint64_array(uint64_t *, uint_t, nwam_value_t *);
nwam_error_t nwam_value_create_string(char *, nwam_value_t *);
nwam_error_t nwam_value_create_string_array(char **, uint_t, nwam_value_t *);
nwam_error_t nwam_value_get_boolean(nwam_value_t, boolean_t *);
nwam_error_t nwam_value_get_boolean_array(nwam_value_t, boolean_t **, uint_t *);
nwam_error_t nwam_value_get_int64(nwam_value_t, int64_t *);
nwam_error_t nwam_value_get_int64_array(nwam_value_t, int64_t **, uint_t *);
nwam_error_t nwam_value_get_uint64(nwam_value_t, uint64_t *);
nwam_error_t nwam_value_get_uint64_array(nwam_value_t, uint64_t **, uint_t *);
nwam_error_t nwam_value_get_string(nwam_value_t, char **);
nwam_error_t nwam_value_get_string_array(nwam_value_t, char ***, uint_t *);
nwam_error_t nwam_value_get_type(nwam_value_t, nwam_value_type_t *);
nwam_error_t nwam_value_get_numvalues(nwam_value_t, uint_t *);
void nwam_value_free(nwam_value_t);
nwam_error_t nwam_value_copy(nwam_value_t, nwam_value_t *);
nwam_error_t nwam_uint64_get_value_string(const char *, uint64_t,
    const char **);
nwam_error_t nwam_value_string_get_uint64(const char *, const char *,
    uint64_t *);

/*
 * To retrieve a localized error string
 */
const char *nwam_strerror(nwam_error_t);

/*
 * State and auxiliary state describe the state of ENMs, NCUs and locations.
 */
typedef enum {
	NWAM_STATE_UNINITIALIZED = 0x0,
	NWAM_STATE_OFFLINE = 0x1,
	NWAM_STATE_ONLINE = 0x2,
	NWAM_STATE_MAINTENANCE = 0x4,
	NWAM_STATE_DEGRADED = 0x8
} nwam_state_t;

#define	NWAM_STATE_ANY			(NWAM_STATE_UNINITIALIZED | \
					NWAM_STATE_OFFLINE | \
					NWAM_STATE_ONLINE | \
					NWAM_STATE_MAINTENANCE | \
					NWAM_STATE_DEGRADED)

#define	NWAM_STATE_UNINITIALIZED_STRING	"uninitialized"
#define	NWAM_STATE_OFFLINE_STRING	"offline"
#define	NWAM_STATE_ONLINE_STRING	"online"
#define	NWAM_STATE_MAINTENANCE_STRING	"maintenance"
#define	NWAM_STATE_DEGRADED_STRING	"degraded"

/* XXX finish aux state */
typedef enum {
	NWAM_AUX_STATE_TO_DO
} nwam_aux_state_t;
/*
 * Location definitions.
 */

/*
 * SMF property names in location template property group.  Templates provide
 * a mechanism for adding new location properties via SMF (instead of having
 * to modify libnwam).
 */
#define	NWAM_LOC_TEMPLATE_PROP_LOC	"nwam_loc_property"
#define	NWAM_LOC_TEMPLATE_PROP_GROUP	"nwam_loc_property_group"
#define	NWAM_LOC_TEMPLATE_PROP_NAME	"nwam_loc_property_name"
#define	NWAM_LOC_TEMPLATE_PROP_DESC	"nwam_loc_property_description"
#define	NWAM_LOC_TEMPLATE_PROP_DEFAULT	"nwam_loc_property_default"

/* Forward definition */
struct nwam_loc_handle;
typedef struct nwam_loc_handle *nwam_loc_handle_t;

/* Forward definition */
struct nwam_loc_prop_template;
typedef struct nwam_loc_prop_template *nwam_loc_prop_template_t;

typedef enum {
	NWAM_ACTIVATION_MODE_MANUAL,
	NWAM_ACTIVATION_MODE_SYSTEM,
	NWAM_ACTIVATION_MODE_CONDITIONAL_ANY,
	NWAM_ACTIVATION_MODE_CONDITIONAL_ALL,
	NWAM_ACTIVATION_MODE_PRIORITIZED
} nwam_activation_mode_t;

#define	NWAM_ACTIVATION_MODE_MANUAL_STRING		"manual"
#define	NWAM_ACTIVATION_MODE_SYSTEM_STRING		"system"
#define	NWAM_ACTIVATION_MODE_PRIORITIZED_STRING		"prioritized"
#define	NWAM_ACTIVATION_MODE_CONDITIONAL_ANY_STRING	"conditional-any"
#define	NWAM_ACTIVATION_MODE_CONDITIONAL_ALL_STRING	"conditional-all"

/*
 * Conditions are of the form
 *
 * ncu|enm|loc name is|is-not active
 * ip-address is|is-not|is-in-range|is-not-in-range ipaddr[/prefixlen]
 * domainname is|is-not|contains|does-not-contain string
 * essid is|is-not|contains|does-not-contain string
 * bssid is|is-not <string>
 */

typedef enum {
	NWAM_CONDITION_IS,
	NWAM_CONDITION_IS_NOT,
	NWAM_CONDITION_IS_IN_RANGE,
	NWAM_CONDITION_IS_NOT_IN_RANGE,
	NWAM_CONDITION_CONTAINS,
	NWAM_CONDITION_DOES_NOT_CONTAIN
} nwam_condition_t;

#define	NWAM_CONDITION_IS_STRING			"is"
#define	NWAM_CONDITION_IS_NOT_STRING			"is-not"
#define	NWAM_CONDITION_IS_IN_RANGE_STRING		"is-in-range"
#define	NWAM_CONDITION_IS_NOT_IN_RANGE_STRING		"is-not-in-range"
#define	NWAM_CONDITION_CONTAINS_STRING			"contains"
#define	NWAM_CONDITION_DOES_NOT_CONTAIN_STRING		"does-not-contain"

typedef enum {
	NWAM_CONDITION_OBJECT_TYPE_NCU,
	NWAM_CONDITION_OBJECT_TYPE_ENM,
	NWAM_CONDITION_OBJECT_TYPE_LOC,
	NWAM_CONDITION_OBJECT_TYPE_IP_ADDRESS,
	NWAM_CONDITION_OBJECT_TYPE_DOMAINNAME,
	NWAM_CONDITION_OBJECT_TYPE_ESSID,
	NWAM_CONDITION_OBJECT_TYPE_BSSID
} nwam_condition_object_type_t;

#define	NWAM_CONDITION_OBJECT_TYPE_NCU_STRING		"ncu"
#define	NWAM_CONDITION_OBJECT_TYPE_ENM_STRING		"enm"
#define	NWAM_CONDITION_OBJECT_TYPE_LOC_STRING		"loc"
#define	NWAM_CONDITION_OBJECT_TYPE_IP_ADDRESS_STRING	"ip-address"
#define	NWAM_CONDITION_OBJECT_TYPE_DOMAINNAME_STRING	"domainname"
#define	NWAM_CONDITION_OBJECT_TYPE_ESSID_STRING		"essid"
#define	NWAM_CONDITION_OBJECT_TYPE_BSSID_STRING		"bssid"

#define	NWAM_CONDITION_ACTIVE_STRING			"active"

/*
 * Activation condition-related functions that convert activation
 * values to an appropriate string and back.
 */
nwam_error_t nwam_condition_to_condition_string(nwam_condition_object_type_t,
    nwam_condition_t, const char *, char **);
nwam_error_t nwam_condition_string_to_condition(const char *,
    nwam_condition_object_type_t *, nwam_condition_t *, char **);

/*
 * Only one location can be active at one time. As a
 * consequence, if the activation conditions of multiple
 * locations are satisfied, we need to compare activation
 * conditions to see if one is more specific than another.
 *
 * The following heuristics are applied to rate an
 * activation condition:
 * - "is" is the most specific condition
 * - it is followed by "is-in-range" and "contains"
 * - "is-not-in-range" and "does-not-contain" are next
 * - finally "is-not" is least specific
 *
 * Regarding the objects these conditions apply to:
 * - NCU, ENM and locations are most specific
 * - domainname is next
 * - IP address is next
 * - wireless BSSID is next
 * - wireless ESSID is least specific
 *
 */
nwam_error_t nwam_condition_rate(nwam_condition_object_type_t,
    nwam_condition_t, uint64_t *);


/* Location properties */

typedef enum {
	NWAM_NAMESERVICES_DNS,
	NWAM_NAMESERVICES_FILES,
	NWAM_NAMESERVICES_NIS,
	NWAM_NAMESERVICES_NISPLUS,
	NWAM_NAMESERVICES_LDAP
} nwam_nameservices_t;

#define	NWAM_NAMESERVICES_DNS_STRING		"dns"
#define	NWAM_NAMESERVICES_FILES_STRING		"files"
#define	NWAM_NAMESERVICES_NIS_STRING		"nis"
#define	NWAM_NAMESERVICES_NISPLUS_STRING	"nisplus"
#define	NWAM_NAMESERVICES_LDAP_STRING		"ldap"

#define	NWAM_LOC_PROP_ACTIVATION_MODE		"activation-mode"
#define	NWAM_LOC_PROP_CONDITION			"condition"
#define	NWAM_LOC_PROP_ENABLED			"enabled"

/* Nameservice location properties */
#define	NWAM_LOC_PROP_NAMESERVICE_DISCOVER	"nameservice-discover"
#define	NWAM_LOC_PROP_NAMESERVICES		"nameservices"
#define	NWAM_LOC_PROP_NAMESERVICES_CONFIG_FILE	"nameservices-config-file"
#define	NWAM_LOC_PROP_DOMAINNAME		"domainname"
#define	NWAM_LOC_PROP_DNS_NAMESERVICE_SERVERS	"dns-nameservice-servers"
#define	NWAM_LOC_PROP_DNS_NAMESERVICE_SEARCH	"dns-nameservice-search"
#define	NWAM_LOC_PROP_NIS_NAMESERVICE_SERVERS	"nis-nameservice-servers"
#define	NWAM_LOC_PROP_NISPLUS_NAMESERVICE_SERVERS	\
						"nisplus-nameservice-servers"
#define	NWAM_LOC_PROP_LDAP_NAMESERVICE_SERVERS	"ldap-nameservice-servers"

/* Path to hosts/ipnodes database */
#define	NWAM_LOC_PROP_HOSTS_FILE		"hosts-file"

/* NFSv4 domain */
#define	NWAM_LOC_PROP_NFSV4_DOMAIN		"nfsv4-domain"

/* IPfilter configuration */
#define	NWAM_LOC_PROP_IPFILTER_CONFIG_FILE	"ipfilter-config-file"
#define	NWAM_LOC_PROP_IPFILTER_V6_CONFIG_FILE	"ipfilter-v6-config-file"
#define	NWAM_LOC_PROP_IPNAT_CONFIG_FILE		"ipnat-config-file"
#define	NWAM_LOC_PROP_IPPOOL_CONFIG_FILE	"ippool-config-file"

/* IPsec configuration */
#define	NWAM_LOC_PROP_IKE_CONFIG_FILE		"ike-config-file"
#define	NWAM_LOC_PROP_IPSECKEY_CONFIG_FILE	"ipseckey-config-file"
#define	NWAM_LOC_PROP_IPSECPOLICY_CONFIG_FILE	"ipsecpolicy-config-file"

/* List of SMF services to enable/disable */
#define	NWAM_LOC_PROP_SVCS_ENABLE		"svcs-enable"
#define	NWAM_LOC_PROP_SVCS_DISABLE		"svcs-disable"


/*
 * NCP/NCU definitions.
 */

#define	NWAM_NCP_NAME_AUTOMATIC		"automatic"
#define	NWAM_NCP_NAME_USER		"user"

typedef struct nwam_ncp_handle *nwam_ncp_handle_t;

/* Forward definition */
struct nwam_ncu_handle;
typedef struct nwam_ncu_handle *nwam_ncu_handle_t;

typedef enum {
	NWAM_NCU_TYPE_LINK,
	NWAM_NCU_TYPE_IP,
	NWAM_NCU_TYPE_ANY
} nwam_ncu_type_t;

#define	NWAM_NCU_TYPE_LINK_STRING	"link"
#define	NWAM_NCU_TYPE_IP_STRING		"ip"

typedef enum {
	NWAM_NCU_CLASS_PHYS,
	NWAM_NCU_CLASS_IPTUN,
	NWAM_NCU_CLASS_IP,
	NWAM_NCU_CLASS_ANY
} nwam_ncu_class_t;

#define	NWAM_NCU_CLASS_PHYS_STRING	"phys"
#define	NWAM_NCU_CLASS_IPTUN_STRING	"iptun"
#define	NWAM_NCU_CLASS_IP_STRING	"ip"

typedef enum {
	NWAM_IPTUN_TYPE_IPV4,
	NWAM_IPTUN_TYPE_IPV6,
	NWAM_IPTUN_TYPE_6TO4
} nwam_iptun_type_t;

#define	NWAM_IPTUN_TYPE_IPV4_STRING	"ipv4"
#define	NWAM_IPTUN_TYPE_IPV6_STRING	"ipv6"
#define	NWAM_IPTUN_TYPE_6TO4_STRING	"6to4"

typedef enum {
	NWAM_IP_VERSION_IPV4,
	NWAM_IP_VERSION_IPV6,
	NWAM_IP_VERSION_ALL
} nwam_ip_version_t;

#define	NWAM_IP_VERSION_IPV4_STRING	"ipv4"
#define	NWAM_IP_VERSION_IPV6_STRING	"ipv6"
#define	NWAM_IP_VERSION_ALL_STRING	"all"

typedef enum {
	NWAM_ADDRSRC_DHCP,
	NWAM_ADDRSRC_DHCPV6,
	NWAM_ADDRSRC_AUTOCONF,
	NWAM_ADDRSRC_STATIC
} nwam_addrsrc_t;

#define	NWAM_ADDRSRC_DHCP_STRING	"dhcp"
#define	NWAM_ADDRSRC_DHCPV6_STRING	"dhcpv6"
#define	NWAM_ADDRSRC_AUTOCONF_STRING	"autoconf"
#define	NWAM_ADDRSRC_STATIC_STRING	"static"

typedef enum {
	NWAM_PRIORITY_MODE_EXCLUSIVE,
	NWAM_PRIORITY_MODE_SHARED,
	NWAM_PRIORITY_MODE_ALL
} nwam_priority_mode_t;

#define	NWAM_PRIORITY_MODE_EXCLUSIVE_STRING	"exclusive"
#define	NWAM_PRIORITY_MODE_SHARED_STRING	"shared"
#define	NWAM_PRIORITY_MODE_ALL_STRING		"all"

/* NCU properties common to all type/classes */
#define	NWAM_NCU_PROP_TYPE		"type"
#define	NWAM_NCU_PROP_CLASS		"class"
#define	NWAM_NCU_PROP_PARENT_NCP	"parent"
#define	NWAM_NCU_PROP_ACTIVATION_MODE	"activation-mode"
#define	NWAM_NCU_PROP_CONDITION		"condition"
#define	NWAM_NCU_PROP_ENABLED		"enabled"
#define	NWAM_NCU_PROP_PRIORITY_GROUP	"priority-group"
#define	NWAM_NCU_PROP_PRIORITY_MODE	"priority-mode"

/* Link NCU properties */
#define	NWAM_NCU_PROP_LINK_MAC_ADDR	"link-mac-addr"
#define	NWAM_NCU_PROP_LINK_AUTOPUSH	"link-autopush"
#define	NWAM_NCU_PROP_LINK_MTU		"link-mtu"

/* IP tunnel link properties */
#define	NWAM_NCU_PROP_IPTUN_TYPE	"iptun-type"
#define	NWAM_NCU_PROP_IPTUN_TSRC	"iptun-tsrc"
#define	NWAM_NCU_PROP_IPTUN_TDST	"iptun-tdst"
#define	NWAM_NCU_PROP_IPTUN_ENCR	"iptun-encr"
#define	NWAM_NCU_PROP_IPTUN_ENCR_AUTH	"iptun-encr-auth"
#define	NWAM_NCU_PROP_IPTUN_AUTH	"iptun-auth"

/* IP NCU properties */
#define	NWAM_NCU_PROP_IP_VERSION	"ip-version"
#define	NWAM_NCU_PROP_IPV4_ADDRSRC	"ipv4-addrsrc"
#define	NWAM_NCU_PROP_IPV4_ADDR		"ipv4-addr"
#define	NWAM_NCU_PROP_IPV6_ADDRSRC	"ipv6-addrsrc"
#define	NWAM_NCU_PROP_IPV6_ADDR		"ipv6-addr"

/* Some properties should only be set on creation */
#define	NWAM_NCU_PROP_READONLY(prop)	\
				(strcmp(prop, NWAM_NCU_PROP_TYPE) == 0 || \
				strcmp(prop, NWAM_NCU_PROP_CLASS) == 0 || \
				strcmp(prop, NWAM_NCU_PROP_PARENT_NCP) == 0)
/*
 * ENM definitions
 */

/* Forward definition */
struct nwam_enm_handle;
typedef struct nwam_enm_handle *nwam_enm_handle_t;

#define	NWAM_ENM_PROP_ACTIVATION_MODE	"activation-mode"
#define	NWAM_ENM_PROP_CONDITION		"condition"
#define	NWAM_ENM_PROP_ENABLED		"enabled"

/* FMRI associated with ENM */
#define	NWAM_ENM_PROP_FMRI		"fmri"

/* Start/stop scripts associated with ENM */
#define	NWAM_ENM_PROP_START		"start"
#define	NWAM_ENM_PROP_STOP		"stop"

/*
 * Location Functions
 */

/* Create a location */
extern nwam_error_t nwam_loc_create(const char *, nwam_loc_handle_t *);

/* Copy a location */
extern nwam_error_t nwam_loc_copy(nwam_loc_handle_t, const char *,
    nwam_loc_handle_t *);

/* Read a location from persistent storage */
extern nwam_error_t nwam_loc_read(const char *, uint64_t,
    nwam_loc_handle_t *);

/* Validate in-memory representation of a location */
extern nwam_error_t nwam_loc_validate(nwam_loc_handle_t, const char **);

/* Commit in-memory representation of a location to persistent storage */
extern nwam_error_t nwam_loc_commit(nwam_loc_handle_t, uint64_t);

/* Destroy a location in persistent storage */
extern nwam_error_t nwam_loc_destroy(nwam_loc_handle_t, uint64_t);

/* Free in-memory representation of a location */
extern void nwam_loc_free(nwam_loc_handle_t);

/* read all locs from persistent storage and walk through each at a time */
extern nwam_error_t nwam_walk_locs(int (*)(nwam_loc_handle_t, void *), void *,
    uint64_t, int *);

/* get/set loc name */
extern nwam_error_t nwam_loc_get_name(nwam_loc_handle_t, char **);
extern nwam_error_t nwam_loc_set_name(nwam_loc_handle_t, const char *);

/* activate loc */
extern nwam_error_t nwam_loc_enable(nwam_loc_handle_t);

/* deactivate loc */
extern nwam_error_t nwam_loc_disable(nwam_loc_handle_t);

/* walk all properties of an in-memory loc */
extern nwam_error_t nwam_loc_walk_props(nwam_loc_handle_t,
	int (*)(const char *, nwam_value_t, void *),
	void *, uint64_t, int *);

/* delete/get/set validate loc property */
extern nwam_error_t nwam_loc_delete_prop(nwam_loc_handle_t,
    const char *);
extern nwam_error_t nwam_loc_get_prop_value(nwam_loc_handle_t,
    const char *, nwam_value_t *);
extern nwam_error_t nwam_loc_set_prop_value(nwam_loc_handle_t,
    const char *, nwam_value_t);
extern nwam_error_t nwam_loc_validate_prop(nwam_loc_handle_t, const char *,
    nwam_value_t);

/* Retrieve data type */
extern nwam_error_t nwam_loc_get_prop_type(const char *, nwam_value_type_t *);

/* get default loc props */
extern nwam_error_t nwam_loc_get_default_proplist(const char ***, uint_t *);

/* SMF-defined location property template functions */
nwam_error_t nwam_walk_loc_prop_templates(int (*)(nwam_loc_prop_template_t,
    void *), void *, uint64_t, int *);
nwam_error_t nwam_get_loc_prop_template(const char *,
    nwam_loc_prop_template_t *);
nwam_error_t nwam_loc_prop_template_get_fmri(nwam_loc_prop_template_t,
    const char **);
nwam_error_t nwam_loc_prop_template_get_prop_name(nwam_loc_prop_template_t,
    const char **);
nwam_error_t nwam_loc_prop_template_get_prop_group(nwam_loc_prop_template_t,
    const char **);
nwam_error_t nwam_loc_prop_template_get_prop_desc(nwam_loc_prop_template_t,
    const char **);
nwam_error_t nwam_loc_prop_template_get_prop_default(nwam_loc_prop_template_t,
    nwam_value_t *);
void nwam_loc_prop_template_free(nwam_loc_prop_template_t);


/*
 * NCP/NCU functions
 */

/* Create an ncp */
extern nwam_error_t nwam_ncp_create(const char *, uint64_t,
	nwam_ncp_handle_t *);

/* Read an ncp from persistent storage */
extern nwam_error_t nwam_ncp_read(const char *, uint64_t, nwam_ncp_handle_t *);

/* Walk ncps */
extern nwam_error_t nwam_walk_ncps(int (*)(nwam_ncp_handle_t, void *),
	void *, uint64_t, int *);

/* Get ncp name */
extern nwam_error_t nwam_ncp_get_name(nwam_ncp_handle_t, char **);

/* Destroy ncp */
extern nwam_error_t nwam_ncp_destroy(nwam_ncp_handle_t, uint64_t);

/*
 * Walk all ncus associated with ncp.  Specific types/classes of ncu can
 * be selected via flags, or all via NWAM_FLAG_ALL.
 */
extern nwam_error_t nwam_ncp_walk_ncus(nwam_ncp_handle_t,
    int(*)(nwam_ncu_handle_t, void *), void *, uint64_t, int *);

/* Activate ncp */
extern nwam_error_t nwam_ncp_enable(nwam_ncp_handle_t);

/* Deactivate ncp */
extern nwam_error_t nwam_ncp_disable(nwam_ncp_handle_t);

/* Free in-memory representation of ncp */
extern void nwam_ncp_free(nwam_ncp_handle_t);

/* Create an ncu or read it from persistent storage */
extern nwam_error_t nwam_ncu_create(nwam_ncp_handle_t, const char *,
	nwam_ncu_type_t, nwam_ncu_class_t, nwam_ncu_handle_t *);
extern nwam_error_t nwam_ncu_read(nwam_ncp_handle_t, const char *,
	nwam_ncu_type_t, uint64_t, nwam_ncu_handle_t *);

/* Destroy an ncu in persistent storage or free the in-memory representation */
extern nwam_error_t nwam_ncu_destroy(nwam_ncu_handle_t, uint64_t);
extern void nwam_ncu_free(nwam_ncu_handle_t);

/* make a copy of existing ncu */
extern nwam_error_t nwam_ncu_copy(nwam_ncu_handle_t, const char *,
	nwam_ncu_handle_t *);

/* Commit ncu changes to persistent storage */
extern nwam_error_t nwam_ncu_commit(nwam_ncu_handle_t, uint64_t);

/* Validate ncu content */
extern nwam_error_t nwam_ncu_validate(nwam_ncu_handle_t, const char **);

/* Walk all properties in in-memory representation of ncu */
extern nwam_error_t nwam_ncu_walk_props(nwam_ncu_handle_t,
	int (*)(const char *, nwam_value_t, void *),
	void *, uint64_t, int *);

/* Get/set name of ncu, get parent ncp */
extern nwam_error_t nwam_ncu_get_name(nwam_ncu_handle_t, char **);
extern nwam_error_t nwam_ncu_name_to_typed_name(const char *, nwam_ncu_type_t,
    char **);
extern nwam_error_t nwam_ncu_set_name(nwam_ncu_handle_t, const char *);
extern nwam_error_t nwam_ncu_get_default_proplist(nwam_ncu_type_t,
    nwam_ncu_class_t, const char ***, uint_t *);
extern nwam_error_t nwam_ncu_get_ncp(nwam_ncu_handle_t, nwam_ncp_handle_t *);

/* delete/get/set/validate property from/in in-memory representation of ncu */
extern nwam_error_t nwam_ncu_delete_prop(nwam_ncu_handle_t,
	const char *);
extern nwam_error_t nwam_ncu_get_prop_value(nwam_ncu_handle_t,
	const char *, nwam_value_t *);
extern nwam_error_t nwam_ncu_set_prop_value(nwam_ncu_handle_t,
	const char *, nwam_value_t);

extern nwam_error_t nwam_ncu_validate_prop(nwam_ncu_handle_t, const char *,
	nwam_value_t);

/* Retrieve data type */
extern nwam_error_t nwam_ncu_get_prop_type(const char *, nwam_value_type_t *);

/* ENM functions */
/*
 * Obtain a specific enm handle, either be creating a new enm
 * or reading an existing one from persistent storage.
 */
extern nwam_error_t nwam_enm_create(const char *, const char *,
	nwam_enm_handle_t *);
extern nwam_error_t nwam_enm_read(const char *, uint64_t, nwam_enm_handle_t *);

/* Make a copy of existing enm */
extern nwam_error_t nwam_enm_copy(nwam_enm_handle_t, const char *,
	nwam_enm_handle_t *);

/*
 * Obtain handles for all existing enms.  Caller-specified callback
 * function will be called once for each enm, passing the handle and
 * the caller-specified arg.
 */
extern nwam_error_t nwam_walk_enms(int (*)(nwam_enm_handle_t, void *), void *,
	uint64_t, int *);

/*
 * Commit an enm to persistent storage.  Does not free the handle.
 */
extern nwam_error_t nwam_enm_commit(nwam_enm_handle_t, uint64_t);

/*
 * Remove an enm from persistent storage.
 */
extern nwam_error_t nwam_enm_destroy(nwam_enm_handle_t, uint64_t);

/*
 * Free an enm handle
 */
extern void nwam_enm_free(nwam_enm_handle_t);

/*
 * Validate an enm, or a specific enm property.  If validating
 * an entire enm, the invalid property type is returned.
 */
extern nwam_error_t nwam_enm_validate(nwam_enm_handle_t, const char **);
extern nwam_error_t nwam_enm_validate_prop(nwam_enm_handle_t, const char *,
	nwam_value_t);

/*
 * Retrieve data type
 */
extern nwam_error_t nwam_enm_get_prop_type(const char *, nwam_value_type_t *);

/*
 * Delete/get/set enm property values.
 */
extern nwam_error_t nwam_enm_delete_prop(nwam_enm_handle_t,
	const char *);
extern nwam_error_t nwam_enm_get_prop_value(nwam_enm_handle_t,
	const char *, nwam_value_t *);
extern nwam_error_t nwam_enm_set_prop_value(nwam_enm_handle_t,
	const char *, nwam_value_t);

extern nwam_error_t nwam_enm_get_default_proplist(const char ***, uint_t *);

/*
 * Walk all properties of a specific enm.  For each property, specified
 * callback function is called.  Caller is responsible for freeing memory
 * allocated for each property.
 */
extern nwam_error_t nwam_enm_walk_props(nwam_enm_handle_t,
    int (*)(const char *, nwam_value_t, void *),
    void *, uint64_t, int *);

/*
 * Get/set the name of an enm.  When getting the name, the library will
 * allocate a buffer; the caller is responsible for freeing the memory.
 */
extern nwam_error_t nwam_enm_get_name(nwam_enm_handle_t, char **);
extern nwam_error_t nwam_enm_set_name(nwam_enm_handle_t, const char *);

/*
 * Start/stop an enm.
 */
extern nwam_error_t nwam_enm_enable(nwam_enm_handle_t);
extern nwam_error_t nwam_enm_disable(nwam_enm_handle_t);


/*
 * Event notification definitions
 */
#define	NWAM_EVENT_TYPE_NOOP			0
#define	NWAM_EVENT_TYPE_REGISTER		1
#define	NWAM_EVENT_TYPE_SOURCE_DEAD		2
#define	NWAM_EVENT_TYPE_SOURCE_BACK		3
#define	NWAM_EVENT_TYPE_NO_MAGIC		4
#define	NWAM_EVENT_TYPE_INFO			5
#define	NWAM_EVENT_TYPE_IF_STATE		6
#define	NWAM_EVENT_TYPE_IF_REMOVED		7
#define	NWAM_EVENT_TYPE_LINK_STATE		8
#define	NWAM_EVENT_TYPE_LINK_REMOVED		9
#define	NWAM_EVENT_TYPE_SCAN_REPORT		10
#define	NWAM_EVENT_TYPE_ENM_ACTION		11

#define	NWAM_EVENT_STATUS_OK			0
#define	NWAM_EVENT_STATUS_NOT_HANDLED		1

#define	NWAM_EVENT_NETWORK_OBJECT_UNDEFINED	0
#define	NWAM_EVENT_NETWORK_OBJECT_LINK		1
#define	NWAM_EVENT_NETWORK_OBJECT_INTERFACE	2

#define	NWAM_EVENT_REQUEST_UNDEFINED		0
#define	NWAM_EVENT_REQUEST_WLAN			1
#define	NWAM_EVENT_REQUEST_KEY			2

#define	NWAM_NAMESIZE		32 /* LIFNAMSIZ, _LIFNAMSIZ, IFNAMESIZ ... */

typedef struct {
	char essid[NWAM_MAX_NAME_LEN];
	char bssid[NWAM_MAX_NAME_LEN];
	char signal_strength[NWAM_MAX_NAME_LEN];
	uint32_t security_mode; /* a dladm_wlan_secmode_t */
} nwam_events_wlan_t;

typedef struct nwam_events_msg nwam_events_msg_t;
struct nwam_events_msg {
	nwam_events_msg_t *next;
	int32_t type;
	int32_t size;

	union {
	    struct {
		int32_t obj_type;	/* NWAM_NETWORK* */
		char name[NWAM_NAMESIZE];
		int32_t req_type;
	    } no_magic;

	    struct {
		char message[80]; /* can be longer, must allocate structure */
	    } info;

	    struct {
		/* assumed NWAM_EVENT_NETWORK_OBJECT_IF */
		char name[NWAM_NAMESIZE];
		uint32_t flags;
		uint32_t index;
		uint32_t addr_valid; /* boolean */
		struct sockaddr addr;
		/* might be longer then sizeof(if_state) for addr */
	    } if_state;

	    struct {
		/* assumed NWAM_EVENT_NETWORK_OBJECT_LINK */
		char name[NWAM_NAMESIZE];
		int32_t link_state; /* link_state_t from sys/mac.h */
	    } link_state;

	    struct {
		/* object referred to by message type has been removed */
		char name[NWAM_NAMESIZE];
	    } removed;

	    struct {
		/* assumed NWAM_EVENT_NETWORK_OBJECT_LINK */
		char name[NWAM_NAMESIZE];
		uint16_t num_wlans;
		nwam_events_wlan_t wlans[1];
		/* space is allocated by user here for the number of wlans */
	    } wlan_scan;

	} data;
};

typedef int (*nwam_events_callback_t)(nwam_events_msg_t *, int, int);

extern int nwam_events_register(nwam_events_callback_t, pthread_t *);


#ifdef	__cplusplus
}
#endif

#endif	/* _LIBNWAM_H */
