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

    /**
     * Open the connection to the given contact and prepare for
     * bundles to be transmitted.
     */
    bool open_contact(Contact* contact);
    
    /**
     * Close the connnection to the contact.
     */
    bool close_contact(Contact* contact);

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
     */
    class Connection : public ContactInfo, public TCPClient, public Thread {
    public:
        /**
         * Constructor for the active connection side of a connection.
         */
        Connection(Contact* contact,
                   in_addr_t remote_addr,
                   u_int16_t remote_port);
        
        /**
         * Constructor for the passive listener side of a connection.
         */
        Connection(int fd,
                   in_addr_t remote_addr,
                   u_int16_t remote_port);
        
    protected:
        virtual void run();
        void send_loop();
        void recv_loop();
        
        Contact* contact_;
    };
};

#endif /* _TCP_CONVERGENCE_LAYER_H_ */
