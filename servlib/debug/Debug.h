#ifndef _DEBUG_H_
#define _DEBUG_H_

#include "Log.h"

#define ASSERT(x)  			                                      \
    do { if (! (x)) {                                                         \
        ::logf("/assert", LOG_CRIT, "ASSERTION FAILED (" #x ") at %s:%d\n",   \
             __FILE__, __LINE__);                                             \
        abort();                                                              \
    } } while(0)

#define PANIC(x, args...)                                                     \
    do {                                                                      \
       ::logf("/panic", LOG_CRIT, "PANIC at %s:%d: " x,                       \
             __FILE__, __LINE__ , ## args);                                   \
        abort();                                                              \
    } while(0)

#define NOTREACHED                                                            \
    do { ::logf("/assert", LOG_CRIT, "NOTREACHED REACHED at %s:%d\n",         \
             __FILE__, __LINE__);                                             \
        abort();                                                              \
    } while(0);

#define NOTIMPLEMENTED                                                        \
    do { ::logf("/assert", LOG_CRIT, "%s NOT IMPLEMENTED at %s:%d\n",         \
             __FUNCTION__, __FILE__, __LINE__);                               \
        abort();                                                              \
    } while(0);

// Compile time static checking
#define STATIC_ASSERT(_x, _what)                \
do {                                            \
    char _what[ (_x) ? 0 : -1 ];                \
    (void)_what;				\
} while(0)

#endif /* _DEBUG_H_ */
