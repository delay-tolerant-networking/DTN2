
#include "Peer.h"
#include "Bundle.h"
#include "BundleList.h"

/**
 * Constructor / Destructor
 */

Peer::Peer(const BundleTuple& tuple)
    : BundleConsumer(&tuple_, false), tuple_(tuple)
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
                    (int)tuple().length(), tuple().data()
                    );
}


// /**
//  * Consume bundle is called by the monster routing logic.
//  * The goal is to put the message on the bundle queue.
//  * The router will at some later point take the message off the
//  * list and put in on a contact. In that respect a Peer
//  * is not a real bundle consumer and only a stoage place
//  */

// void
// Peer::consume_bundle(Bundle* bundle)
// {
//     if (bundle) {
//         if (bundle->is_reactive_fragment_)
//             bundle_list_->push_front(bundle);
//         else 
//             bundle_list_->push_back(bundle);
//     }    
// }

// /**
//  * Check if the given bundle is already queued on this consumer.
//  */
// bool
// Peer::is_queued(Bundle* bundle)
// {
//     return bundle->has_container(bundle_list_);
// }


bool
Peer::has_link(Link *link)
{
    LinkSet::iterator iter = links_->find(link);
    if (iter == links_->end()) return false;
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
        log_err("Error in deleting link from peer.\
                     Link %s does not exist on peer %s \n",link->name(),name());
    } else {
        links_->erase(link);
    }
}
