#include "ContactManager.h"

/**
 * Define the singleton contact manager instance
 */
ContactManager* ContactManager::instance_ ; 

/**
 * Constructor / Destructor
 */
ContactManager::ContactManager() : Logger("/contact_manager")
{
    peers_ = new PeerSet();
    links_ = new LinkSet();
    
}

/**********************************************
 *
 * Peer set accessor functions
 *
 *********************************************/
void
ContactManager::add_peer(Peer *peer)
{
    peers_->insert(peer);
}

void
ContactManager::delete_peer(Peer *peer)
{
    if (!has_peer(peer)) {
        log_err("Error in deleting peer from contact manager.\
                 Peer %s does not exist \n",peer->name());
    } else {
        peers_->erase(peer);
    }
}

bool
ContactManager::has_peer(Peer *peer)
{
    PeerSet::iterator iter = peers_->find(peer);
    if (iter == peers_->end()) return false;
    return true;
}

/**
 * Finds peer with a given bundletuple pattern
 */
Peer*
ContactManager::find_peer(const BundleTuple& tuple)
{
    if (!tuple.valid()) {
        log_err("Trying to find an malformed peer %s\n",tuple.c_str());
        return NULL ;
    }
    
    PeerSet::iterator iter ;
    Peer* peer = NULL;
    assert(peers_ != NULL);
    for (iter = peers_->begin(); iter != peers_->end(); ++iter)
    {
        peer = *iter;
        if (peer->tuple().equals(tuple)) { return peer; }
    }
//    log_info("Returning %s when searching %s\n",
//             peer ? peer->name() : "NULL", tuple.c_str());
    return NULL ;
}


/**********************************************
 *
 * Link set accessor functions
 *
 *********************************************/
void
ContactManager::add_link(Link *link)
{
    links_->insert(link);
}

void
ContactManager::delete_link(Link *link)
{
    if (!has_link(link)) {
        log_err("Error in deleting link from contact manager.\
                 Link %s does not exist \n",link->name());
    } else {
        links_->erase(link);
    }
}

bool
ContactManager::has_link(Link *link)
{
    LinkSet::iterator iter = links_->find(link);
    if (iter == links_->end()) return false;
    return true;
}

/**
 * Finds link with a given bundletuple.
 */
Link*
ContactManager::find_link(const char* name)
{
    LinkSet::iterator iter ;
    Link* link = NULL;
    for (iter = links_->begin(); iter != links_->end(); ++iter)
    {
        link = *iter;
        if (strcasecmp(link->name(),name) == 0) return link;
    }
    return NULL ;
}

/**
 * Dump the contact manager info
 */
void
ContactManager::dump(StringBuffer* buf) const
{
    buf->append("contact manager info:\n");

    buf->append("Links::\n");
    LinkSet::iterator iter ;
    Link* link = NULL;
    for (iter = links_->begin(); iter != links_->end(); ++iter)
    {
        link = *iter;
        buf->appendf("\t link (type %s): (name) %s -> (peer) %s - (status) %s\n",
                     link->type_str(),
                     link->name(),
//                     link->clayer()->proto(),
                     link->dest_tuple()->c_str(),
                     link->isopen() ? "open" : "closed");
    }
}


