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

#include "dtn_api.h"
#include "dtn_ipc.h"
#include "dtn_types.h"

namespace oasys {
class UDPClient;
}

namespace dtn {

class RegistrationList;

/**
 * Class that implements the main server side handling of the DTN
 * application IPC.
 */
class APIServer {
public:
    APIServer();

    /**
     * Initialize and register all the api server related dtn commands.
     */
    static void init_commands();

    /**
     * Post configuration, start up all components.
     */
    static void start_master();

    static in_addr_t local_addr_;	///< loopback address to use
    static u_int16_t handshake_port_;	///< handshaking udp port
    static u_int16_t session_port_;	///< api session port
    
protected:
    dtnipc_handle_t handle;

    char* buf_;
    size_t buflen_;
    oasys::UDPClient* sock_;
    XDR* xdr_encode_;
    XDR* xdr_decode_;
};

/**
 * Class for the generic listening server that just accepts open
 * messages and creates client servers.
 */
class MasterAPIServer : public APIServer,
                        public oasys::Thread,
                        public oasys::Logger {
public:
    MasterAPIServer();
    virtual void run();
};

/**
 * Class for the per-client connection server.
 */
class ClientAPIServer : public APIServer,
                        public oasys::Thread,
                        public oasys::Logger {
public:
    ClientAPIServer(in_addr_t remote_host, u_int16_t remote_port);
    virtual void run();

protected:
    int handle_getinfo();
    int handle_register();
    int handle_bind();
    int handle_send();
    int handle_recv();

    static const char* msgtoa(u_int32_t type);

    RegistrationList* bindings_;
};

} // namespace dtn

#endif /* _APISERVER_H_ */
