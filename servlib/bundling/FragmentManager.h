#ifndef __FRAGMENT_MANAGER_H__
#define __FRAGMENT_MANAGER_H__

#include <string>
#include <oasys/debug/Log.h>
#include <oasys/util/StringUtils.h>

class Bundle;
class BundleList;
class SpinLock;

// XXX/demmer should change the overall flow of the reassembly so all
// arriving bundle fragments are enqueued onto the appropriate
// reassembly state list immediately, not based on the
// FORWARD_REASSEMBLE action

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
     * @param bundle
     *   the source bundle from which we create the
     *   fragment. Note: the bundle may itself be a fragment
     *
     * @param offset
     *   the offset relative to this bundle (not the
     *   original) for the for the new fragment. note that if this
     *   bundle is already a fragment, the offset into the original
     *   bundle will be this bundle's frag_offset + offset
     *
     * @param length
     *   the length of the fragment we want
     * 
     * @return
     *   pointer to the newly created bundle
     */
    Bundle* create_fragment(Bundle* bundle, int offset, size_t length);

    /**
     * Turn a bundle into a fragment. Note this is used just for
     * reactive fragmentation on a newly received partial bundle and
     * therefore the offset is implicitly zero (unless the bundle was
     * already a fragment).
     */
    void convert_to_fragment(Bundle* bundle, size_t length);

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
