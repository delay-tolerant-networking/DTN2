
#include "Interface.h"
#include "conv_layers/ConvergenceLayer.h"

Interface* Interface::if_list_ = NULL;

Interface::Interface(const BundleTuple& tuple,
                     ConvergenceLayer* clayer)
    : tuple_(tuple), clayer_(clayer), info_(NULL)
{
}

Interface::~Interface()
{
}

bool
Interface::add_interface(const char* tuplestr,
                         int argc, const char* argv[])
{
    log_debug("/interface", "creating interface '%s'", tuplestr);
    
    BundleTuple tuple(tuplestr);
    if (!tuple.valid()) {
        log_err("/interface", "invalid interface tuple '%s'", tuplestr);
        return false;
    }

    ConvergenceLayer* cl = ConvergenceLayer::find_clayer(tuple.admin());
    if (!cl) {
        log_err("/interface", "can't find convergence layer for %s",
                tuple.admin().c_str());
        return false;
    }

    Interface* iface = new Interface(tuple, cl);
    if (! cl->add_interface(iface, argc, argv)) {
        log_err("/interface", "convergence layer error adding interface %s",
                tuplestr);
        return false;
    }

    iface->next_ = if_list_;
    if_list_ = iface;

    return true;
}

bool
Interface::del_interface(const char* tuplestr)
{
    log_debug("/interface", "removing interface %s", tuplestr);

    Interface** ifp = &if_list_;
    Interface* deadif;

    while (*ifp != 0) {
        if ((*ifp)->tuple()->tuple().compare(tuplestr) == 0) {
            deadif = *ifp;
            *ifp = (*ifp)->next_;
            
            if (! deadif->clayer()->del_interface(deadif)) {
                log_err("/interface",
                        "convergence layer error removing interface %s",
                        tuplestr);
            }
            
            delete deadif;
            return true;
        }

        ifp = &(*ifp)->next_;
    }

    log_err("/interface", "del_interface: can't find tuple '%s'", tuplestr);
    
    return false;
}
