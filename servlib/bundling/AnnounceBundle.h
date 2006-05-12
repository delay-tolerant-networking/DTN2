#ifndef _ANNOUNCE_BUNDLE_H_
#define _ANNOUNCE_BUNDLE_H_

#include "Bundle.h"
#include "naming/EndpointID.h"

namespace dtn {

/**
 * Beacon sent by Neighbor Discovery element within Convergence Layer
 * to announce to other DTN nodes. 
 */
class AnnounceBundle
{
public:
    static void create_announce_bundle(Bundle *bundle,
                                       const EndpointID& eid);

}; // AnnounceBundle

} // dtn

#endif // _ANNOUNCE_BUNDLE_H_
