#ifndef _INTERNET_ADDRESS_FAMILY_H_
#define _INTERNET_ADDRESS_FAMILY_H_

#include "AddressFamily.h"
#include <sys/types.h>
#include <netinet/in.h>

class BundleTuple;

// XXX/namespace
class InternetAddressFamily;
namespace dtn {
typedef ::InternetAddressFamily InternetAddressFamily;
}

class InternetAddressFamily : public AddressFamily {
public:
    InternetAddressFamily(const char* schema)
        : AddressFamily(schema)
    {
    }
    
    /**
     * Determine if the administrative string is valid.
     */
    bool valid(const std::string& admin);
    
    /**
     * Compare two admin strings, implementing any wildcarding or
     * pattern matching semantics.
     */
    bool match(const std::string& pattern,
               const std::string& admin);

    /**
     * Given a tuple, parse out the ip address and port. Potentially
     * does a hostname lookup if the admin string doesn't contain a
     * specified IP address. If the url did not contain a port, 0 is
     * returned.
     *
     * @return true if the extraction was a success, false if the url
     * is malformed or is not in the internet address family.
     */
    static bool parse(const BundleTuple& tuple,
                      in_addr_t* addr, u_int16_t* port);
     
};

#endif /* _INTERNET_ADDRESS_FAMILY_H_ */
