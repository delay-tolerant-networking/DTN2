
#include "TCPConvergenceLayer.h"
#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleForwarder.h"
#include "bundling/BundleList.h"
#include "bundling/BundleProtocol.h"
#include "bundling/FragmentManager.h"
#include "io/NetUtils.h"
#include "thread/Timer.h"
#include "util/URL.h"

#include <sys/poll.h>
#include <stdlib.h>

struct TCPConvergenceLayer::_defaults TCPConvergenceLayer::Defaults;

/******************************************************************************
 *
 * TCPConvergenceLayer
 *
 *****************************************************************************/
TCPConvergenceLayer::TCPConvergenceLayer()
    : IPConvergenceLayer("/cl/tcp")
{
}

void
TCPConvergenceLayer::init()
{
}

void
TCPConvergenceLayer::fini()
{
}

/**
 * Register a new interface.
 */
bool
TCPConvergenceLayer::add_interface(Interface* iface,
                                   int argc, const char* argv[])
{
    log_debug("adding interface %s", iface->tuple().c_str());
    
    // parse out the hostname / port from the interface
    URL url(iface->admin());
    if (! url.valid()) {
        log_err("admin part '%s' of tuple '%s' not a valid url",
                iface->admin().c_str(), iface->tuple().c_str());
        return false;
    }

    // look up the hostname in the url
    in_addr_t addr;
    if (gethostbyname(url.host_.c_str(), &addr) != 0) {
        log_err("admin host '%s' not a valid hostname", url.host_.c_str());
        return false;
    }

    // make sure the port was specified
    if (url.port_ == 0) {
        log_err("port not specified in interface admin url '%s'",
                iface->admin().c_str());
        return false;
    }

    // create a new server socket for the requested interface
    Listener* listener = new Listener();
    listener->logpathf("%s/iface/%s:%d",
                       logpath_, url.host_.c_str(), url.port_);

    if (listener->bind(addr, url.port_) != 0) {
        listener->logf(LOG_ERR, "error binding to requested socket: %s",
                       strerror(errno));
        return false;
    }

    // start listening and then start the thread to loop calling accept()
    listener->listen();
    listener->start();

    // store the new listener object in the cl specific portion of the
    // interface
    iface->set_info(listener);
    
    return true;
}

/**
 * Remove an interface
 */
bool
TCPConvergenceLayer::del_interface(Interface* iface)
{
    // grab the listener object, set a flag for the thread to stop and
    // then close the socket out from under it, which should cause the
    // thread to break out of the blocking call to accept() and
    // terminate itself
    Listener* listener = (Listener*)iface->info();
    listener->set_should_stop();
    listener->interrupt();
    
    while (! listener->is_stopped()) {
        Thread::yield();
    }

    delete listener;
    return true;
}

/**
 * Open the connection to the given contact and prepare for
 * bundles to be transmitted.
 */
bool
TCPConvergenceLayer::open_contact(Contact* contact)
{
    log_debug("opening contact *%p", contact);
    
    // parse out the hostname / port from the contact tuple
    URL url(contact->tuple().admin());
    if (! url.valid()) {
        log_err("admin part '%s' of tuple '%s' not a valid url",
                contact->tuple().admin().c_str(),
                contact->tuple().tuple().c_str());
        return false;
    }

    // look up the hostname in the url
    in_addr_t addr;
    if (gethostbyname(url.host_.c_str(), &addr) != 0) {
        log_err("admin host '%s' not a valid hostname", url.host_.c_str());
        return false;
    }

    // make sure the port was specified
    if (url.port_ == 0) {
        log_err("port not specified in contact admin url '%s'",
                contact->tuple().admin().c_str());
        return false;
    }
    
    // create a new connection for the contact
    // XXX/demmer bind?
    Connection* conn = new Connection(contact, addr, url.port_);
    contact->set_contact_info(conn);
    conn->start();

    return true;
}

/**
 * Close the connnection to the contact.
 */
bool
TCPConvergenceLayer::close_contact(Contact* contact)
{
    Connection* conn = (Connection*)contact->contact_info();

    log_info("close_contact *%p", contact);

    if (conn) {
        while (!conn->is_stopped()) {
            if (!conn->should_stop()) {
                PANIC("XXX/demmer need to stop the connection thread");
                conn->set_should_stop();
                conn->interrupt();
            }
            
            log_warn("waiting for connection thread to stop...");
            Thread::yield();
        }
        
        log_debug("thread stopped, deleting connection...");
        delete conn;
        
        contact->set_contact_info(NULL);
    }
    
    return true;
}
    
/******************************************************************************
 *
 * TCPConvergenceLayer::Listener
 *
 *****************************************************************************/
TCPConvergenceLayer::Listener::Listener()
    : TCPServerThread("/cl/tcp/listener")
{
    logfd_ = false;
    Thread::flags_ |= INTERRUPTABLE;
}

void
TCPConvergenceLayer::Listener::accepted(int fd, in_addr_t addr, u_int16_t port)
{
    log_debug("new connection from %s:%d", intoa(addr), port);
    Connection* conn = new Connection(fd, addr, port);

    // XXX/demmer this should really make a Contact equivalent or
    // something like that and then not DELETE_ON_EXIT
    conn->set_flag(Thread::DELETE_ON_EXIT);
    conn->start();
}

/******************************************************************************
 *
 * TCPConvergenceLayer::Connection
 *
 *****************************************************************************/
TCPConvergenceLayer::Connection::Connection(Contact* contact,
                                            in_addr_t remote_addr,
                                            u_int16_t remote_port)
    : contact_(contact)
{
    Thread::flags_ |= INTERRUPTABLE;

    logpathf("/cl/tcp/conn/%s:%d", intoa(remote_addr), remote_port);
    ack_blocksz_ = TCPConvergenceLayer::Defaults.ack_blocksz_;

    // cache the remote addr and port in the fields in the socket
    // since we want to actually connect to the peer side from the
    // Connection thread, not from the caller thread
    sock_ = new TCPClient();

    // XXX/demmer the basic socket logging emits errros and the like
    // when connections break. that may not be great since we kinda
    // expect them to happen... so either we should add some flag as
    // to the severity of error messages that can be passed into the
    // IO routines, or just suppress the IO output altogether
    sock_->logpathf("%s/sock", logpath_);
    sock_->set_logfd(false);
    
    sock_->set_remote_addr(remote_addr);
    sock_->set_remote_port(remote_port);
}    

TCPConvergenceLayer::Connection::Connection(int fd,
                                            in_addr_t remote_addr,
                                            u_int16_t remote_port)
    : contact_(NULL)
{
    logpathf("/cl/tcp/conn/%s:%d", intoa(remote_addr), remote_port);
    ack_blocksz_ = TCPConvergenceLayer::Defaults.ack_blocksz_;

    sock_ = new TCPClient(fd, remote_addr, remote_port, logpath_);
    sock_->set_logfd(false);
}

TCPConvergenceLayer::Connection::~Connection()
{
    delete sock_;
}

/**
 * The main loop for a connection. Based on the initial construction
 * state, it calls one of send_loop or recv_loop.
 *
 * XXX/demmer for now, this is keyed on whether or not there is a
 * contact_ cached from the constructor -- figure something better to
 * support sending bundles from the passive acceptor
 */
void
TCPConvergenceLayer::Connection::run()
{
    if (contact_) {
        send_loop();
    } else {
        recv_loop();
    }
}

/**
 * Send one bundle over the wire.
 *
 * Return true if the bundle was sent successfully, false if not.
 */
bool
TCPConvergenceLayer::Connection::send_bundle(Bundle* bundle, size_t* acked_len)
{
    int iovcnt = BundleProtocol::MAX_IOVCNT;
    struct iovec iov[iovcnt + 2];
    
    // use iov slot zero for the one byte frame header type, and
    // slot one for the frame header itself
    BundleStartHeader starthdr;
    char typecode = BUNDLE_START;
    iov[0].iov_base = &typecode;
    iov[0].iov_len  = 1;
    iov[1].iov_base = &starthdr;
    iov[1].iov_len  = sizeof(BundleStartHeader);
        
    // fill in the rest of the iovec with the bundle header
    u_int16_t header_len;
    header_len = BundleProtocol::format_headers(bundle, &iov[2], &iovcnt);
    size_t payload_len = bundle->payload_.length();
    
    log_debug("send_bundle: bundle id %d, header_length %d payload_length %d",
              bundle->bundleid_, header_len, payload_len);

    // fill in the frame header
    starthdr.bundle_id = bundle->bundleid_;
    starthdr.bundle_length = htonl(header_len + payload_len);
    starthdr.header_length = htons(header_len);

    // check to see if it will fit in one block
    size_t block_len;
    if (payload_len <= ack_blocksz_) {
        block_len = payload_len;
        starthdr.block_length = htonl(payload_len);
        starthdr.partial = 0;
    } else {
        block_len = ack_blocksz_;
        starthdr.block_length = htonl(ack_blocksz_);
        starthdr.partial = 1;
    }

    // all set to go, now write out the header (need to bump up iovcnt
    // to account for the frame header)
    log_debug("send_bundle: sending %d byte frame hdr, %d byte bundle hdr",
              1 + sizeof(BundleStartHeader), header_len);
    int cc = sock_->writevall(iov, iovcnt + 2);
    int total = 1 + sizeof(BundleStartHeader) + header_len;

    // free up the iovec data used in the header representation
    BundleProtocol::free_header_iovmem(bundle, &iov[2], iovcnt);

    // make sure the write was successful
    if (cc != total) {
        log_err("send_bundle: error writing bundle header (wrote %d/%d): %s",
                cc, total, strerror(errno));
        return false;
    }
        
    // now write out the first (and maybe only) payload block
    log_debug("send_bundle: sending %d byte first payload block", block_len);
    const char* payload_data = bundle->payload_.read_data(0, block_len);
    cc = sock_->writeall(payload_data, block_len);

    if (cc != (int)block_len) {
        log_err("send_bundle: error writing bundle block (wrote %d/%d): %s",
                cc, block_len, strerror(errno));
        return false;
    }
    
    // update payload_len and payload_offset to count the chunk we
    // just wrote
    size_t payload_offset;
    payload_offset = block_len;
    payload_len   -= block_len;

    // now loop through the rest of the payload, sending blocks and
    // checking for acks as they come in. we intentionally send a
    // second data block before checking for any acks since it's
    // likely that there's no ack to read the first time through and
    // this way we get a second block out on the wire
    bool sentok = false;
        
    while (payload_len > 0) {
        BundleBlockHeader blockhdr;

        if (payload_len <= ack_blocksz_) {
            block_len = payload_len;
        } else {
            block_len = ack_blocksz_;
        }

        // grab the next payload chunk
        payload_data = bundle->payload_.read_data(payload_offset, block_len);
            
        blockhdr.block_length = htonl(block_len);

        // iov[0] already points to the typecode
        typecode = BUNDLE_BLOCK;
        iov[1].iov_base = &blockhdr;
        iov[1].iov_len  = sizeof(BundleBlockHeader);
        iov[2].iov_base = (void*)payload_data;
        iov[2].iov_len  = block_len;

        log_debug("send_bundle: sending %d byte block %p",
                  block_len, payload_data);
        cc = sock_->writevall(iov, 3);
        total = 1 + sizeof(BundleBlockHeader) + block_len;
        if (cc != total) {
            log_warn("send_bundle: "
                     "error writing bundle block (wrote %d/%d): %s",
                     cc, block_len, strerror(errno));
            goto done;
        }

        // update payload_offset and payload_len
        payload_offset += block_len;
        payload_len    -= block_len;

        // XXX/demmer temp
        cc = handle_ack(bundle, 5000, acked_len);
        
        if (cc == IOEOF || cc == IOTIMEOUT) {
            log_warn("send_bundle: %s waiting for ack",
                     (cc == IOEOF) ? "eof" : "timeout");
            goto done;
        }
        else if (cc == IOERROR) {
            log_warn("send_bundle: error waiting for ack: %s",
                     strerror(errno));
            goto done;
        }
        
        log_debug("send_bundle: got ack for %d/%d -- looping to send more",
                  *acked_len, payload_len);
        ASSERT(cc == 1);
    }

    // now loop and block until we get all the acks, but cap the wait
    // at 5 seconds. XXX/demmer maybe this should be adjustable based
    // on the keepalive timer?
    payload_len = bundle->payload_.length();

    while (*acked_len < payload_len) {
        log_debug("send_bundle: acked %d/%d, waiting for more",
                  *acked_len, payload_len);
        
        cc = handle_ack(bundle, 5000, acked_len);
        if (cc == IOEOF || cc == IOTIMEOUT) {
            log_warn("send_bundle: %s waiting for ack",
                     (cc == IOEOF) ? "eof" : "timeout");
            goto done;
        }
        else if (cc == IOERROR) {
            log_warn("send_bundle: error waiting for ack: %s",
                     strerror(errno));
            goto done;
        }
        
        ASSERT(cc == 1);
    }

    sentok = true;

 done:
    if (*acked_len > 0) {
        ASSERT(!sentok || (*acked_len == payload_len));
    }
    
    // all done!
    return sentok;
}

/**
 * Receive one bundle over the wire. We've just consumed the one byte
 * BUNDLE_START typecode off the wire.
 */
bool
TCPConvergenceLayer::Connection::recv_bundle()
{
    char* buf = NULL;
    Bundle* bundle = NULL;

    char typecode;
    BundleStartHeader starthdr;
    BundleBlockHeader blockhdr;

    u_int16_t header_len;
    size_t bundle_len;
    size_t block_len;
    size_t rcvd_len;
    size_t payload_len;

    bool valid = false;
    bool recvok = false;

    // now read the rest of the start header
    log_debug("recv_bundle: reading start header...");
    int cc = sock_->readall((char*)&starthdr, sizeof(BundleStartHeader));
    if (cc != sizeof(BundleStartHeader)) {
        log_err("recv_bundle: error reading start header (read %d/%d): %s",
                cc, sizeof(BundleStartHeader), strerror(errno));
        return false;
    }
    
    // extract the lengths from the start header
    header_len = ntohs(starthdr.header_length);
    bundle_len = ntohl(starthdr.bundle_length);
    block_len = ntohl(starthdr.block_length);

    log_debug("recv_bundle: got start header -- bundle id %d, "
              "header_length %d bundle_length %d block_length %d",
              starthdr.bundle_id, header_len, bundle_len, block_len);
        
    // allocate and fill a buffer for the first block
    size_t buflen = header_len + block_len;
    buf = (char*)malloc(buflen);
    cc = sock_->readall(buf, buflen);
    if (cc != (int)buflen) {
        log_err("recv_bundle: "
                "error reading header / data block (read %d/%d): %s",
                cc, buflen, strerror(errno));
        goto done;
    }

    // parse the headers into the new bundle.
    //
    // note that this extracts payload_len from the headers and
    // assigns it in the bundle payload
    bundle = new Bundle();
    cc = BundleProtocol::parse_headers(bundle, (u_char*)buf, header_len);
    if (cc != (int)header_len) {
        log_err("recv_bundle: error parsing bundle headers (parsed %d/%d)",
                cc, header_len);
        goto done;
    }

    // all lengths have been parsed, so we can do some length
    // validation checks
    payload_len = bundle->payload_.length();
    if (bundle_len != header_len + payload_len) {
        log_err("recv_bundle: error in bundle lengths: "
                "bundle_length %d, header_length %d, payload_length %d",
                bundle_len, header_len, payload_len);
        goto done;
    }

    // so far so good, now tack the first block onto the payload
    bundle->payload_.append_data(buf + header_len, block_len);

    // at this point, we can make at least a valid bundle fragment
    // from what we've gotten thus far.
    valid = true;

    // ack the first block
    log_debug("recv_bundle: "
              "parsed headers and %d byte block, sending first ack",
              block_len);
    rcvd_len = block_len;
    
    if (!send_ack(starthdr.bundle_id, rcvd_len)) {
        log_err("recv_bundle: error sending ack");
        goto done;
    }

    // now loop until we're done with the rest. note that all reads
    // must have a timeout in case the other side goes away in the
    // midst of transmitting
    while (rcvd_len < payload_len) {
        log_debug("recv_bundle: acked %d/%d, reading next typecode...",
                  rcvd_len, payload_len);

        // test hook for reactive fragmentation.
        if (Defaults.test_fragment_size_ != -1) {
            if ((int)rcvd_len > Defaults.test_fragment_size_) {
                log_info("send_bundle: "
                         "XXX forcing fragmentation size %d, rcvd %d",
                         Defaults.test_fragment_size_, rcvd_len);
                usleep(100000);
                goto done;
            }
        }

        cc = sock_->timeout_read(&typecode, 1, 5000);
        if (cc == IOTIMEOUT) {
            log_warn("recv_bundle: timeout waiting for next typecode");
            goto done;
        }
        else if (cc != 1) {
            log_err("recv_bundle: error reading next typecode: %s",
                    strerror(errno));
            goto done;
        }

        if (typecode != BUNDLE_BLOCK) {
            log_err("recv_bundle: "
                    "unexpected typecode 0x%x, expected BUNDLE_BLOCK",
                    typecode);
            goto done;
        }
            
        cc = sock_->timeout_readall((char*)&blockhdr, sizeof(blockhdr),
                                    5000);
        if (cc == IOTIMEOUT) {
            log_warn("recv_bundle: timeout reading block header");
            goto done;
        }
        
        if (cc != sizeof(BundleBlockHeader)) {
            log_err("recv_bundle: "
                    "error reading block header (read %d/%d): %s",
                    cc, sizeof(BundleBlockHeader), strerror(errno));
            goto done;
        }

        block_len = ntohl(blockhdr.block_length);
        log_debug("recv_bundle: "
                  "got block header, reading next block, length %d",
                  block_len);

        // the buffer allocated above should be sufficient since
        // it covers both the original start header and the block
        if (buflen < block_len) {
            log_warn("recv_bundle: "
                     "unexpected large block length %d > buflen %d",
                     block_len, buflen);
            free(buf);
            buflen = block_len;
            buf = (char*)malloc(block_len);
        }

        cc = sock_->timeout_readall(buf, block_len, 5000);
        if (cc == IOTIMEOUT) {
            log_warn("recv_bundle: timeout reading block data");
            goto done;
        }

        if (cc != (int)block_len) {
            log_err("recv_bundle: error reading block (read %d/%d): %s",
                    cc, block_len, strerror(errno));
            goto done;
        }

        // append the block and send the ack
        bundle->payload_.append_data(buf, block_len);
        rcvd_len += block_len;
        if (! send_ack(starthdr.bundle_id, rcvd_len)) {
            log_err("recv_bundle: error sending ack");
            goto done;
        }
    }

    recvok = true;

 done:
    free(buf);
    
    if (!valid) {
        // the bundle isn't valid (maybe the headers couldn't be
        // parsed) so just return
        if (bundle)
            delete bundle;
        
        return false;
    }

    log_debug("recv_bundle: "
              "new bundle id %d arrival, payload length %d (rcvd %d)",
              bundle->bundleid_, payload_len, rcvd_len);
    
    // we've got a valid bundle, but check if we didn't get the whole
    // bundle and therefore that this should be marked as a fragment
    BundleForwarder::post(new BundleReceivedEvent(bundle, rcvd_len));
    ASSERT(bundle->refcount() > 0);
    
    return recvok;
}

/**
 * Send an ack message.
 */
bool
TCPConvergenceLayer::Connection::send_ack(u_int32_t bundle_id,
                                          size_t acked_len)
{
    char typecode = BUNDLE_ACK;
    
    BundleAckHeader ackhdr;
    ackhdr.bundle_id = bundle_id;
    ackhdr.acked_length = htonl(acked_len);

    struct iovec iov[2];
    iov[0].iov_base = &typecode;
    iov[0].iov_len  = 1;
    iov[1].iov_base = &ackhdr;
    iov[1].iov_len  = sizeof(BundleAckHeader);
    
    int total = 1 + sizeof(BundleAckHeader);
    int cc = sock_->writevall(iov, 2);
    if (cc != total) {
        log_err("recv_bundle: error sending ack (wrote %d/%d): %s",
                cc, total, strerror(errno));
        return false;
    }

    return true;
}

/**
 * Handle an incoming ack, updating the acked length count. Timeout
 * specifies the amount of time the sender is willing to wait for the
 * ack.
 */
int
TCPConvergenceLayer::Connection::handle_ack(Bundle* bundle, int timeout,
                                            size_t* acked_len)
{
    char typecode;
    size_t new_acked_len;

    // first try to read in the typecode and check for timeout
    int cc = sock_->timeout_read(&typecode, 1, timeout);
    if (cc == IOEOF || cc == IOERROR || cc == IOTIMEOUT)
    {
        return cc;
    }
    ASSERT(cc == 1);

    // ignore KEEPALIVE messages 
    if (typecode == KEEPALIVE) {
        return 1;
    }

    // other than that, all we should get are acks
    if (typecode != BUNDLE_ACK) {
        log_err("handle_ack: unexpected frame header type code 0x%x",
                typecode);
        return IOERROR;
    }

    // now just read in the ack header and validate the acked length
    // and the bundle id
    BundleAckHeader ackhdr;
    cc = sock_->read((char*)&ackhdr, sizeof(BundleAckHeader));
    if (cc != sizeof(BundleAckHeader))
    {
        log_err("handle_ack: error reading ack header (got %d/%d): %s",
                cc, sizeof(BundleAckHeader), strerror(errno));
        return IOERROR;
    }

    new_acked_len = ntohl(ackhdr.acked_length);
    size_t payload_len = bundle->payload_.length();
    
    log_debug("handle_ack: got ack length %d for bundle id %d length %d",
              new_acked_len, ackhdr.bundle_id, payload_len);

    if (ackhdr.bundle_id != bundle->bundleid_)
    {
        log_err("handle_ack: error: bundle id mismatch %d != %d",
                ackhdr.bundle_id, bundle->bundleid_);
        return IOERROR;
    }

    if (new_acked_len < *acked_len ||
        new_acked_len > payload_len)
    {
        log_err("handle_ack: invalid acked length %d (acked %d, bundle %d)",
                new_acked_len, *acked_len, payload_len);
        return IOERROR;
    }

    *acked_len = new_acked_len;
    return 1;
}

/**
 * Sender side of the initial handshake. First connect to the peer
 * side, then exchange ContactHeaders and negotiate session
 * parameters.
 */
bool
TCPConvergenceLayer::Connection::connect(in_addr_t remote_addr,
                                         u_int16_t remote_port)
{
    // send the contact header
    ContactHeader contacthdr;
    contacthdr.version = CURRENT_VERSION;
    contacthdr.flags = BUNDLE_ACK_ENABLED | BLOCK_ACK_ENABLED |
                       KEEPALIVE_ENABLED;
    contacthdr.ackblock_sz = ack_blocksz_;
    contacthdr.keepalive_sec = TCPConvergenceLayer::Defaults.keepalive_interval_;
    keepalive_msec_ = contacthdr.keepalive_sec * 1000;
    
    // first of all, connect to the receiver side
    ASSERT(sock_->state() != IPSocket::ESTABLISHED);
    log_debug("send_loop: connecting to %s:%d...",
              intoa(sock_->remote_addr()), sock_->remote_port());
    while (sock_->connect(sock_->remote_addr(), sock_->remote_port()) != 0)
    {
        log_info("connection attempt to %s:%d failed... "
                 "will retry in %d seconds",
                 intoa(sock_->remote_addr()), sock_->remote_port(),
                 contacthdr.keepalive_sec);
        sleep(contacthdr.keepalive_sec);
    }

    log_debug("send_loop: "
              "connection established, sending contact header handshake.,.");
    int cc = sock_->writeall((char*)&contacthdr, sizeof(ContactHeader));
    
    if (cc != sizeof(ContactHeader)) {
        log_err("error writing contact header (wrote %d/%d): %s",
                cc, sizeof(ContactHeader), strerror(errno));
        return false;
    }

    // wait for the reply
    cc = sock_->timeout_readall((char*)&contacthdr, sizeof(ContactHeader),
                                keepalive_msec_);

    if (cc == IOTIMEOUT) {
        log_warn("timeout reading contact header");
        return false;
    }
    
    if (cc != sizeof(ContactHeader)) {
        log_err("error reading contact header reply (read %d/%d): %s", 
                cc, sizeof(ContactHeader), strerror(errno));
        return false;
    }

    return true;
}

/**
 * Receiver side of the initial handshake.
 */
bool
TCPConvergenceLayer::Connection::accept()
{
    ASSERT(contact_ == NULL);

    // read and write back the contact header
    ContactHeader contacthdr;
    log_debug("recv_bundle: waiting for contact header...");
    int cc = sock_->readall((char*)&contacthdr, sizeof(ContactHeader));
    if (cc != sizeof(ContactHeader)) {
        log_err("error reading contact header (read %d/%d): %s",
                cc, sizeof(ContactHeader), strerror(errno));
        return false;
    }

    // XXX/demmer do some "negotiation" here
    
    keepalive_msec_ = contacthdr.keepalive_sec * 1000;
    
    log_debug("recv_bundle: got contact header, sending reply...");
    cc = sock_->writeall((char*)&contacthdr, sizeof(ContactHeader));
    if (cc != sizeof(ContactHeader)) {
        log_err("error reading contact header reply (read %d/%d): %s", 
                cc, sizeof(ContactHeader), strerror(errno));
        return false;
    }

    return true;
}

/**
 * Send an event to the system indicating that this contact is broken
 * and close the side of the connection.
 *
 * This results in the Connection thread stopping and the system
 * calling the contact->close() call which cleans up the connection.
 */
void
TCPConvergenceLayer::Connection::break_contact()
{
    ASSERT(sock_);
    sock_->close();

    // In all cases where break_contact is called, the Connection
    // thread is about to terminate. However, once the
    // ContactBrokenEvent is posted, TCPCL::close_contact() will try
    // to stop the Connection thread. So we preemptively indicate in
    // the thread flags that we're gonna stop shortly.
    set_should_stop();

    if (contact_)
        BundleForwarder::post(new ContactBrokenEvent(contact_));
}

/**
 * The sender side main thread loop.
 */
void
TCPConvergenceLayer::Connection::send_loop()
{
    Bundle* bundle;
    char typecode;
    int ret;

    // first connect to the remote side
    if (connect(sock_->remote_addr(), sock_->remote_port()) == false) {
        break_contact();
        return;
    }

    // extract the agreed-upon keepalive timer, and set up the timer
    // event queue and the timer object itself 
    log_debug("send_loop: got contact header: keepalive %d milliseconds",
              keepalive_msec_);
    
    // build up a poll vector since we need to block below on input
    // from both the socket and the bundle list notifier
    struct pollfd pollfds[2];
    
    struct pollfd* bundle_poll = &pollfds[0];
    bundle_poll->fd = contact_->bundle_list()->read_fd();
    bundle_poll->events = POLLIN;
    
    struct pollfd* sock_poll = &pollfds[1];
    sock_poll->fd = sock_->fd();
    sock_poll->events = POLLIN;

    // keep track of the time we got data
    struct timeval now, data_rcvd, keepalive_sent;

    // main loop
    while (1) {
        // first see if someone wants us to stop
        // XXX/demmer debug this and make it a clean close_contact
        if (should_stop()) {
            break_contact();
            return;
        }
        
        // first check the contact's bundle list to see if there's a
        // bundle already there, but don't pop it off the list yet
        bundle = contact_->bundle_list()->front();

        if (bundle) {
            size_t acked_len = 0;
            
            // we got a bundle, so send it off. 
            bool sentok = send_bundle(bundle, &acked_len);
            
            // if any data was acked, pop the bundle off the contact
            // list (making sure it's still the right bundle) and
            // inform the router. note that pop_front does _not_
            // decrement the reference count, so we now have a local
            // reference to the bundle
            if (acked_len != 0) {
                Bundle* bundle2 = contact_->bundle_list()->pop_front();
                ASSERT(bundle == bundle2);

                // XXX/demmer this won't be true wrt expiration...
                // maybe mark the bundle as in-progress or something
                // like that?
                
                BundleForwarder::post(
                    new BundleTransmittedEvent(bundle, contact_, acked_len, true));
                
                bundle->del_ref("tcpcl");
                bundle = NULL;
            }

            // if the last transmission wasn't completely successful,
            // it's time to break the contact
            if (!sentok) {
                goto shutdown;
            }

            // otherwise, we're all good, so mark that we got some
            // data and loop again
            ::gettimeofday(&data_rcvd, 0);
            
            continue;
        }

        // no bundle, so we'll block waiting for a) some activity on
        // the socket, i.e. a keepalive or shutdown, or b) the bundle
        // list notifier indicating there's a new bundle for us to
        // send
        //
        // note that we pass the negoatiated keepalive as the timeout
        // to the poll call to make sure the other side sends its
        // keepalive in time
        pollfds[0].revents = 0;
        pollfds[1].revents = 0;
        pollfds[2].revents = 0;
        
        log_debug("send_loop: calling poll (timeout %d)", keepalive_msec_);
        int nready = poll(pollfds, 2, keepalive_msec_);
        if (nready < 0) {
            if (errno == EINTR) continue;
            log_err("error return %d from poll: %s", nready, strerror(errno));
            goto shutdown;
        }

        // check for a message (or shutdown) from the other side
        if (sock_poll->revents != 0) {
            if ((sock_poll->revents & POLLHUP) ||
                (sock_poll->revents & POLLERR))
            {
                log_warn("send_loop: remote connection error");
                goto shutdown;
            }

            if (! (sock_poll->revents & POLLIN)) {
                PANIC("unknown revents value 0x%x", sock_poll->revents);
            }

            log_debug("send_loop: data available on the socket");
            ret = sock_->read(&typecode, 1);
            if (ret != 1) {
                log_warn("send_loop: "
                         "remote connection unexpectedly closed");
                goto shutdown;
            }

            if (typecode != KEEPALIVE) {
                log_warn("send_loop: "
                         "got unexpected frame code %d", typecode);
                goto shutdown;
            }

            // mark that we got a keepalive as expected
            ::gettimeofday(&data_rcvd, 0);
        }
        
        // if the bundle list was triggered, we check the list at the
        // beginning of the loop, but make sure to drain the pipe here
        if (bundle_poll->revents != 0) {
            ASSERT(bundle_poll->revents == POLLIN);
            ASSERT(contact_->bundle_list()->front() != NULL);
            contact_->bundle_list()->drain_pipe();
        }

        // if nready is zero then the command timed out, implying that
        // it's time to send a keepalive.
        if (nready == 0) {
            log_debug("send_loop: timeout fired, sending keepalive");
            typecode = KEEPALIVE;
            ret = sock_->write(&typecode, 1);
            if (ret != 1) {
                log_warn("send_loop: "
                         "remote connection unexpectedly closed");
                goto shutdown;
            }

            ::gettimeofday(&keepalive_sent, 0);
        }

        // check that it hasn't been too long since the other side
        // sent us a keepalive
        ::gettimeofday(&now, 0);
        int elapsed = TIMEVAL_DIFF_MSEC(now, data_rcvd);
        if (elapsed > (2 * keepalive_msec_)) {
            log_warn("send_loop: no keepalive heard for %d msecs "
                     "(sent %u.%u, rcvd %u.%u, now %u.%u -- closing contact",
                     elapsed,
                     (u_int)keepalive_sent.tv_sec, (u_int)keepalive_sent.tv_usec,
                     (u_int)data_rcvd.tv_sec, (u_int)data_rcvd.tv_usec,
                     (u_int)now.tv_sec, (u_int)now.tv_usec);
            goto shutdown;
        }
    }
    
 shutdown:
    break_contact();
}

void
TCPConvergenceLayer::Connection::recv_loop()
{
    char typecode;

    // first accept the connection and do negotiation
    if (accept() == false) {
        break_contact();
        return;
    }
    
    while (1) {
        // block on the one byte typecode
        int timeout = 2 * keepalive_msec_;
        log_debug("recv_loop: blocking on frame typecode... (timeout %d)",
                  timeout);
        int ret = sock_->timeout_read(&typecode, 1, timeout);
        if (ret == IOEOF || ret == IOERROR) {
            log_warn("recv_loop: remote connection unexpectedly closed");
 shutdown:
            break_contact();
            return;
            
        } else if (ret == IOTIMEOUT) {
            log_warn("recv_loop: no message heard for > %d msecs -- "
                     "breaking contact", 2 * keepalive_msec_);
            goto shutdown;
        }
        
        ASSERT(ret == 1);

        log_debug("recv_loop: got frame packet type 0x%x...", typecode);

        if (typecode == KEEPALIVE) {
            log_debug("recv_loop: "
                      "got keepalive, sending response");
            ret = sock_->write(&typecode, 1);
            if (ret != 1) {
                log_warn("recv_loop: remote connection unexpectedly closed");
                goto shutdown;
            }
            continue;
        }
        
        if (typecode != BUNDLE_START) {
            log_err("recv_loop: "
                    "unexpected typecode 0x%x waiting for BUNDLE_START",
                    typecode);
            goto shutdown;
        }

        // process the bundle
        if (! recv_bundle()) {
            goto shutdown;
        }
     }
}
