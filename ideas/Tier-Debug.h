#ifndef DEBUG_H
#define DEBUG_H

#include <features.h>
#include <stdlib.h>
#include "Log.h"

void tier_abort() __attribute__((__noreturn__));

#define ABORT() tier_abort()

#define ASSERT(x)  			                                      \
    do { if (! (x)) {                                                         \
        ::logf("/assert", LOG_CRIT, "ASSERTION FAILED (" #x ") at %s:%d\n",   \
             __FILE__, __LINE__);                                             \
        tier_abort();                                                         \
    } } while(0)

#define PANIC(x)                                                              \
    do {                                                                      \
       ::logf("/panic", LOG_CRIT, "PANIC " x " at %s:%d\n",                   \
             __FILE__, __LINE__);                                             \
        tier_abort();                                                         \
    } while(0)

#define NOTREACHED                                                            \
    do { ::logf("/assert", LOG_CRIT, "NOTREACHED REACHED at %s:%d\n",         \
             __FILE__, __LINE__);                                             \
        tier_abort();                                                         \
    } while(0);

#define NOTIMPLEMENTED                                                        \
    do { ::logf("/assert", LOG_CRIT, "%s NOT IMPLEMENTED at %s:%d\n",         \
             __FUNCTION__, __FILE__, __LINE__);                               \
        tier_abort();                                                         \
    } while(0);

// Compile time static checking
#define STATIC_ASSERT(_x, _what)                \
do {                                            \
    char _what[ (_x) ? 0 : -1 ];                \
    (void)_what;				\
} while(0)


// print out the bytes values in a blob in hex
void
dbg_print_bytes(
    FILE* out,
    char* blob,
    unsigned int len
);

#undef abort

#endif /* DEBUG_H */
