#include "WildcardAddressFamily.h"

/**
 * Determine if the administrative string is valid.
 */
bool
WildcardAddressFamily::valid(const std::string& admin)
{
    //assert that it's *:* or *.*
    return true;
}

/**
 * Compare two admin strings, implementing any wildcarding or
 * pattern matching semantics.
 */
bool
WildcardAddressFamily::match(const std::string& pattern,
                             const std::string& admin)
{
    return true;
}
