#ifndef _ATOMIC_X86_H_
#define _ATOMIC_X86_H_

#include "debug/Debug.h"

/**
 * When we're not on a SMP platform, there's no need to lock the bus.
 */
#ifndef NO_SMP
#define LOCK "lock ; "
#else
#define LOCK ""
#endif

/*
 * We have to force gcc to not optimize with an alias.
 */
#define __noalias__(x) (*(volatile struct { int value; } *)x)

/**
 * Atomic addition function.
 *
 * @param i	integer value to add
 * @param v	pointer to current value
 * 
 */
static inline void
atomic_add(volatile void *v, int i)
{
    __asm__ __volatile__(
        LOCK "addl %1,%0"
        :"=m" (__noalias__(v))
        :"ir" (i), "m" (__noalias__(v)));
}

/**
 * Atomic subtraction function.
 *
 * @param i	integer value to subtract
 * @param v	pointer to current value
 
 */
static inline void
atomic_sub(volatile void *v, int i)
{
    __asm__ __volatile__(
        LOCK "subl %1,%0"
        :"=m" (__noalias__(v))
        :"ir" (i), "m" (__noalias__(v)));
}

/**
 * Atomic increment.
 *
 * @param v	pointer to current value
 */
static inline void
atomic_incr(volatile void *v)
{
    __asm__ __volatile__(
        LOCK "incl %0"
        :"=m" (__noalias__(v))
        :"m" (__noalias__(v)));
}

/**
 * Atomic decrement.
 *
 * @param v	pointer to current value
 * 
 */ 
static inline void
atomic_decr(volatile void *v)
{
    __asm__ __volatile__(
        LOCK "decl %0"
        :"=m" (__noalias__(v))
        :"m" (__noalias__(v)));
}

/**
 * Atomic decrement and test.
 *
 * @return true if the value zero after the decrement, false
 * otherwise.
 *
 * @param v	pointer to current value
 * 
 */ 
static inline bool
atomic_decr_test(volatile void *v)
{
    unsigned char c;
    
    __asm__ __volatile__(
        LOCK "decl %0; sete %1"
        :"=m" (__noalias__(v)), "=qm" (c)
        :"m" (__noalias__(v)) : "memory");

    return (c != 0);
}

/**
 * Atomic compare and swap. Stores the new value iff the current value
 * is the expected old value.
 *
 * @param v 	pointer to current value
 * @param o 	old value to compare against
 * @param n 	new value to store
 *
 * @return 	the value of v before the swap
 */
static inline unsigned int
atomic_cmpxchg32(volatile void *v, unsigned int o, unsigned int n)
{
    __asm__ __volatile__(
	LOCK "cmpxchgl %1, %2"
	: "+a" (o)
	: "r" (n), "m" (__noalias__(v))
	: "memory");
    
    return o;
}

/**
 * Atomic increment function that returns the old value. Note that the
 * implementation loops until it can successfully do the operation and
 * store the value, so there is an extremely low chance that this will
 * never return.
 *
 * @param v 	pointer to current value
 */
static inline unsigned int
atomic_incr_ret(volatile void* v)
{
    register unsigned int o, n;
#if defined(NDEBUG) && NDEBUG == 1
    while (1)
#else
    register int j;
    for (j = 0; j < 1000000; ++j)
#endif
    {
        o = * (volatile unsigned int*)(v);
        n = o + 1;
        if (atomic_cmpxchg32(v, o, n) == o)
            return n;
    }
    
    NOTREACHED;
    return 0;
}

/**
 * Atomic addition function that returns the old value. Note that the
 * implementation loops until it can successfully do the operation and
 * store the value, so there is an extremely low chance that this will
 * never return.
 *
 * @param v 	pointer to current value
 * @param i 	integer to add
 */
static inline unsigned int
atomic_add32_ret(volatile void* v, unsigned int i)
{
    register unsigned int o, n;
    
#if defined(NDEBUG) && NDEBUG == 1
    while (1)
#else
    register int j;
    for (j = 0; j < 1000000; ++j)
#endif
    {
        o = * (volatile unsigned int*)(v);
        n = o + i;
        if (atomic_cmpxchg32(v, o, n) == o)
            return n;
    }
    
    NOTREACHED;
    return 0;
}

#endif /* _ATOMIC_X86_H_ */
