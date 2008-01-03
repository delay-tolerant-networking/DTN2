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

#include "conv_layers/ConvergenceLayer.h"
#include "bundling/BundleActions.h"
#include "contacts/ContactManager.h"
#include "bundling/BundleDaemon.h"
#include "routing/RouteTable.h"
#include  <oasys/io/NetUtils.h>
#include "contacts/InterfaceTable.h"
#include "conv_layers/TCPConvergenceLayer.h"

#include "TcaRouter.h"

namespace dtn {



// Consts

static const std::string BL = "tca://localhost/bundlelayer";


///////////////////////////////////////////////////////////////////////////////
// Functions


// get_payload_str is a convenient one-call function to get the payload
// of a simple bundle (must not contain any nulls)
static std::string
get_payload_str(const Bundle* b)
{
    size_t len = b->payload().length();
    u_char data[len+1];
    const u_char* p = b->payload().read_data(0, len, data);
    return (const char*)p;
}


// debug: check for expected number of args in control bundle, w/ message
static bool
check_nargs(const TcaControlBundle& cb, uint n_expected)
{
    if (cb.args_.size() != n_expected)
    {
        log_err_p("dtn/tca", "TcaRouter: bundle '%s' contains wrong number "
                  "of args. %d expected.", cb.str().c_str(), n_expected);
        return false;
    }
    return true;
}


// debug: log bundle wrapper (source -> dest) and optionally payload too
static void
log_bundle(const std::string& comment, const Bundle* b, bool include_payload)
{
    (void)comment;
    (void)b;
    
    if (include_payload)
        log_debug_p("/dtn/tca", "%s [%s] -> [%s] : '%s'", comment.c_str(),
                    b->source().str().c_str(), b->dest().c_str(),
                    get_payload_str(b).c_str());
    else
        log_debug_p("/dtn/tca", "%s [%s] -> [%s]", comment.c_str(),
                    b->source().str().c_str(), b->dest().c_str());
} 
    

static void
log_controlbundle(const TcaControlBundle& cb)
{
    log_debug_p("/dtn/tca", "    code='%s', args=%d",
                cb.code_.c_str(), (u_int)cb.args_.size());
    for (unsigned int i=0; i<cb.args_.size(); ++i)
    {
        log_debug_p("/dtn/tca", "        '%s'", cb.args_[i].c_str());
    }
}


/*
// get_tcp_interface gets the "tcp0" Interface object.
// No longer neeed.
static Interface* get_tcp_interface()
{
    InterfaceTable* p_iftab = InterfaceTable::instance();
    return p_iftab->find("tcp0");
}
*/

///////////////////////////////////////////////////////////////////////////////
// class TcaRouter

TcaRouter::TcaRouter(Role role)
    : TableBasedRouter("TcaRouter", "TcaRouter")
{
    role_ = role;

    // Construct the eid of the local admin app.
    admin_app_ = BundleDaemon::instance()->local_eid();
    admin_app_.set_app("admin");

    logpathf("/dtn/tca");

    log_info("TcaRouter started: role='%s', admin_app='%s'",
             get_role_str().c_str(), admin_app_.c_str());
}


std::string
TcaRouter::get_role_str() const
{
    switch (role_)
    {
        case TCA_MOBILE: return "mobile";
        case TCA_ROUTER: return "router";
        case TCA_GATEWAY: return "gateway";
        default: return "null";
    }

}


void
TcaRouter::handle_bundle_received(BundleReceivedEvent* event)
{           
    Bundle* bundle = event->bundleref_.object();
    // log_debug("TcaRouter: handle bundle received: *%p", bundle);

    // Special handling for certain TCA bundles
    // Control bundles are identified by their dest eids.
    // Unbound data bundles need special handling as
    // TCA control bundles are the following:
    // Check for control bundles here (REG, COA, ASK, ADV)

    const EndpointID& dest = bundle->dest();

    if (dest.scheme_str() == "tca")
    {
        log_bundle("TcaRouter: tca bundle received", bundle, true);

        TcaEndpointID tca_dest(dest);
  
        if (tca_dest.ssp() == "//registry")
        {
            handle_register(bundle);
        }

        else if (tca_dest.app() == "admin.coa")
        {
            handle_coa(bundle);
        }

        else if (tca_dest.ssp().substr(0,11) == "//anonymous")
        {
            handle_anonymous_bundle(bundle);
        }

        else if (tca_dest.ssp() == "//localhost/bundlelayer")
        {
            handle_bl_control_bundle(bundle);
        }
            
        else if (tca_dest.host() != admin_app_.host())
        {
            // What we're really checking for is whether the bundle is home.
            // ie. addressed to some app on the local host.
            // If not, then it's a late-bound data bundle:
            handle_tca_unbound_bundle(bundle);
        }
    }
    
    else
    {
        // Not a tca control or data bundle. Just forward the bundle.
        fwd_to_matching(bundle);
    }
}


void
TcaRouter::handle_bundle_transmitted(BundleTransmittedEvent* event)
{
    Bundle* b = event->bundleref_.object();
    log_debug("TcaRouter: handle bundle transmitted: *%p", b);

    const EndpointID& dest = b->dest();

    if (dest.scheme_str() == "tca")
    {
        // TODO: These control codes appended to app (eg. admin.coa) were
        // a bad idea and should be eliminated. I did this initially so that
        // we could switch on control code without inspecting bundle payload.
        // However, if the bundle is addressed to an admin app, then it's
        // fair to assume it is a control bundle and inspect the payload.
        // The "adv" case below uses the preferred method. ask and coa should
        // follow suit.
        TcaEndpointID tca_dest(dest);
        if (tca_dest.app() == "admin.coa")
        {
            TcaControlBundle cb(get_payload_str(b));
            on_coa_transmitted(b, cb);
        }
        else if (tca_dest.app() == "admin.ask")
        {
            TcaControlBundle cb(get_payload_str(b));
            on_ask_transmitted(b, cb);
        }
        else if (tca_dest.app() == "admin")
        {
            TcaControlBundle cb(get_payload_str(b));
            if (cb.code_ == "adv")
            {
                on_adv_transmitted(b, cb);
            }
        }
    }
}   
 

// Note: ContactUp and ContactDown are generated by a sending node when a
// connection is opened.  As such, they're probably not relevant to TCA.

void
TcaRouter::handle_contact_up(ContactUpEvent* event)
{
    ASSERT(event->contact_->link() != NULL);
    ASSERT(!event->contact_->link()->isdeleted());

    // Note: *must* call the base class handler so that existing bundles
    // can be checked against the new contact.
    TableBasedRouter::handle_contact_up(event);
    log_debug("TcaRouter::contact up");
    post_bundle(BL, admin_app_, "contact_up");
}
    

void
TcaRouter::handle_contact_down(ContactDownEvent* event)
{
    (void)event;
    log_debug("TcaRouter::contact down");
    post_bundle(BL, admin_app_, "contact_down");

}


void
TcaRouter::handle_link_available(LinkAvailableEvent* event)
{
    ASSERT(event->link_ != NULL);
    ASSERT(!event->link_->isdeleted());

    // Note: *must* call the base class handler so that existing bundles
    // can be checked against the new link.
    TableBasedRouter::handle_link_available(event);
    log_debug("TcaRouter::link available");
    post_bundle(BL, admin_app_, "link_available");
}


void
TcaRouter::handle_link_unavailable(LinkUnavailableEvent* event)
{
    (void)event;
    log_debug("TcaRouter::link unavailable");
    post_bundle(BL, admin_app_, "link_unavailable");
}


void
TcaRouter::handle_shutdown_request(ShutdownRequest* event)
{
    (void)event;
    log_debug("TcaRouter::daemon shutdown");
    post_bundle(BL, admin_app_, "daemon_shutdown");
}
        

// fwd_to_all "broadcasts" a bundle to all tca routes (ie. "tca://*")
// This is dead code at the moment.

int
TcaRouter::fwd_to_all(Bundle* bundle)
{
    RouteEntryVec matches;
    RouteEntryVec::iterator iter;

    std::string pattern = "tca://*";
    EndpointID tca_all = pattern;

    route_table_->get_matching(tca_all, &matches);

    int count = 0;
    for (iter = matches.begin(); iter != matches.end(); ++iter)
    {
        log_debug("TcaRouter::fwd_to_all: %s",
                  (*iter)->dest_pattern().str().c_str());
        fwd_to_nexthop(bundle, *iter);
        ++count;
    }

    log_debug("TcaRouter::fwd_to_all dest='%s': %d matches",
              bundle->dest().c_str(), count);
    return count;
}


// fwd_to_matching function overridden from TableBasedRouter
// This is necessary to filter outgoing bundles. fwd_to_matching is
// called via TableBasedRouter::check_next_hop on a variety of events
// including link_available and on contact_up.

int
TcaRouter::fwd_to_matching(Bundle* bundle, const LinkRef& next_hop)
{
    ForwardingRule fwd_rule = get_forwarding_rule(bundle);
    return fwd_to_matching_r(bundle, next_hop, fwd_rule);
}


// Specialized fwd_to_matching function with selective default-route handling
// You can foward to the default route either never, always, or "if necessary"
// where "if necessary" means iff there are no non-default routes.

int
TcaRouter::fwd_to_matching_r(Bundle* bundle, const LinkRef& next_hop,
                             ForwardingRule fwd_rule)
{
    log_debug("TcaRouter::fwd_to_matching_r: owner='%s'",
              bundle->owner().c_str());
    log_debug("TcaRouter::fwd_to_matching_r: fwd_rule=%d", fwd_rule);

    if (fwd_rule == FWD_NEVER) return 0;    // no matches

    RouteEntryVec matches;
    RouteEntryVec::iterator iter;

    route_table_->get_matching(bundle->dest(), &matches);

    // First step - split out the default route
    RouteEntry*     default_route = NULL;   
    RouteEntryVec   hard_matches;       // non-default matches
    for (iter = matches.begin(); iter != matches.end(); ++iter)
    {
        if ((*iter)->dest_pattern().str() == "tca://*")
        {
            default_route = *iter;
        }
        else
        {
            hard_matches.push_back(*iter);
        }
    }

    if (fwd_rule == FWD_UDR_EXCLUSIVELY)
    {
        if (default_route != NULL)
        {
            fwd_to_nexthop(bundle, default_route);
            return 1;   // 1 match
        }
    }

    int count = 0;

    // handle default route...
    if (fwd_rule == FWD_UDR_ALWAYS || 
            (fwd_rule == FWD_UDR_IFNECESSARY && hard_matches.size() == 0))
    {
        if (default_route != NULL)
        {
            fwd_to_nexthop(bundle, default_route);
            ++ count;
        }
    }

    // handle hard matches...
    for (iter = hard_matches.begin(); iter != hard_matches.end(); ++iter)
    {
        if (next_hop == NULL || (next_hop == (*iter)->link()))
        {
            fwd_to_nexthop(bundle, *iter);
            ++count;
        }
        else
        {
            log_debug("fwd_to_matching_r dest='%s': "
                      "ignoring match %s since next_hop link %s set",
                      bundle->dest().c_str(), (*iter)->link()->name(),
                      next_hop->name());
        }
    }

    log_debug("fwd_to_matching_r dest='%s': %d matches",
              bundle->dest().c_str(), count);

    return count;
}


bool
TcaRouter::on_coa_transmitted(Bundle* b, const TcaControlBundle& cb)
{   
    log_debug("TcaRouter: COA bundle transmitted");
            
    TcaWrappedBundle wb(cb);
    
    log_debug("    coa: source=%s, dest=%s",
                b->source().c_str(),
                b->dest().c_str());
    
    // todo: use a WrappedBundle here
    std::string coa_sent_payload = "coa_sent:";
    coa_sent_payload += b->source().str();
    coa_sent_payload += "\t";
    coa_sent_payload += b->dest().str();
    coa_sent_payload += "\t";
    coa_sent_payload += cb.args_[0]; // link_addr of mobile, from body of coa

    log_debug("    coa_sent, payload='%s'", coa_sent_payload.c_str());
    post_bundle(BL, admin_app_, coa_sent_payload);

    return true;
}


bool
TcaRouter::on_ask_transmitted(Bundle* b, const TcaControlBundle& cb)
{
    log_debug("TcaRouter: ASK bundle transmitted");

    if (!check_nargs(cb, 1)) return false;

    // todo: use a WrappedBundle here
    std::string ask_sent= "ask_sent:";
    ask_sent += b->source().str();
    ask_sent += "\t";
    ask_sent += b->dest().str();
    ask_sent += "\t";
    ask_sent += cb.args_[0];

    log_debug("    ask sent, payload='%s'", ask_sent.c_str());
    post_bundle(BL, admin_app_, ask_sent);

    return false;
}


bool
TcaRouter::on_adv_transmitted(Bundle* b, const TcaControlBundle& cb)
{
    log_debug("TcaRouter: ADV bundle transmitted");

    if (!check_nargs(cb, 2)) return false;

    // todo: use a WrappedBundle here
    std::string adv_sent= "adv_sent:";
    adv_sent += b->source().str();
    adv_sent += "\t";
    adv_sent += b->dest().str();
    adv_sent += "\t";
    adv_sent += cb.args_[0];
    adv_sent += "\t";
    adv_sent += cb.args_[1];

    log_debug("    adv_sent, payload='%s'", adv_sent.c_str());
    post_bundle(BL, admin_app_, adv_sent);

    return false;
}


bool
TcaRouter::handle_register(Bundle* b)
{
    // DK: New stuff - experimental
    // Register bundles come either from
    // 1) A client application (zero args, source eid important), or
    // 2) An admin app (2 args)
    
    if (b->source().str() == admin_app_.str())
    {
        // The local admin app sent this bundle. Just forward it to the
        // default route:
        LinkRef link("TcaRouter::handle_register: fwd_to_matching null");
        fwd_to_matching_r(b, link, FWD_UDR_EXCLUSIVELY);
    }
    else
    {
        // The bundle came from either a local app (non-admin) or
        // else another node. In either case, deliver it to local
        // admin app.
        TcaControlBundle cb(get_payload_str(b));

        TcaWrappedBundle reg_received("reg_received",
                b->source().str(), b->dest().str());

        log_debug("TcaRouter::handle_register:");
        log_controlbundle(cb);

        if (cb.args_.size() == 2)
        {
            // This bundle came from another node, with mobile_eid
            // and last_hop fields filled in.
            reg_received.append_arg(cb.args_[0]);
            reg_received.append_arg(cb.args_[1]);
        }
        else
        {
            // This bundle came from an app on the local node
            // so fill in mobile_eid and NULL last_hop
            reg_received.append_arg(b->source().str());
            reg_received.append_arg("NULL");
        }
        
        post_bundle(BL, admin_app_, reg_received.str());
    }

    return true;
}


bool
TcaRouter::handle_coa(Bundle* b)
{
    log_debug("TcaRouter: COA bundle received");

    // Propagate it along the reverse path, ignoring default route:
    LinkRef link("TcaRouter::handle_coa: fwd_to_matching null");
    fwd_to_matching_r(b, link, FWD_UDR_NEVER);

    // The old route is deleted after the coa bundle has been
    // transmitted, from on_coa_transmitted
    return true;
}


bool
TcaRouter::handle_anonymous_bundle(Bundle* b)
{
    // these are bundles addressed to a destination eid that begins
    // with tca://anonymous
    // so far, the "ask" bundle is the only one of its kind

    TcaEndpointID dest(b->dest());

    TcaControlBundle cb(get_payload_str(b));

    if (cb.code_ == "ask")
    {
        return handle_ask(b, cb);
    }
    else
    {
        log_debug("TcaRouter:: unrecognized anonymous bundle code '%s'",
                cb.code_.c_str());
        return false;
    }
}


bool
TcaRouter::handle_ask(Bundle* b, const TcaControlBundle& cb)
{   
    if (is_local_source(b))
    {
        // if the ask originated at this node, just forward it to
        // matching, omitting the default route.
        LinkRef link("TcaRouter::handle_ask: fwd_to_matching null");
        fwd_to_matching_r(b, link, FWD_UDR_NEVER);
    }
    else
    {
        if (!check_nargs(cb, 1)) return false;
    
        // generate ask_received for local admin app
        std::string payload = "ask_received:";
        payload += b->source().str();
        payload += "\t";
        payload += b->dest().str();
        payload += "\t";
        payload += cb.args_[0];

        post_bundle(BL, admin_app_, payload);
    }

    return true;
}



// handle_tca_control_bundle handles bundles addressed to
// "localhost/bundlelayer" from the local control application

bool
TcaRouter::handle_bl_control_bundle(Bundle* b)
{
    TcaControlBundle cb(get_payload_str(b));

    // handle ctl bundles:
    if (cb.code_ == "ask")
    {
        return handle_ask(b, cb);
    }
    else if (cb.code_ == "get_routes")
    {
        return handle_get_routes(b, cb);
    }
    else if (cb.code_ == "add_route")
    {
        return handle_add_route(b, cb);
    }
    else if (cb.code_ == "del_route")
    {
        return handle_del_route(b, cb);
    }

    log_debug("TcaRouter: unknown control bundle type '%s'", cb.code_.c_str());
    return false;
    
}


bool
TcaRouter::handle_bl_ask(Bundle* b, const TcaControlBundle& cb)
{
    (void)cb;
    // Note: We should never get here! asks should be addressed to the
    // control app, not bundle layer.
    return post_bundle(BL, b->source(),
        "adv:Don\'t ASK me. You should probably ASK the Control App,"
        " not the Bundle Layer.");
}


bool
TcaRouter::handle_get_routes(Bundle* b, const TcaControlBundle& cb)
{   
    if (!check_nargs(cb, 1)) return false;

    log_debug("TcaRouter:: get_routes bundle received. body = '%s'",
           cb.args_[0].c_str());

    RouteEntryVec matches;
    RouteEntryVec::iterator iter;

    EndpointIDPattern pattern(cb.args_[0]);
    route_table_->get_matching(pattern, &matches);

    std::string response = "routes:";
    for (iter = matches.begin(); iter != matches.end(); ++iter)
    {
        response += (*iter)->dest_pattern().str().c_str();
        response += "\t";
    }

    post_bundle(BL, b->source(), response);

    return true;
}


bool
TcaRouter::handle_add_route(Bundle* b, const TcaControlBundle& cb)
{
    (void)b;
    
    if (!check_nargs(cb, 2)) return false;

    const std::string& pattern = cb.args_[0];
    const std::string& link = cb.args_[1];

    log_debug("TcaRouter:: add_route bundle received. "
              "pattern='%s', link='%s'\n", pattern.c_str(), link.c_str());

    if (pattern.length() == 0 || link.length() == 0) return false;

    // TODO: Some syntax-checking would be a very good idea. Right now,
    // just blast away:
    return create_route(pattern, link);
}


bool
TcaRouter::handle_del_route(Bundle* b, const TcaControlBundle& cb)
{
    (void)b;
    
    if (!check_nargs(cb, 1)) return false;

    log_debug("TcaRouter:: del_route bundle received. body = '%s'",
           cb.args_[0].c_str());

    // TODO: Some syntax-checking would be a very good idea. Right now,
    // just blast away:
    route_table_->del_entries(cb.args_[0]);
    return true;
}


// handle_tca_unbound_bundle handles routing of regular late-bound tca data
// bundles.
// This function assumes that this router is not the dest endpoint.
// for the bundle. The logic goes like this:
// First, forward the bundle to any existing route to dest, using the default
// route iff there are no other matches.
// If there are no matches at all, and if the local node is a gateway,
// then push a "unb" bundle up to the control app for special handling.

bool
TcaRouter::handle_tca_unbound_bundle(Bundle* bundle)
{
    log_debug("TcaRouter::handle_tca_unbound_bundle...");

    LinkRef link("TcaRouter::handle_tca_unbound_bundle: fwd_to_matching null");
    int n_matches = fwd_to_matching_r(bundle, link, FWD_UDR_IFNECESSARY);

    if (n_matches == 0)
    {
        if (role_ == TCA_ROUTER)
        {
            // If I'm a router, this is an error! All tca bundles must have
            // a default route up to a gateway.
            log_err("TcaRouter: Error. TCA_ROUTER has no route to dest %s",
                    bundle->dest().c_str());
            return false;
        }
        else if (role_ == TCA_GATEWAY)
        {
            // If I'm a gateway, try late-binding...
            // Leave the unbound bundle alone, but push a control bundle up
            // to the control app, which will get registration and create a
            // route if possible.
            std::string payload = "unb:";
            payload += bundle->dest().str();
            post_bundle(BL, admin_app_, payload);
        }
    }
    return true;
}



bool
TcaRouter::is_local_source(Bundle* b)
{
    TcaEndpointID src(b->source());
    return src.get_hostid() == admin_app_.get_hostid();
}


TcaRouter::ForwardingRule
TcaRouter::get_forwarding_rule(Bundle* b)
{
    // all non-tca bundles should always be forwarded to all routes
    if (b->dest().scheme_str() != "tca") return FWD_UDR_ALWAYS;
    
    TcaEndpointID dest(b->dest());

    if (dest.ssp() == "//registry")
    {
        // forward to default route (exclusively) iff sent by the local
        // admin app
        if (b->source().str() == admin_app_.str()) return FWD_UDR_EXCLUSIVELY;
        else return FWD_NEVER;
    }

    else if (dest.app() == "admin.coa")
    {
        // Never us the default route for the coa bundles. They are on their
        // way "down" the tree to a mobile and we don't want them getting
        // forwarded back up to a gateway too.
        return FWD_UDR_NEVER;
    }

    else if (dest.ssp().substr(0,11) == "//anonymous")
    {
        // Assume this is an ask bundle. (WARNING: If there are ever any
        // anonymous bundles other than ASK, this should be changed!)
        // For ask, forward only if originating at local node, and never
        // to the default route.
        if (is_local_source(b)) return FWD_UDR_NEVER;
        else return FWD_NEVER;
    }

    else if (dest.ssp() == "//localhost/bundlelayer")
    {
        // Never forward bundles sent to the local bundle layer
        return FWD_NEVER;
    }

    // These are all the special control dest cases. What remains are any
    // ordinarily addressed tca bundles
    else
    {
        if (dest.host() == admin_app_.host())
        {
            // If addressed to local admin app, do not forward. This bundle
            // is home.
            return FWD_NEVER;
        }
        else
        {
            // Treat this as an unbound tca bundle. Forward to matches, using
            // default route iff no other matches.
            return FWD_UDR_IFNECESSARY;
        }
    }
}


// create_link creates a link for the given link_addr iff it doesn't already
// exist.
// link_addr must be in the form "tcp://host:port"

LinkRef
TcaRouter::create_link(const std::string& link_addr)
{
    // Note that deleting the old one and re-creating it is the wrong
    // thing to do, because when the link is re-created all pending
    // bundles (inlcuding the register bundle that initiated this) will
    // be matched and forwarded to the new link.

    EndpointID link_eid(link_addr);
    std::string clayer_name = link_eid.scheme_str();
    const std::string& ssp = link_eid.ssp();
    std::string host = ssp.substr(2, ssp.length()); // chop off the "//"

    ContactManager* p_man = BundleDaemon::instance()->contactmgr();

    // Check if there's an existing link of the same name.
    LinkRef p_link("TcaRouter::create_link: return value");
           
    p_link = p_man->find_link(host.c_str());
    if (p_link != NULL) return p_link;

    ConvergenceLayer* cl = ConvergenceLayer::find_clayer(clayer_name.c_str());
    if (!cl) {
        log_err("TcaRouter: create_link failed: invalid convergence layer"
                  " '%s'", clayer_name.c_str());
        return p_link;
    }
 
    p_link = Link::create_link(host, Link::ONDEMAND, cl, host.c_str(), 0, NULL);
    if (p_link == NULL) return p_link;
        
    // Add the link to contact manager's table, which posts a
    // LinkCreatedEvent to the daemon
    if (!p_man->add_new_link(p_link)) {
        p_link->delete_link();
        log_err("TcaRouter::create_link: "
                "failed to add new link %s", p_link->name());
        p_link = NULL;
        return p_link;
    }

    return p_link;
}


RouteEntry*
TcaRouter::create_route(const std::string& pattern, const LinkRef& p_link)
{

    log_debug("TcaRouter::create_route: pattern=%s, p_link=%p",
            pattern.c_str(), p_link.object());

    RouteEntry* p_entry = new RouteEntry(pattern, p_link);
    p_entry->set_action(ForwardingInfo::COPY_ACTION);

    route_table_->add_entry(p_entry);

    return p_entry;
}


bool
TcaRouter::create_route(const std::string& pattern,
                        const std::string& link_addr)
{
    // First find the right link, or create a new one if necesary
    LinkRef p_link = create_link(link_addr);
    if (p_link == NULL)
    {
        log_err("TcaRouter::create_route: create_link failed");
        return false;
    }

    // Now create the new route entry
    if (!create_route(pattern, p_link))
    {
        log_err("TcaRouter::create_route: create_route failed");
        return false;
    }

    return true;
}


bool
TcaRouter::post_bundle(const EndpointID& src, const EndpointID& dest,
                       const std::string& payload)
{

    log_debug("TcaRouter::post_bundle: [%s] -> [%s] : '%s'\n",
                src.c_str(), dest.c_str(), payload.c_str());

    // Construct bundle
    Bundle* b = new Bundle();

    // if source is unspecified, use bundlelayer
    if (src.length() == 0)
        b->mutable_source()->assign("tca://localhost/bundlelayer");
    else
        b->mutable_source()->assign(src);

    b->mutable_dest()->assign(dest);
    b->mutable_custodian()->assign(BundleDaemon::instance()->local_eid());
    b->mutable_replyto()->assign(BundleDaemon::instance()->local_eid());

    b->mutable_payload()->set_data(payload);

    // We need to set non-zero expiration or else the bundle
    // expires as soon as it arrives.
    b->set_expiration(3600);

    // The default values are ok for the rest of the bundle fields.

    // Post the bundle by generating a BundleReceivedEvent
    BundleReceivedEvent* p_event = new BundleReceivedEvent(b, EVENTSRC_ADMIN);
    BundleDaemon::instance()->post(p_event);

    return true;
}


// dead code at the moment:
bool
TcaRouter::push_wrapped_bundle(const std::string& code,
                               const EndpointID& src,
                               const EndpointID& dest,
                               const std::string& bsp)
{
    std::string payload = code;
    payload += ":";
    payload += src.str();
    payload += "\t";
    payload += dest.str();
    payload += "\t";
    payload += bsp;
    return post_bundle(BL, admin_app_, payload);
}




} // namespace dtn
