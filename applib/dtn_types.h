/*
 * Please do not edit this file.
 * It was generated using rpcgen.
 */

#ifndef _DTN_TYPES_H_RPCGEN
#define _DTN_TYPES_H_RPCGEN

#include <rpc/rpc.h>


#ifdef __cplusplus
extern "C" {
#endif

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

/**********************************
 * Constants.
 * (Note that we use #defines to get the comments as well)
 */
#define DTN_MAX_ENDPOINT_ID 256 /* max endpoint_id size (bytes) */
#define DTN_MAX_PATH_LEN PATH_MAX /* max path length */
#define DTN_MAX_EXEC_LEN ARG_MAX /* length of string passed to exec() */
#define DTN_MAX_AUTHDATA 1024 /* length of auth/security data*/
#define DTN_MAX_REGION_LEN 64 /* 64 chars "should" be long enough */
#define DTN_MAX_BUNDLE_MEM 50000 /* biggest in-memory bundle is ~50K*/
#define DTN_MAX_BLOCK_LEN 1024 /* length of block data (currently 1K) */
#define DTN_MAX_BLOCKS 256 /* number of blocks in bundle */

/**
 * Specification of a dtn endpoint id, i.e. a URI, implemented as a
 * fixed-length char buffer. Note that for efficiency reasons, this
 * fixed length is relatively small (256 bytes). 
 * 
 * The alternative is to use the string XDR type but then all endpoint
 * ids would require malloc / free which is more prone to leaks / bugs.
 */

struct dtn_endpoint_id_t {
	char uri[DTN_MAX_ENDPOINT_ID];
};
typedef struct dtn_endpoint_id_t dtn_endpoint_id_t;

/**
 * A registration cookie.
 */

typedef u_int dtn_reg_id_t;

/**
 * DTN timeouts are specified in seconds.
 */

typedef u_int dtn_timeval_t;

/**
 * An infinite wait is a timeout of -1.
 */
#define DTN_TIMEOUT_INF ((dtn_timeval_t)-1)

#include "dtn-config.h"

#ifdef HAVE_XDR_U_QUAD_T
 #define u_hyper u_quad_t
 #define xdr_u_hyper_t xdr_u_quad_t
#elif defined(HAVE_XDR_U_INT64_T)
 #define u_hyper u_int64_t
 #define xdr_u_hyper_t xdr_u_int64_t
#endif

struct dtn_timestamp_t {
	u_hyper secs;
	u_hyper seqno;
};
typedef struct dtn_timestamp_t dtn_timestamp_t;

/**
 * Specification of a service tag used in building a local endpoint
 * identifier.
 * 
 * Note that the application cannot (in general) expect to be able to
 * use the full DTN_MAX_ENDPOINT_ID, as there is a chance of overflow
 * when the daemon concats the tag with its own local endpoint id.
 */

struct dtn_service_tag_t {
	char tag[DTN_MAX_ENDPOINT_ID];
};
typedef struct dtn_service_tag_t dtn_service_tag_t;

/**
 * Value for an unspecified registration cookie (i.e. indication that
 * the daemon should allocate a new unique id).
 */
#define DTN_REGID_NONE 0

/**
 * Registration flags are a bitmask of the following:

 * Delivery failure actions (exactly one must be selected):
 *     DTN_REG_DROP   - drop bundle if registration not active
 *     DTN_REG_DEFER  - spool bundle for later retrieval
 *     DTN_REG_EXEC   - exec program on bundle arrival
 *
 * Session flags:
 *     DTN_SESSION_CUSTODY   - app assumes custody for the session
 *     DTN_SESSION_PUBLISH   - creates a publication point
 *     DTN_SESSION_SUBSCRIBE - create subscription for the session
 */

enum dtn_reg_flags_t {
	DTN_REG_DROP = 1,
	DTN_REG_DEFER = 2,
	DTN_REG_EXEC = 3,
	DTN_SESSION_CUSTODY = 4,
	DTN_SESSION_PUBLISH = 8,
	DTN_SESSION_SUBSCRIBE = 16,
};
typedef enum dtn_reg_flags_t dtn_reg_flags_t;

/**
 * Registration state.
 */

struct dtn_reg_info_t {
	dtn_endpoint_id_t endpoint;
	dtn_reg_id_t regid;
	u_int flags;
	dtn_timeval_t expiration;
	bool_t init_passive;
	struct {
		u_int script_len;
		char *script_val;
	} script;
};
typedef struct dtn_reg_info_t dtn_reg_info_t;

/**
 * Bundle priority specifier.
 *     COS_BULK      - lowest priority
 *     COS_NORMAL    - regular priority
 *     COS_EXPEDITED - important
 *     COS_RESERVED  - TBD
 */

enum dtn_bundle_priority_t {
	COS_BULK = 0,
	COS_NORMAL = 1,
	COS_EXPEDITED = 2,
	COS_RESERVED = 3,
};
typedef enum dtn_bundle_priority_t dtn_bundle_priority_t;

/**
 * Bundle delivery option flags. Note that multiple options may be
 * selected for a given bundle.
 *     
 *     DOPTS_NONE            - no custody, etc
 *     DOPTS_CUSTODY         - custody xfer
 *     DOPTS_DELIVERY_RCPT   - end to end delivery (i.e. return receipt)
 *     DOPTS_RECEIVE_RCPT    - per hop arrival receipt
 *     DOPTS_FORWARD_RCPT    - per hop departure receipt
 *     DOPTS_CUSTODY_RCPT    - per custodian receipt
 *     DOPTS_DELETE_RCPT     - request deletion receipt
 *     DOPTS_SINGLETON_DEST  - destination is a singleton
 *     DOPTS_MULTINODE_DEST  - destination is not a singleton
 *     DOPTS_DO_NOT_FRAGMENT - set the do not fragment bit
 */

enum dtn_bundle_delivery_opts_t {
	DOPTS_NONE = 0,
	DOPTS_CUSTODY = 1,
	DOPTS_DELIVERY_RCPT = 2,
	DOPTS_RECEIVE_RCPT = 4,
	DOPTS_FORWARD_RCPT = 8,
	DOPTS_CUSTODY_RCPT = 16,
	DOPTS_DELETE_RCPT = 32,
	DOPTS_SINGLETON_DEST = 64,
	DOPTS_MULTINODE_DEST = 128,
	DOPTS_DO_NOT_FRAGMENT = 256,
};
typedef enum dtn_bundle_delivery_opts_t dtn_bundle_delivery_opts_t;

/**
 * Extension block flags. Note that multiple flags may be selected
 * for a given block.
 *
 *     BLOCK_FLAG_NONE          - no flags
 *     BLOCK_FLAG_REPLICATE     - block must be replicated in every fragment
 *     BLOCK_FLAG_REPORT        - transmit report if block can't be processed
 *     BLOCK_FLAG_DELETE_BUNDLE - delete bundle if block can't be processed
 *     BLOCK_FLAG_LAST          - last block
 *     BLOCK_FLAG_DISCARD_BLOCK - discard block if it can't be processed
 *     BLOCK_FLAG_UNPROCESSED   - block was forwarded without being processed
 */

enum dtn_extension_block_flags_t {
	BLOCK_FLAG_NONE = 0,
	BLOCK_FLAG_REPLICATE = 1,
	BLOCK_FLAG_REPORT = 2,
	BLOCK_FLAG_DELETE_BUNDLE = 4,
	BLOCK_FLAG_LAST = 8,
	BLOCK_FLAG_DISCARD_BLOCK = 16,
	BLOCK_FLAG_UNPROCESSED = 32,
};
typedef enum dtn_extension_block_flags_t dtn_extension_block_flags_t;

/**
 * Extension block.
 */

struct dtn_extension_block_t {
	u_int type;
	u_int flags;
	struct {
		u_int data_len;
		char *data_val;
	} data;
};
typedef struct dtn_extension_block_t dtn_extension_block_t;

/**
 * A Sequence ID is a vector of (EID, counter) values in the following
 * text format:
 *
 *    < (EID1 counter1) (EID2 counter2) ... >
 */

struct dtn_sequence_id_t {
	struct {
		u_int data_len;
		char *data_val;
	} data;
};
typedef struct dtn_sequence_id_t dtn_sequence_id_t;

/**
 * Bundle metadata. The delivery_regid is ignored when sending
 * bundles, but is filled in by the daemon with the registration
 * id where the bundle was received.
 */

struct dtn_bundle_spec_t {
	dtn_endpoint_id_t source;
	dtn_endpoint_id_t dest;
	dtn_endpoint_id_t replyto;
	dtn_bundle_priority_t priority;
	int dopts;
	dtn_timeval_t expiration;
	dtn_timestamp_t creation_ts;
	dtn_reg_id_t delivery_regid;
	dtn_sequence_id_t sequence_id;
	dtn_sequence_id_t obsoletes_id;
	struct {
		u_int blocks_len;
		dtn_extension_block_t *blocks_val;
	} blocks;
	struct {
		u_int metadata_len;
		dtn_extension_block_t *metadata_val;
	} metadata;
};
typedef struct dtn_bundle_spec_t dtn_bundle_spec_t;

/**
 * Type definition for a unique bundle identifier. Returned from dtn_send
 * after the daemon has assigned the creation_secs and creation_subsecs,
 * in which case orig_length and frag_offset are always zero, and also in
 * status report data in which case they may be set if the bundle is
 * fragmented.
 */

struct dtn_bundle_id_t {
	dtn_endpoint_id_t source;
	dtn_timestamp_t creation_ts;
	u_int frag_offset;
	u_int orig_length;
};
typedef struct dtn_bundle_id_t dtn_bundle_id_t;
/**
 * Bundle Status Report "Reason Code" flags
 */

enum dtn_status_report_reason_t {
	REASON_NO_ADDTL_INFO = 0x00,
	REASON_LIFETIME_EXPIRED = 0x01,
	REASON_FORWARDED_UNIDIR_LINK = 0x02,
	REASON_TRANSMISSION_CANCELLED = 0x03,
	REASON_DEPLETED_STORAGE = 0x04,
	REASON_ENDPOINT_ID_UNINTELLIGIBLE = 0x05,
	REASON_NO_ROUTE_TO_DEST = 0x06,
	REASON_NO_TIMELY_CONTACT = 0x07,
	REASON_BLOCK_UNINTELLIGIBLE = 0x08,
};
typedef enum dtn_status_report_reason_t dtn_status_report_reason_t;
/**
 * Bundle Status Report status flags that indicate which timestamps in
 * the status report structure are valid.
 */

enum dtn_status_report_flags_t {
	STATUS_RECEIVED = 0x01,
	STATUS_CUSTODY_ACCEPTED = 0x02,
	STATUS_FORWARDED = 0x04,
	STATUS_DELIVERED = 0x08,
	STATUS_DELETED = 0x10,
	STATUS_ACKED_BY_APP = 0x20,
};
typedef enum dtn_status_report_flags_t dtn_status_report_flags_t;

/**
 * Type definition for a bundle status report.
 */

struct dtn_bundle_status_report_t {
	dtn_bundle_id_t bundle_id;
	dtn_status_report_reason_t reason;
	dtn_status_report_flags_t flags;
	dtn_timestamp_t receipt_ts;
	dtn_timestamp_t custody_ts;
	dtn_timestamp_t forwarding_ts;
	dtn_timestamp_t delivery_ts;
	dtn_timestamp_t deletion_ts;
	dtn_timestamp_t ack_by_app_ts;
};
typedef struct dtn_bundle_status_report_t dtn_bundle_status_report_t;

/**
 * The payload of a bundle can be sent or received either in a file,
 * in which case the payload structure contains the filename, or in
 * memory where the struct contains the data in-band, in the 'buf'
 * field.
 *
 * When sending a bundle, if the location specifies that the payload
 * is in a temp file, then the daemon assumes ownership of the file
 * and should have sufficient permissions to move or rename it.
 *
 * When receiving a bundle that is a status report, then the
 * status_report pointer will be non-NULL and will point to a
 * dtn_bundle_status_report_t structure which contains the parsed fields
 * of the status report.
 *
 *     DTN_PAYLOAD_MEM         - payload contents in memory
 *     DTN_PAYLOAD_FILE        - payload contents in file
 *     DTN_PAYLOAD_TEMP_FILE   - in file, assume ownership (send only)
 */

enum dtn_bundle_payload_location_t {
	DTN_PAYLOAD_FILE = 0,
	DTN_PAYLOAD_MEM = 1,
	DTN_PAYLOAD_TEMP_FILE = 2,
};
typedef enum dtn_bundle_payload_location_t dtn_bundle_payload_location_t;

struct dtn_bundle_payload_t {
	dtn_bundle_payload_location_t location;
	struct {
		u_int filename_len;
		char *filename_val;
	} filename;
	struct {
		u_int buf_len;
		char *buf_val;
	} buf;
	dtn_bundle_status_report_t *status_report;
};
typedef struct dtn_bundle_payload_t dtn_bundle_payload_t;

/* the xdr functions */

#if defined(__STDC__) || defined(__cplusplus)
extern  bool_t xdr_dtn_endpoint_id_t (XDR *, dtn_endpoint_id_t*);
extern  bool_t xdr_dtn_reg_id_t (XDR *, dtn_reg_id_t*);
extern  bool_t xdr_dtn_timeval_t (XDR *, dtn_timeval_t*);
extern  bool_t xdr_dtn_timestamp_t (XDR *, dtn_timestamp_t*);
extern  bool_t xdr_dtn_service_tag_t (XDR *, dtn_service_tag_t*);
extern  bool_t xdr_dtn_reg_flags_t (XDR *, dtn_reg_flags_t*);
extern  bool_t xdr_dtn_reg_info_t (XDR *, dtn_reg_info_t*);
extern  bool_t xdr_dtn_bundle_priority_t (XDR *, dtn_bundle_priority_t*);
extern  bool_t xdr_dtn_bundle_delivery_opts_t (XDR *, dtn_bundle_delivery_opts_t*);
extern  bool_t xdr_dtn_extension_block_flags_t (XDR *, dtn_extension_block_flags_t*);
extern  bool_t xdr_dtn_extension_block_t (XDR *, dtn_extension_block_t*);
extern  bool_t xdr_dtn_sequence_id_t (XDR *, dtn_sequence_id_t*);
extern  bool_t xdr_dtn_bundle_spec_t (XDR *, dtn_bundle_spec_t*);
extern  bool_t xdr_dtn_bundle_id_t (XDR *, dtn_bundle_id_t*);
extern  bool_t xdr_dtn_status_report_reason_t (XDR *, dtn_status_report_reason_t*);
extern  bool_t xdr_dtn_status_report_flags_t (XDR *, dtn_status_report_flags_t*);
extern  bool_t xdr_dtn_bundle_status_report_t (XDR *, dtn_bundle_status_report_t*);
extern  bool_t xdr_dtn_bundle_payload_location_t (XDR *, dtn_bundle_payload_location_t*);
extern  bool_t xdr_dtn_bundle_payload_t (XDR *, dtn_bundle_payload_t*);

#else /* K&R C */
extern bool_t xdr_dtn_endpoint_id_t ();
extern bool_t xdr_dtn_reg_id_t ();
extern bool_t xdr_dtn_timeval_t ();
extern bool_t xdr_dtn_timestamp_t ();
extern bool_t xdr_dtn_service_tag_t ();
extern bool_t xdr_dtn_reg_flags_t ();
extern bool_t xdr_dtn_reg_info_t ();
extern bool_t xdr_dtn_bundle_priority_t ();
extern bool_t xdr_dtn_bundle_delivery_opts_t ();
extern bool_t xdr_dtn_extension_block_flags_t ();
extern bool_t xdr_dtn_extension_block_t ();
extern bool_t xdr_dtn_sequence_id_t ();
extern bool_t xdr_dtn_bundle_spec_t ();
extern bool_t xdr_dtn_bundle_id_t ();
extern bool_t xdr_dtn_status_report_reason_t ();
extern bool_t xdr_dtn_status_report_flags_t ();
extern bool_t xdr_dtn_bundle_status_report_t ();
extern bool_t xdr_dtn_bundle_payload_location_t ();
extern bool_t xdr_dtn_bundle_payload_t ();

#endif /* K&R C */

#ifdef __cplusplus
}
#endif

#endif /* !_DTN_TYPES_H_RPCGEN */
