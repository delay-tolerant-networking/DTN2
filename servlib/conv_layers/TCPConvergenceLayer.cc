
#include "TCPConvergenceLayer.h"
#include "bundling/Bundle.h"
#include "bundling/BundleForwarding.h"
#include "bundling/BundleProtocol.h"
#include "bundling/BundleList.h"
#include "io/NetUtils.h"
#include "util/URL.h"
#include <sys/signal.h>

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
    log_debug("adding interface %s", iface->tuple()->c_str());
    
    // parse out the hostname / port from the interface
    URL url(iface->admin());
    if (! url.valid()) {
        log_err("admin part '%s' of tuple '%s' not a valid url",
                iface->admin().c_str(), iface->tuple()->c_str());
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
    listener->logpathf("%s/iface/%s:%d", logpath_, url.host_.c_str(), url.port_);

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
    conn->start();

    return true;
}

/**
 * Close the connnection to the contact.
 */
bool
TCPConvergenceLayer::close_contact(Contact* contact)
{
    NOTIMPLEMENTED;
    return false;
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
    : TCPClient("/cl/tcp/connection"),
      contact_(contact)
{
    Thread::flags_ |= INTERRUPTABLE;
    
    remote_addr_ = remote_addr;
    remote_port_ = remote_port;
}

TCPConvergenceLayer::Connection::Connection(int fd,
                                            in_addr_t local_addr,
                                            u_int16_t local_port)
    : TCPClient(fd, local_addr, local_port, "/cl/tcp/connection"),
      contact_(NULL)
{
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
 * The loop for the sender side of a convergence layer connection.
 * 
 */
void
TCPConvergenceLayer::Connection::send_loop()
{
    while (1) {
        Bundle* bundle;
        int iovcnt = 8;
        int total;
        struct iovec iov[iovcnt + 1];

        // grab a bundle and format the iovec with it, leaving a space
        // for the framing header
        bundle = contact_->bundle_list()->pop_blocking();

        log_debug("got bundle %p, starting to format", bundle);
        
        total = BundleProtocol::fill_iov(bundle, &iov[1], &iovcnt);

        log_debug("format complete %d byte bundle", total);
        
        // stuff in the framing header (just the total length for now)
        iov[0].iov_base = &total;
        iov[0].iov_len = 4;
                
        // we've got something to do, see if we need to try to connect
        if (state_ != ESTABLISHED) {
            while (connect(remote_addr_, remote_port_) != 0) {
                log_info("connection attempt to %s:%d failed... will retry in 10 seconds",
                         intoa(remote_addr_), remote_port_);
                sleep(10);
            }
        }

        if (writevall(iov, iovcnt + 1) <= 0) {
            log_err("error writing out bundle");
            // XXX error could be the other side shutting down, so
            // need to deal with the contact going away
            return;
        }

        // free up the data used by the flattened representation
        BundleProtocol::free_iovmem(bundle, &iov[1], iovcnt);

        // remove the reference on the bundle and clean it up
        bundle->delref();
        if (bundle->refcount() == 0) {
            delete bundle;
        }
    }
}

void
TCPConvergenceLayer::Connection::recv_loop()
{
    while (1) {
        // read the framing header (just the length for now)
        int total;
        if (read((char*)&total, 4) != 4) {
            log_err("error reading length, bailing");
            return;
        }
        log_debug("got frame header of %d byte bundle", total);

        // allocate and fill the buffer
        char* buf = (char*)malloc(total);
        if (readall(buf, total) != total) {
            log_err("error reading data, bailing");
            free(buf);
        }


        // now create a new bundle and parse it
        Bundle* bundle = new Bundle();
        if (! BundleProtocol::parse_buf(bundle, (u_char*)buf, total)) {
            log_err("error parsing bundle, bailing");
            delete bundle;
            free(buf);
            return;
        }

        // finally, deliver the bundle for forwarding
        BundleForwarding::instance()->input(bundle);

        // clean up the buffer and do another
        free(buf);
    }
}
