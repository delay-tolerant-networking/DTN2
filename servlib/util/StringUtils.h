#ifndef _STRING_UTILS_H_
#define _STRING_UTILS_H_

/**
 * Utilities and stl typedefs for basic_string.
 */

#include <string>
#include <vector>
#include <ext/hash_set>

/**
 * Hashing function class for std::strings.
 */
struct StringHash {
    size_t operator()(const std::string& str) const
    {
        return __gnu_cxx::__stl_hash_string(str.data());
    }
};

/**
 * Comparison function class for std::strings.
 */
struct StringEquals {
    bool operator()(const std::string& str1, const std::string& str2) const
    {
        return str1 == str2;
    }
};

/**
 * A stringset_t is a hash set with std::string members.
 */
typedef __gnu_cxx::hash_set<std::string, StringHash, StringEquals> stringset_t;

class StringSet : public stringset_t {
public:
    void dump(const char* log) const;
};

/**
 * A stringvector_t is a std::vector of std::strings.
 */
typedef std::vector<std::string> stringvector_t;

/**
 * Generate a hex string from a binary buffer.
 */
inline void
hex2str(std::string* str, const u_char* bp, size_t len)
{
    static const char hex[] = "0123456789abcdef";
    str->clear();
    for (size_t i = 0; i < len; ++i) {
        str->push_back(hex[(bp[i] >> 4) & 0xf]);
        str->push_back(hex[bp[i] & 0xf]);
    }
}

/**
 * Parse a hex string into a binary buffer. Results undefined if the
 * string contains characters other than [0-9a-f].
 */
inline void
str2hex(const std::string& str, u_char* bp, size_t len)
{
#define HEXTONUM(x) ((x) < 'a' ? (x) - '0' : x - 'a' + 10)
    const char* s = str.data();
    for (size_t i = 0; i < len; ++i) {
        bp[i] = (HEXTONUM(s[2*i]) << 4) + HEXTONUM(s[2*i + 1]);
    }
#undef HEXTONUM
}

#endif /* _STRING_UTILS_H_ */
