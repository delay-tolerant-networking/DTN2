
#include "Bundle.h"
#include "BundleList.h"
#include "BundleForwarding.h"
#include "Contact.h"
#include "routing/RouteTable.h"
#include "storage/BundleStore.h"

BundleForwarding* BundleForwarding::instance_ = NULL;

BundleForwarding::BundleForwarding()
    : Logger("/bundlefwd")
{
}

void
BundleForwarding::init()
{
    instance_ = new BundleForwarding();
}

void
BundleForwarding::input(Bundle* bundle)
{
    Contact* next_hop;
    log_debug("input *%p", bundle);

    BundleStore::instance()->put(bundle, bundle->bundleid_);

    next_hop = RouteTable::instance()->next_hop(bundle);

    if (next_hop) {
        log_debug("next hop *%p", next_hop);
        next_hop->bundle_list()->push_back(bundle);
    } else {
        log_debug("no next hop, storing bundle");
    }
}
