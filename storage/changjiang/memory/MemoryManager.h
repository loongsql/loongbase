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

#ifdef _WIN32
#define THROWS_NOTHING
#define THROWS_BAD_ALLOC
#define WINSTATIC  // static
#include <new>
#else
#include <new>
#define THROWS_NOTHING throw()
#define THROWS_BAD_ALLOC  // throw (std::bad_alloc)
#define WINSTATIC
#endif

#define _MFC_OVERRIDES_NEW

#ifndef ALWAYS_INLINE
#ifdef _WIN32
#define ALWAYS_INLINE inline /* for windows */
#elif __GNUC__
#define ALWAYS_INLINE extern inline __attribute__((always_inline)) /* for gcc \
                                                                    */
#else
#define ALWAYS_INLINE extern inline
#endif
#endif  // ALWAYS_INLINE

namespace Changjiang {
class Stream;
class InfoTable;
class MemMgr;
class MemControl;
struct MemObject;
}  // namespace Changjiang

#ifdef _DEBUG
namespace Changjiang {
extern void *MemMgrAllocateDebug(size_t s, const char *file, int line);
extern void *MemMgrPoolAllocateDebug(MemMgr *pool, size_t s, const char *file,
                                     int line);
}  // namespace Changjiang

#ifndef _WIN32
WINSTATIC ALWAYS_INLINE void *operator new(size_t s) THROWS_BAD_ALLOC {
  return Changjiang::MemMgrAllocateDebug(s, __FILE__, __LINE__);
}

WINSTATIC ALWAYS_INLINE void *operator new(size_t s, const int &n) {
  return Changjiang::MemMgrAllocateDebug(s, __FILE__, __LINE__);
}

WINSTATIC ALWAYS_INLINE void *operator new(size_t s, const char *file,
                                           int line) {
  return Changjiang::MemMgrAllocateDebug(s, file, line);
}

WINSTATIC ALWAYS_INLINE void *operator new[](size_t s) THROWS_BAD_ALLOC {
  return Changjiang::MemMgrAllocateDebug(s, __FILE__, __LINE__);
}

WINSTATIC ALWAYS_INLINE void *operator new[](size_t s, const char *file,
                                             int line) THROWS_BAD_ALLOC {
  return Changjiang::MemMgrAllocateDebug(s, file, line);
}
#endif

WINSTATIC ALWAYS_INLINE void *operator new(size_t s, Changjiang::MemMgr *pool)
    THROWS_BAD_ALLOC {
  return Changjiang::MemMgrPoolAllocateDebug(pool, s, __FILE__, __LINE__);
}

WINSTATIC ALWAYS_INLINE void *operator new(size_t s, Changjiang::MemMgr *pool,
                                           const char *file, int line) {
  return Changjiang::MemMgrPoolAllocateDebug(pool, s, file, line);
}

WINSTATIC ALWAYS_INLINE void *operator new[](size_t s, Changjiang::MemMgr *pool)
    THROWS_BAD_ALLOC {
  return Changjiang::MemMgrPoolAllocateDebug(pool, (unsigned int)s, __FILE__,
                                             __LINE__);
}

WINSTATIC ALWAYS_INLINE void *operator new[](size_t s, Changjiang::MemMgr *pool,
                                             const char *file, int line) {
  return Changjiang::MemMgrPoolAllocateDebug(pool, s, file, line);
}

//#define POOL_NEW(arg) new (arg, __FILE__, __LINE__)
//#define NEW new (__FILE__, __LINE__)
#define POOL_NEW(arg) new (arg)
#define NEW new

#ifndef new
#define new NEW
#endif

#else
namespace Changjiang {
extern void *MemMgrAllocate(size_t s);
extern void *MemMgrPoolAllocate(MemMgr *pool, size_t s);
}  // namespace Changjiang

#ifndef _WIN32

WINSTATIC ALWAYS_INLINE void *operator new(size_t s) THROWS_BAD_ALLOC {
  return Changjiang::MemMgrAllocate(s);
}

WINSTATIC ALWAYS_INLINE void *operator new(size_t s, Changjiang::MemMgr *pool)
    THROWS_BAD_ALLOC {
  return Changjiang::MemMgrPoolAllocate(pool, s);
}

WINSTATIC ALWAYS_INLINE void *operator new[](size_t s, Changjiang::MemMgr *pool)
    THROWS_BAD_ALLOC {
  return Changjiang::MemMgrPoolAllocate(pool, s);
}
#endif

WINSTATIC ALWAYS_INLINE void *operator new(size_t s,
                                           const int &n) THROWS_BAD_ALLOC {
  return Changjiang::MemMgrAllocate(s);
}

WINSTATIC ALWAYS_INLINE void *operator new[](size_t s) THROWS_BAD_ALLOC {
  return Changjiang::MemMgrAllocate(s);
}

#define POOL_NEW(arg) new (arg)
#define NEW new
#endif

namespace Changjiang {
enum MemMgrWhat {
  MemMgrSystemSummary,
  MemMgrSystemDetail,
  MemMgrRecordSummary,
  MemMgrRecordDetail
};

// Memory pool identifiers

static const int MemMgrDefault = 1;
static const int MemMgrGeneral = 2;
static const int MemMgrRecordData = 4;
static const int MemMgrRecord = 8;
static const int MemMgrRecordVersion = 16;
static const int MemMgrAllPools = -1;

// Memory pool group identifiers

static const int MemMgrControlGeneral = 0;  // general memory pool
static const int MemMgrControlRecord = 1;   // record and object memory pools

extern void MemMgrAnalyze(MemMgrWhat what, InfoTable *table);
extern void MemMgrRelease(void *object);
extern void MemMgrValidate(void *object);
extern void MemMgrAnalyze(int mask, Stream *stream);
extern void *MemMgrRecordAllocate(size_t size, const char *file, int line);
extern void MemMgrRecordDelete(char *record);
extern void MemMgrSetMaxRecordMember(long long size);
extern MemMgr *MemMgrGetFixedPool(int id);
extern MemControl *MemMgrGetControl(int id);

extern MemObject *MemMgrFindPriorBlock(void *block);
}  // namespace Changjiang

#ifndef _WIN32
// Added for gcc11
WINSTATIC ALWAYS_INLINE void operator delete(
    void *object, const std::nothrow_t &) THROWS_NOTHING {
  Changjiang::MemMgrRelease(object);
}
WINSTATIC ALWAYS_INLINE void operator delete(void *object,
                                             std::size_t) THROWS_NOTHING {
  Changjiang::MemMgrRelease(object);
}
WINSTATIC ALWAYS_INLINE void operator delete[](void *object,
                                               std::size_t) THROWS_NOTHING {
  Changjiang::MemMgrRelease(object);
}
// Added for gcc11 end

WINSTATIC ALWAYS_INLINE void operator delete(void *object) THROWS_NOTHING {
  Changjiang::MemMgrRelease(object);
}

WINSTATIC ALWAYS_INLINE void operator delete(void *object, const char *file,
                                             int line) {
  Changjiang::MemMgrRelease(object);
}

WINSTATIC ALWAYS_INLINE void operator delete[](void *object) THROWS_NOTHING {
  Changjiang::MemMgrRelease(object);
}

WINSTATIC ALWAYS_INLINE void operator delete[](void *object, const char *file,
                                               int line) {
  Changjiang::MemMgrRelease(object);
}

WINSTATIC ALWAYS_INLINE void operator delete(
    void *object, Changjiang::MemMgr *pool) THROWS_NOTHING {
  Changjiang::MemMgrRelease(object);
}

WINSTATIC ALWAYS_INLINE void operator delete(void *object,
                                             Changjiang::MemMgr *pool,
                                             const char *file, int line) {
  Changjiang::MemMgrRelease(object);
}
#else
template <class T>
void ChangjiangPostDelete(T *p) {
  if (p) {
    // p->~T();
    Changjiang::MemMgrRelease(p);
  }
}
#endif

namespace Changjiang {
extern void MemMgrValidate();
}  // namespace Changjiang
