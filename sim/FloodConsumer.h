#ifndef __FLOOD_CONSUMER___
#define __FLOOD_CONSUMER___

#include "reg/Registration.h"
#include "bundling/BundleTuple.h"
#include "routing/BundleRouter.h"

class FloodConsumer: public Registration {
public:
    FloodConsumer(u_int32_t regid, const BundleTuplePattern& endpoint);
    
    void    set_router(BundleRouter *router);
    void    enqueue_bundle(Bundle *bundle);
protected:
    BundleRouter *router_;
};

#endif
