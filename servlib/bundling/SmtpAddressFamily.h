#ifndef _SMTP_ADDRESS_FAMILY_H_
#define _SMTP_ADDRESS_FAMILY_H_

#include "AddressFamily.h"

class SmtpAddressFamily : public AddressFamily {
public:
    SmtpAddressFamily() : AddressFamily("smtp") {}
    
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

#endif /* _SMTP_ADDRESS_FAMILY_H_ */
