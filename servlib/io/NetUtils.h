#ifndef _NET_UTILS_H_
#define _NET_UTILS_H_

#include <sys/types.h>
#include <netinet/in.h>

/**
 * Faster wrapper around inet_ntoa.
 */
extern const char* _intoa(u_int32_t addr, char* buf, size_t bufsize);

/**
 * Class used to allow for safe concurrent calls to _intoa within an
 * argument list.
 */
class Intoa {
public:
    Intoa(in_addr_t addr) {
        str_ = _intoa(addr, buf_, bufsize_);
    }
    
    ~Intoa() {
        buf_[0] = '\0';
    }
    
    const char* buf() { return str_; }

    static const int bufsize_ = sizeof(".xxx.xxx.xxx.xxx");
    
protected:
    char buf_[bufsize_];
    const char* str_;
};

/**
 * Wrapper macro to give the illusion that intoa() is a function call.
 * Which it is, really... or more accurately two inlined calls and one
 * function call.
 */
#define intoa(addr) Intoa(addr).buf()

/**
 * Utility wrapper around the ::gethostbyname() system call
 */
extern int gethostbyname(const char* name, in_addr_t* addrp);

#endif /* _NET_UTILS_H_ */
