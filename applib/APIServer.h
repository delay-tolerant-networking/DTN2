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

#ifndef _APISERVER_H_
#define _APISERVER_H_

#include <list>

#include <oasys/compat/rpc.h>
#include <oasys/debug/Log.h>
#include <oasys/thread/Thread.h>
#include <oasys/thread/SpinLock.h>
#include <oasys/io/TCPClient.h>
#include <oasys/io/TCPServer.h>

#include "dtn_api.h"
#include "dtn_ipc.h"
#include "dtn_types.h"

namespace dtn {

class APIClient;
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

    // shutdown hook, clean up clients
    virtual void shutdown_hook();

    // Virtual from TCPServerThread
    void accepted(int fd, in_addr_t addr, u_int16_t port);

    bool       enabled() const { return enabled_; }
    bool*      enabled_ptr() { return &enabled_; }
    
    in_addr_t  local_addr() const { return local_addr_; }
    in_addr_t* local_addr_ptr() { return &local_addr_; }
    
    u_int16_t  local_port() const { return local_port_; }
    u_int16_t* local_port_ptr() { return &local_port_; }

    void register_client(APIClient *);
    void unregister_client(APIClient *);

protected:
    bool      enabled_;       ///< whether or not to enable it
    in_addr_t local_addr_;    ///< local address to bind to
    u_int16_t local_port_;    ///< local port to use for api

    std::list<APIClient *> client_list; ///<  active clients
    oasys::SpinLock client_list_lock;   ///< synchronizer
};

/**
 * Class that implements the API session.
 */
class APIClient : public oasys::Thread, public oasys::TCPClient {
public:
    APIClient(int fd, in_addr_t remote_host, u_int16_t remote_port,
              APIServer *parent);
    virtual ~APIClient();
    virtual void run();

    void close_client();
    
protected:
    int handle_handshake();
    int handle_local_eid();
    int handle_register();
    int handle_unregister();
    int handle_find_registration();
    int handle_find_registration2();
    int handle_bind();
    int handle_unbind();
    int handle_send();
    int handle_cancel();
    int handle_recv();
    int handle_ack();
    int handle_begin_poll();
    int handle_cancel_poll();
    int handle_close();
    int handle_session_update();
    int handle_peek();

    // block the calling thread, waiting for bundle arrival on a bound
    // registration, notification that a subscriber has arrived for a
    // custody session, or for traffic on the api socket.
    //
    // returns the oasys IO error code if there was a timeout or an
    // internal error. returns 0 if there is a bundle waiting or
    // socket data on the channel, and assigns the reg or sock_ready
    // pointers appropriately
    int wait_for_notify(const char*       operation,
                        dtn_timeval_t     timeout,
                        APIRegistration** recv_ready_reg,
                        APIRegistration** session_ready_reg,
                        bool*             sock_ready);

    int handle_unexpected_data(const char* operation);

    int send_response(int ret);

    bool is_bound(u_int32_t regid);
    
    char buf_[DTN_MAX_API_MSG];
    XDR xdr_encode_;
    XDR xdr_decode_;
    APIRegistrationList* bindings_;
    APIRegistrationList* sessions_;
    oasys::Notifier notifier_;
    APIServer* parent_;
    size_t total_sent_;
    size_t total_rcvd_;
};

} // namespace dtn

#endif /* _APISERVER_H_ */
