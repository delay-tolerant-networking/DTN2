
#include "TCPConvergenceLayer.h"
#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleForwarder.h"
#include "bundling/BundleList.h"
#include "bundling/BundleProtocol.h"
#include "io/NetUtils.h"
#include "util/URL.h"

#include <sys/poll.h>

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

    if (!conn->is_stopped()) {
        PANIC("XXX/demmer need to stop the connection thread");
    }

    delete conn;
    contact->set_contact_info(NULL);
    
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
    sock_ = new TCPClient(logpath_);
    sock_->set_logfd(false);
    
    sock_->set_remote_addr(remote_addr);
    sock_->set_remote_port(remote_port);
    ack_blocksz_ = TCPConvergenceLayer::Defaults.ack_blocksz_;
}

TCPConvergenceLayer::Connection::Connection(int fd,
                                            in_addr_t remote_addr,
                                            u_int16_t remote_port)
    : contact_(NULL)
{
    logpathf("/cl/tcp/conn/%s:%d", intoa(remote_addr), remote_port);
    sock_ = new TCPClient(fd, remote_addr, remote_port, logpath_);
    sock_->set_logfd(false);
    
    ack_blocksz_ = TCPConvergenceLayer::Defaults.ack_blocksz_;
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
 * Return the amount of payload acked, or -1 on error.
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
    header_len = BundleProtocol::fill_header_iov(bundle, &iov[2], &iovcnt);
    size_t payload_len = bundle->payload_.length();
    
    log_debug("send_bundle: bundle id %d, header_length %d payload_length %d",
              bundle->bundleid_, header_len, payload_len);

    // now fill in the frame header
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
    if (cc != total) {
        log_err("send_bundle: error writing bundle header (wrote %d/%d): %s",
                cc, total, strerror(errno));

        // XXX error could be the other side going away, so need
        // to deal with the contact going away
        return false;
    }
        
    // free up the iovec data used in the header representation
    BundleProtocol::free_header_iovmem(bundle, &iov[2], iovcnt);
        
    // now write out the first (and maybe only) payload block
    log_debug("send_bundle: sending %d byte first payload block", block_len);
    const char* payload_data = bundle->payload_.read_data(0, block_len);
    log_debug("send_bundle: block %s", payload_data);
    cc = sock_->writeall(payload_data, block_len);

    if (cc != (int)block_len) {
        log_err("send_bundle: error writing bundle block (wrote %d/%d): %s",
                cc, block_len, strerror(errno));
            
        // XXX error could be the other side going away, so need
        // to deal with the contact going away
        return false;
    }

    // update payload_len and payload_offset to count the chunk we
    // just wrote
    size_t payload_offset;
    payload_offset = block_len;
    payload_len   -= block_len;
    

    // loop through the rest of the payload, sending blocks and
    // checking for acks as they come in. we intentionally send a
    // second data block before checking for any acks since it's
    // likely that there's no ack to read the first time through and
    // this way we get a second block out on the wire
    *acked_len = 0;
        
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
            log_err("error writing bundle block (wrote %d/%d): %s",
                    cc, block_len, strerror(errno));
            
            // XXX error could be the other side going away, so need
            // to deal with the contact going away
            return false;
        }

        // update payload_offset and payload_len
        payload_offset += block_len;
        payload_len    -= block_len;

        // check for acks. we use timeout_read() with a 0ms timeout so
        // we don't block
        cc = handle_ack(bundle, 0, acked_len);
        if (cc == IOEOF || cc == IOERROR)
        {
            log_err("error checking for ack: %d", cc);
            
            // XXX error could be the other side going away, so need
            // to deal with the contact going away
            return false;
        }
        else if (cc == IOTIMEOUT)
        {
            // no ack there yet, but that's ok...
        }
        else
        {
            ASSERT(cc == 1);
        }
    }

    // now loop and block until we get all the acks, but cap the wait
    // at 5 seconds. 
    payload_len = bundle->payload_.length();
    while (*acked_len < payload_len) {
        log_debug("send_bundle: acked %d/%d, waiting for more",
                  *acked_len, payload_len);
        cc = handle_ack(bundle, 5000, acked_len);
        if (cc == IOEOF || cc == IOERROR || cc == IOTIMEOUT)
        {
            log_err("error or timeout waiting for ack: %d", cc);
            return false;
        }
        
        ASSERT(cc == 1);
    }

    // all done!
    return true;
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
        log_err("recv_loop: error sending ack (wrote %d/%d): %s",
                cc, total, strerror(errno));
        return false;
    }

    return true;
}

/**
 * Handle an incoming ack, updating the acked length count.
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
 * The sender side main thread loop.
 */
void
TCPConvergenceLayer::Connection::send_loop()
{
    // first of all, connect to the receiver side
    ASSERT(sock_->state() != IPSocket::ESTABLISHED);
    log_debug("send_loop: connecting to %s:%d...",
              intoa(sock_->remote_addr()), sock_->remote_port());
    while (sock_->connect(sock_->remote_addr(), sock_->remote_port()) != 0)
    {
        log_info("connection attempt to %s:%d failed... "
                 "will retry in 10 seconds",
                 intoa(sock_->remote_addr()), sock_->remote_port());
        sleep(10);
    }
    
    // send the contact header
    ContactHeader contacthdr;
    contacthdr.version = CURRENT_VERSION;
    contacthdr.flags = BUNDLE_ACK_ENABLED | BLOCK_ACK_ENABLED |
                       KEEPALIVE_ENABLED;
    contacthdr.ackblock_sz = ack_blocksz_;
    contacthdr.keepalive_sec = 2;

    int keepalive_msec = contacthdr.keepalive_sec * 1000;
    
    log_debug("send_loop: "
              "connection established, sending contact header handshake.,.");
    int cc = sock_->writeall((char*)&contacthdr, sizeof(ContactHeader));
    if (cc != sizeof(ContactHeader)) {
        log_err("error writing contact header (wrote %d/%d): %s",
                cc, sizeof(ContactHeader), strerror(errno));
 shutdown:
        sock_->close();
        Thread::set_flag(STOPPED);
        contact_->close();
        return;
    }

    // wait for the reply
    cc = sock_->readall((char*)&contacthdr, sizeof(ContactHeader));
    if (cc != sizeof(ContactHeader)) {
        log_err("error reading contact header reply (read %d/%d): %s", 
                cc, sizeof(ContactHeader), strerror(errno));
        goto shutdown;
    }

    // extract the agreed-upon keepalive timer, and set up the timer
    // event queue and the timer object itself
    log_debug("send_loop: got contact header: keepalive %d milliseconds",
              keepalive_msec);
    
    // build up a poll vector so we can block waiting for a) bundle
    // input from the main daemon thread, b) a message from the other
    // side, or c) the keepalive timer firing
    struct pollfd pollfds[3];
    struct pollfd* bundle_poll 	= &pollfds[0];
    struct pollfd* sock_poll 	= &pollfds[1];
    struct pollfd* timer_poll 	= &pollfds[2];
    
    bundle_poll->fd = contact_->bundle_list()->read_fd();
    bundle_poll->events = POLLIN | POLLPRI;
    sock_poll->fd = sock_->fd();
    sock_poll->events = POLLIN | POLLPRI;
    timer_poll->fd = 0;

    // XXX/demmer todo: add timer poll

    // main loop
    while (1) {
        Bundle* bundle;

        // first check the contact's bundle list to see if there's a
        // bundle already there... if not, we'll block waiting for
        // input either from the remote side (i.e. a keepalive) or the
        // bundle list notifier
        //
        // note that pop_front does _not_ decrement the reference
        // count, so we now have a local reference to the bundle

        bundle = contact_->bundle_list()->pop_front();

        if (!bundle) {
            pollfds[0].revents = 0;
            pollfds[1].revents = 0;
            pollfds[2].revents = 0;
        
            log_debug("send_loop: calling poll");
            int ret = poll(pollfds, 2, -1);
            if (ret <= 0) {
                if (errno == EINTR) continue;
                log_err("error return %d from poll: %s", ret, strerror(errno));
                goto shutdown;
            }
            log_debug("send_loop: poll returned %d", ret);

            // check for a message (or shutdown) from the other side
            if (sock_poll->revents != 0) {
                if ((sock_poll->revents & POLLHUP) ||
                    (sock_poll->revents & POLLERR))
                {
                    log_warn("send_loop: "
                             "remote connection unexpectedly closed");
                    goto shutdown;
                }
                else if ((sock_poll->revents & POLLIN) ||
                         (sock_poll->revents & POLLPRI))
                {
                    char typecode;
                    int ret = sock_->read(&typecode, 1);
                    if (ret == -1 || ret == 0)
                    {
                        log_warn("send_loop: "
                                 "remote connection unexpectedly closed");
                        goto shutdown;
                    }

                    log_err("XXX/demmer todo handle keepalive");
                    goto shutdown;
                }
                else
                {
                    PANIC("unknown revents value 0x%x", sock_poll->revents);
                }
            }

            // XXX/demmer check keepalive timer
        
            // if the bundle list wasn't triggered, loop again waiting
            // for input
            if (bundle_poll->revents == 0) {
                continue;
            }

            // otherwise, we've been notified that a bundle is there,
            // so grab it off the list
            ASSERT(bundle_poll->revents == POLLIN);
            bundle = contact_->bundle_list()->pop_front();
            ASSERT(bundle);
            contact_->bundle_list()->drain_pipe();
        }

        // we got a legit bundle, send it off
        size_t acked_len = 0;
        if (! send_bundle(bundle, &acked_len)) {
            goto shutdown;
        }

        ASSERT(acked_len > 0);

        // cons up a transmission event and pass it to the router
        BundleForwarder::post(
            new BundleTransmittedEvent(bundle, contact_, acked_len, true));
        
        // finally, remove our local reference on the bundle, which
        // may delete it
        bundle->del_ref();
        bundle = NULL;
    }

    goto shutdown;
}

void
TCPConvergenceLayer::Connection::recv_loop()
{
    ASSERT(contact_ == NULL); // XXX/demmer for now

    // declare dynamic storage elements that may need to be cleaned up
    // on error
    char* buf = NULL;
    Bundle* bundle = NULL;
        
    // read and write back the contact header
    ContactHeader contacthdr;
    log_debug("recv_loop: waiting for contact header...");
    int cc = sock_->readall((char*)&contacthdr, sizeof(ContactHeader));
    if (cc != sizeof(ContactHeader)) {
        log_err("error reading contact header (read %d/%d): %s",
                cc, sizeof(ContactHeader), strerror(errno));
 shutdown:
        if (buf)
            free(buf);

        if (bundle)
            delete bundle;
        
        sock_->close();
        return;
    }
    
    log_debug("recv_loop: got contact header, sending reply...");
    cc = sock_->writeall((char*)&contacthdr, sizeof(ContactHeader));
    if (cc != sizeof(ContactHeader)) {
        log_err("error reading contact header reply (read %d/%d): %s", 
                cc, sizeof(ContactHeader), strerror(errno));
        goto shutdown;
    }

    char typecode;
    BundleStartHeader starthdr;
    BundleBlockHeader blockhdr;
    
    u_int16_t header_len;
    size_t bundle_len;
    size_t block_len;
    size_t acked_len;
    
    while (1) {
        // block on the one byte typecode
        log_debug("recv_loop: blocking on frame typecode...");
        cc = sock_->read(&typecode, 1);
        if (cc == 0) {
            log_warn("recv_loop: remote connection unexpectedly closed");
            goto shutdown;
            
        }
        if (cc != 1) {
            log_err("recv_loop: error reading frame type code: %s",
                    strerror(errno));
            goto shutdown;
        }

        log_debug("recv_loop: got frame packet type 0x%x...", typecode);
        if (typecode != BUNDLE_START) {
            log_err("recv_loop: "
                    "unexpected typecode 0x%x waiting for BUNDLE_START",
                    typecode);
            goto shutdown;
        }

        // now read the rest of the start header
        log_debug("recv_loop: reading start header...");
        cc = sock_->readall((char*)&starthdr, sizeof(BundleStartHeader));
        if (cc != sizeof(BundleStartHeader)) {
            log_err("recv_loop: error reading start header (read %d/%d): %s",
                    cc, sizeof(BundleStartHeader), strerror(errno));
            goto shutdown;
        }

        // extract the lengths from the start header
        header_len = ntohs(starthdr.header_length);
        bundle_len = ntohl(starthdr.bundle_length);
        block_len = ntohl(starthdr.block_length);

        log_debug("recv_loop: got start header -- bundle id %d, "
                  "header_length %d bundle_length %d block_length %d",
                  starthdr.bundle_id, header_len, bundle_len, block_len);
        
        // allocate and fill a buffer for the first block
        size_t buflen = header_len + block_len;
        buf = (char*)malloc(buflen);

        cc = sock_->readall(buf, buflen);
        if (cc != (int)buflen) {
            log_err("recv_loop: "
                    "error reading header / data block (read %d/%d): %s",
                    cc, buflen, strerror(errno));
            goto shutdown;
        }
        
        // parse the headers into a new bundle. this sets the
        // payload_len appropriately
        bundle = new Bundle();
        cc = BundleProtocol::parse_headers(bundle, (u_char*)buf, header_len);
        if (cc != (int)header_len) {
            log_err("recv_loop: error parsing bundle headers (parsed %d/%d)",
                    cc, header_len);
            goto shutdown;
        }

        // do some length validation checks
        size_t payload_len = bundle->payload_.length();
        if (bundle_len != header_len + payload_len) {
            log_err("recv_loop: error in bundle lengths: "
                    "bundle_length %d, header_length %d, payload_length %d",
                    bundle_len, header_len, payload_len);
            goto shutdown;
        }

        // so far so good, now tack the first block onto the payload
        bundle->payload_.append_data(buf + header_len, block_len);

        // ack the first block
        log_debug("recv_loop: "
                  "parsed headers and %d byte block, sending first ack",
                  block_len);
        acked_len = block_len;
        if (!send_ack(starthdr.bundle_id, acked_len)) {
            log_err("recv_loop: error sending ack");
            goto shutdown;
        }

        // now loop until we're done with the rest
        while (acked_len < payload_len) {
            log_debug("recv_loop: acked %d/%d, reading next typecode...",
                      acked_len, payload_len);

            cc = sock_->readall(&typecode, 1);
            if (cc != 1) {
                log_err("recv_loop: error reading next typecode: %s",
                        strerror(errno));
                goto shutdown;
            }

            if (typecode != BUNDLE_BLOCK) {
                log_err("recv_loop: "
                        "unexpected typecode 0x%x, expected BUNDLE_BLOCK",
                        typecode);
                goto shutdown;
            }
            
            cc = sock_->readall((char*)&blockhdr, sizeof(BundleBlockHeader));
            if (cc != sizeof(BundleBlockHeader)) {
                log_err("recv_loop: "
                        "error reading block header (read %d/%d): %s",
                        cc, sizeof(BundleBlockHeader), strerror(errno));
                goto shutdown;
            }

            block_len = ntohl(blockhdr.block_length);
            log_debug("recv_loop: "
                      "got block header, reading next block, length %d",
                      block_len);

            // the buffer allocated above should be sufficient since
            // it covers both the original start header and the block
            if (buflen < block_len) {
                log_warn("recv_loop: "
                         "unexpected large block length %d > buflen %d",
                         block_len, buflen);
                free(buf);
                buflen = block_len;
                buf = (char*)malloc(block_len);
            }

            cc = sock_->readall(buf, block_len);
            if (cc != (int)block_len) {
                log_err("recv_loop: error reading block (read %d/%d): %s",
                        cc, block_len, strerror(errno));
                goto shutdown;
            }

            // append the block and send the ack
            bundle->payload_.append_data(buf, block_len);
            acked_len += block_len;
            if (! send_ack(starthdr.bundle_id, acked_len)) {
                log_err("recv_loop: error sending ack");
                goto shutdown;
            }
        }

        // all set, notify the router of the new arrival
        log_debug("recv_loop: new bundle id %d arrival, payload length %d",
                  bundle->bundleid_, bundle->payload_.length());
        BundleForwarder::post(new BundleReceivedEvent(bundle));
        ASSERT(bundle->refcount() > 0);
        
        bundle = NULL;
        free(buf);
        buf = NULL;
     }
}
