
#include "Contact.h"
#include "Bundle.h"
#include "BundleList.h"
#include "BundleMapping.h"
#include "conv_layers/ConvergenceLayer.h"
#include "BundleForwarder.h"
#include "BundleEvent.h"

/**
 * Constructor
 */
Contact::Contact(Link* link)
    : BundleConsumer(link->dest_tuple(), false), link_(link)
{
    logpathf("/contact/%s", link->tuple().c_str());

    bundle_list_ = new BundleList(logpath_);
    contact_info_ = NULL;
    clayer()->open_contact(this);
    
    // Post a contact Available event
    BundleForwarder::post(new ContactUpEvent(this));
    log_info("new contact *%p", this);
}

Contact::~Contact()
{
    ASSERT(bundle_list_->size() == 0);
    delete bundle_list_;
}

void
Contact::enqueue_bundle(Bundle* bundle, const BundleMapping* mapping)
{
    // Add it to the queue (default behavior as defined by bundle consumer
    BundleConsumer::enqueue_bundle(bundle,mapping);
    // XXX/demmer get rid of this
    // XXX/sushant why get rid ?
    clayer()->send_bundles(this);
}

ConvergenceLayer*
Contact::clayer()
{
    return link_->clayer();
}

/**
 * Formatting...XXX Make it better
 */
int
Contact::format(char* buf, size_t sz)
{
    return link_->format(buf,sz);
}
