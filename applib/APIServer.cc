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

#include <sys/stat.h>
#include <oasys/compat/inet_aton.h>
#include <oasys/io/FileIOClient.h>
#include <oasys/io/NetUtils.h>

#include "APIServer.h"
#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleDaemon.h"
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

in_addr_t APIServer::local_addr_;
u_int16_t APIServer::local_port_;

void
APIServer::init()
{
    /*
     * Default values for address / ports, overrided via the
     * configuration interface (see servlib/cmd/APICommand.cc).
     */
    APIServer::local_addr_ = htonl(INADDR_LOOPBACK);
    APIServer::local_port_ = DTN_IPC_PORT;
    oasys::TclCommandInterp::instance()->reg(new APICommand());
}

APIServer::APIServer()
    : TCPServerThread("/apiserver", DELETE_ON_EXIT)
{
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

    log_debug("APIServer init (addr %s port %d)",
              intoa(local_addr_), local_port_);
}

void
APIServer::accepted(int fd, in_addr_t addr, u_int16_t port)
{
    APIClient* c = new APIClient(fd, addr, port);
    c->start();
}

APIClient::APIClient(int fd, in_addr_t addr, u_int16_t port)
    : Thread(DELETE_ON_EXIT),
      TCPClient(fd, addr, port, "/apiclient")
{
    // note that we skip space for the message length and code/status
    xdrmem_create(&xdr_encode_, buf_ + 8, DTN_MAX_API_MSG - 8, XDR_ENCODE);
    xdrmem_create(&xdr_decode_, buf_ + 8, DTN_MAX_API_MSG - 8, XDR_DECODE);

    bindings_ = new APIRegistrationList();
}

void
APIClient::close_session()
{
    // XXX/demmer go through registrations and disassociate them

    TCPClient::close();
}

void
APIClient::run()
{
    int ret;
    u_int32_t handshake, type, len, msglen;
    
    log_debug("new session %s:%d -> %s:%d",
              intoa(local_addr()), local_port(),
              intoa(remote_addr()), remote_port());

    // handle the handshake
    ret = read((char*)&handshake, sizeof(handshake));
    if (ret != sizeof(handshake)) {
        log_err("error reading handshake: %s", strerror(errno));
        close_session();
        return;
    }

    if (ntohl(handshake) != DTN_OPEN) {
        log_err("handshake %d != DTN_OPEN", handshake);
        close_session();
        return;
    }
    
    ret = write((char*)&handshake, sizeof(handshake));
    if (ret != sizeof(handshake)) {
        log_err("error writing handshake: %s", strerror(errno));
        close_session();
        return;
    }

    while (1) {
        xdr_setpos(&xdr_encode_, 0);
        xdr_setpos(&xdr_decode_, 0);

        ret = read(buf_, DTN_MAX_API_MSG);
        if (ret <= 0) {
            if (errno == EINTR)
                continue;

            log_warn("client error or disconnection");
            close_session();
            return;
        }

        if (ret < 8) {
            log_err("ack!! can't handle really short read...");
            close_session();
            return;
        }
        
        memcpy(&type, buf_,     sizeof(type));
        memcpy(&len,  &buf_[4], sizeof(len));

        type = ntohl(type);
        len  = ntohl(len);

        log_debug("got %s (%d/%d bytes)", dtnipc_msgtoa(type), ret - 8, len);
        
        // if we didn't get the whole message, loop to get the rest
        if ((ret - 8) < (int)len) {
            int toget = len - (ret - 8);
            if (readall(&buf_[ret], toget) != toget) {
                log_err("error reading message remainder: %s", strerror(errno));
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
            
            DISPATCH(DTN_GETINFO,  handle_getinfo);
            DISPATCH(DTN_REGISTER, handle_register);
            DISPATCH(DTN_SEND,     handle_send);
            DISPATCH(DTN_BIND,     handle_bind);
            DISPATCH(DTN_RECV,     handle_recv);
#undef DISPATCH

        default:
            log_err("unknown message type code 0x%x", type);
            ret = DTN_ECOMM;
            break;
        }

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
            return;
        }
        
    } // while(1)
}

int
APIClient::handle_getinfo()
{
    dtn_info_request_t request;
    dtn_info_response_t response;
    
    // unpack the request
    if (!xdr_dtn_info_request_t(&xdr_decode_, &request))
    {
        log_err("error in xdr unpacking arguments");
        return DTN_EXDR;
    }

    // initialize the response
    memset(&response, 0, sizeof(response));
    response.request = request;

    switch(request) {
    case DTN_INFOREQ_INTERFACES: {
        log_debug("getinfo DTN_INFOREQ_INTERFACES");
        BundleRouter* router = BundleDaemon::instance()->router();
        
        dtn_tuple_t* local_tuple =
            &response.dtn_info_response_t_u.interfaces.local_tuple;

        strncpy(local_tuple->region,
                router->local_tuple().region().c_str(),
                DTN_MAX_REGION_LEN);
        
        local_tuple->admin.admin_val =
            (char*)router->local_tuple().admin().c_str();
        
        local_tuple->admin.admin_len =
            router->local_tuple().admin().length();

        break;
    }
        
    case DTN_INFOREQ_CONTACTS:
        NOTIMPLEMENTED;
        break;
        
    case DTN_INFOREQ_ROUTES:
        NOTIMPLEMENTED;
        break;
    }
    
    // fill in the info response code
    if (!xdr_dtn_info_response_t(&xdr_encode_, &response)) {
        log_err("internal error in xdr: xdr_dtn_info_response_t");
        return DTN_EXDR;
    }

    log_debug("getinfo encoded %d byte response", xdr_getpos(&xdr_encode_));
    
    return DTN_SUCCESS;
}

int
APIClient::handle_register()
{
    Registration* reg;
    Registration::failure_action_t action;
    BundleTuple endpoint;
    std::string script;
    
    dtn_reg_info_t reginfo;
    dtn_reg_id_t regid;

    memset(&reginfo, 0, sizeof(reginfo));
    
    // unpack and parse the request
    if (!xdr_dtn_reg_info_t(&xdr_decode_, &reginfo))
    {
        log_err("error in xdr unpacking arguments");
        return DTN_EXDR;
    }

    endpoint.assign(&reginfo.endpoint);
    
    switch (reginfo.action) {
    case DTN_REG_ABORT: action = Registration::ABORT; break;
    case DTN_REG_DEFER: action = Registration::DEFER; break;
    case DTN_REG_EXEC:  action = Registration::EXEC;  break;
    default: {
        log_err("invalid action code 0x%x", reginfo.action);
        return DTN_EINVAL;
    }
    }

    if (action == Registration::EXEC) {
        script.assign(reginfo.args.args_val, reginfo.args.args_len);
    }

    if (reginfo.regid != DTN_REGID_NONE){
        PANIC("lookup and modify existing registration unimplemented");
        regid = reginfo.regid;
        
    } else {
        u_int32_t regid = GlobalStore::instance()->next_regid();
        reg = new APIRegistration(regid, endpoint, action);
        BundleDaemon::instance()->reg_table()->add(reg);
        BundleDaemon::post(new RegistrationAddedEvent(reg));
    }

    regid = reg->regid();
    
    // fill the response with the new registration id
    if (!xdr_dtn_reg_id_t(&xdr_encode_, &regid)) {
        log_err("internal error in xdr: xdr_dtn_reg_id_t");
        return DTN_EXDR;
    }
    
    return DTN_SUCCESS;
}

int
APIClient::handle_bind()
{
    dtn_reg_id_t regid;
    dtn_tuple_t endpoint;

    memset(&endpoint, 0, sizeof(endpoint));
    
    // unpack the request
    if (!xdr_dtn_reg_id_t(&xdr_decode_, &regid) ||
        !xdr_dtn_tuple_t(&xdr_decode_, &endpoint))
    {
        log_err("error in xdr unpacking arguments");
        return DTN_EXDR;
    }

    BundleTuple tuple;
    tuple.assign(&endpoint);

    // look up the registration
    RegistrationTable* regtable = BundleDaemon::instance()->reg_table();
    Registration* reg = regtable->get(regid, tuple);

    if (!reg) {
        log_err("can't find registration %d tuple %s", regid, tuple.c_str());
        return DTN_ENOTFOUND;
    }

    APIRegistration* api_reg = dynamic_cast<APIRegistration*>(reg);
    if (api_reg == NULL) {
        log_crit("registration %d tuple %s is not an API registration!!",
                 regid, tuple.c_str());
        return DTN_ENOTFOUND;
    }
    
    // store the registration in the list for this session
    bindings_->push_back(api_reg);

    return DTN_SUCCESS;
}
    
int
APIClient::handle_send()
{
    Bundle* b;
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

    // assign the addressing fields
    b->source_.assign(&spec.source);
    if (!b->source_.valid()) {
        log_err("invalid source tuple [%s %s]",
                spec.source.region, spec.source.admin.admin_val);
        return DTN_EINVAL;
    }
    
    b->dest_.assign(&spec.dest);
    if (!b->dest_.valid()) {
        log_err("invalid dest tuple [%s %s]",
                spec.dest.region, spec.dest.admin.admin_val);
        return DTN_EINVAL;
    }
    
    b->replyto_.assign(&spec.replyto);
    if (!b->replyto_.valid()) {
        log_err("invalid replyto tuple [%s %s]",
                spec.replyto.region, spec.replyto.admin.admin_val);
        return DTN_EINVAL;
    }

    b->custodian_.assign(BundleDaemon::instance()->router()->local_tuple());
    
    // the priority code
    switch (spec.priority) {
#define COS(_cos) case _cos: b->priority_ = _cos; break;
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
        b->custreq_ = true;
    
    if (spec.dopts & DOPTS_RETURN_RCPT)
        b->return_rcpt_ = true;

    if (spec.dopts & DOPTS_RECV_RCPT)
        b->recv_rcpt_ = true;

    if (spec.dopts & DOPTS_FWD_RCPT)
        b->fwd_rcpt_ = true;

    if (spec.dopts & DOPTS_CUST_RCPT)
        b->custody_rcpt_ = true;

    if (spec.dopts & DOPTS_OVERWRITE)
        NOTIMPLEMENTED;

    // finally, the payload
    size_t payload_len;
    switch ( payload.location ) {
    case DTN_PAYLOAD_MEM:
        b->payload_.set_data((u_char*)payload.dtn_bundle_payload_t_u.buf.buf_val,
                             payload.dtn_bundle_payload_t_u.buf.buf_len);
        payload_len = payload.dtn_bundle_payload_t_u.buf.buf_len;
        break;
        
    case DTN_PAYLOAD_FILE:
        char filename[PATH_MAX];
        FILE * file;
        struct stat finfo;
        int r, left;
        u_char buffer[4096];

        sprintf(filename, "%.*s", 
                (int)payload.dtn_bundle_payload_t_u.filename.filename_len,
                payload.dtn_bundle_payload_t_u.filename.filename_val);

        if (stat(filename, &finfo) || (file = fopen(filename, "r")) == NULL)
        {
            log_err("payload file %s does not exist!", filename);
            return DTN_EINVAL;
        }
        
        payload_len = finfo.st_size;
        b->payload_.set_length(payload_len);

        b->payload_.reopen_file();
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
        b->payload_.close_file();
        break;
        
    default:
        log_err("payload.location of %d unknown", payload.location);
        return DTN_EINVAL;
    }
    
    // deliver the bundle
    // Note: the bundle state may change once it has been posted
    BundleDaemon::post(new BundleReceivedEvent(b, EVENTSRC_APP));
    
    return DTN_SUCCESS;
}

// Size for temporary memory buffer used when delivering bundles
// via files.
#define DTN_FILE_DELIVERY_BUF_SIZE 1000

int
APIClient::handle_recv()
{
    int err;
    Bundle* b = NULL;
    APIRegistration* reg = NULL;
    dtn_bundle_spec_t spec;
    dtn_bundle_payload_t payload;
    dtn_bundle_payload_location_t location;
    dtn_timeval_t timeout;
    oasys::StringBuffer buf;

    // unpack the arguments
    if ((!xdr_dtn_bundle_payload_location_t(&xdr_decode_, &location)) ||
        (!xdr_dtn_timeval_t(&xdr_decode_, &timeout)))
    {
        log_err("error in xdr unpacking arguments");
        return DTN_EXDR;
    }

    // XXX/demmer implement this for multiple registrations by
    // building up a poll vector in bind / unbind. for now we assert
    // in bind that there's only one binding.
    reg = bindings_->front();
    if (!reg) {
        log_err("no bound registration");
        return DTN_EINVAL;
    }

    log_debug("handle_recv: "
              "blocking to get bundle for registration %d (timeout %d)",
              reg->regid(), timeout);
    b = reg->bundle_list()->pop_blocking(NULL, timeout);
    
    if (!b) {
        log_debug("handle_recv: timeout waiting for bundle");
        return DTN_ETIMEOUT;
    }
    
    log_debug("handle_recv: popped bundle for registration %d (timeout %d)",
              reg->regid(), timeout);

    ASSERT(b);

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
        buf.reserve(payload_len);
        payload.dtn_bundle_payload_t_u.buf.buf_len = payload_len;
        payload.dtn_bundle_payload_t_u.buf.buf_val =
            (char*)b->payload_.read_data(0, payload_len, (u_char*)buf.data());
        
    } else if (location == DTN_PAYLOAD_FILE) {
        oasys::FileIOClient tmpfile;

        if (tmpfile.mkstemp("/tmp/bundlePayload_XXXXXX") == -1) {
            log_err("can't open temporary file to deliver bundle");
            return DTN_EINTERNAL;
        }
        
        if (b->payload_.location() == BundlePayload::MEMORY) {
            tmpfile.writeall((char*)b->payload_.memory_data(),
                             b->payload_.length());
            
        } else {
            b->payload_.copy_file(&tmpfile);
        }

        payload.dtn_bundle_payload_t_u.filename.filename_val = (char*)tmpfile.path();
        payload.dtn_bundle_payload_t_u.filename.filename_len = tmpfile.path_len();
        tmpfile.close();
        
    } else {
        log_err("payload location %d not understood", location);
        err = DTN_EINVAL;
        goto done;
    }

    if (!xdr_dtn_bundle_spec_t(&xdr_encode_, &spec))
    {
        log_err("internal error in xdr: xdr_dtn_bundle_spec_t");
        err = DTN_EXDR;
        goto done;
    }
    
    if (!xdr_dtn_bundle_payload_t(&xdr_encode_, &payload))
    {
        log_err("internal error in xdr: xdr_dtn_bundle_payload_t");
        err = DTN_EXDR;
        goto done;
    }

    BundleDaemon::post(
        new BundleTransmittedEvent(b, reg, b->payload_.length(), true));

    err = DTN_SUCCESS;
    
 done:
    // nuke our local reference on the bundle (which may delete it)
    b->del_ref("api_client", reg->logpath());

    // clean up tuple memory
    free(spec.source.admin.admin_val);
    free(spec.dest.admin.admin_val);
    free(spec.replyto.admin.admin_val);
    
    return err;
}

} // namespace dtn
