#ifndef _BUNDLEREF_H_
#define _BUNDLEREF_H_

#include "Bundle.h"

/**
 * Simple smart pointer class to maintain reference counts on Bundles.
 */
class BundleRef {
public:
    BundleRef(Bundle* bundle, const char* what1, const char* what2 = "")
        : bundle_(bundle), what1_(what1), what2_(what2)
    {
        if (bundle_)
            bundle->add_ref(what1_, what2_);
    }

    ~BundleRef()
    {
        if (bundle_)
            bundle_->del_ref(what1_, what2_);
    }

    Bundle* bundle()
    {
        return bundle_;
    }
    
private:
    BundleRef();
    BundleRef(const BundleRef& copy);
    
    Bundle* bundle_;

    const char *what1_, *what2_;
};

#endif /* _BUNDLEREF_H_ */
