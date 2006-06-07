#include <oasys/util/ScratchBuffer.h>
#include "AnnounceBundle.h"
#include "BundleProtocol.h"

namespace dtn {

void
AnnounceBundle::create_announce_bundle(Bundle* announce,
                                       const EndpointID& route_eid)
{
    // only meant for DTN admin consumption
    announce->is_admin_ = true;

    // assign router's local EID as bundle source
    announce->source_.assign(route_eid);

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

bool
AnnounceBundle::parse_announce_bundle(Bundle* bundle,
                                      EndpointID *route_eid)
{
    if (bundle->is_admin_ == false)
        return false;

    size_t payload_len = bundle->payload_.length();
    if (payload_len <= 0) return false;

    oasys::ScratchBuffer<u_char*, 256> scratch(payload_len);
    const u_char* payload_buf =
            bundle->payload_.read_data(0, payload_len, scratch.buf(payload_len));

    if ((*payload_buf >> 4) == BundleProtocol::ADMIN_ANNOUNCE) {

        if(route_eid != NULL)
            route_eid->assign(bundle->source_);

        return true;
    }

    return false;
}

} // dtn
