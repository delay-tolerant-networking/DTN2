
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

    BundleStore::instance()->insert(bundle);

    // XXX/demmer temporarily take a local reference. this should go
    // away so instead, the bundle should be on an unassigned list or
    // some such entity
    bundle->add_ref();

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
    
    bundle->del_ref();
}
