#include "ScheduledLink.h"

/**
 * Constructor / Destructor
 */
ScheduledLink::ScheduledLink(std::string name, const char* conv_layer,
              const BundleTuple& tuple)
    : Link(name,SCHEDULED, conv_layer,tuple)
{
    fcts_ = new FutureContactSet();
    //  bundle_list_ = new BundleList(logpath_);
}

ScheduledLink::~ScheduledLink()
{
}

/**
 * Add a future contact
 */
void
ScheduledLink::add_fc(FutureContact* fc)
{
    fcts_->insert(fc);
}

/**
 * Delete a future contact
 */
void
ScheduledLink::delete_fc(FutureContact* fc)
{
      fcts_->erase(fc);
}

/**
 * Convert a future contact to an "active contact"
 */
void
ScheduledLink::convert_fc(FutureContact* fc)
{
    NOTIMPLEMENTED ;
}
