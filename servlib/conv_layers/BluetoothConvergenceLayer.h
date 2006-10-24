#ifndef _BLUETOOTH_CONVERGENCE_LAYER_
#define _BLUETOOTH_CONVERGENCE_LAYER_

#include <config.h>

#ifdef OASYS_BLUETOOTH_ENABLED

#include <errno.h>
extern int errno;

#include <bluetooth/bluetooth.h>
#include <oasys/bluez/BluetoothInquiry.h>
#include <oasys/bluez/RFCOMMClient.h>
#include <oasys/bluez/RFCOMMServer.h>

#include "ConnectionConvergenceLayer.h"
#include "StreamConvergenceLayer.h"

namespace dtn {

/**
 * The Bluetooth Convergence Layer.
 */
class BluetoothConvergenceLayer : public StreamConvergenceLayer {
public:

    /**
     * Current protocol version.
     */
    static const u_int8_t BTCL_VERSION = 0x3; // parallels TCPCL

    /**
     * Default RFCOMM channel used by BTCL
     */
    static const u_int8_t BTCL_DEFAULT_CHANNEL = 10;

    /**
     * Constructor.
     */
    BluetoothConvergenceLayer();

    /// @{ Virtual from ConvergenceLayer
    bool interface_up(Interface* iface, int argc, const char* argv[]);
    bool interface_down(Interface* iface);
    void dump_interface(Interface* iface, oasys::StringBuffer* buf);
    /// @}

    /**
     * Tunable link parameter structure
     */
    class BluetoothLinkParams : public StreamLinkParams {
    public:
        bdaddr_t local_addr_;  ///< Local address to bind to
        bdaddr_t remote_addr_; ///< Remote address to bind to
        u_int8_t channel_;     ///< default channel
    protected:
        // See comment in LinkParams for why this is protected
        BluetoothLinkParams(bool init_defaults);
        friend class BluetoothConvergenceLayer;
    };

    /**
     * Default link parameters.
     */
    static BluetoothLinkParams default_link_params_;


protected:
    /// @{ virtual from ConvergenceLayer
    bool set_link_defaults(int argc, const char* argv[],
                           const char** invalidp);
    void dump_link(Link* link, oasys::StringBuffer* buf);
    /// @}

    /// @{ virtual from ConnectionConvergenceLayer
    virtual LinkParams* new_link_params();
    virtual bool parse_link_params(LinkParams* params, int argc,
                                   const char** argv,
                                   const char** invalidp);
    virtual bool parse_nexthop(Link* link, LinkParams* params);
    virtual CLConnection* new_connection(LinkParams* params);
    /// @}

    // forward decl
    class NeighborDiscovery;

    /**
     * Helper class (and thread) that listens on a registered
     * interface for new connections.
     */
    class Listener : public CLInfo, public oasys::RFCOMMServerThread {
    public:
        Listener(BluetoothConvergenceLayer* cl);
        void accepted(int fd, bdaddr_t addr, u_int8_t channel);

        /// The BT2CL instance
        BluetoothConvergenceLayer* cl_;

        /// Neighbor Discovery instance
        NeighborDiscovery* nd_;
    };

    /**
     * Helper class (and thread) that manages an established
     * connection with a peer daemon.
     */
    class Connection : public StreamConvergenceLayer::Connection {
    public:
        /**
         * Constructor for the active connect side of a connection.
         */
        Connection(BluetoothConvergenceLayer* cl,
                   BluetoothLinkParams* params);

        /**
         * Constructor for passive accept side of a connection. 
         */
        Connection(BluetoothConvergenceLayer* cl,
                   BluetoothLinkParams* params,
                   int fd, bdaddr_t addr, u_int8_t channel);

        virtual ~Connection();

    protected:

        /// @{ Virtual from CLConnection
        virtual void connect();
        virtual void accept();
        virtual void disconnect();
        virtual void initialize_pollfds();
        virtual void handle_poll_activity();
        /// @}

        /// @{ virtual from StreamConvergenceLayer::Connection
        void send_data();
        /// @}

        void recv_data();
        bool recv_contact_header(int timeout);
        bool send_bundle(Bundle* bundle);
        bool recv_bundle();
        bool handle_reply();
        int handle_ack();
        bool send_ack(u_int32_t bundle_id, size_t acked_len);
        bool send_keepalive();

        /**
         * Utility function to downcast the params_ pointer that's
         * stored in the CLConnection parent class.
         */
        BluetoothLinkParams* bt_lparams()
        {
            BluetoothLinkParams* ret =
                dynamic_cast<BluetoothLinkParams*>(params_);
            ASSERT(ret != NULL);
            return ret;
        }

        oasys::RFCOMMClient*  sock_;        ///< The socket
        struct pollfd*        sock_pollfd_; ///< Poll structure for the socket 
    };

    class NeighborDiscovery : public oasys::BluetoothInquiry,
                              public oasys::Thread
    {
    public:
        NeighborDiscovery(BluetoothConvergenceLayer *cl,
                          u_int poll_interval,
                          const char* logpath = "/dtn/cl/bt/neighbordiscovery")
            : Thread("NeighborDiscovery"), cl_(cl)
        {
            poll_interval_ = poll_interval;
            ASSERT(poll_interval_ > 0);
            Thread::set_flag(Thread::INTERRUPTABLE);
            set_logpath(logpath);
            memset(&local_addr_,0,sizeof(bdaddr_t));
            oasys::Bluetooth::hci_get_bdaddr(&local_addr_);
        }

        ~NeighborDiscovery() {}

        u_int poll_interval() {
            return poll_interval_;
        }

        // 0 indicates no polling
        void poll_interval(u_int poll_int) {
            poll_interval_ = poll_int;
        }

    protected:
        void run();
        void initiate_contact(bdaddr_t remote);

        u_int poll_interval_; ///< seconds between neighbor discovery polling
        BluetoothConvergenceLayer* cl_;
        bdaddr_t local_addr_; ///< used for SDP bind

    }; // NeighborDiscovery
}; // BluetoothConvergenceLayer

} // namespace dtn

#endif // OASYS_BLUETOOTH_ENABLED
#endif // _BLUETOOTH_CONVERGENCE_LAYER_
