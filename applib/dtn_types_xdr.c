/*
 * Please do not edit this file.
 * It was generated using rpcgen.
 */

#include "dtn_types.h"
/*
 *    Copyright 2004-2006 Intel Corporation
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
*/

/**********************************
 * This file defines the types used in the DTN client API. The structures are
 * autogenerated using rpcgen, as are the marshalling and unmarshalling
 * XDR routines.
 */

#include <limits.h>
#ifndef ARG_MAX
#define ARG_MAX _POSIX_ARG_MAX
#endif

#include <rpc/rpc.h>


/**********************************
 * Constants.
 * (Note that we use #defines to get the comments as well)
 */
#define DTN_MAX_ENDPOINT_ID 256	/* max endpoint_id size (bytes) */
#define DTN_MAX_PATH_LEN PATH_MAX	/* max path length */
#define DTN_MAX_EXEC_LEN ARG_MAX	/* length of string passed to exec() */
#define DTN_MAX_AUTHDATA 1024		/* length of auth/security data*/
#define DTN_MAX_REGION_LEN 64		/* 64 chars "should" be long enough */
#define DTN_MAX_BUNDLE_MEM 50000	/* biggest in-memory bundle is ~50K*/

/**
 * Specification of a dtn endpoint id, i.e. a URI, implemented as a
 * fixed-length char buffer. Note that for efficiency reasons, this
 * fixed length is relatively small (256 bytes). 
 * 
 * The alternative is to use the string XDR type but then all endpoint
 * ids would require malloc / free which is more prone to leaks / bugs.
 */

bool_t
xdr_dtn_endpoint_id_t (XDR *xdrs, dtn_endpoint_id_t *objp)
{
	register int32_t *buf;

	int i;
	 if (!xdr_opaque (xdrs, objp->uri, DTN_MAX_ENDPOINT_ID))
		 return FALSE;
	return TRUE;
}

/**
 * A registration cookie.
 */

bool_t
xdr_dtn_reg_id_t (XDR *xdrs, dtn_reg_id_t *objp)
{
	register int32_t *buf;

	 if (!xdr_u_int (xdrs, objp))
		 return FALSE;
	return TRUE;
}

/**
 * DTN timeouts are specified in seconds.
 */

bool_t
xdr_dtn_timeval_t (XDR *xdrs, dtn_timeval_t *objp)
{
	register int32_t *buf;

	 if (!xdr_u_int (xdrs, objp))
		 return FALSE;
	return TRUE;
}

/**
 * An infinite wait is a timeout of -1.
 */
#define DTN_TIMEOUT_INF ((dtn_timeval_t)-1)

/**
 * Specification of a service tag used in building a local endpoint
 * identifier.
 * 
 * Note that the application cannot (in general) expect to be able to
 * use the full DTN_MAX_ENDPOINT_ID, as there is a chance of overflow
 * when the daemon concats the tag with its own local endpoint id.
 */

bool_t
xdr_dtn_service_tag_t (XDR *xdrs, dtn_service_tag_t *objp)
{
	register int32_t *buf;

	int i;
	 if (!xdr_vector (xdrs, (char *)objp->tag, DTN_MAX_ENDPOINT_ID,
		sizeof (char), (xdrproc_t) xdr_char))
		 return FALSE;
	return TRUE;
}

/**
 * Value for an unspecified registration cookie (i.e. indication that
 * the daemon should allocate a new unique id).
 */

/**
 * Registration delivery failure actions
 *     DTN_REG_DROP   - drop bundle if registration not active
 *     DTN_REG_DEFER  - spool bundle for later retrieval
 *     DTN_REG_EXEC   - exec program on bundle arrival
 */

bool_t
xdr_dtn_reg_failure_action_t (XDR *xdrs, dtn_reg_failure_action_t *objp)
{
	register int32_t *buf;

	 if (!xdr_enum (xdrs, (enum_t *) objp))
		 return FALSE;
	return TRUE;
}

/**
 * Registration state.
 */

bool_t
xdr_dtn_reg_info_t (XDR *xdrs, dtn_reg_info_t *objp)
{
	register int32_t *buf;

	 if (!xdr_dtn_endpoint_id_t (xdrs, &objp->endpoint))
		 return FALSE;
	 if (!xdr_dtn_reg_id_t (xdrs, &objp->regid))
		 return FALSE;
	 if (!xdr_dtn_reg_failure_action_t (xdrs, &objp->failure_action))
		 return FALSE;
	 if (!xdr_dtn_timeval_t (xdrs, &objp->expiration))
		 return FALSE;
	 if (!xdr_bool (xdrs, &objp->init_passive))
		 return FALSE;
	 if (!xdr_bytes (xdrs, (char **)&objp->script.script_val, (u_int *) &objp->script.script_len, DTN_MAX_EXEC_LEN))
		 return FALSE;
	return TRUE;
}

/**
 * Bundle priority specifier.
 *     COS_BULK      - lowest priority
 *     COS_NORMAL    - regular priority
 *     COS_EXPEDITED - important
 *     COS_RESERVED  - TBD
 */

bool_t
xdr_dtn_bundle_priority_t (XDR *xdrs, dtn_bundle_priority_t *objp)
{
	register int32_t *buf;

	 if (!xdr_enum (xdrs, (enum_t *) objp))
		 return FALSE;
	return TRUE;
}

/**
 * Bundle delivery option flags. Note that multiple options
 * may be selected for a given bundle.
 *     
 *     DOPTS_NONE           - no custody, etc
 *     DOPTS_CUSTODY        - custody xfer
 *     DOPTS_DELIVERY_RCPT  - end to end delivery (i.e. return receipt)
 *     DOPTS_RECEIVE_RCPT   - per hop arrival receipt
 *     DOPTS_FORWARD_RCPT   - per hop departure receipt
 *     DOPTS_CUSTODY_RCPT   - per custodian receipt
 *     DOPTS_DELETE_RCPT    - request deletion receipt
 */

bool_t
xdr_dtn_bundle_delivery_opts_t (XDR *xdrs, dtn_bundle_delivery_opts_t *objp)
{
	register int32_t *buf;

	 if (!xdr_enum (xdrs, (enum_t *) objp))
		 return FALSE;
	return TRUE;
}

/**
 * Bundle metadata.
 */

bool_t
xdr_dtn_bundle_spec_t (XDR *xdrs, dtn_bundle_spec_t *objp)
{
	register int32_t *buf;

	 if (!xdr_dtn_endpoint_id_t (xdrs, &objp->source))
		 return FALSE;
	 if (!xdr_dtn_endpoint_id_t (xdrs, &objp->dest))
		 return FALSE;
	 if (!xdr_dtn_endpoint_id_t (xdrs, &objp->replyto))
		 return FALSE;
	 if (!xdr_dtn_bundle_priority_t (xdrs, &objp->priority))
		 return FALSE;
	 if (!xdr_int (xdrs, &objp->dopts))
		 return FALSE;
	 if (!xdr_dtn_timeval_t (xdrs, &objp->expiration))
		 return FALSE;
	return TRUE;
}

/**
 * The payload of a bundle can be sent or received either in a file,
 * in which case the payload structure contains the filename, or in
 * memory where the struct has the actual data.
 *
 * If the location specifies that the payload is in a temp file, then
 * the daemon assumes ownership of the file and should have sufficient
 * permissions to move or rename it.
 * 
 * Note that there is a limit (DTN_MAX_BUNDLE_MEM) on the maximum size
 * bundle payload that can be sent or received in memory.
 *
 *     DTN_PAYLOAD_MEM		- copy contents from memory
 *     DTN_PAYLOAD_FILE	- file copy the contents of the file
 *     DTN_PAYLOAD_TEMP_FILE	- assume ownership of the file
 */

bool_t
xdr_dtn_bundle_payload_location_t (XDR *xdrs, dtn_bundle_payload_location_t *objp)
{
	register int32_t *buf;

	 if (!xdr_enum (xdrs, (enum_t *) objp))
		 return FALSE;
	return TRUE;
}

bool_t
xdr_dtn_bundle_payload_t (XDR *xdrs, dtn_bundle_payload_t *objp)
{
	register int32_t *buf;

	 if (!xdr_dtn_bundle_payload_location_t (xdrs, &objp->location))
		 return FALSE;
	switch (objp->location) {
	case DTN_PAYLOAD_FILE:
	case DTN_PAYLOAD_TEMP_FILE:
		 if (!xdr_bytes (xdrs, (char **)&objp->dtn_bundle_payload_t_u.filename.filename_val, (u_int *) &objp->dtn_bundle_payload_t_u.filename.filename_len, DTN_MAX_PATH_LEN))
			 return FALSE;
		break;
	case DTN_PAYLOAD_MEM:
		 if (!xdr_bytes (xdrs, (char **)&objp->dtn_bundle_payload_t_u.buf.buf_val, (u_int *) &objp->dtn_bundle_payload_t_u.buf.buf_len, DTN_MAX_BUNDLE_MEM))
			 return FALSE;
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

/**
 * Type definition for a bundle identifier as returned from dtn_send.
 */

bool_t
xdr_dtn_bundle_id_t (XDR *xdrs, dtn_bundle_id_t *objp)
{
	register int32_t *buf;

	 if (!xdr_dtn_endpoint_id_t (xdrs, &objp->source))
		 return FALSE;
	 if (!xdr_u_int (xdrs, &objp->creation_secs))
		 return FALSE;
	 if (!xdr_u_int (xdrs, &objp->creation_subsecs))
		 return FALSE;
	return TRUE;
}
