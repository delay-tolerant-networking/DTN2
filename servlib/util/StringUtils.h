#ifndef _STRING_UTILS_H_
#define _STRING_UTILS_H_

/**
 * Utilities and stl typedefs for basic_string.
 */

#include <ctype.h>
#include <string>
#include <vector>

// Though hash_set was part of std:: in the 2.9x gcc series, it's been
// moved to ext/__gnu_cxx:: in 3.x
#if (__GNUC__ == 2 && __GNUC_MINOR__ >= 95)
#include <hash_set>
#include <hash_map>
#define _std std
#else
#include <ext/hash_set>
#include <ext/hash_map>
#define _std __gnu_cxx
#endif

/**
 * Hashing function class for std::strings.
 */
struct StringHash {
    size_t operator()(const std::string& str) const
    {
        return _std::__stl_hash_string(str.data());
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
 * A StringHashSet is a hash set with std::string members.
 */
class StringHashSet : public _std::hash_set<std::string, StringHash, StringEquals> {
public:
    void dump(const char* log) const;
};

/**
 * A StringHashMap is a hash map with std::string keys.
 */
template <class _Type> class StringHashMap :
    public _std::hash_map<std::string, _Type, StringHash, StringEquals> {
};

/**
 * A StringVector is a std::vector of std::strings.
 */
class StringVector : public std::vector<std::string> {
};

/**
 * Generate a hex string from a binary buffer.
 */
inline void
hex2str(std::string* str, const u_char* bp, size_t len)
{
    static const char hex[] = "0123456789abcdef";
    str->erase();
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

/**
 * Return true if the string contains only printable characters.
 */
inline bool
str_isascii(const u_char* bp, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        if (!isascii(*bp++)) {
            return false;
        }
    }

    return true;
}

#endif /* _STRING_UTILS_H_ */
