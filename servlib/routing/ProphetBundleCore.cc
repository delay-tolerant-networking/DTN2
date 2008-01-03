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

#include "naming/EndpointID.h"
#include "storage/BundleStore.h"
#include "bundling/BundleTimestamp.h"
#include "bundling/BundleDaemon.h"
#include "ProphetTimer.h"
#include <oasys/util/Random.h>

#include "BundleRouter.h"
#include "ProphetBundleCore.h"

#define LOG(_args...) print_log("core",LOG_DEBUG,_args);

#define TEST_MODE_QUOTA 0xffffffff

namespace dtn
{

ProphetBundleCore::ProphetBundleCore(const std::string& local_eid,
                                     BundleActions* actions,
                                     oasys::SpinLock* lock)
    : oasys::Logger("ProphetBundleCore","/dtn/route/prophet"),
      actions_(actions),
      bundles_(this),
      local_eid_(local_eid),
      lock_(lock),
      test_mode_(false)
{
    ASSERT(actions_ != NULL);
    oasys::Random::seed(time(0));
}

ProphetBundleCore::ProphetBundleCore(oasys::Builder)
    : oasys::Logger("ProphetBundleCore","/dtn/route/prophet/test"),
      actions_(NULL),
      bundles_(this),
      lock_(NULL)
{
    test_mode_ = true;
}

void
ProphetBundleCore::load_dtn_bundles(const BundleList* bundles)
{
    ASSERT(bundles != NULL);
    oasys::ScopeLock l(bundles->lock(),"ProphetBundleCore::constructor");
    for (BundleList::iterator i = bundles->begin();
            i != bundles->end(); i++)
    {
        BundleRef ref("ProphetBundleCore");
        ref = *i;
        add(ref);
    }
    LOG("added %zu bundles to core",bundles->size());
}

ProphetBundleCore::~ProphetBundleCore()
{
}

bool
ProphetBundleCore::should_fwd(const prophet::Bundle* b,
                              const prophet::Link* link) const
{
    // convert Facade objects into DTN objects
    BundleRef bundle("should_fwd");
    bundle = bundles_.find_ref(b).object();
    LinkRef nexthop("should_fwd");
    nexthop = links_.find_ref(link).object();
    ProphetBundleCore* me = const_cast<ProphetBundleCore*>(this);
    // if DTN objects are non-NULL ...
    if (bundle.object() != NULL && nexthop.object() != NULL)
    {
        if (test_mode_) return true;
        BundleRouter* router = BundleDaemon::instance()->router();
        bool ok = false;
        if (router != NULL)
        {
            ok = router->should_fwd(bundle.object(),nexthop);
            if (me != NULL) 
                me->print_log("core",LOG_DEBUG,
                          "BundleRouter says%sok to fwd *%p",
                          ok ? " " : " not ", bundle.object());
        }
        return ok;
    } 
    else
    {
        if (me != NULL) 
            me->print_log("core",LOG_DEBUG,
                    "failed to convert prophet handle to DTN object");
    }
    return false;
}

bool
ProphetBundleCore::is_route(const std::string& dest_id,
                            const std::string& route) const
{
    EndpointIDPattern routeid(get_route_pattern(route));
    return routeid.match(EndpointID(dest_id));
}

std::string
ProphetBundleCore::get_route_pattern(const std::string& dest_id) const
{
    std::string routeid = get_route(dest_id);
    EndpointIDPattern route(routeid);
    if (!route.append_service_wildcard())
        return "";
    return route.str();
}

std::string
ProphetBundleCore::get_route(const std::string& dest_id) const
{
    EndpointID eid(dest_id);
    // reduce all but the hostname part of the original dest_id
    if (!eid.remove_service_tag())
        return "";
    return eid.str();
}

u_int64_t
ProphetBundleCore::max_bundle_quota() const
{
    if (test_mode_) return TEST_MODE_QUOTA;
    return BundleStore::instance()->payload_quota();
}

bool
ProphetBundleCore::custody_accepted() const
{
    if (test_mode_) return false;
    return BundleDaemon::params_.accept_custody_;
}

void
ProphetBundleCore::drop_bundle(const prophet::Bundle* bundle)
{
    if (bundle == NULL) 
    {
        LOG("drop_bundle(NULL)");
        return;
    }
    else
        LOG("drop_bundle(%d)",bundle->sequence_num());

    // translate Facade wrapper into DTN::Bundle
    const Bundle* dtn_b = get_bundle(bundle); 
    if (dtn_b == NULL)
    {
        log_err("drop_bundle: failed to convert prophet handle %p to "
                "dtn bundle", bundle);
        return;
    }

    // un-const the pointer
    Bundle* b = const_cast<Bundle*>(dtn_b);

    if (b == NULL)
    {
        log_err("drop_bundle: const cast failed");
        return;
    }

    log_debug("drop bundle *%p",b);
    // remove from Prophet repository
    bundles_.del(bundle);
    // request deletion from other ref holders
    actions_->delete_bundle(b, BundleProtocol::REASON_NO_ADDTL_INFO);
}

bool
ProphetBundleCore::send_bundle(const prophet::Bundle* bundle,
                               const prophet::Link* link)
{
    LOG("send_bundle(%u)",bundle == NULL ? 0 : bundle->sequence_num());
    Link* l = const_cast<Link*>(get_link(link));
    if (l == NULL)
    {
        log_err("failed to convert prophet handle for link (%s)",
                link->remote_eid());
        return false;
    }
    Bundle* b = const_cast<Bundle*>(get_bundle(bundle));
    if (b == NULL)
    {
        log_err("failed to convert prophet handle for bundle (%s, %u)",
                bundle->destination_id().c_str(),bundle->sequence_num());
        return false;
    }
    LinkRef nexthop("send_bundle");
    nexthop = l;

    // if link is available but not open, open it
    if (nexthop->isavailable() &&
        (!nexthop->isopen()) && (!nexthop->isopening()))
    {
        log_debug("opening *%p because a message is intended for it",
                  nexthop.object());
        actions_->open_link(nexthop);
    }

    // we can only send if link is open and not full
    if (nexthop->isopen() && !nexthop->queue_is_full())
    {
        log_debug("send bundle *%p over link *%p", b, l);
        actions_->queue_bundle(b, nexthop,
                               ForwardingInfo::COPY_ACTION,
                               CustodyTimerSpec());
        return true;
    }

    // since we can't send the bundle right now, log why
    if (!nexthop->isavailable())
        log_debug("can't forward *%p to *%p because link not available",
                  b,nexthop.object());
    else if (!nexthop->isopen())
        log_debug("can't forward *%p to *%p because link not open",
                  b,nexthop.object());
    else if (nexthop->queue_is_full())
        log_debug("can't forward *%p to *%p because link queue is full",
                  b, nexthop.object());
    else
        log_debug("can't forward *%p to *%p", b,nexthop.object());

    return false;
}

bool
ProphetBundleCore::write_bundle(const prophet::Bundle* bundle,
        const u_char* buffer, size_t len)
{
    LOG("write_bundle(%u)",bundle == NULL ? 0 : bundle->sequence_num());
    Bundle* b = const_cast<Bundle*>(get_bundle(bundle));
    if (b != NULL)
    {
        b->mutable_payload()->append_data(buffer,len);
        return true;
    }
    return false;
}

bool
ProphetBundleCore::read_bundle(const prophet::Bundle* bundle,
        u_char* buffer, size_t& len)
{
    LOG("read_bundle(%u)",bundle == NULL ? 0 : bundle->sequence_num());
    const Bundle* b = get_bundle(bundle);
    if (b != NULL)
    {
        size_t blen = b->payload().length();
        if (blen < len)
            return false;
        b->payload().read_data(0,blen,buffer);
        len = blen;
        return true;
    }
    return false;
}

prophet::Bundle*
ProphetBundleCore::create_bundle(const std::string& src,
        const std::string& dst, u_int expiration)
{
    LOG("create_bundle");
    Bundle* b = new Bundle();
    b->mutable_source()->assign(src);
    b->mutable_dest()->assign(dst);
    b->mutable_replyto()->assign(EndpointID::NULL_EID());
    b->mutable_custodian()->assign(EndpointID::NULL_EID());
    b->set_expiration(expiration);
    BundleRef tmp("create_bundle");
    tmp = b;
    add(tmp);
    return const_cast<prophet::Bundle*>(get_bundle(b));
}

const prophet::Bundle*
ProphetBundleCore::find(const prophet::BundleList& list,
        const std::string& eid, u_int32_t creation_ts, u_int32_t seqno) const
{
    for (prophet::BundleList::const_iterator i = list.begin();
            i != list.end(); i++)
    {
        if ((*i)->creation_ts() == creation_ts &&
            (*i)->sequence_num() == seqno &&
            is_route((*i)->destination_id(), eid))  return *i;
    }
    return NULL;
}

void
ProphetBundleCore::load_prophet_nodes(prophet::Table* nodes,
                                      prophet::ProphetParams* params)
{
    ASSERTF(nodes_.empty(), "load_prophet_nodes called more than once");

    prophet::Node* node = NULL;
    ProphetStore* prophet_store = ProphetStore::instance();
    ProphetStore::iterator* iter = prophet_store->new_iterator();

    log_notice("loading prophet nodes from data store");

    for (iter->begin(); iter->more(); iter->next())
    {
        node = prophet_store->get(iter->cur_val());
        if (node == NULL)
        {
            log_err("failed to deserialize Prophet route for %s",
                iter->cur_val().c_str());
            continue;
        }
        nodes_.load(node);
    }
    delete iter;

    // push local list into Table
    nodes_.clone(nodes,params);

    // calculate age, now that they're off disk, into memory
    nodes->age_nodes();

    // trim whatever's below minimum
    nodes->truncate(params->epsilon());
    log_notice("prophet nodes loaded");
}

void
ProphetBundleCore::update_node(const prophet::Node* node)
{
    LOG("update_node");
    nodes_.update(node);
}

void
ProphetBundleCore::delete_node(const prophet::Node* node)
{
    LOG("delete_node");
    nodes_.del(node);
}

std::string
ProphetBundleCore::prophet_id(const prophet::Link* link) const
{
    if (link == NULL) return "";
    std::string remote_eid(link->remote_eid());
    EndpointID eid(remote_eid.c_str());
    eid.append_service_tag("prophet");
    ASSERT( eid.str().find("prophet") != std::string::npos );
    return eid.str();
}

prophet::Alarm*
ProphetBundleCore::create_alarm(prophet::ExpirationHandler* handler,
        u_int timeout, bool jitter)
{
    LOG("create_alarm (%u)",timeout);
    if (handler == NULL) return NULL;
    ProphetTimer* alarm = new ProphetTimer(handler,lock_);
    if (jitter)
    {
        // mix up jitter on timeout to roughly +/- 5%
        // by subtracting 6.25% and adding rand (0 .. 12.5)%
        u_int twelve_pct = timeout >> 3; // div 8
        u_int six_pct = timeout >> 4; // div 16
        u_int zero_to_twelve = oasys::Random::rand(twelve_pct);
        timeout = timeout - six_pct + zero_to_twelve;
    }
    alarm->schedule(timeout);
    log_debug("scheduling alarm %s for %u ms",handler->name(),timeout);
    return alarm;
}

void
ProphetBundleCore::print_log(const char* name,
        int level, const char* fmt, ...)
{
    // force arbitrary int into enumerated type
    oasys::log_level_t l_int = (oasys::log_level_t)level;
    oasys::log_level_t log_level =
    (l_int >= oasys::LOG_DEBUG && l_int <= oasys::LOG_ALWAYS) ?
        l_int : oasys::LOG_DEBUG;

    // hack the logpath
    std::string prev(logpath_);
    if (name[0] != '/')
        logpathf("%s/%s",logpath_,name);
    else
        logpathf("%s%s",logpath_,name);

    // submit log to Logger
    va_list ap;
    va_start(ap, fmt);
    vlogf(log_level,fmt,ap);
    va_end(ap);

    // unhack the logpath
    set_logpath(prev.c_str());
}

const Bundle*
ProphetBundleCore::get_bundle(const prophet::Bundle* bundle)
{
    LOG("get_bundle prophet -> DTN");
    if (bundle == NULL) return NULL;
    BundleRef tmp("get_bundle");
    tmp = bundles_.find_ref(bundle);
    return tmp.object();
}

const prophet::Bundle*
ProphetBundleCore::get_bundle(const Bundle* b)
{
    LOG("get_bundle DTN -> prophet");
    if (b == NULL) return NULL;
    return bundles_.find(b);
}

const prophet::Bundle*
ProphetBundleCore::get_temp_bundle(const BundleRef& b)
{
    LOG("get_temp_bundle DTN -> prophet");
    if (b.object() == NULL) return NULL;
    return new ProphetBundle(b);
}

const Link*
ProphetBundleCore::get_link(const prophet::Link* link)
{
    LOG("get_link prophet -> DTN");
    if (link == NULL) return NULL;
    LinkRef tmp("get_link");
    tmp = links_.find_ref(link->remote_eid());
    return tmp.object();
}

const prophet::Link*
ProphetBundleCore::get_link(const Link* link)
{
    LOG("get_link DTN -> prophet");
    Link* l = const_cast<Link*>(link);
    if (l == NULL) return NULL;
    return links_.find(l->remote_eid().c_str());
}

void
ProphetBundleCore::add(const BundleRef& b)
{
    LOG("add(bundle)");
    bundles_.add(b);
}

void
ProphetBundleCore::del(const BundleRef& b)
{
    LOG("del(bundle)");
    bundles_.del(b);
}

void
ProphetBundleCore::add(const LinkRef& link)
{
    LOG("add(link)");
    links_.add(link);
}

void
ProphetBundleCore::del(const LinkRef& link)
{
    LOG("del(link)");
    links_.del(link);
}

}; // namespace dtn
