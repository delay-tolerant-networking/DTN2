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
    BundleTuple(const char* tuple);
    BundleTuple(const std::string& tuple);
    BundleTuple(const BundleTuple& other);
    virtual ~BundleTuple();

    /**
     * Parse and assign the given tuple string.
     */
    void set_tuple(const std::string& tuple);
    
    // Accessors
    const std::string& tuple()  const { return tuple_; }
    size_t	       length() const { return tuple_.length(); }
    const char*        data()   const { return tuple_.data(); }
    const char*        c_str()  const { return tuple_.c_str(); }
    
    const std::string& region() const { return region_; }
    const std::string& admin()  const { return admin_; }
    
    /**
     * @return Whether or not the tuple is valid.
     */
    bool valid() const { return valid_; }

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
