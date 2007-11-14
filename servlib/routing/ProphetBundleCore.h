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

#ifndef _PROPHET_BUNDLE_CORE_H_
#define _PROPHET_BUNDLE_CORE_H_

#include "prophet/Node.h"
#include "prophet/Params.h"
#include "prophet/BundleList.h"
#include "prophet/BundleCore.h"
#include "prophet/Repository.h"
#include "contacts/Link.h"
#include "bundling/Bundle.h"
#include "bundling/BundleActions.h"
#include "bundling/BundleList.h"
#include "ProphetBundleList.h"
#include "ProphetLinkList.h"
#include "ProphetNodeList.h"
#include <oasys/debug/Log.h>
#include <oasys/thread/Lock.h>

#include <string>

namespace dtn
{

/**
 * Implements the prophet::BundleCore API by integrating
 * DTN's system services
 */
class ProphetBundleCore : public prophet::BundleCore,
                          public prophet::Repository::BundleCoreRep,
                          public oasys::Logger
{
public:

    /**
     * Constructor
     */
    ProphetBundleCore(const std::string& local_eid,
                      BundleActions* actions,
                      oasys::SpinLock* lock);

    /**
     * Test constructor
     */
    ProphetBundleCore(oasys::Builder);

    /**
     * Destructor
     */
    virtual ~ProphetBundleCore();

    ///@{ Virtual from prophet::BundleCore
    bool is_route(const std::string& dest_id,
                  const std::string& route) const;
    bool should_fwd(const prophet::Bundle* bundle,
                    const prophet::Link* link) const;
    std::string get_route(const std::string& dest_id) const;
    std::string get_route_pattern(const std::string& dest_id) const;
    u_int64_t max_bundle_quota() const;
    bool custody_accepted() const;
    void drop_bundle(const prophet::Bundle* bundle);
    bool send_bundle(const prophet::Bundle* bundle,
                     const prophet::Link* link);
    bool write_bundle(const prophet::Bundle* bundle,
                      const u_char* buffer,
                      size_t len);
    bool read_bundle(const prophet::Bundle* bundle,
                     u_char* buffer,
                     size_t& len) const
    {
        return const_cast<ProphetBundleCore*>(this)->read_bundle(
                bundle,buffer,len);
    }
    prophet::Bundle* create_bundle(const std::string& src,
                                   const std::string& dst,
                                   u_int expiration);
    const prophet::BundleList& bundles() const
    {
        return bundles_.get_bundles();
    }
    const prophet::Bundle* find(const prophet::BundleList& list,
            const std::string& eid, u_int32_t creation_ts,
            u_int32_t seqno) const;
    void update_node(const prophet::Node* node);
    void delete_node(const prophet::Node* node);
    std::string local_eid() const
    { 
        return local_eid_;
    }
    std::string prophet_id(const prophet::Link* link) const;
    std::string prophet_id() const
    {
        EndpointID eid(get_route(local_eid()));
        eid.append_service_tag("prophet");
        return eid.str();
    }
    prophet::Alarm* create_alarm(prophet::ExpirationHandler* handler,
                                 u_int timeout, bool jitter = false);
    void print_log(const char* name, int level, const char* fmt, ...)
        PRINTFLIKE(4,5);
    ///@}

    /**
     * Implementation trick to get around const issues
     */
    bool read_bundle(const prophet::Bundle* bundle,
                     u_char* buffer,
                     size_t& len);

    /**
     * Initialization routine for deserializing routes from
     * permanent storage
     */
    void load_prophet_nodes(prophet::Table* nodes,
                            prophet::ProphetParams* params);

    /**
     * Initialization routine for loading Bundle metadata
     * into Prophet's facade
     */
    void load_dtn_bundles(const BundleList* list);

    /**
     * Callback for host system's shutdown routine
     */
    void shutdown()
    {
        bundles_.clear();
        links_.clear();
        nodes_.clear();
    }

    ///@{ Conversion between Prophet's Facade type and DTN native type
    const Bundle* get_bundle(const prophet::Bundle* b);
    const prophet::Bundle* get_bundle(const Bundle* b);
    const prophet::Bundle* get_temp_bundle(const BundleRef& b);
    const Link* get_link(const prophet::Link* link);
    const prophet::Link* get_link(const Link* link);
    ///@}

    ///@{ Convenience method for insert/delete into Prophet's
    ///   BundleCore facade
    void add(const BundleRef& b);
    void del(const BundleRef& b);
    void add(const LinkRef& link);
    void del(const LinkRef& link);
    ///@}

    /**
     * Prophet's queue policy implementation
     */
    prophet::Repository* bundles() { return bundles_.bundles(); }

protected:
    friend class ProphetRouter;

    BundleActions* const actions_; ///< actions interface for send, delete, etc
    ProphetBundleList bundles_; ///< objects that link DTN to Prophet bundles
    ProphetLinkList links_; ///< objects that link DTN to Prophet links
    ProphetNodeList nodes_; ///< interface into persistent storage
    const std::string local_eid_; ///< route to local DTN instance
    oasys::SpinLock* const lock_; ///< shared lock with ProphetRouter
    bool test_mode_; ///< test constructor used, meaning that BundleDaemon is
                     ///  unavailable

}; // class ProphetBundleCore

}; // namespace dtn

#endif // _PROPHET_BUNDLE_CORE_H_
