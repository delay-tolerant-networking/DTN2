#ifndef _INTERFACE_TABLE_H_
#define _INTERFACE_TABLE_H_

#include <string>
#include <list>
#include "debug/Debug.h"

class Interface;

/**
 * The list of interfaces.
 */
typedef std::list<Interface*> InterfaceList;

/**
 * Class for the in-memory interface table.
 */
class InterfaceTable : public Logger {
public:
    /**
     * Singleton instance accessor.
     */
    static InterfaceTable* instance() {
        if (instance_ == NULL) {
            PANIC("InterfaceTable::init not called yet");
        }
        return instance_;
    }

    /**
     * Boot time initializer that takes as a parameter the actual
     * storage instance to use.
     */
    static void init() {
        if (instance_ != NULL) {
            PANIC("InterfaceTable::init called multiple times");
        }
        instance_ = new InterfaceTable();
    }

    /**
     * Constructor
     */
    InterfaceTable();

    /**
     * Destructor
     */
    virtual ~InterfaceTable();

    /**
     * Add a new interface to the table. Returns true if the interface
     * is successfully added, false if the interface specification is
     * invalid.
     */
    bool add(const char* tuplestr, int argc, const char* argv[]);
    
    /**
     * Remove the specified interface.
     */
    bool del(const char* tuplestr);

protected:
    static InterfaceTable* instance_;

    /**
     * All interfaces are tabled in-memory in a flat list. It's
     * non-obvious what else would be better since we need to do a
     * prefix match on demux strings in matching_interfaces.
     */
    InterfaceList iflist_;

    /**
     * Internal method to find the location of the given interface
     */
    bool find(const char* tuplestr, InterfaceList::iterator* iter);
};

#endif /* _INTERFACE_TABLE_H_ */
