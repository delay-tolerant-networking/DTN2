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

#include "Dictionary.h"
#include "Table.h"

namespace prophet
{

#define MIN_LENGTH (strlen("dtn://"))

#define LOG_LT_MIN_LENGTH core_->print_log(name_.c_str(),BundleCore::LOG_ERR, \
            "destination id is shorter than required minimum @ %s:%d", \
            __FILE__, __LINE__)

Table::Table(BundleCore* core,
             const std::string& name,
             bool persistent)
    : core_(core), persistent_(persistent), name_(name), max_route_(0)
{
}

Table::Table(const Table& t)
    : core_(t.core_), persistent_(t.persistent_),
      name_(t.name_), max_route_(0)
{
    iterator i = table_.begin();
    for (const_iterator c = t.table_.begin();
            c != t.table_.end();
            c++)
    {
        Node *n = new Node(*(c->second));
        i = table_.insert(i,rib_table::value_type(c->first,n));
        // keep track of min-heap for quota-enforcement
        heap_add(n);
    }
}

Table::~Table()
{
    free();
    table_.clear();
}

void Table::heap_add(Node* n) {
    heap_.add(n);
}

void
Table::heap_del(Node* n) {
    // first make sure we're picking off the right victim
    size_t pos = n->heap_pos_;
    const Node*  old_n;
    if (pos < heap_.size())
    {
        old_n = heap_.sequence()[pos];
        if (old_n->dest_id_ == n->dest_id_)
            heap_.remove(pos);
    }
}

const Node*
Table::find(const std::string& dest_id) const
{
    if (dest_id.length() > MIN_LENGTH)
    {
        const_iterator it = (const_iterator) table_.find(dest_id);
        if (it != table_.end())
            return (*it).second;
    }
    else
        LOG_LT_MIN_LENGTH;
    return NULL;
}

double
Table::p_value(const std::string& dest_id) const
{
    const Node* n = find(dest_id);
    if (n == NULL)
        return 0.0;

    return n->p_value();
}

double
Table::p_value(const Bundle* b) const
{
    std::string dest = b->destination_id();
    std::string route = core_->get_route(dest);
    const Node* n = find(route);
    if (n == NULL)
        return 0.0;

    return n->p_value();
}

void
Table::update(Node* n)
{
    if (n == NULL ||
        strlen(n->dest_id()) < MIN_LENGTH)
    {
        LOG_LT_MIN_LENGTH;
        return;
    }

    // initialize an iterator to point to the insertion
    // point for this dest_id
    iterator i;

    double p_old = 0.0;
    // if this dest_id is already known, perform some
    // memory management while updating the existing node
    if (find(n->dest_id(),&i))
    {
        Node* old = i->second;
        p_old = i->second->p_value();
        i->second = n;
        if (n != old) 
        {
            heap_del(old);
            delete old;
        }
        else
            heap_del(n);
    }
    // else this is a new dest_id, insert right here
    else
        table_.insert(i,rib_table::value_type(n->dest_id(),n));

    heap_add(n);
    enforce_quota();

    // log the results
    core_->print_log(name_.c_str(),BundleCore::LOG_INFO,
            "updating route %s (%.2f -> %.2f) direct\n",
            n->dest_id(), p_old, n->p_value());

    // utilize persistent storage as appropriate
    if (persistent_) core_->update_node(n);
}

void
Table::update_route(const std::string& dest_id,
                    bool relay, bool custody, bool internet)
{
    if (dest_id.length() < MIN_LENGTH)
    {
        LOG_LT_MIN_LENGTH;
        return;
    }

    double p_old = 0.0;

    iterator i;
    Node* n = NULL;
    if (find(dest_id,&i)) 
    {
        n = i->second;
        p_old = n->p_value();
        n->update_pvalue();
        n->set_relay(relay);
        n->set_custody(custody);
        n->set_internet_gw(internet);
        heap_del(n);
    }
    else
    {
        n = new Node(dest_id,relay,custody,internet);
        n->update_pvalue();
        table_.insert(i,rib_table::value_type(dest_id,n));
    }
    heap_add(n);
    enforce_quota();

    // log the results
    core_->print_log(name_.c_str(),BundleCore::LOG_INFO,
            "updating route %s (%.2f -> %.2f) direct\n",
            n->dest_id(), p_old, n->p_value());

    // utilize persistent storage as appropriate
    if (persistent_) core_->update_node(n);
}

void
Table::update_transitive(const std::string& dest_id,
                         const std::string& peer_id, double peer_pvalue,
                         bool relay, bool custody, bool internet)
{
    if (dest_id.length() < MIN_LENGTH ||
            peer_id.length() < MIN_LENGTH)
    {
        LOG_LT_MIN_LENGTH;
        return;
    }

    double ab = p_value(peer_id);
    double p_old = 0.0;
    iterator i;
    Node* n = NULL;
    if (find(dest_id,&i)) 
    {
        n = i->second;
        p_old = n->p_value();
        n->update_transitive(ab,peer_pvalue);
        n->set_relay(relay);
        n->set_custody(custody);
        n->set_internet_gw(internet);
        heap_del(n);
    }
    else
    {
        n = new Node(dest_id,relay,custody,internet);
        n->update_transitive(ab,peer_pvalue);
        table_.insert(i,rib_table::value_type(dest_id,n));
    }
    heap_add(n);
    enforce_quota();

    // log the results
    core_->print_log(name_.c_str(),BundleCore::LOG_INFO,
            "updating route %s (%.2f -> %.2f) transitive\n",
            n->dest_id(), p_old, n->p_value());

    // utilize persistent storage as appropriate
    if (persistent_) core_->update_node(n);
}

void
Table::update_transitive(const std::string& peer_id,
                         const RIBNodeList& rib,
                         const Dictionary&  ribd)
{
    if (peer_id.length() < MIN_LENGTH)
    {
        LOG_LT_MIN_LENGTH;
        return;
    }

    double ab = p_value(peer_id);
    for (RIBNodeList::const_iterator i = rib.begin(); i != rib.end(); i++)
    {
        double p_old = 0.0;

        if ((*i)->sid_ == Dictionary::INVALID_SID)
            continue; // perhaps logging would be appropriate ?

        std::string eid = ribd.find((*i)->sid_);

        if (eid == "")
            continue; // perhaps logging would be appropriate ?

        iterator t;
        Node* n = NULL;
        if (find(eid,&t)) 
        {
            n = t->second;
            p_old = n->p_value();
            n->update_transitive(ab,(*i)->p_value());
            n->set_relay((*i)->relay());
            n->set_custody((*i)->custody());
            n->set_internet_gw((*i)->internet_gw());
            heap_del(n);
        }
        else
        {
            n = new Node(eid,
                         (*i)->relay(),
                         (*i)->custody(),
                         (*i)->internet_gw());
            n->update_transitive(ab,(*i)->p_value());
            table_.insert(t,rib_table::value_type(eid,n));
        }
        heap_add(n);
        enforce_quota();

        // log the results
        core_->print_log(name_.c_str(),BundleCore::LOG_INFO,
                "updating route %s (%.2f -> %.2f) transitive\n",
                eid.c_str(), p_old, n->p_value());

        // utilize persistent storage as appropriate
        if (persistent_) core_->update_node(n);
    }
}

size_t
Table::clone(NodeList& list) const
{
    // make sure nothing else is cluttering the incoming list 
    list.clear();

    size_t num = 0;
    for (const_iterator i = table_.begin(); i != table_.end(); i++)
    {
        // NodeList assumes ownership of each Node, so allocate
        // a new copy
        list.push_back(new Node(*((*i).second)));
        // increment the reported total
        num++;
    }
    return num;
}

size_t
Table::truncate(double epsilon) 
{
    size_t num = 0;

    // weed out the oddball case first
    if (epsilon >= 1.0)
    {

        // all nodes are goners, so skip the iteration below
        num = table_.size();
        free();
        table_.clear();

        // most efficient way to clear heap is from the back
        size_t pos = heap_.size();
        while (pos-- > 0)
            heap_.remove(pos);

        return num;
    }

    Node* n;
    iterator i;
    if (heap_.empty()) {
        for (i = table_.begin(); i != table_.end(); i++) {
            n = (*i).second;
            if (n->p_value() < epsilon) {
                iterator j = i++;
                // remove this element and clean up its memory
                remove(&j);
                // count this removal towards the reported total
                num++;
            } else
                i++;
        }
    } else {
        n = heap_.top();
        while (n->p_value() < epsilon)
        {
            if (find(n->dest_id_ref(),&i)) // if (find(n->dest_id(),&i))
            {
                // remove this element and clean up its memory
                remove(&i);
                // remove() will pop() from heap, now advance to next lowest
                n = heap_.top();
                // count this removal in the reported total
                num++;
            }
            else
                break;
        }
    }

    // log the results
    if (num > 0)
    core_->print_log(name_.c_str(),BundleCore::LOG_INFO,
            "removed %zu routes with p_value < (epsilon %.4f)\n",
            num, epsilon);

    return num;
}

void
Table::assign(const RIBNodeList& list,
              const Dictionary& ribd)
{
    for (RIBNodeList::const_iterator i = list.begin(); i != list.end(); i++)
    {
        std::string eid = ribd.find((*i)->sid_);
        Node* n = new Node(*(*i));
        n->set_dest_id(eid);
        update(n);
    }

    // log if anything happens
    if (list.size() > 0)
    core_->print_log(name_.c_str(),BundleCore::LOG_INFO,
            "assigned %zu routes from RIB\n",
            list.size());
}

void Table::assign(const std::list<const Node*>& list, 
                   const NodeParams* params)
{
    // turn off permanent storage updates, since this is coming from storage
    bool old = persistent_;
    persistent_ = false;
    for (std::list<const Node*>::const_iterator i = list.begin();
            i != list.end(); i++)
    {
        Node* n = new Node(*(*i));
        n->set_params(params);
        update(n);
    }
    persistent_ = old;

    // log if anything happens
    if (list.size() > 0)
    core_->print_log(name_.c_str(),BundleCore::LOG_INFO,
            "assigned %zu routes from permanent storage\n",
            list.size());
}

size_t
Table::age_nodes()
{
    size_t num = 0;
    for (iterator i = table_.begin(); i != table_.end(); i++)
    {
        Node *n = (*i).second;
        n->update_age();
        heap_del(n);
        heap_add(n);
        num++;

        // utilize persistent storage as appropriate
        if (persistent_) core_->update_node(n);
    }

    // log if anything happens
    if (num > 0)
    core_->print_log(name_.c_str(),BundleCore::LOG_INFO,
            "applied age algorithm to %zu nodes\n", num);

    return num;
}

void
Table::remove(iterator* i)
{
    // update persistent storage as appropriate
    if (persistent_) core_->delete_node((*i)->second);

    Node* n = (**i).second;
    table_.erase(*i);
    heap_del(n);
    delete n;
}

void
Table::enforce_quota()
{
    if (max_route_ == 0)
        // quota disabled
        return;

    iterator i;
    core_->print_log(name_.c_str(),BundleCore::LOG_INFO,
            "enforce_quota table_size=[%zu] max_route=[%u]",
            table_.size(), max_route_);
    while(table_.size() > max_route_)
    {
        if (heap_.empty())
            break;

        // select the minimum known route and evict
        Node* n = heap_.top();
        if (find(n->dest_id(),&i))
            remove(&i);
        else
            // shouldn't be reached ... ?
            heap_del(n);
    }
}

void
Table::free()
{
    for (iterator i = table_.begin(); i != table_.end(); i++)
    {
        heap_del((*i).second);
        delete ((*i).second);
    }
}

bool
Table::find(const std::string& dest_id, iterator* i)
{
    if (dest_id.length() < MIN_LENGTH)
    {
        LOG_LT_MIN_LENGTH;
        return false;
    }
    if (i == NULL) return false;

    *i = table_.lower_bound(dest_id);
    return (*i != table_.end() && !(table_.key_comp()(dest_id,(*i)->first)));
}

}; // namespace prophet
