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

// StreamLogWindow.cpp: implementation of the StreamLogWindow class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "StreamLogWindow.h"
#include "StreamLogFile.h"
#include "StreamLog.h"
#include "StreamLogControl.h"
#include "SQLError.h"
#include "Log.h"
#include "Sync.h"
#include "Database.h"
#include "Thread.h"
#include "Interlock.h"

namespace Changjiang {

#define NEXT_BLOCK(prior) \
  (StreamLogBlock *)((UCHAR *)prior + ROUNDUP(prior->length, sectorSize))

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

StreamLogWindow::StreamLogWindow(StreamLog *streamLog, StreamLogFile *logFile,
                                 int64 logOrigin) {
  log = streamLog;
  bufferLength = SRL_WINDOW_SIZE;
  buffer = NULL;
  setPosition(logFile, logOrigin);
  next = NULL;
  lastBlock = NULL;
  inUse = 0;
  useCount = 0;
  currentLength = 0;
  virtualOffset = 0;
  interestCount = 0;
}

StreamLogWindow::~StreamLogWindow() {}

void StreamLogWindow::setPosition(StreamLogFile *logFile, int64 logOrigin) {
  file = logFile;
  sectorSize = file->sectorSize;
  origin = logOrigin;
}

StreamLogBlock *StreamLogWindow::readFirstBlock() {
  uint32 length;

  try {
    length = file->read(origin, sectorSize, buffer);
  } catch (SQLException &) {
    return NULL;
  }

  if (length != sectorSize) return NULL;

  StreamLogBlock *block = (StreamLogBlock *)buffer;

  if (block->creationTime != (uint32)log->creationTime || block->length == 0)
    return NULL;

  currentLength = sectorSize;
  length = block->length - sectorSize;
  length = ROUNDUP(length, sectorSize);

  if (length) {
    uint32 len = file->read(origin + sectorSize, block->length - sectorSize,
                            buffer + sectorSize);

    if (len != length)
      throw SQLError(IO_ERROR_STREAMLOG, "truncated log file \"%s\"",
                     (const char *)file->fileName);

    currentLength += length;
  }

  const UCHAR *end = (const UCHAR *)block + block->length;

  if (end[-1] != (srlEnd | LOW_BYTE_FLAG)) return NULL;

  firstBlockNumber = block->blockNumber;

  return block;
}

void StreamLogWindow::write(StreamLogBlock *block) {
  uint32 length = ROUNDUP(block->length, sectorSize);
  int64 offset = origin + ((UCHAR *)block - buffer);
  ASSERT(length <= bufferLength);
  try {
    file->write(offset, length, block);
  } catch (SQLException &exception) {
    if (exception.getSqlcode() != DEVICE_FULL) throw;

    log->database->setIOError(&exception);
    Thread *thread = Thread::getThread("Cache::writePage");

    for (bool error = true; error;) {
      if (thread->shutdownInProgress) return;

      thread->sleep(1000);

      try {
        file->write(offset, length, block);
        error = false;
        log->database->clearIOError();
      } catch (SQLException &exception2) {
        if (exception2.getSqlcode() != DEVICE_FULL) throw;
      }
    }
  }

  ++log->windowWrites;
}

StreamLogBlock *StreamLogWindow::findLastBlock(StreamLogBlock *first) {
  int length = bufferLength - currentLength;

  if (length) {
    length = file->read(origin + currentLength, length, buffer + currentLength);
    length = length / sectorSize * sectorSize;
    currentLength += length;
  }

  uint64 blockNumber = first->blockNumber;

  for (StreamLogBlock *prior = first,
                      *end = (StreamLogBlock *)(buffer + currentLength), *block;
       ; prior = block) {
    block = NEXT_BLOCK(prior);
    UCHAR *endBlock = (UCHAR *)block + block->length;

    if (endBlock < (UCHAR *)end &&
        block->creationTime == (uint32)log->creationTime &&
        block->blockNumber != blockNumber + 1) {
      printf("Stream Log possible gap: " I64FORMAT " - " I64FORMAT "\n",
             blockNumber + 1, block->blockNumber);
      // StreamLogControl control(log);
      // control.printBlock(prior);
    }

    if (endBlock > (UCHAR *)end ||
        block->creationTime != (uint32)log->creationTime ||
        block->blockNumber != ++blockNumber) {
      currentLength = (int)(((const UCHAR *)block) - buffer);
      lastBlockNumber = prior->blockNumber;

      return lastBlock = prior;
    }

    if (endBlock[-1] != (srlEnd | LOW_BYTE_FLAG)) {
      Log::log("damaged stream log block " I64FORMAT "\n", block->blockNumber);
      currentLength = (int)((const UCHAR *)block - buffer);
      lastBlockNumber = prior->blockNumber;

      return lastBlock = prior;
    }

    // StreamLogControl validation(log);
    // validation.validate(this, block);
  }

  return NULL;
}

StreamLogBlock *StreamLogWindow::findBlock(uint64 blockNumber) {
  for (StreamLogBlock *block = firstBlock();
       block < (StreamLogBlock *)(buffer + currentLength);
       block = NEXT_BLOCK(block))
    if (block->blockNumber == blockNumber) return block;

  NOT_YET_IMPLEMENTED;

  return NULL;
}

StreamLogBlock *StreamLogWindow::nextBlock(StreamLogBlock *block) {
  StreamLogBlock *nextBlk = NEXT_BLOCK(block);

  if (nextBlk < (StreamLogBlock *)(buffer + currentLength) &&
      nextBlk->creationTime == (uint32)log->creationTime &&
      nextBlk->blockNumber == block->blockNumber + 1) {
    // ASSERT(validate(nextBlk));
    return nextBlk;
  }

  return NULL;
}

StreamLogBlock *StreamLogWindow::nextAvailableBlock(StreamLogBlock *block) {
  StreamLogBlock *nextBlk = NEXT_BLOCK(block);

  if ((UCHAR *)nextBlk >= bufferEnd) return NULL;

  lastBlock = nextBlk;

  return nextBlk;
}

int64 StreamLogWindow::getNextFileOffset() {
  StreamLogBlock *end = NEXT_BLOCK(lastBlock);

  return origin + (UCHAR *)end - buffer;
}

int64 StreamLogWindow::getNextVirtualOffset() {
  return virtualOffset + currentLength;
}

void StreamLogWindow::setBuffer(UCHAR *newBuffer) {
  if ((buffer = newBuffer)) {
    bufferEnd = buffer + bufferLength;
    warningTrack = bufferEnd - 1;
  } else
    bufferEnd = warningTrack = buffer;
}

void StreamLogWindow::activateWindow(bool read) {
  Sync sync(&log->syncWrite, "StreamLogWindow::activateWindow");
  sync.lock(Exclusive);
  ++inUse;

  if (buffer) {
    ASSERT(firstBlock()->blockNumber == firstBlockNumber);
    return;
  }

  setBuffer(log->allocBuffer());

  if (read) {
    ASSERT(currentLength > 0 && currentLength <= (uint32)SRL_WINDOW_SIZE);
    file->read(origin, currentLength, buffer);
    ++log->windowReads;
    lastBlock = (StreamLogBlock *)(buffer + lastBlockOffset);
    ASSERT(firstBlock()->blockNumber == firstBlockNumber);
  }
}

void StreamLogWindow::deactivateWindow() {
  Sync sync(&log->syncWrite, "StreamLogWindow::deactivateWindow");
  sync.lock(Exclusive);
  ASSERT(inUse > 0);
  --inUse;
}

void StreamLogWindow::addRef(void) { ++useCount; }

void StreamLogWindow::release(void) { --useCount; }

void StreamLogWindow::setLastBlock(StreamLogBlock *block) {
  ASSERT(block->blockNumber >= firstBlock()->blockNumber);
  ASSERT(block->blockNumber <= log->nextBlockNumber);
  lastBlock = block;
  lastBlockOffset = (int)((UCHAR *)block - buffer);
  lastBlockNumber = block->blockNumber;
  currentLength = lastBlockOffset + block->length;
}

uint64 StreamLogWindow::getVirtualOffset() { return (virtualOffset); }

void StreamLogWindow::print(void) {
  Log::debug("  Window #" I64FORMAT "- blocks " I64FORMAT ":" I64FORMAT
             " len %d, in-use %d, useCount %d, buffer %p, virtoff " I64FORMAT
             "\n",
             virtualOffset / SRL_WINDOW_SIZE, firstBlockNumber, lastBlockNumber,
             currentLength, inUse, useCount, buffer, virtualOffset);
}

bool StreamLogWindow::validate(StreamLogBlock *block) {
  ASSERT((UCHAR *)block >= buffer && (UCHAR *)block < buffer + bufferLength);
  ASSERT((UCHAR *)block + block->length < buffer + bufferLength);
  ASSERT(block->blockNumber >= firstBlockNumber);
  ASSERT(block->blockNumber <= lastBlockNumber);

  return true;
}

bool StreamLogWindow::validate(const UCHAR *pointer) {
  ASSERT(pointer >= buffer && pointer < buffer + bufferLength);

  return true;
}

void StreamLogWindow::setInterest(void) {
  INTERLOCKED_INCREMENT(interestCount);
}

void StreamLogWindow::clearInterest(void) {
  INTERLOCKED_DECREMENT(interestCount);
}

}  // namespace Changjiang
