
#include "InterfaceTable.h"
#include "BundleTuple.h"
#include "conv_layers/ConvergenceLayer.h"
#include "debug/Debug.h"

InterfaceTable* InterfaceTable::instance_ = NULL;

InterfaceTable::InterfaceTable()
    : Logger("/interface")
{
}

InterfaceTable::~InterfaceTable()
{
    NOTREACHED;
}

/**
 * Add a new interface to the table. Returns true if the interface
 * is successfully added, false if the interface specification is
 * invalid.
 */
bool
InterfaceTable::add(const char* tuplestr, int argc, const char* argv[])
{
    log_debug("creating interface '%s'", tuplestr);
    
    BundleTuple tuple(tuplestr);
    if (!tuple.valid()) {
        log_err("invalid interface tuple '%s'", tuplestr);
        return false;
    }

    ConvergenceLayer* cl = ConvergenceLayer::find_clayer(tuple.admin());
    if (!cl) {
        log_err("can't find convergence layer for %s", tuple.admin().c_str());
        return false;
    }

    Interface* iface = new Interface(tuple, cl);
    if (! cl->add_interface(iface, argc, argv)) {
        log_err("convergence layer error adding interface %s", tuplestr);
        return false;
    }

    iflist_.push_back(iface);

    return true;
}

/**
 * Internal method to find the location of the given interface
 */
bool
InterfaceTable::find(const char* tuplestr, InterfaceList::iterator* iter)
{
    for (*iter = iflist_.begin(); *iter != iflist_.end(); ++(*iter)) {
        
        if ((**iter)->tuple().tuple().compare(tuplestr) == 0) {
            return true;
        }
    }        

    return false;
}


/**
 * Remove the specified interface.
 */
bool
InterfaceTable::del(const char* tuplestr)
{
    InterfaceList::iterator iter;
    
    log_debug("removing interface %s", tuplestr);

    if (! find(tuplestr, &iter)) {
        log_err("error removing interface %s: no such interface", tuplestr);
        return false;
    }

    iflist_.erase(iter);
    return true;
}
