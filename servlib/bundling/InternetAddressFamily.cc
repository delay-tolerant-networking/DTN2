#include "InternetAddressFamily.h"

/**
 * Determine if the administrative string is valid.
 */
bool
InternetAddressFamily::valid(const std::string& admin)
{
    // XXX/demmer fix this
    return true;
}

/**
 * Compare two admin strings, implementing any wildcarding or
 * pattern matching semantics.
 */
bool
InternetAddressFamily::match(const std::string& pattern,
                             const std::string& admin)
{
    size_t patternlen = pattern.length();

    // first of all, try exact matching
    if (pattern.compare(admin) == 0)
        return true;
    
    // otherwise, try supporting * as a trailing character
    if (patternlen >= 1 && pattern[patternlen-1] == '*') {
        patternlen --;
        
        if (pattern.substr(0, patternlen) == admin.substr(0, patternlen))
            return true;
    }

    // XXX/demmer also support CIDR style matching
    
    return false;

}
