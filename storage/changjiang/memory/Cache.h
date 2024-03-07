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

// Cache.h: interface for the Cache class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Page.h"
#include "SyncObject.h"
#include "Queue.h"

namespace Changjiang {

class Bdb;
class Dbb;
class PageWriter;
class Stream;
class Sync;
class Thread;
class Database;
class Bitmap;
class SectorCache;

class Cache {
 public:
  void shutdownNow(void);
  void shutdown(void);
  Bdb *probePage(Dbb *dbb, int32 pageNumber);
  void setPageWriter(PageWriter *writer);
  bool hasDirtyPages(Dbb *dbb);
  void flush(Dbb *dbb);
  void freePage(Dbb *dbb, int32 pageNumber);
  void validateUnique(Bdb *bdb);
  void analyze(Stream *stream);
  void writePage(Bdb *bdb, int type);
  void markClean(Bdb *bdb);
  void markDirty(Bdb *bdb);
  void validate();
  void moveToHead(Bdb *bdb);
  void flush(int64 arg);
  void validateCache(void);
  void syncFile(Dbb *dbb, const char *text);
  void ioThread(void);
  void shutdownThreads(void);
  bool continueWrite(Bdb *startingBdb);

  static void ioThread(void *arg);

  Bdb *fakePage(Dbb *dbb, int32 pageNumber, PageType type, TransId transId);
  Bdb *fetchPage(Dbb *dbb, int32 pageNumber, PageType type, LockType lockType);
  Bdb *trialFetch(Dbb *dbb, int32 pageNumber, LockType lockType);

  void analyzeFlush(void);
  static void openTraceFile(void);
  static void closeTraceFile(void);

  Cache(Database *db, int pageSize, int hashSize, int numberBuffers);
  virtual ~Cache();

 private:
  void startThreads();

 public:
  SyncObject syncObject;
  PageWriter *pageWriter;
  Database *database;
  int numberBuffers;
  bool shuttingDown;
  bool panicShutdown;
  bool flushing;
  bool recovering;

 protected:
  Bdb *findBuffer(Dbb *dbb, int pageNumber, LockType lockType);
  Bdb *findBdb(Dbb *dbb, int32 pageNumber);

  int64 flushArg;
  Bdb *bdbs;
  Bdb *endBdbs;
  Queue<Bdb> bufferQueue;
  Bdb **hashTable;
  Bdb *firstDirty;
  Bdb *lastDirty;
  Bitmap *flushBitmap;
  char **bufferHunks;
  Thread **ioThreads;
  SectorCache *sectorCache;
  SyncObject syncFlush;
  SyncObject syncDirty;
  SyncObject syncThreads;
  SyncObject syncWait;
  time_t flushStart;
  int flushPages;
  int physicalWrites;
  int hashSize;
  int pageSize;
  int upperFraction;
  int numberHunks;
  int numberDirtyPages;
  int numberIoThreads;
  volatile int bufferAge;

 private:
  int numberIoThreadsStarted;

 public:
  void flushWait(void);
};

}  // namespace Changjiang
