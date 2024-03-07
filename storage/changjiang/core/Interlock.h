/* Copyright (c) 2024 LoongSQL, Inc.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#pragma once

namespace Changjiang {

#if defined(__sun)
#include <sys/atomic.h>

#if (defined(__SunOS_5_8) || defined(__SunOS_5_9)) && defined(__SUNPRO_CC) && \
    defined(__sparc)
#include "CompareAndSwapSparc.h"
#endif

#endif

#define INTERLOCKED_INCREMENT(variable) interlockedIncrement(&variable)
#define INTERLOCKED_DECREMENT(variable) interlockedDecrement(&variable)
#define INTERLOCKED_EXCHANGE(ptr, value) interlockedExchange(ptr, value)
#define INTERLOCKED_ADD(ptr, value) interlockedAdd(ptr, value)

#ifdef _WIN32
#include <windows.h>

#define COMPARE_EXCHANGE(target, compare, exchange) \
  (InterlockedCompareExchange(target, exchange, compare) == compare)

#define COMPARE_EXCHANGE_POINTER(target, compare, exchange)    \
  (InterlockedCompareExchangePointer((void *volatile *)target, \
                                     (void *)exchange,         \
                                     (void *)compare) == (void *)compare)
/*
  x86 compilers (both VS2003 or VS2005) never use instrinsics, but generate
  function calls to kernel32 instead, even in the optimized build.
  We force intrinsics as described in MSDN documentation for
  _InterlockedCompareExchange.
  x64 on the other hand, always uses intrinsic version, even in debug build
*/
#ifdef _M_IX86
#if (_MSC_VER >= 1400)
#include <intrin.h>
#else
/* Visual Studio 2003 and earlier do not have prototypes for atomic intrinsics
 */
extern "C" {
long _InterlockedIncrement(long volatile *Addend);
long _InterlockedDecrement(long volatile *Addend);
long _InterlockedExchangeAdd(long volatile *Addend, long Value);
long _InterlockedExchange(long volatile *Target, long Value);
long _InterlockedCompareExchange(long volatile *Target, long Value, long Comp);
}
#pragma intrinsic(_InterlockedIncrement)
#pragma intrinsic(_InterlockedDecrement)
#pragma intrinsic(_InterlockedExchange)
#pragma intrinsic(_InterlockedExchangeAdd)
#pragma intrinsic(_InterlockedCompareExchange)
#endif /* _MSC_VER */

#define InterlockedIncrement _InterlockedIncrement
#define InterlockedDecrement _InterlockedDecrement
#define InterlockedExchangeAdd _InterlockedExchangeAdd
#define InterlockedExchange _InterlockedExchange
#define InterlockedCompareExchange _InterlockedCompareExchange
/*
 No need to handle InterlockedCompareExchangePointer
 it is a defined as InterlockedCompareExchange.
*/
#endif /*_M_IX86*/

#else /* _WIN32 */

#define COMPARE_EXCHANGE(target, compare, exchange) \
  (inline_cas(target, compare, exchange))
#define COMPARE_EXCHANGE_POINTER(target, compare, exchange)      \
  (inline_cas_pointer((volatile void **)target, (void *)compare, \
                      (void *)exchange))

#endif /* _WIN32 */

inline int inline_cas(volatile int *target, int compare, int exchange) {
#ifdef _WIN32
  /*
     Windows 64 bit model is LLP64, longs are still 4 bytes like ints.
     Need to perform explicit type casting to make the compiler happy.
  */
  return COMPARE_EXCHANGE((volatile long *)target, compare, exchange);
#elif (defined(__i386) || defined(__x86_64__)) && defined(__GNUC__)
  char ret;
  __asm__ __volatile__(
      "lock\n\t"
      "cmpxchg %2,%1\n\t"
      "sete %0\n\t"
      : "=q"(ret), "+m"(*(target))
      : "r"(exchange), "a"(compare)
      : "cc", "memory");
  return ret;
#elif defined(__ppc__) || defined(__powerpc__)
  char ret = 0, tmp;
  __asm__ __volatile__(
      "lwsync\n\t"
      "0:\n\t"
      "lwarx %1,0,%4\n\t"  /* load and reserve */
      "cmpw %2,%1\n\t"     /* are the operands equal? */
      "bne- 1f\n\t"        /* skip if not */
      "stwcx. %3,0,%4\n\t" /* store new value if still reserved */
      "bne- 0b\n\t"        /* loop if lost reservation */
      "addi %0,0,1\n\t"
      "isync\n\t"
      "b 2f\n\t"
      "1:\n\t"
      "stwcx. %1,0,%4\n\t" /* remove reservation */
      "2:\n\t"
      : "+r"(ret), "=&r"(tmp)
      : "r"(compare), "r"(exchange), "b"(target)
      : "cr0", "memory");
  return ret;
  /*
     We are running gcc on SPARC.
   */
#elif defined(__sparc__)
  char ret;
  __asm__ __volatile__(
      "membar #LoadLoad | #LoadStore | #StoreLoad | #StoreStore\n\t"
      "cas [%2],%0,%1\n\t"
      "cmp %0,%1\n\t"
      "be,a 0f\n\t"
      "mov 1,%0\n\t" /* one insn after branch always executed */
      "clr %0\n\t"
      "0:\n\t"
      : "=r"(ret), "+r"(exchange)
      : "r"(target), "0"(compare)
      : "memory", "cc");
  return ret;
  /*
     We are running Sun Studio on Solaris >= 10 (SPARC or x86), use libc
     implementation Todo: get assembler version of atomic_cas_uint().
   */
#elif (defined(__SunOS_5_10) || defined(__SunOS_5_11)) && defined(__SUNPRO_CC)
  return (compare ==
          atomic_cas_uint((volatile uint_t *)target, compare, exchange));
  /*
     We are running Sun Studio on Solaris < 10 (SPARC, no x86 yet), use inline
     assembler
  */
#elif defined(__sparc) && defined(__SUNPRO_CC)
  return cas_sparc(target, compare, exchange);

#else
#error inline_cas not defined for this platform
#endif
}

inline char inline_cas_pointer(volatile void **target, void *compare,
                               void *exchange) {
#ifdef _WIN32
  return COMPARE_EXCHANGE_POINTER(target, compare, exchange);
#elif (defined(__i386) || defined(__x86_64__)) && defined(__GNUC__)
  char ret;
  __asm__ __volatile__(
      "lock\n\t"
      "cmpxchg %2,%1\n\t"
      "sete %0\n\t"
      : "=q"(ret), "+m"(*(target))
      : "r"(exchange), "a"(compare)
      : "cc", "memory");
  return ret;
#elif defined(__ppc__) || defined(__powerpc__)
  void *ret;
  if (sizeof(void *) == 8) {
    __asm__ __volatile__(
        "lwsync\n\t"
        "0:\n\t"
        "ldarx %0,0,%3\n\t"  /* load and reserve */
        "cmpw %1,%0\n\t"     /* are the operands equal? */
        "bne- 1f\n\t"        /* skip if not */
        "stdcx. %2,0,%3\n\t" /* store new value if still reserved */
        "bne- 0b\n\t"        /* loop if lost reservation */
        "isync\n\t"
        "b 2f\n\t"
        "1:\n\t"
        "stdcx. %0,0,%3\n\t" /* remove reservation */
        "2:\n\t"
        : "=&r"(ret)
        : "r"(compare), "r"(exchange), "b"(target)
        : "cr0", "memory");
  } else {
    __asm__ __volatile__(
        "lwsync\n\t"
        "0:\n\t"
        "lwarx %0,0,%3\n\t"  /* load and reserve */
        "cmpw %1,%0\n\t"     /* are the operands equal? */
        "bne- 1f\n\t"        /* skip if not */
        "stwcx. %2,0,%3\n\t" /* store new value if still reserved */
        "bne- 0b\n\t"        /* loop if lost reservation */
        "isync\n\t"
        "b 2f\n\t"
        "1:\n\t"
        "stwcx. %0,0,%3\n\t" /* remove reservation */
        "2:\n\t"
        : "=&r"(ret)
        : "r"(compare), "r"(exchange), "b"(target)
        : "cr0", "memory");
  }
  return (char)(ret == compare);
  /*
     We are running gcc on SPARC.
   */
#elif defined(__sparc__)
  char ret;
  if (sizeof(void *) == 8) {
    __asm__ __volatile__(
        "membar #LoadLoad | #LoadStore | #StoreLoad | #StoreStore\n\t"
        "casx [%2],%0,%1\n\t"
        "cmp %0,%1\n\t"
        "be,a 0f\n\t"
        "mov 1,%0\n\t" /* one insn after branch always executed */
        "clr %0\n\t"
        "0:\n\t"
        : "=r"(ret), "+r"(exchange)
        : "r"(target), "0"(compare)
        : "memory", "cc");
  } else {
    __asm__ __volatile__(
        "membar #LoadLoad | #LoadStore | #StoreLoad | #StoreStore\n\t"
        "cas [%2],%0,%1\n\t"
        "cmp %0,%1\n\t"
        "be,a 0f\n\t"
        "mov 1,%0\n\t" /* one insn after branch always executed */
        "clr %0\n\t"
        "0:\n\t"
        : "=r"(ret), "+r"(exchange)
        : "r"(target), "0"(compare)
        : "memory", "cc");
  }
  return ret;
  /*
     We are running Sun Studio on Solaris >= 10 (SPARC or x86), use libc
     implementation Todo: get assembler version of atomic_cas_ptr().
   */
#elif (defined(__SunOS_5_10) || defined(__SunOS_5_11)) && defined(__SUNPRO_CC)
  return (char)(compare == atomic_cas_ptr(target, compare, exchange));
  /*
     We are running Sun Studio on Solaris < 10 (SPARC, no x86 yet), use inline
     assembler
  */
#elif defined(__sparc) && defined(__SUNPRO_CC)
  return cas_pointer_sparc(target, compare, exchange);

#else
#error inline_cas not defined for this platform
#endif
}

inline INTERLOCK_TYPE interlockedIncrement(volatile INTERLOCK_TYPE *ptr) {
#ifdef _WIN32
  return InterlockedIncrement((long *)ptr);
#elif (defined(__i386) || defined(__x86_64__)) && defined(__GNUC__)
  INTERLOCK_TYPE ret = 1;
  __asm__ __volatile__(
      "lock\n\t"
      "xaddl %0,%1\n\t"
      : "+r"(ret)
      : "m"(*ptr)
      : "memory");
  return ret + 1;
#elif defined(__ppc__) || defined(__powerpc__)
  INTERLOCK_TYPE ret;
  __asm__ __volatile__(
      "lwsync\n\t"
      "0:\n\t"
      "lwarx %0,0,%2\n\t"
      "addi %0,%0,1\n\t"
      "stwcx. %0,0,%2\n\t"
      "bne- 0b\n\t"
      "isync\n\t"
      : "=&b"(ret), "+m"(*ptr)
      : "b"(ptr)
      : "cr0", "memory");
  return ret;
#else
  for (;;) {
    INTERLOCK_TYPE current = *ptr;
    INTERLOCK_TYPE ret = current + 1;
    if (COMPARE_EXCHANGE(ptr, current, ret)) return ret;
  }
#endif
}

inline INTERLOCK_TYPE interlockedDecrement(volatile INTERLOCK_TYPE *ptr) {
#ifdef _WIN32
  return InterlockedDecrement((long *)ptr);
#elif (defined(__i386) || defined(__x86_64__)) && defined(__GNUC__)
  INTERLOCK_TYPE ret = -1;
  __asm__ __volatile__(
      "lock\n\t"
      "xaddl %0,%1\n\t"
      : "+r"(ret)
      : "m"(*ptr)
      : "memory");
  return ret - 1;
#elif defined(__ppc__) || defined(__powerpc__)
  INTERLOCK_TYPE ret;
  __asm__ __volatile__(
      "lwsync\n\t"
      "0:\n\t"
      "lwarx %0,0,%2\n\t"
      "addi %0,%0,-1\n\t"
      "stwcx. %0,0,%2\n\t"
      "bne- 0b\n\t"
      "isync\n\t"
      : "=&b"(ret), "+m"(*ptr)
      : "b"(ptr)
      : "cr0", "memory");
  return ret;
#else
  for (;;) {
    INTERLOCK_TYPE current = *ptr;
    INTERLOCK_TYPE ret = current - 1;
    if (COMPARE_EXCHANGE(ptr, current, ret)) return ret;
  }
#endif
}

inline INTERLOCK_TYPE interlockedAdd(volatile INTERLOCK_TYPE *addend,
                                     INTERLOCK_TYPE value) {
#ifdef _WIN32
  return InterlockedExchangeAdd((long *)addend, value);
#elif (defined(__i386) || defined(__x86_64__)) && defined(__GNUC__)
  INTERLOCK_TYPE ret = value;
  __asm__ __volatile__(
      "lock\n\t"
      "xadd %0,%1\n\t"
      : "=r"(ret)
      : "m"(*(addend)), "0"(ret)
      : "memory");
  return ret + value;
#elif defined(__ppc__) || defined(__powerpc__)
  INTERLOCK_TYPE ret;
  __asm__ __volatile__(
      "lwsync\n\t"
      "0:\n\t"
      "lwarx %0,0,%2\n\t"
      "add %0,%0,%1\n\t"
      "stwcx. %0,0,%2\n\t"
      "bne- 0b\n\t"
      "isync\n\t"
      : "=&b"(ret)
      : "r"(value), "b"(addend)
      : "cr0", "memory");
  return ret;
#else
  for (;;) {
    INTERLOCK_TYPE current = *addend;
    INTERLOCK_TYPE ret = current + value;
    if (COMPARE_EXCHANGE(addend, current, ret)) return ret;
  }
#endif
}

inline INTERLOCK_TYPE interlockedExchange(volatile INTERLOCK_TYPE *addend,
                                          INTERLOCK_TYPE value) {
#ifdef _WIN32
  return InterlockedExchange((long *)addend, value);
#elif (defined(__i386) || defined(__x86_64__)) && defined(__GNUC__)
  long ret = value;
  __asm__ __volatile__(
      "lock\n\t"
      "xchg %0,%1\n\t"
      : "=r"(ret)
      : "m"(*(addend)), "0"(ret)
      : "memory");
  return ret;
#elif defined(__ppc__) || defined(__powerpc__)
  INTERLOCK_TYPE ret;
  __asm__ __volatile__(
      "lwsync\n\t"
      "0:\n\t"
      "lwarx %0,0,%3\n\t"  /* ret= *addend */
      "stwcx. %2,0,%3\n\t" /* if (reservation == addend)
                            *addend= value */
      "bne- 0b\n\t"        /* else
                               goto 0 */
      : "=&b"(ret), "+m"(*addend)
      : "r"(value), "b"(addend)
      : "cr0", "memory");
  return ret;
#elif defined(__sparc__)
  INTERLOCK_TYPE ret;
  __asm__ __volatile__(
      "membar #LoadLoad | #LoadStore | #StoreLoad | #StoreStore\n\t"
      "swap [%1],%0\n\t"
      : "=r"(ret)
      : "r"(addend), "0"(value)
      : "memory");
  return ret;
#else
  for (;;) {
    INTERLOCK_TYPE ret = *addend;
    if (COMPARE_EXCHANGE(addend, ret, value)) return ret;
  }
#endif
}

}  // namespace Changjiang
