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

#include "Mutex.h"
#include "SyncObject.h"
#include <limits.h>

namespace Changjiang {

#ifndef MEM_DEBUG
#ifdef _DEBUG
#define MEM_DEBUG
#endif
#endif

void MemMgrLogDump();

static const int defaultRounding = 8;
static const int defaultCutoff = 4096;
static const int defaultAllocation = 65536;
static const int defaultSignature = 12345678;

// Limit of the total memory for the pools within a MemControl group

static const uint64 MaxTotalMemory = 0xffffffffffffffffLL;

class MemMgr;
class Stream;
class InfoTable;
class MemControl;

class MemHeader {
 public:
  MemMgr *pool;
  int32 length;
#ifdef MEM_DEBUG
  int32 lineNumber;
  const char *fileName;
#endif
};

class MemBlock : public MemHeader {
 public:
  unsigned char body;
};

class MemBigObject;

class MemBigHeader {
 public:
  MemBigObject *next;
  MemBigObject *prior;
};

class MemBigObject : public MemBigHeader {
 public:
  MemHeader memHeader;
};

}  // namespace Changjiang

/***
class MemFreeBlock : public MemBigObject
{
public:
        //MemFreeBlock	*nextLarger;
        //MemFreeBlock	*priorSmaller;
        MemFreeBlock	*smaller;
        MemFreeBlock	*larger;
        MemFreeBlock	*nextTwin;
        MemFreeBlock	*priorTwin;
        int				balance;
};
***/
#include "MemFreeBlock.h"

namespace Changjiang {

class MemSmallHunk {
 public:
  MemSmallHunk *nextHunk;
  int length;
  unsigned char *memory;
  int spaceRemaining;
};

class MemBigHunk {
 public:
  MemBigHunk *nextHunk;
  int length;
  MemBigHeader blocks;
};

class MemMgr {
 public:
  MemMgr(int mgrId = MemMgrDefault, int rounding = defaultRounding,
         int cutoff = defaultCutoff, int minAllocation = defaultAllocation,
         bool *alive = NULL, MemControl *memCtrl = NULL);

  MemMgr(void *arg1, void *arg2);
  virtual ~MemMgr(void);

  int id;
  int signature;
  int roundingSize;
  int threshold;
  int minAllocation;
  int headerSize;
  int numberSmallHunks;
  int numberBigHunks;
  MemBlock **freeObjects;
  MemBigHunk *bigHunks;
  MemSmallHunk *smallHunks;
  MemControl *memControl;
  // MemFreeBlock	freeBlocks;
  MemFreeBlock freeBlockTree;
  MemFreeBlock junk;
  Mutex mutex;  // Win32 critical regions are faster than SyncObject
  int64 currentMemory;
  uint64 activeMemory;
  uint64 maxMemory;
  int blocksAllocated;
  int blocksActive;
  bool *isAlive;

  friend void MemMgrLogDump();

 protected:
  MemBlock *alloc(size_t size);
  static void corrupt(const char *text);

 public:
  void *allocate(size_t size);
  void *allocateDebug(size_t size, const char *fileName, int line);
  void releaseBlock(MemBlock *block);
  void validateBlock(MemBlock *block);
  void analyze(int mask, Stream *stream, InfoTable *summaryTable,
               InfoTable *detailTable);
  void validate();
  void releaseDebug(void *object);
  void remove(MemFreeBlock *block);
  void insert(MemFreeBlock *block);
  void debugStop(void);
  void validateFreeList(void);
  void validateBigBlock(MemBigObject *block);
  void *allocRaw(int length);
  //	void		releaseRaw(void *block);
  void releaseRaw(MemBlock **block);
  void releaseRaw(MemSmallHunk *block);
  void releaseRaw(MemBigHunk *block);

  virtual void *memoryIsExhausted(void);

  static void release(void *block);
  static void validate(void *object);
  static void validateBlock(void *object);
  static int blockSize(void *object);
};

}  // namespace Changjiang
