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

#ifndef _PROPHET_CONTROLLER_H_
#define _PROPHET_CONTROLLER_H_

#include <string>
#include <list>

#include "Link.h"
#include "Bundle.h"
#include "Alarm.h"
#include "Oracle.h"
#include "BundleCore.h"
#include "Encounter.h"

namespace prophet
{

class Controller : public Oracle,
                   public ExpirationHandler
{
public:
    typedef std::list<Encounter*> List;

    /**
     * Constructor
     */
    Controller(BundleCore* core, Repository* repository,
               ProphetParams* params);

    /**
     * Destructor
     */
    virtual ~Controller(); 

    /**
     * Signal Prophet that a new neighbor has been discovered
     */
    void new_neighbor(const Link* link);

    /**
     * Signal Prophet that contact with neighbor is lost 
     */
    void neighbor_gone(const Link* link);

    /**
     * Query whether Bundle should be accepted
     */
    bool accept_bundle(const Bundle* bundle);

    /**
     * Add Bundle to Prophet storage
     */
    void handle_bundle_received(const Bundle* b, const Link* link);

    /**
     * Update Prophet statistics on successful transmission
     */
    void handle_bundle_transmitted(const Bundle* b, const Link* link);

    /**
     * Add Prophet ACK for bundle
     */
    void ack(const Bundle* b);

    /**
     * Set Prophet's storage policy (query params_ for change)
     */
    void set_queue_policy();

    /**
     * Set Hello interval (longest acceptable delay in peering; 
     * query params_ for change)
     */
    void set_hello_interval();

    /**
     * Callback for runtime changes made to
     * ProphetParams::max_table_size_
     */
    void set_max_route();

    /**
     * Callback for host system to notify of shutdown event
     */
    void shutdown();

    ///@{ virtual from Oracle
    const ProphetParams* params() const { return params_; }
    Stats*               stats()        { return &stats_; }
    Table*               nodes()        { return &nodes_; }
    AckList*             acks()         { return &acks_; }
    BundleCore*          core()         { return core_; }
    ///@}

    ///@{ Iterators over Encounter list ... not thread safe
    const List::const_iterator begin() const { return list_.begin(); }
    const List::const_iterator end() const { return list_.end(); }
    ///@}

    ///@{ Accessors
    size_t size() { return list_.size(); }
    bool empty() { return list_.empty(); }
    ///@}

    /**
     * Virtual from ExpirationHandler
     */
    void handle_timeout();

protected:

    /**
     * Search for Encounter based on Link properties
     */
    bool find(const Link* link, List::iterator& i);

    /**
     * Check Bundle destination for Prophet node endpoint ID
     */
    bool is_prophet_control(const Bundle* b) const;

    BundleCore* core_; ///< facade interface for interacting with Bundle host
    ProphetParams* params_; ///< tuneable parameter struct
    Stats stats_; ///< forwarding statistics per bundle, used by queue policy
    u_int max_route_; ///< upper limit to number of routes retained by Prophet
    Table nodes_; ///< table of routes learned by local node
    AckList acks_; ///< Prophet ack's for successfully delivered bundles
    u_int16_t next_instance_; ///< used to generate serial number for
                              ///  Encounters
    Alarm* alarm_; ///< timeout handler for aging Nodes and Acks
    u_int timeout_; ///< protocol timeout value
                    /// (mirrors params_->hello_interval_)
    Repository* repository_; ///< Prophet's queue policy enforcement
    List list_; ///< active Prophet peering sessions
}; // class Controller

}; // namespace prophet

#endif // _PROPHET_CONTROLLER_H_
