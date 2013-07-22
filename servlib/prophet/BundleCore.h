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

#ifndef _PROPHET_BUNDLE_CORE_FACADE_H_
#define _PROPHET_BUNDLE_CORE_FACADE_H_

#include "Alarm.h"
#include "Node.h"
#include "Bundle.h"
#include "BundleImpl.h"
#include "BundleList.h"
#include "Link.h"
#include <cstdio>
#include <cstdarg>
#include <string>
#include <utility>
#include <list>

#if defined(__GNUC__)
# define PRINTFLIKE(fmt, arg) __attribute__((format (printf, fmt, arg)))
#else
# define PRINTFLIKE(a, b)
#endif

namespace prophet
{

/**
 * Prophet facade's abstract interface into DTN host's bundle core.
 * Prophet requires Bundle handling (create, read, write, send, find), 
 * routing functions, preferences and parameters, persistent storage,
 * timers and timeout handling, and debug logging.  BundleCore decouples
 * the Prophet facade library from its host implementation by abstracting
 * away the specifics of implmentation for these core functions.
 */
class BundleCore
{
public:

    /**
     * Destructor
     */
    virtual ~BundleCore() {}

    /**
     * Answers whether dest_id matches route
     */
    virtual bool is_route(const std::string& dest_id,
                          const std::string& route) const = 0;

    /**
     * Query the Bundle core for route status
     */
    virtual bool should_fwd(const Bundle* bundle,
                            const Link* link) const = 0;

    /**
     * Given a Bundle's destination, return the route
     */
    virtual std::string get_route(const std::string& dest_id) const = 0;

    /**
     * Given a Bundle's destination, return the route pattern
     */
    virtual std::string get_route_pattern(const std::string& dest_id) const = 0;

    /**
     * Callback method for Prophet to query storage_quota()
     */
    virtual u_int64_t max_bundle_quota() const = 0;

    /**
     * Callback method for Prophet to query whether Bundle host
     * is willing to accept custody transfers
     */
    virtual bool custody_accepted() const = 0;

    /**
     * Enumerate Bundles in host's store
     */
    virtual const BundleList& bundles() const = 0;

    /**
     * Callback method for Prophet to request for a bundle to be deleted;
     * required by Repository's evict() routine
     */
    virtual void drop_bundle(const Bundle* bundle) = 0;

    /**
     * Given a Bundle and a candidate Link, attempt to send a Bundle
     */
    virtual bool send_bundle(const Bundle* bundle,
                             const Link* link) = 0;

    /**
     * Transfer buffer into Bundle's payload
     * @param bundle bundle metadata object
     * @param buf Buffer to write into payload
     * @return success
     */
    virtual bool write_bundle(const Bundle* bundle,
                              const u_char* buf,
                              size_t len) = 0;

    /**
     * Request a Bundle's payload from Bundle host. 
     * @param bundle bundle metadata object
     * @param buffer in/out memory into which to write Bundle payload
     * @param len in/out size of inbound buffer, amount written into
     *            outbound buffer
     * @return success
     */
    virtual bool read_bundle(const Bundle* bundle,
                             u_char* buffer,
                             size_t& len) const = 0;

    /**
     * Factory method to request new Bundle from bundle host.
     * @param src Route of bundle originator
     * @param dst Route of bundle destination
     * @param exp Lifespan in seconds
     */
    virtual Bundle* create_bundle(const std::string& src,
                                  const std::string& dst,
                                  u_int exp) = 0;

    /**
     * Given a BundleList, a route, a creation ts, and a sequence number,
     * find the Bundle
     */
    virtual const Bundle* find(const BundleList& list, const std::string& eid,
                         u_int32_t creation_ts, u_int32_t seqno) const = 0;

    /**
     * Update (or create) a persistent Node to reflect handle's info
     */
    virtual void update_node(const Node* node) = 0;

    /**
     * Remove this Node from persistent storage
     */
    virtual void delete_node(const Node* node) = 0;

    /**
     * Query the local endpoint ID
     */
    virtual std::string local_eid() const = 0;

    /**
     * Query for the endpoint ID to the Prophet node on the remote end of
     * this link
     */
    virtual std::string prophet_id(const Link* link) const = 0;

    /**
     * Query for the endpoint ID to the local Prophet instance
     */
    virtual std::string prophet_id() const = 0;

    /**
     * Factory method to allocate memory for and return pointer to new
     * Alarm object that will invoke handler->handle_timeout() after
     * timeout milliseconds.
     */
    virtual Alarm* create_alarm(ExpirationHandler* handler,
                                u_int timeout, bool jitter = false) = 0;

    ///@{ Log level
    static const int LOG_DEBUG   = 1;
    static const int LOG_INFO    = 2;
    static const int LOG_NOTICE  = 3;
    static const int LOG_WARN    = 4;
    static const int LOG_ERR     = 5;
    static const int LOG_CRIT    = 6;
    static const int LOG_ALWAYS  = 7;
    ///@}

    /**
     * Defer implementation of logging to host system ... but still
     * propagate messages up to host system from within prophet namespace
     */
    virtual void print_log(const char* name, int level, const char* fmt, ...)
        PRINTFLIKE(4,5) = 0;

}; // class BundleCore

/**
 * Mock object for use in testing
 */
class AlarmImpl : public Alarm
{
public:
    AlarmImpl(ExpirationHandler* h)
        : Alarm(h), pending_(false), cancelled_(false) {}

    virtual ~AlarmImpl() {}

    void schedule(u_int) { pending_ = true; }
    u_int time_remaining() const { return 0; }
    void cancel() { cancelled_ = true; }
    bool pending() const { return pending_; }
    bool cancelled() const { return cancelled_; }
    bool pending_, cancelled_;
}; // class AlarmImpl

/**
 * Mock object for use in unit testing; this doesn't really do 
 * anything other than capture state for inspection by unit tests
 */
class BundleCoreTestImpl : public BundleCore
{
public:
    typedef std::string BundleBuffer;
    BundleCoreTestImpl(const std::string& str = "dtn://somehost")
        : str_(str), max_(0xffff) {}
    virtual ~BundleCoreTestImpl()
    {
        while (!alarms_.empty())
        {
            delete alarms_.front();
            alarms_.pop_front();
        }
    }
    ///@{ virtual from BundleCore
    bool is_route(const std::string& dest,const std::string& route) const
    {
        if (route.length() > dest.length()) return false;
        return route.compare(0,route.length(),dest) == 0;
    }
    bool should_fwd(const Bundle*,const Link*) const { return true; }
    std::string get_route(const std::string& str ) const { return str; }
    std::string get_route_pattern(const std::string& str ) const { return str + "/*"; }
    u_int64_t max_bundle_quota() const { return max_; }
    bool custody_accepted() const { return true; }
    void drop_bundle(const Bundle* b)
    {
        for (std::list<bundle>::iterator i = rcvd_.begin();
                i != rcvd_.end(); i++)
        {
            if (b->destination_id() == (*i).first->destination_id() &&
                b->creation_ts() == (*i).first->creation_ts() &&
                b->sequence_num() == (*i).first->sequence_num())
            {
                rcvd_.erase(i);
                break;
            }
        }
    }
    bool send_bundle(const Bundle* b,const Link*)
    {
        sent_.push_back(b);
        return true;
    }
    bool write_bundle(const Bundle* b,const u_char* buf,size_t len)
    { 
        BundleBuffer bunbuf((char*)buf,len);
        written_.push_back(std::make_pair(b,bunbuf));
        return written_.back().second.size() <= len;
    }
    bool read_bundle(const Bundle* b,u_char* buf,size_t& len) const
    {
        for (std::list<bundle>::const_iterator i = rcvd_.begin();
                i != rcvd_.end(); i++)
        {
            if (b->destination_id() == (*i).first->destination_id() &&
                b->creation_ts() == (*i).first->creation_ts() &&
                b->sequence_num() == (*i).first->sequence_num())
            {
                size_t blen = (*i).second.size();
                if (blen < len) return false;
                len = blen;
                memcpy(buf,(*i).second.data(),len);
                return true;
            }
        }
        return false;
    }
    Bundle* create_bundle(const std::string& src, const std::string& dst,u_int exp=3600)
    { return new BundleImpl(src,dst,0,0,exp); }
    const Bundle* find(const BundleList&,const std::string&,u_int32_t,
            u_int32_t) const
    { return NULL; }
    const BundleList& bundles() const { return list_; }
    void update_node(const Node*) {}
    void delete_node(const Node*) {}
    std::string local_eid() const { return str_; }
#define PROPHESY(_str) do { \
        size_t pos = _str.size() - 1; \
        if (_str[pos] == '/') \
            _str += "prophet"; \
        else \
            _str += "/prophet"; \
    } while (0)
    std::string prophet_id(const Link* link) const
    {
        remote_.assign(link->nexthop());
        PROPHESY(remote_);
        return remote_;
    }
    std::string prophet_id() const
    {
        if (local_ == "")
        {
            local_.assign(str_);
            PROPHESY(local_);
        }
        return local_;
    }
#undef PROPHESY
    Alarm* create_alarm(ExpirationHandler* handler, u_int timeout,bool)
    {
        AlarmImpl* alarm = new AlarmImpl(handler);
        alarm->schedule(timeout);
        alarms_.push_back(alarm);
        return alarm;
    }
    void print_log(const char* name, int level, const char* fmt, ...)
        PRINTFLIKE(4,5);

    ///@}
    void set_max(u_int64_t max) { max_ = max; }
    void set_eid(const std::string& id) { str_.assign(id); }
    std::string str_;
    mutable std::string local_, remote_;
    u_int64_t max_;
    std::list<const Bundle*> sent_;
    typedef std::pair<const Bundle*,BundleBuffer> bundle;
    std::list<bundle> written_;
    std::list<bundle> rcvd_;
    std::list<Alarm*> alarms_;
    prophet::BundleList list_;
}; // class BundleCoreTestImpl

inline void 
BundleCoreTestImpl::print_log(const char* name, int level, const char* fmt, ...)
{
    printf("[%s][%d]\n",name,level);
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
}

}; // namespace prophet

#endif // _PROPHET_BUNDLE_CORE_FACADE_H_
