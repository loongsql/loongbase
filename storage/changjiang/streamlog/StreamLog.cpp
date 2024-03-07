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

// StreamLog.cpp: implementation of the StreamLog class.
//
//////////////////////////////////////////////////////////////////////

#include <string.h>
#include <stdio.h>
#include <memory.h>
#include "Engine.h"
#include "StreamLog.h"
#include "Sync.h"
#include "SQLError.h"
#include "Thread.h"
#include "StreamLogFile.h"
#include "StreamLogControl.h"
#include "StreamLogWindow.h"
#include "StreamLogTransaction.h"
#include "Threads.h"
#include "Thread.h"
#include "Database.h"
#include "Dbb.h"
#include "Scheduler.h"
#include "Bitmap.h"
#include "Log.h"
#include "RecoveryPage.h"
#include "RecoveryObjects.h"
#include "SRLVersion.h"
#include "InfoTable.h"
#include "Configuration.h"
#include "TableSpaceManager.h"
#include "TableSpace.h"
#include "Porpoise.h"
#include "ErrorInjector.h"

namespace Changjiang {

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

static const int TRACE_PAGE = 0;
static const int RECORD_MAX = 100000;

extern uint changjiang_porpoise_threads;
extern uint64 changjiang_stream_log_file_size;

// static const int windowBuffers = 10;
static bool debug;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

StreamLog::StreamLog(Database *db, JString schedule, int maxTransactionBacklog)
    : Schedule(schedule) {
  database = db;
  defaultDbb = database->dbb;
  tableSpaceManager = database->tableSpaceManager;
  creationTime = database->creationTime;
  maxTransactions = maxTransactionBacklog;
  file1 = new StreamLogFile(database);
  file2 = new StreamLogFile(database);
  logControl = new StreamLogControl(this);
  active = false;
  nextBlockNumber = 0;
  firstWindow = NULL;
  lastWindow = NULL;
  freeWindows = NULL;
  writeWindow = NULL;
  writeBlock = NULL;
  memset(transactions, 0, sizeof(transactions));
  earliest = latest = NULL;
  lastFlushBlock = 1;
  lastReadBlock = 0;
  eventNumber = 0;
  endSrlQueue = NULL;
  writer = NULL;
  finishing = false;
  logicalFlushes = 0;
  physicalFlushes = 0;
  highWaterBlock = 0;
  nextLimboTransaction = 0;
  bufferSpace = NULL;
  recoveryPages = NULL;
  recoveryIndexes = NULL;
  recoveryOverflowPages = NULL;
  recoverySections = NULL;
  recoveryPhase = 0;
  tracePage = TRACE_PAGE;
  traceRecord = 0;
  chilledRecords = 0;
  chilledBytes = 0;
  windowReads = 0;
  priorWindowWrites = 0;
  windowWrites = 0;
  priorWindowReads = 0;
  maxWindows = 0;
  commitsComplete = 0;
  backlogStalls = 0;
  priorBacklogStalls = 0;
  priorCount = 0;
  priorDelta = 0;
  priorCommitsComplete = 0;
  priorWrites = 0;
  lastBlockWritten = 0;
  recoveryBlockNumber = 0;
  recovering = false;
  blocking = false;
  writeError = false;
  windowBuffers =
      MAX(database->configuration->streamLogWindows, SRL_MIN_WINDOWS);

  memset(tableSpaces, 0, sizeof(tableSpaces));
  tableSpaceInfo = NULL;

  syncWrite.setName("StreamLog::syncWrite");
  syncSections.setName("StreamLog::syncSections");
  syncIndexes.setName("StreamLog::syncIndexes");
  syncPorpoise.setName("StreamLog::syncPorpoise");
  syncUpdateStall.setName("StreamLog::syncUpdateStall");
  pending.syncObject.setName("StreamLog::pending.syncObject");
  inactions.syncObject.setName("StreamLog::inactions.syncObject");
  running.syncObject.setName("StreamLog::running.syncObject");
  porpoises = NULL;
  wantToSerializePorpoises = 0;
  serializePorpoises = 0;
  startRecordVirtualOffset = 0;
  logRotations = 0;

  for (uint n = 0; n < changjiang_porpoise_threads; ++n) {
    Porpoise *porpoise = new Porpoise(this);
    porpoise->next = porpoises;
    porpoises = porpoise;
  }
}

StreamLog::~StreamLog() {
  delete file1;
  delete file2;
  delete logControl;
  delete recoveryIndexes;
  delete recoverySections;
  delete recoveryOverflowPages;
  StreamLogWindow *window;

  while ((window = firstWindow)) {
    firstWindow = window->next;
    delete window;
  }

  while ((window = freeWindows)) {
    freeWindows = window->next;
    delete window;
  }

  while (!buffers.isEmpty()) buffers.pop();

  delete[] bufferSpace;

  while (tableSpaceInfo) {
    TableSpaceInfo *info = tableSpaceInfo;
    tableSpaceInfo = info->next;
    delete info;
  }

  for (Porpoise *porpoise; (porpoise = porpoises);) {
    porpoises = porpoise->next;
    delete porpoise;
  }
}

void StreamLog::open(JString fileRoot, bool createFlag) {
  file1->open(fileRoot + ".cl1", createFlag);
  file2->open(fileRoot + ".cl2", createFlag);

  int sectorSize = file1->sectorSize;
  int len = windowBuffers * SRL_WINDOW_SIZE + sectorSize;
  bufferSpace = new UCHAR[len];

#ifdef HAVE_purify
  bzero(bufferSpace, len);
#endif

  UCHAR *space = (UCHAR *)(((UIPTR)bufferSpace + sectorSize - 1) / sectorSize *
                           sectorSize);

  for (int n = 0; n < windowBuffers; ++n, space += SRL_WINDOW_SIZE)
    buffers.push(space);

  if (createFlag) initializeLog(1);
}

void StreamLog::start() {
  logControl->session.append(recoveryBlockNumber, 0);

  for (Porpoise *porpoise = porpoises; porpoise; porpoise = porpoise->next)
    porpoise->start();
}

// Setup up tableSpaceManager and open all tablespaces.
//
// This function should be called after phase 1 of recovery,
// At this stage, stream log is inspected and  up-to-date tablespace list
// is stored in SRLTableSpaces.
//
// If no SRLTableSpaces is not found in stream log, it means no changes
// were done to tablespace list after the last checkpoint and it is  safe
// to use TableSpaceManager::bootstrap() that reads from disk

static void openTableSpaces(Database *database) {
  TableSpaceManager *manager = SRLTableSpaces::getTableSpaceManager();

  if (!manager) {
    manager = new TableSpaceManager(database);
    manager->bootstrap(database->dbb->tableSpaceSectionId);
  }

  manager->openTableSpaces();

  database->tableSpaceManager = database->streamLog->tableSpaceManager =
      manager;
}

void StreamLog::recover() {
  Log::log("Recovering database %s ...\n", (const char *)defaultDbb->fileName);
  Sync sync(&syncWrite, "StreamLog::recover");
  sync.lock(Exclusive);
  recovering = true;
  recoveryPhase = 0;  // Find last block and recovery block

  defaultDbb->setCacheRecovering(true);

  // See if either or both files have valid blocks

  StreamLogWindow *window1 = allocWindow(file1, 0);
  StreamLogWindow *window2 = allocWindow(file2, 0);
  StreamLogBlock *block1 = window1->readFirstBlock();
  StreamLogBlock *block2 = window2->readFirstBlock();

  if (!block1) {
    window1->deactivateWindow();
    release(window1);
    window1 = NULL;
  }

  if (!block2) {
    window2->deactivateWindow();
    release(window2);
    window2 = NULL;
  }

  // Pick a window to start the search for the last block
  // If there are two files, we want to find the last block
  // in the second file.  Given the way the windows were
  // allocated, the fl2 window is linked in before the file1
  // window.  If they're in the other order swap them around.

  StreamLogWindow *recoveryWindow = NULL;
  StreamLogWindow *otherWindow = NULL;

  if (block1) {
    if (block2) {
      if (block1->blockNumber > block2->blockNumber) {
        recoveryWindow = window1;
        otherWindow = window2;
        window1->next = NULL;
        lastWindow = window2->next = window1;
        firstWindow = window1->prior = window2;
        window2->prior = NULL;
      } else {
        recoveryWindow = window2;
        otherWindow = window1;
      }
    } else
      recoveryWindow = window1;
  } else if (block2)
    recoveryWindow = window2;
  else {
    Log::log("No recovery block found\n");
    initializeLog(1);

    return;
  }

  // recoveryWindow is the first window in the second file.
  // Look through windows looking for the very last block,
  // starting by remember the first block in the recoverWindow
  // as recoveryBlock and it's number as recoveryBlockNumber.
  // These are used only to determine whether recovery is all
  // from one file, or whether it needes to start in the earlier
  // file.

  StreamLogBlock *recoveryBlock = recoveryWindow->firstBlock();
  recoveryBlockNumber = recoveryBlock->blockNumber;
  StreamLogBlock *lastBlock = findLastBlock(recoveryWindow);

  // The last block's "readBlock" number is the first block where we
  // actually perform recovery - it the first block that has something
  // to do with a transaction that's not write complete.

  // Debugging hint.  If there are unidentified objects in recovery -
  // unwritten pages or something, try starting recovery at the first
  // block in the first file - if and only if the two are contigous.

  uint64 readBlockNumber = lastBlock->readBlockNumber;

  if (readBlockNumber == 0) readBlockNumber = lastBlock->blockNumber;

  Log::log("\nFirst block in the stream log is " I64FORMAT "\n",
           (otherWindow) ? otherWindow->firstBlock()->blockNumber
                         : recoveryBlockNumber);
  Log::log("Last block in the stream log is " I64FORMAT "\n",
           lastBlock->blockNumber);
  Log::log("First block used in recovery is " I64FORMAT "\n", readBlockNumber);

  // the nextBlockNumber is the first block number to use after
  // recovery starts

  nextBlockNumber = lastBlock->blockNumber + 1;

  // If we're using both files, they'd better be contiguous.  Check and
  // when done, release any window allocated in the process. A side
  // effect of the check is that it sets up the first window's last
  // block number - that's used later.

  if (otherWindow) {
    StreamLogBlock *block = findLastBlock(otherWindow);

    if (block && block->blockNumber != (recoveryBlockNumber - 1))
      if (readBlockNumber < recoveryBlock->blockNumber)
        throw SQLError(LOG_ERROR, "corrupted stream log");

    StreamLogWindow *window = findWindowGivenBlock(block->blockNumber);
    window->deactivateWindow();
  }

  // OK.  Everything looks good.  Release the window we used
  // probing the log files, open a window on the first block
  // we're going to use for recovery.  That should work, but
  // if it doesn't, complain.

  lastWindow->deactivateWindow();
  StreamLogWindow *window = findWindowGivenBlock(readBlockNumber);

  if (!window)
    throw SQLError(LOG_ERROR, "can't find recovery block %d\n",
                   (int)readBlockNumber);

  window->activateWindow(true);
  StreamLogBlock *block = window->findBlock(readBlockNumber);
  StreamLogControl control(this);
  control.debug = debug;
  writeWindow = NULL;
  writeBlock = NULL;
  control.setWindow(window, block, 0);
  StreamLogRecord *record;

  // OK, we're good to go.  Start recovering.

  pass1 = true;
  recoveryPages = new RecoveryObjects(this);
  recoverySections = new RecoveryObjects(this);
  recoveryIndexes = new RecoveryObjects(this);
  recoveryOverflowPages = new RecoveryObjects(this);
  recoveryPhase = 1;

  // Phase 1 - read from the start to end of the part of the
  // log that's involved in recovery.  Take Inventory of streamLogTransactions,
  // recoveryObject states, last checkpoint)

  Log::log("Recovery phase 1...\n");

  unsigned long int recordCount = 0;

  while ((record = control.nextRecord())) {
    if (++recordCount % RECORD_MAX == 0)
      Log::log("Processed: %8ld\n", recordCount);
    record->pass1();
    ERROR_INJECTOR_EVENT(InjectorRecoveryPhase1, record->type);
  }

  Log::log("Processed: %8ld\n", recordCount);

  // control.debug = false;
  pass1 = false;
  control.setWindow(window, block, 0);

  recoveryPages->reset();
  recoveryIndexes->reset();
  recoverySections->reset();

  recoveryPhase = 2;  // Physical operations, skip old incarnations
  // Next, make a second pass to reallocate any necessary pages

  Log::log("Recovery phase 2...\n");
  recordCount = 0;

  openTableSpaces(database);

  while ((record = control.nextRecord())) {
    if (++recordCount % RECORD_MAX == 0)
      Log::log("Processed: %8ld\n", recordCount);

    if (!isTableSpaceDropped(record->tableSpaceId) ||
        record->type == srlDropTableSpace)
      record->pass2();
    ERROR_INJECTOR_EVENT(InjectorRecoveryPhase2, record->type);
  }

  Log::log("Processed: %8ld\n", recordCount);

  recoveryPages->reset();
  recoveryIndexes->reset();
  recoverySections->reset();

  // Now mark any transactions still pending as rolled back

  control.setWindow(window, block, 0);
  StreamLogTransaction *transaction;
  recoveryPhase = 3;  // Logical operations, skip old incarnations

  for (transaction = running.first; transaction;
       transaction = transaction->next)
    transaction->preRecovery();

  // Make a third pass doing things

  Log::log("Recovery phase 3...\n");
  recordCount = 0;

  while ((record = control.nextRecord())) {
    if (++recordCount % RECORD_MAX == 0)
      Log::log("Processed: %8ld\n", recordCount);

    if (!isTableSpaceDropped(record->tableSpaceId)) record->redo();
    ERROR_INJECTOR_EVENT(InjectorRecoveryPhase3, record->type);
  }

  Log::log("Processed: %8ld\n", recordCount);

  for (StreamLogTransaction *action, **ptr = &running.first; (action = *ptr);)
    if (action->completedRecovery()) {
      running.remove(action);
      delete action;
    } else
      ptr = &action->next;

  control.fini();
  window->deactivateWindow();
  window = findWindowGivenBlock(lastBlock->blockNumber);
  window->activateWindow(true);

  if ((writeBlock = window->nextAvailableBlock(lastBlock))) {
    writeWindow = window;
    initializeWriteBlock(writeBlock);
  } else {
    StreamLogFile *file = (window->file == file1) ? file2 : file1;
    writeWindow = allocWindow(file, 0);
    writeBlock = writeWindow->firstBlock();
    window->deactivateWindow();
    initializeWriteBlock(writeBlock);
    writeWindow->firstBlockNumber = writeBlock->blockNumber;
  }

  // preFlushBlock = writeBlock->blockNumber;
  delete recoveryPages;
  delete recoverySections;
  delete recoveryIndexes;
  delete recoveryOverflowPages;
  recoveryPages = NULL;
  recoveryIndexes = NULL;
  recoverySections = NULL;
  recoveryOverflowPages = NULL;

  droppedTablespaces.clear();

  for (window = firstWindow; window; window = window->next)
    if (!(window->inUse == 0 || window == writeWindow)) ASSERT(false);

  recovering = false;
  lastFlushBlock = writeBlock->blockNumber;
  // checkpoint(true);
  database->flush(lastBlockWritten);
  database->flushWait();

  for (TableSpaceInfo *info = tableSpaceInfo; info; info = info->next) {
    info->sectionUseVector.zap();
    info->indexUseVector.zap();
  }

  defaultDbb->setCacheRecovering(false);
  Log::log("Recovery complete\n");
  recoveryPhase = 0;  // Find last lock and recovery block
}

void StreamLog::overflowFlush(void) {
  ++eventNumber;
  ++logicalFlushes;

  // OK, we're going to do some writing.  Start by locking the stream log

  *writePtr++ = srlEnd | LOW_BYTE_FLAG;
  writeBlock->length = (int)(writePtr - (UCHAR *)writeBlock);
  writeWindow->setLastBlock(writeBlock);
  lastReadBlock = writeBlock->readBlockNumber = getReadBlock();
  // ASSERT(writeWindow->validate(writeBlock));

  // Keep track of what needs to be written

  StreamLogWindow *flushWindow = writeWindow;
  StreamLogBlock *flushBlock = writeBlock;
  createNewWindow();

  mutex.lock();

  try {
    flushWindow->write(flushBlock);
    lastBlockWritten = flushBlock->blockNumber;
  } catch (SQLException &exception) {
    setWriteError(exception.getSqlcode(), exception.getText());
    mutex.unlock();
    throw;
  }

  // ASSERT(flushWindow->validate(flushBlock));
  ++physicalFlushes;
  mutex.unlock();

  highWaterBlock = flushBlock->blockNumber;
  ASSERT(writer || !srlQueue);
}

uint64 StreamLog::flush(bool forceNewWindow, uint64 commitBlockNumber,
                        Sync *clientSync) {
  Sync sync(&syncWrite, "StreamLog::flush");
  Sync *syncPtr = clientSync;

  if (!syncPtr) {
    sync.lock(Exclusive);
    syncPtr = &sync;
  }

  ++eventNumber;
  Thread *thread = Thread::getThread("StreamLog::flush");
  ++logicalFlushes;
  thread->commitBlockNumber = commitBlockNumber;

  // Add ourselves to the queue to preserve our place in order

  thread->srlQueue = NULL;

  if (endSrlQueue)
    endSrlQueue->srlQueue = thread;
  else
    srlQueue = thread;

  endSrlQueue = thread;
  thread->wakeupType = None;

  // If there's a writer and it's not use, go to sleep

  if (writer && writer != thread) {
    syncPtr->unlock();

    for (;;) {
      thread->sleep();

      if (thread->wakeupType != None) break;
    }

    syncPtr->lock(Exclusive);

    if (commitBlockNumber <= highWaterBlock) {
      if (writer == thread || (!writer && srlQueue)) wakeupFlushQueue(thread);

      ASSERT(writer || !srlQueue);
      syncPtr->unlock();

      return nextBlockNumber;
    }
  }

  // OK, we're going to do some writing.

  ASSERT(writer == NULL || writer == thread);
  writer = thread;
  *writePtr++ = srlEnd | LOW_BYTE_FLAG;
  writeBlock->length = (int)(writePtr - (UCHAR *)writeBlock);
  lastReadBlock = writeBlock->readBlockNumber = getReadBlock();

  // Keep track of what needs to be written

  StreamLogWindow *flushWindow = writeWindow;
  StreamLogBlock *flushBlock = writeBlock;

  // Prepare the next write block for use while we're writing

  if (!forceNewWindow &&
      (writeBlock = writeWindow->nextAvailableBlock(writeBlock))) {
    initializeWriteBlock(writeBlock);
    writeBlock->readBlockNumber = lastReadBlock;
  } else {
    flushBlock->length = (int)(writePtr - (UCHAR *)flushBlock);
    createNewWindow();
  }

  // Everything is ready to go.  Release the locks and start writing

  mutex.lock();
  syncPtr->unlock();
  // Log::debug("Flushing log block %d, read block %d\n", (int)
  // flushBlock->blockNumber, (int) flushBlock->readBlockNumber);

  try {
    flushWindow->write(flushBlock);
    lastBlockWritten = flushBlock->blockNumber;
  } catch (SQLException &exception) {
    setWriteError(exception.getSqlcode(), exception.getText());
    mutex.unlock();
    throw;
  }

  // ASSERT(flushWindow->validate(flushBlock));
  ++physicalFlushes;
  mutex.unlock();

  // We're done.  Wake up anyone who got flushed and pick the next writer

  syncPtr->lock(Exclusive);
  highWaterBlock = MAX(highWaterBlock, flushBlock->blockNumber);
  wakeupFlushQueue(thread);
  ASSERT(writer != thread);
  ASSERT(writer || !srlQueue);
  // ASSERT(writeWindow->validate(writeBlock));
  syncPtr->unlock();

  return nextBlockNumber;
}

void StreamLog::createNewWindow(void) {
  // We need a new window.  Start by purging any unused windows

  while ((firstWindow->lastBlockNumber < lastReadBlock) &&
         (firstWindow->inUse == 0) && (firstWindow->useCount == 0) &&
         (firstWindow->interestCount == 0)) {
    release(firstWindow);
  }

  // Then figure out which file to use.  If the other isn't in use, take it

  StreamLogFile *file = (writeWindow->file == file1) ? file2 : file1;
  int64 fileOffset = 0;

  for (StreamLogWindow *window = firstWindow; window; window = window->next)
    if (window->file == file) {
      file = writeWindow->file;
      fileOffset = writeWindow->getNextFileOffset();
      break;
    }

  if (fileOffset == 0) {
    // Logfile switch, truncate file if required

    logRotations++;

    if (Log::isActive(LogInfo))
      Log::log(LogInfo, "%d: Switching log files (%d used)\n",
               database->deltaTime, file->highWater);

    if ((uint64)file->size() > changjiang_stream_log_file_size) {
      file->truncate((int64)changjiang_stream_log_file_size);
      ERROR_INJECTOR_EVENT(InjectorStreamLogTruncate, logRotations);
    }
  }

  writeWindow->deactivateWindow();
  writeWindow = allocWindow(file, fileOffset);
  writeWindow->firstBlockNumber = nextBlockNumber;
  initializeWriteBlock(writeWindow->firstBlock());
  ASSERT(writeWindow->firstBlockNumber == writeBlock->blockNumber);
  // ASSERT(writeWindow->validate(writeBlock));
}

void StreamLog::shutdown() {
  finishing = true;

  // Wake up porpoises that are not currently doing anything.
  // If they are active, they will see the shutdownInProgress flag.

  for (Porpoise *porpoise = porpoises; porpoise; porpoise = porpoise->next)
    if ((porpoise->workerThread) && porpoise->workerThread->sleeping &&
        !porpoise->active)
      porpoise->wakeup();

  // Wait for all porpoise threads to exit

  Sync sync(&syncPorpoise, "StreamLog::shutdown");
  sync.lock(Exclusive);

  if (blocking) unblockUpdates();

  checkpoint(false);

  for (Porpoise *porpoise = porpoises; porpoise; porpoise = porpoise->next)
    porpoise->shutdown();
}

void StreamLog::close() {
  if (Log::isActive(LogInfo))
    Log::log(LogInfo, "Serial log shutdown after changing logs %d times\n",
             logRotations);
}

void StreamLog::putData(uint32 length, const UCHAR *data) {
  if ((writePtr + length) < writeWarningTrack) {
    memcpy(writePtr, data, length);
    writePtr += length;
    writeBlock->length = (int)(writePtr - (UCHAR *)writeBlock);
    writeWindow->currentLength = (int)(writePtr - writeWindow->buffer);

    return;
  }

  // Data didn't fit in block -- find out how much didn't fit, then flush the
  // rest Note: the record code is going to be overwritten by an srlEnd byte.

  int tailLength = (int)(writePtr - recordStart);
  UCHAR recordCode = *recordStart;
  writeBlock->length = (int)(recordStart - (UCHAR *)writeBlock);
  writePtr = recordStart;
  overflowFlush();

  while (writePtr + length + tailLength >= writeWarningTrack) overflowFlush();

  putVersion();

  // Restore the initial part of the record

  if (tailLength) {
    *writePtr++ = recordCode;

    if (tailLength > 1) {
      memcpy(writePtr, recordStart + 1, tailLength - 1);
      writePtr += tailLength - 1;
    }
  }

  // And finally finish the copy of data

  memcpy(writePtr, data, length);
  writePtr += length;
  writeBlock->length = (int)(writePtr - (UCHAR *)writeBlock);
  writeWindow->currentLength = (int)(writePtr - writeWindow->buffer);
  recordStart = writeBlock->data;
  // ASSERT(writeWindow->validate(writeBlock));
}

void StreamLog::startRecord() {
  ASSERT(!recovering);

  if (writeError)
    throw SQLError(
        IO_ERROR_STREAMLOG,
        "Previous I/O error on stream log prevents further processing");

  startRecordVirtualOffset = writeWindow->getNextVirtualOffset();

  if (writePtr == writeBlock->data) putVersion();

  recordStart = writePtr;
  recordIncomplete = true;
}

void StreamLog::endRecord(void) { recordIncomplete = false; }

void StreamLog::wakeup() {
  for (Porpoise *porpoise = porpoises; porpoise; porpoise = porpoise->next)
    if ((porpoise->workerThread) && porpoise->workerThread->sleeping &&
        !porpoise->active) {
      porpoise->wakeup();
      break;
    }
}

void StreamLog::release(StreamLogWindow *window) {
  ASSERT(window->inUse == 0);

  if (window->buffer) {
    releaseBuffer(window->buffer);
    window->buffer = NULL;
  }

  if (window->next)
    window->next->prior = window->prior;
  else
    lastWindow = window->prior;

  if (window->prior)
    window->prior->next = window->next;
  else
    firstWindow = window->next;

  window->next = freeWindows;
  freeWindows = window;
}

StreamLogWindow *StreamLog::allocWindow(StreamLogFile *file, int64 origin) {
  StreamLogWindow *window = freeWindows;

  if (window) {
    freeWindows = window->next;
    window->setPosition(file, origin);
  } else
    window = new StreamLogWindow(this, file, origin);

  window->next = NULL;

  if ((window->prior = lastWindow))
    lastWindow->next = window;
  else
    firstWindow = window;

  // Set the virtual byte offset of this window

  if (window->prior)
    window->virtualOffset = window->prior->virtualOffset + SRL_WINDOW_SIZE;
  else
    window->virtualOffset = 0;

  lastWindow = window;
  window->activateWindow(false);

  return window;
}

void StreamLog::initializeLog(int64 blockNumber) {
  Sync sync(&syncWrite, "StreamLog::initializeLog");
  sync.lock(Exclusive);
  nextBlockNumber = blockNumber;
  writeWindow = allocWindow(file1, 0);
  writeWindow->firstBlockNumber = nextBlockNumber;
  writeWindow->lastBlockNumber = nextBlockNumber;
  initializeWriteBlock((StreamLogBlock *)writeWindow->buffer);
}

StreamLogWindow *StreamLog::findWindowGivenOffset(uint64 offset) {
  Sync sync(&syncWrite, "StreamLog::findWindowGivenOffset");
  sync.lock(Exclusive);

  for (StreamLogWindow *window = firstWindow; window; window = window->next)
    if (offset >= window->virtualOffset &&
        offset < (window->virtualOffset + window->currentLength)) {
      window->activateWindow(true);

      return window;
    }

  Log::debug("StreamLog::findWindowGivenOffset -- can't find window\n");
  printWindows();
  ASSERT(false);

  return NULL;
}

StreamLogWindow *StreamLog::findWindowGivenBlock(uint64 blockNumber) {
  for (StreamLogWindow *window = firstWindow; window; window = window->next)
    if (blockNumber >= window->firstBlockNumber &&
        blockNumber <= window->lastBlockNumber) {
      // window->activateWindow(true);
      return window;
    }

  return NULL;
}

StreamLogTransaction *StreamLog::findTransaction(TransId transactionId) {
  Sync sync(&pending.syncObject, "StreamLog::findTransaction");
  sync.lock(Shared);

  for (StreamLogTransaction *transaction =
           transactions[transactionId % SLT_HASH_SIZE];
       transaction; transaction = transaction->collision)
    if (transaction->transactionId == transactionId) return transaction;

  return NULL;
}

StreamLogTransaction *StreamLog::getTransaction(TransId transactionId) {
  ASSERT(transactionId > 0);
  StreamLogTransaction *transaction = findTransaction(transactionId);

  if (transaction) return transaction;

  Sync sync(&pending.syncObject, "StreamLog::getTransaction");
  sync.lock(Exclusive);

  /***
  if (transactionId == 0 || transactionId == 41)
          printf ("StreamLog::getTransaction id %ld\n", transactionId);
  ***/

  int slot = (int)(transactionId % SLT_HASH_SIZE);

  for (transaction = transactions[transactionId % SLT_HASH_SIZE]; transaction;
       transaction = transaction->collision)
    if (transaction->transactionId == transactionId) return transaction;

  transaction = new StreamLogTransaction(this, transactionId);
  transaction->collision = transactions[slot];
  transactions[slot] = transaction;
  running.append(transaction);

  if (writeWindow) {
    transaction->setStart(recordStart, writeBlock, writeWindow);
    transaction->later = NULL;

    if ((transaction->earlier = latest))
      transaction->earlier->later = transaction;
    else
      earliest = transaction;

    latest = transaction;
    transaction->ordered = true;
  }

  return transaction;
}

uint64 StreamLog::getReadBlock() {
  Sync sync(&pending.syncObject, "StreamLog::getReadBlock");
  sync.lock(Shared);
  uint64 blockNumber = lastBlockWritten;

  if (earliest) {
    uint64 number = earliest->getBlockNumber();

    if (number > 0) blockNumber = MIN(blockNumber, number);
  }

  return blockNumber;
}

void StreamLog::transactionDelete(StreamLogTransaction *transaction) {
  Sync sync(&pending.syncObject, "StreamLog::transactionDelete");
  sync.lock(Exclusive);
  int slot = (int)(transaction->transactionId % SLT_HASH_SIZE);

  for (StreamLogTransaction **ptr = transactions + slot; *ptr;
       ptr = &(*ptr)->collision)
    if (*ptr == transaction) {
      *ptr = transaction->collision;
      break;
    }

  if (transaction->ordered) {
    if (transaction->earlier)
      transaction->earlier->later = transaction->later;
    else
      earliest = transaction->later;

    if (transaction->later)
      transaction->later->earlier = transaction->earlier;
    else
      latest = transaction->earlier;

    transaction->ordered = false;
  }
}

void StreamLog::execute(Scheduler *scheduler) {
  try {
    checkpoint(false);
  } catch (SQLException &exception) {
    Log::log("StreamLog checkpoint failed: %s\n", exception.getText());
  }

  getNextEvent();
  scheduler->addEvent(this);
}

void StreamLog::checkpoint(bool force) {
  if (force || writePtr != writeBlock->data ||
      getReadBlock() > lastReadBlock + 1 || inactions.first) {
    Sync sync(&syncWrite, "StreamLog::checkpoint");
    sync.lock(Shared);
    int64 blockNumber = lastBlockWritten;
    sync.unlock();
    database->flush(blockNumber);
  }
}

void StreamLog::pageCacheFlushed(int64 blockNumber) {
  if (blockNumber) {
    database->sync();
    logControl->checkpoint.append(blockNumber);
  }

  if (blockNumber) lastFlushBlock = blockNumber;  // preFlushBlock;

  Sync sync(&pending.syncObject,
            "StreamLog::pageCacheFlushed");  // pending.syncObject use for both
  sync.lock(Exclusive);

  for (StreamLogTransaction *transaction, **ptr = &inactions.first;
       (transaction = *ptr);)
    if (transaction->flushing && transaction->maxBlockNumber < lastFlushBlock) {
      inactions.remove(transaction);
      delete transaction;
    } else
      ptr = &transaction->next;
}

void StreamLog::initializeWriteBlock(StreamLogBlock *block) {
  writeBlock = block;
  writePtr = writeBlock->data;
  writeBlock->blockNumber = nextBlockNumber++;
  writeBlock->creationTime = (uint32)creationTime;
  writeBlock->version = srlCurrentVersion;
  writeBlock->length = (int)(writePtr - (UCHAR *)writeBlock);
  writeWindow->setLastBlock(writeBlock);
  writeWarningTrack = writeWindow->warningTrack;
  // ASSERT(writeWindow->validate(writeBlock));
}

StreamLogBlock *StreamLog::findLastBlock(StreamLogWindow *window) {
  StreamLogBlock *lastBlock = window->firstBlock();

  for (;;) {
    lastBlock = window->findLastBlock(lastBlock);
    StreamLogWindow *newWindow =
        allocWindow(window->file, window->origin + window->currentLength);

    if (window->next != newWindow) {
      lastWindow = newWindow->prior;
      lastWindow->next = NULL;
      newWindow->next = window->next;
      newWindow->prior = window;
      newWindow->next->prior = newWindow;
      window->next = newWindow;
    }

    StreamLogBlock *newBlock = newWindow->readFirstBlock();

    if (!newBlock || newBlock->blockNumber != lastBlock->blockNumber + 1) {
      newWindow->deactivateWindow();
      release(newWindow);
      break;
    }

    window->deactivateWindow();
    window = newWindow;
    lastBlock = newBlock;
  }

  return lastBlock;
}

bool StreamLog::isIndexActive(int indexId, int tableSpaceId) {
  if (!recoveryIndexes) return true;

  return recoveryIndexes->isObjectActive(indexId, tableSpaceId);
}

bool StreamLog::isSectionActive(int sectionId, int tableSpaceId) {
  if (!recoverySections) return true;

  return recoverySections->isObjectActive(sectionId, tableSpaceId);
}

bool StreamLog::isOverflowPageValid(int pageNumber, int tableSpaceId) {
  // If page was not touched by recovery, assume it is valid
  if (!recoveryPages->findRecoveryObject(pageNumber, tableSpaceId)) return true;

  // Otherwise, if page was not created during recovery itself,
  // or was deleted in course of recovery, it is invalid
  if (!recoveryOverflowPages->findRecoveryObject(pageNumber, tableSpaceId))
    return false;
  return recoveryOverflowPages->isObjectActive(pageNumber, tableSpaceId);
}

void StreamLog::setOverflowPageValid(int pageNumber, int tableSpaceId) {
  recoveryOverflowPages->setActive(pageNumber, tableSpaceId);
}

void StreamLog::setOverflowPageInvalid(int pageNumber, int tableSpaceId) {
  recoveryOverflowPages->setInactive(pageNumber, tableSpaceId);
}

uint32 StreamLog::appendLog(IO *shadow, int lastPage) {
  Sync sync(&syncWrite, "StreamLog::appendLog");
  sync.lock(Exclusive);
  shadow->seek(lastPage);
  uint32 totalLength = 0;
  uint64 endBlock = flush(false, 0, NULL);

  for (uint64 blockNumber = getReadBlock(); blockNumber < endBlock;
       ++blockNumber) {
    StreamLogWindow *window = findWindowGivenBlock(blockNumber);

    if (window) {
      window->activateWindow(true);
      StreamLogBlock *block = window->findBlock(blockNumber);
      uint32 length = ROUNDUP(block->length, window->sectorSize);
      shadow->write(length, block->data);
      totalLength += length;
      window->deactivateWindow();
    }
  }

  return totalLength;
}

void StreamLog::dropDatabase() {
  if (file1) file1->dropDatabase();

  if (file2) file2->dropDatabase();
}

void StreamLog::shutdownNow() {}

UCHAR *StreamLog::allocBuffer() {
  if (!buffers.isEmpty()) return (UCHAR *)buffers.pop();

  for (StreamLogWindow *window = firstWindow; window; window = window->next)
    if (window->buffer && window->inUse == 0) {
      ASSERT(window != writeWindow);
      UCHAR *buffer = window->buffer;
      window->setBuffer(NULL);

      return buffer;
    }

  ASSERT(false);

  return NULL;
}

void StreamLog::releaseBuffer(UCHAR *buffer) { buffers.push(buffer); }

void StreamLog::copyClone(JString fileRoot, int logOffset, int logLength) {
  file1->open(fileRoot + ".nl1", true);
  defaultDbb->seek(logOffset);
  uint32 bufferLength = 32768;
  UCHAR *buffer = new UCHAR[bufferLength];
  uint32 position = 0;

  for (uint32 length = logLength; length > 0;) {
    uint32 len = MIN(length, bufferLength);
    defaultDbb->read(len, buffer);
    file1->write(position, len, (StreamLogBlock *)buffer);
    length -= len;
    position += len;
  }

  delete[] buffer;
  file1->close();
  file2->open(fileRoot + ".nl2", true);
  file2->close();
}

bool StreamLog::bumpPageIncarnation(int32 pageNumber, int tableSpaceId,
                                    int state) {
  if (pageNumber == tracePage)
    printf("bumpPageIncarnation; page %d\n", tracePage);

  bool ret =
      recoveryPages->bumpIncarnation(pageNumber, tableSpaceId, state, pass1);

  if (ret && recoveryPhase == 2) {
    Dbb *dbb = getDbb(tableSpaceId);
    dbb->reallocPage(pageNumber);
  }

  return ret;
}

bool StreamLog::bumpSectionIncarnation(int sectionId, int tableSpaceId,
                                       int state) {
  return recoverySections->bumpIncarnation(sectionId, tableSpaceId, state,
                                           pass1);
}

bool StreamLog::bumpIndexIncarnation(int indexId, int tableSpaceId, int state) {
  return recoveryIndexes->bumpIncarnation(indexId, tableSpaceId, state, pass1);
}

void StreamLog::preFlush(void) {
  Sync sync(&pending.syncObject, "StreamLog::preFlush");
  sync.lock(Shared);

  for (StreamLogTransaction *action = inactions.first; action;
       action = action->next)
    action->flushing = true;
}

int StreamLog::recoverLimboTransactions(void) {
  int count = 0;

  for (StreamLogTransaction *transaction = running.first; transaction;
       transaction = transaction->next)
    if (transaction->state == sltPrepared) ++count;

  if (count)
    Log::log("Warning: Recovery found %d prepared transactions in limbo\n",
             count);

  return count;
}

void StreamLog::putVersion() {
  *writePtr++ = srlVersion | LOW_BYTE_FLAG;
  *writePtr++ = srlCurrentVersion | LOW_BYTE_FLAG;
}

void StreamLog::wakeupFlushQueue(Thread *ourThread) {
  // ASSERT(syncWrite.getExclusiveThread() ==
  // Thread::getThread("StreamLog::wakeupFlushQueue"));
  writer = NULL;
  Thread *thread = srlQueue;

  // Start by making sure we're out of the que

  for (Thread *prior = NULL; thread; prior = thread, thread = thread->srlQueue)
    if (thread == ourThread) {
      if (prior)
        prior->srlQueue = thread->srlQueue;
      else
        srlQueue = thread->srlQueue;

      if (endSrlQueue == thread) endSrlQueue = prior;

      break;
    }

  while ((thread = srlQueue)) {
    srlQueue = thread->srlQueue;
    thread->eventNumber = ++eventNumber;

    if (thread->commitBlockNumber <= highWaterBlock) {
      thread->wakeupType = Shared;
      thread->wake();
    } else {
      writer = thread;
      thread->wakeupType = Exclusive;
      thread->wake();
      break;
    }
  }

  if (!srlQueue) endSrlQueue = NULL;
}

void StreamLog::setSectionActive(int id, int tableSpaceId) {
  if (!recoverySections) return;

  getDbb(tableSpaceId);

  if (recovering)
    recoverySections->bumpIncarnation(id, tableSpaceId, objInUse, false);
  else
    recoverySections->deleteObject(id, tableSpaceId);
}

void StreamLog::setSectionInactive(int id, int tableSpaceId) {
  if (!recoverySections) recoverySections = new RecoveryObjects(this);

  recoverySections->bumpIncarnation(id, tableSpaceId, objDeleted, true);
}

void StreamLog::setIndexActive(int id, int tableSpaceId) {
  if (!recoveryIndexes) return;

  getDbb(tableSpaceId);

  if (recovering)
    recoveryIndexes->setActive(id, tableSpaceId);
  else
    recoveryIndexes->deleteObject(id, tableSpaceId);
}

void StreamLog::setIndexInactive(int id, int tableSpaceId) {
  if (!recoveryIndexes) recoveryIndexes = new RecoveryObjects(this);

  recoveryIndexes->setInactive(id, tableSpaceId);
}

void StreamLog::setTableSpaceDropped(int tableSpaceId) {
  ASSERT(recovering);
  droppedTablespaces.set(tableSpaceId);
}

bool StreamLog::isTableSpaceDropped(int tableSpaceId) {
  ASSERT(recovering);
  return droppedTablespaces.isSet(tableSpaceId);
}

bool StreamLog::sectionInUse(int sectionId, int tableSpaceId) {
  TableSpaceInfo *info = getTableSpaceInfo(tableSpaceId);

  return info->sectionUseVector.get(sectionId) > 0;
}

bool StreamLog::indexInUse(int indexId, int tableSpaceId) {
  TableSpaceInfo *info = getTableSpaceInfo(tableSpaceId);
  return info->indexUseVector.get(indexId) > 0;
}

void StreamLog::redoFreePage(int32 pageNumber, int tableSpaceId) {
  if (pageNumber == tracePage)
    Log::debug("Redoing free of page %d\n", pageNumber);

  Dbb *dbb = getDbb(tableSpaceId);
  dbb->redoFreePage(pageNumber);
}

void StreamLog::setPhysicalBlock(TransId transId) {
  if (transId) {
    StreamLogTransaction *transaction = findTransaction(transId);

    if (transaction) transaction->setPhysicalBlock();
  }
}

void StreamLog::reportStatistics(void) {
  if (!Log::isActive(LogInfo) || !writeBlock) return;

  Sync sync(&pending.syncObject, "StreamLog::reportStatistics");
  sync.lock(Shared);
  /***
  int count = 0;
  uint64 minBlockNumber = writeBlock->blockNumber;

  for (StreamLogTransaction *action = inactions.first; action; action =
  action->next)
          {
          ++count;

          if (action->minBlockNumber < minBlockNumber)
                  minBlockNumber = action->minBlockNumber;
          }
  ***/

  uint64 minBlockNumber =
      (earliest) ? earliest->minBlockNumber : writeBlock->blockNumber;
  int count = inactions.count;
  uint64 delta = writeBlock->blockNumber - minBlockNumber;
  int reads = windowReads - priorWindowReads;
  priorWindowReads = windowReads;
  int writes = windowWrites - priorWindowWrites;
  priorWindowWrites = windowWrites;
  int windows = maxWindows;
  int commits = commitsComplete - priorCommitsComplete;
  int stalls = backlogStalls - priorBacklogStalls;
  priorCommitsComplete = commitsComplete;
  maxWindows = 0;
  sync.unlock();
  windows = MAX(windows, 1);

  if (count != priorCount || (uint64)delta != priorDelta ||
      priorWrites != writes) {
    Log::log(LogInfo,
             "%d: StreamLog: %d reads, %d writes, %d transactions, %d "
             "completed, %d stalls, " I64FORMAT " blocks, %d windows\n",
             database->deltaTime, reads, writes, count, commits, stalls, delta,
             windows);
    priorCount = count;
    priorDelta = delta;
    priorWrites = writes;
    priorBacklogStalls = backlogStalls;
  }
}

void StreamLog::getStreamLogInfo(InfoTable *tableInfo) {
  Sync sync(&pending.syncObject, "StreamLog::getStreamLogInfo(1)");
  sync.lock(Shared);
  int numberTransactions = 0;
  uint64 minBlockNumber = writeBlock->blockNumber;

  for (StreamLogTransaction *action = inactions.first; action;
       action = action->next) {
    ++numberTransactions;

    if (action->minBlockNumber < minBlockNumber)
      minBlockNumber = action->minBlockNumber;
  }

  int64 delta = writeBlock->blockNumber - minBlockNumber;
  sync.unlock();

  Sync syncWindows(&syncWrite, "StreamLog::getStreamLogInfo(1)");
  syncWindows.lock(Shared);
  int windows = 0;
  int buffers = 0;

  for (StreamLogWindow *window = firstWindow; window; window = window->next) {
    ++windows;

    if (window->buffer) ++buffers;
  }

  syncWindows.unlock();
  int n = 0;
  //	tableInfo->putString(n++, database->name);
  tableInfo->putInt(n++, numberTransactions);
  tableInfo->putInt64(n++, delta);
  tableInfo->putInt(n++, windows);
  tableInfo->putInt(n++, buffers);
  tableInfo->putRecord();
}

void StreamLog::commitByXid(int xidLength, const UCHAR *xid) {
  Sync sync(&pending.syncObject, "StreamLog::commitByXid");
  sync.lock(Shared);

  for (StreamLogTransaction *action = inactions.first; action;
       action = action->next) {
    StreamLogTransaction *transaction = (StreamLogTransaction *)action;

    if (transaction->isXidEqual(xidLength, xid)) transaction->commit();
  }
}

void StreamLog::rollbackByXid(int xidLength, const UCHAR *xid) {
  Sync sync(&pending.syncObject, "StreamLog::rollbackByXid");
  sync.lock(Shared);

  for (StreamLogTransaction *action = inactions.first; action;
       action = action->next) {
    StreamLogTransaction *transaction = (StreamLogTransaction *)action;

    if (transaction->isXidEqual(xidLength, xid)) transaction->rollback();
  }
}

void StreamLog::preCommit(Transaction *transaction) {
  StreamLogTransaction *streamLogTransaction =
      findTransaction(transaction->transactionId);

  if (!streamLogTransaction) {
    Sync writeSync(&syncWrite, "StreamLog::preCommit(1)");
    writeSync.lock(Exclusive);
    startRecord();
    streamLogTransaction = getTransaction(transaction->transactionId);
  }

  Sync sync(&pending.syncObject, "StreamLog::preCommit(2)");
  sync.lock(Exclusive);
  running.remove(streamLogTransaction);
  pending.append(streamLogTransaction);
}

void StreamLog::printWindows(void) {
  Log::debug("Stream Log Windows:\n");

  for (StreamLogWindow *window = firstWindow; window; window = window->next)
    window->print();
}

Dbb *StreamLog::getDbb(int tableSpaceId) {
  ASSERT(recoveryPhase != 1);
  if (tableSpaceId == 0) return defaultDbb;

  return tableSpaceManager->getTableSpace(tableSpaceId)->dbb;
}

Dbb *StreamLog::findDbb(int tableSpaceId) {
  ASSERT(recoveryPhase != 1);
  if (tableSpaceId == 0) return defaultDbb;

  TableSpace *tableSpace = tableSpaceManager->findTableSpace(tableSpaceId);

  if (!tableSpace) return NULL;

  return tableSpace->dbb;
}

TableSpaceInfo *StreamLog::getTableSpaceInfo(int tableSpaceId) {
  TableSpaceInfo *info;

  // Map reserved tablespace ids from the end of the hash table

  int slot = (tableSpaceId >= 0) ? (tableSpaceId % SLT_HASH_SIZE)
                                 : (SLT_HASH_SIZE + tableSpaceId);

  for (info = tableSpaces[slot]; info; info = info->collision)
    if (info->tableSpaceId == tableSpaceId) return info;

  info = new TableSpaceInfo;
  info->tableSpaceId = tableSpaceId;
  info->collision = tableSpaces[slot];
  tableSpaces[slot] = info;
  info->next = tableSpaceInfo;
  tableSpaceInfo = info;

  return info;
}

void StreamLog::updateSectionUseVector(uint sectionId, int tableSpaceId,
                                       int delta) {
  TableSpaceInfo *info = getTableSpaceInfo(tableSpaceId);

  if (sectionId >= info->sectionUseVector.length)
    info->sectionUseVector.extend(sectionId + 10);

  // info->sectionUseVector.vector[sectionId] += delta;
  INTERLOCKED_ADD(
      (volatile INTERLOCK_TYPE *)(info->sectionUseVector.vector + sectionId),
      delta);
}

void StreamLog::updateIndexUseVector(uint indexId, int tableSpaceId,
                                     int delta) {
  TableSpaceInfo *info = getTableSpaceInfo(tableSpaceId);

  if (indexId >= info->indexUseVector.length)
    info->indexUseVector.extend(indexId + 10);

  info->indexUseVector.vector[indexId] += delta;
}

void StreamLog::preUpdate(void) {
  if (!blocking) return;

  Sync sync(&syncUpdateStall, "StreamLog::preUpdate");
  sync.lock(Shared);
}

uint64 StreamLog::getWriteBlockNumber(void) { return writeBlock->blockNumber; }

void StreamLog::unblockUpdates(void) {
  Sync sync(&pending.syncObject, "StreamLog::unblockUpdates");
  sync.lock(Exclusive);

  if (blocking) {
    blocking = false;
    syncUpdateStall.unlock();
  }
}

void StreamLog::blockUpdates(void) {
  Sync sync(&pending.syncObject, "StreamLog::blockUpdates");
  sync.lock(Exclusive);

  if (!blocking) {
    blocking = true;
    syncUpdateStall.lock(NULL, Exclusive);
    ++backlogStalls;
  }
}

int StreamLog::getBlockSize(void) { return file1->sectorSize; }

int StreamLog::recoverGetNextLimbo(int xidSize, unsigned char *xid) {
  StreamLogTransaction *transaction = nextLimboTransaction;

  if (!transaction) return 0;

  if (transaction->xidLength == xidSize) memcpy(xid, transaction->xid, xidSize);

  for (transaction = transaction->next; transaction;
       transaction = transaction->next)
    if (transaction->state == sltPrepared) break;

  nextLimboTransaction = transaction;
  return 1;
}

StreamLogWindow *StreamLog::setWindowInterest(void) {
  StreamLogWindow *window = writeWindow;
  window->setInterest();

  return window;
}

void StreamLog::setWriteError(int sqlCode, const char *errorText) {
  // Logs an error message to the error log and sets the status of the
  // stream log to "writeError"

  char msgBuf[1024];

  if (!errorText) {
    errorText = "Not provided";
  }

  sprintf(
      msgBuf,
      "StreamLog: Error during writing to stream log. Cause: %s (sqlcode=%d)\n",
      errorText, sqlCode);
  Log::log(msgBuf);

  writeError = true;
}

}  // namespace Changjiang
