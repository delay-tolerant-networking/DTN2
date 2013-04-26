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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <algorithm>

#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>

#include <oasys/compat/inet_aton.h>
#include <oasys/compat/rpc.h>
#include <oasys/io/FileIOClient.h>
#include <oasys/io/NetUtils.h>
#include <oasys/util/Pointers.h>
#include <oasys/util/ScratchBuffer.h>
#include <oasys/util/XDRUtils.h>

#include "APIServer.h"
#include "bundling/APIBlockProcessor.h"
#include "bundling/UnknownBlockProcessor.h"
#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleProtocol.h"
#include "bundling/BundleStatusReport.h"
#include "bundling/SDNV.h"
#include "bundling/GbofId.h"
#include "naming/EndpointID.h"
#include "cmd/APICommand.h"
#include "reg/APIRegistration.h"
#include "reg/RegistrationTable.h"
#include "routing/BundleRouter.h"
#include "storage/GlobalStore.h"
#include "session/Session.h"

#ifndef MIN
#define MIN(x, y) ((x)<(y) ? (x) : (y))
#endif

namespace dtn {

//----------------------------------------------------------------------
APIServer::APIServer()
      // DELETE_ON_EXIT flag is not set; see below.
    : TCPServerThread("APIServer", "/dtn/apiserver", 0)	
{
    enabled_    = true;
    local_addr_ = htonl(INADDR_LOOPBACK);
    local_port_ = DTN_IPC_PORT;

    // override the defaults via environment variables, if given
    char *env;
    if ((env = getenv("DTNAPI_ADDR")) != NULL) {
        if (inet_aton(env, (struct in_addr*)&local_addr_) == 0)
        {
            log_err("DTNAPI_ADDR environment variable (%s) "
                    "not a valid ip address, using default of localhost",
                    env);
            // in case inet_aton touched it
            local_addr_ = htonl(INADDR_LOOPBACK);
        } else {
            log_debug("local address set to %s by DTNAPI_ADDR "
                      "environment variable", env);
        }
    }

    if ((env = getenv("DTNAPI_PORT")) != NULL) {
        char *end;
        u_int port = strtoul(env, &end, 10);
        if (*end != '\0' || port > 0xffff)
        {
            log_err("DTNAPI_PORT environment variable (%s) "
                    "not a valid ip port, using default of %d",
                    env, DTN_IPC_PORT);
            port = DTN_IPC_PORT;
        } else {
            log_debug("api port set to %s by DTNAPI_PORT "
                      "environment variable", env);
        }
        local_port_ = (u_int16_t)port;
    }

    if (local_addr_ != INADDR_ANY || local_port_ != 0) {
        log_debug("APIServer init (evironment set addr %s port %d)",
                  intoa(local_addr_), local_port_);
    } else {
        log_debug("APIServer init");
    }

    oasys::TclCommandInterp::instance()->reg(new APICommand(this));
}

//----------------------------------------------------------------------
void
APIServer::accepted(int fd, in_addr_t addr, u_int16_t port)
{
    APIClient* c = new APIClient(fd, addr, port, this);
    register_client(c);
    c->start();
}

//----------------------------------------------------------------------

// We keep a list of clients (register_client, unregister_client). As
// each client shuts down it removes itself from the list. The server
// sets should_stop to each of the clients, then spins waiting for the
// list of clients to be emptied out. If we spin for a long time
// (MAX_SPIN_TIME) without the list getting empty we give up.

// note that the thread was created without DELETE_ON_EXIT so that the
// thread object sticks around after the thread has died. This has the
// upside of helping out APIClient objects that wake up after the
// APIServer has given up on them (saving us from a core dump) but has
// the downside of losing memory (one APIServer thread object). But
// since the APIServer is shut down when we're about to exit, it's not
// an issue. And only one APIServer is ever created.

void
APIServer::shutdown_hook()
{
    // tell the clients to shut down
    std::list<APIClient *>::iterator ci;
    client_list_lock.lock("APIServer::shutdown");
    for (ci = client_list.begin(); ci != client_list.end();  ++ci) {
        (*ci)->set_should_stop();
    }
    client_list_lock.unlock();

#define MAX_SPIN_TIME (5 * 1000000) // max sleep in usec
#define EACH_SPIN_TIME 10000	// sleep 10ms each time

    // As clients exit they unregister themselves, so if a client is
    // still on the list we assume that it is still alive.  So here we
    // loop until the list is empty or MAX_SLEEP_TIME usecs have
    // passed. (We have a time out in case a client thread is wedged
    // or blocked waiting for a client. What we really want to catch
    // here is clients in the middle of processing a request.)
    int count = 0;
    while (count++ < (MAX_SPIN_TIME / EACH_SPIN_TIME)) {
        client_list_lock.lock("APIServer::shutdown");
        bool empty = client_list.empty();
        client_list_lock.unlock();
        if (!empty)
          usleep(EACH_SPIN_TIME);
        else
          break;
    }
    return;
}


//----------------------------------------------------------------------

// manages a list of APIClient objects (threads) that have not exited yet.

void
APIServer::register_client(APIClient *c)
{
    oasys::ScopeLock l(&client_list_lock, "APIServer::register_client");
    client_list.push_front(c);
}

void
APIServer::unregister_client(APIClient *c)
{
    // remove c from the list of active clients
    oasys::ScopeLock l(&client_list_lock, "APIServer::unregister_client");
    client_list.remove(c);
}

//----------------------------------------------------------------------
APIClient::APIClient(int fd, in_addr_t addr, u_int16_t port, APIServer *parent)
    : Thread("APIClient", DELETE_ON_EXIT),
      TCPClient(fd, addr, port, "/dtn/apiclient"),
      notifier_(logpath_),
      parent_(parent),
      total_sent_(0),
      total_rcvd_(0)
{
    // note that we skip space for the message length and code/status
    xdrmem_create(&xdr_encode_, buf_ + 8, DTN_MAX_API_MSG - 8, XDR_ENCODE);
    xdrmem_create(&xdr_decode_, buf_ + 8, DTN_MAX_API_MSG - 8, XDR_DECODE);

    bindings_ = new APIRegistrationList();
    sessions_ = new APIRegistrationList();
}

//----------------------------------------------------------------------
APIClient::~APIClient()
{
    log_debug("client destroyed");
    delete_z(bindings_);
    delete_z(sessions_);
}

//----------------------------------------------------------------------
void
APIClient::close_client()
{
    TCPClient::close();

    APIRegistration* reg;
    while (! bindings_->empty()) {
        reg = bindings_->front();
        bindings_->pop_front();
        
        reg->set_active(false);

        if (reg->expired()) {
            log_debug("removing expired registration %d", reg->regid());
            BundleDaemon::post(new RegistrationExpiredEvent(reg));
        }
    }

    // XXX/demmer memory leak here?
    sessions_->clear();
    
    parent_->unregister_client(this);
}

//----------------------------------------------------------------------
int
APIClient::handle_handshake()
{
    u_int32_t handshake;
    u_int16_t message_type, ipc_version;
    
    int ret = readall((char*)&handshake, sizeof(handshake));
    if (ret != sizeof(handshake)) {
        log_err("error reading handshake: (got %d/%zu), \"error\" %s",
                ret, sizeof(handshake), strerror(errno));
        return -1;
    }

    total_rcvd_ += ret;

    message_type = ntohl(handshake) >> 16;
    ipc_version = (u_int16_t) (ntohl(handshake) & 0x0ffff);

    if (message_type != DTN_OPEN) {
        log_err("handshake (0x%x)'s message type %d != DTN_OPEN (%d)",
                handshake, message_type, DTN_OPEN);
        return -1;
    }
    
    // to handle version mismatch more cleanly, we re-build the
    // handshake word with our own version and send it back to inform
    // the client, then if there's a mismatch, close the channel
    handshake = htonl(DTN_OPEN << 16 | DTN_IPC_VERSION);
    
    ret = writeall((char*)&handshake, sizeof(handshake));
    if (ret != sizeof(handshake)) {
        log_err("error writing handshake: %s", strerror(errno));
        return -1;
    }

    total_sent_ += ret;
    
    if (ipc_version != DTN_IPC_VERSION) {
        log_err("handshake (0x%x)'s version %d != DTN_IPC_VERSION (%d)",
                handshake, ipc_version, DTN_IPC_VERSION);
        return -1;
    }

    return 0;
}

//----------------------------------------------------------------------
void
APIClient::run()
{
    int ret;
    u_int8_t type;
    u_int32_t len;
    
    log_info("new session %s:%d -> %s:%d",
             intoa(local_addr()), local_port(),
             intoa(remote_addr()), remote_port());

    if (handle_handshake() != 0) {
        close_client();
        return;
    }
    
    while (true) {
        // check if someone has told us to quit by setting the
        // should_stop flag. if so, we're all done
        if (should_stop()) {
            close_client();
            return;
        }

        xdr_setpos(&xdr_encode_, 0);
        xdr_setpos(&xdr_decode_, 0);

        // read the typecode and length of the incoming message into
        // the fourth byte of the, since the pair is five bytes long
        // and the XDR engines are set to point at the eighth byte of
        // the buffer
        log_debug("waiting for next message... total sent/rcvd: %zu/%zu",
                  total_sent_, total_rcvd_);
        
        ret = read(&buf_[3], 5);
        if (ret <= 0) {
            log_warn("client disconnected without calling dtn_close");
            close_client();
            return;
        }
        total_rcvd_ += ret;
        
        if (ret < 5) {
            log_err("ack!! can't handle really short read...");
            close_client();
            return;
        }

        // NOTE: this protocol is duplicated in the implementation of
        // handle_begin_poll to take care of a cancel_poll request
        // coming in while the thread is waiting for bundles so any
        // modifications must be propagated there
        type = buf_[3];
        memcpy(&len, &buf_[4], sizeof(len));

        len = ntohl(len);

        ret -= 5;
        log_debug("got %s (%d/%d bytes)", dtnipc_msgtoa(type), ret, len);

        // if we didn't get the whole message, loop to get the rest,
        // skipping the header bytes and the already-read amount
        if (ret < (int)len) {
            int toget = len - ret;
            log_debug("reading remainder of message... total sent/rcvd: %zu/%zu",
                      total_sent_, total_rcvd_);
            if (readall(&buf_[8 + ret], toget) != toget) {
                log_err("error reading message remainder: %s",
                        strerror(errno));
                close_client();
                return;
            }
            total_rcvd_ += toget;
        }

        // check if someone has told us to quit by setting the
        // should_stop flag. if so, we're all done
        if (should_stop()) {
            close_client();
            return;
        }

        // dispatch to the handler routine
        switch(type) {
#define DISPATCH(_type, _fn)                    \
        case _type:                             \
            ret = _fn();                        \
            break;
            
            DISPATCH(DTN_LOCAL_EID,         handle_local_eid);
            DISPATCH(DTN_REGISTER,          handle_register);
            DISPATCH(DTN_UNREGISTER,        handle_unregister);
            DISPATCH(DTN_FIND_REGISTRATION, handle_find_registration);
            DISPATCH(DTN_FIND_REGISTRATION_WTOKEN, handle_find_registration2);
            DISPATCH(DTN_SEND,              handle_send);
            DISPATCH(DTN_CANCEL,            handle_cancel);
            DISPATCH(DTN_BIND,              handle_bind);
            DISPATCH(DTN_UNBIND,            handle_unbind);
            DISPATCH(DTN_RECV,              handle_recv);
            DISPATCH(DTN_ACK,               handle_ack);
            DISPATCH(DTN_BEGIN_POLL,        handle_begin_poll);
            DISPATCH(DTN_CANCEL_POLL,       handle_cancel_poll);
            DISPATCH(DTN_CLOSE,             handle_close);
            DISPATCH(DTN_SESSION_UPDATE,    handle_session_update);
            DISPATCH(DTN_PEEK,              handle_peek);
#undef DISPATCH

        default:
            log_err("unknown message type code 0x%x", type);
            ret = DTN_EMSGTYPE;
            break;
        }

        // if the handler returned -1, then the session should be
        // immediately terminated
        if (ret == -1) {
            close_client();
            return;
        }
        
        // send the response
        if (send_response(ret) != 0) {
            return;
        }

        // if there was an IPC communication error or unknown message
        // type, close terminate the session
        // XXX/matt we could potentially close on all errors, not just these 2
        if (ret == DTN_ECOMM || ret == DTN_EMSGTYPE) {
            close_client();
            return;
        }
        
    } // while(1)
}

//----------------------------------------------------------------------
int
APIClient::send_response(int ret)
{
    u_int32_t len, msglen;
    
    // make sure the dispatched function returned a valid error
    // code
    ASSERT(ret == DTN_SUCCESS ||
           (DTN_ERRBASE <= ret && ret <= DTN_ERRMAX));
        
    // fill in the reply message with the status code and the
    // length of the reply. note that if there is no reply, then
    // the xdr position should still be zero
    len = xdr_getpos(&xdr_encode_);
    log_debug("building reply: status %s, length %d",
              dtn_strerror(ret), len);

    msglen = len + 8;
    ret = ntohl(ret);
    len = htonl(len);

    memcpy(buf_,     &ret, sizeof(ret));
    memcpy(&buf_[4], &len, sizeof(len));

    log_debug("sending %d byte reply message... total sent/rcvd: %zu/%zu",
              msglen, total_sent_, total_rcvd_);
    
    if (writeall(buf_, msglen) != (int)msglen) {
        log_err("error sending reply: %s", strerror(errno));
        close_client();
        return -1;
    }

    total_sent_ += msglen;

    return 0;
}
        
//----------------------------------------------------------------------
bool
APIClient::is_bound(u_int32_t regid)
{
    APIRegistrationList::iterator iter;
    for (iter = bindings_->begin(); iter != bindings_->end(); ++iter) {
        if ((*iter)->regid() == regid) {
            return true;
        }
    }

    return false;
}

//----------------------------------------------------------------------
int
APIClient::handle_local_eid()
{
    dtn_service_tag_t service_tag;
    dtn_endpoint_id_t local_eid;
    
    // unpack the request
    if (!xdr_dtn_service_tag_t(&xdr_decode_, &service_tag))
    {
        log_err("error in xdr unpacking arguments");
        return DTN_EXDR;
    }

    // build up the response
    EndpointID eid(BundleDaemon::instance()->local_eid());
    if (eid.append_service_tag(service_tag.tag) == false) {
        log_err("error appending service tag");
        return DTN_EINVAL;
    }

    memset(&local_eid, 0, sizeof(local_eid));
    eid.copyto(&local_eid);
    
    // pack the response
    if (!xdr_dtn_endpoint_id_t(&xdr_encode_, &local_eid)) {
        log_err("internal error in xdr: xdr_dtn_endpoint_id_t");
        return DTN_EXDR;
    }

    log_debug("get_local_eid encoded %d byte response",
              xdr_getpos(&xdr_encode_));
    
    return DTN_SUCCESS;
}

//----------------------------------------------------------------------
int
APIClient::handle_register()
{
    APIRegistration* reg;
    Registration::failure_action_t f_action;
    Registration::replay_action_t r_action;
    EndpointIDPattern endpoint;
    std::string script;
    
    dtn_reg_info_t reginfo;

    memset(&reginfo, 0, sizeof(reginfo));
    
    // unpack and parse the request
    if (!xdr_dtn_reg_info_t(&xdr_decode_, &reginfo))
    {
        log_err("error in xdr unpacking arguments");
        return DTN_EXDR;
    }

    // make sure we free any dynamically-allocated bits in the
    // incoming structure before we exit the proc
    oasys::ScopeXDRFree x((xdrproc_t)xdr_dtn_reg_info_t, (char*)&reginfo);
    
    endpoint.assign(&reginfo.endpoint);

    if (!endpoint.valid()) {
        log_err("invalid endpoint id in register: '%s'",
                reginfo.endpoint.uri);
        return DTN_EINVAL;
    }

    u_int64_t reg_token;
    memcpy(&reg_token, &reginfo.reg_token, sizeof(u_int64_t));

    // registration flags are a bitmask currently containing:
    //
    // [unused] [3 bits session flags] [2 bits failure action]

    u_int failure_action = reginfo.flags & 0x3;
    switch (failure_action) {
    case DTN_REG_DEFER: f_action = Registration::DEFER; break;
    case DTN_REG_DROP:  f_action = Registration::DROP;  break;
    case DTN_REG_EXEC:  f_action = Registration::EXEC;  break;
    default: {
        log_err("invalid registration flags 0x%x", reginfo.flags);
        return DTN_EINVAL;
    }
    }

    // replay flag processing

    u_int replay_action = reginfo.replay_flags & 0x3;
    switch (replay_action) {
    case DTN_REPLAY_NEW:  r_action = Registration::NEW; break;
    case DTN_REPLAY_NONE: r_action = Registration::NONE;  break;
    case DTN_REPLAY_ALL:  r_action = Registration::ALL;  break;
    default: {
        log_err("invalid registration replay flags 0x%x", reginfo.replay_flags);
        return DTN_EINVAL;
    }
    }

    
    u_int32_t session_flags = 0;
    bool ack_delivery_flag = false;
    if (reginfo.flags & DTN_SESSION_CUSTODY) {
        session_flags |= Session::CUSTODY;
    }
    if (reginfo.flags & DTN_SESSION_SUBSCRIBE) {
        session_flags |= Session::SUBSCRIBE;
    }
    if (reginfo.flags & DTN_SESSION_PUBLISH) {
        session_flags |= Session::PUBLISH;
    }
    if (reginfo.flags & DTN_DELIVERY_ACKS) {
        ack_delivery_flag = true;
    }

    u_int other_flags = reginfo.flags & ~0x3f;
    if (other_flags != 0) {
        log_err("invalid registration flags 0x%x", reginfo.flags);
        return DTN_EINVAL;
    }

    if (f_action == Registration::EXEC) {
        script.assign(reginfo.script.script_val, reginfo.script.script_len);
    }

    u_int32_t regid = GlobalStore::instance()->next_regid();
    reg = new APIRegistration(regid, endpoint, f_action, r_action,
                              session_flags, reginfo.expiration,
                              ack_delivery_flag, reg_token, script);

    if (! reginfo.init_passive) {
        // store the registration in the list for this session
        bindings_->push_back(reg);
        reg->set_active(true);
    }

    if (session_flags & Session::CUSTODY) {
        sessions_->push_back(reg);
        ASSERT(reg->session_notify_list() != NULL);
    }
    
    BundleDaemon::post_and_wait(new RegistrationAddedEvent(reg, EVENTSRC_APP),
                                &notifier_);
    
    // fill the response with the new registration id
    if (!xdr_dtn_reg_id_t(&xdr_encode_, &regid)) {
        log_err("internal error in xdr: xdr_dtn_reg_id_t");
        return DTN_EXDR;
    }
    
    return DTN_SUCCESS;
}

//----------------------------------------------------------------------
int
APIClient::handle_unregister()
{
    Registration* reg;
    dtn_reg_id_t regid;
    
    // unpack and parse the request
    if (!xdr_dtn_reg_id_t(&xdr_decode_, &regid))
    {
        log_err("error in xdr unpacking arguments");
        return DTN_EXDR;
    }

    reg = BundleDaemon::instance()->reg_table()->get(regid);
    if (reg == NULL) {
        return DTN_ENOTFOUND;
    }

    // handle the special case in which we're unregistering a
    // currently bound registration, in which we actually leave it
    // around in the expired state, soit will be cleaned up when the
    // application either calls dtn_unbind() or closes the api socket
    if (is_bound(reg->regid()) && reg->active()) {
        if (reg->expired()) {
            return DTN_EINVAL;
        }
        
        reg->force_expire();
        ASSERT(reg->expired());
        return DTN_SUCCESS;
    }

    // otherwise it's an error to call unregister on a registration
    // that's in-use by someone else
    if (reg->active()) {
        return DTN_EBUSY;
    }

    BundleDaemon::post_and_wait(new RegistrationRemovedEvent(reg),
                                &notifier_);
    
    return DTN_SUCCESS;
}

//----------------------------------------------------------------------
int
APIClient::handle_find_registration2()
{
    Registration* reg;
    EndpointIDPattern endpoint;
    dtn_endpoint_id_t app_eid;
    dtn_reg_token_t reg_token;

    // unpack and parse the request
    if (!xdr_dtn_endpoint_id_t(&xdr_decode_, &app_eid))
    {
        log_err("error in xdr unpacking arguments");
        return DTN_EXDR;
    }
    if (!xdr_dtn_reg_token_t(&xdr_decode_, &reg_token))
    {
        log_err("error in xdr unpacking arguments");
        return DTN_EXDR;
    }

    endpoint.assign(&app_eid);
    if (!endpoint.valid()) {
        log_err("invalid endpoint id in find_registration: '%s'",
                app_eid.uri);
        return DTN_EINVAL;
    }

 
    u_int64_t regtoken;
    memcpy(&regtoken, &reg_token, sizeof(u_int64_t));

    reg = BundleDaemon::instance()->reg_table()->get(endpoint, regtoken);
    if (reg == NULL) {
        return DTN_ENOTFOUND;
    }

    u_int32_t regid = reg->regid();
    
    // fill the response with the new registration id
    if (!xdr_dtn_reg_id_t(&xdr_encode_, &regid)) {
        log_err("internal error in xdr: xdr_dtn_reg_id_t");
        return DTN_EXDR;
    }
    
    return DTN_SUCCESS;
}


//----------------------------------------------------------------------
int
APIClient::handle_find_registration()
{
    Registration* reg;
    EndpointIDPattern endpoint;
    dtn_endpoint_id_t app_eid;

    // unpack and parse the request
    if (!xdr_dtn_endpoint_id_t(&xdr_decode_, &app_eid))
    {
        log_err("error in xdr unpacking arguments");
        return DTN_EXDR;
    }

    endpoint.assign(&app_eid);
    if (!endpoint.valid()) {
        log_err("invalid endpoint id in find_registration: '%s'",
                app_eid.uri);
        return DTN_EINVAL;
    }

    reg = BundleDaemon::instance()->reg_table()->get(endpoint);
    if (reg == NULL) {
        return DTN_ENOTFOUND;
    }

    u_int32_t regid = reg->regid();
    
    // fill the response with the new registration id
    if (!xdr_dtn_reg_id_t(&xdr_encode_, &regid)) {
        log_err("internal error in xdr: xdr_dtn_reg_id_t");
        return DTN_EXDR;
    }
    
    return DTN_SUCCESS;
}

//----------------------------------------------------------------------
int
APIClient::handle_bind()
{
    dtn_reg_id_t regid;

    // unpack the request
    if (!xdr_dtn_reg_id_t(&xdr_decode_, &regid)) {
        log_err("error in xdr unpacking arguments");
        return DTN_EXDR;
    }

    // look up the registration
    const RegistrationTable* regtable = BundleDaemon::instance()->reg_table();
    Registration* reg = regtable->get(regid);

    if (!reg) {
        log_err("can't find registration %d", regid);
        return DTN_ENOTFOUND;
    }

    APIRegistration* api_reg = dynamic_cast<APIRegistration*>(reg);
    if (api_reg == NULL) {
        log_crit("registration %d is not an API registration!!",
                 regid);
        return DTN_ENOTFOUND;
    }

    if (api_reg->active()) {
        log_err("registration %d is already in active mode", regid);
        return DTN_EBUSY;
    }

    // store the registration in the list for this session
    bindings_->push_back(api_reg);
    api_reg->set_active(true);

    log_info("DTN_BIND: bound to registration %d", reg->regid());
    
    return DTN_SUCCESS;
}
    
//----------------------------------------------------------------------
int
APIClient::handle_unbind()
{
    dtn_reg_id_t regid;

    // unpack the request
    if (!xdr_dtn_reg_id_t(&xdr_decode_, &regid)) {
        log_err("error in xdr unpacking arguments");
        return DTN_EXDR;
    }

    // look up the registration
    const RegistrationTable* regtable = BundleDaemon::instance()->reg_table();
    Registration* reg = regtable->get(regid);

    if (!reg) {
        log_err("can't find registration %d", regid);
        return DTN_ENOTFOUND;
    }

    APIRegistration* api_reg = dynamic_cast<APIRegistration*>(reg);
    if (api_reg == NULL) {
        log_crit("registration %d is not an API registration!!",
                 regid);
        return DTN_ENOTFOUND;
    }

    APIRegistrationList::iterator iter;
    for (iter = bindings_->begin(); iter != bindings_->end(); ++iter) {
        if (*iter == api_reg) {
            bindings_->erase(iter);
            ASSERT(api_reg->active());
            api_reg->set_active(false);

            if (reg->expired()) {
                log_debug("removing expired registration %d", reg->regid());
                BundleDaemon::post(new RegistrationExpiredEvent(reg));
            }
            
            log_info("DTN_UNBIND: unbound from registration %d", regid);
            return DTN_SUCCESS;
        }
    }

    log_err("registration %d not bound to this api client", regid);
    return DTN_ENOTFOUND;
}
    
//----------------------------------------------------------------------
int
APIClient::handle_send()
{
    dtn_reg_id_t regid;
    dtn_bundle_spec_t spec;
    dtn_bundle_payload_t payload;

    memset(&spec, 0, sizeof(spec));
    memset(&payload, 0, sizeof(payload));
    
    /* Unpack the arguments */
    if (!xdr_dtn_reg_id_t(&xdr_decode_, &regid) ||
        !xdr_dtn_bundle_spec_t(&xdr_decode_, &spec) ||
        !xdr_dtn_bundle_payload_t(&xdr_decode_, &payload))
    {
        log_err("error in xdr unpacking arguments");
        return DTN_EXDR;
    }

    BundleRef b("APIClient::handle_send");
    b = new Bundle();
    
    // make sure any xdr calls to malloc are cleaned up
    oasys::ScopeXDRFree f1((xdrproc_t)xdr_dtn_bundle_spec_t,
                           (char*)&spec);
    oasys::ScopeXDRFree f2((xdrproc_t)xdr_dtn_bundle_payload_t,
                           (char*)&payload);
    
    // assign the addressing fields...

    // source  could be dtn:none; allow for a NULL URI to
    // be treated as dtn:none
    if (spec.source.uri[0] == '\0') {
        b->mutable_source()->assign(EndpointID::NULL_EID());
    } else {
        b->mutable_source()->assign(&spec.source);
    }

    // destination has to be specified
    b->mutable_dest()->assign(&spec.dest);

    // magic values for zeroing out creation timestamp time
    log_info("spec: %llu %llu", spec.creation_ts.secs, spec.creation_ts.seqno);
    if(spec.creation_ts.secs == 42 && 
       spec.creation_ts.seqno == 1337) {
        b->set_creation_ts(BundleTimestamp(0, b->creation_ts().seqno_));
    }

    // replyto defaults to null
    if (spec.replyto.uri[0] == '\0') {
        b->mutable_replyto()->assign(EndpointID::NULL_EID());
    } else {
        b->mutable_replyto()->assign(&spec.replyto);
    }

    // custodian is always null
    b->mutable_custodian()->assign(EndpointID::NULL_EID());

    // set the is_singleton bit, first checking if the application
    // specified a value, then seeing if the scheme is known and can
    // therefore determine for itself, and finally, checking the
    // global default
    if (spec.dopts & DOPTS_SINGLETON_DEST)
    {
        b->set_singleton_dest(true);
    }
    else if (spec.dopts & DOPTS_MULTINODE_DEST)
    {
        b->set_singleton_dest(false);
    }
    else 
    {
        EndpointID::singleton_info_t info;
        
        if (b->dest().known_scheme()) {
            info = b->dest().is_singleton();

            // all schemes must make a decision one way or the other
            ASSERT(info != EndpointID::UNKNOWN);
        } else {
            info = EndpointID::is_singleton_default_;
        }

        switch (info) {
        case EndpointID::UNKNOWN:
            log_err("bundle destination %s in unknown scheme and "
                    "app did not assert singleton/multipoint",
                    b->dest().c_str());
            return DTN_EINVAL;

        case EndpointID::SINGLETON:
            b->set_singleton_dest(true);
            break;

        case EndpointID::MULTINODE:
            b->set_singleton_dest(false);
            break;
        }
    }
    
    // the priority code
    switch (spec.priority) {
#define COS(_cos) case _cos: b->set_priority(Bundle::_cos); break;
        COS(COS_BULK);
        COS(COS_NORMAL);
        COS(COS_EXPEDITED);
        COS(COS_RESERVED);
#undef COS
    default:
        log_err("invalid priority level %d", (int)spec.priority);
        return DTN_EINVAL;
    };
    
    // The bundle's source EID must be either dtn:none or an EID
    // registered at this node so check that now.
    const RegistrationTable* reg_table = BundleDaemon::instance()->reg_table();
    RegistrationList unused;
    if (b->source() == EndpointID::NULL_EID())
    {
        // Bundles with a null source EID are not allowed to request reports or
        // custody transfer, and must not be fragmented.
        if ((spec.dopts & (DOPTS_CUSTODY | DOPTS_DELIVERY_RCPT |
                           DOPTS_RECEIVE_RCPT | DOPTS_FORWARD_RCPT |
                           DOPTS_CUSTODY_RCPT | DOPTS_DELETE_RCPT)) ||
            ((spec.dopts & DOPTS_DO_NOT_FRAGMENT)==0) ){
            log_err("bundle with null source EID requested report and/or "
                    "custody transfer and/or allowed fragmentation");
            return DTN_EINVAL;
        }

        b->set_do_not_fragment(true);
    }
    else if (reg_table->get_matching(b->source(), &unused) != 0)
    {
        // Local registration -- don't do anything
    }
    else if (b->source().subsume(BundleDaemon::instance()->local_eid()))
    {
        // Allow source EIDs that subsume the local eid
    }
    else
    {
        log_err("this node is not a member of the bundle's source EID (%s)",
                b->source().str().c_str());
        return DTN_EINVAL;
    }
    
    // Now look up the registration ID passed in to see if the bundle
    // was sent as part of a session
    Registration* reg = reg_table->get(regid);
    if (reg && reg->session_flags() != 0) {
        b->mutable_session_eid()->assign(reg->endpoint().str());
    }

    // delivery options
    if (spec.dopts & DOPTS_CUSTODY)
        b->set_custody_requested(true);
    
    if (spec.dopts & DOPTS_DELIVERY_RCPT)
        b->set_delivery_rcpt(true);

    if (spec.dopts & DOPTS_RECEIVE_RCPT)
        b->set_receive_rcpt(true);

    if (spec.dopts & DOPTS_FORWARD_RCPT)
        b->set_forward_rcpt(true);

    if (spec.dopts & DOPTS_CUSTODY_RCPT)
        b->set_custody_rcpt(true);

    if (spec.dopts & DOPTS_DELETE_RCPT)
        b->set_deletion_rcpt(true);

    if (spec.dopts & DOPTS_DO_NOT_FRAGMENT)
        b->set_do_not_fragment(true);

#if 0
    // expiration time
    struct timeval now;
    gettimeofday(&now, 0);
    u_int64_t temp = BundleTimestamp::TIMEVAL_CONVERSION +
        now.tv_sec +
        spec.expiration;

    if (temp>INT_MAX) {
        log_err("Bundle lifetime '%08lX' too large (>%08X)", temp, INT_MAX);
        log_err("BundleTimestamp::TIMEVAL_CONVERSTION is %08X",
                BundleTimestamp::TIMEVAL_CONVERSION);
        log_err("now.tv_sec is '%08X'", now.tv_sec);
        log_err("spec.expiration is %d", spec.expiration);
        return DTN_EINVAL;
    }
#endif

    b->set_expiration(spec.expiration);

    // sequence id and obsoletes id
    if (spec.sequence_id.data.data_len != 0)
    {
        std::string str(spec.sequence_id.data.data_val,
                        spec.sequence_id.data.data_len);
        
        bool ok = b->mutable_sequence_id()->parse(str);
        if (! ok) {
            log_err("invalid sequence id '%s'", str.c_str());
            return DTN_EINVAL;
        }
    }

    if (spec.obsoletes_id.data.data_len != 0)
    {
        std::string str(spec.obsoletes_id.data.data_val,
                        spec.obsoletes_id.data.data_len);
        
        bool ok = b->mutable_obsoletes_id()->parse(str);
        if (! ok) {
            log_err("invalid obsoletes id '%s'", str.c_str());
            return DTN_EINVAL;
        }
    }

    // extension blocks
    for (u_int i = 0; i < spec.blocks.blocks_len; i++) {
        dtn_extension_block_t* block = &spec.blocks.blocks_val[i];

        BlockProcessor* new_bp = BundleProtocol::find_processor(block->type);
        if (new_bp == UnknownBlockProcessor::instance()) {
        	new_bp = APIBlockProcessor::instance();
        	log_warn("sent bundle %d has block type %d not known by this daemon",
        			 b->bundleid(), block->type);
        }
        BlockInfo* info =
            b->api_blocks()->append_block(new_bp);
        new_bp->
            init_block(info,
					   b->api_blocks(),
					   b.object(),
                       block->type,
                       block->flags,
                       (u_char*)block->data.data_val,
                       block->data.data_len);
        info->set_complete(true);
    }

    // metadata blocks
    for (unsigned int i = 0; i < spec.metadata.metadata_len; ++i) {
        dtn_extension_block_t* block = &spec.metadata.metadata_val[i];

        LinkRef null_link("APIServer::handle_send");
        MetadataVec * vec = b->generated_metadata().find_blocks(null_link);
        if (vec == NULL) {
            vec = b->mutable_generated_metadata()->create_blocks(null_link);
        }
        ASSERT(vec != NULL);

        MetadataBlock * meta_block = new MetadataBlock(
                                             (u_int64_t)block->type,
                                             (u_char *)block->data.data_val,
                                             (u_int32_t)block->data.data_len);
        meta_block->set_flags((u_int64_t)block->flags);

        // XXX/demmer currently this block needs to be stuck on the
        // outgoing metadata for the null link (so it's transmit to
        // all destinations) as well as on the recv_metadata vector so
        // it's conveyed to local applications. this should really be
        // cleaned up...
        vec->push_back(meta_block);
        b->mutable_recv_metadata()->push_back(meta_block);

        // XXX/kscott This has to get put somewhere where it will be serialized
        // so that if it is delivered locally, it'll be available after a restart.
        // Need to prepend the metadata type SDNV (block->type) to the actual
        // content of the BlockInfo (difference between BlockInfo and MetadataBlock)
        {
        u_char temp[block->data.data_len+20];
        int curPos = 0;

        BlockProcessor *owner =
            BundleProtocol::find_processor(BundleProtocol::METADATA_BLOCK);
        BlockInfo* info =
            b->api_blocks()->append_block(owner);
        curPos += SDNV::encode(block->type, &(temp[curPos]), 1024);
        curPos += SDNV::encode(block->data.data_len, &(temp[curPos]), 1024);
        memcpy(&(temp[curPos]),
               (u_char*)block->data.data_val, block->data.data_len);

        APIBlockProcessor::instance()->
            init_block(info,
            		   b->api_blocks(),
            		   b.object(),
            		   BundleProtocol::METADATA_BLOCK,
            		   block->flags,
                       temp,
                       curPos+block->data.data_len);
        info->set_complete(true);
        }
    }

    // validate the bundle metadata
    oasys::StringBuffer error;
    if (!b->validate(&error)) {
        log_err("bundle validation failed: %s", error.data());
        return DTN_EINVAL;
    }
    
    // set up the payload, including calculating its length, but don't
    // copy it in yet
    size_t payload_len;
    char filename[PATH_MAX];

    switch (payload.location) {
    case DTN_PAYLOAD_MEM:
        payload_len = payload.buf.buf_len;
        break;
        
    case DTN_PAYLOAD_FILE:
    case DTN_PAYLOAD_TEMP_FILE:
        struct stat finfo;
        sprintf(filename, "%.*s", 
                (int)payload.filename.filename_len,
                payload.filename.filename_val);

        if (stat(filename, &finfo) != 0)
        {
            log_err("payload file %s does not exist!", filename);
            return DTN_EINVAL;
        }
        
        payload_len = finfo.st_size;
        break;

    default:
        log_err("payload.location of %d unknown", payload.location);
        return DTN_EINVAL;
    }
    
    b->mutable_payload()->set_length(payload_len);

    // before filling in the payload, we first probe the router to
    // determine if there's sufficient storage for the bundle
    bool result;
    int  reason;
    BundleDaemon::post_and_wait(
        new BundleAcceptRequest(b, EVENTSRC_APP, &result, &reason),
        &notifier_);

    if (!result) {
        log_info("DTN_SEND bundle not accepted: reason %s",
                 BundleStatusReport::reason_to_str(reason));

        switch (reason) {
        case BundleProtocol::REASON_DEPLETED_STORAGE:
            return DTN_ENOSPACE;
        default:
            return DTN_EINTERNAL;
        }
    }

    switch (payload.location) {
    case DTN_PAYLOAD_MEM:
        b->mutable_payload()->set_data((u_char*)payload.buf.buf_val,
                                       payload.buf.buf_len);
        break;
        
    case DTN_PAYLOAD_FILE:
        FILE* file;
        int r, left;
        u_char buffer[4096];
        size_t offset;

        if ((file = fopen(filename, "r")) == NULL)
        {
            log_err("payload file %s can't be opened: %s",
                    filename, strerror(errno));
            return DTN_EINVAL;
        }
        
        left = payload_len;
        r = 0;
        offset = 0;
        while (left > 0)
        {
            r = fread(buffer, 1, (left>4096)?4096:left, file);
            
            if (r)
            {
                b->mutable_payload()->write_data(buffer, offset, r);
                left   -= r;
                offset += r;
            }
            else
            {
                sleep(1); // pause before re-reading
            }
        }

        fclose(file);
        break;
        
    case DTN_PAYLOAD_TEMP_FILE:
        if (! b->mutable_payload()->replace_with_file(filename)) {
            log_err("payload file %s can't be linked or copied",
                    filename);
            return DTN_EINVAL;
        }
        
        if (::unlink(filename) != 0) {
            log_err("error unlinking payload temp file: %s",
                    strerror(errno));
            // continue on since this is non-fatal
        }
    }

    //  before posting the received event, fill in the bundle id struct
    dtn_bundle_id_t id;
    memcpy(&id.source, &spec.source, sizeof(dtn_endpoint_id_t));
    id.creation_ts.secs  = b->creation_ts().seconds_;
    id.creation_ts.seqno = b->creation_ts().seqno_;
    id.frag_offset = 0;
    id.orig_length = 0;
    
    log_info("DTN_SEND bundle *%p", b.object());

    // Sync the bundle payload to disk
    b->mutable_payload()->sync_payload();

    // deliver the bundle
    // Note: the bundle state may change once it has been posted
    BundleDaemon::post_and_wait(
        new BundleReceivedEvent(b.object(), EVENTSRC_APP),
        &notifier_);
    
    // return the bundle id struct
    if (!xdr_dtn_bundle_id_t(&xdr_encode_, &id)) {
        log_err("internal error in xdr: xdr_dtn_bundle_id_t");
        return DTN_EXDR;
    }
    
    return DTN_SUCCESS;
}

//----------------------------------------------------------------------
int
APIClient::handle_cancel()
{
    dtn_bundle_id_t id;

    memset(&id, 0, sizeof(id));
    
    /* Unpack the arguments */
    if (!xdr_dtn_bundle_id_t(&xdr_decode_, &id))
    {
        log_err("error in xdr unpacking arguments");
        return DTN_EXDR;
    }
    
    GbofId gbof_id;
    gbof_id.source_ = EndpointID( std::string(id.source.uri) );
    gbof_id.creation_ts_.seconds_ = id.creation_ts.secs;
    gbof_id.creation_ts_.seqno_ = id.creation_ts.seqno;
    gbof_id.is_fragment_ = (id.orig_length > 0);
    gbof_id.frag_length_ = id.orig_length;
    gbof_id.frag_offset_ = id.frag_offset;
    
    BundleRef bundle;
    oasys::ScopeLock pending_lock(
        BundleDaemon::instance()->pending_bundles()->lock(), "handle_cancel");
    bundle = BundleDaemon::instance()->pending_bundles()->find(gbof_id);
    
    if (!bundle.object()) {
        log_warn("no bundle matching [%s]; cannot cancel", 
                 gbof_id.str().c_str());
        return DTN_ENOTFOUND;
    }
    
    log_info("DTN_CANCEL bundle *%p", bundle.object());
    
    BundleDaemon::post(new BundleCancelRequest(bundle, std::string()));
    return DTN_SUCCESS;
}

// Size for temporary memory buffer used when delivering bundles
// via files.
#define DTN_FILE_DELIVERY_BUF_SIZE 1000

//----------------------------------------------------------------------
int
APIClient::handle_ack()
{
    dtn_bundle_spec_t             spec;

    // unpack the arguments
    memset(&spec, 0, sizeof(spec));
    
    /* Unpack the arguments */
    if (!xdr_dtn_bundle_spec_t(&xdr_decode_, &spec))
    {
        log_err("error in xdr unpacking arguments");
        return DTN_EXDR;
    }

    // make sure any xdr calls to malloc are cleaned up
    oasys::ScopeXDRFree f1((xdrproc_t)xdr_dtn_bundle_spec_t,
                           (char*)&spec);
    
    log_debug("APIClient::handle_ack");
    
    BundleDaemon::post(new BundleAckEvent(spec.delivery_regid,
                                          std::string(spec.source.uri),
                                          spec.creation_ts.secs,
                                          spec.creation_ts.seqno));

    return DTN_SUCCESS;
}

//----------------------------------------------------------------------
int
APIClient::handle_recv()
{
    dtn_bundle_spec_t             spec;
    dtn_bundle_payload_t          payload;
    dtn_bundle_payload_location_t location;
    dtn_bundle_status_report_t    status_report;
    dtn_timeval_t                 timeout;
    oasys::ScratchBuffer<u_char*> buf;
    APIRegistration*              reg = NULL;
    bool                          sock_ready = false;
    oasys::FileIOClient           tmpfile;

    // unpack the arguments
    if ((!xdr_dtn_bundle_payload_location_t(&xdr_decode_, &location)) ||
        (!xdr_dtn_timeval_t(&xdr_decode_, &timeout)))
    {
        log_err("error in xdr unpacking arguments");
        return DTN_EXDR;
    }
    
    int err = wait_for_notify("recv", timeout, &reg, NULL, &sock_ready);
    if (err != 0) {
        return err;
    }
    
    // if there's data on the socket, that either means the socket was
    // closed by an exiting application or the app is violating the
    // protocol...
    if (sock_ready) {
        return handle_unexpected_data("handle_recv");
    }

    ASSERT(reg != NULL);

    BundleRef bref("APIClient::handle_recv");
#if 0
    bref = reg->bundle_list()->pop_front();
    Bundle* b = bref.object();
    ASSERT(b != NULL);

    // Move the bundle to either the app unacked or acked list, depending
    // on whether the app is actively acking or not.
    reg->save(b);
#else
    // Pull the front bundle off the bundle_list and also move it to either
    // the app unacked or acked list, depending on whether the app is
    // actively acking or not.
    bref = reg->deliver_front();
    Bundle* b = bref.object();
    ASSERT(b != NULL);
#endif
    
    log_debug("handle_recv: popped *%p for registration %d (timeout %d)",
              b, reg->regid(), timeout);
    
    memset(&spec, 0, sizeof(spec));
    memset(&payload, 0, sizeof(payload));
    memset(&status_report, 0, sizeof(status_report));

    // copyto will malloc string buffer space that needs to be freed
    // at the end of the fn
    b->source().copyto(&spec.source);
    b->dest().copyto(&spec.dest);
    b->replyto().copyto(&spec.replyto);

    spec.dopts = 0;
    if (b->custody_requested()) spec.dopts |= DOPTS_CUSTODY;
    if (b->delivery_rcpt())     spec.dopts |= DOPTS_DELIVERY_RCPT;
    if (b->receive_rcpt())      spec.dopts |= DOPTS_RECEIVE_RCPT;
    if (b->forward_rcpt())      spec.dopts |= DOPTS_FORWARD_RCPT;
    if (b->custody_rcpt())      spec.dopts |= DOPTS_CUSTODY_RCPT;
    if (b->deletion_rcpt())     spec.dopts |= DOPTS_DELETE_RCPT;

    spec.expiration = b->expiration();
    spec.creation_ts.secs = b->creation_ts().seconds_;
    spec.creation_ts.seqno = b->creation_ts().seqno_;
    spec.delivery_regid = reg->regid();

    // copy out the sequence id and obsoletes id
    std::string sequence_id_str, obsoletes_id_str;
    if (! b->sequence_id().empty()) {
        sequence_id_str = b->sequence_id().to_str();
        spec.sequence_id.data.data_val = const_cast<char*>(sequence_id_str.c_str());
        spec.sequence_id.data.data_len = sequence_id_str.length();
    }

    if (! b->obsoletes_id().empty()) {
        obsoletes_id_str = b->obsoletes_id().to_str();
        spec.obsoletes_id.data.data_val = const_cast<char*>(obsoletes_id_str.c_str());
        spec.obsoletes_id.data.data_len = obsoletes_id_str.length();
    }

    // copy extension blocks from recv_blocks and api_blocks
    // Note that the data_length is the total length of the block
    // including the preamble and any EIDs associated with the block
    //
    unsigned int blocks_found = 0;
    unsigned int data_len = 0;
    for (unsigned int i = 0; i < b->recv_blocks().size(); ++i) {
        if ((b->recv_blocks()[i].type() == BundleProtocol::PRIMARY_BLOCK) ||
            (b->recv_blocks()[i].type() == BundleProtocol::PAYLOAD_BLOCK) ||
            (b->recv_blocks()[i].type() == BundleProtocol::METADATA_BLOCK)) {
            continue;
        }
        blocks_found++;
        data_len += b->recv_blocks()[i].data_length();
    }
    for (unsigned int i = 0; i < b->api_blocks_r().size(); ++i) {
        if ((b->api_blocks_r()[i].type() == BundleProtocol::PRIMARY_BLOCK) ||
            (b->api_blocks_r()[i].type() == BundleProtocol::PAYLOAD_BLOCK) ||
            (b->api_blocks_r()[i].type() == BundleProtocol::METADATA_BLOCK)) {
            continue;
        }
        blocks_found++;
        data_len += b->api_blocks_r()[i].data_length();
    }

    if (blocks_found > 0) {
        unsigned int buf_len = (blocks_found * sizeof(dtn_extension_block_t)) +
                               data_len;
        void * buf = malloc(buf_len);
        memset(buf, 0, buf_len);
        log_debug("Block found on recv_blocks");

        dtn_extension_block_t * bp = (dtn_extension_block_t *)buf;
        char * dp = (char*)buf + (blocks_found * sizeof(dtn_extension_block_t));
        for (unsigned int i = 0; i < b->recv_blocks().size(); ++i) {
            if ((b->recv_blocks()[i].type() == BundleProtocol::PRIMARY_BLOCK) ||
                (b->recv_blocks()[i].type() == BundleProtocol::PAYLOAD_BLOCK) ||
                (b->recv_blocks()[i].type() == BundleProtocol::METADATA_BLOCK)) {
                continue;
            }

            bp->type          = b->recv_blocks()[i].type();
            bp->flags         = b->recv_blocks()[i].flags();
            bp->data.data_len = b->recv_blocks()[i].data_length();
            bp->data.data_val = dp;
            memcpy(dp, b->recv_blocks()[i].data(), bp->data.data_len);
            if(bp->data.data_len < 120) {
                log_debug("Extension block data = %s", bp->data.data_val);
            } else {
                log_debug("Extension block data is longer than 120 bytes");
            }

            // These 2 lines were swapped before, causing problems with multiple extension blocks
            dp += bp->data.data_len;
            bp++;
        }
        for (unsigned int i = 0; i < b->api_blocks_r().size(); ++i) {
            if ((b->api_blocks_r()[i].type() == BundleProtocol::PRIMARY_BLOCK) ||
                (b->api_blocks_r()[i].type() == BundleProtocol::PAYLOAD_BLOCK) ||
                (b->api_blocks_r()[i].type() == BundleProtocol::METADATA_BLOCK)) {
                continue;
            }

            bp->type          = b->api_blocks_r()[i].type();
            bp->flags         = b->api_blocks_r()[i].flags();
            bp->data.data_len = b->api_blocks_r()[i].data_length();
            bp->data.data_val = dp;
            memcpy(dp, b->api_blocks_r()[i].data(), bp->data.data_len);

            // These 2 lines were swapped before, causing problems with multiple extension blocks
            dp += bp->data.data_len;
            bp++;
        }

        spec.blocks.blocks_len = blocks_found;
        spec.blocks.blocks_val = (dtn_extension_block_t *)buf;
    }

    // copy metadata extension blocks including ones generated for null link
    // This will include any blocks generated by the API.  There may be others
    // in future but not at present. [EBD - July 2012]
    blocks_found = 0;
    data_len = 0;
    LinkRef null_link("APIServer::handle_send");
    MetadataVec * vec = b->generated_metadata().find_blocks(null_link);

    for (unsigned int i = 0; i < b->recv_metadata().size(); ++i) {
        blocks_found++;
        data_len += b->recv_metadata()[i]->metadata_len();
    }
    if (vec != NULL) {
		for (unsigned int i = 0; i < vec->size(); ++i) {
			blocks_found++;
			data_len += (*vec)[i]->metadata_len();
		}
    }

    if (blocks_found > 0) {
        unsigned int buf_len = (blocks_found * sizeof(dtn_extension_block_t)) +
                               data_len;
        void * buf = (char *)malloc(buf_len);
        memset(buf, 0, buf_len);

        dtn_extension_block_t * bp = (dtn_extension_block_t *)buf;
        char * dp = (char*)buf + (blocks_found * sizeof(dtn_extension_block_t));
        for (unsigned int i = 0; i < b->recv_metadata().size(); ++i) {
            bp->type          = b->recv_metadata()[i]->ontology();
            bp->flags         = b->recv_metadata()[i]->flags();
            bp->data.data_len = b->recv_metadata()[i]->metadata_len();
            bp->data.data_val = dp;
            memcpy(dp, b->recv_metadata()[i]->metadata(), bp->data.data_len);
            dp += bp->data.data_len;
            bp++;
        }
        if (vec != NULL) {
			for (unsigned int i = 0; i < vec->size(); ++i) {
				bp->type          = (*vec)[i]->ontology();
				bp->flags         = (*vec)[i]->flags();
				bp->data.data_len = (*vec)[i]->metadata_len();
				bp->data.data_val = dp;
				memcpy(dp, (*vec)[i]->metadata(), bp->data.data_len);
				dp += bp->data.data_len;
				bp++;
			}
        }

        spec.metadata.metadata_len = blocks_found;
        spec.metadata.metadata_val = (dtn_extension_block_t *)buf;
    }

    size_t payload_len = b->payload().length();

    if (location == DTN_PAYLOAD_MEM && payload_len > DTN_MAX_BUNDLE_MEM)
    {
        log_debug("app requested memory delivery but payload is too big (%zu bytes)... "
                  "using files instead",
                  payload_len);
        location = DTN_PAYLOAD_FILE;
    }

    if (location == DTN_PAYLOAD_MEM) {
        // the app wants the payload in memory
        payload.buf.buf_len = payload_len;
        if (payload_len != 0) {
            buf.reserve(payload_len);
            payload.buf.buf_val =
                (char*)b->payload().read_data(0, payload_len, buf.buf());
        } else {
            payload.buf.buf_val = 0;
        }
        
    } else if (location == DTN_PAYLOAD_FILE) {
        const char *tdir;
        char templ[64];
        
        tdir = getenv("TMP");
        if (tdir == NULL) {
            tdir = getenv("TEMP");
        }
        if (tdir == NULL) {
            tdir = "/tmp";
        }
        
        snprintf(templ, sizeof(templ), "%s/bundlePayload_XXXXXX", tdir);

        if (tmpfile.mkstemp(templ) == -1) {
            log_err("can't open temporary file to deliver bundle");
            return DTN_EINTERNAL;
        }
        
        if (chmod(tmpfile.path(), 0666) < 0) {
            log_warn("can't set the permission of temp file to 0666: %s",
                     strerror(errno));
        }
        
        b->payload().copy_file(&tmpfile);

        payload.filename.filename_val = (char*)tmpfile.path();
        payload.filename.filename_len = tmpfile.path_len() + 1;
        tmpfile.close();
        
    } else {
        log_err("payload location %d not understood", location);
        return DTN_EINVAL;
    }

    payload.location = location;
    
    /*
     * If the bundle is a status report, parse it and copy out the
     * data into the status report.
     */
    BundleStatusReport::data_t sr_data;
    if (BundleStatusReport::parse_status_report(&sr_data, b))
    {
        payload.status_report = &status_report;
        sr_data.orig_source_eid_.copyto(&status_report.bundle_id.source);
        status_report.bundle_id.creation_ts.secs =
            sr_data.orig_creation_tv_.seconds_;
        status_report.bundle_id.creation_ts.seqno =
            sr_data.orig_creation_tv_.seqno_;
        status_report.bundle_id.frag_offset = sr_data.orig_frag_offset_;
        status_report.bundle_id.orig_length = sr_data.orig_frag_length_;

        status_report.reason = (dtn_status_report_reason_t)sr_data.reason_code_;
        status_report.flags =  (dtn_status_report_flags_t)sr_data.status_flags_;

        status_report.receipt_ts.secs     = sr_data.receipt_tv_.seconds_;
        status_report.receipt_ts.seqno    = sr_data.receipt_tv_.seqno_;
        status_report.custody_ts.secs     = sr_data.custody_tv_.seconds_;
        status_report.custody_ts.seqno    = sr_data.custody_tv_.seqno_;
        status_report.forwarding_ts.secs  = sr_data.forwarding_tv_.seconds_;
        status_report.forwarding_ts.seqno = sr_data.forwarding_tv_.seqno_;
        status_report.delivery_ts.secs    = sr_data.delivery_tv_.seconds_;
        status_report.delivery_ts.seqno   = sr_data.delivery_tv_.seqno_;
        status_report.deletion_ts.secs    = sr_data.deletion_tv_.seconds_;
        status_report.deletion_ts.seqno   = sr_data.deletion_tv_.seqno_;
        status_report.ack_by_app_ts.secs  = sr_data.ack_by_app_tv_.seconds_;
        status_report.ack_by_app_ts.seqno = sr_data.ack_by_app_tv_.seqno_;
    }
    
    if (!xdr_dtn_bundle_spec_t(&xdr_encode_, &spec))
    {
        log_err("internal error in xdr: xdr_dtn_bundle_spec_t");
        return DTN_EXDR;
    }
    
    if (!xdr_dtn_bundle_payload_t(&xdr_encode_, &payload))
    {
        log_err("internal error in xdr: xdr_dtn_bundle_payload_t");
        return DTN_EXDR;
    }

    // prevent xdr_free of non-malloc'd pointer
    payload.status_report = NULL;
    
    log_info("DTN_RECV: "
             "successfully delivered bundle %d to registration %d",
             b->bundleid(), reg->regid());
    
    BundleDaemon::post(new BundleDeliveredEvent(b, reg));

    return DTN_SUCCESS;
}


//----------------------------------------------------------------------
int
APIClient::handle_peek()
{
    dtn_bundle_spec_t             spec;
    dtn_bundle_payload_t          payload;
    dtn_bundle_payload_location_t location;
    dtn_bundle_status_report_t    status_report;
    dtn_timeval_t                 timeout;
    oasys::ScratchBuffer<u_char*> buf;
    APIRegistration*              reg = NULL;
    bool                          sock_ready = false;
    oasys::FileIOClient           tmpfile;

    // unpack the arguments
    if ((!xdr_dtn_bundle_payload_location_t(&xdr_decode_, &location)) ||
        (!xdr_dtn_timeval_t(&xdr_decode_, &timeout)))
    {
        log_err("error in xdr unpacking arguments");
        return DTN_EXDR;
    }
    
    int err = wait_for_notify("recv", timeout, &reg, NULL, &sock_ready);
    if (err != 0) {
        return err;
    }
    
    // if there's data on the socket, that either means the socket was
    // closed by an exiting application or the app is violating the
    // protocol...
    if (sock_ready) {
        return handle_unexpected_data("handle_recv");
    }

    ASSERT(reg != NULL);

    BundleRef bref("APIClient::handle_recv");
    bref = reg->bundle_list()->pop_front();
    Bundle* b = bref.object();
    ASSERT(b != NULL);
    
    log_debug("handle_recv: popped *%p for registration %d (timeout %d)",
              b, reg->regid(), timeout);
    
    memset(&spec, 0, sizeof(spec));
    memset(&payload, 0, sizeof(payload));
    memset(&status_report, 0, sizeof(status_report));

    // copyto will malloc string buffer space that needs to be freed
    // at the end of the fn
    b->source().copyto(&spec.source);
    b->dest().copyto(&spec.dest);
    b->replyto().copyto(&spec.replyto);

    spec.dopts = 0;
    if (b->custody_requested()) spec.dopts |= DOPTS_CUSTODY;
    if (b->delivery_rcpt())     spec.dopts |= DOPTS_DELIVERY_RCPT;
    if (b->receive_rcpt())      spec.dopts |= DOPTS_RECEIVE_RCPT;
    if (b->forward_rcpt())      spec.dopts |= DOPTS_FORWARD_RCPT;
    if (b->custody_rcpt())      spec.dopts |= DOPTS_CUSTODY_RCPT;
    if (b->deletion_rcpt())     spec.dopts |= DOPTS_DELETE_RCPT;

    spec.expiration = b->expiration();
    spec.creation_ts.secs = b->creation_ts().seconds_;
    spec.creation_ts.seqno = b->creation_ts().seqno_;
    spec.delivery_regid = reg->regid();

    // copy out the sequence id and obsoletes id
    std::string sequence_id_str, obsoletes_id_str;
    if (! b->sequence_id().empty()) {
        sequence_id_str = b->sequence_id().to_str();
        spec.sequence_id.data.data_val = const_cast<char*>(sequence_id_str.c_str());
        spec.sequence_id.data.data_len = sequence_id_str.length();
    }

    if (! b->obsoletes_id().empty()) {
        obsoletes_id_str = b->obsoletes_id().to_str();
        spec.obsoletes_id.data.data_val = const_cast<char*>(obsoletes_id_str.c_str());
        spec.obsoletes_id.data.data_len = obsoletes_id_str.length();
    }

    // copy extension blocks
    unsigned int blocks_found = 0;
    unsigned int data_len = 0;
    for (unsigned int i = 0; i < b->recv_blocks().size(); ++i) {
        if ((b->recv_blocks()[i].type() == BundleProtocol::PRIMARY_BLOCK) ||
            (b->recv_blocks()[i].type() == BundleProtocol::PAYLOAD_BLOCK) ||
            (b->recv_blocks()[i].type() == BundleProtocol::METADATA_BLOCK)) {
            continue;
        }
        blocks_found++;
        data_len += b->recv_blocks()[i].data_length();
    }

    if (blocks_found > 0) {
        unsigned int buf_len = (blocks_found * sizeof(dtn_extension_block_t)) +
                               data_len;
        void * buf = malloc(buf_len);
        memset(buf, 0, buf_len);

        dtn_extension_block_t * bp = (dtn_extension_block_t *)buf;
        char * dp = (char*)buf + (blocks_found * sizeof(dtn_extension_block_t));
        for (unsigned int i = 0; i < b->recv_blocks().size(); ++i) {
            if ((b->recv_blocks()[i].type() == BundleProtocol::PRIMARY_BLOCK) ||
                (b->recv_blocks()[i].type() == BundleProtocol::PAYLOAD_BLOCK) ||
                (b->recv_blocks()[i].type() == BundleProtocol::METADATA_BLOCK)) {
                continue;
            }

            bp->type          = b->recv_blocks()[i].type();
            bp->flags         = b->recv_blocks()[i].flags();
            bp->data.data_len = b->recv_blocks()[i].data_length();
            bp->data.data_val = dp;
            memcpy(dp, b->recv_blocks()[i].data(), bp->data.data_len);

            if(bp->data.data_len < 120) {
                log_debug("Extension block data = %s", bp->data.data_val);
            } else {
                log_debug("Extension block data is longer than 120 bytes");
            }

            // These 2 lines were swapped before, causing problems with multiple extension blocks
            dp += bp->data.data_len;
            bp++;
        }

        spec.blocks.blocks_len = blocks_found;
        spec.blocks.blocks_val = (dtn_extension_block_t *)buf;
    }

    // copy metadata extension blocks
    blocks_found = 0;
    data_len = 0;
    for (unsigned int i = 0; i < b->recv_metadata().size(); ++i) {
        blocks_found++;
        data_len += b->recv_metadata()[i]->metadata_len();
    }

    if (blocks_found > 0) {
        unsigned int buf_len = (blocks_found * sizeof(dtn_extension_block_t)) +
                               data_len;
        void * buf = (char *)malloc(buf_len);
        memset(buf, 0, buf_len);

        dtn_extension_block_t * bp = (dtn_extension_block_t *)buf;
        char * dp = (char*)buf + (blocks_found * sizeof(dtn_extension_block_t));
        for (unsigned int i = 0; i < b->recv_metadata().size(); ++i) {
            bp->type          = b->recv_metadata()[i]->ontology();
            bp->flags         = b->recv_metadata()[i]->flags();
            bp->data.data_len = b->recv_metadata()[i]->metadata_len();
            bp->data.data_val = dp;
            memcpy(dp, b->recv_metadata()[i]->metadata(), bp->data.data_len);
            dp += bp->data.data_len;
            bp++;
        }

        spec.metadata.metadata_len = blocks_found;
        spec.metadata.metadata_val = (dtn_extension_block_t *)buf;
    }

    size_t payload_len = b->payload().length();

    if (location == DTN_PAYLOAD_MEM && payload_len > DTN_MAX_BUNDLE_MEM)
    {
        log_debug("app requested memory delivery but payload is too big (%zu bytes)... "
                  "using files instead",
                  payload_len);
        location = DTN_PAYLOAD_FILE;
    }

    if (location == DTN_PAYLOAD_MEM) {
        // the app wants the payload in memory
        payload.buf.buf_len = payload_len;
        if (payload_len != 0) {
            buf.reserve(payload_len);
            payload.buf.buf_val =
                (char*)b->payload().read_data(0, payload_len, buf.buf());
        } else {
            payload.buf.buf_val = 0;
        }
        
    } else if (location == DTN_PAYLOAD_FILE) {
        const char *tdir;
        char templ[64];
        
        tdir = getenv("TMP");
        if (tdir == NULL) {
            tdir = getenv("TEMP");
        }
        if (tdir == NULL) {
            tdir = "/tmp";
        }
        
        snprintf(templ, sizeof(templ), "%s/bundlePayload_XXXXXX", tdir);

        if (tmpfile.mkstemp(templ) == -1) {
            log_err("can't open temporary file to deliver bundle");
            return DTN_EINTERNAL;
        }
        
        if (chmod(tmpfile.path(), 0666) < 0) {
            log_warn("can't set the permission of temp file to 0666: %s",
                     strerror(errno));
        }
        
        b->payload().copy_file(&tmpfile);

        payload.filename.filename_val = (char*)tmpfile.path();
        payload.filename.filename_len = tmpfile.path_len() + 1;
        tmpfile.close();
        
    } else {
        log_err("payload location %d not understood", location);
        return DTN_EINVAL;
    }

    payload.location = location;
    
    /*
     * If the bundle is a status report, parse it and copy out the
     * data into the status report.
     */
    BundleStatusReport::data_t sr_data;
    if (BundleStatusReport::parse_status_report(&sr_data, b))
    {
        payload.status_report = &status_report;
        sr_data.orig_source_eid_.copyto(&status_report.bundle_id.source);
        status_report.bundle_id.creation_ts.secs =
            sr_data.orig_creation_tv_.seconds_;
        status_report.bundle_id.creation_ts.seqno =
            sr_data.orig_creation_tv_.seqno_;
        status_report.bundle_id.frag_offset = sr_data.orig_frag_offset_;
        status_report.bundle_id.orig_length = sr_data.orig_frag_length_;

        status_report.reason = (dtn_status_report_reason_t)sr_data.reason_code_;
        status_report.flags =  (dtn_status_report_flags_t)sr_data.status_flags_;

        status_report.receipt_ts.secs     = sr_data.receipt_tv_.seconds_;
        status_report.receipt_ts.seqno    = sr_data.receipt_tv_.seqno_;
        status_report.custody_ts.secs     = sr_data.custody_tv_.seconds_;
        status_report.custody_ts.seqno    = sr_data.custody_tv_.seqno_;
        status_report.forwarding_ts.secs  = sr_data.forwarding_tv_.seconds_;
        status_report.forwarding_ts.seqno = sr_data.forwarding_tv_.seqno_;
        status_report.delivery_ts.secs    = sr_data.delivery_tv_.seconds_;
        status_report.delivery_ts.seqno   = sr_data.delivery_tv_.seqno_;
        status_report.deletion_ts.secs    = sr_data.deletion_tv_.seconds_;
        status_report.deletion_ts.seqno   = sr_data.deletion_tv_.seqno_;
        status_report.ack_by_app_ts.secs  = sr_data.ack_by_app_tv_.seconds_;
        status_report.ack_by_app_ts.seqno = sr_data.ack_by_app_tv_.seqno_;
    }
    
    if (!xdr_dtn_bundle_spec_t(&xdr_encode_, &spec))
    {
        log_err("internal error in xdr: xdr_dtn_bundle_spec_t");
        return DTN_EXDR;
    }
    
    if (!xdr_dtn_bundle_payload_t(&xdr_encode_, &payload))
    {
        log_err("internal error in xdr: xdr_dtn_bundle_payload_t");
        return DTN_EXDR;
    }

    // prevent xdr_free of non-malloc'd pointer
    payload.status_report = NULL;
    
    log_info("DTN_PEEK: "
             "successfully delivered bundle %d to registration %d",
             b->bundleid(), reg->regid());

    return DTN_SUCCESS;
}


//----------------------------------------------------------------------
int
APIClient::handle_begin_poll()
{
    dtn_timeval_t    timeout;
    APIRegistration* recv_reg = NULL;
    APIRegistration* notify_reg = NULL;
    bool             sock_ready = false;
    
    log_debug("handle_begin_poll: entering");
    
    // unpack the arguments
    if ((!xdr_dtn_timeval_t(&xdr_decode_, &timeout)))
    {
        log_err("error in xdr unpacking arguments");
        return DTN_EXDR;
    }

    int err = wait_for_notify("poll", timeout, &recv_reg, &notify_reg,
                             &sock_ready);
    log_debug("handle_begin_poll: wait_for_notify returns sock_ready(%d)\n",
              sock_ready);

    if (err != 0) {
        return err;
    }

    // if there's data on the socket, then the application either quit
    // and closed the socket, or called dtn_poll_cancel
    if (sock_ready) {
        log_debug("handle_begin_poll: "
                  "api socket ready -- trying to read one byte");
        char type;
        
        int ret = read(&type, 1);
        log_debug("handle_begin_poll: read one byte: %d", ret);
        if (ret == 0) {
            log_info("IPC socket closed while blocked in read... "
                     "application must have exited");
            return -1;
        }

        if (ret == -1) {
            log_err("handle_begin_poll: protocol error -- "
                    "error while blocked in poll");
            return DTN_ECOMM;
        }

        if (type != DTN_CANCEL_POLL) {
            log_debug("handle_begin_poll: DTN_CANCEL_POLL");
            log_err("handle_poll: error got unexpected message '%s' "
                    "while blocked in poll", dtnipc_msgtoa(type));
            return DTN_ECOMM;
        }

        // read in the length which must be zero
        u_int32_t len;
        ret = read((char*)&len, 4);
        if (ret != 4 || len != 0) {
            log_err("handle_begin_poll: protocol error -- "
                    "error getting cancel poll length");
            return DTN_ECOMM;
        }

        total_rcvd_ += 5;

        log_debug("got DTN_CANCEL_POLL while blocked in poll");
        // immediately send the response to the poll cancel, then
        // we return from the handler which will follow it with the
        // response code to the original poll request
        send_response(DTN_SUCCESS);
    } else if (recv_reg != NULL) {
        log_debug("handle_begin_poll: bundle arrived");

    } else if (notify_reg != NULL) {
        log_debug("handle_begin_poll: subscriber notify arrived");

    } else {
        // wait_for_notify must have returned one of the above cases
        NOTREACHED;
    }

    return DTN_SUCCESS;
}

//----------------------------------------------------------------------
int
APIClient::handle_cancel_poll()
{
    // the only reason we should get in here is if the call to
    // dtn_begin_poll() returned but the app still called cancel_poll
    // and so the messages crossed. but, since there's nothing wrong
    // with this, we just return success in both cases
    
    return DTN_SUCCESS;
}

//----------------------------------------------------------------------
int
APIClient::handle_close()
{
    log_info("received DTN_CLOSE message; closing API handle");
    // return -1 to force the session to close:
    return -1;
}

//----------------------------------------------------------------------
int
APIClient::handle_session_update()
{
    APIRegistration* reg = NULL;
    bool             sock_ready = false;
    dtn_timeval_t    timeout;

    // unpack the arguments
    if ((!xdr_dtn_timeval_t(&xdr_decode_, &timeout)))
    {
        log_err("error in xdr unpacking arguments");
        return DTN_EXDR;
    }
    
    int err = wait_for_notify("session_update", timeout, NULL, &reg,
                              &sock_ready);
    if (err != 0) {
        return err;
    }
    
    // if there's data on the socket, that either means the socket was
    // closed by an exiting application or the app is violating the
    // protocol...
    if (sock_ready) {
        return handle_unexpected_data("handle_session_update");
    }

    ASSERT(reg != NULL);
    
    BundleRef bref("APIClient::handle_session_update");
    bref = reg->session_notify_list()->pop_front();
    Bundle* b = bref.object();
    ASSERT(b != NULL);
    
    log_debug("handle_session_update: "
              "popped *%p for registration %d (timeout %d)",
              b, reg->regid(), timeout);

    
    ASSERT(b->session_flags() != 0);

    unsigned int session_flags = 0;
    if (b->session_flags() & Session::SUBSCRIBE) {
        session_flags |= DTN_SESSION_SUBSCRIBE;
    }
    // XXX/demmer what to do about UNSUBSCRIBE/PUBLISH??

    dtn_endpoint_id_t session_eid;
    b->session_eid().copyto(&session_eid);
    
    if (!xdr_u_int(&xdr_encode_, &session_flags) ||
        !xdr_dtn_endpoint_id_t(&xdr_encode_, &session_eid))
    {
        log_err("internal error in xdr");
        return DTN_EXDR;
    }
    
    log_info("session_update: "
             "notification for session %s status %s",
             b->session_eid().c_str(), Session::flag_str(b->session_flags()));

    BundleDaemon::post(new BundleDeliveredEvent(b, reg));

    return DTN_SUCCESS;
}

//----------------------------------------------------------------------
int
APIClient::wait_for_notify(const char*       operation,
                           dtn_timeval_t     dtn_timeout,
                           APIRegistration** recv_ready_reg,
                           APIRegistration** session_ready_reg,
                           bool*             sock_ready)
{
    APIRegistration* reg;

    ASSERT(sock_ready != NULL);
    if (recv_ready_reg)    *recv_ready_reg    = NULL;
    if (session_ready_reg) *session_ready_reg = NULL;

    if (bindings_->empty()) {
        log_err("wait_for_notify(%s): no bound registrations", operation);
        return DTN_EINVAL;
    }

    int timeout = (int)dtn_timeout;
    if (timeout < -1) {
        log_err("wait_for_notify(%s): "
                "invalid timeout value %d", operation, timeout);
        return DTN_EINVAL;
    }

    // try to optimize by using a statically sized pollfds array,
    // otherwise we need to malloc the array.
    //
    // XXX/demmer this would be cleaner by tweaking the
    // StaticScratchBuffer class to be handle arrays of arbitrary
    // sized structs
    struct pollfd static_pollfds[64];
    struct pollfd* pollfds;
    oasys::ScopeMalloc pollfd_malloc;
    size_t npollfds = 1;
    if (recv_ready_reg)    npollfds += bindings_->size();
    if (session_ready_reg) npollfds += sessions_->size();
    
    if (npollfds <= 64) {
        pollfds = &static_pollfds[0];
    } else {
        pollfds = (struct pollfd*)malloc(npollfds * sizeof(struct pollfd));
        pollfd_malloc = pollfds;
    }
    
    struct pollfd* sock_poll = &pollfds[0];
    sock_poll->fd            = TCPClient::fd_;
    sock_poll->events        = POLLIN | POLLERR;
    sock_poll->revents       = 0;

    // loop through all the registrations -- if one has bundles on its
    // list, we don't need to poll, just return it immediately.
    // otherwise we'll need to poll it
    APIRegistrationList::iterator iter;
    unsigned int i = 1;
    if (recv_ready_reg) {
        log_debug("wait_for_notify(%s): checking %zu bindings",
                  operation, bindings_->size());
        
        for (iter = bindings_->begin(); iter != bindings_->end(); ++iter) {
            reg = *iter;
            
            if (! reg->bundle_list()->empty()) {
                log_debug("wait_for_notify(%s): "
                          "immediately returning bundle for reg %d",
                          operation, reg->regid());
                *recv_ready_reg = reg;
                return 0;
            }
        
            pollfds[i].fd = reg->bundle_list()->notifier()->read_fd();
            pollfds[i].events = POLLIN;
            pollfds[i].revents = 0;
            ++i;
            ASSERT(i <= npollfds);
        }
    }

    // ditto for sessions
    if (session_ready_reg) {
        log_debug("wait_for_notify(%s): checking %zu sessions",
                  operation, sessions_->size());
    
        for (iter = sessions_->begin(); iter != sessions_->end(); ++iter)
        {
            reg = *iter;
            ASSERT(reg->session_notify_list() != NULL);
            if (! reg->session_notify_list()->empty()) {
                log_debug("wait_for_notify(%s): "
                          "immediately returning notified reg %d",
                          operation, reg->regid());
                *session_ready_reg = reg;
                return 0;
            }

            pollfds[i].fd = reg->session_notify_list()->notifier()->read_fd();
            pollfds[i].events = POLLIN;
            pollfds[i].revents = 0;
            ++i;
            ASSERT(i <= npollfds);
        }
    }

    if (timeout == 0) {
        log_debug("wait_for_notify(%s): "
                  "no ready registrations and timeout=%d, returning immediately",
                  operation, timeout);
        return DTN_ETIMEOUT;
    }
    
    log_debug("wait_for_notify(%s): "
              "blocking to get events from %zu sources (timeout %d)",
              operation, npollfds, timeout);
    int nready = oasys::IO::poll_multiple(&pollfds[0], npollfds, timeout,
                                          NULL, logpath_);

    if (nready == oasys::IOTIMEOUT) {
        log_debug("wait_for_notify(%s): timeout waiting for events",
                  operation);
        return DTN_ETIMEOUT;

    } else if (nready <= 0) {
        log_err("wait_for_notify(%s): unexpected error polling for events",
                operation);
        return DTN_EINTERNAL;
    }

    // if there's data on the socket, immediately exit without
    // checking the registrations
    if (sock_poll->revents != 0) {
        log_debug("wait_for_notify(%s) sock_poll->revents!=0", operation);
        *sock_ready = true;
        return 0;
    }

    // otherwise, there should be data on one (or more) bundle lists, so
    // scan the list to find the first one.
    log_debug("wait_for_notify: looking for data on bundle lists: recv_ready_reg(%p)\n",
              recv_ready_reg);
    if (recv_ready_reg) {
        for (iter = bindings_->begin(); iter != bindings_->end(); ++iter) {
            reg = *iter;
            if (! reg->bundle_list()->empty()) {
                log_debug("wait_for_notify: found one %p", reg);
                *recv_ready_reg = reg;
                break;
            }
        }
    }

    if (session_ready_reg) {
        for (iter = sessions_->begin(); iter != sessions_->end(); ++iter)
        {
            reg = *iter;
            if (! reg->session_notify_list()->empty()) {
                *session_ready_reg = reg;
                break;
            }
        }
    }

    if ((recv_ready_reg    && *recv_ready_reg    == NULL) &&
        (session_ready_reg && *session_ready_reg == NULL))
    {
        log_err("wait_for_notify(%s): error -- no lists have any events",
                operation);
        return DTN_EINTERNAL;
    }
    
    return 0;
}

//----------------------------------------------------------------------
int
APIClient::handle_unexpected_data(const char* operation)
{
    log_debug("%s: api socket ready -- trying to read one byte",
              operation);
    char b;
    if (read(&b, 1) != 0) {
        log_err("%s: protocol error -- "
                "data arrived or error while blocked in recv",
                operation);
        return DTN_ECOMM;
    }

    log_info("IPC socket closed while blocked in read... "
             "application must have exited");
    return -1;
}

} // namespace dtn
