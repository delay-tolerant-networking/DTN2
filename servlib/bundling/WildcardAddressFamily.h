#ifndef _WILDCARD_ADDRESS_FAMILY_H_
#define _WILDCARD_ADDRESS_FAMILY_H_

#include "AddressFamily.h"

class WildcardAddressFamily : public AddressFamily {
public:
    WildcardAddressFamily() : AddressFamily("*") {}
    
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

#endif /* _WILDCARD_ADDRESS_FAMILY_H_ */
