
#include "FixedAddressFamily.h"

/**
 * Determine if the administrative string is valid.
 *
 * Any one or two byte value is acceptable.
 */
bool
FixedAddressFamily::valid(const std::string& admin)
{
    ASSERT(admin.length() == 1 || admin.length() == 2);
    return true;
}
    
/**
 * Compare two admin strings and determine if they're equal.
 */
bool
FixedAddressFamily::match(const std::string& pattern,
                          const std::string& tuple)
{
    return pattern.compare(tuple) == 0;
}
