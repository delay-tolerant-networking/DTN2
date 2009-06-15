/*
 *    Copyright 2007 Baylor University
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

#include "bundling/BundleProtocol.h"
#include "bundling/BundleDaemon.h"
#include <oasys/thread/Lock.h>

#include "prophet/QueuePolicy.h"
#include "ProphetRouter.h"

namespace dtn
{

void prophet_router_shutdown(void*)
{
    BundleDaemon::instance()->router()->shutdown();
}

prophet::ProphetParams ProphetRouter::params_;

bool ProphetRouter::is_init_ = false;

ProphetRouter::ProphetRouter()
    : BundleRouter("ProphetRouter","prophet"),
      core_(NULL), oracle_(NULL),
      lock_(new oasys::SpinLock("ProphetRouter"))
{
}

ProphetRouter::~ProphetRouter()
{
    delete oracle_;
    delete core_;
    delete lock_;
}

void
ProphetRouter::initialize()
{
    ASSERT( is_init_ == false );

    // create local instance of ProphetBundleCore,
    // prophet::Repository, and prophet::Controller
    std::string local_eid(BundleDaemon::instance()->local_eid().str());

    core_ = new ProphetBundleCore(local_eid,actions_,lock_);
    oracle_ = new prophet::Controller(core_,core_->bundles(),&params_);

    // register the global shutdown function
    BundleDaemon::instance()->set_rtr_shutdown(
            prophet_router_shutdown, (void *) 0);

    // deserialize any routes from permanent storage 
    core_->load_prophet_nodes(oracle_->nodes(),&params_);

    is_init_ = true;
    log_info("ProphetRouter initialization complete");
}

void
ProphetRouter::shutdown()
{
    log_info("ProphetRouter shutdown");
    oasys::ScopeLock l(lock_, "shutdown");
    oracle_->shutdown();
    core_->shutdown();
}

void
ProphetRouter::handle_event(BundleEvent* e)
{
    dispatch_event(e);
}

void
ProphetRouter::get_routing_state(oasys::StringBuffer* buf)
{
    log_info("ProphetRouter get_routing_state");
    oasys::ScopeLock l(lock_, "get_routing_state");

    // summarize current number of routes, misc. statistics
    buf->appendf("ProphetRouter:\n"
            "  %zu routes, %zu queued bundles, %zu ACKs, %zu active sessions\n",
            oracle_->nodes()->size(),
            core_->bundles()->size(),
            oracle_->acks()->size(),
            oracle_->size());
    // iterate over Encounters and query their status
    buf->appendf("Active Sessions\n");
    for (prophet::Controller::List::const_iterator i = oracle_->begin();
            i != oracle_->end(); i++)
    {
        buf->appendf(" %4d: %-30s %s timeout %u\n",
                (*i)->local_instance(),
                (*i)->nexthop()->remote_eid(),
                (*i)->state_str(),
                (*i)->time_remaining());
    }
    buf->appendf("Routes\n");
    for (prophet::Table::const_iterator i = oracle_->nodes()->begin();
            i != oracle_->nodes()->end(); i++)
    {
        buf->appendf("       %-30s: %.2f %s%s%s %lu s old\n",
                i->second->dest_id(), i->second->p_value(),
                i->second->relay() ? "R" : " ",
                i->second->custody() ? "C" : " ",
                i->second->internet_gw() ? "I" : " ",
                (time(0) - i->second->age()));
    }
    buf->appendf("\n R - relay   C - custody   I - internet gateway \n\n");

    //XXX/wilson debug
    buf->appendf("Bundles:\n");
    prophet::BundleList bundles = core_->bundles()->get_bundles();
    for (prophet::BundleList::iterator i = bundles.begin();
            i != bundles.end(); i++)
    {
        buf->appendf("%s -> %s (%u:%u)\n",
                (*i)->source_id().c_str(),
                (*i)->destination_id().c_str(),
                (*i)->creation_ts(),
                (*i)->sequence_num());
    }
}

bool
ProphetRouter::accept_bundle(Bundle* bundle, int* errp)
{
    log_info("ProphetRouter accept_bundle");

    // first ask base class
    if (!BundleRouter::accept_bundle(bundle,errp))
    {
        log_debug("BundleRouter rejects *%p",bundle);
        return false;
    }

    BundleRef tmp("accept_bundle");
    tmp = bundle;

    oasys::ScopeLock l(lock_, "accept_bundle");
    // retrieve temp prophet handle to Bundle metadata
    const prophet::Bundle* b = core_->get_temp_bundle(tmp);
    if (errp != NULL) errp = (int) BundleProtocol::REASON_NO_ADDTL_INFO;
    // ask controller's opinion on this bundle
    bool ok = oracle_->accept_bundle(b);
    // clean up memory used by temporary wrapper
    delete b;
    log_debug("do%saccept bundle *%p", ok ? " " : " not ", bundle);
    return ok;
}

void
ProphetRouter::handle_bundle_received(BundleReceivedEvent* e)
{
    log_info("ProphetRouter handle_bundle_received");

    // should not be reached, but somehow still is
    if (e->source_ == EVENTSRC_STORE)
        return;

    const prophet::Link* l = NULL;

    oasys::ScopeLock sl(lock_, "handle_bundle_received");
    // Locally generated files do not have a link specified either
    // The ping reflector generates bundles with EVENTSRC_ADMIN
    // [Note from Elwyn Davies: Maybe using a special link might be useful]
    if ((e->source_ != EVENTSRC_APP) && (e->source_ != EVENTSRC_ADMIN))
    {
	// The external CL does not set this field, which the Prophet
	// implementation needs. We want to fail quickly if we're
	// running with the ECL.
	ASSERT(e->link_ != NULL);

        // add DTN's Link to BundleCore facade
        core_->add(e->link_);
        
        // retrieve prophet's handle to Link metadata
        l = core_->get_link(e->link_.object());

        if (l == NULL) return;
    }

    // create temporary prophet handle to Bundle metadata
    const prophet::Bundle* b = core_->get_temp_bundle(e->bundleref_);

    if (b == NULL)
    {
        log_err("failed to retrieve prophet handle for *%p",
                e->bundleref_.object());
        return;
    }

    core_->bundles_.add(b);

    // inform Controller that a new bundle has arrived on this link
    oracle_->handle_bundle_received(b,l);
}

void
ProphetRouter::handle_bundle_delivered(BundleDeliveredEvent* e)
{
    log_info("ProphetRouter handle_bundle_delivered");
    oasys::ScopeLock l(lock_, "handle_bundle_delivered");

    Bundle* bundle = e->bundleref_.object();
    if (bundle == NULL) return;

    // retrieve prophet's handle to Bundle metadata
    const prophet::Bundle* b = core_->get_bundle(bundle);
    if (b == NULL) 
    {
        log_err("Failed to convert *%p to prophet object",bundle);
        return;
    }
    // BundleDeliveredEvent means prophet::Ack, which kicks Bundle out of Prophet
    oracle_->ack(b);
}

void
ProphetRouter::handle_bundle_expired(BundleExpiredEvent* e)
{
    log_info("ProphetRouter handle_bundle_expired");
    oasys::ScopeLock l(lock_, "handle_bundle_expired");

    const prophet::Bundle* b = NULL;
    Bundle* bundle = e->bundleref_.object();
    if (bundle != NULL && ((b = core_->get_bundle(bundle)) != NULL))
    {
        // drop Prophet stats on this bundle
        oracle_->stats()->drop_bundle(b);
    }

    core_->del(e->bundleref_);
}

void 
ProphetRouter::handle_bundle_transmitted(BundleTransmittedEvent* e)
{
    const prophet::Bundle* bundle = core_->get_bundle(e->bundleref_.object());
    const prophet::Link* link = core_->get_link(e->link_.object());
    if (bundle != NULL && link != NULL)
        oracle_->handle_bundle_transmitted(bundle,link);
}

void
ProphetRouter::handle_contact_up(ContactUpEvent* e)
{
    log_info("ProphetRouter handle_contact_up");
    oasys::ScopeLock lk(lock_, "handle_contact_up");

    Link* link = e->contact_->link().object();
    if (link == NULL) return;

    // add DTN's Link to BundleCore facade
    core_->add(e->contact_->link());
    // retrieve prophet's handle to Link metadata
    const prophet::Link* l = core_->get_link(link);
    if (l == NULL) return;
    // tell Controller about our new friend
    oracle_->new_neighbor(l);
}

void
ProphetRouter::handle_contact_down(ContactDownEvent* e)
{
    log_info("ProphetRouter handle_contact_down");
    oasys::ScopeLock lk(lock_, "handle_contact_down");

    Link* link = e->contact_->link().object();

    // retrieve prophet's handle to Link metadata
    const prophet::Link* l = NULL;
    if (link != NULL &&
            (l = core_->get_link(e->contact_->link().object())) != NULL)
        // inform Controller about the loss
        oracle_->neighbor_gone(l);
    // drop BundleCore's knowledge about this Link
    core_->del(e->contact_->link());
}

void
ProphetRouter::handle_link_available(LinkAvailableEvent* e)
{
    LinkRef next_hop = e->link_;
    ASSERT(next_hop != NULL);
    ASSERT(!next_hop->isdeleted());

    // Prophet initiates its protocol based on handle_contact_up,
    // which fires upon success link open ... so poke it and see 
    // what happens
    if (!next_hop->isopen())
    {
        // request to open link
        actions_->open_link(next_hop);
    }
}

void
ProphetRouter::set_queue_policy()
{
    log_info("ProphetRouter set_queue_policy");
    oasys::ScopeLock l(lock_, "set_queue_policy");
    // tell Controller to reorganize internal bundle policy based on new 
    // parameters written to params_ by ProphetCommand
    oracle_->set_queue_policy();
}

void
ProphetRouter::set_hello_interval()
{
    log_info("ProphetRouter set_hello_interval");
    oasys::ScopeLock l(lock_, "set_hello_interval");
    // tell Controller to change internal protocol timeouts based on new
    // parameters written to params_ by ProphetCommand
    oracle_->set_hello_interval();
}

void
ProphetRouter::set_max_route()
{
    log_info("ProphetRouter set_max_route");
    oasys::ScopeLock l(lock_, "set_max_route");
    // tell Controller to change internal limit on number of routes
    // to retain, based on changes made to params_ by ProphetCommand
    oracle_->set_max_route();
}

}; // namespace dtn
