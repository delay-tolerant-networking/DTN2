#ifndef _PEER_H_
#define _PEER_H_

#include <set>
#include <oasys/debug/Formatter.h>
#include <oasys/debug/Debug.h>
#include <oasys/util/StringBuffer.h>
#include "BundleConsumer.h"
#include "BundleTuple.h"
#include "Link.h"

class Link;
class LinkSet;
class Peer;

/**
 * Set of peers
 */
class PeerSet : public std::set<Peer*> {};

/**
 * Encapsulation of a next-hop DTN node. The object
 * contains a list of incoming links and a list of bundles
 * that are destined to it.
 * Used by the routing logic to store bundles going towards
 * a common destination but yet to be assigned to a particular
 * link or contact.
 */
class Peer : public Formatter,  public BundleConsumer {
public:
    
    /**
     * Constructor / Destructor
     */
    Peer(const BundleTuple& tuple);
    virtual ~Peer();

    /**
     * Add a link
     */
    void add_link(Link* link) ;
    
    /**
     * Delete a link
     */
    void delete_link(Link* link) ;
    
    /**
     * Check if the peer already has this link
     */
    bool has_link(Link* link) ; 
    
    /**
     * Return the list of links that lead to the peer
     */
    LinkSet* links() { return links_; }
    
    /**
     * Accessor to tuple
     */
    BundleTuple tuple() { return tuple_; }
    
    /**
     * Name of the peer
     */
    const char* name() { return tuple().data() ; }
    
    /**
     * Virtual from formatter
     */
    int format(char* buf, size_t sz);
    
    /**
     * Virtual from bundle consumer
     */
    const char* type() { return "Peer" ;}
        
protected:
    LinkSet*  links_;      ///< List of links that lead to this peer
    BundleTuple tuple_;    ///< Identity of peer

};

#endif /* _PEER_H_ */
