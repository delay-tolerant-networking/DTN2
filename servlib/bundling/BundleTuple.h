#ifndef _BUNDLE_TUPLE_H_
#define _BUNDLE_TUPLE_H_

#include "storage/Serialize.h"

class BundleTuple : public SerializableObject {
public:
    BundleTuple();
    virtual ~BundleTuple();
        
    /**
     * Return whether or not this tuple is valid.
     */
    bool valid() { return valid_; }

    virtual void serialize(SerializeAction* a);

protected:
    bool valid_;
};

#endif /* _BUNDLE_TUPLE_H_ */
