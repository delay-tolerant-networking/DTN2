
#include "StringBuffer.h"
#include <stdlib.h>

StringBuffer::StringBuffer(size_t initsz, const char* initstr)
{
    buf_ = (char*)malloc(initsz);
    buflen_ = initsz;
    len_ = 0;
    
    if (initstr) append(initstr);
}

StringBuffer::~StringBuffer()
{
    free(buf_);
    buf_ = 0;
    buflen_ = 0;
    len_ = 0;
}

void
StringBuffer::reserve(size_t sz, size_t grow)
{
    if (sz > buflen_) {
        if (grow == 0) {
            grow = buflen_ * 2;
        }
        
        buflen_ = grow;
        buf_ = (char*)realloc(buf_, buflen_);
    }
}

size_t
StringBuffer::append(const char* str, size_t len)
{
    if (len == 0) {
        len = strlen(str);
    }
    
    reserve(len_ + len);
    
    memcpy(&buf_[len_], str, len);
    len_ += len;
    return len;
}

size_t
StringBuffer::append(char c)
{
    reserve(len_ + 1);
    buf_[len_++] = c;
    return 1;
}

size_t
StringBuffer::appendf(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int nfree = buflen_ - len_;
    int ret = vsnprintf(&buf_[len_], nfree, fmt, ap);

    if (ret >= nfree) {
        reserve(buflen_ * 2);
        ret = vsnprintf(&buf_[len_], buflen_ - len_, fmt, ap);
    }

    len_ += ret;
    va_end(ap);
    return ret;
}

#ifdef STRINGBUFFER_TEST
int
main(int argc, char** argv)
{
    int n;
    StringBuffer b(10);

    b.append("abcdefg");
    printf("%.*s (len %d)\n", b.length(), b.data(), b.length());

    b.append("hiXXXX", 2);
    printf("%.*s (len %d)\n", b.length(), b.data(), b.length());

    n = b.appendf("%s%d%.*s", "jklm", 1234, 4, "nopqXXXX");
    printf("%.*s (len %d) (n %d)\n", b.length(), b.data(), b.length(), n);
}

#endif
