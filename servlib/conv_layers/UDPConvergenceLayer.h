#ifndef _UDP_CONVERGENCE_LAYER_H_
#define _UDP_CONVERGENCE_LAYER_H_

#include "IPConvergenceLayer.h"
#include "io/UDPClient.h"
#include "thread/Thread.h"

class UDPConvergenceLayer : public IPConvergenceLayer {
public:
    /**
     * Constructor.
     */
    UDPConvergenceLayer();
        
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
     * Open the connection to a given contact and send/listen for 
     * bundles over this contact.
     */
    bool open_contact(Contact* contact);
    
    /**
     * Close the connnection to the contact.
     */
    bool close_contact(Contact* contact);

    /*
     * Tunable Parameters 
     */
    static struct _defaults {
        int ack_blocksz_; /* Not used (version 1.0) */
    } Defaults;    
    
    /**
     * Helper class (and thread) that listens on a registered
     * interface for incoming data 
     */
    class Receiver : public InterfaceInfo, public IPSocket, public Thread {
public:
        Receiver();
        void process_data(char* payload, size_t payload_len);
        /**
         * Loop forever, issuing blocking calls to IPSocket::recvfrom(),
         * then calling the process_data function when new data does
         * arrive
         * 
         * Note that unlike in the Thread base class, this run() method is
         * public in case we don't want to actually create a new thread
         * for this guy, but instead just want to run the main loop.
         */
        void run();
    };

    /**
     * Helper class (and thread) that manages an "established"
     * connection with a peer daemon (virtual connection in the
     * case of UDP).
     *
     * Only the sender side of the connection needs to initiate
     * a connection. The receiver will just receive data. Therefore,
     * we don't need a passive side of a connection
     */
    class Sender : public ContactInfo, public Thread, public Logger {
public:
        /**
         * Constructor for the active connection side of a connection.
         */
        Sender(Contact* contact,
               in_addr_t remote_addr,
               u_int16_t remote_port);

	
        
        /**
         * Destructor.
         */
        ~Sender();
        
protected:
        virtual void run();
        void send_loop();
        bool send_bundle(Bundle* bundle);
        
        Contact* contact_;
        UDPClient* sock_; 
    };   
    
};

#endif /* _UDP_CONVERGENCE_LAYER_H_ */
