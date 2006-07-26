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

#include <oasys/util/OptParser.h>
#include "StreamConvergenceLayer.h"
#include "bundling/AnnounceBundle.h"
#include "bundling/BundleDaemon.h"
#include "bundling/SDNV.h"
#include "bundling/TempBundle.h"
#include "contacts/ContactManager.h"

namespace dtn {

//----------------------------------------------------------------------
StreamConvergenceLayer::StreamLinkParams::StreamLinkParams(bool init_defaults)
    : LinkParams(init_defaults),
      block_ack_enabled_(true),
      keepalive_interval_(10),
      block_length_(4096)
{
}

//----------------------------------------------------------------------
StreamConvergenceLayer::StreamConvergenceLayer(const char* logpath,
                                               const char* cl_name,
                                               u_int8_t    cl_version)
    : ConnectionConvergenceLayer(logpath, cl_name),
      cl_version_(cl_version)
{
}

//----------------------------------------------------------------------
bool
StreamConvergenceLayer::parse_link_params(LinkParams* lparams,
                                          int argc, const char** argv,
                                          const char** invalidp)
{
    // all subclasses should create a params structure that derives
    // from StreamLinkParams
    StreamLinkParams* params = dynamic_cast<StreamLinkParams*>(lparams);
    ASSERT(params != NULL);
                               
    oasys::OptParser p;

    p.addopt(new oasys::BoolOpt("block_ack_enabled",
                                &params->block_ack_enabled_));
    
    p.addopt(new oasys::UIntOpt("keepalive_interval",
                                &params->keepalive_interval_));
    
    p.addopt(new oasys::UIntOpt("block_length",
                                &params->block_length_));
    
    int count = p.parse_and_shift(argc, argv, invalidp);
    if (count == -1) {
        return false;
    }
    argc -= count;
    
    return ConnectionConvergenceLayer::parse_link_params(lparams, argc, argv,
                                                         invalidp);
}

//----------------------------------------------------------------------
void
StreamConvergenceLayer::dump_link(Link* link, oasys::StringBuffer* buf)
{
    ConnectionConvergenceLayer::dump_link(link, buf);
    
    StreamLinkParams* params =
        dynamic_cast<StreamLinkParams*>(link->cl_info());
    ASSERT(params != NULL);
    
    buf->appendf("block_ack_enabled: %u\n", params->block_ack_enabled_);
    buf->appendf("keepalive_interval: %u\n", params->keepalive_interval_);
    buf->appendf("block_length: %u\n", params->block_length_);
}

//----------------------------------------------------------------------
StreamConvergenceLayer::Connection::Connection(const char* classname,
                                               const char* logpath,
                                               ConvergenceLayer* cl,
                                               StreamLinkParams* params)
    : CLConnection(classname, logpath, cl, params),
      params_(params),
      send_block_todo_(0),
      recv_block_todo_(0)
{
}

//----------------------------------------------------------------------
void
StreamConvergenceLayer::Connection::initiate_contact()
{
    log_debug("initiate_contact called");

    // format the contact header
    ContactHeader contacthdr;
    contacthdr.magic   = htonl(MAGIC);
    contacthdr.version = ((StreamConvergenceLayer*)cl_)->cl_version_;
    
    contacthdr.flags = 0;
    if (params_->block_ack_enabled_)
        contacthdr.flags |= BLOCK_ACK_ENABLED;
    
    if (params_->reactive_frag_enabled_)
        contacthdr.flags |= REACTIVE_FRAG_ENABLED;
    
    contacthdr.keepalive_interval = htons(params_->keepalive_interval_);

    // copy the contact header into the send buffer
    ASSERT(sendbuf_.fullbytes() == 0);
    if (sendbuf_.tailbytes() < sizeof(ContactHeader)) {
        log_warn("send buffer too short: %zu < needed %zu",
                 sendbuf_.tailbytes(), sizeof(ContactHeader));
        sendbuf_.reserve(sizeof(ContactHeader));
    }
    
    memcpy(sendbuf_.start(), &contacthdr, sizeof(ContactHeader));
    sendbuf_.fill(sizeof(ContactHeader));
    
    // format the announce bundle and copy it into the send buffer,
    // preceded by an SDNV of its length
    TempBundle announce;
    BundleDaemon* bd = BundleDaemon::instance();
    AnnounceBundle::create_announce_bundle(&announce, bd->local_eid());
    size_t announce_len = BundleProtocol::formatted_length(&announce);
    size_t sdnv_len = SDNV::encoding_len(announce_len);

    if (sendbuf_.tailbytes() < sdnv_len + announce_len) {
        log_warn("send buffer too short: %zu < needed %zu",
                 sendbuf_.tailbytes(), sdnv_len + announce_len);
        sendbuf_.reserve(sdnv_len + announce_len);
    }
    
    sdnv_len = SDNV::encode(announce_len,
                            (u_char*)sendbuf_.end(),
                            sendbuf_.tailbytes());
    sendbuf_.fill(sdnv_len);

    int ret = BundleProtocol::format_bundle(&announce,
                                            (u_char*)sendbuf_.end(),
                                            sendbuf_.tailbytes());
    
    ASSERTF(ret > 0, "BundleProtocol::formatted_length disagreed with "
            "BundleProtocol::format_bundle");
    sendbuf_.fill(ret);

    // drain the send buffer
    send_data();
}

//----------------------------------------------------------------------
void
StreamConvergenceLayer::Connection::handle_contact_initiation()
{
    /*
     * First check that we have enough data for the contact header
     */
    size_t len_needed = sizeof(ContactHeader);
    if (recvbuf_.fullbytes() < len_needed) {
 tooshort:
        log_debug("handle_contact_initiation: not enough data received "
                  "(need > %zu, got %zu)",
                  len_needed, recvbuf_.fullbytes());
        return;
    }

    /*
     * Now check for enough data for the announce bundle.
     */
    u_int64_t announce_len;
    int sdnv_len = SDNV::decode((u_char*)recvbuf_.start() +sizeof(ContactHeader),
                                recvbuf_.fullbytes() - sizeof(ContactHeader),
                                &announce_len);
    if (sdnv_len < 0) {
        goto tooshort;
    }
    
    len_needed = sizeof(ContactHeader) + sdnv_len + announce_len;
    if (recvbuf_.fullbytes() < len_needed) {
        goto tooshort;
    }
    
    /*
     * Ok, we have enough data, parse the contact header.
     */
    ContactHeader contacthdr;
    memcpy(&contacthdr, recvbuf_.start(), sizeof(ContactHeader));

    contacthdr.magic              = ntohl(contacthdr.magic);
    contacthdr.keepalive_interval = ntohs(contacthdr.keepalive_interval);

    recvbuf_.consume(sizeof(ContactHeader));
    
    /*
     * Check for valid magic number and version.
     */
    if (contacthdr.magic != MAGIC) {
        log_warn("remote sent magic number 0x%.8x, expected 0x%.8x "
                 "-- disconnecting.", contacthdr.magic, MAGIC);
        break_contact(ContactEvent::BROKEN);
        return;
    }
    
    u_int8_t cl_version = ((StreamConvergenceLayer*)cl_)->cl_version_;
    if (contacthdr.version != cl_version) {
        log_warn("remote sent version %d, expected version %d "
                 "-- disconnecting.", contacthdr.version, cl_version);
        break_contact(ContactEvent::BROKEN);
        return;
    }

    /*
     * Now do parameter negotiation.
     */
    params_->keepalive_interval_ =
        std::min(params_->keepalive_interval_,
                 (u_int)contacthdr.keepalive_interval);

    params_->block_ack_enabled_ = params_->block_ack_enabled_ &&
                                  (contacthdr.flags & BLOCK_ACK_ENABLED);
    
    params_->reactive_frag_enabled_ = params_->reactive_frag_enabled_ &&
                                      (contacthdr.flags & REACTIVE_FRAG_ENABLED);

    /*
     * Now skip the sdnv that encodes the announce bundle length since
     * we parsed it above.
     */
    recvbuf_.consume(sdnv_len);

    /*
     * Finally, parse the announce bundle.
     */
    TempBundle announce;
    int ret = BundleProtocol::parse_bundle(&announce,
                                           (u_char*)recvbuf_.start(),
                                           recvbuf_.fullbytes());
    
    if (ret != (int)announce_len) {
        if (ret < 0) {
            log_err("protocol error: announcement bundle length given as %llu, "
                    "parser requires more bytes", announce_len);
        } else {
            log_err("protocol error: announcement bundle length given as %llu, "
                    "parser requires only %u", announce_len, ret);
        }
        break_contact(ContactEvent::BROKEN);
        return;
    }

    log_debug("got announce bundle: source eid %s", announce.source_.c_str());
    
    /**
     * Now, if we're the passive acceptor, we need to find or create
     * an appropriate opportunistic link for the connection.
     *
     * First, we check if there's an idle (i.e. UNAVAILABLE) link to
     * the remote eid. We explicitly ignore the nexthop address, since
     * that can change (due to things like TCP/UDP port number
     * assignment), but we pass in the remote eid to match for a link.
     *
     * If we can't find one, then we create a new opportunistic link
     * for the connection.
     */
    if (contact_ == NULL) {

        ASSERT(nexthop_ != ""); // the derived class must have set the
                                // nexthop in the constructor
        
        ContactManager* cm = BundleDaemon::instance()->contactmgr();

        Link* link = cm->find_link_to(cl_, "", announce.source_,
                                      Link::OPPORTUNISTIC,
                                      Link::AVAILABLE | Link::UNAVAILABLE);

        if (link != NULL) {
            link->set_nexthop(nexthop_);
            log_debug("found idle opportunistic link *%p", link);
            
        } else {
            link = cm->new_opportunistic_link(cl_,
                                              nexthop_.c_str(),
                                              announce.source_);
            log_debug("created new opportunistic link *%p", link);
        }
        
        ASSERT(! link->isopen());

        contact_ = new Contact(link);
        contact_->set_cl_info(this);
        link->set_contact(contact_.object());
    }

    recvbuf_.consume(announce_len);

    /**
     * Finally, we note that the contact is now up.
     */
    BundleDaemon::post(new ContactUpEvent(contact_));
}


//----------------------------------------------------------------------
void
StreamConvergenceLayer::Connection::handle_send_bundle(Bundle* bundle)
{
    // push the bundle onto the inflight queue. we'll handle sending
    // the bundle out in the callback for transmit_bundle_data
    inflight_.push_back(InFlightBundle(bundle));
    InFlightBundle* inflight = &inflight_.back();
    inflight->header_block_length_ =
        BundleProtocol::header_block_length(bundle);
    inflight->tail_block_length_ =
        BundleProtocol::tail_block_length(bundle);
    inflight->formatted_length_ =
        inflight->header_block_length_ +
        inflight->bundle_->payload_.length() +
        inflight->tail_block_length_;
}

//----------------------------------------------------------------------
void
StreamConvergenceLayer::Connection::send_pending_data()
{
    // if the outgoing data buffer is full, we can't do anything
    if (sendbuf_.tailbytes() == 0) {
        return;
    }

    // if we're in the middle of sending a block, we need to continue
    // sending it.
    if (send_block_todo_ != 0) {
        log_debug("send_pending_data: "
                  "continuing block transmission (%zu bytes to go)",
                  send_block_todo_);
        PANIC("todo");
    }
    
    // now check if there are acks we need to send
    send_pending_acks();

    // finally, transmit data blocks until we fill the buffer
    send_pending_blocks();
}

//----------------------------------------------------------------------
void
StreamConvergenceLayer::Connection::send_pending_acks()
{
    // normally, there's only a single IncomingBundle that we're
    // processing acks for. however, it's possible that a whole bunch
    // of small bundles made it in a single read buffer, in which case
    // we process them all here. thus, if we've sent the last ack for
    // a bundle, we jump back here to the start to process the next
    // bundle in the IncomingList
next_incoming_bundle:
    if (incoming_.empty()) {
        return; // nothing to do
    }
    
    IncomingBundle* incoming = &incoming_.front();

    if (incoming->ack_data_.empty()) {
        return; // nothing to do
    }

    // when data blocks are generated in send_next_block, the first
    // and last bit of the length of the block are marked in ack_data.
    //
    // therefore, to figure out what acks should be sent for those
    // blocks, we set the start iterator to be the last byte in the
    // contiguous range at the beginning of ack_data_, thereby
    // skipping everything that's already been acked. we then set the
    // end iterator to the next byte to be acked (i.e. the last byte
    // in the range that was received). the length to be acked is
    // therefore the range up to and including the end byte
    DataBitmap::iterator start = incoming->ack_data_.begin();
    start.skip_contiguous();
    DataBitmap::iterator end = start + 1;

    int sent_start = -1;
    size_t sent_end = 0;
    while (end != incoming->ack_data_.end()) {
        size_t ack_len   = *end + 1;
        size_t block_len = ack_len - *start;
        (void)block_len;
        
        // make sure we have space in the send buffer
        size_t encoding_len = 1 + SDNV::encoding_len(ack_len);
        if (encoding_len <= sendbuf_.tailbytes()) {
            log_debug("send_pending_acks: "
                      "sending ack length %zu for %zu byte block "
                      "[range %zu..%zu]",
                      block_len, ack_len, *start, *end);

            if (sent_start == -1) {
                sent_start = *start;
                log_debug("set sent_start to %d", sent_start);
            }
            
            ASSERT(sent_end <= ack_len);
            sent_end = ack_len;
            
            *sendbuf_.end() = ACK_BLOCK;
            int len = SDNV::encode(ack_len, (u_char*)sendbuf_.end() + 1,
                                   sendbuf_.tailbytes() - 1);
            ASSERT(encoding_len = len + 1);
            sendbuf_.fill(encoding_len);
            
            send_data();

        } else {
            log_debug("send_pending_acks: "
                      "no space for ack in buffer (need %zu, have %zu)",
                      encoding_len, sendbuf_.tailbytes());
            break;
        }

        // advance the two iterators to the next block
        start = end + 1;
        if (start == incoming->ack_data_.end()) {
            break;
        }
        
        end  = start + 1;

        log_debug("send_pending_acks: "
                  "found another gap (%zu, %zu)", *start, *end);
    }

    if (sent_end != 0) {
        incoming->ack_data_.set(0, sent_end);
        
        log_debug("send_pending_acks: "
                  "updated ack_data for sent range %d..%zu: *%p ",
                  sent_start, sent_end, &incoming->ack_data_);
    }
    
    // now, check if we're done with all the acks we need to send
    if (incoming->ack_data_.num_contiguous() == incoming->formatted_length_) {
        log_debug("send_pending_acks: acked all %zu bytes of bundle %d",
                  incoming->formatted_length_, incoming->bundle_->bundleid_);

        incoming_.pop_front();
        goto next_incoming_bundle;

    } else {
        log_debug("send_pending_acks: acked %zu/%zu bytes of bundle %d",
                  incoming->ack_data_.num_contiguous(),
                  incoming->formatted_length_, incoming->bundle_->bundleid_);
    }
}

//----------------------------------------------------------------------
void
StreamConvergenceLayer::Connection::send_pending_blocks()
{
    InFlightList::iterator iter;
    for (iter = inflight_.begin(); iter != inflight_.end(); ++iter) {
        InFlightBundle* inflight = &(*iter);
        
        // skip entries that we've sent completely
        if (inflight->sent_data_.num_contiguous() ==
            inflight->formatted_length_)
        {
            log_debug("send_pending_blocks: "
                      "transmission of bundle %d already complete, skipping",
                      inflight->bundle_->bundleid_);
            continue;
        }
        
        // check if this is a new bundle (i.e. no sent data)
        if (inflight->sent_data_.empty()) {
            if (! send_start_bundle(inflight)) {
                return; // out of space
            }
        }

        // now send the rest of it
        bool ok;
        do {
            ok = send_next_block(inflight);
        } while (ok);
        
        // if we didn't finish sending the bundle, there must not be
        // enough space in the buffer, so break out of the loop,
        // otherwise we can continue onto the next bundle (if any)
        if (inflight->sent_data_.num_contiguous() ==
            inflight->formatted_length_)
        {
            log_debug("send_pending_blocks: completed transmission of bundle %d",
                      inflight->bundle_->bundleid_);
                
            continue;
        }
        else
        {
            log_debug("send_pending_blocks: partial transmission of bundle %d",
                      inflight->bundle_->bundleid_);
            break;
        }
    }
}
         
//----------------------------------------------------------------------
bool
StreamConvergenceLayer::Connection::send_start_bundle(InFlightBundle* inflight)
{
    if (sendbuf_.tailbytes() == 0) {
        return false;
    }
    
    u_char* bp = (u_char*)sendbuf_.end();
    *bp = START_BUNDLE;

    int len = SDNV::encode(inflight->formatted_length_,
                           bp + 1,
                           sendbuf_.tailbytes() - 1);
    if (len < 0) {
        log_debug("send_start_bundle: no space in buffer (%zu bytes free)",
                  sendbuf_.tailbytes());
        return false;
    }

    sendbuf_.fill(len + 1);
    log_debug("send_start_bundle: "
              "sending %d byte START_BUNDLE header for bundle %d",
              len + 1, inflight->bundle_->bundleid_);
    send_data();
    return true;
}

//----------------------------------------------------------------------
bool
StreamConvergenceLayer::Connection::send_next_block(InFlightBundle* inflight)
{
    if (sendbuf_.tailbytes() == 0) {
        return false;
    }

    bool more_to_send = true;
    Bundle* bundle = inflight->bundle_.object();
    
    // first send the header blocks
    if (inflight->sent_data_.empty()) {
        u_char* bp = (u_char*)sendbuf_.end();
        size_t len = sendbuf_.tailbytes();
        
        *bp = DATA_BLOCK;
        bp  += 1;
        len -= 1;

        size_t header_len = BundleProtocol::header_block_length(bundle);
        int sdnv_len = SDNV::encode(header_len, bp, len);
        if (sdnv_len < 0) {
            log_debug("send_next_block: not enough space for sdnv header");
            return false;
        }

        bp  += sdnv_len;
        len -= sdnv_len;
        
        int hlen = BundleProtocol::format_header_blocks(bundle, bp, len);
        if (hlen < 0) {
            log_debug("send_next_block: not enough space for header blocks");
            return false;
        }

        // verify that header_block_length didn't lie
        ASSERT(hlen == (int)header_len);

        log_debug("send_next_block: sending header block [len %zu]",
                  header_len);
        sendbuf_.fill(1 + sdnv_len + header_len);
        inflight->sent_data_.set(0, header_len);
    }
    else
    {
        size_t payload_len  = bundle->payload_.length();
        size_t last_sent    = *(--inflight->sent_data_.end()) + 1;
        size_t payload_sent = last_sent - inflight->header_block_length_;
        size_t block_len    = std::min((size_t)params_->block_length_,
                                       payload_len - payload_sent);

        ASSERT(payload_sent + block_len <= payload_len);

        size_t sdnv_len = SDNV::encoding_len(block_len);
        size_t encoded_len = 1 + sdnv_len  + block_len;
        if (sendbuf_.tailbytes() < encoded_len) {
            log_debug("send_next_block: "
                      "not enough space for block [need %zu, have %zu]",
                      encoded_len, sendbuf_.tailbytes());
            return false;
        }

        log_debug("send_next_block: "
                  "sending %zu byte block [payload range %zu..%zu]",
                  block_len, payload_sent, payload_sent + block_len);
        
        u_char* bp = (u_char*)sendbuf_.end();
        *bp++ = DATA_BLOCK;
        size_t sdnv_len2 = SDNV::encode(block_len, bp, sendbuf_.tailbytes() - 1);
        ASSERT(sdnv_len == sdnv_len2);

        bp += sdnv_len;
        bundle->payload_.read_data(payload_sent, block_len, bp,
                                   BundlePayload::FORCE_COPY);
        sendbuf_.fill(encoded_len);

        log_debug("send_next_block: "
                  "marking sent_data [range %zu..%zu]",
                  last_sent, last_sent + block_len);
        inflight->sent_data_.set(last_sent, block_len);

        if (payload_sent + block_len == payload_len) {
            more_to_send = false;
        }
    }

    send_data();
    return more_to_send;
}

//----------------------------------------------------------------------
void
StreamConvergenceLayer::Connection::handle_cancel_bundle(Bundle* bundle)
{
    (void)bundle;
}

//----------------------------------------------------------------------
void
StreamConvergenceLayer::Connection::handle_poll_timeout()
{
}

//----------------------------------------------------------------------
void
StreamConvergenceLayer::Connection::process_data()
{
    if (recvbuf_.fullbytes() == 0) {
        return;
    }

    note_data_rcvd();
    
    log_debug("processing up to %zu bytes from receive buffer",
              recvbuf_.fullbytes());

    // the first thing we need to do is handle the contact initiation
    // sequence, i.e. the contact header and the announce bundle. we
    // know we need to do this if we don't have an OPEN link yet
    if (contact_ == NULL || contact_->link()->state() == Link::OPENING) {
        handle_contact_initiation();
        return;
    }

    // if a data block is bigger than the receive buffer. when
    // processing a data block, we mark the unread amount in the
    // data_block_remaining_ field so if that's not zero, we need to
    // drain it, then fall through to handle the rest of the buffer
    if (recv_block_todo_ != 0) {
        handle_data_todo();
    }
    
    // now, drain cl messages from the receive buffer. we peek at the
    // first byte and dispatch to the correct handler routine
    // depending on the type of the CL message. we don't consume the
    // byte yet since there's a possibility that we need to read more
    // from the remote side to handle the whole message
    while (recvbuf_.fullbytes() != 0) {
        char type = *recvbuf_.start();

        log_debug("recvbuf has %zu full bytes, dispatching to handler routine",
                  recvbuf_.fullbytes());
        bool ok;
        switch (type) {
        case START_BUNDLE:
            ok = handle_start_bundle();
            break;
        case DATA_BLOCK:
            ok = handle_data_block();
            break;
        case ACK_BLOCK:
            ok = handle_ack_block();
            break;
        case KEEPALIVE:
            ok = handle_keepalive();
            break;
        case SHUTDOWN:
            ok = handle_shutdown();
            break;
        default:
            log_debug("invalid CL message type 0x%x", type);
            break_contact(ContactEvent::BROKEN);
            return;
        }

        // if there's not enough data in the buffer to handle the
        // message, make sure there's space to receive more
        if (! ok) {
            recvbuf_.reserve(recvbuf_.size() - recvbuf_.fullbytes());
            return;
        }
    }
}

//----------------------------------------------------------------------
void
StreamConvergenceLayer::Connection::note_data_rcvd()
{
    log_debug("noting receipt of data");
    ::gettimeofday(&data_rcvd_, 0);
}

//----------------------------------------------------------------------
bool
StreamConvergenceLayer::Connection::handle_start_bundle()
{
    u_char* bp = (u_char*)recvbuf_.start();
    u_int64_t bundle_len;
    
    int sdnv_len = SDNV::decode(bp + 1, recvbuf_.fullbytes() - 1,
                                &bundle_len);
    
    if (sdnv_len < 0) {
        log_debug("handle_start_bundle: too few bytes for sdnv (%zu)",
                  recvbuf_.fullbytes());
        return false;
    }

    recvbuf_.consume(1 + sdnv_len);
    
    if (! incoming_.empty()) {
        log_err("protocol error: got START_BUNDLE before bundle completed");
        break_contact(ContactEvent::BROKEN);
        return false;
    }
    
    log_debug("START_BUNDLE: total length %llu", bundle_len);
    incoming_.push_back(IncomingBundle(NULL));
    IncomingBundle* incoming = &incoming_.back();
    incoming->formatted_length_ = bundle_len;

    return true;
}

//----------------------------------------------------------------------
bool
StreamConvergenceLayer::Connection::handle_data_block()
{
    u_char* bp = (u_char*)recvbuf_.start();

    u_int32_t block_len;
    
    int sdnv_len = SDNV::decode(bp + 1, recvbuf_.fullbytes() - 1,
                                &block_len);

    if (sdnv_len < 0) {
        log_debug("handle_data_block: too few bytes for sdnv (%zu)",
                  recvbuf_.fullbytes());
        return false;
    }

    if (recvbuf_.fullbytes() < 1 + sdnv_len + block_len) {
        log_debug("handle_data_block: "
                  "not enough space for block [need %u, have %zu]",
                  1 + sdnv_len + block_len, recvbuf_.fullbytes());
        return false;
    }

    if (incoming_.empty()) {
        log_err("protocol error: got data block before START_BUNDLE");
        break_contact(ContactEvent::BROKEN);
        return false;
    }

    // Note that there may be more than one incoming bundle on the
    // IncomingList. There's always only one (at the back) that we're
    // reading in data for, the rest are waiting for acks to go out
    IncomingBundle* incoming = &incoming_.back();
     
    size_t rcvd_offset;
    if (incoming->rcvd_data_.empty()) {
        // if this is the first block, parse the header blocks and create
        // the bundle structure
        Bundle* bundle = new Bundle();
        int len = BundleProtocol::parse_header_blocks(bundle,
                                                      bp + 1 + sdnv_len,
                                                      block_len);
        if (len != (int)block_len) {
            log_err("protocol error: "
                    "first block of %u bytes != header block len %d",
                    block_len, len);
            
            // XXX/demmer should have a ContactEvent::ERROR that
            // doesn't try to reestablish the connection??
            
            break_contact(ContactEvent::BROKEN);
            return false;
        }

        log_debug("handle_data_block: "
                  "got header blocks for bundle *%p", bundle);
        
        rcvd_offset = 0;
        incoming->header_block_length_ = block_len;
        incoming->bundle_ = bundle;
        
    } else {
        // read in the payload block and fill it into the
        // BundlePayload class
        rcvd_offset = incoming->rcvd_data_.num_contiguous();
        
        size_t payload_offset = rcvd_offset - incoming->header_block_length_;
        
        log_debug("handle_data_block: "
                  "got payload block len %u at offset %zu (%zu - %zu)",
                  block_len, payload_offset, rcvd_offset, incoming->header_block_length_);
        
        u_char* bp = (u_char*)recvbuf_.start() + 1 + sdnv_len;
        incoming->bundle_->payload_.write_data(bp, payload_offset, block_len);
    }
    
    incoming->rcvd_data_.set(rcvd_offset, block_len);
    incoming->ack_data_.set(rcvd_offset);
    incoming->ack_data_.set(rcvd_offset + block_len - 1);

    recvbuf_.consume(1 + sdnv_len + block_len);
    
    log_debug("handle_data_block: "
              "updated recv_data (rcvd_offset %zu) *%p ack_data *%p",
              rcvd_offset, &incoming->rcvd_data_, &incoming->ack_data_);

    // check if we've gotten the whole bundle
    check_completed(incoming);
    
    return true;
}

//----------------------------------------------------------------------
bool
StreamConvergenceLayer::Connection::handle_data_todo()
{
    // We shouldn't get ourselves here unless there's something incoming
    ASSERT(!incoming_.empty());
    
    // Note that there may be more than one incoming bundle on the
    // IncomingList. There's always only one (at the back) that we're
    // reading in data for, the rest are waiting for acks to go out
    IncomingBundle* incoming = &incoming_.back();
    
    check_completed(incoming);
    return true;
}

//----------------------------------------------------------------------
void
StreamConvergenceLayer::Connection::check_completed(IncomingBundle* incoming)
{
    log_debug("check_completed: %zu/%zu received",
              incoming->rcvd_data_.num_contiguous(), incoming->formatted_length_);

    // note that when we get the whole bundle, we post the
    // BundleReceivedEvent but keep the bundle in the IncomingList
    // since we still need to send out all the acks for it. in
    // send_pending_acks, we check if we're all done, in which case we
    // pop the inflight bundle off the list.
    if (incoming->rcvd_data_.num_contiguous() == incoming->formatted_length_) {
        log_debug("check_completed: got whole bundle");
        incoming->bundle_->payload_.close_file();

        // XXX/demmer need to fix the fragmentation code to assume the
        // event includes the header bytes as well as the payload.
        size_t bytes_rcvd = incoming->bundle_->payload_.length();
        
        BundleDaemon::post(
            new BundleReceivedEvent(incoming->bundle_.object(),
                                    EVENTSRC_PEER,
                                    bytes_rcvd));
    }
}

//----------------------------------------------------------------------
bool
StreamConvergenceLayer::Connection::handle_ack_block()
{
    u_char* bp = (u_char*)recvbuf_.start();
    u_int32_t acked_len;
    int sdnv_len = SDNV::decode(bp + 1, recvbuf_.fullbytes() - 1, &acked_len);
    
    if (sdnv_len < 0) {
        log_debug("handle_ack_block: too few bytes for sdnv (%zu)",
                  recvbuf_.fullbytes());
        return false;
    }

    recvbuf_.consume(1 + sdnv_len);

    if (inflight_.empty()) {
        log_err("protocol error: got ack block with no inflight bundle");
        break_contact(ContactEvent::BROKEN);
        return false;
    }
    
    InFlightBundle* inflight = &inflight_.front();

    size_t ack_begin;
    DataBitmap::iterator i = inflight->ack_data_.begin();
    if (i == inflight->ack_data_.end()) {
        ack_begin = 0;
    } else {
        i.skip_contiguous();
        ack_begin = *i + 1;
    }

    if (acked_len < ack_begin) {
        log_err("protocol error: got ack for length %u but already acked up to %zu",
                acked_len, ack_begin);
        break_contact(ContactEvent::BROKEN);
        return false;
    }
    
    inflight->ack_data_.set(0, acked_len);

    // now check if this was the last ack for the bundle, in which
    // case we can pop it off the list and post a
    // BundleTransmittedEvent
    if (acked_len == inflight->formatted_length_) {
        log_debug("handle_ack_block: "
                  "got final ack for %zu byte range -- acked_len %u, ack_data *%p",
                  (size_t)acked_len - ack_begin, acked_len, &inflight->ack_data_);

        // XXX/demmer change this event to include the header bytes
        
        BundleDaemon::post(new BundleTransmittedEvent(inflight->bundle_.object(),
                                                      contact_,
                                                      inflight->sent_data_.num_contiguous() -
                                                      inflight->header_block_length_,
                                                      inflight->ack_data_.num_contiguous() -
                                                        inflight->header_block_length_));

        inflight_.pop_front();
        
    } else {
        log_debug("handle_ack_block: "
                  "got acked_len %u (%zu byte range) -- ack_data *%p",
                  acked_len, (size_t)acked_len - ack_begin, &inflight->ack_data_);
    }

    return true;
}

//----------------------------------------------------------------------
bool
StreamConvergenceLayer::Connection::handle_keepalive()
{
    return true;
}

//----------------------------------------------------------------------
bool
StreamConvergenceLayer::Connection::handle_shutdown()
{
    log_debug("got SHUTDOWN byte");
    recvbuf_.consume(1);
    break_contact(ContactEvent::SHUTDOWN);
    
    return false;
}

} // namespace dtn
