#ifndef _TCP_CONVERGENCE_LAYER_H_
#define _TCP_CONVERGENCE_LAYER_H_

#include <oasys/io/TCPClient.h>
#include <oasys/io/TCPServer.h>

#include "IPConvergenceLayer.h"

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

    /**
     * Tunable parameters
     */
    static struct _defaults {
        int ack_blocksz_;
        int keepalive_interval_;
        int test_fragment_size_;
    } Defaults;

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
     * Although the same class is used in either case, a particular
     * Connection is either a receiver or a sender, as negotiated by
     * the convergence layer specific framing protocol.
     */
    class Connection : public ContactInfo, public Thread, public Logger {
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

        /**
         * Destructor.
         */
        ~Connection();
        
    protected:
        virtual void run();
        
        bool connect(in_addr_t remote_addr, u_int16_t remote_port);
        bool accept();
        void break_contact();
        
        void send_loop();
        void recv_loop();

        bool send_bundle(Bundle* bundle, size_t* acked_len);
        int handle_ack(Bundle* bundle, size_t* acked_len);

        bool recv_bundle();
        bool send_ack(u_int32_t bundle_id, size_t acked_len);

        Contact* contact_;
        TCPClient* sock_;
        size_t ack_blocksz_;
        int keepalive_msec_;
    };
};

#endif /* _TCP_CONVERGENCE_LAYER_H_ */
