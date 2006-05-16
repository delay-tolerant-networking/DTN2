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
#ifndef _APISERVER_H_
#define _APISERVER_H_

#include <oasys/debug/Log.h>
#include <oasys/thread/Thread.h>
#include <oasys/io/TCPClient.h>
#include <oasys/io/TCPServer.h>

#include "dtn_api.h"
#include "dtn_ipc.h"
#include "dtn_types.h"

namespace dtn {

class APIRegistration;
class APIRegistrationList;

/**
 * Class that implements the main server side handling of the DTN
 * application IPC.
 */
class APIServer : public oasys::TCPServerThread {
public:
    /**
     * The constructor checks for environment variable overrides of
     * the address / port. It is expected that someone else will call
     * bind_listen_start() on the APIServer instance.
     */
    APIServer();

    /**
     * Initialize and register all the api server related dtn commands.
     */
    void init();
    
    // Virtual from TCPServerThread
    void accepted(int fd, in_addr_t addr, u_int16_t port);
    
    static in_addr_t local_addr_;	///< local address to bind to
    static u_int16_t local_port_;	///< local port to use for api
};

/**
 * Class that implements the API session.
 */
class APIClient : public oasys::Thread, public oasys::TCPClient {
public:
    APIClient(int fd, in_addr_t remote_host, u_int16_t remote_port);
    virtual ~APIClient();
    virtual void run();

    void close_session();
    
protected:
    int handle_handshake();
    int handle_local_eid();
    int handle_register();
    int handle_unregister();
    int handle_find_registration();
    int handle_bind();
    int handle_send();
    int handle_recv();
    int handle_begin_poll();
    int handle_cancel_poll();
    int handle_close();

    // wait for a bundle arrival on any bound registration, or for
    // traffic on the api socket.
    //
    // returns the oasys IO error code if there was a timeout or an
    // internal error. returns 0 if there is a bundle waiting or
    // socket data on the channel, and assigns the reg or sock_ready
    // pointers appropriately
    int wait_for_bundle(const char* operation, dtn_timeval_t timeout,
                        APIRegistration** reg, bool* sock_ready);

    int send_response(int ret);

    bool is_bound(u_int32_t regid);
    
    char buf_[DTN_MAX_API_MSG];
    XDR xdr_encode_;
    XDR xdr_decode_;
    APIRegistrationList* bindings_;
    oasys::Notifier notifier_;
};

} // namespace dtn

#endif /* _APISERVER_H_ */
