
#include "InterfaceTable.h"
#include "BundleTuple.h"
#include "conv_layers/ConvergenceLayer.h"

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
 * Internal method to find the location of the given interface
 */
bool
InterfaceTable::find(BundleTuple& tuple, ConvergenceLayer* cl,
                     InterfaceList::iterator* iter)
{
    Interface* iface;
    for (*iter = iflist_.begin(); *iter != iflist_.end(); ++(*iter)) {
        iface = **iter;
        
        if (iface->tuple().equals(tuple) && iface->clayer() == cl) {
            return true;
        }
    }        
    
    return false;
}

/**
 * Create and add a new interface to the table. Returns true if
 * the interface is successfully added, false if the interface
 * specification is invalid.
 */
bool
InterfaceTable::add(BundleTuple& tuple, ConvergenceLayer* cl,
                    const char* proto,
                    int argc, const char* argv[])
{
    InterfaceList::iterator iter;
    
    if (find(tuple, cl, &iter)) {
        log_err("interface %s already exists", tuple.c_str());
        return false;
    }
    
    log_info("adding interface %s %s", proto, tuple.c_str());

    Interface* iface = new Interface(tuple, cl);
    if (! cl->add_interface(iface, argc, argv)) {
        log_err("convergence layer error adding interface %s", tuple.c_str());
        return false;
    }

    iflist_.push_back(iface);

    return true;
}

/**
 * Remove the specified interface.
 */
bool
InterfaceTable::del(BundleTuple& tuple, ConvergenceLayer* cl,
                    const char* proto)
{
    Interface* iface;
    InterfaceList::iterator iter;
    
    log_info("removing interface %s %s", proto, tuple.c_str());

    if (! find(tuple, cl, &iter)) {
        log_err("error removing interface %s: no such interface",
                tuple.c_str());
        return false;
    }

    iface = *iter;
    iflist_.erase(iter);
    delete iface;
    
    return true;
}
