
#include "Bundle.h"
#include "BundleList.h"
#include "BundleForwarding.h"
#include "Contact.h"
#include "reg/RegistrationTable.h"
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
    int count;
    RegistrationList reglist;
    RegistrationList::iterator reg;
    Contact* next_hop;
    
    log_debug("input *%p", bundle);

    BundleStore::instance()->put(bundle, bundle->bundleid_);

    // check for matching registrations
    count = RegistrationTable::instance()->get_matching(bundle->dest_, &reglist);
    log_debug("forwarding to %d matching registrations", count);
    for (reg = reglist.begin(); reg != reglist.end(); ++reg) {
        (*reg)->consume_bundle(bundle);
    }
    
    next_hop = RouteTable::instance()->next_hop(bundle);

    if (next_hop) {
        log_debug("next hop *%p", next_hop);
        next_hop->consume_bundle(bundle);
    } else {
        log_debug("no next hop, storing bundle");
    }
}
