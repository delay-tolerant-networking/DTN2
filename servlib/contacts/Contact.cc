
#include "Contact.h"
#include "BundleList.h"

Contact::Contact(contact_type_t type, const BundleTuple& tuple)
    : type_(type), tuple_(tuple)
{
    logpathf("/contact/%s", tuple.c_str());
    bundle_list_ = new BundleList();
}

Contact::~Contact()
{
    ASSERT(bundle_list_->size() == 0);
    delete bundle_list_;
}

int
Contact::format(char* buf, size_t sz)
{
    return snprintf(buf, sz, "contact %s %.*s", contact_type_toa(type_),
                    tuple_.length(), tuple_.data());
}
