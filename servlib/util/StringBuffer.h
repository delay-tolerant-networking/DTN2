#ifndef _STRING_BUFFER_H_
#define _STRING_BUFFER_H_

#include "debug/Log.h"

/**
 * Utility class that wraps a growable string buffer, much like that
 * which is done by the C++ ostringstream, but with printf() style
 * arguments instead of the << operator.
 *
 * The initial size of the buffer is 256 bytes, but can be overridden
 * at the constructor and/or through the reserve() function call.
 */
class StringBuffer {
public:
    /**
     * Constructor
     *
     * @param initsz the initial buffer size
     * @param initstr the initial buffer contents 
     */
    StringBuffer(size_t initsz = 256, const char* initstr = 0);

    /**
     * Constructor
     *
     * @param fmt the initial buffer contents
     */
    StringBuffer(const char* fmt, ...);

    /**
     * Destructor. Frees the buffer.
     */
    ~StringBuffer();

    /**
     * Reserve buffer space.
     *
     * @param sz 	the minimum required free buffer space
     * @param grow 	size to alloc if needed (default is 2x buflen)
     */
    void reserve(size_t sz, size_t grow = 0);
    
    /**
     * @return the data buffer (const variant).
     */
    const char* data() const { return buf_; }

    /**
     * @return the data buffer (non-const variant)
     */
    char* data() { return buf_; }

    /**
     * @return length of the buffer.
     */
    size_t length() { return len_; }

    /**
     * @return the data buffer, ensuring null termination.
     */
    char* c_str()
    {
        reserve(len_ + 1);
        buf_[len_] = '\0';
        return buf_;
    }

    /**
     * Append the string to the tail of the buffer.
     *
     * @param str string data
     * @param len string length (if unspecified, will call strlen())
     * @return the number of bytes written
     */
    size_t append(const char* str, size_t len = 0);

    /**
     * Append the string to the tail of the buffer.
     *
     * @param str string data
     * @return the number of bytes written
     */
    size_t append(const std::string& str)
    {
        return append(str.data(), str.length());
    }

    /**
     * Append the character to the tail of the buffer.
     *
     * @param c the character
     * @return the number of bytes written (always one)
     */
    size_t append(char c);

    /**
     * Formatting append function.
     *
     * @param fmt the format string
     * @return the number of bytes written
     */
    size_t appendf(const char* fmt, ...) PRINTFLIKE(2, 3);

    /**
     * Trim cnt characters from the tail of the string.
     */
    void trim(size_t cnt)
    {
        ASSERT(len_ >= cnt);
        len_ -= cnt;
    }

protected:
    char*  buf_;
    size_t len_;
    size_t buflen_;
};

#endif /* _STRING_BUFFER_H_ */
