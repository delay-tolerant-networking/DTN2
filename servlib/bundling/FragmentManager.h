#ifndef _FRAGMENTMANAGER_H_
#define _FRAGMENTMANAGER_H_

#include "BundlePayload.h"
#include "BundleTuple.h"
#include "debug/Formatter.h"
#include "Bundle.h"

/**
 * The Fragment Manager maintains state for all of the 
 * fragmentary bundles, reconstructing whole bundles from
 * partial bundles, and creating bundle fragments from
 * larger bundles.
 */

class FragmentManager 
{
 public:
    /**
     * Create a bundle fragment from another bundle
     *
     * @param bundle the source bundle from which we create 
     *  the fragmentary bundle. Note: this may be a fragment
     * @param offset the offset in the *original* bundle for the 
     *  for the new fragment
     * @param length the length of the bundle
     * 
     * @return pointer to the newly created bundle
     */
    Bundle * fragment(Bundle * bundle, int offset, size_t length);
    Bundle * process(Bundle * fragment);

 protected:
    static FragmentManager instance_;

    std::hash_map<string, BundleList*> fragments_;
}
