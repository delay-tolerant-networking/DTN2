/*
 *    Copyright 2006 Baylor University
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
#  include <config.h>
#endif

#include <netinet/in.h>
#include "bundling/Bundle.h"
#include "bundling/BundleRef.h"
#include "bundling/BundleList.h"
#include "bundling/BundleProtocol.h"
#include "bundling/BundleDaemon.h"
#include "storage/BundleStore.h"
#include "ProphetController.h"
#include <oasys/thread/Lock.h>
#include <oasys/util/Random.h>
#include <oasys/util/ScratchBuffer.h>

#include "storage/ProphetStore.h"

namespace dtn {

template<>
ProphetController* oasys::Singleton<ProphetController,false>::instance_ = NULL;

void
ProphetController::do_init(ProphetParams* params,
                           const BundleList* list,
                           BundleActions* actions,
                           const char* logpath) 
{
    ASSERT(params != NULL);
    ASSERT(actions != NULL);
    ASSERT(list != NULL);

    params_ = params;
    actions_ = actions;

    bundles_ = new ProphetBundleQueue(list, params,
                           *(QueueComp::queuecomp(params->qp_,
                                                &pstats_,
                                                &nodes_)));

    node_age_timer_ = new ProphetTableAgeTimer(&nodes_,
                                               params_->age_period_,
                                               params_->epsilon_);
    ack_age_timer_ = new ProphetAckAgeTimer(&acks_,params_->age_period_);

    lock_ = new oasys::SpinLock();

    set_logpath(logpath);
}

ProphetController::ProphetController()
    : oasys::Logger("ProphetController","/dtn/route/prophet/controller"),
      params_(NULL),
      node_age_timer_(NULL),
      ack_age_timer_(NULL),
      actions_(NULL),
      bundles_(NULL)
{
    Prophet::UniqueID::init();
    encounters_.clear();
    prophet_eid_.assign(BundleDaemon::instance()->local_eid());
    ASSERT(prophet_eid_.append_service_tag("prophet"));
}

ProphetController::~ProphetController()
{
    delete node_age_timer_;
    delete ack_age_timer_;
    delete bundles_;
    delete lock_;
}

void
ProphetController::load_prophet_nodes()
{
    ProphetNode* node;
    ProphetStore* prophet_store = ProphetStore::instance();
    ProphetStore::iterator* iter = prophet_store->new_iterator();

    log_notice("loading prophet nodes from data store");

    for (iter->begin(); iter->more(); iter->next()) {
        EndpointID eid = iter->cur_val();
        log_debug("prophet_store iterator at %s", eid.c_str());
        node = prophet_store->get(iter->cur_val());

        if (node == NULL) {
            log_err("error loading node data from data store");
            continue;
        }

        node->set_params(params_);
        nodes_.update(node);
    }
    delete iter;

    // once deserialized, calculate the age effect
    nodes_.age_nodes();
    // then trim off anything below minimum predictability
    nodes_.truncate(params_->epsilon_);
}

void 
ProphetController::shutdown()
{
    log_notice("processing shutdown event");
    {
        oasys::ScopeLock l(lock_,"destructor");
        enc_set::iterator it = encounters_.begin();
        while( it != encounters_.end() )
        {
            ProphetEncounter* pe = *it;
            pe->neighbor_gone();
            it++;
        }
        encounters_.clear();
    }
    node_age_timer_->cancel();
    ack_age_timer_->cancel();
}

// this will eventually turn into a nasty beast, what can I do to condense it?
void
ProphetController::dump_state(oasys::StringBuffer* buf)
{
    oasys::ScopeLock l(lock_,"dump_state");
    buf->appendf("\n"
                 "ProphetRouter [%s] [%s] (%zu active, %zu known)\n"
                 "-------------\n",
                 Prophet::fs_to_str(params_->fs_),
                 Prophet::qp_to_str(params_->qp_),
                 encounters_.size(), nodes_.size());

    // iterate over active encounters
    for (enc_set::iterator it = encounters_.begin();
         it != encounters_.end();
         it++)
    {
         ProphetEncounter* pe = *it;
         pe->dump_state(buf);
    }

    // iterate over nodes
    buf->appendf("\n"
                 "Known peers\n"
                 "-----------\n");
    oasys::ScopeLock n(nodes_.lock(),"ProphetController::dump_state");
    for (ProphetTable::iterator i = nodes_.begin();
         i != nodes_.end();
         i++)
    {
        EndpointID eid = (*i).first;
        ProphetNode* node = (*i).second;
        buf->appendf("%02.2f  %c%c%c %s\n",
                     node->p_value(),
                     node->relay() ? 'R' : ' ',
                     node->custody() ? 'C' : ' ',
                     node->internet_gw() ? 'I' : ' ',
                     eid.c_str());
    }

    buf->appendf("\n R - relay   C - custody   I - internet gateway\n\n");
}

ProphetEncounter*
ProphetController::find_instance(const LinkRef& link)
{
    oasys::ScopeLock l(lock_,"find_instance");
    enc_set::iterator it = encounters_.begin();
    while( it != encounters_.end() )
    {
        if((*it)->next_hop()->remote_eid().equals(link->remote_eid()))
            return (ProphetEncounter*) (*it);
        else
            log_debug("find_instance: %s != %s",
                      (*it)->next_hop()->remote_eid().c_str(),
                      link->remote_eid().c_str());
        it++;
    }
    return NULL;
}

void
ProphetController::new_neighbor(const ContactRef& contact)
{
    log_info("NEW_NEIGHBOR signal from *%p",contact.object());
    LinkRef link = contact.object()->link();
    ProphetEncounter* pe = find_instance(link);
    if (pe == NULL && !link->remote_eid().equals(EndpointID::NULL_EID()))
    {
        pe = new ProphetEncounter(link, this);
        if (!reg(pe))
        {
          delete pe;
          return;
        }
        pe->start();
    }
}

void
ProphetController::neighbor_gone(const ContactRef& contact)
{
    LinkRef link = contact.object()->link();
    log_info("NEIGHBOR_GONE signal from *%p",contact.object());
    ProphetEncounter* pe = NULL;
    if((pe = find_instance(link)) != NULL)
    {
        pe->neighbor_gone(); // self deletes once stopped
        log_info("found and stopped ProphetEncounter instance");
    }
    else
    {
        log_info("did not find ProphetEncounter instance");
    }
}

void
ProphetController::handle_bundle_received(Bundle* bundle,const ContactRef& contact)
{
    log_debug("handle_bundle_received, *%p from *%p",bundle,contact.object());

    // Look up information on Bundle destination, else start a new record
    EndpointID routeid = Prophet::eid_to_routeid(bundle->dest_);
    ProphetNode* node = nodes_.find(routeid);
    if (node == NULL && !routeid.equals(BundleDaemon::instance()->local_eid()))
    {
        node = new ProphetNode(params_);
        node->set_eid(routeid);
        nodes_.update(node);
    }

    // This is pretty much a hack, redundant to Registration; it's necessary to
    // preserve the association of Bundle to Contact, needed by ProphetEncounter
    if (prophet_eid_.equals(bundle->dest_))
    {
        // attempt to read out Prophet control message
        ProphetTLV* pt = ProphetTLV::deserialize(bundle);
        if (pt != NULL)
        {
            log_debug("handle_bundle_received, got TLV size %d",pt->length());
            // demux to appropriate ProphetEncounter instance
            ProphetEncounter *pe = find_instance(contact->link());
            if (pe == NULL)
            {
                // this is first contact, create a new instance
                new_neighbor(contact);
                if ((pe = find_instance(contact->link())) == NULL)
                {
                    log_err("Unable to find or create ProphetEncounter to "
                            "handle Prophet control message *%p",bundle);
                    delete pt;
                }
            }

            if (pe != NULL)
            {
                // dispatch message to ProphetEncounter
                log_debug("handle_bundle_received, dispatching TLV to instance %d",
                          pe->local_instance());
                pe->receive_tlv(pt);
            }

            // our way of signalling Bundle Delivered
            actions_->delete_bundle(bundle,BundleProtocol::REASON_NO_ADDTL_INFO);
        }
    }

    // not a control message
    else
    {
        oasys::ScopeLock l(lock_,"handle_bundle_received");
        // signal each thread that a Bundle has arrived
        enc_set::iterator it = encounters_.begin();
        while( it != encounters_.end() )
        {
            ProphetEncounter* pe = *it;
            pe->handle_bundle_received(bundle);
            it++;
        }

        // enqueue with Prophet queueing policy
        bundles_->push(bundle);
    }
}

void
ProphetController::handle_bundle_delivered(Bundle* b)
{
    BundleRef bundle("handle_bundle_delivered");
    bundle = b;
    if (bundle.object() == NULL) return;

    // add to ack list
    if (acks_.insert(bundle.object()))
    {
        log_info("inserting Prophet ack for *%p",bundle.object());
    }
    else
    {
        log_info("failed to insert Prophet ack for *%p",bundle.object());
    }

    // drop from local store
    bundle = NULL;
    bundles_->drop_bundle(b);
    actions_->delete_bundle(b,BundleProtocol::REASON_NO_ADDTL_INFO);
}

void 
ProphetController::handle_bundle_expired(Bundle* b)
{
    // drop stats entry for this bundle
    pstats_.drop_bundle(b);

    // dequeue from Prophet's bundle store
    bundles_->drop_bundle(b);
}

void
ProphetController::handle_link_state_change_request(const ContactRef& c)
{
    // demux to appropriate instance
    ProphetEncounter* pe = find_instance(c.object()->link());
    if (pe != NULL)
    {
        // attempt to send queued bundles, if any
        pe->flush_pending();
    }
}

bool
ProphetController::accept_bundle(Bundle* bundle,int* errp)
{
    // first ask the basic question: do we relay to other nodes?
    if (params_->relay_node_ == false)
    {
        // reject any messages from remote source that don't concern local node 
        if (! (Prophet::route_to_me(bundle->source_) ||
               Prophet::route_to_me(bundle->dest_)) )
        {
            return false;
        }
    }

    // synch up with BundleStore
    if (BundleStore::instance()->payload_quota() != bundles_->max())
        bundles_->set_max(BundleStore::instance()->payload_quota());

    // answer the question
    if (bundles_->max() != 0 &&
        bundles_->current() + bundle->payload_.length() > bundles_->max())
    {
        log_info("accept_bundle: rejecting bundle *%p since "
                 "cur size %u + bundle size %zu > %u",
                 bundle,bundles_->current(),
                 bundle->payload_.length(),bundles_->max());
        *errp = BundleProtocol::REASON_DEPLETED_STORAGE;
        return false;
    }
    *errp = 0;
    return true;
}

} // namespace dtn
