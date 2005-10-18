/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2004 Intel Corporation. All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 
 *   Neither the name of the Intel Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <oasys/compat/inet_aton.h>
#include <oasys/compat/inttypes.h>

#include "dtn_ipc.h"
#include "dtn_errno.h"
#include "dtn_types.h"

const char*
dtnipc_msgtoa(u_int32_t type)
{
#define CASE(_type) case _type : return #_type; break;
    
    switch(type) {
        CASE(DTN_OPEN);
        CASE(DTN_CLOSE);
        CASE(DTN_LOCAL_EID);
        CASE(DTN_REGISTER);
        CASE(DTN_BIND);
        CASE(DTN_SEND);
        CASE(DTN_RECV);

    default:
        return "(unknown type)";
    }
    
#undef CASE
}

/*
 * Initialize the handle structure.
 */
int
dtnipc_open(dtnipc_handle_t* handle)
{
    int ret;
    char *env, *end;
    struct sockaddr_in sa;
    in_addr_t ipc_addr;
    u_int16_t ipc_port;
    u_int32_t handshake;
    u_int port;

    // zero out the handle
    memset(handle, 0, sizeof(dtnipc_handle_t));
    
    // note that we leave eight bytes free in both buffers to be used
    // for the framing -- the type code and length for send, and the
    // return code and length for recv
    xdrmem_create(&handle->xdr_encode, handle->buf + 8,
                  DTN_MAX_API_MSG, XDR_ENCODE);
    
    xdrmem_create(&handle->xdr_decode, handle->buf + 8,
                  DTN_MAX_API_MSG, XDR_DECODE);

    // open the socket
    handle->sock = socket(PF_INET, SOCK_STREAM, 0);
    if (handle->sock < 0)
    {
        dtnipc_close(handle);
        handle->err = DTN_ECOMM;
        return -1;
    }

    // check for DTNAPI environment variables overriding the address /
    // port defaults
    ipc_addr = htonl(INADDR_LOOPBACK);
    ipc_port = DTN_IPC_PORT;
    
    if ((env = getenv("DTNAPI_ADDR")) != NULL) {
        if (inet_aton(env, (struct in_addr*)&ipc_addr) == 0)
        {
            fprintf(stderr, "DTNAPI_ADDR environment variable (%s) "
                    "not a valid ip address\n", env);
            exit(1);
        }
    }

    if ((env = getenv("DTNAPI_PORT")) != NULL) {
        port = strtoul(env, &end, 10);
        if (*end != '\0' || port > 0xffff)
        {
            fprintf(stderr, "DTNAPI_PORT environment variable (%s) "
                    "not a valid ip port\n", env);
            exit(1);
        }
        ipc_port = (u_int16_t)port;
    }

    // connect to the server
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = ipc_addr;
    sa.sin_port = htons(ipc_port);
    
    ret = connect(handle->sock, (const struct sockaddr*)&sa, sizeof(sa));
    if (ret != 0) {
        dtnipc_close(handle);
        handle->err = DTN_ECOMM;
        return -1;
    }

    // send the session initiation to the server on the handshake port
    handshake = htonl(DTN_OPEN);
    ret = write(handle->sock, &handshake, sizeof(handshake));
    if (ret != sizeof(handshake)) {
        dtnipc_close(handle);
        handle->err = DTN_ECOMM;
        return -1;
    }

    // wait for the handshake response
    handshake = 0;
    ret = read(handle->sock, &handshake, sizeof(handshake));
    if (ret != sizeof(handshake) || htonl(handshake) != DTN_OPEN) {
        dtnipc_close(handle);
        handle->err = DTN_ECOMM;
        return -1;
    }
    
    return 0;
}

/*
 * Clean up the handle. dtnipc_open must have already been called on
 * the handle.
 */
int
dtnipc_close(dtnipc_handle_t* handle)
{
    // first send a close over RPC
    int ret = dtnipc_send_recv(handle, DTN_CLOSE);
    
    xdr_destroy(&handle->xdr_encode);
    xdr_destroy(&handle->xdr_decode);

    if (handle->sock > 0) {
        close(handle->sock);
    }

    handle->sock = 0;

    return ret;
}
      

/*
 * Send a message over the dtn ipc protocol.
 *
 * Returns 0 on success, -1 on error.
 */
int
dtnipc_send(dtnipc_handle_t* handle, dtnapi_message_type_t type)
{
    int ret;
    size_t len, msglen;
    u_int32_t typecode = type;
    
    // pack the message code in the first four bytes of the buffer and
    // the message length into the next four. we don't use xdr
    // routines for these since we need to be able to decode the
    // length on the other side to make sure we've read the whole
    // message, and we need the type to know which xdr decoder to call
    typecode = htonl(type);
    memcpy(handle->buf, &typecode, sizeof(typecode));

    msglen = len = xdr_getpos(&handle->xdr_encode);
    len = htonl(len);
    memcpy(&handle->buf[4], &len, sizeof(len));
    
    // reset the xdr encoder
    xdr_setpos(&handle->xdr_encode, 0);
    
    // send the message, looping until it's all sent
    msglen += 8;
    do {
        ret = write(handle->sock, handle->buf, msglen);
        
        if (ret <= 0) {
            if (errno == EINTR)
                continue;
            
            dtnipc_close(handle);
            handle->err = DTN_ECOMM;
            return -1;
        }
        
        msglen -= ret;
        
    } while (msglen > 0);
    
    return 0;
}

/*
 * Receive a message on the ipc channel. May block if there is no
 * pending message.
 *
 * Sets status to the server-returned status code and returns the
 * length of any reply message on success, returns -1 on internal
 * error.
 */
int
dtnipc_recv(dtnipc_handle_t* handle, int* status)
{
    int ret;
    size_t len, nread;
    u_int32_t statuscode;

    // reset the xdr decoder before reading in any data
    xdr_setpos(&handle->xdr_decode, 0);

    // read as much as possible into the buffer
    ret = read(handle->sock, handle->buf, sizeof(handle->buf));

    // make sure we got at least the status code and length
    if (ret < 8) {
        dtnipc_close(handle);
        handle->err = DTN_ECOMM;
        return -1;
    }
    
    memcpy(&statuscode, handle->buf, sizeof(statuscode));
    statuscode = ntohl(statuscode);
    *status = statuscode;
    
    memcpy(&len, &handle->buf[4], sizeof(len));
    len = ntohl(len);

    // read the rest of the message if we didn't get it all
    nread = ret;
    while (nread < len + 8) {
        ret = read(handle->sock,
                   &handle->buf[nread], sizeof(handle->buf) - nread);
        if (ret <= 0) {
            if (errno == EINTR)
                continue;
            
            dtnipc_close(handle);
            handle->err = DTN_ECOMM;
            return -1;
        }

        nread += ret;
    }

    return len;
}


/**
 * Send a message and wait for a response over the dtn ipc protocol.
 *
 * Returns 0 on success, -1 on error.
 */
int dtnipc_send_recv(dtnipc_handle_t* handle, dtnapi_message_type_t type)
{
    int status;

    // send the message
    if (dtnipc_send(handle, type) < 0) {
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
