#ifndef _ATOMIC_H_
#define _ATOMIC_H_

/**
 * Include the appropriate architecture-specific version of the atomic
 * functions here.
 */

#if defined(__i386__)
#include "Atomic-x86.h"
#elif defined(__arm__)
#define __NO_ATOMIC__
#define NO_SMP
#else
#error "Need to define an Atomic.h variant for your architecture"
#endif

#endif /* _ATOMIC_H_ */
