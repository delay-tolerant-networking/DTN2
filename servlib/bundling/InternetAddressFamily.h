#ifndef _INTERNET_ADDRESS_FAMILY_H_
#define _INTERNET_ADDRESS_FAMILY_H_

#include "AddressFamily.h"

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
};

#endif /* _INTERNET_ADDRESS_FAMILY_H_ */
