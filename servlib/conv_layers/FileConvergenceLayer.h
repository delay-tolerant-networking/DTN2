#ifndef _FILE_CONVERGENCE_LAYER_H_
#define _FILE_CONVERGENCE_LAYER_H_

#include "ConvergenceLayer.h"

class FileConvergenceLayer : public ConvergenceLayer {
public:
    /**
     * Constructor.
     */
    FileConvergenceLayer();
    
    /**
     * The meat of the initialization happens here.
     */
    virtual void init();

    /**
     * Hook for shutdown.
     */
    virtual void fini();

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

#endif /* _FILE_CONVERGENCE_LAYER_H_ */
