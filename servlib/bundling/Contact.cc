
#include "Contact.h"
#include "BundleList.h"

Contact::Contact(contact_type_t type, const char* name)
{
    logpathf("/contact/%s", name);
    bundle_list_ = new BundleList();
}

Contact::~Contact()
{
    ASSERT(bundle_list_->size() == 0);
    delete bundle_list_;
}
