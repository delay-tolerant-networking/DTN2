#ifndef __FRAGMENT_MANAGER_H__
#define __FRAGMENT_MANAGER_H__

#include "debug/Log.h"
#include <string>
#include "util/StringUtils.h"

class Bundle;
class BundleList;
class SpinLock;

/**
 * The Fragment Manager maintains state for all of the fragmentary
 * bundles, reconstructing whole bundles from partial bundles.
 *
 * It also implements the routine for creating bundle fragments from
 * larger bundles.
 */
class FragmentManager : public Logger {
public:
    /**
     * Constructor.
     */
    FragmentManager();
    
    /**
     * Singleton accessor.
     */
    static FragmentManager* instance() {
        return &instance_;
    }
    
    /**
     * Create a bundle fragment from another bundle.
     *
     * @param bundle the source bundle from which we create the
     * fragment. Note: the bundle may itself be a fragment
     *
     * @param offset the offset in the *original* bundle for the for
     * the new fragment
     *
     * @param length the length of the fragment we want
     * 
     * @return pointer to the newly created bundle
     */
    Bundle* fragment(Bundle* bundle, int offset, size_t length);

    /**
     * Given a newly arrived bundle fragment, append it to the table
     * of fragments and see if it allows us to reassemble the bundle.
     */
    Bundle* process(Bundle* fragment);

 protected:
    /// Reassembly state structure
    struct ReassemblyState;
    
    /**
     * Calculate a hash table key from a bundle
     */
    void get_hash_key(const Bundle*, std::string* key);

    /**
     * Check if the bundle has been completely reassembled.
     */
    bool check_completed(ReassemblyState* state);

    /// Singleton instance
    static FragmentManager instance_;
    
    /// Table of partial bundles
    typedef StringHashMap<ReassemblyState*> ReassemblyTable;
    ReassemblyTable reassembly_table_;

    /// Lock
    SpinLock* lock_;
};

#endif /* __FRAGMENT_MANAGER_H__ */
