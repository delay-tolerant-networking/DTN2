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
    void set_tuple(const std::string& tuple)
    {
        tuple_.assign(tuple);
        parse_tuple();
    }
    
    /**
     * Parse and assign the given tuple string.
     */
    void set_tuple(u_char* str, size_t len)
    {
        tuple_.assign((char*)str, len);
        parse_tuple();
    }
    
    // Accessors
    const std::string& tuple()  const { return tuple_; }
    size_t	       length() const { return tuple_.length(); }
    const char*        data()   const { return tuple_.data(); }
    const char*        c_str()  const { return tuple_.c_str(); }
    
    const std::string& region() const { return region_; }
    const std::string& proto()  const { return proto_; }
    const std::string& admin()  const { return admin_; }
    
    /**
     * @return Whether or not the tuple is valid.
     */
    bool valid() const { return valid_; }

    /**
     * @return if the two tuples are equal
     */
    bool equals(const BundleTuple& other) const
    {
        return (tuple_.compare(other.tuple_) == 0);
    }

    /**
     * Comparison function
     */
    int compare(const BundleTuple& other) const
    {
        return tuple_.compare(other.tuple_);
    }

    /**
     * Comparison function
     */
    int compare(const std::string& tuple) const
    {
        return tuple_.compare(tuple);
    }

    /**
     * Virtual from SerializableObject
     */
    virtual void serialize(SerializeAction* a);

protected:
    void parse_tuple();
    
    bool valid_;
    std::string tuple_;
    std::string region_;
    std::string proto_;
    std::string admin_;
};

/**
 * A class that implements pattern matching of bundle tuples. The
 * pattern class has the same general form as the basic BundleTuple,
 * and implements a hierarchical wildcard matching on the region and
 * dispatches to the convergence layer to match the admin portion.
 */
class BundleTuplePattern : public BundleTuple {
public:
    BundleTuplePattern() 			: BundleTuple() {}
    BundleTuplePattern(const char* tuple) 	: BundleTuple(tuple) {}
    BundleTuplePattern(const std::string& tuple): BundleTuple(tuple) {}

    /**
     * Matching function, implementing wildcarding semantics.
     */
    bool match(const BundleTuple& tuple) const;

protected:
    bool match_region(const std::string& tuple_region) const;
    bool match_admin(const std::string& tuple_admin) const;
};

#endif /* _BUNDLE_TUPLE_H_ */
