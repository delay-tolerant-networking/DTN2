#ifndef _BUNDLEREF_H_
#define _BUNDLEREF_H_

#include "Bundle.h"

/**
 * Simple smart pointer class to maintain reference counts on Bundles.
 */
class BundleRef {
public:
    BundleRef(Bundle* bundle)
        : bundle_(bundle)
    {
        if (bundle_)
            bundle->add_ref();
    }

    ~BundleRef()
    {
        if (bundle_)
            bundle_->del_ref();
    }

    Bundle* bundle()
    {
        return bundle_;
    }
    
private:
    BundleRef();
    BundleRef(const BundleRef& copy);
    
    Bundle* bundle_;
};

#endif /* _BUNDLEREF_H_ */
