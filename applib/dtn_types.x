/*
 * IMPORTANT:  READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
 * By downloading, copying, installing or using the software you agree to this
 * license.  If you do not agree to this license, do not download, install,
 * copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2004 Intel Corporation. All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met: 
 * 
 * 	Redistributions of source code must retain the above copyright notice,
 * 	this list of conditions and the following disclaimer. 
 * 
 * 	Redistributions in binary form must reproduce the above copyright
 * 	notice, this list of conditions and the following disclaimer in the
 * 	documentation and/or other materials provided with the distribution. 
 * 
 * 	Neither the name of the Intel Corporation nor the names of its
 * 	contributors may be used to endorse or promote products derived from
 * 	this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR ITS  CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * Constants for the dtn api layer.
 */
const DTN_MAX_TUPLE = 65536;		/* max tuple size (bytes) */
const DTN_MAX_PATH_LEN = PATH_MAX;	/* max path length */
const DTN_MAX_EXEC_LEN = ARG_MAX;	/* length of string passed to exec() */
const DTN_MAX_AUTHDATA = 1024;		/* length of auth/security data*/
const DTN_MAX_REGION_LEN = 64;		/* 64 chars "should" be long enough */
const DTN_MAX_BUNDLE_MEM = 1048576; 	/* max in-memory bundle size (bytes) */

typedef	int DTN_STATUS;		/* return codes */

/*
 * Bundle status codes
 *
 * These are #defines because the rpcgen tool isn't smart enough to
 * notice that (128+1) could possibly be considered the constant 129,
 * and its 'const' construct requires real constants :(
 */

%#define DTN_BASE 128
%#define DTN_SUCCESS	(DTN_BASE+0)	/* ok */
%#define DTN_TOOBIG	(DTN_BASE+1)	/* payload too large */
%#define DTN_NOTFOUND	(DTN_BASE+2)	/* not found: (eg:file) */
%#define DTN_PERMS	(DTN_BASE+3)	/* permissions problem of some kind */
%#define DTN_EXISTS	(DTN_BASE+4)	/* bundle already being sent? */
%#define DTN_TPROB	(DTN_BASE+5)	/* tuple problem (too long) */
%#define DTN_NODATA	(DTN_BASE+6)	/* data missing/too short */
%#define DTN_DISPERR	(DTN_BASE+7)	/* error delivering to local app */
%#define DTN_SPECERR	(DTN_BASE+8)	/* bad bundle spec */
%#define DTN_SERVERR	(DTN_BASE+9)	/* server error (eg failed syscall) */
%#define DTN_SMEMERR	(DTN_BASE+10)	/* server memory error */
%#define DTN_RTPROB	(DTN_BASE+11)	/* forwarding error */
%#define DTN_BADCOOKIE	(DTN_BASE+12)	/* bad cookie provided by app */
%#define DTN_ISOPEN	(DTN_BASE+13)	/* open on opened handle */
%#define DTN_ISCLOSED	(DTN_BASE+14)	/* close on closed handle */
%#define DTN_AGENTCOMM	(DTN_BASE+15)	/* misc error with app/agent comm */
%#define DTN_INVAL	(DTN_BASE+16)	/* invalid value in an argument */
%#define DTN_LIBERR	(DTN_BASE+17)	/* misc app side error */

/**
 * Specification of a dtn tuple.
 */
struct dtn_tuple_t {
    opaque	data<DTN_MAX_TUPLE>;	/* space for a tuple */
    uint16_t	admin_offset;		/* 1st byte of admin area in name */
};

/**
 * A registration cookie.
 */
typedef uint32_t dtn_reg_cookie_t;

/**
 * Value for an unspecified registration cookie (i.e. indication that
 * the daemon should allocate something unique.
 */
const DTN_REG_COOKIE_NONE = 0;

/**
 * Registration actions
 */
enum dtn_reg_action_t {
    DTN_REG_ABORT  = 1,	/* demux:can't reach me? abort */
    DTN_REG_DEFER  = 2,	/* demux:try me later */
    DTN_REG_EXEC   = 3,	/* demux:can't reach me? exec this pgm */
    DTN_REG_CANCEL = 4	/* cancel a prior registration */
};

/**
 * Bundle priority specifier.
 */
enum dtn_bundle_priority_t {
    COS_BULK = 0,			/* lowest priority */
    COS_NORMAL = 1,			/* regular priority */
    COS_EXPEDITED = 2,			/* important */
    COS_RESERVED = 3			/* TBD */
};

/**
 * Bundle delivery options:
 *      custody xfer
 *      traceroute-like options
 *      return receipt
 */
enum dtn_bundle_delivery_opts_t {
    COS_NONE = 0,			/* no custody, etc */
    COS_CUSTODY = 1,			/* custody xfer */
    COS_RET_RCPT = 2,			/* return receipt */
    COS_DELIV_REC_FORW = 4,		/* request forwarder info */
    COS_DELIV_REC_CUST = 8,		/* request custody xfer info */
    COS_OVERWRITE = 16			/* request queue overwrite option */
};

/**
 * Bundle authentication data.
 */
struct dtn_bundle_auth_t {
    opaque	blob<DTN_MAX_AUTHDATA>;	/* TBD */
};


/*
 * Most of the header info in a bundle, used by send routines
 */
struct dtn_bundle_spec_t {
    dtn_bundle_priority_t	priority;
    dtn_bundle_delivery_opts_t	dopts;
    dtn_tuple_t			source;
    dtn_tuple_t			dest;
    dtn_tuple_t			replyto;
    int32_t			expiration;
};

/*
 * The payload of a bundle can be sent or received either in a file,
 * in which case the payload structure contains the filename, or in
 * memory where the struct has the actual data.
 */
enum dtn_bundle_payload_location_t {
    DTN_PAYLOAD_FILE,	/* bundle in file */
    DTN_PAYLOAD_MEM	/* bundle in memory */
};

union dtn_bundle_payload_t switch(dtn_bundle_payload_location_t location)
{
 case DTN_PAYLOAD_FILE:
     opaque	filename<DTN_MAX_PATH_LEN>;
 case DTN_PAYLOAD_MEM:
     opaque	buf<DTN_MAX_BUNDLE_MEM>;
};

/*
 * Convenience macros to hide the unionizing.
 */
%#define DTN_PAYLOAD_FILENAME_VAL payload.DTN_PAYLOAD_u.filename.filename_val
%#define DTN_PAYLOAD_FILENAME_LEN payload.DTN_PAYLOAD_u.filename.filename_len
%#define DTN_PAYLOAD_BUF_VAL      payload.DTN_PAYLOAD_u.buf.buf_val
%#define DTN_PAYLOAD_BUF_LEN      payload.DTN_PAYLOAD_u.buf.buf_len
    
