
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
    if (info_) {
        delete info_;
    }
}

bool
Interface::add_interface(const char* tuplestr,
                         int argc, const char* argv[])
{
    logf("/interface", LOG_DEBUG, "creating interface '%s'", tuplestr);
    
    BundleTuple tuple(tuplestr);
    if (!tuple.valid()) {
        logf("/interface", LOG_ERR, "invalid interface tuple '%s'", tuplestr);
        return false;
    }

    ConvergenceLayer* cl = ConvergenceLayer::find_clayer(tuple.admin());
    if (!cl) {
        logf("/interface", LOG_ERR, "can't find convergence layer for %s",
             tuple.admin().c_str());
        return false;
    }

    Interface* iface = new Interface(tuple, cl);
    cl->add_interface(iface, argc, argv);

    iface->next_ = if_list_;
    if_list_ = iface;

    return true;
}

bool
Interface::del_interface(const char* tuplestr)
{
    logf("/interface", LOG_DEBUG, "removing interface %s", tuplestr);

    Interface** ifp = &if_list_;
    Interface* deadif;

    while (*ifp != 0) {
        if ((*ifp)->tuple()->tuple().compare(tuplestr) == 0) {
            deadif = *ifp;
            *ifp = (*ifp)->next_;

            deadif->clayer()->del_interface(deadif);
            delete deadif;
            return true;
        }

        ifp = &(*ifp)->next_;
    }

    logf("/interface", LOG_ERR, "del_interface: can't find tuple '%s'",
         tuplestr);
    
    return false;
}
