
#include "Contact.h"
#include "BundleList.h"
#include "conv_layers/ConvergenceLayer.h"

Contact::Contact(contact_type_t type, const BundleTuple& tuple)
    : Logger("/contact"), type_(type), tuple_(tuple), open_(false)
{
    bundle_list_ = new BundleList();
    log_debug("new *%p", this);

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

int
Contact::format(char* buf, size_t sz)
{
    return snprintf(buf, sz, "contact %s %.*s (%s)", contact_type_to_str(type_),
                    tuple_.length(), tuple_.data(), open_ ? "open" : "closed");
}

