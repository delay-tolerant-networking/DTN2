#include "AnnounceBundle.h"
#include "BundleProtocol.h"

namespace dtn {

void
AnnounceBundle::create_announce_bundle(Bundle* announce,
                                       const EndpointID& local_eid)
{
    // only meant for DTN admin consumption
    announce->is_admin_ = true;

    // fetch a copy of local eid
    announce->source_.assign(local_eid);

    // null out the rest
    announce->dest_.assign(EndpointID::NULL_EID());
    announce->replyto_.assign(EndpointID::NULL_EID());
    announce->custodian_.assign(EndpointID::NULL_EID());

    // non-zero expire time
    announce->expiration_ = 3600;

    // one byte payload:  admin type
    u_char buf = (BundleProtocol::ADMIN_ANNOUNCE << 4);
    announce->payload_.set_data(&buf,1);
}

} // dtn
