
#include "APIServer.h"
#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleForwarder.h"
#include "io/UDPClient.h"
#include "io/NetUtils.h"
#include "reg/Registration.h"
#include "reg/RegistrationTable.h"
#include "routing/BundleRouter.h"
#include "storage/GlobalStore.h"

in_addr_t APIServer::local_addr_ = htonl(INADDR_LOOPBACK);
u_int16_t APIServer::handshake_port_ = DTN_API_HANDSHAKE_PORT;
u_int16_t APIServer::session_port_ = DTN_API_SESSION_PORT;

APIServer::APIServer()
{
    sock_ = new UDPClient();
    sock_->set_logfd(false);
    sock_->logpathf("/apisrv/sock");
    
    buflen_ = DTN_MAX_API_MSG;
    buf_ = (char*)malloc(buflen_);

    // note that we skip four bytes for the ipc typecode when setting
    // up the xdr decoder
    xdr_encode_ = new XDR;
    xdr_decode_ = new XDR;
    xdrmem_create(xdr_encode_, buf_, DTN_MAX_API_MSG, XDR_ENCODE);
    xdrmem_create(xdr_decode_, buf_ + sizeof(u_int32_t),
                  DTN_MAX_API_MSG, XDR_DECODE);
}

MasterAPIServer::MasterAPIServer()
    : APIServer(),
      Thread(), Logger("/apisrv/master")
{
    log_debug("MasterAPIServer::init (port %d)", handshake_port_);
    sock_->bind(local_addr_, handshake_port_);
}

void
MasterAPIServer::run()
{
    in_addr_t addr;
    u_int16_t port;
    u_int32_t handshake;

    while (1) {
        int cc = sock_->recvfrom(buf_, buflen_, 0, &addr, &port);
        if (cc <= 0) {
            log_err("error in recvfrom: %s", strerror(errno));
            continue;
        }
        
        log_debug("got %d byte message from %s:%d", cc, intoa(addr), port);

        if (cc != sizeof(handshake)) {
            log_err("unexpected %d message from %s:%d "
                    "(expected %d byte handshake)",
                    cc, intoa(addr), port, sizeof(handshake));
            continue;
        }
        
        memcpy(&handshake, buf_, sizeof(handshake));
        
        if (handshake != DTN_OPEN) {
            log_warn("unexpected handshake type code 0x%x on master thread",
                     handshake);
            continue;
        }

        // otherwise create the client api server thread
        ClientAPIServer* server = new ClientAPIServer(addr, port);
        server->start();

        // XXX/demmer store these in a table so they can be queried
    }
}

ClientAPIServer::ClientAPIServer(in_addr_t remote_host,
                                 u_int16_t remote_port)
    : APIServer(),
      Thread(DELETE_ON_EXIT), Logger("/apisrv")
{
    log_debug("APIServer::init on port %d (remote %s:%d)",
              session_port_, intoa(remote_host), remote_port);

    sock_->bind(local_addr_, session_port_);
    sock_->connect(remote_host, remote_port);

    bindings_ = new RegistrationList();
}

const char*
ClientAPIServer::msgtoa(u_int32_t type)
{
#define CASE(_type) case _type : return #_type; break;
    switch(type) {
        CASE(DTN_OPEN);
        CASE(DTN_CLOSE);
        CASE(DTN_GETINFO);
        CASE(DTN_REGISTER);
        CASE(DTN_BIND);
        CASE(DTN_SEND);
        CASE(DTN_RECV);

    default:
        return "(unknown type)";
    }
#undef CASE
}

void
ClientAPIServer::run()
{
    // first and foremost, send the handshake response
    u_int32_t handshake = DTN_OPEN;
    if (sock_->send((char*)&handshake, sizeof(handshake), 0) < 0)
    {
        log_err("error sending handshake: %s", strerror(errno));
        return;
    }
    
    while (1) {
        xdr_setpos(xdr_encode_, 0);
        xdr_setpos(xdr_decode_, 0);

        int cc = sock_->recv(buf_, buflen_, 0);
        if (cc <= 0) {
            log_err("error in recvfrom: %s", strerror(errno));
            continue;
        }
        
        u_int32_t type;
        memcpy(&type, buf_, sizeof(type));

        log_debug("%s (%d bytes)", msgtoa(type), cc);
        
#define DISPATCH(_type, _fn)                    \
        case _type:                             \
            if (_fn() != 0) {                   \
                return;                         \
            }                                   \
            break;

        switch(type) {
            DISPATCH(DTN_GETINFO,  handle_getinfo);
            DISPATCH(DTN_REGISTER, handle_register);
            DISPATCH(DTN_SEND,     handle_send);
            DISPATCH(DTN_BIND,     handle_bind);
            DISPATCH(DTN_RECV,     handle_recv);

        default:
            log_err("unknown message type code 0x%x", type);
        }
        
#undef DISPATCH
    }
}

int
ClientAPIServer::handle_getinfo()
{
    dtn_info_request_t request;
    dtn_info_response_t response;
    
    // unpack the request
    if (!xdr_dtn_info_request_t(xdr_decode_, &request))
    {
        log_err("error in xdr unpacking arguments");
        return -1;
    }

    // blank the response
    memset(&response, 0, sizeof(response));

    response.request = request;

    switch(request) {
    case DTN_INFOREQ_INTERFACES: {
        dtn_tuple_t* local_tuple =
            &response.dtn_info_response_t_u.interfaces.local_tuple;

        strncpy(local_tuple->region,
                BundleRouter::local_tuple_.region().c_str(),
                DTN_MAX_REGION_LEN);
        
        local_tuple->admin.admin_val =
            (char*)BundleRouter::local_tuple_.admin().c_str();
        
        local_tuple->admin.admin_len =
            BundleRouter::local_tuple_.admin().length();

        break;
    }
        
    case DTN_INFOREQ_CONTACTS:
        NOTIMPLEMENTED;
        break;
    case DTN_INFOREQ_ROUTES:
        NOTIMPLEMENTED;
        break;
    }
    
    /* send the return code */
    if (!xdr_dtn_info_response_t(xdr_encode_, &response)) {
        log_err("internal error in xdr");
        return -1;
    }
    
    int len = xdr_getpos(xdr_encode_);
    if (sock_->send(buf_, len, 0) != len) {
        log_err("error sending response code");
        return -1;
    }

    return 0;
}

int
ClientAPIServer::handle_register()
{
    Registration* reg;
    Registration::failure_action_t action;
    BundleTuple endpoint;
    std::string script;
    
    dtn_reg_info_t reginfo;
    dtn_reg_id_t regid;

    memset(&reginfo, 0, sizeof(reginfo));
    
    // unpack and parse the request
    if (!xdr_dtn_reg_info_t(xdr_decode_, &reginfo))
    {
        log_err("error in xdr unpacking arguments");
        return -1;
    }

    endpoint.assign(&reginfo.endpoint);
    
    switch (reginfo.action) {
    case DTN_REG_ABORT: action = Registration::ABORT; break;
    case DTN_REG_DEFER: action = Registration::DEFER; break;
    case DTN_REG_EXEC:  action = Registration::EXEC;  break;
    default: {
        log_err("invalid action code 0x%x", reginfo.action);
        return -1;
    }
    }

    if (action == Registration::EXEC) {
        script.assign(reginfo.args.args_val, reginfo.args.args_len);
    }

    if (reginfo.regid != DTN_REGID_NONE){
        PANIC("lookup and modify existing registration unimplemented");
        regid = reginfo.regid;
        
    } else {
        reg = new Registration(endpoint, action);
    }

    regid = reg->regid();
    
    // return the new registration id
    if (!xdr_dtn_reg_id_t(xdr_encode_, &regid)) {
        log_err("internal error in xdr");
        return -1;
    }
    
    int len = xdr_getpos(xdr_encode_);
    if (sock_->send(buf_, len, 0) != len) {
        log_err("error sending response code");
        return -1;
    }

    return 0;
}

int
ClientAPIServer::handle_bind()
{
    dtn_reg_id_t regid;
    dtn_tuple_t endpoint;

    memset(&endpoint, 0, sizeof(endpoint));
    
    // unpack the request
    if (!xdr_dtn_reg_id_t(xdr_decode_, &regid) ||
        !xdr_dtn_tuple_t(xdr_decode_, &endpoint))
    {
        log_err("error in xdr unpacking arguments");
        return -1;
    }

    BundleTuple tuple;
    tuple.assign(&endpoint);

    // look up the registration
    RegistrationTable* regtable = RegistrationTable::instance();
    Registration* reg = regtable->get(regid, tuple);

    if (!reg) {
        log_err("can't find registration %d tuple %s",
                regid, tuple.c_str());
        return -1;
    }

    bindings_->push_back(reg);

    return 0;
}
    
int
ClientAPIServer::handle_send()
{
    Bundle* b;
    long ret;
    dtn_bundle_spec_t spec;
    dtn_bundle_payload_t payload;

    memset(&spec, 0, sizeof(spec));
    memset(&payload, 0, sizeof(payload));
    
    /* Unpack the arguments */
    if (!xdr_dtn_bundle_spec_t(xdr_decode_, &spec) ||
        !xdr_dtn_bundle_payload_t(xdr_decode_, &payload))
    {
        log_err("error in xdr unpacking arguments");
        return -1;
    }

    b = new Bundle();

    // assign the addressing fields
    b->source_.assign(&spec.source);
    if (!b->source_.valid()) {
        log_err("invalid source tuple [%s %s]",
                spec.source.region, spec.source.admin.admin_val);
        ret = DTN_INVAL;
        goto done;
    }
    
    b->dest_.assign(&spec.dest);
    if (!b->dest_.valid()) {
        log_err("invalid dest tuple [%s %s]",
                spec.dest.region, spec.dest.admin.admin_val);
        ret = DTN_INVAL;
        goto done;
    }
    
    b->replyto_.assign(&spec.replyto);
    if (!b->replyto_.valid()) {
        log_err("invalid replyto tuple [%s %s]",
                spec.replyto.region, spec.replyto.admin.admin_val);
        ret = DTN_INVAL;
        goto done;
    }

    b->custodian_.assign(BundleRouter::local_tuple_);
    
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
        ret = DTN_INVAL;
        goto done;
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
    if (payload.location == DTN_PAYLOAD_MEM) {
        b->payload_.set_data(payload.dtn_bundle_payload_t_u.buf.buf_val,
                             payload.dtn_bundle_payload_t_u.buf.buf_len);
        payload_len = payload.dtn_bundle_payload_t_u.buf.buf_len;
    } else {
        char filename[512];
        FILE * file;
        struct stat finfo;
        int r, left;
        char buffer[4096];

        sprintf(filename, "%.*s", 
                payload.dtn_bundle_payload_t_u.filename.filename_len,
                payload.dtn_bundle_payload_t_u.filename.filename_val);

        if (stat(filename, &finfo) || (file = fopen(filename, "r")) == NULL)
        {
            char buffer[4096];
            sprintf(buffer, "payload file %s does not exist!", filename);
            log_err(buffer);

            ret = DTN_INVAL;
            goto done;
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
    }
    
    // deliver the bundle
    // Note: the bundle state may change once it has been posted
    BundleForwarder::post(new BundleReceivedEvent(b, payload_len));
    
    ret = DTN_SUCCESS;

    /* send the return code */
 done:
    if (!xdr_putlong(xdr_encode_, &ret)) {
        log_err("internal error in xdr");
        return -1;
    }

    int len = xdr_getpos(xdr_encode_);
    if (sock_->send(buf_, len, 0) != len) {
        log_err("error sending response code");
        return -1;
    }

    return 0;
}

int
ClientAPIServer::handle_recv()
{
    Bundle* b;
    dtn_bundle_spec_t spec;
    dtn_bundle_payload_t payload;
    dtn_bundle_payload_location_t location;
    dtn_timeval_t timeout;

    // unpack the arguments
    if ((!xdr_dtn_bundle_payload_location_t(xdr_decode_, &location)) ||
        (!xdr_dtn_timeval_t(xdr_decode_, &timeout)))
    {
        log_err("error in xdr unpacking arguments");
        return -1;
    }

    // XXX/demmer implement this for multiple registrations by
    // building up a poll vector in bind / unbind. for now we assert
    // in bind that there's only one binding.
    // also implement the recv timeout
    Registration* reg = bindings_->front();
    if (!reg) {
        log_err("no bound registration");
        return -1;
    }

    b = reg->bundle_list()->pop_blocking();
    ASSERT(b);

    memset(&spec, 0, sizeof(spec));
    memset(&payload, 0, sizeof(payload));
    
    b->source_.copyto(&spec.source);
    b->dest_.copyto(&spec.dest);
    b->replyto_.copyto(&spec.replyto);

    // XXX/demmer copy delivery options and class of service fields

    // XXX/demmer verify bundle size constraints
    payload.location = location;
    
    if (location == DTN_PAYLOAD_MEM) {
        char** dst = &payload.dtn_bundle_payload_t_u.buf.buf_val;
        u_int* len = &payload.dtn_bundle_payload_t_u.buf.buf_len;

        *len = b->payload_.length();

        if (b->payload_.location() != BundlePayload::MEMORY)
        {
            log_err("file-based payload cannot be received in memory");
            return -1;
        }
        
        *dst = b->payload_.memory_data();
        
    } else {
        PANIC("file-based payloads not implemented");
    }
 
    if (!xdr_dtn_bundle_spec_t(xdr_encode_, &spec) ||
        !xdr_dtn_bundle_payload_t(xdr_encode_, &payload))
    {
       log_err("internal error in xdr");
       return -1;
    }
    
    int len = xdr_getpos(xdr_encode_);
    if (sock_->send(buf_, len, 0) != len) {
        log_err("error sending bundle reply");
        return -1;
    }

    BundleForwarder::post(
        new BundleTransmittedEvent(b, reg, b->payload_.length(), true));
    
    return 0;
}

