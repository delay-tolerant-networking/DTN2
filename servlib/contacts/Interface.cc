
#include "Interface.h"
#include "ConvergenceLayer.h"

Interface* Interface::if_list_ = NULL;

Interface::Interface(const BundleTuple& tuple,
                     ConvergenceLayer* clayer,
                     const char* args[])
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
Interface::add_interface(const char* tuple, int argc,
                         const char* args[])
{
    logf("/interface", LOG_DEBUG, "creating interface %s", tuple);
    
    BundleTuple t(tuple);
    if (!t.valid()) {
        logf("/interface", LOG_ERR, "invalid interface tuple %s", tuple);
        return false;
    }

    ConvergenceLayer* cl = ConvergenceLayer::find_clayer(tuple.admin());
    if (!cl) {
        logf("/interface", LOG_ERR, "can't find convergence layer for %s",
             tuple.admin().c_str(*));
        return false;
    }

    Interface iface = new Interface(t, cl);
    cl->add_interface(iface. argc, argv);

    iface->next_ = if_list_;
    if_list_ = iface;
}

bool
Interface::del_interface(const char* tuple)
{
    logf("/interface", LOG_DEBUG, "removing interface %s", tuple);

    Interface** ifp = &if_list_;
    Interface* deadif;

    while (*ifp != 0) {
        if ((*ifp)->tuple()->tuple()->compare(tuple) == 0) {
            deadif = *ifp;
            *ifp = (*ifp)->next_;

            deadif->clayer()del_interface(deadif);
            delete deadif;
            return true
        }

        ifp = &(*ifp)->next_;
    }
    
    return false;
}
