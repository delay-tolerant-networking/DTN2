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
};

#endif /* _TCP_CONVERGENCE_LAYER_H_ */
