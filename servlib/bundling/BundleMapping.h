#ifndef _BUNDLE_MAPPING_H_
#define _BUNDLE_MAPPING_H_

#include <oasys/debug/Debug.h>
#include "BundleList.h"

/**
 * Each time a bundle is put on a bundle list, a mapping is maintained
 * in the bundle structure containing information about the
 * association. This information can be used by the routers to make
 * more informed scheduling decisions.
 */
class BundleMappingInfo;
class RouterInfo;

/**
 * Various forwarding actions
 */
typedef enum {
    FORWARD_INVALID = 0,///< Invalid action
    
    FORWARD_UNIQUE,	///< Forward the bundle only to one next hop
    FORWARD_COPY,	///< Forward a copy of the bundle
    FORWARD_FIRST,	///< Forward to the first of a set
    FORWARD_REASSEMBLE	///< First reassemble fragments (if any) then forward
} bundle_fwd_action_t;

inline const char*
bundle_fwd_action_toa(bundle_fwd_action_t action)
{
    switch(action) {
    case FORWARD_UNIQUE:	return "FORWARD_UNIQUE";
    case FORWARD_COPY:		return "FORWARD_COPY";
    case FORWARD_FIRST:		return "FORWARD_FIRST";
    case FORWARD_REASSEMBLE:	return "FORWARD_REASSEMBLE";
    default:
        NOTREACHED;
    }
}

/**
 * Simple structure to encapsulate mapping state that's passed in when the
 * bundle is queued on a list.
 */
struct BundleMapping {
     BundleMapping()
        : action_(FORWARD_INVALID),
          mapping_grp_(0),
          timeout_(0),
          router_info_(NULL)
    {}
    
    BundleMapping(const BundleMapping& other)
        : action_(other.action_),
          mapping_grp_(other.mapping_grp_),
          timeout_(other.timeout_),
          router_info_(other.router_info_)
    {}

    BundleList::iterator position_;	///< Position of the bundle in the list
    bundle_fwd_action_t action_;	///< The forwarding action code
    int mapping_grp_;			///< Mapping group identifier
    u_int32_t timeout_;			///< Timeout for the mapping
    RouterInfo* router_info_;		///< Slot_ for router private state
};

#endif /* _BUNDLE_MAPPING_H_ */
