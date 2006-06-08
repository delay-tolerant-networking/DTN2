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
#ifndef DTN_IPC_H
#define DTN_IPC_H

#include <rpc/rpc.h>

#ifdef __CYGWIN__
#include <stdio.h>
#include <string.h>
#include <cygwin/socket.h>
#endif

#ifdef  __cplusplus
extern "C" {
#endif

/*************************************************************
 *
 * Internal structures and functions that implement the DTN
 * IPC layer. Generally, this is not exposed to applications.
 *
 *************************************************************/

/**
 * DTN IPC version. Just a simple number for now; we can refine it to
 * a major/minor version later if desired.
 *
 * Make sure to bump this when changing any data structures, message
 * types, adding functions, etc.
 */
#define DTN_IPC_VERSION 2

/**
 * Default api ports. The handshake port is used for initial contact
 * with the daemon to establish a session, and the latter is used for
 * individual sessions.
 */
#define DTN_IPC_PORT 5010

/**
 * The maximum IPC message size (in bytes). Used primarily for
 * efficiency in buffer allocation since the transport uses TCP.
 */
#define DTN_MAX_API_MSG 65536

/**
 * State of a DTN IPC channel.
 */
struct dtnipc_handle {
    int sock;				///< Socket file descriptor
    int err;				///< Error code
    int in_poll;			///< Flag to register a call to poll()
    char buf[DTN_MAX_API_MSG];		///< send/recv buffer
    XDR xdr_encode;			///< XDR encoder
    XDR xdr_decode;			///< XDR decoder
};

typedef struct dtnipc_handle dtnipc_handle_t;

/**
 * Type codes for api messages.
 */
typedef enum {
    DTN_OPEN	    		= 1,
    DTN_CLOSE			= 2,
    DTN_LOCAL_EID		= 3,
    DTN_REGISTER		= 4,
    DTN_UNREGISTER		= 5,
    DTN_FIND_REGISTRATION	= 6,
    DTN_CHANGE_REGISTRATION	= 7,
    DTN_BIND			= 8,
    DTN_UNBIND			= 9,
    DTN_SEND			= 10,
    DTN_RECV			= 11,
    DTN_BEGIN_POLL		= 12,
    DTN_CANCEL_POLL		= 13
} dtnapi_message_type_t;

/**
 * Type code to string conversion routine.
 */
const char* dtnipc_msgtoa(u_int8_t type);

/*
 * Initialize the handle structure and a new ipc session with the
 * daemon.
 *
 * Returns 0 on success, -1 on error.
 */
int dtnipc_open(dtnipc_handle_t* handle);

/*
 * Clean up the handle. dtnipc_open must have already been called on
 * the handle.
 *
 * Returns 0 on success, -1 on error.
 */
int dtnipc_close(dtnipc_handle_t* handle);
    
/*
 * Send a message over the dtn ipc protocol.
 *
 * Returns 0 on success, -1 on error.
 */
int dtnipc_send(dtnipc_handle_t* handle, dtnapi_message_type_t type);

/*
 * Receive a message response on the ipc channel. May block if there
 * is no pending message.
 *
 * Sets status to the server-returned status code and returns the
 * length of any reply message on success, returns -1 on internal
 * error.
 */
int dtnipc_recv(dtnipc_handle_t* handle, int* status);

/**
 * Send a message and wait for a response over the dtn ipc protocol.
 *
 * Returns 0 on success, -1 on error.
 */
int dtnipc_send_recv(dtnipc_handle_t* handle, dtnapi_message_type_t type);


#ifdef  __cplusplus
}
#endif

#endif /* DTN_IPC_H */
