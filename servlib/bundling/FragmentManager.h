#ifndef __FRAGMENT_MANAGER_H__
#define __FRAGMENT_MANAGER_H__

#include "debug/Log.h"
#include <string>

// Though hash_map was part of std:: in the 2.9x gcc series, it's been
// moved to ext/__gnu_cxx:: in 3.x
// XXX/demmer move this all into a compat.h or some such header file 
#if (__GNUC__ == 2 && __GNUC_MINOR__ >= 95)
#include <hash_map>
#define _std std
#else
#include <ext/hash_map>
#define _std __gnu_cxx
#endif


class Bundle;
class BundleList;

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
    /// Singleton instance
    static FragmentManager instance_;

    /// Table of partial bundles
    _std::hash_map<std::string, BundleList*> partial_;
};

#endif /* __FRAGMENT_MANAGER_H__ */
