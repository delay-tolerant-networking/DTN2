#ifndef _ATOMIC_H_
#define _ATOMIC_H_

/**
 * Include the appropriate architecture-specific version of the atomic
 * functions here.
 */

// XXX/demmer some compile time hook should be here
#define INTEL_X86

#ifdef INTEL_X86
#include "Atomic-x86.h"
#else
#error "Need to define an Atomic.h variant for your architecture"

#endif /* _ATOMIC_H_ */
