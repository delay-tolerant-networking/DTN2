
#include "BundleConsumer.h"
#include "Bundle.h"
#include "BundleList.h"
#include "BundleMapping.h"

BundleConsumer::BundleConsumer(const BundleTuple* dest_tuple, bool is_local)
    : dest_tuple_(dest_tuple),
      is_local_(is_local),
      bundle_list_(NULL)
{
    logpathf("/bundle/consumer/%s", dest_tuple->tuple().c_str());
}

void
BundleConsumer::enqueue_bundle(Bundle* bundle, const BundleMapping* mapping)
{
    log_info("enqueue bundle id %d for delivery to %s",
             bundle->bundleid_, dest_tuple_->c_str());
    bundle_list_->push_back(bundle, mapping);
}


bool
BundleConsumer::dequeue_bundle(Bundle* bundle, BundleMapping** mappingp)
{
    log_info("dequeue bundle id %d from %s",
             bundle->bundleid_, dest_tuple_->c_str());
    
    BundleMapping* mapping = bundle->get_mapping(bundle_list_);

    if (!mapping)
        return false;
    
    bundle_list_->erase(mapping->position_, mappingp);
    return true;
}

/**
 * Check if the given bundle is already queued on this consumer.
 */
bool
BundleConsumer::is_queued(Bundle* bundle)
{
    return (bundle->get_mapping(bundle_list_) != NULL);
}

