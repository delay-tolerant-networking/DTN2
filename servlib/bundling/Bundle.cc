
#include "Bundle.h"
#include "thread/SpinLock.h"

void
Bundle::init()
{
    refcount_		= 0;
    bundleid_		= 0;
    priority_		= COS_NORMAL;
    expiration_		= 0;
    custreq_		= false;
    custody_rcpt_	= false;
    recv_rcpt_		= false;
    fwd_rcpt_		= false;
    return_rcpt_	= false;
    expiration_		= 0; // XXX/demmer
    
    gettimeofday(&creation_ts_, 0);
}

Bundle::Bundle()
{
    init();
}

Bundle::Bundle(const std::string& source,
               const std::string& dest,
               const std::string& payload)
{
    init();
    source_.set_tuple(source);
    replyto_.set_tuple(source);
    custodian_.set_tuple(source);
    dest_.set_tuple(dest);
    payload_.set_data(payload);
}

Bundle::~Bundle()
{
    ASSERT(containers_.size() == 0);
    // XXX/demmer remove the bundle from the database
    
    bundleid_ = 0xdeadf00d;
    payload_.set_data("(dead bundle data");
}

int
Bundle::format(char* buf, size_t sz)
{
    return snprintf(buf, sz, "bundle id %d %s -> %s (%d bytes payload)",
                    bundleid_, source_.c_str(), dest_.c_str(),
                    payload_.length());
}

void
Bundle::serialize(SerializeAction* a)
{
    a->process("test_binary",test_binary_,10);

    a->process("bundleid", &bundleid_);
    a->process("source", &source_);
    a->process("dest", &dest_);
    a->process("custodian", &custodian_);
    a->process("replyto", &replyto_);
    a->process("priority", &priority_);
    a->process("custreq", &custreq_);
    a->process("custody_rcpt", &custody_rcpt_);
    a->process("recv_rcpt", &recv_rcpt_);
    a->process("fwd_rcpt", &fwd_rcpt_);
    a->process("return_rcpt", &return_rcpt_);
    a->process("creation_ts_sec",  (u_int32_t*)&creation_ts_.tv_sec);
    a->process("creation_ts_usec", (u_int32_t*)&creation_ts_.tv_usec);
    a->process("expiration", &expiration_);
    a->process("payload", &payload_);
}

/**
 * Bump up the reference count.
 */
int
Bundle::add_ref()
{
    lock_.lock();
    ASSERT(refcount_ >= 0);
    int ret = ++refcount_;
    log_debug("/bundle/refs", "bundle id %d: refcount %d -> %d (%d containers)",
              bundleid_, refcount_ - 1, refcount_, containers_.size());
    lock_.unlock();
    return ret;
}

/**
 * Decrement the reference count.
 *
 * If the reference count becomes zero, the bundle is deleted.
 */
int
Bundle::del_ref()
{
    lock_.lock();
    ASSERT(refcount_ > 0);
    int ret = --refcount_;
    log_debug("/bundle/refs", "bundle id %d: refcount %d -> %d (%d containers)",
              bundleid_, refcount_ + 1, refcount_, containers_.size());
    if (refcount_ != 0) {
        lock_.unlock();
        return ret;
    }

    delete this;
    return 0;
}

/**
 * Add a BundleList to the set of containers.
 *
 * @return true if the list pointer was added successfully, false
 * if it was already in the set
 */
bool
Bundle::add_container(BundleList *blist)
{
    ScopeLock l(&lock_);
    log_debug("/bundle/container", "bundle id %d add container %p",
              bundleid_, blist);
    if (containers_.insert(blist).second == true) {
        return true;
    }
    return false;
}


/**
 * Remove a BundleList from the set of containers.
 *
 * @return true if the list pointer was removed successfully,
 * false if it wasn't in the set
 */
bool
Bundle::del_container(BundleList* blist)
{
    ScopeLock l(&lock_);
    log_debug("/bundle/container", "bundle id %d del container %p",
              bundleid_, blist);
    
    size_t n = containers_.erase(blist);
    if (n == 1) {
        return true;
    } else if (n == 0) {
        return false;
    } else {
        PANIC("unexpected return %d from set::erase", n);
    }
}
