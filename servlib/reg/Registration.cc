
#include "Registration.h"
#include "bundling/Bundle.h"
#include "bundling/BundleList.h"

/**
 * Constructor.
 */
Registration::Registration(u_int32_t regid,
                           const BundleTuplePattern& endpoint,
                           failure_action_t action,
                           const std::string& script,
                           time_t expiration)
    : BundleConsumer(&endpoint_, true),
      regid_(regid),
      endpoint_(endpoint),
      failure_action_(action),
      script_(script),
      expiration_(expiration),
      active_(false)
{
    logpathf("/registration/%d", regid);
    bundle_list_ = new BundleList(logpath_);
    
    if (expiration == 0) {
        // XXX/demmer default expiration
    }
    
}

/**
 * Destructor.
 */
Registration::~Registration()
{
    // XXX/demmer loop through bundle list and remove refcounts
    NOTIMPLEMENTED;
    delete bundle_list_;
}
    
/**
 * Consume a bundle for the registered endpoint.
 */
void
Registration::consume_bundle(Bundle* bundle)
{
    log_info("queue bundle id %d for delivery to registration %d (%s)",
             bundle->bundleid_, regid_, endpoint_.c_str());
    bundle_list_->push_back(bundle);
}
 
/**
 * Virtual from SerializableObject.
 */
void
Registration::serialize(SerializeAction* a)
{
    a->process("endpoint", &endpoint_);
    a->process("regid", &regid_);
    a->process("action", (u_int32_t*)&failure_action_);
    a->process("script", &script_);
    a->process("expiration", (u_int32_t*)&expiration_);
}
