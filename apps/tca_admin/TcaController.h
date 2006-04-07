/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * University of Waterloo Open Source License
 * 
 * Copyright (c) 2005 University of Waterloo. All rights reserved. 
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
 *   Neither the name of the University of Waterloo nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY
 * OF WATERLOO OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

//#include <stdio.h>
//#include <unistd.h>
//#include <errno.h>
//#include <strings.h>
//#include <stdlib.h>
//#include <sys/time.h>

#include <string>
//#include "dtn_api.h"
#include "TcaEndpointID.h"
#include "../../servlib/routing/TcaControlBundle.h"
#include "TcaRegistry.h"


class TcaController
{
public:

    enum Role { TCA_MOBILE, TCA_ROUTER, TCA_GATEWAY };
        
    TcaController(Role role, const std::string& link_id,
                  const std::string& ask_addr, const std::string& adv_str,
                  int registry_ttl, int control_ttl);

    virtual ~TcaController();

    // open dtn, initialize class members
    bool init(bool tidy);   

    // process incoming bundles
    void run();

    // low-level send/recv functions
    bool send_bundle(const dtn_bundle_spec_t& spec, const std::string& payload);
    bool recv_bundle(dtn_bundle_spec_t& spec, std::string& payload,
                     unsigned int timeout=0);

    // a more convenient send, using reasonable defaults for most args
    bool send_bundle(const std::string& dest, const std::string& payload);

protected:
    
    Role                    role_;
    std::string             link_id_;
    std::string             ask_addr_;      // address to send ask to
    std::string             adv_str_;
    int                     registry_ttl_;
    int                     control_ttl_;

    TcaRegistry             registry_;      // the DHT registry (gateway only)
    dtn_handle_t            handle_;
    dtn_endpoint_id_t       local_eid_;

    // register an endpoint id with the daemon and bind it to handle_
    bool dtn_reg(dtn_endpoint_id_t& eid, dtn_reg_id_t& id);

    bool handle_bundle_received(const dtn_bundle_spec_t& spec,
                                const std::string& payload);

    bool handle_reg_received(const dtn_bundle_spec_t& spec,
                             const TcaControlBundle& cb);
    bool route_reg(const dtn_bundle_spec_t& spec, const TcaControlBundle& cb);
    bool gate_reg(const dtn_bundle_spec_t& spec, const TcaControlBundle& cb);

    bool handle_unb(const dtn_bundle_spec_t& spec, const TcaControlBundle& cb);

    bool handle_coa_sent(const dtn_bundle_spec_t& spec,
                         const TcaControlBundle& cb);

    bool handle_link_announce(const dtn_bundle_spec_t& spec,
                              const TcaControlBundle& cb);

    bool handle_ask(const dtn_bundle_spec_t& spec, const TcaControlBundle& cb);

    bool handle_ask_received(const dtn_bundle_spec_t& spec,
                             const TcaControlBundle& cb);

    bool handle_ask_sent(const dtn_bundle_spec_t& spec,
                         const TcaControlBundle& cb);

    bool handle_adv(const dtn_bundle_spec_t& spec, const TcaControlBundle& cb);

    bool handle_adv_sent(const dtn_bundle_spec_t& spec,
                         const TcaControlBundle& cb);

    bool handle_routes(const dtn_bundle_spec_t& spec,
                       const TcaControlBundle& cb);

    bool ask(const std::string& link);  // experimental
    bool get_routes();
    bool add_route(const std::string& route_pattern, const std::string& link);
    bool del_route(const std::string& route_pattern);


    // lookup existing registration info for the given endpoint
    bool get_registration(const TcaEndpointID& eid, RegRecord& rr);

    // update DHT registry for given endpoint
    // link_addr is the link addr of the endpoint's new gateway
    bool do_registration(const TcaEndpointID& eid,
                         const std::string& link_addr);

    // Testing functions for various parts of the protocol
    // These generally send a query bundle, get the response, and check
    // that it's correct.

    bool test_all();
    
    // recv and discard all pending bundles
    void eat_bundles(bool verbose = true);
};



