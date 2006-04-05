/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 *
 * Intel Open Source License
 *
 * Copyright (c) 2004 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 *   Neither the name of the Intel Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _TCP_CONVERGENCE_LAYER_H_
#define _TCP_CONVERGENCE_LAYER_H_

#include <oasys/io/TCPClient.h>
#include <oasys/io/TCPServer.h>
#include <oasys/util/ScratchBuffer.h>
#include <oasys/util/StreamBuffer.h>

#include "IPConvergenceLayer.h"
#include "bundling/BundleEvent.h"

namespace dtn {

/**
 * The TCP Convergence Layer.
 *
 * The implementation is fairly straightforward (hopefully). It makes
 * use of three helper classes, Listener, Connection, and Params. The
 * Listener is attached to an interface and, as expected, opens a
 * listening socket on the specified port and waits for new
 * connections. A pointer to the Listener is stored in the CLInfo slot
 * in the interface.
 *
 * The link's CLInfo slot holds a pointer to a Params struct, to keep
 * track of the configured parameters for that link. Each time a new
 * Contact is created (when the link is opened), a Connection object
 * with an associated thread is also created and stored in the CLInfo
 * slot of the Contact.
 *
 * A slightly more complicated situation arises with the
 * reciever_connect option. To implement this feature, intended to
 * allow a TCPCL connection to traverse a firewall or NAT box, the
 * user would configure a special interface with the receiver_connect
 * parameter set. In this case, rather than creating a Listener, the
 * interface would create a new Connection object and initiate contact
 * with the specified peer.
 *
 * To the connection receiver (who will be the data sender), a new
 * OPPORTUNISTIC link appears when the contact is completed. In this
 * case, the Connection object already exists (having been created to
 * accept the connection), so it is handed to the ContactManager to be
 * installed in the new Contact structure.
 */
class TCPConvergenceLayer : public IPConvergenceLayer {
public:
    /**
     * Current version of the protocol.
     */
    static const u_int8_t TCPCL_VERSION = 0x02;

    /**
     * Values for ContactHeader flags.
     */
    typedef enum {
        BUNDLE_ACK_ENABLED    = 0x1,	///< bundle acks requested
        REACTIVE_FRAG_ENABLED = 0x2,	///< reactive fragmentation enabled
        RECEIVER_CONNECT      = 0x4,	///< connector is receiver
    } contact_header_flags_t;

    /**
     * Contact initiation header.
     */
    struct ContactHeader {
        u_int32_t magic;		///< magic word (MAGIC: "dtn!")
        u_int8_t  version;		///< tcpcl protocol version
        u_int8_t  flags;		///< connection flags (see above)
        u_int16_t keepalive_interval;	///< seconds between keepalive packets
        u_int32_t partial_ack_length;	///< requested size for partial acks
    } __attribute__((packed));

    /**
     * Address header. Sent by the active connector side immediately
     * after the contact header in the case of the receiver connect
     * option.
     */
    struct AddressHeader {
        u_int32_t addr;			///< address
        u_int16_t port;			///< port
    } __attribute__((packed));

    /**
     * Valid type codes for the protocol headers.
     *
     * For TCP, the one byte code is always sent first, followed by
     * the per-type header. For UDP, the one byte type code is sent
     * with three bytes of padding to preserve the word alignment of
     * the subsequent protocol header.
     */
    typedef enum {
        BUNDLE_DATA	= 0x1,		///< bundle data
        BUNDLE_ACK	= 0x2,		///< bundle acknowledgement
        KEEPALIVE	= 0x3,		///< keepalive packet
        SHUTDOWN	= 0x4,		///< sending side will shutdown now
    } tcpcl_header_type_t;

    /**
     * Header for the start of a block of bundle data. In UDP mode,
     * this always precedes a full bundle (or a fragment at the
     * bundling layer).
     */
    struct BundleDataHeader {
        u_int32_t bundle_id;		///< bundle identifier at sender
        u_char    total_length[0];	///< SDNV of total length
    } __attribute__((packed));

    /**
     * Header for a bundle acknowledgement.
     */
    struct BundleAckHeader {
        u_int32_t bundle_id;		///< identical to BundleStartHeader
        u_char    acked_length[0];	///< SDNV of acked length
    } __attribute__((packed));

    /**
     * Constructor.
     */
    TCPConvergenceLayer();

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
    bool init_link(Link* link, int argc, const char* argv[]);

    /**
     * Dump out CL specific link information.
     */
    void dump_link(Link* link, oasys::StringBuffer* buf);

    /**
     * Open the connection to the given contact and prepare for
     * bundles to be transmitted.
     */
    bool open_contact(Link* link);

    /**
     * Close the connnection to the contact.
     */
    bool close_contact(const ContactRef& contact);

    /**
     * Send a bundle to the contact. Mark the link as busy and queue
     * the bundle on the Connection's bundle queue.
     */
    void send_bundle(const ContactRef& contact, Bundle* bundle);

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
        in_addr_t local_addr_;		///< Local address to bind to
        u_int16_t local_port_;		///< Local port to bind to
        in_addr_t remote_addr_;		///< Peer address used for rcvr-connect
        u_int16_t remote_port_;		///< Peer port used for rcvr-connect
        bool pipeline_;			///< Pipeline bundles on the socket
        u_int32_t busy_queue_depth_;	///< Max # bundles in BD -> conn. queue
        bool bundle_ack_enabled_;	///< Use CL-specific bundle acks?
        bool reactive_frag_enabled_;	///< Is reactive fragmentation enabled
        bool receiver_connect_;		///< rcvr-initiated connect (for NAT)
        u_int partial_ack_len_;		///< Bytes to send before ack
        u_int writebuf_len_;		///< Buffer size per write() call
        u_int readbuf_len_;		///< Buffer size per read() call
        u_int keepalive_interval_;	///< Seconds between keepalive pacekts
        u_int retry_interval_;		///< (copied from Link params)
        u_int min_retry_interval_;	///< (copied from Link params)
        u_int max_retry_interval_;	///< (copied from Link params)
        u_int16_t idle_close_time;	///< Seconds of idle time before close
        u_int rtt_timeout_;		///< Msecs to wait for data

        u_int test_read_delay_;		///< Msecs to sleep between read calls
        u_int test_write_delay_;	///< Msecs to sleep between write calls
    };

    void dump_params(Params* params);

    /**
     * Default parameters.
     */
    static Params defaults_;

protected:
    bool parse_params(Params* params, int argc, const char** argv,
                      const char** invalidp);

    /**
     * Helper class (and thread) that listens on a registered
     * interface for new connections.
     */
    class Listener : public CLInfo, public oasys::TCPServerThread {
    public:
        Listener(TCPConvergenceLayer* cl, Params* params);
        void accepted(int fd, in_addr_t addr, u_int16_t port);

        /// The TCPCL instance
        TCPConvergenceLayer* cl_;

        /// Per-connection parameters for accepted connections.
        TCPConvergenceLayer::Params params_;
    };

    /**
     * Helper class (and thread) that manages an established
     * connection with a peer daemon.
     *
     * Although the same class is used in both cases, a particular
     * Connection is either a receiver or a sender, as indicated by
     * the direction variable. Note that to deal with NAT, the side
     * which does the active connect is not necessarily the sender.
     */
    class Connection : public CLInfo,
                       public oasys::Thread,
                       public oasys::Logger {
    public:
        typedef enum {
            UNKNOWN,
            SENDER,
            RECEIVER
        } direction_t;

        /**
         * Constructor for the active connection side of a connection.
         * Note that this may be used both for the actual data sender
         * or for the data receiver side when used with the
         * receiver_connect option.
         */
        Connection(TCPConvergenceLayer* cl,
                   in_addr_t remote_addr,
                   u_int16_t remote_port,
                   direction_t direction,
                   Params* params);

        /**
         * Constructor for the passive accept side of a connection.
         */
        Connection(TCPConvergenceLayer* cl,
                   int fd,
                   in_addr_t remote_addr,
                   u_int16_t remote_port,
                   Params* params);

        /**
         * Destructor.
         */
        ~Connection();

        /**
         * Per-connection parameters.
         */
        TCPConvergenceLayer::Params params_;

        /**
         * Attach to the given contact. Used on the sender side when
         * the link is activated.
         */
        void set_contact(Contact* contact) { contact_ = contact; }

        //! Interrupt from IO operations
        void interrupt_from_io() { sock_->interrupt_from_io(); }

    protected:
        friend class TCPConvergenceLayer;

        virtual void run();
        void send_loop();
        void recv_loop();
        void break_contact(ContactEvent::reason_t reason);
        bool connect();
        bool accept();
        bool send_contact_header();
        bool recv_contact_header(int timeout);
        bool send_address();
        bool recv_address(int timeout);
        bool open_opportunistic_link();
        bool send_bundle(Bundle* bundle);
        bool recv_bundle();
        bool handle_reply();
        int handle_ack();
        bool send_ack(u_int32_t bundle_id, size_t acked_len);
        bool send_keepalive();
        void note_data_rcvd();

        /// Struct used to record bundles that are in-flight along
        /// with their transmission times
        struct InFlightBundle {
            InFlightBundle(Bundle* b)
                : bundle_(b, "TCPCL::InFlightBundle"),
                  xmit_len_(0), acked_len_(0) {}

            BundleRef      bundle_;
            struct timeval xmit_start_time_;
            struct timeval xmit_finish_time_;
            size_t         xmit_len_;
            size_t         acked_len_;
        };

        /// Typedef for the list of in-flight bundles
        typedef std::list<InFlightBundle> InFlightList;

        TCPConvergenceLayer* cl_;	///< Pointer to the CL       instance
        bool                initiate_;	///< Do we initiate the connection
        direction_t         direction_; ///< SENDER or RECEIVER
        ContactRef          contact_;	///< Contact for sender-side
        oasys::TCPClient*   sock_;	///< The socket
        oasys::StreamBuffer rcvbuf_;	///< Buffer for incoming data
        oasys::ScratchBuffer<u_char*> sndbuf_;///< Buffer for outgoing bundle data
        BlockingBundleList* queue_;	///< Queue of bundles for the connection
        InFlightList	    inflight_;	///< List of bundles to be acked
        oasys::Notifier*    event_notifier_; ///< Notifier for BD event synch.

        struct timeval data_rcvd_;	///< Timestamp for idle timer
        struct timeval keepalive_sent_;	///< Timestamp for keepalive timer
    };
};

} // namespace dtn

#endif /* _TCP_CONVERGENCE_LAYER_H_ */
