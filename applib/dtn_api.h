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
#ifndef DTN_API_H
#define DTN_API_H

#include "dtn_errno.h"
#include "dtn_types.h"

/**
 * The basic handle for communication with the dtn router.
 */
typedef char* dtn_handle_t;

#ifdef  __cplusplus
extern "C" {
#endif

/*************************************************************
 *
 *                     API Functions
 *
 *************************************************************/

/**
 * Open a new connection to the router.
 *
 * Returns the new handle on success, NULL on error.
 */
extern dtn_handle_t dtn_open();

/**
 * Close an open dtn handle.
 */
extern int dtn_close(dtn_handle_t handle);

/**
 * Get the error associated with the given handle.
 */
extern int dtn_errno(dtn_handle_t handle);

/**
 * Get a string value associated with the dtn error code.
 */
char* dtn_strerror(int err);

/**
 * Build an appropriate local endpoint id by appending the specified
 * service tag to the daemon's preferred administrative endpoint id.
 */
extern int dtn_build_local_eid(dtn_handle_t handle,
                               dtn_endpoint_id_t* local_eid,
                               const char* service_tag);

/**
 * Create a dtn registration.
 */
extern int dtn_register(dtn_handle_t handle,
                        dtn_reg_info_t* reginfo,
                        dtn_reg_id_t* newregid);

/**
 * Remove a dtn registration.
 *
 * If the registration is in the passive state (i.e. not bound to any
 * handle), it is immediately removed from the system. If it is in
 * active state and bound to the given handle, the removal is deferred
 * until the handle unbinds the registration or closes. This allows
 * applications to pre-emptively call unregister so they don't leak
 * registrations.
 */
extern int dtn_unregister(dtn_handle_t handle,
                          dtn_reg_id_t regid);

/**
 * Check for an existing registration on the given endpoint id,
 * returning DTN_SUCCSS and filling in the registration id if it
 * exists, or returning ENOENT if it doesn't.
 */
extern int dtn_find_registration(dtn_handle_t handle,
                                 dtn_endpoint_id_t* eid,
                                 dtn_reg_id_t* regid);

/**
 * Modify an existing registration.
 */
extern int dtn_change_registration(dtn_handle_t handle,
                                   dtn_reg_id_t newregid,
                                   dtn_reg_info_t *reginfo);

/**
 * Associate a registration with the current ipc channel. This must
 * be called before calling dtn_recv to inform the daemon of which
 * registrations the application is interested in.
 *
 * This serves to put the registration in "active" mode.
 */
extern int dtn_bind(dtn_handle_t handle, dtn_reg_id_t regid);

/**
 * Explicitly remove an association from the current ipc handle. Note
 * that this is also implicitly done when the handle is closed.
 *
 * This serves to put the registration back in "passive" mode.
 */
extern int dtn_unbind(dtn_handle_t handle, dtn_reg_id_t regid);

/**
 * Send a bundle either from memory or from a file.
 */
extern int dtn_send(dtn_handle_t handle,
                    dtn_bundle_spec_t* spec,
                    dtn_bundle_payload_t* payload,
                    dtn_bundle_id_t* id);

/**
 * Blocking receive for a bundle, filling in the spec and payload
 * structures with the bundle data. The location parameter indicates
 * the manner by which the caller wants to receive payload data (i.e.
 * either in memory or in a file). The timeout parameter specifies an
 * interval in milliseconds to block on the server-side (-1 means
 * infinite wait).
 *
 * Note that it is advisable to call dtn_free_payload on the returned
 * structure, otherwise the XDR routines will memory leak.
 */
extern int dtn_recv(dtn_handle_t handle,
                    dtn_bundle_spec_t* spec,
                    dtn_bundle_payload_location_t location,
                    dtn_bundle_payload_t* payload,
                    dtn_timeval_t timeout);

/**
 * Begin a polling period for incoming bundles. Returns a file
 * descriptor suitable for calling poll() or select() on. Note that
 * dtn_bind() must have been previously called at least once on the
 * handle.
 *
 * If the kernel returns an indication that there is data ready on the
 * file descriptor, a call to dtn_recv will then return the bundle
 * without blocking.
 *
 * Also, no other API calls besides dtn_recv can be executed during a
 * polling period, so an app must call dtn_poll_cancel before trying
 * to run other API calls.
 */
extern int dtn_begin_poll(dtn_handle_t handle, dtn_timeval_t timeout);

/**
 * Cancel a polling interval.
 */
extern int dtn_cancel_poll(dtn_handle_t handle);

/*************************************************************
 *
 *                     Utility Functions
 *
 *************************************************************/

/*
 * Copy the contents of one eid into another.
 */
extern void dtn_copy_eid(dtn_endpoint_id_t* dst, dtn_endpoint_id_t* src);

/*
 * Parse a string into an endpoint id structure, validating that it is
 * in fact a valid endpoint id (i.e. a URI).
 */
extern int dtn_parse_eid_string(dtn_endpoint_id_t* eid, const char* str);

/*
 * Sets the value of the given payload structure to either a memory
 * buffer or a file location.
 *
 * Returns: 0 on success, DTN_ESIZE if the memory location is
 * selected and the payload is too big.
 */
extern int dtn_set_payload(dtn_bundle_payload_t* payload,
                           dtn_bundle_payload_location_t location,
                           char* val, int len);

/*
 * Frees dynamic storage allocated by the xdr for a bundle payload in
 * dtn_recv.
 */
void dtn_free_payload(dtn_bundle_payload_t* payload);

#ifdef  __cplusplus
}
#endif

#endif /* DTN_API_H */
