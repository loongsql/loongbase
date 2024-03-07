/* Copyright � 2006-2008 MySQL AB, 2009 Sun Microsystems, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

// Bdb.cpp: implementation of the Bdb class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "BDB.h"
#include "Cache.h"
#include "Interlock.h"
#include "PageWriter.h"
#include "Thread.h"
#include "SQLError.h"
#include "Dbb.h"
#include "Log.h"
#include "Database.h"
#include "Sync.h"
#include "Page.h"

namespace Changjiang {

//#define TRACE_PAGE 130049

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Bdb::Bdb() {
  cache = NULL;
  next = NULL;
  prior = NULL;
  hash = NULL;
  buffer = NULL;
  isDirty = false;
  isRegistered = false;
  pageNumber = -1;
  useCount = 0;
  age = 0;
#ifdef CHECK_STALLED_BDB
  stallCount = 0;
#endif  // CHECK_STALLED_BDB
  markingThread = NULL;
  priorDirty = nextDirty = NULL;
  flushIt = false;
  dbb = NULL;
  syncObject.setName("Bdb::syncObject");
  syncWrite.setName("Bdb::syncWrite");

#ifdef COLLECT_BDB_HISTORY
  syncHistory.setName("Bdb::syncHistory");
  lockType = None;
  initCount = 0;
  historyCount = 0;
  memset(history, 0, sizeof(history));
#endif
}

Bdb::~Bdb() {}

void Bdb::mark(TransId transId) {
  ASSERT(useCount > 0);
  ASSERT(lockType == Exclusive);
  ASSERT(!dbb->isReadOnly);
  transactionId = transId;
  // cache->validateCache();
  lastMark = cache->database->timestamp;

#ifdef TRACE_PAGE
  if (pageNumber == TRACE_PAGE)
    Log::debug("Marking page %d/%d\n", pageNumber, dbb->tableSpaceId);
#endif

  if (!markingThread) {
    markingThread = syncObject.getExclusiveThread();
    ++markingThread->pageMarks;
  }

  if (!isDirty) {
    isDirty = true;
    cache->markDirty(this);
  }
}

void Bdb::addRef(LockType lType) {
  incrementUseCount();
  syncObject.lock(NULL, lType);
  lockType = lType;
}

void Bdb::release() {
  ASSERT(useCount > 0);
  decrementUseCount();

  if (markingThread) {
    // cache->validateCache();
    --markingThread->pageMarks;
    markingThread = NULL;
  }

  if (isRegistered) {
    isRegistered = false;
    cache->pageWriter->writePage(dbb, pageNumber, transactionId);
  }

  syncObject.unlock(NULL, lockType);

  if (cache->panicShutdown) {
    Thread *thread = Thread::getThread("Cache::fetchPage");

    if (thread->pageMarks == 0)
      throw SQLError(RUNTIME_ERROR, "Emergency shut is underway");
  }
}

void Bdb::setPageHeader(short type) {
  buffer->pageType = type;

#ifdef HAVE_PAGE_NUMBER
  buffer->pageNumber = pageNumber;
#endif
}

void Bdb::downGrade(LockType lType) {
  ASSERT(lockType == Exclusive);
  lockType = lType;
  syncObject.downGrade(lType);
}

void Bdb::incrementUseCount() { INTERLOCKED_INCREMENT(useCount); }

void Bdb::decrementUseCount() {
  ASSERT(useCount > 0);
  INTERLOCKED_DECREMENT(useCount);
}

void Bdb::setWriter() { isRegistered = true; }

#ifdef COLLECT_BDB_HISTORY
void Bdb::addRef(LockType lType, int category, const char *file, int line) {
  addRef(lType);
  addHistory(category, file, line);
}

void Bdb::release(int category, const char *file, int line) {
  addHistory(category, file, line);
  release();
}

void Bdb::incrementUseCount(int category, const char *file, int line) {
  INTERLOCKED_INCREMENT(useCount);
  addHistory(category, file, line);
}

void Bdb::decrementUseCount(int category, const char *file, int line) {
  ASSERT(useCount > 0);
  addHistory(category, file, line);
  INTERLOCKED_DECREMENT(useCount);
}

void Bdb::initHistory() {
  initCount++;
  historyCount = 0;
  memset(history, 0, sizeof(history));
}

void Bdb::addHistory(int delta, const char *file, int line) {
  Sync sync(&syncHistory, "Bdb::addHistory");
  sync.lock(Exclusive);
  unsigned int historyOffset = historyCount++ % MAX_BDB_HISTORY;

#ifdef _WIN32
  history[historyOffset].threadId = (unsigned long)GetCurrentThreadId();
#endif
#ifdef _PTHREADS
  history[historyOffset].threadId = (unsigned long)pthread_self();
#endif

  // lockType is never set to None from Shared since a shared lock
  // can be added concurrently.   So let's try to catch a lock of None here.
  history[historyOffset].lockType = lockType;
  if (syncObject.getState() == 0) history[historyOffset].lockType = None;

  history[historyOffset].useCount = useCount;
  history[historyOffset].delta = delta;
  strncpy(history[historyOffset].file, file, BDB_HISTORY_FILE_LEN - 1);
  history[historyOffset].line = line;
}
#endif

}  // namespace Changjiang
