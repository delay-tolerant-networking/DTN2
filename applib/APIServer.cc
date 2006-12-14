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


#include <sys/stat.h>
#include <oasys/compat/inet_aton.h>
#include <oasys/io/FileIOClient.h>
#include <oasys/io/NetUtils.h>
#include <oasys/util/Pointers.h>
#include <oasys/util/ScratchBuffer.h>
#include <oasys/util/XDRUtils.h>

#include "APIServer.h"
#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleStatusReport.h"
#include "cmd/APICommand.h"
#include "reg/APIRegistration.h"
#include "reg/RegistrationTable.h"
#include "routing/BundleRouter.h"
#include "storage/GlobalStore.h"

#ifndef MIN
#define MIN(x, y) ((x)<(y) ? (x) : (y))
#endif


#ifdef __CYGWIN__
// Cygwin's xdr.h file is k&r, so we need to make the declarations
// more specific here to avoid errors when compiling with g++ instead
// of gcc.

extern "C" {
    extern void xdrmem_create(XDR *__xdrs, __const caddr_t __addr,
                              u_int __size, enum xdr_op __xop);
}

// these defines add a cast to change the function pointer for a function
// with no args (which we get from xdr.h) into a function pointer with
// args (i.e. k&r to ansi c).

typedef void (*xdr_setpos_t)(XDR *, int);
#undef xdr_setpos
#define xdr_setpos(xdrs, pos) ((xdr_setpos_t)(*(xdrs)->x_ops->x_setpostn))(xdrs, pos)

typedef int (*xdr_getpos_t)(XDR *);
#undef xdr_getpos
#define xdr_getpos(xdrs) ((xdr_getpos_t)(*(xdrs)->x_ops->x_getpostn))(xdrs)

typedef int (*xdr_putlong_t)(XDR *, long *);
#undef xdr_putlong
#define xdr_putlong(xdrs, ptr) ((xdr_putlong_t)(*(xdrs)->x_ops->x_putlong))(xdrs, ptr)

#endif

namespace dtn {

//----------------------------------------------------------------------
APIServer::APIServer()
    : TCPServerThread("APIServer", "/dtn/apiserver", DELETE_ON_EXIT)
{
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
    APIClient* c = new APIClient(fd, addr, port);
    c->start();
}

//----------------------------------------------------------------------
APIClient::APIClient(int fd, in_addr_t addr, u_int16_t port)
    : Thread("APIClient", DELETE_ON_EXIT),
      TCPClient(fd, addr, port, "/dtn/apiclient"),
      notifier_(logpath_)
{
    // note that we skip space for the message length and code/status
    xdrmem_create(&xdr_encode_, buf_ + 8, DTN_MAX_API_MSG - 8, XDR_ENCODE);
    xdrmem_create(&xdr_decode_, buf_ + 8, DTN_MAX_API_MSG - 8, XDR_DECODE);

    bindings_ = new APIRegistrationList();
}

//----------------------------------------------------------------------
APIClient::~APIClient()
{
    log_debug("client destroyed");
    delete_z(bindings_);
}

//----------------------------------------------------------------------
void
APIClient::close_session()
{
    TCPClient::close();

    APIRegistration* reg;
    while (! bindings_->empty()) {
        reg = bindings_->front();
        bindings_->pop_front();
        
        reg->set_active(false);

        if (reg->expired()) {
            log_debug("removing expired registration %d", reg->regid());
            BundleDaemon::post(new RegistrationExpiredEvent(reg->regid()));
        }
    }
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

    message_type = ntohl(handshake) >> 16;
    ipc_version = (u_int16_t) (ntohl(handshake) & 0x0ffff);

    if (message_type != DTN_OPEN) {
        log_err("handshake (%d)'s message type %d != DTN_OPEN (%d)",
                handshake, message_type, DTN_OPEN);
        return -1;
    }
    
    if (ipc_version != DTN_IPC_VERSION) {
        log_err("handshake (%d)'s version %d != DTN_IPC_VERSION (%d)",
                handshake, ipc_version, DTN_IPC_VERSION);
        return -1;
    }
    
    ret = writeall((char*)&handshake, sizeof(handshake));
    if (ret != sizeof(handshake)) {
        log_err("error writing handshake: %s", strerror(errno));
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
        close_session();
        return;
    }
    
    while (true) {
        xdr_setpos(&xdr_encode_, 0);
        xdr_setpos(&xdr_decode_, 0);

        // read the incoming message into the fourth byte of the
        // buffer, since the typecode + message length is only five
        // bytes long, but the XDR engines are set to point at the
        // eighth byte of the buffer
        ret = read(&buf_[3], DTN_MAX_API_MSG);
            
        if (ret <= 0) {
            log_warn("client error or disconnection");
            close_session();
            return;
        }
        
        if (ret < 5) {
            log_err("ack!! can't handle really short read...");
            close_session();
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
            if (readall(&buf_[8 + ret], toget) != toget) {
                log_err("error reading message remainder: %s",
                        strerror(errno));
                close_session();
                return;
            }
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
            DISPATCH(DTN_SEND,              handle_send);
            DISPATCH(DTN_BIND,              handle_bind);
            DISPATCH(DTN_UNBIND,            handle_unbind);
            DISPATCH(DTN_RECV,              handle_recv);
            DISPATCH(DTN_BEGIN_POLL,        handle_begin_poll);
            DISPATCH(DTN_CANCEL_POLL,       handle_cancel_poll);
            DISPATCH(DTN_CLOSE,             handle_close);
#undef DISPATCH

        default:
            log_err("unknown message type code 0x%x", type);
            ret = DTN_EMSGTYPE;
            break;
        }

        // if the handler returned -1, then the session should be
        // immediately terminated
        if (ret == -1) {
            close_session();
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
            close_session();
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
              dtnipc_msgtoa(ret), len);

    msglen = len + 8;
    ret = ntohl(ret);
    len = htonl(len);

    memcpy(buf_,     &ret, sizeof(ret));
    memcpy(&buf_[4], &len, sizeof(len));

    log_debug("sending %d byte reply message", msglen);
    if (writeall(buf_, msglen) != (int)msglen) {
        log_err("error sending reply: %s", strerror(errno));
        close_session();
        return -1;
    }

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
    Registration::failure_action_t action;
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
    
    switch (reginfo.failure_action) {
    case DTN_REG_DEFER: action = Registration::DEFER; break;
    case DTN_REG_DROP:  action = Registration::DROP;  break;
    case DTN_REG_EXEC:  action = Registration::EXEC;  break;
    default: {
        log_err("invalid failure action code 0x%x", reginfo.failure_action);
        return DTN_EINVAL;
    }
    }

    if (action == Registration::EXEC) {
        script.assign(reginfo.script.script_val, reginfo.script.script_len);
    }

    u_int32_t regid = GlobalStore::instance()->next_regid();
    reg = new APIRegistration(regid, endpoint, action,
                              reginfo.expiration, script);

    if (! reginfo.init_passive) {
        // XXX/demmer fixme to allow multiple registrations
        if (! bindings_->empty()) {
            log_err("error: handle already bound to a registration");
            return DTN_EBUSY;
        }
        
        // store the registration in the list for this session
        bindings_->push_back(reg);
        reg->set_active(true);
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

    // XXX/demmer fixme to allow multiple registrations
    if (! bindings_->empty()) {
        log_err("error: handle already bound to a registration");
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
    BundleRef b;
    dtn_bundle_spec_t spec;
    dtn_bundle_payload_t payload;

    memset(&spec, 0, sizeof(spec));
    memset(&payload, 0, sizeof(payload));
    
    /* Unpack the arguments */
    if (!xdr_dtn_bundle_spec_t(&xdr_decode_, &spec) ||
        !xdr_dtn_bundle_payload_t(&xdr_decode_, &payload))
    {
        log_err("error in xdr unpacking arguments");
        return DTN_EXDR;
    }

    b = new Bundle();

    // make sure the xdr unpacked call to malloc is cleaned up
    oasys::ScopeMalloc m((payload.location == DTN_PAYLOAD_MEM) ?
                         payload.dtn_bundle_payload_t_u.buf.buf_val :
                         payload.dtn_bundle_payload_t_u.filename.filename_val);
    
    // assign the addressing fields
    b->source_.assign(&spec.source);
    b->dest_.assign(&spec.dest);
    if (spec.replyto.uri[0] == '\0') {
        b->replyto_.assign(EndpointID::NULL_EID());
    } else {
        b->replyto_.assign(&spec.replyto);
    }
    b->custodian_.assign(EndpointID::NULL_EID());
     
    oasys::StringBuffer error;
    if (!b->validate(&error)) {
        log_err("bundle validation failed: %s", error.data());
        return DTN_EINVAL;
    }
    
    // the priority code
    switch (spec.priority) {
#define COS(_cos) case _cos: b->priority_ = Bundle::_cos; break;
        COS(COS_BULK);
        COS(COS_NORMAL);
        COS(COS_EXPEDITED);
        COS(COS_RESERVED);
#undef COS
    default:
        log_err("invalid priority level %d", (int)spec.priority);
        return DTN_EINVAL;
    };

    // delivery options
    if (spec.dopts & DOPTS_CUSTODY)
        b->custody_requested_ = true;
    
    if (spec.dopts & DOPTS_DELIVERY_RCPT)
        b->delivery_rcpt_ = true;

    if (spec.dopts & DOPTS_RECEIVE_RCPT)
        b->receive_rcpt_ = true;

    if (spec.dopts & DOPTS_FORWARD_RCPT)
        b->forward_rcpt_ = true;

    if (spec.dopts & DOPTS_CUSTODY_RCPT)
        b->custody_rcpt_ = true;

    if (spec.dopts & DOPTS_DELETE_RCPT)
        b->deletion_rcpt_ = true;

    // expiration time
    b->expiration_ = spec.expiration;

    // set up the payload, including calculating its length, but don't
    // copy it in yet
    size_t payload_len;
    char filename[PATH_MAX];

    switch (payload.location) {
    case DTN_PAYLOAD_MEM:
        payload_len = payload.dtn_bundle_payload_t_u.buf.buf_len;
        break;
        
    case DTN_PAYLOAD_FILE:
    case DTN_PAYLOAD_TEMP_FILE:
        struct stat finfo;
        sprintf(filename, "%.*s", 
                (int)payload.dtn_bundle_payload_t_u.filename.filename_len,
                payload.dtn_bundle_payload_t_u.filename.filename_val);

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
    
    b->payload_.set_length(payload_len);

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
        b->payload_.set_data((u_char*)payload.dtn_bundle_payload_t_u.buf.buf_val,
                             payload.dtn_bundle_payload_t_u.buf.buf_len);
        break;
        
    case DTN_PAYLOAD_FILE:
        FILE* file;
        int r, left;
        u_char buffer[4096];

        if ((file = fopen(filename, "r")) == NULL)
        {
            log_err("payload file %s can't be opened!", filename);
            return DTN_EINVAL;
        }
        
        left = payload_len;
        r = 0;
        while (left > 0)
        {
            r = fread(buffer, 1, (left>4096)?4096:left, file);
            
            if (r)
            {
                b->payload_.append_data(buffer, r);
                left -= r;
            }
            else
            {
                sleep(1); // pause before re-reading
            }
        }

        fclose(file);
        break;
        
    case DTN_PAYLOAD_TEMP_FILE:
        if (! b->payload_.replace_with_file(filename)) {
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
    id.creation_secs    = b->creation_ts_.seconds_;
    id.creation_subsecs = b->creation_ts_.seqno_;
    
    log_info("DTN_SEND bundle *%p", b.object());

    // deliver the bundle
    // Note: the bundle state may change once it has been posted
    BundleDaemon::post_and_wait(new BundleReceivedEvent(b.object(), EVENTSRC_APP),
                                &notifier_);

    // return the bundle id struct
    if (!xdr_dtn_bundle_id_t(&xdr_encode_, &id)) {
        log_err("internal error in xdr: xdr_dtn_bundle_id_t");
        return DTN_EXDR;
    }
    
    return DTN_SUCCESS;
}

// Size for temporary memory buffer used when delivering bundles
// via files.
#define DTN_FILE_DELIVERY_BUF_SIZE 1000

//----------------------------------------------------------------------
int
APIClient::handle_recv()
{
    dtn_bundle_spec_t             spec;
    dtn_bundle_payload_t          payload;
    dtn_bundle_payload_location_t location;
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
    
    int err = wait_for_bundle("recv", timeout, &reg, &sock_ready);
    if (err != 0) {
        return err;
    }
    
    // if there's data on the socket, that either means the socket was
    // closed by an exiting application or the app is violating the
    // protocol...
    if (sock_ready) {
        log_debug("handle_recv: api socket ready -- trying to read one byte");
        char b;
        if (read(&b, 1) != 0) {
            log_err("handle_recv: protocol error -- "
                    "data arrived or error while blocked in recv");
            return DTN_ECOMM;
        }

        log_info("IPC socket closed while blocked in read... "
                 "application must have exited");
        return -1;
    }
    
    BundleRef bref("APIClient::handle_recv");
    bref = reg->bundle_list()->pop_front();
    Bundle* b = bref.object();
    ASSERT(b != NULL);
    
    log_debug("handle_recv: popped bundle %d for registration %d (timeout %d)",
              b->bundleid_, reg->regid(), timeout);
    
    memset(&spec, 0, sizeof(spec));
    memset(&payload, 0, sizeof(payload));

    // copyto will malloc string buffer space that needs to be freed
    // at the end of the fn
    b->source_.copyto(&spec.source);
    b->dest_.copyto(&spec.dest);
    b->replyto_.copyto(&spec.replyto);

    // XXX/demmer copy delivery options and class of service fields

    // XXX/demmer verify bundle size constraints
    payload.location = location;
    
    if (location == DTN_PAYLOAD_MEM) {
        // the app wants the payload in memory
        // XXX/demmer verify bounds

        size_t payload_len = b->payload_.length();
        payload.dtn_bundle_payload_t_u.buf.buf_len = payload_len;
        if (payload_len != 0) {
            buf.reserve(payload_len);
            payload.dtn_bundle_payload_t_u.buf.buf_val =
                (char*)b->payload_.read_data(0, payload_len, buf.buf());
        } else {
            payload.dtn_bundle_payload_t_u.buf.buf_val = 0;
        }
        
    } else if (location == DTN_PAYLOAD_FILE) {
        char *tdir, templ[64];

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
        
        if (b->payload_.location() == BundlePayload::MEMORY) {
            tmpfile.writeall((char*)b->payload_.memory_data(),
                             b->payload_.length());
            
        } else {
            b->payload_.copy_file(&tmpfile);
        }

        payload.dtn_bundle_payload_t_u.filename.filename_val =
            (char*)tmpfile.path();
        payload.dtn_bundle_payload_t_u.filename.filename_len =
            tmpfile.path_len();
        tmpfile.close();
        
    } else {
        log_err("payload location %d not understood", location);
        return DTN_EINVAL;
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
    
    log_info("DTN_RECV: "
             "successfully delivered bundle %d to registration %d",
             b->bundleid_, reg->regid());
    
    BundleDaemon::post(new BundleDeliveredEvent(b, reg));

    return DTN_SUCCESS;
}

//----------------------------------------------------------------------
int
APIClient::handle_begin_poll()
{
    dtn_timeval_t    timeout;
    APIRegistration* reg = NULL;
    bool             sock_ready = false;
    
    // unpack the arguments
    if ((!xdr_dtn_timeval_t(&xdr_decode_, &timeout)))
    {
        log_err("error in xdr unpacking arguments");
        return DTN_EXDR;
    }

    int err = wait_for_bundle("poll", timeout, &reg, &sock_ready);
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

        // immediately send the response to the poll cancel, then
        // we return from the handler which will follow it with the
        // response code to the original poll request
        send_response(DTN_SUCCESS);
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
APIClient::wait_for_bundle(const char* operation, dtn_timeval_t dtn_timeout,
                           APIRegistration** regp, bool* sock_ready)
{
    APIRegistration* reg;
    
    // XXX/demmer implement this for multiple registrations by
    // building up a poll vector here. for now we assert in bind that
    // there's only one binding.
    
    if (bindings_->empty()) {
        log_err("wait_for_bundle(%s): no bound registration", 
                operation);
        return DTN_EINVAL;
    }
    
    reg = bindings_->front();

    // short-circuit the poll
    if (reg->bundle_list()->size() != 0) {
        log_debug("wait_for_bundle(%s): "
                  "immediately returning bundle for reg %d",
                  operation, reg->regid());
        *regp = reg;
        return 0;
    }

    int timeout = (int)dtn_timeout;
    if (timeout < -1) {
        log_err("wait_for_bundle(%s): "
                "invalid timeout value %d", operation, timeout);
        return DTN_EINVAL;
    }

    struct pollfd pollfds[2];

    struct pollfd* bundle_poll = &pollfds[0];
    bundle_poll->fd            = reg->bundle_list()->notifier()->read_fd();
    bundle_poll->events        = POLLIN;
    bundle_poll->revents       = 0;
    
    struct pollfd* sock_poll   = &pollfds[1];
    sock_poll->fd              = TCPClient::fd_;
    sock_poll->events          = POLLIN | POLLERR;
    sock_poll->revents         = 0;

    log_debug("wait_for_bundle(%s): "
              "blocking to get bundle for registration %d (timeout %d)",
              operation, reg->regid(), timeout);
    int nready = oasys::IO::poll_multiple(&pollfds[0], 2, timeout,
                                          NULL, logpath_);

    if (nready == oasys::IOTIMEOUT) {
        log_debug("wait_for_bundle(%s): timeout waiting for bundle",
                  operation);
        return DTN_ETIMEOUT;

    } else if (nready <= 0) {
        log_err("wait_for_bundle(%s): unexpected error polling for bundle",
                operation);
        return DTN_EINTERNAL;
    }

    ASSERT(nready == 1);
    
    // if there's data on the socket, that either means the socket was
    // closed by an exiting application or the app is violating the
    // protocol...
    if (sock_poll->revents != 0) {
        *sock_ready = true;
        return 0;
    }

    // otherwise, there should be data on the bundle list
    if (bundle_poll->revents == 0) {
        log_crit("wait_for_bundle(%s): unexpected error polling for bundle: "
                 "neither file descriptor is ready", operation);
        return DTN_EINTERNAL;
    }

    if (reg->bundle_list()->size() == 0) {
        log_err("wait_for_bundle(%s): "
                "bundle list returned ready but no bundle on queue!!",
                operation);
        return DTN_EINTERNAL;
    }

    *regp = reg;
    return 0;
}

//----------------------------------------------------------------------
int
APIClient::handle_close()
{
    log_info("received DTN_CLOSE message; closing API handle");
    // return -1 to force the session to close:
    return -1;
}

} // namespace dtn
