#ifndef _FIXED_ADDRESS_FAMILY_H_
#define _FIXED_ADDRESS_FAMILY_H_

#include "AddressFamily.h"

class FixedAddressFamily : public AddressFamily {
public:
    FixedAddressFamily() : AddressFamily("__fixed__") {}
    
    /**
     * Determine if the administrative string is valid.
     */
    bool valid(const std::string& admin);

    /**
     * Compare two admin strings, implementing any wildcarding or
     * pattern matching semantics. The default implementation just
     * does a byte by byte string comparison.
     */
    bool match(const std::string& pattern,
               const std::string& admin);

};

#endif /* _FIXED_ADDRESS_FAMILY_H_ */
