/*
 *    Copyright 2005-2006 University of Waterloo
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
                             const dtn::TcaControlBundle& cb);
    bool route_reg(const dtn_bundle_spec_t& spec, const dtn::TcaControlBundle& cb);
    bool gate_reg(const dtn_bundle_spec_t& spec, const dtn::TcaControlBundle& cb);

    bool handle_unb(const dtn_bundle_spec_t& spec, const dtn::TcaControlBundle& cb);

    bool handle_coa_sent(const dtn_bundle_spec_t& spec,
                         const dtn::TcaControlBundle& cb);

    bool handle_link_announce(const dtn_bundle_spec_t& spec,
                              const dtn::TcaControlBundle& cb);

    bool handle_ask(const dtn_bundle_spec_t& spec, const dtn::TcaControlBundle& cb);

    bool handle_ask_received(const dtn_bundle_spec_t& spec,
                             const dtn::TcaControlBundle& cb);

    bool handle_ask_sent(const dtn_bundle_spec_t& spec,
                         const dtn::TcaControlBundle& cb);

    bool handle_adv(const dtn_bundle_spec_t& spec, const dtn::TcaControlBundle& cb);

    bool handle_adv_sent(const dtn_bundle_spec_t& spec,
                         const dtn::TcaControlBundle& cb);

    bool handle_routes(const dtn_bundle_spec_t& spec,
                       const dtn::TcaControlBundle& cb);

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



