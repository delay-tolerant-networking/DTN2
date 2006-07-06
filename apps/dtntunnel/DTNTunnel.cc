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

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <sys/socket.h>

#include "dtn_api.h"
#include "APIEndpointIDOpt.h"

#include <oasys/util/Getopt.h>

#include "DTNTunnel.h"
#include "TCPTunnel.h"
#include "UDPTunnel.h"

namespace dtntunnel {

template <>
DTNTunnel* oasys::Singleton<DTNTunnel>::instance_ = 0;

//----------------------------------------------------------------------
DTNTunnel::DTNTunnel()
    : Logger("DTNTunnel", "/dtntunnel"),
      loglevelstr_(""),
      loglevel_(LOG_DEFAULT_THRESHOLD),
      logfile_("-"),
      send_lock_("/dtntunnel", oasys::Mutex::TYPE_RECURSIVE, true),
      listen_(false),
      expiration_(30),
      tcp_(false),
      udp_(false),
      local_addr_(INADDR_ANY),
      local_port_(0),
      remote_addr_(INADDR_NONE),
      remote_port_(0),
      max_size_(4096)
{
    memset(&local_eid_, 0, sizeof(local_eid_));
    memset(&dest_eid_,  0, sizeof(dest_eid_));
}

//----------------------------------------------------------------------
void
DTNTunnel::get_options(int argc, char* argv[])
{
    oasys::Getopt::addopt(
        new oasys::StringOpt('o', "output", &logfile_, "<output>",
                             "file name for error logging output "
                             "(- indicates stdout)"));

    oasys::Getopt::addopt(
        new oasys::StringOpt('l', NULL, &loglevelstr_, "<level>",
                             "default log level [debug|warn|info|crit]"));

    oasys::Getopt::addopt(
        new oasys::BoolOpt('L', "listen", &listen_,
                           "run in listen mode for incoming CONN bundles"));
    
    oasys::Getopt::addopt(
        new oasys::UIntOpt('e', "expiration", &expiration_, "<secs>",
                           "expiration time"));
    
    oasys::Getopt::addopt(
        new oasys::BoolOpt('t', "tcp", &tcp_,
                           "proxy for TCP connections"));
    
    oasys::Getopt::addopt(
        new oasys::BoolOpt('u', "udp", &udp_,
                           "proxy for UDP traffic"));
    
    bool dest_eid_set = false;
    oasys::Getopt::addopt(
        new dtn::APIEndpointIDOpt('d', "dest_eid", &dest_eid_, "<eid>",
                                  "destination endpoint id", &dest_eid_set));
    
    oasys::Getopt::addopt(
        new dtn::APIEndpointIDOpt("local_eid_override", &local_eid_, "<eid>",
                                  "local endpoint id"));
    
    oasys::Getopt::addopt(
        new oasys::InAddrOpt("laddr", &local_addr_, "<addr>",
                             "local address to listen on"));
    
    oasys::Getopt::addopt(
        new oasys::UInt16Opt("lport", &local_port_, "<port>",
                             "local port to listen on"));
    
    oasys::Getopt::addopt(
        new oasys::InAddrOpt("rhost", &remote_addr_, "<addr>",
                             "remote host/address to proxy for"));
    
    oasys::Getopt::addopt(
        new oasys::UInt16Opt("rport", &remote_port_, "<port>",
                             "remote port to proxy"));
    
    oasys::Getopt::addopt(
        new oasys::UIntOpt('z', "max_size", &max_size_, "<bytes>",
                           "maximum bundle size for stream transports (e.g. tcp)"));
    
    int remainder = oasys::Getopt::getopt(argv[0], argc, argv);
    if (remainder != argc) {
        fprintf(stderr, "invalid argument '%s'\n", argv[remainder]);
 usage:
        oasys::Getopt::usage(argv[0]);
        exit(1);
    }
    
#define CHECK_OPT(_condition, _err) \
    if ((_condition)) { \
        fprintf(stderr, "error: " _err "\n"); \
        goto usage; \
    }
    
    //
    // TODO?:  couldn't we use these in listen mode to over-ride
    // what is provided at the sender? -kfall.  Also: mcast for udp?
    //
    if (listen_) {
        CHECK_OPT(dest_eid_set, "setting destination eid is "
                  "meaningless in listen mode");
        CHECK_OPT((tcp_ != false) || (udp_ != false), "setting tcp or udp is "
                  "meaningless in listen mode");
        CHECK_OPT(local_port_  != 0,  "setting local port is "
                  "meaningless in listen mode");
        CHECK_OPT(remote_addr_ != INADDR_NONE, "setting remote host is "
                  "meaningless in listen mode");
        CHECK_OPT(remote_port_ != 0,  "setting remote port is "
                  "meaningless in listen mode");
    } else {
        CHECK_OPT(!dest_eid_set, "must set destination eid "
                  "or be in listen mode");
        CHECK_OPT((tcp_ == false) && (udp_ == false),
                  "must set either tcp or udp mode");
        CHECK_OPT((tcp_ != false) && (udp_ != false),
                  "cannot set both tcp and udp mode");
        CHECK_OPT(local_addr_  == INADDR_NONE, "local addr is invalid");
        CHECK_OPT(local_port_  == 0,  "must set local port");
        CHECK_OPT(remote_addr_ == INADDR_NONE, "must set remote host");
        CHECK_OPT(remote_port_ == 0,  "must set remote port");
    }
    
#undef CHECK_OPT
}

//----------------------------------------------------------------------
void
DTNTunnel::init_log()
{
    // Parse the debugging level argument
    if (loglevelstr_.length() == 0) 
    {
        loglevel_ = oasys::LOG_NOTICE;
    }
    else 
    {
        loglevel_ = oasys::str2level(loglevelstr_.c_str());
        if (loglevel_ == oasys::LOG_INVALID) 
        {
            fprintf(stderr, "invalid level value '%s' for -l option, "
                    "expected debug | info | warning | error | crit\n",
                    loglevelstr_.c_str());
            exit(1);
        }
    }
    oasys::Log::init(logfile_.c_str(), loglevel_, "", "~/.dtndebug");
}

//----------------------------------------------------------------------
void
DTNTunnel::init_tunnel()
{
    tcptunnel_ = new TCPTunnel();
    udptunnel_ = new UDPTunnel();

    if (!listen_) {
        if (tcp_) {
            tcptunnel_->add_listener(local_addr_, local_port_,
                                     remote_addr_, remote_port_);
        } else if (udp_) {
            udptunnel_->add_listener(local_addr_, local_port_,
                                     remote_addr_, remote_port_);
        }
    }
}

//----------------------------------------------------------------------
void
DTNTunnel::init_registration()
{
    int err = dtn_open(&recv_handle_);
    if (err != DTN_SUCCESS) {
        log_crit("can't open recv handle to daemon: %s",
                 dtn_strerror(err));
        exit(1);
    }
    
    err = dtn_open(&send_handle_);
    if (err != DTN_SUCCESS) {
        log_crit("can't open send handle to daemon: %s",
                 dtn_strerror(err));
        exit(1);
    }

    if (local_eid_.uri[0] == '\0') {
        err = dtn_build_local_eid(recv_handle_, &local_eid_, "dtntunnel");
        if (err != DTN_SUCCESS) {
            log_crit("can't build local eid: %s",
                     dtn_strerror(dtn_errno(recv_handle_)));
            exit(1);
        }
    }

    log_debug("using local endpoint id %s", local_eid_.uri);

    u_int32_t regid;
    err = dtn_find_registration(recv_handle_, &local_eid_, &regid);
    if (err == 0) {
        log_notice("found existing registration id %d, calling dtn_bind",
                   regid);
        
        err = dtn_bind(recv_handle_, regid);
        if (err != 0) {
            log_crit("error in dtn_bind: %s", 
                     dtn_strerror(dtn_errno(recv_handle_)));
            exit(1);
        }
    } else if (dtn_errno(recv_handle_) == DTN_ENOTFOUND) {
        dtn_reg_info_t reginfo;
        memset(&reginfo, 0, sizeof(reginfo));
        dtn_copy_eid(&reginfo.endpoint, &local_eid_);
        reginfo.failure_action = DTN_REG_DEFER;
        reginfo.expiration     = 60 * 60 * 24; // 1 day

        err = dtn_register(recv_handle_, &reginfo, &regid);
        if (err != 0) {
            log_crit("error in dtn_register: %s",
                     dtn_strerror(dtn_errno(recv_handle_)));
            exit(1);
        }
    } else {
        log_crit("error in dtn_find_registration: %s",
                 dtn_strerror(dtn_errno(recv_handle_)));
        exit(1);
    }
}

//----------------------------------------------------------------------
int
DTNTunnel::send_bundle(dtn::APIBundle* bundle, dtn_endpoint_id_t* dest_eid)
{
    // lock to coordinate access to the send_handle_ from multiple
    // client threads
    oasys::ScopeLock l(&send_lock_, "DTNTunnel::send_bundle");
    
    dtn_bundle_spec_t spec;
    memset(&spec, 0, sizeof(spec));
    dtn_copy_eid(&spec.source, &local_eid_);
    dtn_copy_eid(&spec.dest,   dest_eid);
    spec.priority   = COS_NORMAL;
    spec.dopts      = DOPTS_NONE;
    spec.expiration = expiration_;

    dtn_bundle_payload_t payload;
    memset(&payload, 0, sizeof(payload));

    int err = dtn_set_payload(&payload,
                              DTN_PAYLOAD_MEM,
                              bundle->payload_.buf(),
                              bundle->payload_.len());
    if (err != 0) {
        log_err("error setting payload: %s",
                dtn_strerror(dtn_errno(recv_handle_)));
        return err;
    }
    
    dtn_bundle_id_t bundle_id;
    memset(&bundle_id, 0, sizeof(bundle_id));

    err = dtn_send(send_handle_, &spec, &payload, &bundle_id);
    if (err != 0) {
        log_err("error sending bundle: %s",
                dtn_strerror(dtn_errno(recv_handle_)));
        return err;
    }

    log_info("sent %zu byte bundle", bundle->payload_.len());

    return DTN_SUCCESS;
}

//----------------------------------------------------------------------
int
DTNTunnel::handle_bundle(dtn_bundle_spec_t* spec,
                         dtn_bundle_payload_t* payload)
{
    dtn::APIBundle* b = new dtn::APIBundle();
    b->spec_ = *spec;
    ASSERT(payload->location == DTN_PAYLOAD_MEM);

    int len = payload->dtn_bundle_payload_t_u.buf.buf_len;

    if (len < (int)sizeof(BundleHeader)) {
        log_err("too short bundle: len %d < sizeof bundle header", len);
        delete b;
        return -1;
    }
    
    char* dst = b->payload_.buf(len);
    memcpy(dst, payload->dtn_bundle_payload_t_u.buf.buf_val, len);
    b->payload_.set_len(len);
    
    BundleHeader* hdr = (BundleHeader*)dst;
    
    switch (hdr->protocol_) {
    case IPPROTO_UDP: udptunnel_->handle_bundle(b); break;
    case IPPROTO_TCP: tcptunnel_->handle_bundle(b); break;
    default:
        log_err("unknown protocol %d in %d byte tunnel bundle",
                hdr->protocol_, len);
        delete b;
        return -1;
    }

    return 0;
}

//----------------------------------------------------------------------
int
DTNTunnel::main(int argc, char* argv[])
{
    get_options(argc, argv);
    init_log();
    log_notice("DTNTunnel starting up...");

    init_tunnel();
    init_registration();

    log_debug("DTNTunnel starting receive loop...");
    
    while (1) {
        dtn_bundle_spec_t    spec;
        dtn_bundle_payload_t payload;

        memset(&spec,    0, sizeof(spec));
        memset(&payload, 0, sizeof(payload));
    
        log_debug("calling dtn_recv...");
        int err = dtn_recv(recv_handle_, &spec, DTN_PAYLOAD_MEM, &payload,
                           DTN_TIMEOUT_INF);
        if (err != 0) {
            log_err("error in dtn_recv: %s",
                    dtn_strerror(dtn_errno(recv_handle_)));
            break;
        }

        log_info("got %d byte bundle",
                 payload.dtn_bundle_payload_t_u.buf.buf_len);

        handle_bundle(&spec, &payload);

        dtn_free_payload(&payload);
    }

    dtn_close(recv_handle_);
    dtn_close(send_handle_);
    
    return 0;
}

} // namespace dtntunnel

int
main(int argc, char** argv)
{
    dtntunnel::DTNTunnel::create();
    dtntunnel::DTNTunnel::instance()->main(argc, argv);
}
