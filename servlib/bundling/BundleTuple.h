#ifndef _BUNDLE_TUPLE_H_
#define _BUNDLE_TUPLE_H_

#include "storage/Serialize.h"

/**
 * Class to wrap a bundle tuple of the general form:
 *
 * @code
 * bundles://<region>/<admin>
 * @endcode
 *
 * The region must not contain any '/' characters. and determines a
 * particular convergence layer to use, based on parsing the admin
 * portion.
 */
class BundleTuple : public SerializableObject {
public:
    BundleTuple();
    BundleTuple(const std::string& tuple);
    virtual ~BundleTuple();

    /**
     * Parse and assign the given tuple string.
     */
    void set_tuple(const std::string& tuple);
    
    /// Accessors
    // @{
    const std::string& tuple()  { return tuple_; }
    const std::string& region() { return region_; }
    const std::string& admin()  { return admin_; }
    // @}
    
    /**
     * @return Whether or not the tuple is valid.
     */
    bool valid() { return valid_; }

    /**
     * Virtual from SerializableObject
     */
    virtual void serialize(SerializeAction* a);

protected:
    void parse_tuple();
    
    bool valid_;
    std::string tuple_;
    std::string region_;
    std::string admin_;
};

#endif /* _BUNDLE_TUPLE_H_ */
