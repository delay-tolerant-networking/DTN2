
#include "TCPConvergenceLayer.h"
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
    listener->stop();
    listener->interrupt();
    
    while (! listener->is_stopped()) {
        Thread::yield();
    }

    delete listener;
    return true;
}

TCPConvergenceLayer::Listener::Listener()
    : TCPServerThread()
{
    logfd_ = false;
    Thread::flags_ |= INTERRUPTABLE;
}

void
TCPConvergenceLayer::Listener::accepted(int fd, in_addr_t addr, u_int16_t port)
{
    log_debug("new connection from %s:%d", intoa(addr), port);
}

