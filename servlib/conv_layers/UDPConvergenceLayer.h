#ifndef _UDP_CONVERGENCE_LAYER_H_
#define _UDP_CONVERGENCE_LAYER_H_

#include "IPConvergenceLayer.h"

class UDPConvergenceLayer : public IPConvergenceLayer {
public:
    /**
     * Constructor.
     */
    UDPConvergenceLayer();
        
    /**
     * The meat of the initialization happens here.
     */
    virtual void init();

    /**
     * Hook for shutdown.
     */
    virtual void fini();
};

#endif /* _UDP_CONVERGENCE_LAYER_H_ */
