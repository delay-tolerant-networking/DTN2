
#include <oasys/util/URL.h>
#include <oasys/io/NetUtils.h>

#include "InternetAddressFamily.h"
#include "bundling/BundleTuple.h"

/**
 * Given a tuple, parse out the ip address and port. Potentially
 * does a hostname lookup if the admin string doesn't contain a
 * specified IP address. If the url did not contain a port, 0 is
 * returned.
 *
 * @return true if the extraction was a success, false if the url
 * is malformed or is not in the internet address family.
 */
bool
InternetAddressFamily::parse(const BundleTuple& tuple,
                             in_addr_t* addr, u_int16_t* port)
{
    // XXX/demmer validate that the AF in the tuple is in fact of type
    // InternetAddressFamily

    URL url(tuple.admin());
    if (! url.valid()) {
        logf("/af/internet", LOG_DEBUG,
             "admin part '%s' of tuple '%s' not a valid url",
             tuple.admin().c_str(),
             tuple.tuple().c_str());
        return false;
    }

    // look up the hostname in the url
    *addr = INADDR_NONE;
    if (gethostbyname(url.host_.c_str(), addr) != 0) {
        logf("/af/internet", LOG_DEBUG,
             "admin host '%s' not a valid hostname", url.host_.c_str());
        return false;
    }

    *port = url.port_;

    return true;
}

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
