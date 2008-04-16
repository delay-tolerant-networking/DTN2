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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

//#include <stdio.h>
#include <unistd.h>
#include <errno.h>
//#include <strings.h>
//#include <stdlib.h>
//#include <sys/time.h>

//#include <string>
#include "dtn_api.h"
#include "TcaController.h"


static const int debug = 1;


// RECV_TIMEOUT can be made very large. It's just used as a timeout for a
// blocking function call to receive bundle.
static const int RECV_TIMEOUT = 30000;  // 30s


// REG_EXPIRATION_TIME is how long the registration lasts. We want it to
// last forever, but we're up against maxint.
// The value specified here is multiplied by 1000 in class Registration
// to get milliseconds, and I think that deeper in the actual timer code it's
// converted to a signed int, so 2000000 (23 days) is about as high as we
// can go here.
static const u_int32_t REG_EXPIRATION_TIME = 2000000; 



// Fake spec constructor. For conveniently building a dtn_bundle_spec_t

bool
make_spec(dtn_bundle_spec_t& spec,
          std::string source,
          std::string dest,
          std::string replyto,
          int expiration,
          dtn_bundle_priority_t priority = COS_NORMAL,
          dtn_bundle_delivery_opts_t dopts = DOPTS_NONE
          )
{
    memset(&spec, 0, sizeof(spec));

    if (dtn_parse_eid_string(&spec.source, source.c_str()))
    {
        fprintf(stderr, "make_spec: invalid source eid '%s'\n",
                    source.c_str());
        return false;
    }

    if (dtn_parse_eid_string(&spec.dest, dest.c_str()))
    {
        fprintf(stderr, "make_spec: invalid dest eid '%s'\n",
                    dest.c_str());
        return false;
    }

    if (dtn_parse_eid_string(&spec.replyto, replyto.c_str()))
    {
        fprintf(stderr, "make_spec: invalid replyto eid '%s'\n",
                    replyto.c_str());
        return false;
    }

    spec.priority = priority;
    spec.dopts = dopts;
    spec.expiration = expiration;

    return true;
}


static bool
check_nargs(const dtn::TcaControlBundle& cb, uint n_expected)
{
    if (cb.args_.size() != n_expected)
    {
        printf("TcaController: bundle '%s' contains wrong number of args. "
                "%d expected.\n", cb.str().c_str(), n_expected);
        return false;
    }
    return true;
}


TcaController::TcaController(TcaController::Role role,
                             const std::string& link_id,
                             const std::string& ask_addr,
                             const std::string& adv_str,
                             int registry_ttl, int control_ttl)
    : role_(role), link_id_(link_id),
      ask_addr_(ask_addr), adv_str_(adv_str),
      registry_ttl_(registry_ttl), control_ttl_(control_ttl)

{

}


TcaController::~TcaController()
{
    dtn_close(handle_);
}


bool
TcaController::dtn_reg(dtn_endpoint_id_t& eid, dtn_reg_id_t& id)
{   
    // register eid for specified app, eg. "tca://hail/admin"

    // create a new registration
    dtn_reg_info_t reginfo;
    // dtn_reg_id_t regid;
    int ret;

    memset(&reginfo, 0, sizeof(reginfo));
    dtn_copy_eid(&reginfo.endpoint, &eid);
    reginfo.flags = DTN_REG_DEFER;
    reginfo.regid = DTN_REGID_NONE;
    reginfo.expiration = REG_EXPIRATION_TIME;
    if ((ret = dtn_register(handle_, &reginfo, &id)) != 0) {
        fprintf(stderr, "error creating registration: %d (%s)\n",
                ret, dtn_strerror(dtn_errno(handle_)));
        return false;
    }    
    // if (debug) printf("dtn_register succeeded, id 0x%x\n", id);
    
    printf("TcaController::dtn_reg: app registered as %s, id=0x%x\n",
            eid.uri, id);

    return true;
}


bool
TcaController::init(bool tidy)
{
    // open the ipc handle
    int err = dtn_open(&handle_);
    if (err != DTN_SUCCESS) {
        fprintf(stderr, "fatal error opening dtn handle: %s\n",
                dtn_strerror(err));
        return false;
    }

    printf("TcaController::init: dtn_open succeeded\n");

    // build local eid and register it:
    dtn_reg_id_t app_id;
    dtn_build_local_eid(handle_, &local_eid_, "/admin");
    if (!dtn_reg(local_eid_, app_id)) return false;

    /*
    // test: register another eid and make sure that works
    dtn_reg_id_t test_id;
    dtn_endpoint_id_t test_eid;
    dtn_build_local_eid(handle_, &test_eid, "/testing123");
    if (!dtn_reg(test_eid, test_id)) return false;
    */

    // discard any pending bundles iff 'tidy' option specified
    if (tidy) eat_bundles();

    if (role_ == TCA_GATEWAY)
    {
        // now initialize the list of live registry nodes
        registry_.init_nodes();
        if (!registry_.init_addrs())
        {
            // This is an unrecoverable failure condition -- no dht nodes
            // available. The gateway will not be able to do its job,
            // so we might as well exit().
            fprintf(stderr,
                "TcaController fatal error: no registry nodes available!\n");
            return false;
        }
    }

    return true;
}


void
TcaController::run()
{

    if (ask_addr_.length() != 0)
    {
        // user has specified an ask address. Send out an ask bundle.
        ask(ask_addr_);
    }
   
    dtn_bundle_spec_t spec;
    std::string payload;

    for (;;)
    {
        if (recv_bundle(spec, payload, RECV_TIMEOUT))
        {
            handle_bundle_received(spec, payload);
        }
    }
}


bool
TcaController::handle_bundle_received(const dtn_bundle_spec_t& spec,
                                      const std::string& payload)
{
    dtn::TcaControlBundle cb(payload);

    // handle control bundles:
    switch (cb.type_)
    {
        case dtn::TcaControlBundle::CB_LINK_ANNOUNCE:
            handle_link_announce(spec, cb);
            break;
        case dtn::TcaControlBundle::CB_ASK:
            handle_ask(spec, cb);
            break;
        case dtn::TcaControlBundle::CB_ASK_RECEIVED:
            handle_ask_received(spec, cb);
            break;
        case dtn::TcaControlBundle::CB_ASK_SENT:
            handle_ask_sent(spec, cb);
            break;
        case dtn::TcaControlBundle::CB_ADV:
            handle_adv(spec, cb);
            break;
        case dtn::TcaControlBundle::CB_ADV_SENT:
            handle_adv_sent(spec, cb);
            break;
        case dtn::TcaControlBundle::CB_REG_RECEIVED:
            handle_reg_received(spec, cb);
            break;
        case dtn::TcaControlBundle::CB_UNB:
            handle_unb(spec, cb);
            break;
        case dtn::TcaControlBundle::CB_COA:
            // all of the real coa logic is in the bundle_tranmitted handler
            // in bundle layer
            break;
        case dtn::TcaControlBundle::CB_COA_SENT:
            handle_coa_sent(spec, cb);
            break;
        case dtn::TcaControlBundle::CB_ROUTES:
            handle_routes(spec, cb);
            break;
        case dtn::TcaControlBundle::CB_LINK_AVAILABLE:
            break;
        case dtn::TcaControlBundle::CB_LINK_UNAVAILABLE:
            break;
        case dtn::TcaControlBundle::CB_CONTACT_UP:
            break;
        case dtn::TcaControlBundle::CB_CONTACT_DOWN:
            break;
        default:
            printf("TcaController: unrecognized bundle code received: '%s'\n",
                cb.code_.c_str());
    }

    return true;
}



bool
TcaController::handle_reg_received(const dtn_bundle_spec_t& spec,
                                   const dtn::TcaControlBundle& cb)
{
    switch (role_)
    {
        case TCA_MOBILE:
            // TODO: This isn't an error. Just add the link addr and forward
            // the bundle to default route. Exactly the same logic as
            // for a router.
            return route_reg(spec, cb);
            break;
        case TCA_ROUTER:
            return route_reg(spec, cb);
            break;
        case TCA_GATEWAY:
            return gate_reg(spec, cb);
            break;
    }   

    return false;
}


bool
TcaController::handle_unb(const dtn_bundle_spec_t& spec,
                          const dtn::TcaControlBundle& cb)
{
    (void)spec;
    
    if (role_ != TCA_GATEWAY)
    {
        printf("TcaController error: non-gateway received unb bundle.\n");
        return false;
    }

    TcaEndpointID dest_eid(cb.args_[0]);
    RegRecord rr;

    if (!get_registration(dest_eid, rr))
    {
        printf("TcaController: bind failed: unregistered node [%s]\n",
            dest_eid.c_str());
        // TODO: What should happen here?
        // dest may never have registered, or may have previously registered
        // but expired. Depending how we want to handle registration
        // lifetime, we may need to re-try this get_registration later.
        return false;
    }

    // Found it! Add a route.
    // Note: There's no point deleting old routes for this endpoint. We
    // got here because no old routes exist!

    if (!add_route(dest_eid.str(), rr.link_addr_)) return false;

    return true;
}


bool
TcaController::handle_coa_sent(const dtn_bundle_spec_t& spec,
                               const dtn::TcaControlBundle& cb)
{
    (void)spec;
    
    if (role_ == TCA_MOBILE)
    {
        printf("TcaController error: mobile received coa_sent bundle.\n");
        return false;
    }

    // TODO: update this to use ControlBundle class
    const std::string& src = cb.args_[0];   // coa src (gateway)
    const std::string& dest = cb.args_[1];  // coa dest (mobile)
    const std::string& link = cb.args_[2];  // last hop to mobile

    // if we're a router or a gateway, delete routes to the bundle dest:
    TcaEndpointID pattern = dest;
    pattern.set_app("*");
    del_route(pattern.str());
    
    // if the coa bundle originated at *this* gateway node then we have to
    // update the registry and finally add the route back to the 
    // registering mobile
    if (src == local_eid_.uri)
    {
        TcaEndpointID tca_dest = dest;
        if (!do_registration(tca_dest, link_id_)) return false;

        tca_dest.set_app("*");
        if (!add_route(tca_dest.str(), link)) return false;
    }

    return true;
}


bool
TcaController::handle_link_announce(const dtn_bundle_spec_t& spec,
                                    const dtn::TcaControlBundle& cb)
{
    (void)spec;
            
    // Note: the port number is omitted from the link_announce bundle.
    // It is up to TcaController to decide which port to probe.
    if (!check_nargs(cb, 1)) return false;
    std::string link_spec = cb.args_[0];
    link_spec += ":5000";
    return ask(link_spec);
}


bool
TcaController::handle_ask(const dtn_bundle_spec_t& spec,
                          const dtn::TcaControlBundle& cb)
{
    (void)spec;
    (void)cb;
    printf("error -- we should never receive an ask bundle!\n");

    return true;
}


bool
TcaController::handle_ask_received(const dtn_bundle_spec_t& spec,
                                   const dtn::TcaControlBundle& cb)
{
    (void)spec;
    dtn::TcaWrappedBundle wb(cb);

    // respond by adding a route and sending adv

    TcaEndpointID src_eid = wb.source();
    src_eid.set_app("*");

    if (!add_route(src_eid.str(), wb.args_[2])) return false;
    
    // respond with adv
    // note that adv is a regular bundle addressed to asking node eid
    // (not an inter-layer WrappedBundle)
    std::string response = "adv:";
    response += wb.dest();  // echo original ask dest
    response += "\t";
    response += adv_str_;       // send adv string
    send_bundle(wb.source(), response);
    
    return true;
}


bool
TcaController::handle_ask_sent(const dtn_bundle_spec_t& spec,
                               const dtn::TcaControlBundle& cb)
{
    (void)spec;
    dtn::TcaWrappedBundle wb(cb);

    // respond by deleting the route
    TcaEndpointID dest_eid = wb.dest(); // dest of original ask
    dest_eid.set_app("*");

    if (!del_route(dest_eid.str())) return false;
    return true;
}


bool
TcaController::handle_adv(const dtn_bundle_spec_t& spec,
                          const dtn::TcaControlBundle& cb)
{
    (void)spec;
    (void)cb;
    // TODO: This is the place to be smart and try to decide on a
    // router to use for the default route.
    // For now, do nothing.
    return true;
}


bool
TcaController::handle_adv_sent(const dtn_bundle_spec_t& spec,
                               const dtn::TcaControlBundle& cb)
{
    (void)spec;
    dtn::TcaWrappedBundle wb(cb);

    // respond by deleting the route
    // TODO: This is actually more complicated! We should only delete the
    // route if it was created by the ask bundle that spawned this adv
    // (ie. if it was a pre-existing route, we should keep it!)
    // Best idea on how to do that is to use the route table to store
    // extra info on creating the entry, indicating it's a temp route.
    TcaEndpointID dest_eid = wb.dest(); // dest of original ask
    dest_eid.set_app("*");

    if (!del_route(dest_eid.str())) return false;
    return true;
}


bool
TcaController::handle_routes(const dtn_bundle_spec_t& spec,
                             const dtn::TcaControlBundle& cb)
{
    (void)spec;

    printf("routes:\n");
    for (unsigned int i=0; i<cb.args_.size(); ++i)
    {
        printf("    %s \n", cb.args_[i].c_str());
    }
    return true;
}


bool
TcaController::route_reg(const dtn_bundle_spec_t& spec,
                         const dtn::TcaControlBundle& cb)
{
    (void)spec;
    // A (small) problem is that we will get 2 links for a node that
    // already has an link defined explicitly in the dtn.conf file.
    // Another link will be auto-created with a different name, same params.
    // We could be smarter and check for this.

    if (!check_nargs(cb, 4)) return false;

    dtn::TcaWrappedBundle wb(cb);

    std::string mobile_eid = wb.args_[2];
    std::string last_hop = wb.args_[3];

    if (last_hop != "NULL")
    {
        // The register bundle did not originate at this node.
        // Add a link and route back to mobile.

        mobile_eid = wb.args_[2];
        last_hop = wb.args_[3];

        TcaEndpointID pattern(mobile_eid);

        // Delete the old route to mobile.
        // This is the correct behaviour because there should not
        // be any static routes for a tca mobile, only the one obtained
        // through the previous registration, and that one's now invalid.
        pattern.set_app("*");
        printf("deleteing routes for %s\n", pattern.str().c_str());
        if (!del_route(pattern.str())) return false;

        // Create the new route to mobile.
        if (!add_route(pattern.str(), last_hop)) return false;
    }

    // Now propagate the registry bundle (actually send a new one with
    // last_hop set to local node).
    // Skip this step if we're a gateway.
    
    if (role_ != TCA_GATEWAY)
    {
        std::string new_payload = "register:";
        new_payload += mobile_eid;      // original mobile id
        new_payload += "\t";
        new_payload += link_id_;        // link id of this router (last hop)

        if (!send_bundle("tca://registry", new_payload)) return false;
    }

    return true;
}


bool
TcaController::gate_reg(const dtn_bundle_spec_t& spec,
                        const dtn::TcaControlBundle& cb)
{
    (void)spec;
    dtn::TcaWrappedBundle wb(cb);

    std::string mobile_eid = wb.args_[2];
    std::string last_hop = wb.args_[3];

    TcaEndpointID src(mobile_eid);

    // First, get the old gateway for the source endpoint. We'll need it later.
    // It's ok if it doesn't exist -- then this is a new registration.
    // printf("TcaController: reading old registration...\n");
    RegRecord old_reg;
    bool prev_reg_exists = get_registration(src, old_reg);

    if (prev_reg_exists) 
    {
        // if (old_gateway == me) don't bother re-registering
        if (old_reg.link_addr_ == link_id_)
        {
            printf("TcaController: ignoring re-registration with same"
                    " gateway\n");

            // TODO: We still need to destruct old reverse path, even
            // if the mobile has re-registerd in the same gateway.
            // In this case, only part of the route is different.
            // Probably each router should check for this case on
            // handling the reg bundle -- if duplicate for same endpoint
            // destruct old route and construct new one. Don't even propagate
            // the reg. up to the gateway.
            return true;
        }
        else
        {
            printf("TcaController: initiating change-of-address\n");

            // There is a previous registration with another gateway.
            // We need to send a coa bundle to that gateway. Don't re-register
            // until this is done (handle_bundle_transmitted).

            // First we need to create a route to the old gateway,
            // as in gate_unbound_bundle.

            // Construct a route pattern based on the source eid.
            // Note: we use the special application "admin.coa" to ensure
            // that this route is only used for the coa bundle.
            TcaEndpointID pattern = src;
            pattern.set_app("admin.coa");

            printf("TcaController: adding route %s -> %s\n",
                    pattern.c_str(), old_reg.link_addr_.c_str());

            if (!add_route(pattern.str(), old_reg.link_addr_)) return false;


            TcaEndpointID dest_eid = src;
            dest_eid.set_app("admin.coa");

            // Construct coa bundle.
            // source:  this gateway
            // dest:    original mobile with special app "admin.coa"

            // This is a bit of a kludge...
            // The last thing done by the gateway after issuing the coa
            // bundle and updating the registry, is to create the route
            // back to the mobile (just like during routing of a reg bundle)
            // We'll need the last_hop link to do that. So we put it in
            // the body of the coa bundle, to be used on this very same node
            // on bundle transmitted.

            std::string coa_payload = "coa:";
            coa_payload += last_hop;

            dtn_bundle_spec_t coa_spec;
            if (!make_spec(coa_spec, local_eid_.uri, dest_eid.str(),
                    local_eid_.uri, control_ttl_, COS_NORMAL, DOPTS_NONE))
                return false;

            // We need to set non-zero expiration or else the bundle
            // expires as soon as it arrives.
            // The default values are ok for the rest of the bundle fields.

            if (!send_bundle(coa_spec, coa_payload)) return false;

            // Note: I thought we might have a problem here. The send_bundle
            // will cause a BundleReceivedEvent in the daemon, but it's ok
            // because the BL checks for a null last-hop, indicating that
            // the bundle came from this node.
        }

        // Note: We do *not* update the link and route just yet.
        // Do it on handle_bundle_transmitted instead, at which point
        // we should delete the temporary admin.coa route (to the mobile's
        // old location and add a new route to the mobile's new location.
    }

    else
    {
        // No previous registration so no change-of-address required.
        // Go ahead and register.
        // update the registry entry for the source endpoint
        // printf("TcaController: doing new registration...\n");
        if (!do_registration(src, link_id_)) return false;

        // update local link and route
        if (!route_reg(spec, cb)) return false;
    }

    return true;

}


bool
TcaController::ask(const std::string& link)
{
    // "ask" on an unknown link
    // Idea: create temporary route and send ask bundle
    // on handling the corresponding adv, delete the route
    

    std::string host = "tca://anonymous";
    // omit prefix "tcp://" due to slashes:
    host += link.substr(6, link.length());

    std::string pattern = host + "/*";

    if (!add_route(pattern, link)) return false;

    std::string payload = "ask:";
    payload += link_id_;            // our own link, for return route

    std::string dest = host + "/admin.ask";

    if (!send_bundle(dest, payload))
    {
        fprintf(stderr,
                "TcaController::ask: error: failed to send ask bundle\n");
        return false;
    }

    return true;
}


bool
TcaController::get_routes()
{
    if (!send_bundle("tca://localhost/bundlelayer", "get_routes:tca://*"))
    {
        fprintf(stderr, "error: failed to send get_routes bundle\n");
        return false;
    }
   
    return true;
}


bool
TcaController::add_route(const std::string& route_pattern,
                         const std::string& link)
{
    std::string body = "add_route:";
    body += route_pattern;
    body += "\t";
    body += link;

    // testing -- try to add a route
    if (!send_bundle("tca://localhost/bundlelayer", body))
    {
        fprintf(stderr, "error: failed to send add_route bundle\n");
        return false;
    }

    return true;
}


bool
TcaController::del_route(const std::string& route_pattern)
{
    std::string body = "del_route:";
    body += route_pattern;

    if (!send_bundle("tca://localhost/bundlelayer", body))
    {
        fprintf(stderr, "error: failed to send del_route bundle\n");
        return false;
    }
   
    return true;
}


bool
TcaController::get_registration(const TcaEndpointID& eid, RegRecord& rr)
{
    rr.host_ = eid.get_hostid();
    if (registry_.read(rr))
    {
        printf("TcaController: found registry entry %s\n", rr.str().c_str());
        return true;
    }
    else
    {
        printf("TcaController: no registry entry for host %s\n",
                rr.host_.c_str());
        return false;
    }
}


// TODO: Make this async, with periodic refresh
bool
TcaController::do_registration(const TcaEndpointID& eid,
                               const std::string& link_addr)
{
    RegRecord rr(eid.get_hostid(), link_addr);

    if (registry_.write(rr, registry_ttl_))
    {
        printf("TcaController: wrote registry entry %s\n", rr.str().c_str());
        return true;
    }
    else
    {
        printf("TcaController: failed to write registry entry %s\n",
                    rr.str().c_str());
        return false;
    }

}


bool
TcaController::test_all()
{
    get_routes();

    add_route("tca://booyah", "tcp://9.8.7.6:54321");
    sleep(5);

    get_routes();

    del_route("tca://booyah");
    sleep(5);

    get_routes();
    
    return true;
}


void
TcaController::eat_bundles(bool verbose)
{
    // check for any bundles queued for this registration and discard them.

    dtn_bundle_spec_t recv_spec;
    std::string payload;

    printf("checking for queued bundles...\n");

    if (verbose)
    {
        while (recv_bundle(recv_spec, payload, 0))
            printf("    discarding bundle: %s\n", payload.c_str());
    }
    else
    {
        dtn_bundle_payload_t    recv_payload;
        int ret;
        do
        {
            memset(&recv_spec, 0, sizeof(recv_spec));
            memset(&recv_payload, 0, sizeof(recv_payload));
        
            ret = dtn_recv(handle_, &recv_spec,
                       DTN_PAYLOAD_MEM, &recv_payload, 0);

            if (ret == 0)
            {
                fprintf(stderr, "error: unexpected bundle already queued... "
                        "discarding\n");
            }
            else if (dtn_errno(handle_) != DTN_ETIMEOUT)
            {
                fprintf(stderr, "error: "
                    "unexpected error checking for queued bundles: %s\n",
                    dtn_strerror(dtn_errno(handle_)));
                return;
            }
        } while (ret == 0);
    }
}


bool
TcaController::send_bundle(const dtn_bundle_spec_t& spec,
                           const std::string& payload)
{
    printf("send_bundle: [%s] -> [%s] : '%s'\n",
                spec.source.uri, spec.dest.uri, payload.c_str());

    dtn_bundle_payload_t send_payload;
    memset(&send_payload, 0, sizeof(send_payload));
    dtn_set_payload(&send_payload, DTN_PAYLOAD_MEM,
            const_cast<char*>(payload.c_str()), payload.length());

    dtn_bundle_id_t bundle_id;
    memset(&bundle_id, 0, sizeof(bundle_id));

    int r = 0;    
    if ((r = dtn_send(handle_, DTN_REGID_NONE,
                      const_cast<dtn_bundle_spec_t*>(&spec),
                      &send_payload, &bundle_id)) != 0)
    {
        fprintf(stderr, "TcaController::send_bundle error %d (%s)\n",
                r, dtn_strerror(dtn_errno(handle_)));
        return false;
    }

    return true;
}


bool
TcaController::recv_bundle(dtn_bundle_spec_t& spec,
                           std::string& payload,
                           unsigned int timeout)
{   
    dtn_bundle_payload_t recv_payload;
    memset(&spec, 0, sizeof(spec));
    memset(&recv_payload, 0, sizeof(recv_payload));
        
    // now we block waiting for the echo reply
    // DK: original timeout was -1, causes compile warning with gcc
    int r;
    if ((r = dtn_recv(handle_, &spec,
                        DTN_PAYLOAD_MEM, &recv_payload, timeout)) < 0)
    {
        // fprintf(stderr, "TcaController::recv_bundle error %d (%s)\n",
        //        r, dtn_strerror(dtn_errno(handle_)));
        return false;
    }

    int n = recv_payload.buf.buf_len;
    char s_buf[n+1];
    memcpy(s_buf, recv_payload.buf.buf_val, n);
    s_buf[n] = '\0';

    payload = s_buf;

    printf("%d bytes from [%s]: %s\n",
               // recv_payload.buf.buf_len,
               n,
               spec.source.uri,
               payload.c_str());

    return true;
}


// a more convenient send, using reasonable defaults for most args
bool
TcaController::send_bundle(const std::string& dest,
                           const std::string& payload)
{
    // printf("send_bundle: [%s] -> [%s] : '%s'\n",
    //              local_eid_.uri, dest.c_str(), payload.c_str());

    dtn_bundle_spec_t spec;
    memset(&spec, 0, sizeof(spec));

    // set the source and reply_to to be the local eid
    dtn_copy_eid(&spec.source, &local_eid_);
    dtn_copy_eid(&spec.replyto, &local_eid_);

    if (dtn_parse_eid_string(&spec.dest, dest.c_str()))
    {
        fprintf(stderr, "TcaController::send_bundle: invalid destination"
                " eid string, %s\n", dest.c_str());
        return false;
    }

    // set the expiration time and the return receipt option
    spec.priority = COS_NORMAL;
    spec.dopts = DOPTS_NONE;
    spec.expiration = control_ttl_;

    return send_bundle(spec, payload);
}



