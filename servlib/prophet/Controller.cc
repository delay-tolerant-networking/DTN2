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

#include "Controller.h"

#define NEXT_INSTANCE \
    (++next_instance_ == 0) ? ++next_instance_ : next_instance_

#define LOG(_level, _args...) core_->print_log( \
        "controller", BundleCore::_level, _args )

namespace prophet
{

Controller::Controller(BundleCore* core, Repository* repository,
                       ProphetParams* params)
    : ExpirationHandler("controller"),
      core_(core),
      params_(params), 
      max_route_(0),
      nodes_(core,"local",true), 
      next_instance_(0),
      timeout_(params_->hello_interval_ * 100),
      repository_(repository)
{
    if (core == NULL || params == NULL || repository == NULL)
        // uh, how to signal error state?
    {
        LOG(LOG_ERR,"constructor invoked with NULL parameter");
        return;
    }

    if (repository_->get_comparator()->qp() != params_->qp())
    {
        QueueComp* qc = QueuePolicy::policy(params_->qp(), &stats_,
                                            &nodes_, params_->min_forward());
        repository_->set_comparator(qc);
    }

    // set up reminder for aging out Nodes, Acks
    alarm_ = core_->create_alarm(this,timeout_ * 100);

    // set upper limit to growth of routing table
    max_route_ = params->max_table_size_;
    nodes_.set_max_route(max_route_);

    LOG(LOG_DEBUG,"constructor");
}

Controller::~Controller()
{
    List::iterator i = list_.begin();
    while (i != list_.end())
    {
        delete *i;
        list_.erase(i++);
    }
}

bool
Controller::find(const Link* link, List::iterator& i)
{
    if (link == NULL) return false;

    i = list_.begin();
    while (i != list_.end())
        if ((*i)->nexthop() == link)
            break; 
        else
            i++;
    return (i != list_.end());
}

bool
Controller::is_prophet_control(const Bundle* b) const
{
    if (b == NULL) return false;

    std::string str = b->destination_id();
    return (str.rfind("/prophet") ==
            str.length() - strlen("/prophet"));
}

void
Controller::new_neighbor(const Link* link)
{
    if (link == NULL) return;

    LOG(LOG_DEBUG,"new_neighbor(%s)",link->remote_eid());

    // first search for existing Encounter
    List::iterator i;
    if (find(link,i))
    {
        if ((*i)->neighbor_gone())
        {
            LOG(LOG_INFO,"encounter-%d: neighbor gone",
                    (*i)->local_instance());
            delete *i;
            list_.erase(i);
        }
        else
        {
            LOG(LOG_DEBUG,"session exists (%s)",(*i)->name());
            return; // do nothing
        }
    }

    // if none found, create a new Encounter
    Encounter* e = new Encounter(link,this,NEXT_INSTANCE);
    if (e->neighbor_gone())
    {
        LOG(LOG_INFO,"encounter-%d: neighbor gone",
                e->local_instance());
        delete e;
    }
    else
        list_.push_back(e);
}

void
Controller::neighbor_gone(const Link* link)
{
    if (link == NULL) return;

    LOG(LOG_DEBUG,"neighbor_gone(%s)",link->remote_eid());
    // search for Encounter
    // if found, erase from list and delete from memory
    List::iterator i;
    if (find(link,i))
    {
        Encounter* e = (*i);
        list_.erase(i);
        delete e;
    }
}

bool
Controller::accept_bundle(const Bundle* b)
{
    if (b == NULL) return false;

    LOG(LOG_DEBUG,"accept_bundle(%u)",b->sequence_num());

    // do we relay to other nodes?
    if (params_->relay_node() == false)
    {
        std::string eid(core_->local_eid());
        // if not, limit what bundles are accepted -- reject all unless
        //    src or dst is local eid
        if (!core_->is_route(eid,b->destination_id()) &&
            !core_->is_route(eid,b->source_id()))
        {
            return false;
        }
    }

    // else accept 
    return true;
}

void
Controller::handle_bundle_received(const Bundle* b, const Link* link)
{
    if (b == NULL) return;

    LOG(LOG_DEBUG,"handle_bundle_received(%d,%s)",
            b->sequence_num(), (link == NULL) ? "NULL" : link->remote_eid());

    if (link == NULL)
        return;

    Encounter* e = NULL;
    List::iterator i;
    // find the appropriate Encounter protocol handler
    if (find(link,i))
    {
        e = *i;
        if (e->neighbor_gone()) 
        {
            LOG(LOG_INFO,"encounter-%d: neighbor gone",
                    e->local_instance());
            delete e;
            list_.erase(i);
            e = NULL;
        }
    }
    if (e == NULL)
    { 
        LOG(LOG_INFO,"unable to find session to assign bundle from %s",
                b->source_id().c_str());
        return;
    }
    // handle the protocol message
    if (is_prophet_control(b))
    {
        // read contents from core's bundle payload into BundleBuffer
        size_t len = b->size();
        u_char* buf = new u_char[len];
        if (!core_->read_bundle(b,buf,len))
        {
            delete [] buf;
            // signal error state?!?
            LOG(LOG_ERR,"BundleCore failed to read bundle buffer");
            core_->drop_bundle(b);
            return;
        }
        // parse into ProphetTLV
        ProphetTLV* tlv = ProphetTLV::deserialize(b->source_id(),
                b->destination_id(), buf, len);

        delete [] buf;

        if (tlv == NULL)
        {
            // signal error state?!?
            LOG(LOG_ERR,"ProphetTLV failed to deserialize");
            core_->drop_bundle(b);
            return;
        }

        e->receive_tlv(tlv);
        if (e->neighbor_gone())
        {
            LOG(LOG_INFO,"encounter-%d: neighbor gone",
                    e->local_instance());
            delete e;
            list_.erase(i);
        }

        // signal back to core that this bundle can be dropped now
        core_->drop_bundle(b);
    }
    else 
    {
        // decrement the request count
        e->handle_bundle_received(b);
    } 
}

void
Controller::handle_bundle_transmitted(const Bundle* b, const Link* l)
{
    if (b == NULL) return;
    if (l == NULL) return;
    if (is_prophet_control(b))
    {
        core()->drop_bundle(b);
        return;
    }
    stats_.update_stats(b,nodes_.p_value(l->remote_eid()));
    repository_->change_priority(b);
}

void
Controller::ack(const Bundle* b)
{
    if (b == NULL || is_prophet_control(b)) return;

    LOG(LOG_DEBUG,"ack(%u)", b->sequence_num());
    // insert ack, drop from stats, drop from bundles, drop from core
    Oracle::ack(b);
}

void
Controller::set_queue_policy()
{
    // read current values to decide whether to create new comparator
    QueueComp* qc = const_cast<QueueComp*>(repository_->get_comparator());

    if (qc->qp() != params_->qp() || params_->min_forward() != qc->min_fwd_)
    {
        LOG(LOG_DEBUG,"set_queue_policy (%s -> %s)",
                QueuePolicy::qp_to_str(qc->qp()),
                QueuePolicy::qp_to_str(params_->qp()));

        qc = QueuePolicy::policy(params_->qp(), &stats_, &nodes_,
                                 params_->min_forward());
        // repository_ cleans up previous qc object
        repository_->set_comparator(qc);
    }
}

void
Controller::set_hello_interval()
{
    if (timeout_ != ((u_int)(params_->hello_interval_ * 100)))
    {
        LOG(LOG_DEBUG,"set_hello_interval (%u -> %u)",
                timeout_/100, params_->hello_interval_);

        timeout_ = params_->hello_interval_ * 100;
        // alert the active peering sessions to parameter change
        for (List::iterator i = list_.begin(); i != list_.end(); i++)
            (*i)->hello_interval_changed();
    }
}

void
Controller::set_max_route()
{
    if (max_route_ != ((u_int)(params_->max_table_size_)))
    {
        LOG(LOG_DEBUG,"set_max_route (%u -> %u)",
                max_route_, params_->max_table_size_);
        nodes_.set_max_route(max_route_);
    }
}

void
Controller::handle_timeout()
{
    LOG(LOG_DEBUG,"handle_timeout");

    acks_.expire();
    nodes_.age_nodes();
    nodes_.truncate(params_->epsilon());

    // schedule new alarm
    alarm_ = core_->create_alarm(this,timeout_ * 100);

    List::iterator i = list_.begin();
    while (i != list_.end())
    {
        if ((*i)->neighbor_gone())
        {
            LOG(LOG_INFO,"encounter-%d: neighbor gone",
                    (*i)->local_instance());
            delete *i;
            list_.erase(i++);
        }
        else
            i++;
    }
}

void
Controller::shutdown()
{
    LOG(LOG_DEBUG,"shutdown");
    if (alarm_ != NULL && alarm_->pending())
        // host's timer implementation will clean this up
        alarm_->cancel(); 
    while (! list_.empty())
    {
        delete list_.front();
        list_.pop_front();
    }
}

}; // namespace prophet
