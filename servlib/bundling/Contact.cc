
#include "Contact.h"
#include "Bundle.h"
#include "BundleList.h"
#include "conv_layers/ConvergenceLayer.h"

Contact::Contact(contact_type_t type, const BundleTuple& tuple)
    : Logger("/contact"), BundleConsumer(&tuple_),
      type_(type), tuple_(tuple), open_(false)
{
    bundle_list_ = new BundleList("/contact/" + tuple.tuple());
    log_debug("new contact *%p", this);

    clayer_ = ConvergenceLayer::find_clayer(tuple.admin());
    if (!clayer_) {
        PANIC("can't find convergence layer for %s", tuple.admin().c_str());
        // XXX/demmer need better error handling
    }
}

Contact::~Contact()
{
    ASSERT(bundle_list_->size() == 0);
    delete bundle_list_;
}

/**
 * Open a channel to the contact for bundle transmission.
 */
void
Contact::open()
{
    log_err("XXX/demmer Contact::open not implemented");
}
    
/**
 * Close the transmission channel. May be triggered by the
 * convergence layer in case the underlying channel is torn down.
 */
void
Contact::close()
{
    log_err("XXX/demmer Contact::close not implemented");
}

int
Contact::format(char* buf, size_t sz)
{
    return snprintf(buf, sz, "%.*s %s (%s)",
                    (int)tuple_.length(), tuple_.data(),
                    contact_type_to_str(type_), open_ ? "open" : "closed");
}

void
Contact::consume_bundle(Bundle* bundle)
{
    if (!isopen() && (type_ == ONDEMAND || type_ == OPPORTUNISTIC)) {
        clayer_->open_contact(this);
        open_ = true;
    }
    
    bundle_list_->push_back(bundle);
    clayer_->send_bundles(this);
}

