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
 * Information request function.
 */
extern int dtn_get_info(dtn_handle_t handle,
                        dtn_info_request_t request,
                        dtn_info_response_t* response);

/**
 * Create or modify a dtn registration.
 */
extern int dtn_register(dtn_handle_t handle,
                        dtn_reg_info_t *reginfo,
                        dtn_reg_id_t* newregid);

/**
 * Associate a registration id and endpoint string with the current
 * ipc channel. This must be called before calling dtn_recv to inform
 * the daemon of which registrations the application is interested in.
 */
extern int dtn_bind(dtn_handle_t handle,
                    dtn_reg_id_t regid,
                    dtn_tuple_t* endpoint);

/**
 * Explicitly remove an association from the current ipc handle. Note
 * that this is also implicitly done when the handle is closed.
 */
extern int dtn_unbind(dtn_handle_t handle,
                      dtn_reg_id_t regid);

/**
 * Send a bundle either from memory or from a file.
 */
extern int dtn_send(dtn_handle_t handle,
                    dtn_bundle_spec_t* spec,
                    dtn_bundle_payload_t* payload);

/**
 * Blocking receive for a bundle
 */
extern int dtn_recv(dtn_handle_t handle,
                    dtn_bundle_spec_t* spec,
                    dtn_bundle_payload_location_t location,
                    dtn_bundle_payload_t* payload,
                    dtn_timeval_t timeout);

/*************************************************************
 *
 *                     Utility Functions
 *
 *************************************************************/

/*
 * Copy the value of one tuple to another.
 *
 * Note that this will _not_ call malloc for the admin part so the
 * underlying memory will be shared.
 *
 */
extern void dtn_copy_tuple(dtn_tuple_t* dst, dtn_tuple_t* src);

/*
 * Sets the value of the tuple to the given region, admin, and
 * endpoint demultiplexer strings.
 *
 * If length fields are <= 0, then their corresponding values are
 * assumed to be C strings and strlen() is used to determine their
 * length.
 *
 * Returns: 0 on success, DTNERR_INVAL if the arguments are malformed.
 */
extern int dtn_set_tuple(dtn_tuple_t* tuple,
                         char* region, size_t region_len,
                         char* admin, size_t admin_len);

/*
 * Queries the router for the default local tuple and appends the
 * given endpoint string to the url, assigning the result to the given
 * tuple pointer.
 *
 * Returns: 0 on success, XXX/demmer fill in others
 */
extern int dtn_build_local_tuple(dtn_handle_t handle,
                                 dtn_tuple_t* tuple, char* endpoint);

/*
 * Parses the given string according to the generic schema
 * dtn://<region>/<admin> and assigns it to the given tuple.
 *
 * Returns: 0 on success, DTNERR_INVAL if the given string is not a
 * valid tuple.
 */
extern int dtn_parse_tuple_string(dtn_tuple_t* tuple, char* str);

/*
 * Sets the value of the given payload structure to either a memory
 * buffer or a file location.
 *
 * Returns: 0 on success, DTNERR_SIZE if the memory location is
 * selected and the payload is too big.
 */
extern int dtn_set_payload(dtn_bundle_payload_t* payload,
                           dtn_bundle_payload_location_t location,
                           char* val, int len);

#ifdef  __cplusplus
}
#endif

#endif /* DTN_API_H */
