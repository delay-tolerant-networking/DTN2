/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
 * By downloading, copying, installing or using the software you agree
 * to this license. If you do not agree to this license, do not
 * download, install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2004 Intel Corporation. All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 	Redistributions of source code must retain the above copyright
 * 	notice, this list of conditions and the following disclaimer.
 * 
 * 	Redistributions in binary form must reproduce the above
 * 	copyright notice, this list of conditions and the following
 * 	disclaimer in the documentation and/or other materials
 * 	provided with the distribution.
 * 
 * 	Neither the name of the Intel Corporation nor the names of its
 * 	contributors may be used to endorse or promote products
 * 	derived from this software without specific prior written
 * 	permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * INTEL OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>

#include "dtn_api.h"
#include "dtn_ipc.h"

/**
 * Open a new connection to the router.
 * Returns the new handle on success, 0 on error.
 */
dtn_handle_t
dtn_open()
{
    dtnipc_handle_t* handle;

    handle = (dtnipc_handle_t *) malloc(sizeof(struct dtnipc_handle));
    if (!handle)
        return 0;
    
    if (dtnipc_open(handle) != 0) {
        free(handle);
        return 0;
    }

    xdr_setpos(&handle->xdr_encode, 0);
    xdr_setpos(&handle->xdr_decode, 0);

    return (dtn_handle_t)handle;
}

/**
 * Close an open dtn handle.
 */
int
dtn_close(dtn_handle_t handle)
{
    dtnipc_close((dtnipc_handle_t *)handle);
    free(handle);
    return -1;
}

/**
 * Get the error associated with the given handle.
 */
int
dtn_errno(dtn_handle_t handle)
{
    return ((dtnipc_handle_t*)handle)->err;
}

/**
 * Get a string value associated with the dtn error code.
 */
char*
dtn_strerror(int err)
{
    switch(err) {
    case DTN_SUCCESS: 	return "success";
    case DTN_EINVAL: 	return "invalid argument";
    case DTN_EXDR: 	return "error in xdr routines";
    case DTN_ECOMM: 	return "error in ipc communication";
    case DTN_ECONNECT: 	return "error connecting to server";
    case DTN_ETIMEOUT: 	return "operation timed out";
    case DTN_ESIZE: 	return "payload too large";
    case DTN_ENOTFOUND: return "not found";
    case DTN_EINTERNAL: return "internal error";
    }

    return "(unknown error)";
}

/**
 * Information request function.
 */
int
dtn_get_info(dtn_handle_t h,
             dtn_info_request_t request,
             dtn_info_response_t* response)
{
    int status;
    dtnipc_handle_t* handle = (dtnipc_handle_t*)h;
    XDR* xdr_encode = &handle->xdr_encode;
    XDR* xdr_decode = &handle->xdr_decode;
    
    // pack the request
    if (!xdr_dtn_info_request_t(xdr_encode, &request)) {
        handle->err = DTN_EXDR;
        return -1;
    }

    // send the message
    if (dtnipc_send(handle, DTN_GETINFO) != 0) {
        return -1;
    }

    // get the reply
    if (dtnipc_recv(handle, &status) < 0) {
        return -1;
    }

    // handle server-side errors
    if (status != DTN_SUCCESS) {
        handle->err = status;
        return -1;
    }

    // unpack the response
    memset(response, 0, sizeof(*response));
    if (!xdr_dtn_info_response_t(xdr_decode, response)) {
        handle->err = DTN_EXDR;
        return -1;
    }

    return 0;
}

    
/**
 * Create or modify a dtn registration.
 */
int
dtn_register(dtn_handle_t h,
             dtn_reg_info_t *reginfo,
             dtn_reg_id_t* newregid)
{
    int status;
    dtnipc_handle_t* handle = (dtnipc_handle_t*)h;
    XDR* xdr_encode = &handle->xdr_encode;
    XDR* xdr_decode = &handle->xdr_decode;
    
    // pack the request
    if (!xdr_dtn_reg_info_t(xdr_encode, reginfo)) {
        handle->err = DTN_EXDR;
        return -1;
    }

    // send the message
    if (dtnipc_send(handle, DTN_REGISTER) != 0) {
        return -1;
    }

    // get the reply
    if (dtnipc_recv(handle, &status) < 0) {
        return -1;
    }

    // handle server-side errors
    if (status != DTN_SUCCESS) {
        handle->err = status;
        return -1;
    }

    // unpack the response
    if (!xdr_dtn_reg_id_t(xdr_decode, newregid)) {
        handle->err = DTN_EXDR;
        return -1;
    }

    return 0;
}

/**
 * Associate a registration id with the current ipc channel. This must
 * be called before calling dtn_recv to inform the daemon of which
 * registrations the application is interested in.
 */
int
dtn_bind(dtn_handle_t h, dtn_reg_id_t regid, dtn_tuple_t* endpoint)
{
    int status;
    dtnipc_handle_t* handle = (dtnipc_handle_t*)h;
    XDR* xdr_encode = &handle->xdr_encode;
    
    // pack the request
    if (!xdr_dtn_reg_id_t(xdr_encode, &regid) ||
        !xdr_dtn_tuple_t(xdr_encode, endpoint))
    {
        handle->err = DTN_EXDR;
        return -1;
    }

    // send the message
    if (dtnipc_send(handle, DTN_BIND) < 0) {
        return -1;
    }

    // get the reply
    if (dtnipc_recv(handle, &status) < 0) {
        return -1;
    }

    // handle server-side errors
    if (status != DTN_SUCCESS) {
        handle->err = status;
        return -1;
    }

    return 0;
}

/**
 * Send a bundle either from memory or from a file.
 */
int
dtn_send(dtn_handle_t h,
         dtn_bundle_spec_t* spec,
         dtn_bundle_payload_t* payload)
{
    int status;
    dtnipc_handle_t* handle = (dtnipc_handle_t*)h;
    XDR* xdr_encode = &handle->xdr_encode;

    // pack the arguments
    if ((!xdr_dtn_bundle_spec_t(xdr_encode, spec)) ||
        (!xdr_dtn_bundle_payload_t(xdr_encode, payload))) {
        handle->err = DTN_EXDR;
        return -1;
    }

    // send the message
    if (dtnipc_send(handle, DTN_SEND) < 0) {
        return -1;
    }

    // wait for a response
    if (dtnipc_recv(handle, &status) < 0) {
        return -1;
    }

    // handle server-side errors
    if (status != DTN_SUCCESS) {
        handle->err = status;
        return -1;
    }

    return 0;
}

/**
 * Blocking receive for a bundle
 */
int
dtn_recv(dtn_handle_t h,
         dtn_bundle_spec_t* spec,
         dtn_bundle_payload_location_t location,
         dtn_bundle_payload_t* payload,
         dtn_timeval_t timeout)
{
    int ret;
    int status;
    dtnipc_handle_t* handle = (dtnipc_handle_t*)h;
    XDR* xdr_encode = &handle->xdr_encode;
    XDR* xdr_decode = &handle->xdr_decode;

    // zero out the spec and payload structures
    memset(spec, 0, sizeof(*spec));
    memset(payload, 0, sizeof(*payload));

    // pack the arguments
    if ((!xdr_dtn_bundle_payload_location_t(xdr_encode, &location)) ||
        (!xdr_dtn_timeval_t(xdr_encode, &timeout)))
    {
        handle->err = DTN_EXDR;
        return -1;
    }

    // send the message
    if (dtnipc_send(handle, DTN_RECV) < 0) {
        return -1;
    }

    // wait for a response
    if ((ret = dtnipc_recv(handle, &status)) < 0) {
        return -1;
    }

    // handle server-side errors
    if (status != DTN_SUCCESS) {
        handle->err = status;
        return -1;
    }

    // unpack the bundle
    if (!xdr_dtn_bundle_spec_t(xdr_decode, spec) ||
        !xdr_dtn_bundle_payload_t(xdr_decode, payload))
    {
        handle->err = DTN_EXDR;
        return -1;
    }
    
    return 0;
}

/*************************************************************
 *
 *                     Utility Functions
 *
 *************************************************************/

/*
 * Copy the value of one tuple to another.
 *
 * Note that this will _not_ call malloc for the admin part so the
 * underlying memory will be shared. XXX/demmer this is bad, we need a
 * dtn_free_tuple call...
 *
 */
void
dtn_copy_tuple(dtn_tuple_t* dst, dtn_tuple_t* src)
{
    memcpy(dst->region, src->region, DTN_MAX_REGION_LEN);
    dst->admin.admin_val = src->admin.admin_val;
    dst->admin.admin_len = src->admin.admin_len;
}
/*
 * Sets the value of the tuple to the given region, admin, and demux
 * strings.
 *
 * Note that the region and admin strings may be set to the special
 * values of DTN_REGION_LOCAL and DTN_ADMIN_LOCAL in which case the
 * corresponding length arguments are ignored.
 *
 * Returns: 0 on success, DTN_EINVAL if the arguments are invalid.
 */
int dtn_set_tuple(dtn_tuple_t* tuple,
                  char* region, size_t region_len,
                  char* admin, size_t admin_len)
{
    return 0;
}


/*
 * Parses the given string according to the generic schema
 * bundles://<region>/<admin> and assigns it to the given tuple.
 *
 * Returns: 0 on success, DTN_EINVAL if the given string is not a
 * valid tuple.
 */
int
dtn_parse_tuple_string(dtn_tuple_t* tuple, char* str)
{
    char *s, *end;
    size_t len;

    s = strchr(str, ':');
    if (!s) {
        return DTN_EINVAL;
    }
    
    s++;
    if ((s[0] != '/') || s[1] != '/') {
        return DTN_EINVAL;
    }
    s += 2;

    // s now points to the start of the region string, so search to
    // the next '/' character
    end = strchr(s, '/');
    if (!end) {
        return DTN_EINVAL;
    }

    // copy the region string
    len = end - s;
    memcpy(&tuple->region, s, len);
    tuple->region[len] = '\0';

    // the admin part is everything past the '/'
    s = end + 1;
    len = strlen(s);
    tuple->admin.admin_len = len + 1;

    // XXX/demmer memory leak here?
    tuple->admin.admin_val = (char*)malloc(len + 1);
    memcpy(tuple->admin.admin_val, s, len);
    tuple->admin.admin_val[len] = '\0';

    return 0;
}

/*
 * Queries the router for the default local tuple and appends the
 * given endpoint string to the url, assigning the result to the given
 * tuple pointer.
 *
 * Returns: 0 on success, XXX/demmer fill in others
 */
int
dtn_build_local_tuple(dtn_handle_t handle,
                      dtn_tuple_t* tuple, char* endpoint)
{
    dtn_tuple_t* local_tuple;
    dtn_info_response_t info;
    int appendslash = 0;
    
    int ret = dtn_get_info(handle, DTN_INFOREQ_INTERFACES, &info);
    if (ret != 0)
        return ret;

    local_tuple = &info.dtn_info_response_t_u.interfaces.local_tuple;
    strncpy(tuple->region, local_tuple->region, DTN_MAX_REGION_LEN);
    
    tuple->admin.admin_len = local_tuple->admin.admin_len +
                             strlen(endpoint) + 1;

    if (local_tuple->admin.admin_val[local_tuple->admin.admin_len-1] != '/' &&
        endpoint[0] != '/')
    {
        appendslash = 1;
        tuple->admin.admin_len++;
    }

    // XXX/demmer memory leak?
    tuple->admin.admin_val = (char*)malloc(tuple->admin.admin_len);

    snprintf(tuple->admin.admin_val, tuple->admin.admin_len, "%.*s%s%s",
             (int)local_tuple->admin.admin_len, local_tuple->admin.admin_val,
             appendslash ? "/" : "", endpoint);
    
    return 0;
}


/*
 * Sets the value of the given payload structure to either a memory
 * buffer or a file location.
 *
 * Returns: 0 on success, DTN_ESIZE if the memory location is
 * selected and the payload is too big.
 */
int
dtn_set_payload(dtn_bundle_payload_t* payload,
                dtn_bundle_payload_location_t location,
                char* val, int len)
{
    payload->location = location;

    if (location == DTN_PAYLOAD_MEM && len > DTN_MAX_BUNDLE_MEM) {
        return DTN_ESIZE;
    }
    
    switch (location) {
    case DTN_PAYLOAD_MEM:
        payload->dtn_bundle_payload_t_u.buf.buf_val = val;
        payload->dtn_bundle_payload_t_u.buf.buf_len = len;
        break;
    case DTN_PAYLOAD_FILE:
        payload->dtn_bundle_payload_t_u.filename.filename_val = val;
        payload->dtn_bundle_payload_t_u.filename.filename_len = len;
        break;
    }

    return 0;
}

