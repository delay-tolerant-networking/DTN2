#ifndef _TCP_CONVERGENCE_LAYER_H_
#define _TCP_CONVERGENCE_LAYER_H_

#include "IPConvergenceLayer.h"
#include "io/TCPClient.h"
#include "io/TCPServer.h"

class TCPConvergenceLayer : public IPConvergenceLayer {
public:
    /**
     * Constructor.
     */
    TCPConvergenceLayer();
    
    /**
     * The meat of the initialization happens here.
     */
    virtual void init();

    /**
     * Hook for shutdown.
     */
    virtual void fini();

    /**
     * Register a new interface.
     */
    bool add_interface(Interface* iface, int argc, const char* argv[]);

    /**
     * Remove an interface
     */
    bool del_interface(Interface* iface);

protected:
    /**
     * Helper class (and thread) that listens on a registered
     * interface for new connections.
     */
    class Listener : public InterfaceInfo, public TCPServerThread {
    public:
        Listener();
        void accepted(int fd, in_addr_t addr, u_int16_t port);
    };

    /**
     * Helper class (and thread) that manages an established
     * connection with a peer daemon.
     *
     * This is stored in the 
     */
    class Connection : public ContactInfo, TCPClient, public Thread {
    public:
        Connection(int fd, in_addr_t local_addr, u_int16_t local_port);

    protected:
        
    };
};

#endif /* _TCP_CONVERGENCE_LAYER_H_ */
