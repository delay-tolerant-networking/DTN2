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
      segment_ack_enabled_(true),
      keepalive_interval_(10),
      segment_length_(4096)
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

    p.addopt(new oasys::BoolOpt("segment_ack_enabled",
                                &params->segment_ack_enabled_));
    
    p.addopt(new oasys::UIntOpt("keepalive_interval",
                                &params->keepalive_interval_));
    
    p.addopt(new oasys::UIntOpt("segment_length",
                                &params->segment_length_));
    
    int count = p.parse_and_shift(argc, argv, invalidp);
    if (count == -1) {
        return false;
    }
    argc -= count;

    return ConnectionConvergenceLayer::parse_link_params(lparams, argc, argv,
                                                         invalidp);
}

//----------------------------------------------------------------------
bool
StreamConvergenceLayer::finish_init_link(Link* link, LinkParams* lparams)
{
    StreamLinkParams* params = dynamic_cast<StreamLinkParams*>(lparams);
    ASSERT(params != NULL);

    // make sure to set the reliability bit in the link structure
    if (params->segment_ack_enabled_) {
        link->set_reliable(true);
    }
    
    return true;
}

//----------------------------------------------------------------------
void
StreamConvergenceLayer::dump_link(Link* link, oasys::StringBuffer* buf)
{
    ConnectionConvergenceLayer::dump_link(link, buf);
    
    StreamLinkParams* params =
        dynamic_cast<StreamLinkParams*>(link->cl_info());
    ASSERT(params != NULL);
    
    buf->appendf("segment_ack_enabled: %u\n", params->segment_ack_enabled_);
    buf->appendf("keepalive_interval: %u\n", params->keepalive_interval_);
    buf->appendf("segment_length: %u\n", params->segment_length_);
}

//----------------------------------------------------------------------
StreamConvergenceLayer::Connection::Connection(const char* classname,
                                               const char* logpath,
                                               ConvergenceLayer* cl,
                                               StreamLinkParams* params,
                                               bool active_connector)
    : CLConnection(classname, logpath, cl, params, active_connector),
      current_inflight_(NULL),
      send_segment_todo_(0),
      recv_segment_todo_(0)
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

    StreamLinkParams* params = stream_lparams();
    
    if (params->segment_ack_enabled_)
        contacthdr.flags |= SEGMENT_ACK_ENABLED;
    
    if (params->reactive_frag_enabled_)
        contacthdr.flags |= REACTIVE_FRAG_ENABLED;
    
    contacthdr.keepalive_interval = htons(params->keepalive_interval_);

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
    note_data_sent();
    send_data();
}

//----------------------------------------------------------------------
void
StreamConvergenceLayer::Connection::handle_contact_initiation()
{
    ASSERT(! contact_up_);

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
    int sdnv_len = SDNV::decode((u_char*)recvbuf_.start() +
                                  sizeof(ContactHeader),
                                recvbuf_.fullbytes() -
                                  sizeof(ContactHeader),
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
    StreamLinkParams* params = stream_lparams();
    
    params->keepalive_interval_ =
        std::min(params->keepalive_interval_,
                 (u_int)contacthdr.keepalive_interval);

    params->segment_ack_enabled_ = params->segment_ack_enabled_ &&
                                   (contacthdr.flags & SEGMENT_ACK_ENABLED);
    
    params->reactive_frag_enabled_ = params->reactive_frag_enabled_ &&
                                     (contacthdr.flags & REACTIVE_FRAG_ENABLED);

    /*
     * Now skip the sdnv that encodes the announce bundle length since
     * we parsed it above.
     */
    recvbuf_.consume(sdnv_len);

    /*
     * Finally, parse the announce bundle and give it to the base
     * class to handle (i.e. by linking us to a Contact if we don't
     * have one).
     */
    TempBundle announce;
    int ret = BundleProtocol::parse_bundle(&announce,
                                           (u_char*)recvbuf_.start(),
                                           recvbuf_.fullbytes());
    
    if (ret != (int)announce_len) {
        if (ret < 0) {
            log_err("protocol error: announce bundle length given as %llu, "
                    "parser requires more bytes", announce_len);
        } else {
            log_err("protocol error: announce bundle length given as %llu, "
                    "parser requires only %u", announce_len, ret);
        }
        break_contact(ContactEvent::BROKEN);
        return;
    }

    handle_announce_bundle(&announce);
    recvbuf_.consume(announce_len);

    /*
     * Now we initialize the various timers that are used for
     * keepalives / idle timeouts to make sure they're not used
     * uninitialized.
     */
    ::gettimeofday(&data_rcvd_, 0);
    ::gettimeofday(&data_sent_, 0);
    ::gettimeofday(&keepalive_sent_, 0);

    /*
     * Finally, we note that the contact is now up.
     */
    contact_up();
}


//----------------------------------------------------------------------
void
StreamConvergenceLayer::Connection::handle_send_bundle(Bundle* bundle)
{
    // push the bundle onto the inflight queue. we'll handle sending
    // the bundle out in the callback for transmit_bundle_data
    InFlightBundle* inflight = new InFlightBundle(bundle);

    inflight->header_block_length_ =
        BundleProtocol::header_block_length(bundle);

    inflight->tail_block_length_ =
        BundleProtocol::tail_block_length(bundle);

    inflight->formatted_length_ =
        inflight->header_block_length_ +
        inflight->bundle_->payload_.length() +
        inflight->tail_block_length_;

    inflight_.push_back(inflight);
}

//----------------------------------------------------------------------
bool
StreamConvergenceLayer::Connection::send_pending_data()
{
    // if the outgoing data buffer is full, we can't do anything until
    // we poll()
    if (sendbuf_.tailbytes() == 0) {
        return false;
    }

    // if we're in the middle of sending a segment, we need to continue
    // sending it. only if we completely send the segment do we fall
    // through to send acks, otherwise we return to try to finish it
    // again later.
    if (send_segment_todo_ != 0) {
        ASSERT(current_inflight_ != NULL);        
        send_data_todo(current_inflight_);
    }
    
    // see if we're broken or write blocked
    if (contact_broken_ || (send_segment_todo_ != 0)) {
        return false;
    }
    
    // now check if there are acks we need to send -- even if it
    // returns true (i.e. we sent an ack), we continue on and try to
    // send some real payload data, otherwise we could get starved by
    // arriving data and never send anything out.
    bool sent_ack = send_pending_acks();
    
    // check if we need to start a new bundle. if we do, then
    // start_next_bundle handles the correct return code
    bool sent_data;
    if (current_inflight_ == NULL) {
        sent_data = start_next_bundle();
    } else {
        // otherwise send the next segment of the current bundle
        sent_data = send_next_segment(current_inflight_);
    }

    return sent_ack || sent_data;
}

//----------------------------------------------------------------------
bool
StreamConvergenceLayer::Connection::send_pending_acks()
{
    if (contact_broken_ || incoming_.empty()) {
        return false; // nothing to do
    }
    IncomingBundle* incoming = incoming_.front();
    DataBitmap::iterator iter = incoming->ack_data_.begin();
    bool generated_ack = false;
    
    // when data segment headers are received, the last bit of the
    // segment is marked in ack_data.
    //
    // therefore, to figure out what acks should be sent for those
    // segments, we set the start iterator to skip past everything
    // we've already acked (recorded in acked_length_), then check to
    // see if we're at the end, or if there's more to ack. 
    //
    // however, we have to be careful to check the recv_data as well
    // to make sure we've actually gotten the segment, since the bit
    // is marked in ack_data when the segment is begun.

    if (iter == incoming->ack_data_.end()) {
        // XXX/demmer this is lame... the END_BUNDLE message comes
        // after the last segment, so until the protocol changes s.t.
        // END_BUNDLE is just a bundle segment with an appropriate bit
        // set, we need to check whether we've received the END_BUNDLE
        // (and set total_length_) to know if we're done with this
        // bundle
        goto check_done;
    }

    while (1) {
        size_t rcvd_bytes  = incoming->rcvd_data_.last() + 1;
        size_t ack_len     = *iter + 1;
        size_t segment_len = ack_len - incoming->acked_length_;
        (void)segment_len;
        
        if (ack_len > rcvd_bytes) {
            log_debug("send_pending_acks: "
                      "waiting to send ack length %zu for %zu byte segment "
                      "since only received %zu",
                      ack_len, segment_len, rcvd_bytes);
            break;
        }

        // make sure we have space in the send buffer
        size_t encoding_len = 1 + SDNV::encoding_len(ack_len);
        if (encoding_len > sendbuf_.tailbytes()) {
            log_debug("send_pending_acks: "
                      "no space for ack in buffer (need %zu, have %zu)",
                      encoding_len, sendbuf_.tailbytes());
            break;
        }
        
        log_debug("send_pending_acks: "
                  "sending ack length %zu for %zu byte segment "
                  "[range %zu..%zu]",
                  ack_len, segment_len, incoming->acked_length_, *iter);
        
        *sendbuf_.end() = ACK_SEGMENT;
        int len = SDNV::encode(ack_len, (u_char*)sendbuf_.end() + 1,
                               sendbuf_.tailbytes() - 1);
        ASSERT(encoding_len = len + 1);
        sendbuf_.fill(encoding_len);

        generated_ack = true;
        incoming->acked_length_ = ack_len;
        incoming->ack_data_.clear(*iter);
        iter = incoming->ack_data_.begin();
        
        if (iter == incoming->ack_data_.end()) {
            break;
        }
        
        log_debug("send_pending_acks: "
                  "found another segment (%zu)", *iter);
    }
    
    if (generated_ack) {
        send_data();
        note_data_sent();
    }

    // now, check if a) we've gotten everything we're supposed to
    // (i.e. total_length_ isn't zero), and b) we're done with all the
    // acks we need to send
 check_done:
    if ((incoming->total_length_ != 0) &&
        (incoming->total_length_ == incoming->acked_length_))
    {
        log_debug("send_pending_acks: acked all %zu bytes of bundle %d",
                  incoming->total_length_, incoming->bundle_->bundleid_);
        
        incoming_.pop_front();
        delete incoming;
    }
    else
    {
        log_debug("send_pending_acks: "
                  "still need to send acks -- total_rcvd %zu, acked_range %zu",
                  incoming->total_length_,
                  incoming->ack_data_.num_contiguous());
    }

    // return true if we've sent something
    return generated_ack;
}

         
//----------------------------------------------------------------------
bool
StreamConvergenceLayer::Connection::start_next_bundle()
{
    ASSERT(current_inflight_ == NULL);

    // find the bundle to start (identified by having nothing yet in
    // sent_data) and store it in current_inflight_
    InFlightList::iterator iter;
    for (iter = inflight_.begin(); iter != inflight_.end(); ++iter) {
        InFlightBundle* inflight = *iter;
        
        if (contact_broken_)
            return false;

        // skip entries that we've sent completely
        if (inflight->sent_data_.num_contiguous() ==
            inflight->formatted_length_)
        {
            log_debug("start_next_bundle: "
                      "transmission of bundle %d already complete, skipping",
                      inflight->bundle_->bundleid_);
            continue;
        }

        // otherwise, we must not have sent anything for this bundle
        // at all, so assert that sent_data is empty
        ASSERT(inflight->sent_data_.empty());
        current_inflight_ = inflight;
        break;
    }

    // there might not be anything to send, in which case we return
    // false to indicate as such
    if (current_inflight_ == NULL) {
        return false;
    }

    Bundle* bundle = current_inflight_->bundle_.object();
    StreamLinkParams* params = stream_lparams();
    
    // to start off the transmission, we send the START_BUNDLE
    // typecode, followed by a segment of bundle data, including at a
    // minimum the bundle headers.

    size_t header_len = BundleProtocol::header_block_length(bundle);
    size_t segment_len = std::min((size_t)params->segment_length_,
                                  current_inflight_->formatted_length_);
    if (segment_len < header_len) {
        log_warn("configured segment_len %zu smaller than header segment "
                 "length %zu... upping segment length", segment_len, header_len);
        segment_len = header_len;
    }
    
    size_t sdnv_len = SDNV::encoding_len(segment_len);

    // we need to format all the headers at once, so make sure the
    // buffer has enough space for that. note that the segment on the
    // wire might include some payload bytes that we don't need to
    // handle here.
    size_t min_buffer_len = 1 + 1 + sdnv_len + header_len;
    if (min_buffer_len > sendbuf_.tailbytes()) {
        if (min_buffer_len > sendbuf_.size()) {
            log_warn("start_bundle: "
                     "not enough space for headers [need %zu, have %zu] "
                     "(expanding buffer)",
                     min_buffer_len, sendbuf_.size());
            sendbuf_.reserve(min_buffer_len);
            ASSERT(min_buffer_len <= sendbuf_.tailbytes());
            
        } else {
            log_debug("start_bundle: "
                      "not enough space for headers [need %zu, have %zu] "
                      "(waiting for buffer to drain)",
                      min_buffer_len, sendbuf_.tailbytes());
            
            current_inflight_ = NULL;
            return false;
        }
    }

    log_debug("send_start_bundle: sending "
              "START_BUNDLE for bundle %d: initial segment len %zu [hdrs %zu]",
              current_inflight_->bundle_->bundleid_, segment_len, header_len);
    
    u_char* bp = (u_char*)sendbuf_.end();
    size_t len = sendbuf_.tailbytes();

    *bp++ = START_BUNDLE;
    len--;
    
    *bp++ = DATA_SEGMENT;
    len--;
    
    int cc = SDNV::encode(segment_len, bp, len);
    ASSERT(cc == (int)sdnv_len);
        
    bp  += sdnv_len;
    len -= sdnv_len;
    
    cc = BundleProtocol::format_header_blocks(bundle, bp, len);
    ASSERT(cc == (int)header_len);
    
    sendbuf_.fill(1 + 1 + sdnv_len + header_len);
    current_inflight_->sent_data_.set(0, header_len);

    // now that the header is done, we record the amount of the first
    // segment that's left to send and then call send_data_todo to send
    // out the payload. if there's nothing left to do (i.e. the first
    // segment is exactly the length of the headers), then we return
    // true to indicate that we want to make another pass through
    send_segment_todo_ = segment_len - header_len;

    if (send_segment_todo_ == 0) {
        note_data_sent();
        send_data();
        return true;
    }

    return send_data_todo(current_inflight_);
}

//----------------------------------------------------------------------
bool
StreamConvergenceLayer::Connection::send_next_segment(InFlightBundle* inflight)
{
    if (sendbuf_.tailbytes() == 0) {
        return false;
    }

    ASSERT(send_segment_todo_ == 0);

    StreamLinkParams* params = stream_lparams();
    Bundle* bundle = inflight->bundle_.object();

    // we must have already sent at least the header segment
    ASSERT(!inflight->sent_data_.empty());

    size_t bytes_sent   = inflight->sent_data_.last() + 1;
    size_t payload_sent = bytes_sent - inflight->header_block_length_;
    size_t payload_len  = bundle->payload_.length();

    if (payload_sent == payload_len) {
        log_debug("send_next_segment: "
                  "already sent all %zu bytes of payload, finishing bundle",
                  payload_len);
        return finish_bundle(inflight);
    }
    
    size_t segment_len = std::min((size_t)params->segment_length_,
                                  payload_len - payload_sent);
    
    ASSERT(payload_sent + segment_len <= payload_len);

    size_t sdnv_len = SDNV::encoding_len(segment_len);
    
    if (sendbuf_.tailbytes() < 1 + sdnv_len) {
        log_debug("send_next_segment: "
                  "not enough space for segment header [need %zu, have %zu]",
                  1 + sdnv_len, sendbuf_.tailbytes());
        return false;
    }
    
    log_debug("send_next_segment: "
              "starting %zu byte segment [payload range %zu..%zu]",
              segment_len, payload_sent, payload_sent + segment_len);

    u_char* bp = (u_char*)sendbuf_.end();
    *bp++ = DATA_SEGMENT;
    int cc = SDNV::encode(segment_len, bp, sendbuf_.tailbytes() - 1);
    ASSERT(cc == (int)sdnv_len);
    bp += sdnv_len;
    
    sendbuf_.fill(1 + sdnv_len);
    send_segment_todo_ = segment_len;

    // send_data_todo actually does the deed
    return send_data_todo(inflight);
}

//----------------------------------------------------------------------
bool
StreamConvergenceLayer::Connection::send_data_todo(InFlightBundle* inflight)
{
    ASSERT(send_segment_todo_ != 0);
    
    while (send_segment_todo_ != 0 && sendbuf_.tailbytes() != 0) {
        u_char* bp = (u_char*)sendbuf_.end();
        
        size_t bytes_sent     = inflight->sent_data_.last() + 1;
        size_t payload_offset = bytes_sent - inflight->header_block_length_;
        size_t send_len       = std::min(send_segment_todo_, sendbuf_.tailbytes());
    
        Bundle* bundle = inflight->bundle_.object();
        bundle->payload_.read_data(payload_offset, send_len, bp,
                                   BundlePayload::FORCE_COPY);
        sendbuf_.fill(send_len);
    
        inflight->sent_data_.set(bytes_sent, send_len);
    
        log_debug("send_data_todo: "
                  "sent %zu/%zu of current segment from payload offset %zu "
                  "(%zu todo), updated sent_data *%p",
                  send_len, send_segment_todo_, payload_offset,
                  send_segment_todo_ - send_len, &inflight->sent_data_);

        send_segment_todo_ -= send_len;

        note_data_sent();
        send_data();
        
        if (contact_broken_)
            return true;
    }

    return (send_segment_todo_ == 0);
}

//----------------------------------------------------------------------
bool
StreamConvergenceLayer::Connection::finish_bundle(InFlightBundle* inflight)
{
    if (sendbuf_.tailbytes() == 0) {
        log_warn("finish_bundle: rare out of space condition... "
                 "making room for one byte");
        sendbuf_.reserve(1);
        ASSERT(sendbuf_.tailbytes() > 0);
    }
    
    log_debug("finish_bundle: sending END_BUNDLE message");
    
    *sendbuf_.end() = END_BUNDLE;
    sendbuf_.fill(1);
    
    note_data_sent();
    send_data();

    ASSERT(! inflight->bundle_->payload_.is_file_open());
    
    ASSERT(current_inflight_ == inflight);
    current_inflight_ = NULL;
    
    check_completed(inflight);
    
    return true;
}

//----------------------------------------------------------------------
void
StreamConvergenceLayer::Connection::check_completed(InFlightBundle* inflight)
{
    // we can pop the inflight bundle off of the queue and clean it up
    // only when both finish_bundle is called (so current_inflight_ no
    // longer points to the inflight bundle), and after the final ack
    // for the bundle has been received (determined by looking at
    // inflight->ack_data_)

    if (current_inflight_ == inflight) {
        log_debug("check_completed: bundle %d still waiting for finish_bundle",
                  inflight->bundle_->bundleid_);
        return;
    }

    size_t acked_len = inflight->ack_data_.num_contiguous();
    if (acked_len < inflight->formatted_length_) {
        log_debug("check_completed: bundle %d only acked %zu/%zu",
                  inflight->bundle_->bundleid_,
                  acked_len, inflight->formatted_length_);
        return;
    }

    log_debug("check_completed: bundle %d transmission complete",
              inflight->bundle_->bundleid_);
    ASSERT(inflight == inflight_.front());
    inflight_.pop_front();
    delete inflight;
}

//----------------------------------------------------------------------
void
StreamConvergenceLayer::Connection::send_keepalive()
{
    // there's no point in putting another byte in the buffer if
    // there's already data waiting to go out, since the arrival of
    // that data on the other end will do the same job as the
    // keepalive byte
    if (sendbuf_.fullbytes() != 0) {
        log_debug("send_keepalive: "
                  "send buffer has %zu bytes queued, suppressing keepalive",
                  sendbuf_.fullbytes());
        return;
    }
    ASSERT(sendbuf_.tailbytes() > 0);

    ::gettimeofday(&keepalive_sent_, 0);

    *(sendbuf_.end()) = KEEPALIVE;
    sendbuf_.fill(1);

    // don't note_data_sent() here since keepalive messages shouldn't
    // be counted for keeping an idle link open
    send_data();
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
    struct timeval now;
    u_int elapsed, elapsed2;

    StreamLinkParams* params = dynamic_cast<StreamLinkParams*>(params_);
    ASSERT(params != NULL);
    
    ::gettimeofday(&now, 0);
    
    // check that it hasn't been too long since we got some data from
    // the other side
    elapsed = TIMEVAL_DIFF_MSEC(now, data_rcvd_);
    if (elapsed > params->data_timeout_) {
        log_info("handle_poll_timeout: no data heard for %d msecs "
                 "(keepalive_sent %u.%u, data_rcvd %u.%u, now %u.%u) "
                 "-- closing contact",
                 elapsed,
                 (u_int)keepalive_sent_.tv_sec,
                 (u_int)keepalive_sent_.tv_usec,
                 (u_int)data_rcvd_.tv_sec, (u_int)data_rcvd_.tv_usec,
                 (u_int)now.tv_sec, (u_int)now.tv_usec);
            
        break_contact(ContactEvent::BROKEN);
        return;
    }

    // check if the connection has been idle for too long
    // (on demand links only)
    if (contact_->link()->type() == Link::ONDEMAND) {
        u_int idle_close_time = contact_->link()->params().idle_close_time_;
        
        elapsed  = TIMEVAL_DIFF_MSEC(now, data_rcvd_);
        elapsed2 = TIMEVAL_DIFF_MSEC(now, data_sent_);
        
        if (idle_close_time != 0 &&
            (elapsed > idle_close_time * 1000) &&
            (elapsed2 > idle_close_time * 1000))
        {
            log_info("closing idle connection "
                     "(no data received for %d msecs or sent for %d msecs)",
                     elapsed, elapsed2);
            break_contact(ContactEvent::IDLE);
            return;
        } else {
            log_debug("connection not idle: recvd %d / sent %d <= timeout %d",
                      elapsed, elapsed2, idle_close_time * 1000);
        }
    }
    
    // check if it's time for us to send a keepalive (i.e. that we
    // haven't sent some data or another keepalive in at least the
    // configured keepalive_interval)
    if (params->keepalive_interval_ != 0) {
        elapsed  = TIMEVAL_DIFF_MSEC(now, data_sent_);
        elapsed2 = TIMEVAL_DIFF_MSEC(now, keepalive_sent_);

        if (std::min(elapsed, elapsed2) > (params->keepalive_interval_ * 1000))
        {
            log_debug("handle_poll_timeout: sending keepalive");
            send_keepalive();
        }
    }
}

//----------------------------------------------------------------------
void
StreamConvergenceLayer::Connection::process_data()
{
    if (recvbuf_.fullbytes() == 0) {
        return;
    }

    log_debug("processing up to %zu bytes from receive buffer",
              recvbuf_.fullbytes());

    // all data (keepalives included) should be noted since it's used
    // for generating new keepalives
    note_data_rcvd();

    // the first thing we need to do is handle the contact initiation
    // sequence, i.e. the contact header and the announce bundle. we
    // know we need to do this if we haven't yet called contact_up()
    if (! contact_up_) {
        handle_contact_initiation();
        return;
    }

    // if a data segment is bigger than the receive buffer. when
    // processing a data segment, we mark the unread amount in the
    // data_segment_remaining_ field so if that's not zero, we need to
    // drain it, then fall through to handle the rest of the buffer
    if (recv_segment_todo_ != 0) {
        bool ok = handle_data_todo();
        
        if (!ok) {
            return;
        }
    }
    
    // now, drain cl messages from the receive buffer. we peek at the
    // first byte and dispatch to the correct handler routine
    // depending on the type of the CL message. we don't consume the
    // byte yet since there's a possibility that we need to read more
    // from the remote side to handle the whole message
    while (recvbuf_.fullbytes() != 0) {
        if (contact_broken_) return;
        
        char type = *recvbuf_.start();

        log_debug("recvbuf has %zu full bytes, dispatching to handler routine",
                  recvbuf_.fullbytes());
        bool ok;
        switch (type) {
        case START_BUNDLE:
            ok = handle_start_bundle();
            break;
        case END_BUNDLE:
            ok = handle_end_bundle();
            break;
        case DATA_SEGMENT:
            ok = handle_data_segment();
            break;
        case ACK_SEGMENT:
            ok = handle_ack_segment();
            break;
        case KEEPALIVE:
            ok = handle_keepalive();
            break;
        case SHUTDOWN:
            ok = handle_shutdown();
            break;
        default:
            log_err("invalid CL message type 0x%x", type);
            break_contact(ContactEvent::BROKEN);
            return;
        }

        // if there's not enough data in the buffer to handle the
        // message, make sure there's space to receive more
        if (! ok) {
            if (recvbuf_.fullbytes() == recvbuf_.size()) {
                log_warn("process_data: "
                         "%zu byte recv buffer full but too small for msg %u... "
                         "doubling buffer size",
                         recvbuf_.size(), type);
                
                recvbuf_.reserve(recvbuf_.size() * 2);

            } else if (recvbuf_.tailbytes() == 0) {
                // force it to move the full bytes up to the front
                recvbuf_.reserve(recvbuf_.size() - recvbuf_.fullbytes());
                ASSERT(recvbuf_.tailbytes() != 0);
            }
            
            return;
        }
    }
}

//----------------------------------------------------------------------
void
StreamConvergenceLayer::Connection::note_data_rcvd()
{
    log_debug("noting data_rcvd");
    ::gettimeofday(&data_rcvd_, 0);
}

//----------------------------------------------------------------------
void
StreamConvergenceLayer::Connection::note_data_sent()
{
    log_debug("noting data_sent");
    ::gettimeofday(&data_sent_, 0);
}

//----------------------------------------------------------------------
bool
StreamConvergenceLayer::Connection::handle_start_bundle()
{
    recvbuf_.consume(1);

    if (! incoming_.empty()) {
        IncomingBundle* incoming = incoming_.back();
        if (incoming->total_length_ == 0) {
            log_err("protocol error: got START_BUNDLE before bundle completed");
            break_contact(ContactEvent::BROKEN);
            return false;
        }
    }
        
    log_debug("got START_BUNDLE message, creating new IncomingBundle");
    IncomingBundle* incoming = new IncomingBundle(NULL);
    incoming_.push_back(incoming);
    
    return true;
}

//----------------------------------------------------------------------
bool
StreamConvergenceLayer::Connection::handle_end_bundle()
{
    recvbuf_.consume(1);
    
    if (incoming_.empty()) {
        log_err("protocol error: got END_BUNDLE with no incoming bundle");
        // XXX/demmer protocol error
        break_contact(ContactEvent::BROKEN);
        return false;
    }
    
    IncomingBundle* incoming = incoming_.back();

    if (incoming->rcvd_data_.empty()) {
        log_err("protocol error: got END_BUNDLE with no DATA_SEGMENT");

        // XXX/demmer protocol error
        break_contact(ContactEvent::BROKEN);
        return false;
    }

    // note that we could get an END_BUNDLE message before the whole
    // payload has been transmitted, in case the other side is doing
    // some fancy reordering. we mark the total amount in the
    // IncomingBundle structure so send_pending_acks knows when we're
    // done.
    
    incoming->total_length_ = incoming->rcvd_data_.last() + 1;
    
    size_t formatted_len =
        BundleProtocol::formatted_length(incoming->bundle_.object());

    log_debug("got END_BUNDLE: rcvd %zu / %zu",
              incoming->total_length_, formatted_len);
    
    if (incoming->total_length_ > formatted_len) {
        log_err("protocol error: received too much data -- "
                "got %zu, formatted length %zu",
                incoming->total_length_, formatted_len);

        // we pretend that we got nothing so the cleanup code in
        // CLConnection::close_contact doesn't try to post a received
        // event for the bundle
        incoming->rcvd_data_.clear();

        // XXX/demmer protocol error
        break_contact(ContactEvent::BROKEN);
        return false;
    }

    // XXX/demmer need to fix the fragmentation code to assume the
    // event includes the header bytes as well as the payload.
    size_t payload_rcvd = incoming->total_length_ -
                          incoming->header_block_length_;

    incoming->bundle_->payload_.close_file();
    
    BundleDaemon::post(
        new BundleReceivedEvent(incoming->bundle_.object(),
                                EVENTSRC_PEER,
                                payload_rcvd));

    return true;
}

//----------------------------------------------------------------------
bool
StreamConvergenceLayer::Connection::handle_data_segment()
{
    if (incoming_.empty()) {
        log_err("protocol error: got data segment before START_BUNDLE");
        break_contact(ContactEvent::BROKEN);
        return false;
    }

    u_char* bp = (u_char*)recvbuf_.start();

    size_t segment_offset;
    u_int32_t segment_len;
    
    int sdnv_len = SDNV::decode(bp + 1, recvbuf_.fullbytes() - 1, &segment_len);

    if (sdnv_len < 0) {
        log_debug("handle_data_segment: too few bytes for sdnv (%zu)",
                  recvbuf_.fullbytes());
        return false;
    }

    // Note that there may be more than one incoming bundle on the
    // IncomingList, but it's the one at the back that we're reading
    // in data for. Others are waiting for acks to be sent.
    IncomingBundle* incoming = incoming_.back();
    
    // if this is the first segment, try to parse the headers and create
    // the bundle structure. if we can't handle it fully, we leave the
    // partial header bytes in the buffer (potentially increasing its
    // size) and return to wait for more data to arrive.
    if (incoming->rcvd_data_.empty()) {
        Bundle* bundle = new Bundle();

        size_t rcvd_bytes = recvbuf_.fullbytes() - 1 - sdnv_len;
        int header_len = BundleProtocol::parse_header_blocks(bundle,
                                                             bp + 1 + sdnv_len,
                                                             rcvd_bytes);
        if (header_len < 0) {
            log_debug("handle_data_segment: "
                      "not enough data to parse bundle headers [have %zu]",
                      recvbuf_.fullbytes());
            delete bundle;
            return false;
        }

        log_debug("handle_data_segment: "
                  "got header segments [len %u] for bundle *%p",
                  header_len, bundle);
        
        incoming->bundle_ = bundle;
        incoming->header_block_length_ = header_len;
        recvbuf_.consume(1 + sdnv_len + header_len);
        incoming->rcvd_data_.set(0, header_len);

        segment_offset = 0;
        recv_segment_todo_ = segment_len - header_len;

    } else {
        // this is a chunk of payload and/or tail block
        segment_offset        = incoming->rcvd_data_.num_contiguous();
        size_t payload_offset = segment_offset - incoming->header_block_length_;
        (void)payload_offset;

        log_debug("handle_data_segment: "
                  "got segment of length %u at offset %zu "
                  "(payload offset %zu/%zu)",
                  segment_len, segment_offset, payload_offset,
                  incoming->bundle_->payload_.length());
        
        recvbuf_.consume(1 + sdnv_len);
        
        recv_segment_todo_ = segment_len;
    }
    
    incoming->ack_data_.set(segment_offset + segment_len - 1);

    log_debug("handle_data_segment: "
              "updated recv_data (segment_offset %zu) *%p ack_data *%p",
              segment_offset, &incoming->rcvd_data_, &incoming->ack_data_);

    if (recv_segment_todo_ != 0) {
        handle_data_todo();
    }
    
    return true;
}

//----------------------------------------------------------------------
bool
StreamConvergenceLayer::Connection::handle_data_todo()
{
    // We shouldn't get ourselves here unless there's something
    // incoming and there's something left to read
    ASSERT(!incoming_.empty());
    ASSERT(recv_segment_todo_ != 0);
    
    // Note that there may be more than one incoming bundle on the
    // IncomingList. There's always only one (at the back) that we're
    // reading in data for, the rest are waiting for acks to go out
    IncomingBundle* incoming = incoming_.back();

    size_t rcvd_offset    = incoming->rcvd_data_.num_contiguous();
    size_t rcvd_len       = recvbuf_.fullbytes();
    size_t payload_offset = rcvd_offset - incoming->header_block_length_;
    size_t payload_len    = incoming->bundle_->payload_.length();
    size_t chunk_len      = std::min(rcvd_len, recv_segment_todo_);

    if (rcvd_len == 0) {
        return false; // nothing to do
    }
    
    log_debug("handle_data_todo: "
              "reading todo segment %zu/%zu at payload offset %zu/%zu",
              chunk_len, recv_segment_todo_, payload_offset, payload_len);
    
    if (chunk_len + payload_offset > payload_len) {
        log_err("NEED TO HANDLE BLOCKS AFTER THE PAYLOAD");
        break_contact(ContactEvent::BROKEN);
        return false;
    }

    u_char* bp = (u_char*)recvbuf_.start();
    incoming->bundle_->payload_.write_data(bp, payload_offset, chunk_len);

    recv_segment_todo_ -= chunk_len;
    recvbuf_.consume(chunk_len);

    incoming->rcvd_data_.set(rcvd_offset, chunk_len);
    
    log_debug("handle_data_todo: "
              "updated recv_data (rcvd_offset %zu) *%p ack_data *%p",
              rcvd_offset, &incoming->rcvd_data_, &incoming->ack_data_);
    
    if (recv_segment_todo_ == 0) {
        return true; // completed segment
    }

    return false;
}

//----------------------------------------------------------------------
bool
StreamConvergenceLayer::Connection::handle_ack_segment()
{
    u_char* bp = (u_char*)recvbuf_.start();
    u_int32_t acked_len;
    int sdnv_len = SDNV::decode(bp + 1, recvbuf_.fullbytes() - 1, &acked_len);
    
    if (sdnv_len < 0) {
        log_debug("handle_ack_segment: too few bytes for sdnv (%zu)",
                  recvbuf_.fullbytes());
        return false;
    }

    recvbuf_.consume(1 + sdnv_len);

    if (inflight_.empty()) {
        log_err("protocol error: got ack segment with no inflight bundle");
        break_contact(ContactEvent::BROKEN);
        return false;
    }

    InFlightBundle* inflight = inflight_.front();

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
        log_debug("handle_ack_segment: "
                  "got final ack for %zu byte range -- acked_len %u, ack_data *%p",
                  (size_t)acked_len - ack_begin, acked_len, &inflight->ack_data_);

        // XXX/demmer change this event to include the header bytes
        
        BundleDaemon::post(
            new BundleTransmittedEvent(inflight->bundle_.object(),
                                       contact_,
                                       inflight->sent_data_.num_contiguous() -
                                         inflight->header_block_length_,
                                       inflight->ack_data_.num_contiguous() -
                                         inflight->header_block_length_));

        // might delete inflight
        check_completed(inflight);
        
    } else {
        log_debug("handle_ack_segment: "
                  "got acked_len %u (%zu byte range) -- ack_data *%p",
                  acked_len, (size_t)acked_len - ack_begin, &inflight->ack_data_);
    }

    return true;
}

//----------------------------------------------------------------------
bool
StreamConvergenceLayer::Connection::handle_keepalive()
{
    log_debug("got keepalive message");
    recvbuf_.consume(1);
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
