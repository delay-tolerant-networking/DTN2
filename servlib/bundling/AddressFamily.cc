
#include "AddressFamily.h"
#include "FixedAddressFamily.h"
#include "InternetAddressFamily.h"
#include "SmtpAddressFamily.h"
#include "WildcardAddressFamily.h"

AddressFamilyTable* AddressFamilyTable::instance_;

/**
 * Constructor.
 */
AddressFamily::AddressFamily(std::string& schema)
    : schema_(schema)
{
}

/**
 * Constructor.
 */
AddressFamily::AddressFamily(const char* schema)
    : schema_(schema)
{
}

/**
 * Destructor (should never be called).
 */
AddressFamily::~AddressFamily()
{
    NOTREACHED;
}

/**
 * Determine if the administrative string is valid. By default
 * this returns true.
 */
bool
AddressFamily::valid(const std::string& admin)
{
    return true;
}
    
/**
 * Compare two admin strings, implementing any wildcarding or
 * pattern matching semantics. The default implementation just
 * does a byte by byte string comparison.
 */
bool
AddressFamily::match(const std::string& pattern,
                     const std::string& admin)
{
    return (pattern.compare(admin) == 0);
}

/**
 * Constructor -- instantiates and registers all known address
 * families.
 */
AddressFamilyTable::AddressFamilyTable()
{
    fixed_family_ 	= new FixedAddressFamily();
    wildcard_family_	= new WildcardAddressFamily();
    
    table_["host"] 	= new InternetAddressFamily("host");
    table_["ip"] 	= new InternetAddressFamily("ip");
    table_["smtp"]	= new SmtpAddressFamily();

    // XXX/demmer temp
    table_["tcp"] 	= new InternetAddressFamily("tcp");
    table_["udp"] 	= new InternetAddressFamily("udp");
}

/**
 * Find the appropriate AddressFamily instance based on the URI
 * schema of the admin string.
 *
 * @return the address family or NULL if there's no match.
 */
AddressFamily*
AddressFamilyTable::lookup(const std::string& schema)
{
    AFTable::iterator iter;

    iter = table_.find(schema);
    if (iter != table_.end()) {
        return (*iter).second;
    }

    return NULL;
}
