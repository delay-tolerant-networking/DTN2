
#include "Peer.h"
#include "Bundle.h"
#include "BundleList.h"

/**
 * Constructor / Destructor
 */
Peer::Peer(const BundleTuple& tuple)
    : BundleConsumer(&tuple_, false, "Peer"), tuple_(tuple)
{
    ASSERT(tuple.valid());
    logpathf("/peer/%s",tuple.c_str());
    bundle_list_ = new BundleList(logpath_);
    log_debug("new peer *%p", this);
    links_ = new LinkSet();
}

Peer::~Peer()
{
    ASSERT(bundle_list_->size() == 0);
    delete bundle_list_;
}

int
Peer::format(char* buf, size_t sz)
{
    return snprintf(buf, sz, "Peer %.*s",
                    (int)tuple().length(), tuple().data());
}

bool
Peer::has_link(Link *link)
{
    LinkSet::iterator iter = links_->find(link);
    if (iter == links_->end())
        return false;
    return true;
}

void
Peer::add_link(Link *link)
{
    links_->insert(link);
}

void
Peer::delete_link(Link *link)
{
    if (!has_link(link)) {
        log_err("Error in deleting link from peer -- "
                "link %s does not exist on peer %s",
                link->name(), name());
    } else {
        links_->erase(link);
    }
}
