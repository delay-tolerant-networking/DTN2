#ifndef _TCP_CONVERGENCE_LAYER_H_
#define _TCP_CONVERGENCE_LAYER_H_

#include "IPConvergenceLayer.h"

class TCPConvergenceLayer : public IPConvergenceLayer {
public:
    /**
     * The meat of the initialization happens here.
     */
    virtual void init();

    /**
     * Hook for shutdown.
     */
    virtual void fini();

    /**
     * Register a new interface.
     */
    void add_interface(Interface* iface,
                       int argc, const char* argv[]);

    /**
     * Remove an interface
     */
    void del_interface(Interface* iface);

};

#endif /* _TCP_CONVERGENCE_LAYER_H_ */
