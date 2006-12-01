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

//----------------------------------------------------------------------
int 
dtn_open(dtn_handle_t* h)
{
    dtnipc_handle_t* handle;

    handle = (dtnipc_handle_t *) malloc(sizeof(struct dtnipc_handle));
    if (!handle) {
        *h = NULL;
        return DTN_EINTERNAL;
    }
    
    if (dtnipc_open(handle) != 0) {
        int ret = handle->err;
        free(handle);
        *h = NULL;
        return ret;
    }

    xdr_setpos(&handle->xdr_encode, 0);
    xdr_setpos(&handle->xdr_decode, 0);

    *h = (dtn_handle_t)handle;
    return DTN_SUCCESS;
}

//----------------------------------------------------------------------
int
dtn_close(dtn_handle_t handle)
{
    dtnipc_close((dtnipc_handle_t *)handle);
    free(handle);
    return DTN_SUCCESS;
}

//----------------------------------------------------------------------
int
dtn_errno(dtn_handle_t handle)
{
    return ((dtnipc_handle_t*)handle)->err;
}

//----------------------------------------------------------------------
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
    case DTN_EINPOLL:   return "illegal operation called after dtn_poll";
    case DTN_EBUSY:     return "registration already in use";
    case DTN_EMSGTYPE:  return "unknown ipc message type";
    case DTN_ENOSPACE:	return "no storage space";
    case -1:            return "(invalid error code -1)";
    }

    // there's a small race condition here in case there are two
    // simultaneous calls that will clobber the same buffer, but this
    // should be rare and the worst that happens is that the output
    // string is garbled
    static char buf[128];
    snprintf(buf, sizeof(buf), "(unknown error %d)", err);
    return buf;
}

//----------------------------------------------------------------------
int
dtn_build_local_eid(dtn_handle_t h,
                    dtn_endpoint_id_t* local_eid,
                    const char* tag)
{
    int status;
    dtnipc_handle_t* handle = (dtnipc_handle_t*)h;
    XDR* xdr_encode = &handle->xdr_encode;
    XDR* xdr_decode = &handle->xdr_decode;
    struct dtn_service_tag_t service_tag;

    // check if the handle is in the middle of poll
    if (handle->in_poll) {
        handle->err = DTN_EINPOLL;
        return -1;
    }
    
    // validate the tag length
    size_t len = strlen(tag) + 1;
    if (len > DTN_MAX_ENDPOINT_ID) {
        handle->err = DTN_EINVAL;
        return -1;
    }

    // pack the request
    memset(&service_tag, 0, sizeof(service_tag));
    memcpy(&service_tag.tag[0], tag, len);
    if (!xdr_dtn_service_tag_t(xdr_encode, &service_tag)) {
        handle->err = DTN_EXDR;
        return -1;
    }

    // send the message
    if (dtnipc_send(handle, DTN_LOCAL_EID) != 0) {
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
    memset(local_eid, 0, sizeof(*local_eid));
    if (!xdr_dtn_endpoint_id_t(xdr_decode, local_eid)) {
        handle->err = DTN_EXDR;
        return -1;
    }
    
    return 0;
}

//----------------------------------------------------------------------
int
dtn_register(dtn_handle_t h,
             dtn_reg_info_t *reginfo,
             dtn_reg_id_t* newregid)
{
    int status;
    dtnipc_handle_t* handle = (dtnipc_handle_t*)h;
    XDR* xdr_encode = &handle->xdr_encode;
    XDR* xdr_decode = &handle->xdr_decode;
    
    // check if the handle is in the middle of poll
    if (handle->in_poll) {
        handle->err = DTN_EINPOLL;
        return -1;
    }
    
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

//----------------------------------------------------------------------
int
dtn_unregister(dtn_handle_t h, dtn_reg_id_t regid)
{
    int status;
    dtnipc_handle_t* handle = (dtnipc_handle_t*)h;

    XDR* xdr_encode = &handle->xdr_encode;

    // check if the handle is in the middle of poll
    if (handle->in_poll) {
        handle->err = DTN_EINPOLL;
        return -1;
    }
    
    // pack the request
    if (!xdr_dtn_reg_id_t(xdr_encode, &regid)) {
        handle->err = DTN_EXDR;
        return -1;
    }

    // send the message
    if (dtnipc_send(handle, DTN_UNREGISTER) != 0) {
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

//----------------------------------------------------------------------
int
dtn_find_registration(dtn_handle_t h,
                      dtn_endpoint_id_t *eid,
                      dtn_reg_id_t* regid)
{
    int status;
    dtnipc_handle_t* handle = (dtnipc_handle_t*)h;
    XDR* xdr_encode = &handle->xdr_encode;
    XDR* xdr_decode = &handle->xdr_decode;

    // check if the handle is in the middle of poll
    if (handle->in_poll) {
        handle->err = DTN_EINPOLL;
        return -1;
    }
    
    // pack the request
    if (!xdr_dtn_endpoint_id_t(xdr_encode, eid)) {
        handle->err = DTN_EXDR;
        return -1;
    }

    // send the message
    if (dtnipc_send(handle, DTN_FIND_REGISTRATION) != 0) {
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
    if (!xdr_dtn_reg_id_t(xdr_decode, regid)) {
        handle->err = DTN_EXDR;
        return -1;
    }

    return 0;
}

//----------------------------------------------------------------------
int
dtn_change_registration(dtn_handle_t h,
                        dtn_reg_id_t regid,
                        dtn_reg_info_t *reginfo)
{
    (void)regid;
    (void)reginfo;
    
    // XXX/demmer implement me
    dtnipc_handle_t* handle = (dtnipc_handle_t*)h;

    // check if the handle is in the middle of poll
    if (handle->in_poll) {
        handle->err = DTN_EINPOLL;
        return -1;
    }
    
    handle->err = DTN_EINTERNAL;
    return -1;
}

//---------------------------------------------------------------------- 
int
dtn_bind(dtn_handle_t h, dtn_reg_id_t regid)
{
    dtnipc_handle_t* handle = (dtnipc_handle_t*)h;
    XDR* xdr_encode = &handle->xdr_encode;

    // check if the handle is in the middle of poll
    if (handle->in_poll) {
        handle->err = DTN_EINPOLL;
        return -1;
    }
    
    // pack the request
    if (!xdr_dtn_reg_id_t(xdr_encode, &regid)) {
        handle->err = DTN_EXDR;
        return -1;
    }

    // send the message
    if (dtnipc_send_recv(handle, DTN_BIND) < 0) {
        return -1;
    }

    return 0;
}

//---------------------------------------------------------------------- 
int
dtn_unbind(dtn_handle_t h, dtn_reg_id_t regid)
{
    dtnipc_handle_t* handle = (dtnipc_handle_t*)h;
    XDR* xdr_encode = &handle->xdr_encode;

    // check if the handle is in the middle of poll
    if (handle->in_poll) {
        handle->err = DTN_EINPOLL;
        return -1;
    }
    
    // pack the request
    if (!xdr_dtn_reg_id_t(xdr_encode, &regid)) {
        handle->err = DTN_EXDR;
        return -1;
    }

    // send the message
    if (dtnipc_send_recv(handle, DTN_UNBIND) < 0) {
        return -1;
    }

    return 0;
}

//----------------------------------------------------------------------
int
dtn_send(dtn_handle_t h,
         dtn_bundle_spec_t* spec,
         dtn_bundle_payload_t* payload,
         dtn_bundle_id_t* id)
{
    dtnipc_handle_t* handle = (dtnipc_handle_t*)h;
    XDR* xdr_encode = &handle->xdr_encode;
    XDR* xdr_decode = &handle->xdr_decode;

    // check if the handle is in the middle of poll
    if (handle->in_poll) {
        handle->err = DTN_EINPOLL;
        return -1;
    }

    // pack the arguments
    if ((!xdr_dtn_bundle_spec_t(xdr_encode, spec)) ||
        (!xdr_dtn_bundle_payload_t(xdr_encode, payload))) {
        handle->err = DTN_EXDR;
        return -1;
    }

    // send the message
    if (dtnipc_send_recv(handle, DTN_SEND) < 0) {
        return -1;
    }
    
    // unpack the bundle id return value
    memset(id, 0, sizeof(id));
    
    if (!xdr_dtn_bundle_id_t(xdr_decode, id))
    {
        handle->err = DTN_EXDR;
        return DTN_EXDR;
    }

    return 0;
}

//----------------------------------------------------------------------
int
dtn_recv(dtn_handle_t h,
         dtn_bundle_spec_t* spec,
         dtn_bundle_payload_location_t location,
         dtn_bundle_payload_t* payload,
         dtn_timeval_t timeout)
{
    dtnipc_handle_t* handle = (dtnipc_handle_t*)h;
    XDR* xdr_encode = &handle->xdr_encode;
    XDR* xdr_decode = &handle->xdr_decode;

    if (handle->in_poll) {
        int poll_status = 0;
        if (dtnipc_recv(handle, &poll_status) != 0) {
            return -1;
        }
        
        if (poll_status != DTN_SUCCESS) {
            handle->err = poll_status;
            return -1;
        }
    }

    
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
    if (dtnipc_send_recv(handle, DTN_RECV) < 0) {
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

//----------------------------------------------------------------------
int
dtn_begin_poll(dtn_handle_t h, dtn_timeval_t timeout)
{
    dtnipc_handle_t* handle = (dtnipc_handle_t*)h;
    XDR* xdr_encode = &handle->xdr_encode;
    
    // check if the handle is already in the middle of poll
    if (handle->in_poll) {
        handle->err = DTN_EINPOLL;
        return -1;
    }

    handle->in_poll = 1;
    
    if ((!xdr_dtn_timeval_t(xdr_encode, &timeout)))
    {
        handle->err = DTN_EXDR;
        return -1;
    }

    // send the message but don't block for the response code -- we'll
    // grab it in dtn_recv or dtn_cancel_poll
    if (dtnipc_send(handle, DTN_BEGIN_POLL) < 0) {
        return -1;
    }
    
    return 0;
}

//----------------------------------------------------------------------
int
dtn_cancel_poll(dtn_handle_t h)
{
    dtnipc_handle_t* handle = (dtnipc_handle_t*)h;
    
    // make sure the handle is already in the middle of poll
    if (! (handle->in_poll)) {
        handle->err = DTN_EINVAL;
        return -1;
    }

    // clear the poll flag
    handle->in_poll = 0;
    
    // send the message and get the response code. however there is a
    // race condition meaning that we can't tell whether what we get
    // back is the original response from poll or from the subsequent
    // call to cancel poll
    if (dtnipc_send_recv(handle, DTN_CANCEL_POLL) < 0) {
        return -1;
    }
    
    return 0;
}

/*************************************************************
 *
 *                     Utility Functions
 *
 *************************************************************/
void
dtn_copy_eid(dtn_endpoint_id_t* dst, dtn_endpoint_id_t* src)
{
    memcpy(dst->uri, src->uri, DTN_MAX_ENDPOINT_ID);
}

//----------------------------------------------------------------------
int
dtn_parse_eid_string(dtn_endpoint_id_t* eid, const char* str)
{
    char *s;
    size_t len;

    len = strlen(str) + 1;

    // check string length
    if (len > DTN_MAX_ENDPOINT_ID) {
        return DTN_ESIZE;
    }
    
    // make sure there's a colon
    s = strchr(str, ':');
    if (!s) {
        return DTN_EINVAL;
    }

    // XXX/demmer make sure the scheme / ssp are comprised only of
    // legal URI characters
    memcpy(&eid->uri[0], str, len);

    return 0;
}

//----------------------------------------------------------------------
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

//----------------------------------------------------------------------
void
dtn_free_payload(dtn_bundle_payload_t* payload)
{
    xdr_free((xdrproc_t)xdr_dtn_bundle_payload_t, (char*)payload);
}
