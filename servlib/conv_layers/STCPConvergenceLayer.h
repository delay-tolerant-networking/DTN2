/*
 *    Copyright 2015 United States Government as represented by NASA
 *       Marshall Space Flight Center. All Rights Reserved.
 *
 *    Released under the NASA Open Source Software Agreement version 1.3;
 *    You may obtain a copy of the Agreement at:
 * 
 *        http://ti.arc.nasa.gov/opensource/nosa/
 * 
 *    The subject software is provided "AS IS" WITHOUT ANY WARRANTY of any kind,
 *    either expressed, implied or statutory and this agreement does not,
 *    in any manner, constitute an endorsement by government agency of any
 *    results, designs or products resulting from use of the subject software.
 *    See the Agreement for the specific language governing permissions and
 *    limitations.
 */

#ifndef _STCP_CONVERGENCE_LAYER_H_
#define _STCP_CONVERGENCE_LAYER_H_

#include <netinet/tcp.h>                // for SOL_TCP

#include <oasys/debug/Log.h>
#include <oasys/io/IO.h>        // this will pick up IORATELIMIT definition
#include <oasys/io/TCPClient.h>
#include <oasys/io/TCPServer.h>
#include <oasys/thread/Thread.h>
#include <oasys/thread/MsgQueue.h>
#include <oasys/util/ScratchBuffer.h>
#include <oasys/util/TokenBucket.h>

#include "naming/EndpointID.h"
#include "bundling/BundleList.h"
#include "IPConvergenceLayer.h"

namespace dtn {

class STCPConvergenceLayer : public IPConvergenceLayer {
public:
    /**
     * Maximum bundle size
     */
    static const u_int MAX_STCP_LEN = 0xFFFFFFFF;
    static const int   NANOSECS_IN_A_SEC = 1000000000;

    /**
     * Default port used by the udp cl.
     */
    static const u_int16_t STCPCL_DEFAULT_PORT = 4557;
    
    /**
     * Constructor.
     */
    STCPConvergenceLayer();
        
    /**
     * Bring up a new interface.
     */
    bool interface_up(Interface* iface, int argc, const char* argv[]);

    /**
     * Bring down the interface.
     */
    bool interface_down(Interface* iface);
    
    /**
     * Dump out CL specific interface information.
     */
    void dump_interface(Interface* iface, oasys::StringBuffer* buf);

    /**
     * Create any CL-specific components of the Link.
     */
    bool init_link(const LinkRef& link, int argc, const char* argv[]);

    /**
     * Set default link options.
     */
    bool set_link_defaults(int argc, const char* argv[],
                           const char** invalidp);

    /**
     * Delete any CL-specific components of the Link.
     */
    void delete_link(const LinkRef& link);

    /**
     * Dump out CL specific link information.
     */
    void dump_link(const LinkRef& link, oasys::StringBuffer* buf);
    
    /**
     * Open the connection to a given contact and send/listen for 
     * bundles over this contact.
     */
    bool open_contact(const ContactRef& contact);
    
    /**
     * Close the connnection to the contact.
     */
    bool close_contact(const ContactRef& contact);

    /**
     * Send the bundle out the link.
     */
    void bundle_queued(const LinkRef& link, const BundleRef& bundle);

    /**
      * Reconfigure Link methods 
     */
    bool reconfigure_link(const LinkRef& link,
                          int argc, const char* argv[]);
    void reconfigure_link(const LinkRef& link,
                          AttributeVector& params);

    /**
     * handle bundle and add to queue.
     */
    void handle_bundle(dtn::Bundle* bundle);

    /**
     * Tunable parameter structure.
     *
     * Per-link and per-interface settings are configurable via
     * arguments to the 'link add' and 'interface add' commands.
     *
     * The parameters are stored in each Link's CLInfo slot.
     *
     */
    class Params : public CLInfo {
    public:
        /**
         * Virtual from SerializableObject
         */
        virtual void serialize( oasys::SerializeAction *a );

        in_addr_t local_addr_;		///< Local address to bind to
        u_int16_t local_port_;		///< Local port to bind to
        in_addr_t remote_addr_;		///< Peer address to connect to
        u_int16_t remote_port_;		///< Peer port to connect to
        u_int32_t rate_;		///< Rate (packets per second)
        int32_t   sleep_delay_;		///< milliseconds to sleep if rate is set
        int32_t   poll_timeout_;        ///< Msecs to wait for connection 
        int32_t   keepalive_interval_;   ///< secs to wait before checking Keepalive
    };
    
    /**
     * Default parameters.
     */
    static Params defaults_;

protected:
    bool parse_params(Params* params, int argc, const char** argv,
                      const char** invalidp);
    virtual CLInfo* new_link_params();

    /// Lock to protect the connections table
    oasys::SpinLock lock_;
    
    in_addr_t next_hop_addr_;
    u_int16_t next_hop_port_;
    int       next_hop_flags_; 

    /**
     * Helper class (and thread) that listens on a registered
     * interface for incoming data.
     */
    /// Helper class to accept incoming TCP connections

    class Listener : public CLInfo,
                     public oasys::TCPServerThread {

    public:

        Listener(STCPConvergenceLayer * t, in_addr_t listen_addr, u_int16_t listen_port);
        void accepted(int fd, in_addr_t addr, u_int16_t port);
        STCPConvergenceLayer * cl_;
    
    protected:

        /// @{
        /// Proxy parameters
        in_addr_t listen_addr_;
        u_int16_t listen_port_;
        /// @}
   
        /// Force close a TCP socket
        void force_close_socket(int fd);
    };

    /// Helper class to handle an actively proxied connection
    class Connection : public oasys::TCPClient,
                       public oasys::Thread
    {
        public:

            /// Constructor called when a new connection was accepted
            Connection(STCPConvergenceLayer* t, int fd,
                       in_addr_t client_addr, u_int16_t client_port);

            /// Destructor
            ~Connection();

            /// Handle a newly arriving bundle
   
        protected:

            friend class STCPConvergenceLayer;

            /// virtual run method
            void run();

            /// The stcp CL object
            STCPConvergenceLayer * tcptun_;

            /// Queue for bundles on this connection
            BundleList * queue_;            

            /// Parameters for the connection
            in_addr_t         client_addr_;
            u_int16_t         client_port_;

        /**
         * Handler to process an arrived packet.
         */
        void process_data(u_char* bp, size_t len);
    };

    /*
     * Helper class that wraps the sender-side per-contact state.
     */
    class Sender : public CLInfo, public Logger, public oasys::Thread {
    public:
        /**
         * Destructor.
         */
        virtual ~Sender();
        /**
         * Initialize the sender (the "real" constructor).
         */
        bool init(Params* params, in_addr_t addr, u_int16_t port);

    private:

        friend class STCPConvergenceLayer;
        
        /**
         * Constructor.
         */
        Sender(const ContactRef& contact);
        
        void run();
        virtual void initialize_pollfds();
        virtual bool handle_poll_activity();
        virtual void break_contact();
        virtual int  send_keepalive(int * keepalive);
     
        /**
         * Send one bundle.
         * @return the length of the bundle sent or -1 on error
         */
        int  send_bundle(const BundleRef& bundle);
        int  rate_write(const char* bp, size_t len);
        bool reconfigure();

        /**
         * Pointer to the link parameters.
         */
        Params* params_;

        /**
         * The TCP client socket.
         */
        oasys::TCPClient * socket_;
        oasys::TokenBucket * bucket_;

        struct pollfd*     sock_pollfd_; ///< Poll structure for the socket

        // copied from CLConnection but I don't have a clue
        int                 poll_timeout_;     ///< Timeout to wait for poll data
        int                 num_pollfds_;      ///< Number of pollfds in use
        static const int    MAXPOLL = 8;       ///< Maximum number of pollfds 
        struct pollfd       pollfds_[MAXPOLL]; ///< Array of pollfds 
        bool                contact_broken_;   ///< broken contact

        /**
         * The contact that we're representing.
         */
        ContactRef contact_;

        oasys::SpinLock write_lock_;
    };   
};

} // namespace dtn

#endif /* _STCP_CONVERGENCE_LAYER_H_ */
