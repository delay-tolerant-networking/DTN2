
#include "Registration.h"
#include "RegistrationTable.h"
#include "bundling/Bundle.h"
#include "bundling/BundleList.h"
#include "bundling/BundleMapping.h"
#include "storage/GlobalStore.h"

void
Registration::init(u_int32_t regid,
                   const BundleTuplePattern& endpoint,
                   failure_action_t action,
                   time_t expiration,
                   const std::string& script)
{
    regid_ = regid;
    endpoint_.assign(endpoint);
    failure_action_ = action;
    script_.assign(script);
    expiration_ = expiration;
    active_ = false;

    logpathf("/registration/%d", regid);
    
    bundle_list_ = new BundleList(logpath_);
    
    if (expiration == 0) {
        // XXX/demmer default expiration
    }

    if (! RegistrationTable::instance()->add(this)) {
        log_err("unexpected error adding registration to table");
    }
}

/**
 * Constructor.
 */
Registration::Registration(u_int32_t regid)
    : BundleConsumer(&endpoint_, true, "Reg")
{
    regid_ = regid;
}

/**
 * Constructor.
 */
Registration::Registration(const BundleTuplePattern& endpoint,
                           failure_action_t action,
                           time_t expiration,
                           const std::string& script)
    
    : BundleConsumer(&endpoint_, true, "Reg")
{
    init(GlobalStore::instance()->next_regid(),
         endpoint, action, expiration, script);
}

Registration::Registration(u_int32_t regid,
                           const BundleTuplePattern& endpoint,
                           failure_action_t action,
                           time_t expiration,
                           const std::string& script)
    
    : BundleConsumer(&endpoint_, true, "Reg")
{
    init(regid, endpoint, action, expiration, script);
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
