#ifndef _IP_CONVERGENCE_LAYER_H_
#define _IP_CONVERGENCE_LAYER_H_

#include "ConvergenceLayer.h"

class IPConvergenceLayer : public ConvergenceLayer {
public:
    /**
     * Constructor.
     */
    IPConvergenceLayer(const char* logpath)
        : ConvergenceLayer(logpath)
    {
    }
    
    /**
     * Hook to validate the admin part of a bundle tuple.
     *
     * @return true if the admin string is valid
     */
    virtual bool validate(const std::string& admin);

    /**
     * Hook to match the admin part of a bundle tuple.
     *
     * @return true if the demux matches the tuple.
     */
    virtual bool match(const std::string& demux, const std::string& admin);
};

#endif /* _IP_CONVERGENCE_LAYER_H_ */
