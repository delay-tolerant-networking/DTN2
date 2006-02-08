/* $Id$ */
#ifndef _BT_CONVERGENCE_LAYER_H_
#define _BT_CONVERGENCE_LAYER_H_

#include <config.h>
#ifdef OASYS_BLUETOOTH_ENABLED

#include <oasys/bluez/RFCOMMClient.h>
#include <oasys/bluez/RFCOMMServer.h>
#include <oasys/util/Options.h>

#include <set>
#include <map>
using namespace std;

#include <oasys/util/ScratchBuffer.h>
#include <oasys/util/StreamBuffer.h>
#include "bundling/BundleEvent.h"
#include "ConvergenceLayer.h"

namespace oasys {
/**
 * Bluetooth address (colon-separated hex) option class.
 */
class BdAddrOpt : public Opt {
public:
   /**
    * Basic constructor.
    *
    * @param opt     the option string
    * @param valp    pointer to the value
    * @param valdesc short description for the value
    * @param desc    descriptive string
    * @param setp    optional pointer to indicate whether or not
                     the option was set
    */
    BdAddrOpt(const char* opt, bdaddr_t* valp,
              const char* valdesc = "", const char* desc = "",
              bool* setp = NULL);

   /**
    * Alternative constructor with both short and long options,
    * suitable for getopt calls.
    *
    * @param shortopt  short option character
    * @param longopt   long option string
    * @param valp      pointer to the value
    * @param valdesc   short description for the value
    * @param desc      descriptive string
    * @param setp      optional pointer to indicate whether or not 
                       the option was set
    */
    BdAddrOpt(char shortopt, const char* longopt, bdaddr_t* valp,
              const char* valdesc = "", const char* desc = "",
              bool* setp = NULL);

protected:
    int set(const char* val, size_t len);
};

/**
 * Unsigned short integer option class.
 */
class UInt8Opt : public Opt {
public:
   /**
    * Basic constructor.
    *
    * @param opt     the option string
    * @param valp    pointer to the value
    * @param valdesc short description for the value
    * @param desc    descriptive string
    * @param setp    optional pointer to indicate whether or not
                     the option was set
    */
    UInt8Opt(const char* opt, u_int8_t* valp,
             const char* valdesc = "", const char* desc = "",
             bool* setp = NULL);

    /**
     * Alternative constructor with both short and long options,
     * suitable for getopt calls.
     *
     * @param shortopt  short option character
     * @param longopt   long option string
     * @param valp      pointer to the value
     * @param valdesc   short description for the value
     * @param desc      descriptive string
     * @param setp      optional pointer to indicate whether or not
                        the option was set
     */
    UInt8Opt(char shortopt, const char* longopt, u_int8_t* valp,
             const char* valdesc = "", const char* desc = "",
             bool* setp = NULL);

    protected:
        int set(const char* val, size_t len);
};


} // namespace oasys

namespace dtn {

class BluetoothConvergenceLayer : public ConvergenceLayer {
public:

    /**
     * Current version of the protocol
     */
    static const u_int8_t BTCL_VERSION = 0x01;

    /**
     * Values for ContactHeader flags
     */
    typedef enum {
        BUNDLE_ACK_ENABLED    = 0x1,
    } contact_header_flags_t;

    /**
     * Contact parameter header.  Sent once in each direction for BT/RFCOMM.
     */
    struct BTCLHeader {
        u_int32_t magic;              ///< magic word (MAGIC: "dtn!")
        u_int8_t  version;            ///< btcl protocol version
        u_int8_t  flags;              ///< connection flags (see above)
        u_int16_t partial_ack_len;    ///< requested size for partial acks
        u_int16_t keepalive_interval; ///< seconds between keepalive packets
        u_int16_t xx__unused;
        u_int32_t payload_size_;      ///< sizeof(bundle) ... not including BTCLHeader
    } __attribute__((packed));

    /**
     * Valid type codes for the protocol headers.
     *
     * For BT/RFCOMM, the one byte code is always sent first, followed by
     * the per-type header
     */
    typedef enum {
        BUNDLE_DATA = 0x1,  ///< bundle data
        BUNDLE_ACK  = 0x2,  ///< bundle acknowledgment
        KEEPALIVE   = 0x3,  ///< keepalive packet
        SHUTDOWN    = 0x4,  ///< indicates sending side will close connection
    } btcl_header_type_t;

    /**
     * Header for the start of a block of bundle data.
     */
    struct BundleDataHeader {
        u_int32_t bundle_id;       ///< bundle identifier at sender
        u_char    total_length[0]; ///< SDNV of total length
    } __attribute__((packed));

    /**
     * Header for a bundle acknowledgment.
     */
    struct BundleAckHeader {
        u_int32_t bundle_id;       ///< identical to BundleStartHeader
        u_int32_t acked_length;    ///< total length received
    } __attribute__((packed));

    /**
      * Tunable parameter structure.
      *
      * Defaults can be configured for all links (and interfaces) via
      * the 'param set' command'. Additionally, per-link and
      * per-interface settings are also configurable via arguments to
      * the 'link add' and 'interface add' commands.
      *
      * The parameters are stored in each Link's CLInfo slot, as well
      * as part of the Listener and Connection helper classes.
      */
    class Params : public CLInfo {
    public:
        bdaddr_t local_addr_;      ///< local address
        bdaddr_t remote_addr_;     ///< remote address
        u_int8_t channel_;         ///< RFCOMM channel
        std::string hcidev_;       ///< local device
        u_int bind_retry_;         ///< how many times to retry failed bind()?
        bool bundle_ack_enabled_;  ///< Use CL-specific bundle acks?
        u_int partial_ack_len_;    ///< Bytes to send before ack
        u_int writebuf_len_;       ///< Buffer size per write() call
        u_int readbuf_len_;        ///< Buffer size per read() call
        u_int keepalive_interval_; ///< Seconds between keepalive pacekts
        u_int retry_interval_;     ///< (copied from Link params)
        u_int min_retry_interval_; ///< (copied from Link params)
        u_int max_retry_interval_; ///< (copied from Link params)
        u_int16_t idle_close_time; ///< Seconds of idle time before close
        u_int rtt_timeout_;        ///< Msecs to wait for data
    };

    // forward declarations;
    class Listener;
    class Connection;

    /**
     * Keep up with the Listener object for each Bluetooth adapter,
     * as indexed by bdaddr_t, so that only one object (whether
     * Listener or Connection) binds on a bdaddr_t at any given
     * point in time.
     * 
     * Typical operation is, add a Listener with add_interface,
     * then open_contact brings up a Connection for a brief time,
     * then close_contact shuts down that Connection. ConnectionManager
     * facilitates the bookkeeping of who's doing what. Essentially
     * Listener hibernates while Connection exists, then as 
     * Connection is shut down, Listener is put back into listening
     * mode.
     */
    class ConnectionManager : public Logger {
    public:
        ConnectionManager() :
        Logger("/cl/bt/connmgr")
        { l_map_.clear(); }
        ~ConnectionManager() {;}

        /**
         * Factory method to create, or reuse, Listeners
         */
        Listener *listener(BluetoothConvergenceLayer*,Params*);

        /**
         * Factory method to create active, or reuse passive, Connections
         */
        Connection *connection(BluetoothConvergenceLayer*,bdaddr_t&,u_int8_t,Params*);

        bool del_listener(Listener*); 
        bool del_connection(Connection*);
        bool del_connection(bdaddr_t&);

    protected:
        Listener* listener(bdaddr_t&);
        void addListener(Listener*); 
        bool delListener(Listener*);

        struct less_bdaddr_ {
            bool operator() (const bdaddr_t& a, const bdaddr_t& b) const {
                return (bacmp(&a,&b) < 0);
            }
        };

        typedef std::map<bdaddr_t,Listener*,less_bdaddr_> adapter_map;
        typedef adapter_map::iterator adapter_map_i;
        adapter_map   l_map_; ///< map of Listeners
        adapter_map_i it_;    ///< iterator for search functions
    };

    BluetoothConvergenceLayer();

    bool init_link(Link* link, int argc, const char* argv[]);
    void dump_link(Link* link, oasys::StringBuffer* buf);
    bool interface_up(Interface* iface, int argc, const char* argv[]);
    bool interface_down(Interface* iface);
    void dump_interface(Interface* iface, oasys::StringBuffer* buf);
    bool open_contact(Contact* contact);
    bool close_contact(Contact* contact);
    void send_bundle(Contact*,Bundle*);

    static Params defaults_;
    static ConnectionManager connections_;

protected:
    bool parse_params(Params* params, int argc, const char** argv,
                      const char** invalidp);
    bool parse_nexthop(const char*, bdaddr_t*, u_int8_t*);

    /**
     * Helper class (and thread) that listens on a registered
     * interface for incoming data.
     */
    class Listener : public CLInfo,
                     public oasys::RFCOMMServerThread
    {
    public:
        /**
         * Constructor.
         */
        Listener(BluetoothConvergenceLayer *cl,
                 BluetoothConvergenceLayer::Params* params);

        /**
         * To wait out a conflict on the socket ... allow retry by setting
         * parameter bind_retry to something greater than 0
         */
        int bind_with_retry();

        /**
         * Callback handler for passive connections
         */
        void accepted(int fd, bdaddr_t addr, u_int8_t channel); 

        BluetoothConvergenceLayer::Params params_;

    protected:
        friend class Connection;
        friend class ConnectionManager;
        BluetoothConvergenceLayer* cl_;
        Connection* connection_;
    };

    /**
     * Helper class that wraps the sender-side per-contact state.
     */
    class Connection : public CLInfo,
                       public oasys::Thread,
                       public oasys::Logger {
    public:
        typedef enum {
            UNKNOWN,
            ACTIVE_SENDER,     ///< implies receive
            PASSIVE_RECEIVER,
            PASSIVE_SENDER     ///< implies receive
        } direction_t;

        /**
         * Constructor for the active connection side of a connection.
         */
         Connection(BluetoothConvergenceLayer* cl,
                    bdaddr_t remote_addr,
                    u_int8_t remote_channel,
                    Params* params);

         /**
          * Constructor for the passive accept side of a connection.
          */
         Connection(BluetoothConvergenceLayer* cl,
                    int fd,
                    bdaddr_t remote_addr,
                    u_int8_t remote_channel,
                    Params* params);

        /**
         * Destructor.
         */
        ~Connection();

        /**
         * Per-connection parameters.
         */
        BluetoothConvergenceLayer::Params params_;

        /**
         * Since bind() must precede connect() it's possible we may need
         * to wait out a conflict on the socket ... allow retry by setting
         * parameter bind_retry to something greater than 0
         */
        int connect_with_retry();

        void set_contact(Contact *c) { contact_ = c; }

        /**
         * Calling thread blocks until connection object is initialized
         * and ready to process bundles
         */
        bool is_ready(int timeout = -1);

        /**
         * Was this object formed by active or passive connect?
         */
        bool passive() { return (!initiate_); }

        /**
         * Interrupt from IO operations
         */
        void interrupt_from_io() { sock_->interrupt_from_io(); }

        /**
         * Perhaps it makes sense to run without threading for SLAVE scenario
         */
        virtual void run();

    protected:
        friend class BluetoothConvergenceLayer;
        friend class ConnectionManager;
        friend class Listener;

        void main_loop();
        void break_contact(ContactEvent::reason_t reason);
        bool connect();
        bool accept();
        bool send_contact_header();
        bool recv_contact_header(int timeout);
        bool send_address();
        bool recv_address(int timeout);
        bool send_bundle(Bundle* bundle);
        bool recv_bundle();
        bool handle_reply();
        int handle_ack();
        bool send_ack(u_int32_t bundle_id, size_t acked_len);
        bool send_keepalive();
        void note_data_rcvd();
        void note_data_sent();
        bool init_passive_contact();
        //void notify_passive_contact() {
            //ASSERT(!initiate_);
            //pci_notifier_->notify();
        //}

        /// Struct used to record bundles that are in-flight along
        /// with their transmission times
        struct InFlightBundle {
            InFlightBundle(Bundle* b)
                : bundle_(b, "BTCL::InFlightBundle"), acked_len_(0) {}
            InFlightBundle(const InFlightBundle& other)
                : bundle_(other.bundle_),
                xmit_start_time_(other.xmit_start_time_),
                xmit_finish_time_(other.xmit_finish_time_),
                acked_len_(other.acked_len_) {}

            BundleRef      bundle_;
            struct timeval xmit_start_time_;
            struct timeval xmit_finish_time_;
            size_t         acked_len_;
        };

        /// Typedef for the list of in-flight bundles
        typedef std::list<InFlightBundle> InFlightList;

        Listener* parent_;              ///< only used by passive
        BluetoothConvergenceLayer* cl_; ///< Pointer to the CL instance
        bool                initiate_;  ///< Do we initiate the connection
        direction_t         direction_; ///< SENDER or RECEIVER
        oasys::RFCOMMClient* sock_;     ///< The socket
        Contact*            contact_;   ///< Contact for sender-side
        oasys::StreamBuffer rcvbuf_;    ///< Buffer for incoming data
        oasys::ScratchBuffer<u_char*> sndbuf_;///< Buffer outgoing bundle data
        BlockingBundleList* queue_;     ///< Queue of bundles
        InFlightList        inflight_;  ///< List of bundles to be acked
        oasys::Notifier*    event_notifier_; ///< Notifier for BD event synch.
        bool contact_init_; ///< has contact been initialized? (passive only)

        struct timeval data_rcvd_;      ///< Timestamp for idle timer
        struct timeval data_sent_;      ///< Timestamp for keepalive timer
    };
};

} // namespace dtn

#endif /* OASYS_BLUETOOTH_ENABLED */
#endif /* _BT_CONVERGENCE_LAYER_H_ */
