#ifndef _ADDRESS_FAMILY_H_
#define _ADDRESS_FAMILY_H_

#include "debug/Debug.h"
#include "util/StringUtils.h"

/**
 * To support different types of addressing within different regions,
 * each region identifier is used as a lookup key into the table of
 * supported address families.
 */
class AddressFamily {
public:
    /**
     * Constructor.
     */
    AddressFamily(std::string& schema);
    AddressFamily(const char* schema);

    /**
     * Destructor (should never be called).
     */
    virtual ~AddressFamily();

    /**
     * Determine if the administrative string is valid. By default
     * this returns true.
     */
    virtual bool valid(const std::string& admin);
    
    /**
     * Compare two admin strings, implementing any wildcarding or
     * pattern matching semantics. The default implementation just
     * does a byte by byte string comparison.
     */
    virtual bool match(const std::string& pattern,
                       const std::string& admin);

    /**
     * Return the schema name for this address family.
     */
    const std::string& schema() const { return schema_; }

protected:
    std::string schema_;	///< the schema for this family
};

/**
 * The table of registered address families.
 */
class AddressFamilyTable {
public:
    /**
     * Singleton accessor.
     */
    static AddressFamilyTable* instance() {
        ASSERT(instance_ != NULL);
        return instance_;
    }

    /**
     * Boot time initializer.
     */
    static void init()
    {
        ASSERT(instance_ == NULL);
        instance_ = new AddressFamilyTable();
    }

    /**
     * Constructor -- instantiates and registers all known address
     * families.
     */
    AddressFamilyTable();

    /**
     * Find the appropriate AddressFamily instance based on the URI
     * schema of the admin string.
     *
     * @return the address family or NULL if there's no match.
     */
    AddressFamily* lookup(const std::string& schema);

    /**
     * For integer value admin strings, we use the fixed length
     * address family.
     */
    AddressFamily* fixed_family() { return fixed_family_; }
    
    /**
     * The special wildcard address family.
     */
    AddressFamily* wildcard_family() { return wildcard_family_; }
    
protected:
    static AddressFamilyTable* instance_;
    
    typedef StringHashMap<AddressFamily*> AFTable;
    AFTable table_;

    AddressFamily* fixed_family_;
    AddressFamily* wildcard_family_;
};

#endif /* _ADDRESS_FAMILY_H_ */
