#ifndef _UDP_CONVERGENCE_LAYER_H_
#define _UDP_CONVERGENCE_LAYER_H_

#include "IPConvergenceLayer.h"
#include "io/UDPClient.h"
#include "io/UDPServer.h"


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
     * interface for data 
     */
     class DataListener : public InterfaceInfo, public UDPServerThread {
	public:
	  DataListener();
	  void ProcessData(char* payload, size_t payload_len);
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
    class Connection : public ContactInfo, public Thread, public Logger {
    public:
        /**
         * Constructor for the active connection side of a connection.
         */
        Connection(Contact* contact,
                   in_addr_t remote_addr,
                   u_int16_t remote_port);

	
        
        /**
         * Destructor.
         */
        ~Connection();
        
       protected:
        virtual void run();
        void send_loop();
        bool send_bundle(Bundle* bundle);
        
        Contact* contact_;
        UDPClient* sock_; 
    };   
    
};

#endif /* _UDP_CONVERGENCE_LAYER_H_ */
