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

// Cache.cpp: implementation of the Cache class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include <stdio.h>
#include "Engine.h"
#include "Cache.h"
#include "BDB.h"
#include "Dbb.h"
#include "Page.h"
#include "IndexPage.h"
#include "PageInventoryPage.h"
#include "Sync.h"
#include "Log.h"
#include "LogLock.h"
#include "Stream.h"
#include "PageWriter.h"
#include "SQLError.h"
#include "Thread.h"
#include "Threads.h"
#include "DatabaseCopy.h"
#include "Database.h"
#include "Bitmap.h"
#include "Priority.h"
#include "SectorCache.h"

namespace Changjiang {

#define PARAMETER_UINT(_name, _text, _min, _default, _max, _flags, \
                       _update_function)                           \
  extern uint changjiang_##_name;
#define PARAMETER_BOOL(_name, _text, _default, _flags, _update_function) \
  extern bool changjiang_##_name;
#include "StorageParameters.h"
#undef PARAMETER_UINT
#undef PARAMETER_BOOL
extern uint changjiang_io_threads;

//#define STOP_PAGE		15380
#define TRACE_FILE "cache.trace"

static FILE *traceFile;

static const uint64 cacheHunkSize = 1024 * 1024 * 128;
static const int ASYNC_BUFFER_SIZE = 1024000;
static const int sectorCacheSize = 20000000;
#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Cache::Cache(Database *db, int pageSz, int hashSz, int numBuffers) {
  // openTraceFile();
  database = db;
  shuttingDown = false;
  panicShutdown = false;
  pageSize = pageSz;
  hashSize = hashSz;
  numberBuffers = numBuffers;
  upperFraction = numberBuffers / 4;
  bufferAge = 0;
  firstDirty = NULL;
  lastDirty = NULL;
  numberDirtyPages = 0;
  pageWriter = NULL;
  hashTable = new Bdb *[hashSz];
  memset(hashTable, 0, sizeof(Bdb *) * hashSize);

  if (changjiang_use_sectorcache)
    sectorCache =
        new SectorCache(sectorCacheSize / SECTOR_BUFFER_SIZE, pageSize);

  uint64 n =
      ((uint64)pageSize * numberBuffers + cacheHunkSize - 1) / cacheHunkSize;
  numberHunks = (int)n;
  bufferHunks = new char *[numberHunks];
  memset(bufferHunks, 0, numberHunks * sizeof(char *));
  syncObject.setName("Cache::syncObject");
  syncFlush.setName("Cache::syncFlush");
  syncDirty.setName("Cache::syncDirty");
  syncThreads.setName("Cache::syncThreads");
  syncWait.setName("Cache::syncWait");
  bufferQueue.syncObject.setName("Cache::bufferQueue.syncObject");

  flushBitmap = new Bitmap;
  numberIoThreads = changjiang_io_threads;
  ioThreads = NULL;
  flushing = false;
  recovering = false;

  try {
    bdbs = new Bdb[numberBuffers];
    endBdbs = bdbs + numberBuffers;
    int remaining = 0;
    int hunk = 0;
    int allocated = 0;
    char *stuff = NULL;

    for (Bdb *bdb = bdbs; bdb < endBdbs; ++bdb, --remaining) {
      if (remaining == 0) {
        remaining =
            MIN(numberBuffers - allocated, (int)(cacheHunkSize / pageSize));
        stuff = bufferHunks[hunk++] = new char[pageSize * (remaining + 1)];
        stuff = (char *)(((UIPTR)stuff + pageSize - 1) / pageSize * pageSize);
        allocated += remaining;
      }

      bdb->cache = this;
      bufferQueue.append(bdb);
      bdb->buffer = (Page *)stuff;
      stuff += pageSize;
    }
  } catch (...) {
    delete[] bdbs;

    for (int n = 0; n < numberHunks; ++n) delete[] bufferHunks[n];

    delete[] bufferHunks;

    throw;
  }

  validateCache();

  startThreads();
}

Cache::~Cache() {
  if (traceFile) closeTraceFile();

  delete[] hashTable;
  delete[] bdbs;
  delete[] ioThreads;
  delete flushBitmap;

  if (changjiang_use_sectorcache) delete sectorCache;

  if (bufferHunks) {
    for (int n = 0; n < numberHunks; ++n) delete[] bufferHunks[n];

    delete[] bufferHunks;
  }
}

Bdb *Cache::probePage(Dbb *dbb, int32 pageNumber) {
  ASSERT(pageNumber >= 0);
  Sync sync(&syncObject, "Cache::probePage");
  sync.lock(Shared);
  Bdb *bdb = findBdb(dbb, pageNumber);

  if (bdb) {
    bdb->incrementUseCount(ADD_HISTORY);
    sync.unlock();

    if (bdb->buffer->pageType == PAGE_free) {
      bdb->decrementUseCount(REL_HISTORY);

      return NULL;
    }

    bdb->addRef(Shared COMMA_ADD_HISTORY);
    bdb->decrementUseCount(REL_HISTORY);

    return bdb;
  }

  return NULL;
}

Bdb *Cache::findBdb(Dbb *dbb, int32 pageNumber) {
  for (Bdb *bdb = hashTable[pageNumber % hashSize]; bdb; bdb = bdb->hash)
    if (bdb->pageNumber == pageNumber && bdb->dbb == dbb) return bdb;

  return NULL;
}

Bdb *Cache::fetchPage(Dbb *dbb, int32 pageNumber, PageType pageType,
                      LockType lockType) {
  if (panicShutdown) {
    Thread *thread = Thread::getThread("Cache::fetchPage");

    if (thread->pageMarks == 0)
      throw SQLError(RUNTIME_ERROR, "Emergency shut is underway");
  }

#ifdef STOP_PAGE
  if (pageNumber == STOP_PAGE)
    Log::debug("fetching page %d/%d\n", pageNumber, dbb->tableSpaceId);
#endif

  ASSERT(pageNumber >= 0);

  if (recovering && pageType != PAGE_inventory &&
      !PageInventoryPage::isPageInUse(dbb, pageNumber)) {
    Log::debug(
        "During recovery, fetched page %d tablespace %d type %d marked free in "
        "PIP\n",
        pageNumber, dbb->tableSpaceId, pageType);
    PageInventoryPage::markPageInUse(dbb, pageNumber, 0);
  }

  int slot = pageNumber % hashSize;
  LockType actual = lockType;
  Sync sync(&syncObject, "Cache::fetchPage");
  sync.lock(Shared);
  int hit = 0;

  /* If we already have a buffer for this go, we're done */

  Bdb *bdb;

  for (bdb = hashTable[slot]; bdb; bdb = bdb->hash)
    if (bdb->pageNumber == pageNumber && bdb->dbb == dbb) {
      // syncObject.validateShared("Cache::fetchPage");
      bdb->incrementUseCount(ADD_HISTORY);
      sync.unlock();
      bdb->addRef(lockType COMMA_ADD_HISTORY);
      bdb->decrementUseCount(REL_HISTORY);
      hit = 1;
      break;
    }

  if (!bdb) {
    sync.unlock();
    actual = Exclusive;
    sync.lock(Exclusive);

    for (bdb = hashTable[slot]; bdb; bdb = bdb->hash)
      if (bdb->pageNumber == pageNumber && bdb->dbb == dbb) {
        // syncObject.validateExclusive("Cache::fetchPage (retry)");
        bdb->incrementUseCount(ADD_HISTORY);
        sync.unlock();
        bdb->addRef(lockType COMMA_ADD_HISTORY);
        bdb->decrementUseCount(REL_HISTORY);
        hit = 2;
        break;
      }

    if (!bdb) {
      bdb = findBuffer(dbb, pageNumber, actual);
      moveToHead(bdb);
      sync.unlock();

#ifdef STOP_PAGE
      if (bdb->pageNumber == STOP_PAGE)
        Log::debug("reading page %d/%d\n", bdb->pageNumber, dbb->tableSpaceId);
#endif

      Priority priority(database->ioScheduler);
      priority.schedule(PRIORITY_MEDIUM);
      if (changjiang_use_sectorcache)
        sectorCache->readPage(bdb);
      else
        dbb->readPage(bdb);
      priority.finished();
      if (bdb->buffer->pageNumber != pageNumber) {
        FATAL("page %d tablespace %d, got wrong page number %d\n", pageNumber,
              dbb->tableSpaceId, bdb->buffer->pageNumber);
      }
      if (actual != lockType) bdb->downGrade(lockType);
    }
  }

  Page *page = bdb->buffer;

  if (pageType && page->pageType != pageType) {
    FATAL("page %d tablespace %d, wrong page type, expected %d got %d\n",
          bdb->pageNumber, dbb->tableSpaceId, pageType, page->pageType);
  }

  // If buffer has moved out of the upper "fraction" of the LRU queue, move it
  // back up

  if (bdb->age < bufferAge - (uint64)upperFraction) {
    sync.lock(Exclusive);
    moveToHead(bdb);
  }

  ASSERT(bdb->pageNumber == pageNumber);
  ASSERT(bdb->dbb == dbb);
  ASSERT(bdb->useCount > 0);

  return bdb;
}

Bdb *Cache::fakePage(Dbb *dbb, int32 pageNumber, PageType type,
                     TransId transId) {
  Sync sync(&syncObject, "Cache::fakePage");
  sync.lock(Exclusive);
  int slot = pageNumber % hashSize;

#ifdef STOP_PAGE
  if (pageNumber == STOP_PAGE)
    Log::debug("faking page %d/%d\n", pageNumber, dbb->tableSpaceId);
#endif

  /* If we already have a buffer for this, we're done */

  Bdb *bdb;

  for (bdb = hashTable[slot]; bdb; bdb = bdb->hash)
    if (bdb->pageNumber == pageNumber && bdb->dbb == dbb) {
      if (bdb->syncObject.isLocked()) {
        // The pageWriter may still be cleaning up this freed page with a shared
        // lock
        ASSERT(bdb->buffer->pageType == PAGE_free);
        ASSERT(bdb->syncObject.getState() >= 0);
      }

      bdb->addRef(Exclusive COMMA_ADD_HISTORY);

      break;
    }

  if (!bdb) bdb = findBuffer(dbb, pageNumber, Exclusive);

  if (!dbb->isReadOnly) bdb->mark(transId);

  memset(bdb->buffer, 0, pageSize);
  bdb->setPageHeader(type);
  moveToHead(bdb);

  return bdb;
}

void Cache::flush(int64 arg) {
  Sync flushLock(&syncFlush, "Cache::flush(1)");
  Sync sync(&syncDirty, "Cache::flush(2)");
  flushLock.lock(Exclusive);

  if (flushing || shuttingDown) return;

  syncWait.lock(NULL, Exclusive);
  sync.lock(Shared);
  // Log::debug(%d: "Initiating flush\n", dbb->deltaTime);
  flushArg = arg;
  flushPages = 0;
  physicalWrites = 0;

  for (Bdb *bdb = firstDirty; bdb; bdb = bdb->nextDirty) {
    bdb->flushIt = true;
    flushBitmap->set(bdb->pageNumber);
    ++flushPages;
  }

  if (traceFile) analyzeFlush();

  flushStart = database->timestamp;
  flushing = true;
  sync.unlock();
  flushLock.unlock();

  for (int n = 0; n < numberIoThreads; ++n)
    if (ioThreads[n]) ioThreads[n]->wake();
}

void Cache::moveToHead(Bdb *bdb) {
  bdb->age = bufferAge++;
  bufferQueue.remove(bdb);
  bufferQueue.prepend(bdb);
  // validateUnique (bdb);
}

Bdb *Cache::findBuffer(Dbb *dbb, int pageNumber, LockType lockType) {
  // syncObject.validateExclusive("Cache::findBuffer");
  int slot = pageNumber % hashSize;
  Sync sync(&syncDirty, "Cache::findBuffer");

  /* Find least recently used, not-in-use buffer */

  Bdb *bdb;

  // Find a candidate BDB.

  for (;;) {
    for (bdb = bufferQueue.last; bdb; bdb = bdb->prior)
      if (bdb->useCount == 0) break;

    if (!bdb) throw SQLError(RUNTIME_ERROR, "buffer pool is exhausted\n");

    if (!bdb->isDirty) break;

    writePage(bdb, WRITE_TYPE_REUSE);
  }

  /* Unlink its old incarnation from the page/hash table */

  if (bdb->pageNumber >= 0)
    for (Bdb **ptr = hashTable + bdb->pageNumber % hashSize;;
         ptr = &(*ptr)->hash)
      if (*ptr == bdb) {
        *ptr = bdb->hash;
        break;
      } else
        ASSERT(*ptr);

  bdb->addRef(lockType COMMA_ADD_HISTORY);

  /* Set new page number and relink into hash table */

  bdb->hash = hashTable[slot];
  hashTable[slot] = bdb;
  bdb->pageNumber = pageNumber;
  bdb->dbb = dbb;

#ifdef COLLECT_BDB_HISTORY
  bdb->initHistory();
#endif

  return bdb;
}

void Cache::validate() {
  for (Bdb *bdb = bufferQueue.last; bdb; bdb = bdb->prior) {
    // IndexPage *page = (IndexPage*) bdb->buffer;
    ASSERT(bdb->useCount == 0);
  }
}

void Cache::markDirty(Bdb *bdb) {
  Sync sync(&syncDirty, "Cache::markDirty");
  sync.lock(Exclusive);
  bdb->nextDirty = NULL;
  bdb->priorDirty = lastDirty;

  if (lastDirty)
    lastDirty->nextDirty = bdb;
  else
    firstDirty = bdb;

  lastDirty = bdb;
  ++numberDirtyPages;
  // validateUnique (bdb);
}

void Cache::markClean(Bdb *bdb) {
  Sync sync(&syncDirty, "Cache::markClean");
  sync.lock(Exclusive);

  /***
  if (bdb->flushIt)
          Log::debug(" Cleaning page %d in %s marked for flush\n",
  bdb->pageNumber, (const char*) bdb->dbb->fileName);
  ***/

  bdb->flushIt = false;
  --numberDirtyPages;

  if (bdb == lastDirty) lastDirty = bdb->priorDirty;

  if (bdb->priorDirty) bdb->priorDirty->nextDirty = bdb->nextDirty;

  if (bdb->nextDirty) bdb->nextDirty->priorDirty = bdb->priorDirty;

  if (bdb == firstDirty) firstDirty = bdb->nextDirty;

  bdb->nextDirty = NULL;
  bdb->priorDirty = NULL;
}

void Cache::writePage(Bdb *bdb, int type) {
  Sync writer(&bdb->syncWrite, "Cache::writePage(1)");
  writer.lock(Exclusive);

  if (!bdb->isDirty) {
    // Log::debug("Cache::writePage: page %d not dirty\n", bdb->pageNumber);
    markClean(bdb);

    return;
  }

  // ASSERT(!(bdb->flags & BDB_write_pending));
  Dbb *dbb = bdb->dbb;
  ASSERT(database);
  markClean(bdb);
  // time_t start = database->timestamp;
  Priority priority(database->ioScheduler);
  priority.schedule(PRIORITY_MEDIUM);

  try {
    if (changjiang_use_sectorcache) sectorCache->writePage(bdb);
    dbb->writePage(bdb, type);
  } catch (SQLException &exception) {
    priority.finished();

    if (exception.getSqlcode() != DEVICE_FULL) throw;

    database->setIOError(&exception);
    Thread *thread = Thread::getThread("Cache::writePage");

    for (bool error = true; error;) {
      if (thread->shutdownInProgress) return;

      thread->sleep(1000);

      try {
        priority.schedule(PRIORITY_MEDIUM);
        dbb->writePage(bdb, type);
        error = false;
        database->clearIOError();
      } catch (SQLException &exception2) {
        priority.finished();

        if (exception2.getSqlcode() != DEVICE_FULL) throw;
      }
    }
  }

  priority.finished();

  /***
  time_t delta = database->timestamp - start;

  if (delta > 1)
          Log::debug("Page %d took %d seconds to write\n", bdb->pageNumber,
  delta);
  ***/

#ifdef STOP_PAGE
  if (bdb->pageNumber == STOP_PAGE)
    Log::debug("writing page %d/%d\n", bdb->pageNumber, dbb->tableSpaceId);
#endif

  bdb->isDirty = false;

  if (pageWriter && bdb->isRegistered) {
    bdb->isRegistered = false;
    pageWriter->pageWritten(bdb->dbb, bdb->pageNumber);
  }

  if (dbb->shadows) {
    Sync sync(&dbb->syncClone, "Cache::writePage(2)");
    sync.lock(Shared);

    for (DatabaseCopy *shadow = dbb->shadows; shadow; shadow = shadow->next)
      shadow->rewritePage(bdb);
  }
}

void Cache::analyze(Stream *stream) {
  Sync sync(&syncDirty, "Cache::analyze");
  sync.lock(Shared);
  int inUse = 0;
  int dirty = 0;
  int dirtyList = 0;
  int total = 0;
  Bdb *bdb;

  for (bdb = bdbs; bdb < endBdbs; ++bdb) {
    ++total;

    if (bdb->isDirty) ++dirty;

    if (bdb->useCount) ++inUse;
  }

  for (bdb = firstDirty; bdb; bdb = bdb->nextDirty) ++dirtyList;

  stream->format("Cache: %d pages, %d in use, %d dirty, %d in dirty chain\n",
                 total, inUse, dirty, dirtyList);
}

void Cache::validateUnique(Bdb *target) {
  int slot = target->pageNumber % hashSize;

  for (Bdb *bdb = hashTable[slot]; bdb; bdb = bdb->hash)
    ASSERT(bdb == target ||
           !(bdb->pageNumber == target->pageNumber && bdb->dbb == target->dbb));
}

void Cache::freePage(Dbb *dbb, int32 pageNumber) {
  Sync sync(&syncObject, "Cache::freePage");
  sync.lock(Shared);
  int slot = pageNumber % hashSize;

  // If page exists in cache (usual case), clean it up

  for (Bdb *bdb = hashTable[slot]; bdb; bdb = bdb->hash)
    if (bdb->pageNumber == pageNumber && bdb->dbb == dbb) {
      if (bdb->isDirty) {
        sync.unlock();
        markClean(bdb);
      }

      bdb->isDirty = false;
      break;
    }
}

void Cache::flush(Dbb *dbb) {
  // Sync sync (&syncDirty, "Cache::flush(1)");
  // sync.lock (Exclusive);
  Sync sync(&syncObject, "Cache::flush(3)");
  sync.lock(Shared);

  for (Bdb *bdb = bdbs; bdb < endBdbs; ++bdb)
    if (bdb->dbb == dbb) {
      if (bdb->isDirty) writePage(bdb, WRITE_TYPE_FORCE);

      bdb->dbb = NULL;
    }
}

bool Cache::hasDirtyPages(Dbb *dbb) {
  Sync sync(&syncDirty, "Cache::hasDirtyPages");
  sync.lock(Shared);

  for (Bdb *bdb = firstDirty; bdb; bdb = bdb->nextDirty)
    if (bdb->dbb == dbb) return true;

  return false;
}

void Cache::setPageWriter(PageWriter *writer) { pageWriter = writer; }

void Cache::validateCache(void) {
  // MemMgrValidate(bufferSpace);
}

Bdb *Cache::trialFetch(Dbb *dbb, int32 pageNumber, LockType lockType) {
  if (panicShutdown) {
    Thread *thread = Thread::getThread("Cache::trialFetch");

    if (thread->pageMarks == 0)
      throw SQLError(RUNTIME_ERROR, "Emergency shut is underway");
  }

  ASSERT(pageNumber >= 0);
  int slot = pageNumber % hashSize;
  Sync sync(&syncObject, "Cache::trialFetch");
  sync.lock(Shared);
  int hit = 0;

  /* If we already have a buffer for this go, we're done */

  Bdb *bdb;

  for (bdb = hashTable[slot]; bdb; bdb = bdb->hash)
    if (bdb->pageNumber == pageNumber && bdb->dbb == dbb) {
      // syncObject.validateShared("Cache::trialFetch");
      bdb->incrementUseCount(ADD_HISTORY);
      sync.unlock();
      bdb->addRef(lockType COMMA_ADD_HISTORY);
      bdb->decrementUseCount(REL_HISTORY);
      hit = 1;
      break;
    }

  return bdb;
}

void Cache::syncFile(Dbb *dbb, const char *text) {
  const char *fileName = dbb->fileName;
  int writes = dbb->writesSinceSync;
  time_t start = database->timestamp;
  dbb->sync();

  if (Log::isActive(LogInfo)) {
    time_t delta = database->timestamp - start;

    if (delta > 1)
      Log::log(LogInfo, "%d: %s %s sync: %d pages in %d seconds\n",
               database->deltaTime, fileName, text, writes, delta);
  }
}

void Cache::ioThread(void *arg) {
#ifdef _PTHREADS
  prctl(PR_SET_NAME, "cj_cacheio");
#endif
  ((Cache *)arg)->ioThread();
}

void Cache::ioThread(void) {
  Sync syncThread(&syncThreads, "Cache::ioThread(1)");
  syncThread.lock(Shared);
  Sync flushLock(&syncFlush, "Cache::ioThread(2)");
  Sync sync(&syncObject, "Cache::ioThread(3)");
  Priority priority(database->ioScheduler);
  Thread *thread = Thread::getThread("Cache::ioThread");
  UCHAR *rawBuffer = new UCHAR[ASYNC_BUFFER_SIZE];
  UCHAR *buffer =
      (UCHAR *)(((UIPTR)rawBuffer + pageSize - 1) / pageSize * pageSize);
  UCHAR *end =
      (UCHAR *)((UIPTR)(rawBuffer + ASYNC_BUFFER_SIZE) / pageSize * pageSize);
  flushLock.lock(Exclusive);

  // Update that a new IO thread has started. Use flushLock to protect this.

  numberIoThreadsStarted++;

  // This is the main loop.  Write blocks until there's nothing to do, then
  // sleep

  for (;;) {
    int32 pageNumber = flushBitmap->nextSet(0);
    int count;
    Dbb *dbb;

    if (pageNumber >= 0) {
      int slot = pageNumber % hashSize;
      bool hit = false;
      Bdb *bdbList = NULL;
      UCHAR *p = buffer;
      sync.lock(Shared);

      // Look for a page to flush.  Then get all his friends

      for (Bdb *bdb = hashTable[slot]; bdb; bdb = bdb->hash)
        if (bdb->pageNumber == pageNumber && bdb->flushIt && bdb->isDirty) {
          hit = true;
          count = 0;
          dbb = bdb->dbb;

          if (!bdb->hash) flushBitmap->clear(pageNumber);

          while (p < end) {
            ++count;
            bdb->incrementUseCount(ADD_HISTORY);
            sync.unlock();
            bdb->addRef(Shared COMMA_ADD_HISTORY);
            if (changjiang_use_sectorcache) sectorCache->writePage(bdb);

            bdb->syncWrite.lock(NULL, Exclusive);
            bdb->ioThreadNext = bdbList;
            bdbList = bdb;

            // ASSERT(!(bdb->flags & BDB_write_pending));
            // bdb->flags |= BDB_write_pending;
            memcpy(p, bdb->buffer, pageSize);
            p += pageSize;
            bdb->flushIt = false;
            markClean(bdb);
            bdb->isDirty = false;
            bdb->release(REL_HISTORY);
            sync.lock(Shared);

            if (!(bdb = findBdb(dbb, bdb->pageNumber + 1))) break;

            if (!bdb->isDirty && !continueWrite(bdb)) break;
          }

          if (sync.state != None) sync.unlock();

          flushLock.unlock();
          // Log::debug(" %d Writing %s %d pages: %d - %d\n", thread->threadId,
          // (const char*) dbb->fileName, count, pageNumber, pageNumber + count
          // - 1);
          int length = (int)(p - buffer);
          priority.schedule(PRIORITY_LOW);

          try {
            priority.schedule(PRIORITY_LOW);
            dbb->writePages(pageNumber, length, buffer, WRITE_TYPE_FLUSH);
          } catch (SQLException &exception) {
            priority.finished();

            if (exception.getSqlcode() != DEVICE_FULL) throw;

            database->setIOError(&exception);

            for (bool error = true; error;) {
              if (thread->shutdownInProgress) {
                Bdb *next;

                for (bdb = bdbList; bdb; bdb = next) {
                  // bdb->flags &= ~BDB_write_pending;
                  next = bdb->ioThreadNext;
                  bdb->syncWrite.unlock();
                  bdb->decrementUseCount(REL_HISTORY);
                }

                return;
              }

              thread->sleep(1000);

              try {
                priority.schedule(PRIORITY_LOW);
                dbb->writePages(pageNumber, length, buffer, WRITE_TYPE_FLUSH);
                error = false;
                database->clearIOError();
              } catch (SQLException &exception2) {
                priority.finished();

                if (exception2.getSqlcode() != DEVICE_FULL) throw;
              }
            }
          }

          priority.finished();
          Bdb *next;

          for (bdb = bdbList; bdb; bdb = next) {
            // ASSERT(bdb->flags & BDB_write_pending);
            // bdb->flags &= ~BDB_write_pending;
            next = bdb->ioThreadNext;
            bdb->syncWrite.unlock();
            bdb->decrementUseCount(REL_HISTORY);
          }

          flushLock.lock(Exclusive);
          ++physicalWrites;

          break;
        }

      if (!hit) {
        sync.unlock();
        flushBitmap->clear(pageNumber);
      }
    } else {
      if (flushing) {
        int writes = physicalWrites;
        int pages = flushPages;
        int delta = (int)(database->timestamp - flushStart);
        flushing = false;
        syncWait.unlock();
        flushLock.unlock();

        if (writes > 0 && Log::isActive(LogInfo))
          Log::log(
              LogInfo,
              "%d: Cache flush: %d pages, %d writes in %d seconds (%d pps)\n",
              database->deltaTime, pages, writes, delta, pages / MAX(delta, 1));

        try {
          database->pageCacheFlushed(flushArg);
        } catch (...) {
          // Ignores any errors from writing the checkpoint
          // log record (ie. if we have issues with the stream log)
        }
      } else
        flushLock.unlock();

      // Check if there is another pending flush command already.
      // If not then we should check if we have been requested to
      // shutdown or to sleep until a new request arrives

      flushLock.lock(Exclusive);
      if (!flushing) {
        // Check whether we have been requested to exit the IO thread.
        // Note that this is neccessary to hold the flush lock when
        // checking the shutdown status to avoid that a new
        // flush request arriving just before the shutdown command is
        // overlooked.

        if (thread->shutdownInProgress) {
          flushLock.unlock();
          break;
        }
        flushLock.unlock();

        // No pending flush request so lets sleep

        thread->sleep();

        flushLock.lock(Exclusive);
      }
    }
  }

  delete[] rawBuffer;
}

bool Cache::continueWrite(Bdb *startingBdb) {
  Dbb *dbb = startingBdb->dbb;
  int clean = 1;
  int dirty = 0;

  for (int32 pageNumber = startingBdb->pageNumber + 1, end = pageNumber + 5;
       pageNumber < end; ++pageNumber) {
    Bdb *bdb = findBdb(dbb, pageNumber);

    if (dirty > clean) return true;

    if (!bdb) return dirty >= clean;

    if (bdb->isDirty)
      ++dirty;
    else
      ++clean;
  }

  return (dirty >= clean);
}

void Cache::shutdown(void) {
  // To avoid new flush requests we set status to shuttingDown

  Sync flushLock(&syncFlush, "Cache::shutdown(1)");
  flushLock.lock(Exclusive);
  shuttingDown = true;
  flushLock.unlock();

  shutdownThreads();
  Sync sync(&syncDirty, "Cache::shutdown(2)");
  sync.lock(Exclusive);

  for (Bdb *bdb = firstDirty; bdb; bdb = bdb->nextDirty)
    bdb->dbb->writePage(bdb, WRITE_TYPE_SHUTDOWN);
}

void Cache::shutdownNow(void) {
  panicShutdown = true;
  shutdown();
}

void Cache::shutdownThreads(void) {
  for (int n = 0; n < numberIoThreads; ++n) {
    database->threads->shutdown(ioThreads[n]);
    ioThreads[n] = 0;
  }

  Sync sync(&syncThreads, "Cache::shutdownThreads");
  sync.lock(Exclusive);
}

void Cache::analyzeFlush(void) {
  Dbb *dbb = NULL;
  Bdb *bdb;

  for (bdb = firstDirty; bdb; bdb = bdb->nextDirty)
    if (bdb->dbb->tableSpaceId == 1) {
      dbb = bdb->dbb;

      break;
    }

  if (!dbb) return;

  fprintf(traceFile, "-------- time %d -------\n", database->deltaTime);

  for (int pageNumber = 0;
       (pageNumber = flushBitmap->nextSet(pageNumber)) >= 0;)
    if ((bdb = findBdb(dbb, pageNumber))) {
      int start = pageNumber;
      int type = bdb->buffer->pageType;

      for (; (bdb = findBdb(dbb, ++pageNumber)) && bdb->flushIt;)
        ;

      fprintf(traceFile, " %d flushed: %d to %d, first type %d\n",
              pageNumber - start, start, pageNumber - 1, type);

      for (int max = pageNumber + 5;
           pageNumber < max && (bdb = findBdb(dbb, pageNumber)) &&
           !bdb->flushIt;
           ++pageNumber) {
        if (bdb->isDirty)
          fprintf(traceFile, "     %d dirty not flushed, type %d \n",
                  pageNumber, bdb->buffer->pageType);
        else
          fprintf(traceFile, "      %d not dirty, type %d\n", pageNumber,
                  bdb->buffer->pageType);
      }
    } else
      ++pageNumber;

  fflush(traceFile);
}

void Cache::openTraceFile(void) {
#ifdef TRACE_FILE
  if (traceFile) closeTraceFile();

  traceFile = fopen(TRACE_FILE, "w");
#endif
}

void Cache::closeTraceFile(void) {
#ifdef TRACE_FILE
  if (traceFile) {
    fclose(traceFile);
    traceFile = NULL;
  }
#endif
}

void Cache::startThreads() {
  // Allocate memory for array with pointers to the IO threads

  ioThreads = new Thread *[numberIoThreads];
  memset(ioThreads, 0, numberIoThreads * sizeof(ioThreads[0]));
  numberIoThreadsStarted = 0;

  // Start the IO threads

  for (int n = 0; n < numberIoThreads; ++n)
    ioThreads[n] =
        database->threads->start("Cache::Cache", &Cache::ioThread, this);

  // Wait for the IO threads to start

  while (true) {
    // numberIoThreadsStarted is protected by using syncFlush

    Sync sync(&syncFlush, "Cache::startThreads");
    sync.lock(Exclusive);

    if (numberIoThreadsStarted == numberIoThreads) break;

    sync.unlock();

    // Take a short sleep to avoid busy-waiting

    Thread::getThread("Cache::startThreads")->sleep(1);
  }
}

void Cache::flushWait(void) {
  Sync sync(&syncWait, "Cache::flushWait");
  sync.lock(Shared);
}

}  // namespace Changjiang
