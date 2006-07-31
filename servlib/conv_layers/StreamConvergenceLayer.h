/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2006 Intel Corporation. All rights reserved. 
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
#ifndef _STREAM_CONVERGENCE_LAYER_H_
#define _STREAM_CONVERGENCE_LAYER_H_

#include "ConnectionConvergenceLayer.h"
#include "CLConnection.h"

namespace dtn {

/**
 * Another shared-implementation convergence layer class for use with
 * reliable, in-order delivery protocols (i.e. TCP, SCTP, and
 * Bluetooth RFCOMM). The goal is to share as much functionality as
 * possible between protocols that have in-order, reliable, delivery
 * semantics.
 *
 * For the protocol, bundles are broken up into configurable-sized
 * blocks that are sent sequentially. Only a single bundle is inflight
 * on the wire at one time (i.e. we don't interleave blocks from
 * different bundles). When block acknowledgements are enabled (the
 * default behavior), the receiving node sends an acknowledgement for
 * each block of the bundle that was received.
 *
 * Keepalive messages are sent back and forth to ensure that the
 * connection remains open. In the case of on demand links, a
 * configurable idle timer is maintained to close the link when no
 * bundle traffic has been sent or received. Links that are expected
 * to be open but have broken due to underlying network conditions
 * (i.e. always on and on demand links) are reopened by a timer that
 * is managed by the contact manager.
 *
 * Flow control is managed through the poll callbacks given by the
 * base class CLConnection. In send_pending_data, we check if there
 * are any acks that need to be sent, then check if there are bundle
 * blocks to be sent (i.e. acks are given priority). The only
 * exception to this is that the connection might be write blocked in
 * the middle of sending a data block. In that case, we must first
 * finish transmitting the current block before sending any other acks
 * (or the shutdown message), otherwise those messages will be
 * consumed as part of the payload.
 *
 * To make sure that we don't deadlock with the other side, we always
 * drain any data that is ready on the channel. All incoming messages
 * mark state in the appropriate data structures (i.e. InFlightList
 * and IncomingList), then rely on send_pending_data to send the
 * appropriate responses.
 *
 * To record the blocks we've sent, we fill in the sent_data_ sparse
 * bitmap with the range of bytes as we send blocks out. As acks come
 * in, we extend the ack_data_ field to match. Once the whole bundle
 * is acked, the entry is removed from the InFlightList.
 *
 * To track blocks that we have received but haven't yet acked, we set
 * a single bit for the offset of the end of the block in the
 * ack_data_ bitmap as well as extending the contiguous range in
 * rcvd_data_. That way, the gap in the ack_data_ structure expresses
 * the individual block lengths that were sent by the peer. Then, once
 * the acks are sent, we fill in the contiguous range to identify as
 * such.
 */
class StreamConvergenceLayer : public ConnectionConvergenceLayer {
public:
    /**
     * Constructor
     */
    StreamConvergenceLayer(const char* logpath, const char* cl_name,
                           u_int8_t cl_version);

protected:
    /**
     * Values for ContactHeader flags.
     */
    typedef enum {
        BLOCK_ACK_ENABLED     = 0x1,	///< block acks requested
        REACTIVE_FRAG_ENABLED = 0x2,	///< reactive fragmentation enabled
    } contact_header_flags_t;

    /**
     * Contact initiation header. Sent at the beginning of a contact,
     * and immediately followed by a SDNV length of the AnnounceBundle
     * then the bundle itself.
     */
    struct ContactHeader {
        u_int32_t magic;		///< magic word (MAGIC: "dtn!")
        u_int8_t  version;		///< cl protocol version
        u_int8_t  flags;		///< connection flags (see above)
        u_int16_t keepalive_interval;	///< seconds between keepalive packets
    } __attribute__((packed));

    /**
     * Valid type codes for the protocol headers.
     */
    typedef enum {
        START_BUNDLE	= 0x1,		///< begin a new bundle transmission
        END_BUNDLE	= 0x2,		///< end of a bundle transmission
        DATA_BLOCK	= 0x3,		///< a block of bundle data
                                        ///< (followed by a SDNV block length)
        ACK_BLOCK	= 0x4,		///< acknowledgement of a block
                                        ///< (followed by a SDNV ack length)
        KEEPALIVE	= 0x5,		///< keepalive packet
        SHUTDOWN	= 0x6,		///< about to shutdown
    } stream_cl_header_type_t;
    
    /**
     * Link parameters shared among all stream based convergence layers.
     */
    class StreamLinkParams : public ConnectionConvergenceLayer::LinkParams {
    public:
        bool  block_ack_enabled_;	///< Use per-block acks
        u_int keepalive_interval_;	///< Seconds between keepalive packets
        u_int block_length_;		///< Maximum size of transmitted blocks

    protected:
        // See comment in LinkParams for why this should be protected
        StreamLinkParams(bool init_defaults);
    };
    
    /**
     * Version of the actual CL protocol.
     */
    u_int8_t cl_version_;

    /**
     * Stream connection class.
     */
    class Connection : public CLConnection {
    public:
        /**
         * Constructor.
         */
        Connection(const char* classname,
                   const char* logpath,
                   ConvergenceLayer* cl,
                   StreamLinkParams* params,
                   bool active_connector);

        /// @{ virtual from CLConnection
        void send_pending_data();
        void handle_send_bundle(Bundle* bundle);
        void handle_cancel_bundle(Bundle* bundle);
        void handle_poll_timeout();
        /// @}

    protected:
        /**
         * Hook used to tell the derived CL class to drain data out of
         * the send buffer.
         */
        virtual void send_data() = 0;

        /// @{ utility functions used by derived classes
        void initiate_contact();
        void process_data();
        /// @}

    private:
        /// @{ utility functions used internally in this class
        void note_data_rcvd();
        void note_data_sent();
        void send_pending_acks();
        void send_pending_blocks();
        bool start_bundle(InFlightBundle* inflight);
        bool send_next_block(InFlightBundle* inflight);
        void send_data_todo(InFlightBundle* inflight);
        bool finish_bundle(InFlightBundle* inflight);
        void send_keepalive();
        
        void handle_contact_initiation();
        bool handle_start_bundle();
        bool handle_end_bundle();
        bool handle_data_block();
        bool handle_data_todo();
        bool handle_ack_block();
        bool handle_keepalive();
        bool handle_shutdown();
        /// @}

        /**
         * Utility function to downcast the params_ pointer that's
         * stored in the CLConnection parent class.
         */
        StreamLinkParams* stream_lparams()
        {
            StreamLinkParams* ret = dynamic_cast<StreamLinkParams*>(params_);
            ASSERT(ret != NULL);
            return ret;
        }
        
    protected:
        InFlightBundle* current_inflight_; ///< Current bundle that's in flight 
        size_t send_block_todo_; 	///< Bytes left to send of current block
        size_t recv_block_todo_; 	///< Bytes left to recv of current block
        struct timeval data_rcvd_;	///< Timestamp for idle/keepalive timer
        struct timeval data_sent_;	///< Timestamp for idle timer
        struct timeval keepalive_sent_;	///< Timestamp for keepalive timer
    };

    /// For some gcc variants, this typedef seems to be needed
    typedef ConnectionConvergenceLayer::LinkParams LinkParams;

    /// @{ Virtual from ConvergenceLayer
    void dump_link(Link* link, oasys::StringBuffer* buf);
    /// @}
    
    /// @{ Virtual from ConnectionConvergenceLayer
    bool parse_link_params(LinkParams* params,
                           int argc, const char** argv,
                           const char** invalidp);
    bool finish_init_link(Link* link, LinkParams* params);
    /// @}

};

} // namespace dtn

#endif /* _STREAM_CONVERGENCE_LAYER_H_ */
