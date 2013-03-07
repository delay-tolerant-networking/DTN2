/*
 *    Copyright 2004-2006 Intel Corporation
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

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

#include <oasys/io/NetUtils.h>
#include <oasys/util/Getopt.h>
#include <oasys/util/StringBuffer.h>
#include <oasys/util/StringUtils.h>

#include "DTNTunnel.h"
#include "TCPTunnel.h"
#include "UDPTunnel.h"

template <>
dtntunnel::DTNTunnel* oasys::Singleton<dtntunnel::DTNTunnel>::instance_ = 0;

namespace dtntunnel {

//----------------------------------------------------------------------
DTNTunnel::DTNTunnel()
    : App("DTNTunnel", "dtntunnel"),
      send_lock_("/dtntunnel", oasys::Mutex::TYPE_RECURSIVE, true),
      listen_(false),
      custody_(false),
      expiration_(600),
      tcp_(true),
      udp_(false),
      reorder_udp_(false),
      local_addr_(htonl(INADDR_ANY)),
      local_port_(0),
      remote_addr_(htonl(INADDR_NONE)),
      remote_port_(0),
      delay_(0),
      delay_set_(false),
      max_size_(32 * 1024),
      tunnel_spec_(""),
      tunnel_spec_set_(false),
      transparent_(false)
{
    // override default logging setting
    loglevel_ = oasys::LOG_NOTICE;
    
    memset(&local_eid_, 0, sizeof(local_eid_));
    memset(&dest_eid_,  0, sizeof(dest_eid_));
}

//----------------------------------------------------------------------
void
DTNTunnel::fill_options()
{
    App::fill_default_options(DAEMONIZE_OPT);
    App::set_extra_usage("<destination_eid|destination eid table file name>");
    
    opts_.addopt(
        new oasys::BoolOpt('L', "listen", &listen_,
                           "run in listen mode for incoming CONN bundles"));
    
    opts_.addopt(
        new oasys::BoolOpt('c', "custody", &custody_, "use custody transfer"));
    
    opts_.addopt(
        new oasys::UIntOpt('e', "expiration", &expiration_, "<secs>",
                           "expiration time"));
    
    opts_.addopt(
        new oasys::BoolOpt('t', "tcp", &tcp_,
                           "proxy for TCP connections (default)"));
    
    opts_.addopt(
        new oasys::BoolOpt('u', "udp", &udp_,
                           "proxy for UDP traffic instead of tcp"));
    
    opts_.addopt(
        new dtn::APIEndpointIDOpt("local_eid", &local_eid_, "<eid>",
                                  "local endpoint id override"));

    opts_.addopt(
        new oasys::UIntOpt('D', "delay", &delay_, "<millisecs>",
                           "nagle delay in msecs for stream transports (e.g. tcp)",
                           &delay_set_));
    
    opts_.addopt(
        new oasys::UIntOpt('z', "max_size", &max_size_, "<bytes>",
                           "maximum bundle size for stream transports (e.g. tcp)"));

    opts_.addopt(
        new oasys::StringOpt('T', "tunnel", &tunnel_spec_, "<spec>",
                             "tunnel specification [lhost:]lport:rhost:rport",
                             &tunnel_spec_set_));

    opts_.addopt(
        new oasys::BoolOpt('r', "reorder_udp", &reorder_udp_,
                           "udp bundles received out of order will buffer until in-order deliver can be made"));
#ifdef __linux__
    opts_.addopt(
        new oasys::BoolOpt('p', "transparent_proxy", &transparent_, 
    			    "proxy application traffic transparently"));
#endif
}

//----------------------------------------------------------------------
void
DTNTunnel::validate_options(int argc, char* const argv[], int remainder)
{
    if (udp_) {
        tcp_ = false;
    }
    
    bool dest_eid_set = false;
    if (remainder == argc - 1) {
	// check if a destination eid table file name is given
	char token[4] = "://";
	if (strstr(argv[argc-1], token) != NULL) {
	    // have "://" in the arg => got a eid string
            if (dtn_parse_eid_string(&dest_eid_, argv[argc-1]) == -1) {
	        fprintf(stderr, "invalid destination endpoint id '%s'\n", 
			argv[argc - 1]);
        	print_usage_and_exit();
    	    } else {
        	dest_eid_set = true;
    	    }
	} else {
	    // no "://" in the arg => got a file path
	    load_dest_eid_table(argv[argc-1]);
	    dest_eid_set = true;
	}
    } else if (remainder != argc) {
        fprintf(stderr, "invalid argument '%s'\n", argv[remainder]);
        print_usage_and_exit();
    }

    // parse the tunnel spec
    if (tunnel_spec_set_) {
        std::vector<std::string> tokens;
        int ntoks = oasys::tokenize(tunnel_spec_, ":", &tokens);
        if (ntoks < 3 || ntoks > 4) {
            fprintf(stderr, "invalid tunnel specification %s: bad format\n",
                    tunnel_spec_.c_str());
            print_usage_and_exit();
        }

        if (ntoks == 4) {
            if (oasys::gethostbyname(tokens[0].c_str(), &local_addr_) != 0) {
                fprintf(stderr,
                        "invalid tunnel specification %s: local addr %s invalid\n",
                        tunnel_spec_.c_str(), tokens[0].c_str());
                print_usage_and_exit();
            }
        }
    
        local_port_ = atoi(tokens[ntoks - 3].c_str());

        if (oasys::gethostbyname(tokens[ntoks - 2].c_str(), &remote_addr_) != 0) {
            fprintf(stderr,
                    "invalid tunnel specification %s: remote host %s invalid\n",
                    tunnel_spec_.c_str(), tokens[ntoks - 2].c_str());
            print_usage_and_exit();
        }
    
        remote_port_ = atoi(tokens[ntoks - 1].c_str());
    }

#define CHECK_OPT(_condition, _err)             \
    if ((_condition)) {                         \
        fprintf(stderr, "error: " _err "\n");   \
        print_usage_and_exit();                 \
    }

    //
    // TODO?:  couldn't we use these in listen mode to over-ride
    // what is provided at the sender? -kfall.  Also: mcast for udp?
    //
    if (listen_) {
        CHECK_OPT(dest_eid_set, "setting destination eid is "
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
        CHECK_OPT(remote_port_ == 0,  "must set remote port");
	if (!dest_eid_table_.empty()) {
	    CHECK_OPT(transparent_ == false,
                  "destination eid table is supported "
		  "only in transparent_proxy mode");
	}
    }
    
#undef CHECK_OPT
}

//----------------------------------------------------------------------
void
DTNTunnel::load_dest_eid_table(char* filename)
{
    FILE* ifile;
    char line[512];

    ifile = fopen(filename, "r");
    if (ifile == NULL) {
	fprintf(stderr, "error opening destination eid table file '%s' for reading\n", filename);
        print_usage_and_exit();
    }

    int lnum = 0;
    while(!feof(ifile)) {
	if (fgets(line, sizeof(line), ifile) != NULL) {
	    int a, b, c, d, bits;
	    char* eid_str;
	    char addr_str[16];
	    u_int32_t addr;
	    dtn_endpoint_id_t* eid = new dtn_endpoint_id_t();

	    lnum++;
	    eid_str = (char*)calloc(DTN_MAX_ENDPOINT_ID, sizeof(char));
	    int n = sscanf(line, "%d.%d.%d.%d/%d %s", &a, &b, &c, &d, &bits, eid_str);
	    if (n < 6) {
    		if (errno != 0) {
		    fprintf(stderr, "error parsing destination eid table file at line %d: %s\n", 
			    lnum, strerror(errno));
		    exit(1);
		} else {
		    fprintf(stderr, "error parsing destination eid table file at line %d\n", lnum);
		    exit(1);
	    	}
	    }
    	    // fprintf(stderr, "DEBUG: %d.%d.%d.%d/%d %s\n", a, b, c, d, bits, eid_str);

	    sprintf(addr_str, "%d.%d.%d.%d", a, b, c, d);
	    addr = inet_addr(addr_str);
	    if (addr == INADDR_NONE) {
		fprintf(stderr, "error parsing network address in destination eid table at line %d\n", lnum);
		exit(1);
	    }

	    if ((bits < 0) || (bits > 32)) {
		fprintf(stderr, "error parsing network mask in destination eid table at line %d\n", lnum);
		exit(1);
	    }

    	    if (dtn_parse_eid_string(eid, eid_str) == -1) {
		fprintf(stderr, "invalid endpoint id in destination eid table '%s' at line %d\n", 
			eid_str, lnum);
    		exit(1);
    	    }

            NetworkEidTable::iterator i;
            CIDR key(addr, bits);

            i = dest_eid_table_.find(key);
	    if (i != dest_eid_table_.end()) {
    	        fprintf(stderr, "duplicate network address in destination eid table %s/%d at line %d\n", 
			addr_str, bits, lnum);
    		exit(1);
    	    }

            dest_eid_table_[key] = eid_str;
	}
    }
    fclose(ifile);
}

//----------------------------------------------------------------------
void
DTNTunnel::get_dest_eid(in_addr_t remote_addr, dtn_endpoint_id_t* dest_eid)
{
    // if we don't have dest_eid_table, return
    if (dest_eid_table_.empty())
        return;

    memset(dest_eid->uri, 0, sizeof(dest_eid->uri));
    for (NetworkEidTable::iterator i = dest_eid_table_.begin();
         i != dest_eid_table_.end(); i++)
    {
        CIDR net = i->first;
        u_int32_t mask = ~(~u_int32_t(0) << net.bits_);
        //log_debug("remote_addr=%s, addr=%s, bits=%d, mask=%s",
        //        intoa(remote_addr), intoa(net.addr_), net.bits_, intoa(mask));
        if ((remote_addr & mask) == (net.addr_ & mask)) {
	    memcpy(dest_eid->uri, dest_eid_table_[i->first], DTN_MAX_ENDPOINT_ID);
            log_debug("found dest_eid=%s for address=%s", 
		      dest_eid->uri, intoa(remote_addr));
            break;
        }
    }
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
        notify_and_exit(1);
    }
    
    err = dtn_open(&send_handle_);
    if (err != DTN_SUCCESS) {
        log_crit("can't open send handle to daemon: %s",
                 dtn_strerror(err));
        notify_and_exit(1);
    }

    oasys::StaticStringBuffer<128> demux;
    if (listen_) {
        demux.appendf("dtntunnel");
    } else {
        demux.appendf("dtntunnel?dest_eid=%s&tunnel=%s-%s:%u-%s:%u",
                      dest_eid_.uri,
                      tcp_ ? "tcp" : "udp",
                      intoa(local_addr_),
                      local_port_,
                      intoa(remote_addr_),
                      remote_port_);
    }
    
    if (local_eid_.uri[0] == '\0') {
        err = dtn_build_local_eid(recv_handle_, &local_eid_, demux.c_str());
        if (err != DTN_SUCCESS) {
            log_crit("can't build local eid: %s",
                     dtn_strerror(dtn_errno(recv_handle_)));
            notify_and_exit(1);
        }
    }

    log_notice("local endpoint id %s", local_eid_.uri);

    u_int32_t regid;
    err = dtn_find_registration(recv_handle_, &local_eid_, &regid);
    if (err == 0) {
        log_notice("found existing registration id %d, calling dtn_bind",
                   regid);
        
        err = dtn_bind(recv_handle_, regid);
        if (err != 0) {
            log_crit("error in dtn_bind: %s", 
                     dtn_strerror(dtn_errno(recv_handle_)));
            notify_and_exit(1);
        }
    } else if (dtn_errno(recv_handle_) == DTN_ENOTFOUND) {
        dtn_reg_info_t reginfo;
        memset(&reginfo, 0, sizeof(reginfo));
        dtn_copy_eid(&reginfo.endpoint, &local_eid_);
        reginfo.flags = DTN_REG_DEFER;
        reginfo.expiration = 60 * 60 * 24; // 1 day

        err = dtn_register(recv_handle_, &reginfo, &regid);
        if (err != 0) {
            log_crit("error in dtn_register: %s",
                     dtn_strerror(dtn_errno(recv_handle_)));
            notify_and_exit(1);
        }
    } else {
        log_crit("error in dtn_find_registration: %s",
                 dtn_strerror(dtn_errno(recv_handle_)));
        notify_and_exit(1);
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
    if (custody_) {
        spec.dopts |= DOPTS_CUSTODY;
    }
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

    err = dtn_send(send_handle_, DTN_REG_DEFER, &spec, &payload, &bundle_id);

    if (err != 0) {
        return dtn_errno(send_handle_);
    }
    
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

    int len = payload->buf.buf_len;

    if (len < (int)sizeof(BundleHeader)) {
        log_err("too short bundle: len %d < sizeof bundle header", len);
        delete b;
        return -1;
    }
    
    char* dst = b->payload_.buf(len);
    memcpy(dst, payload->buf.buf_val, len);
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
    init_app(argc, argv);

    log_notice("DTNTunnel starting up...");

    init_tunnel();
    init_registration();

    // if we've daemonized, now is the time to notify our parent
    // process that we've successfully initialized
    if (daemonize_) {
        daemonizer_.notify_parent(0);
    }

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

        
        log_info("got %d byte bundle", payload.buf.buf_len);

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
