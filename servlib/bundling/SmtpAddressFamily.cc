#include "SmtpAddressFamily.h"

/**
 * Determine if the administrative string is valid.
 */
bool
SmtpAddressFamily::valid(const std::string& admin)
{
    NOTIMPLEMENTED;
}

/**
 * Compare two admin strings, implementing any wildcarding or
 * pattern matching semantics.
 */
bool
SmtpAddressFamily::match(const std::string& pattern,
                             const std::string& admin)
{
    NOTIMPLEMENTED;
}
