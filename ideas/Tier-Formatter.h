#ifndef tier_formatter_h
#define tier_formatter_h

#include <stdio.h>
#include <stdarg.h>

/**
 * This class is intended to be used with a modified implementation of
 * snprintf/vsnprintf, defined in Formatter.cc.
 *
 * The modification implements a special control code combination of
 * '*%p' that triggers a call to Formatter::format(), called on the
 * object pointer passed in the vararg list.
 *
 * For example:
 *
 * @code
 * class FormatterTest : public Formatter {
 * public:
 *     virtual int format(char* buf, size_t sz) {
 *         return snprintf(buf, sz, "FormatterTest");
 *     }
 * };
 *
 * FormatterTest f;
 * char buf[256];
 * snprintf(buf, sizeof(buf), "pointer %p, format *%p\n");
 * // buf contains "pointer 0xabcd1234, format FormatterTest\n"
 * @endcode
 */

class Formatter {
public:
    /**
     * Virtual callback, called from this vsnprintf implementation
     * whenever it encounters a format string of the form "*%p".
     *
     * The output routine must not write more than sz bytes. The
     * return is the number of bytes written, or the number of bytes
     * that would have been written if the output is truncated.
     */
    virtual int format(char* buf, size_t sz) = 0;

    /**
     * Assertion check to make sure that obj is a valid formatter.
     * This basically just makes sure that in any multiple inheritance
     * situation where each base class has virtual functions,
     * Formatter _must_ be the first class in the inheritance chain.
     */
    static inline void assert_valid(Formatter* obj);

#ifndef NDEBUG
#define FORMAT_MAGIC 0xffeeeedd
    Formatter() : format_magic_(FORMAT_MAGIC) {}
    unsigned int format_magic_;
#endif // NDEBUG
    
};

void __log_assert(bool x, const char* what, const char* file, int line);

inline void
Formatter::assert_valid(Formatter* obj)
{
#ifndef NDEBUG
    __log_assert(obj->format_magic_ == FORMAT_MAGIC,
                 "Formatter object invalid -- maybe need a cast to Formatter*",
                 __FILE__, __LINE__);
#endif
}

#endif /* tier_formatter_h */
