#ifndef _CONTACT_MANAGER_H_
#define _CONTACT_MANAGER_H_

#include <oasys/debug/Log.h>

class BundleTuple;
class Contact;
class Link;
class LinkSet;
class Peer;
class PeerSet;
class StringBuffer;

/**
 * A contact manager singleton class.
 * Maintains  topological information regarding available
 * links,peers,contacts.
 *
 */
class ContactManager : public Logger {
public:
    /**
     * Singleton instance accessor.
     */
    static ContactManager* instance() {
        if (instance_ == NULL) {
            PANIC("ContactManager::init not called yet");
        }
        return instance_;
    }

    /**
     * Creates a new contact manager
     */
    static void init() {
        if (instance_ != NULL) {
            PANIC("ContactManager::init called multiple times");
        }
        instance_ = new ContactManager();
    }

    /**
     * Return true if initialization has completed.
     */
    static bool initialized() { return (instance_ != NULL); }
    
    /**
     * Constructor / Destructor
     */
    ContactManager();
    virtual ~ContactManager() {}
    
    /**
     * Dump a string representation of the info inside contact manager.
     */
    void dump(StringBuffer* buf) const;
    
    /**********************************************
     *
     * Peer set accessor functions
     *
     *********************************************/
    
    /**
     * Add a peer
     */
    void add_peer(Peer* peer);
    
    /**
     * Delete a peer
     */
    void delete_peer(Peer* peer);
    
    /**
     * Check if contact manager already has this peer
     */
    bool has_peer(Peer* peer);

    /**
     * Finds peer corresponding to this bundletuple
     * Creates one, if no such peer exists.
     */
    Peer* find_peer(const BundleTuple& tuple);
    
    /**
     * Return the list of peers 
     */
    PeerSet* peers() { return peers_; }
    
    /**********************************************
     *
     * Link set accessor functions
     *
     *********************************************/
    /**
     * Add a link
     */
    void add_link(Link* link);
    
    /**
     * Delete a link
     */
    void delete_link(Link* link);
    
    /**
     * Check if contact manager already has this link
     */
    bool has_link(Link* link);

    /**
     * Finds link corresponding to this name
     */
    Link* find_link(const char* name);
    
    /**
     * Return the list of links 
     */
    LinkSet* links() { return links_; }
    
protected:
    static ContactManager* instance_;  ///< singleton instance
    PeerSet* peers_;                  ///< Set of all peers
    LinkSet* links_;                  ///< Set of all links
};


#endif /* _CONTACT_MANAGER_H_ */
