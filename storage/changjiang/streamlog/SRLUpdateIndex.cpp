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

#include <stdio.h>
#include "Engine.h"
#include "Database.h"
#include "SRLUpdateIndex.h"
#include "DeferredIndex.h"
#include "Index.h"
#include "IndexRootPage.h"
#include "IndexKey.h"
#include "DeferredIndexWalker.h"
#include "Transaction.h"
#include "StreamLogTransaction.h"
#include "StreamLogControl.h"
#include "StreamLogWindow.h"
#include "Dbb.h"
#include "Log.h"

namespace Changjiang {

SRLUpdateIndex::SRLUpdateIndex(void) {}

SRLUpdateIndex::~SRLUpdateIndex(void) {}

void SRLUpdateIndex::append(DeferredIndex *deferredIndex) {
  // To ensure coordination with a concurrent SRLDeleteIndex, grab syncIndexes
  // before getting the index id from the DeferredIndex.

  Sync syncIndexes(&log->syncIndexes, "SRLUpdateIndex::append(Indexes)");
  syncIndexes.lock(Shared);

  Sync syncDI(&deferredIndex->syncObject, "SRLUpdateIndex::append(DI)");
  syncDI.lock(Shared);

  Index *index = deferredIndex->index;

  // A null index or indexId == -1 means that the index has been deleted

  if (!index || index->indexId == -1) return;

  int idxId = index->indexId;
  int idxVersion = index->indexVersion;
  int tableSpaceId = index->dbb->tableSpaceId;

  syncDI.unlock();

  Transaction *transaction = deferredIndex->transaction;
  DeferredIndexWalker walker(deferredIndex, NULL);
  uint64 virtualOffset = 0;
  uint64 virtualOffsetAtEnd = 0;

  for (DINode *node = walker.next(); node;) {
    START_RECORD(srlUpdateIndex, "SRLUpdateIndex::append(2)");

    // Save the absolute offset of the DeferredIndex record within the stream
    // log. This must be done inside the StreamLog::syncWrite lock set by
    // START_RECORD().

    if (virtualOffset == 0) virtualOffset = log->startRecordVirtualOffset;

    log->updateIndexUseVector(idxId, tableSpaceId, 1);
    StreamLogTransaction *srlTrans =
        log->getTransaction(transaction->transactionId);
    srlTrans->setTransaction(transaction);
    ASSERT(transaction->writePending);

    // Set the record header fields

    putInt(tableSpaceId);
    putInt(transaction->transactionId);
    putInt(idxId);
    putInt(idxVersion);

    // Initialize the length field, adjust with correct length later.
    // Use a fixed-length integer to accommodate a larger number.

    UCHAR *lengthPtr = putFixedInt(0);
    UCHAR *start = log->writePtr;
    UCHAR *end = log->writeWarningTrack;

    // Write the variable-length index node data. If the data length
    // will extend past the end of the current window, start a new
    // record.

    for (; node; node = walker.next()) {
      if (log->writePtr + byteCount(node->recordNumber) +
              byteCount(node->keyLength) + node->keyLength >=
          end)
        break;

      putInt(node->recordNumber);
      putInt(node->keyLength);
      log->putData(node->keyLength, node->key);
    }

    int len = (int)(log->writePtr - start);
    // printf("SRLUpdateIndex::append tid %d, index %d, length %d, ptr %x
    // (%x)\n",  transaction->transactionId, idxId, len, lengthPtr, org);
    ASSERT(len >= 0);

    // Update the length field

    if (len > 0) putFixedInt(len, lengthPtr);

    // Save the absolute offset of the end of the DeferredIndex record

    virtualOffsetAtEnd = log->writeWindow->getNextVirtualOffset();

    // End this stream log record and flush to disk. Force the creation of
    // a new stream log window.
    log->endRecord();

    if (node)
      log->flush(true, 0, &sync);
    else
      sync.unlock();
  }

  // Update the virtual offset only if one more nodes were written

  if (virtualOffsetAtEnd > 0) {
    deferredIndex->virtualOffset = virtualOffset;
    deferredIndex->virtualOffsetAtEnd = virtualOffsetAtEnd;
  }
}

void SRLUpdateIndex::read(void) {
  if (control->version >= srlVersion8)
    tableSpaceId = getInt();
  else
    tableSpaceId = 0;

  transactionId = getInt();
  indexId = getInt();

  if (control->version >= srlVersion6)
    indexVersion = getInt();
  else
    indexVersion = INDEX_VERSION_1;

  dataLength = getInt();
  data = getData(dataLength);
}

void SRLUpdateIndex::print(void) {
  logPrint("UpdateIndex: transaction %d, length %d\n", transactionId,
           dataLength);

  for (const UCHAR *p = data, *end = data + dataLength; p < end;) {
    int recordNumber = getInt(&p);
    int length = getInt(&p);
    char temp[40];
    Log::debug("   rec %d to index %d %s\n", recordNumber, indexId,
               format(length, p, sizeof(temp), temp));
    p += length;
  }
}

void SRLUpdateIndex::pass1(void) { control->getTransaction(transactionId); }

void SRLUpdateIndex::redo(void) { execute(); }

void SRLUpdateIndex::commit(void) {
  Sync sync(&log->syncIndexes, "SRLUpdateIndex::commit");
  sync.lock(Shared);
  log->updateIndexUseVector(indexId, tableSpaceId, -1);
  execute();
  log->setPhysicalBlock(transactionId);
}

void SRLUpdateIndex::execute(void) {
  if (!log->isIndexActive(indexId, tableSpaceId)) return;

  // StreamLogTransaction *transaction =
  control->getTransaction(transactionId);
  ptr = data;
  end = ptr + dataLength;
  Dbb *dbb = log->getDbb(tableSpaceId);

  IndexRootPage::indexMerge(dbb, indexId, this, NO_TRANSACTION);
}

int SRLUpdateIndex::nextKey(IndexKey *indexKey) {
  if (ptr >= end) return -1;

  int recordNumber = getInt(&ptr);
  int length = getInt(&ptr);

  indexKey->setKey(length, ptr);
  ptr += length;

  return recordNumber;
}

void SRLUpdateIndex::thaw(DeferredIndex *deferredIndex) {
  Sync sync(&log->syncWrite, "SRLUpdateIndex::thaw");
  sync.lock(Exclusive);

  uint64 virtualOffset = deferredIndex->virtualOffset;
  int recordNumber = 0;  // a valid record number to get into the loop.
  Transaction *trans = deferredIndex->transaction;
  TransId transId = trans->transactionId;
  indexId = deferredIndex->index->indexId;

  Log::log(LogInfo,
           "%d: Index thaw: transaction %ld, index %ld, %ld bytes, address %p, "
           "vofs %llx\n",
           log->database->deltaTime, transId, indexId,
           deferredIndex->sizeEstimate, this, virtualOffset);

  // Find the window where the DeferredIndex is stored using the virtualOffset,
  // then activate the window, reading from disk if necessary.

  StreamLogWindow *window =
      log->findWindowGivenOffset(deferredIndex->virtualOffset);

  if (window == NULL) {
    Log::log(
        "Index thaw FAIL: A window for DeferredIndex::virtualOffset=" I64FORMAT
        " could not be found.\n",
        deferredIndex->virtualOffset);
    log->printWindows();
    return;
  }

  // Location of the DeferredIndex within the window

  uint32 windowOffset = (uint32)(virtualOffset - window->virtualOffset);

  // Location of the block in which the DeferredIndex resides

  uint32 blockOffset = 0;

  // Find the block in which the DeferredIndex resides, starting with the first
  // block in the window. Accumulate each block's offset in blockOffset.

  StreamLogBlock *block = window->firstBlock();

  while (blockOffset + block->length <= windowOffset) {
    StreamLogBlock *prevBlock = block;
    block = window->nextBlock(block);
    uint32 thisBlockOffset = (uint32)((UCHAR *)block - (UCHAR *)prevBlock);
    blockOffset += thisBlockOffset;
  }

  // Find the location of the DeferredIndex in the target block. Adjust for the
  // offset of the data buffer within the block structure.

  uint32 offsetWithinBlock =
      (windowOffset - blockOffset - OFFSET(StreamLogBlock *, data));

  // Get the stream log version and set the input pointer to the specified
  // offset within the target block. Activate the window, if necessary.

  control->setWindow(window, block, offsetWithinBlock);
  ASSERT(control->input == window->buffer + windowOffset);
  ASSERT(control->inputEnd <= window->bufferEnd);

  // Now we are pointing at a stream log record, so read the entire record.
  // Version records are written at the top of each block. If necessary,
  // advance past the version record and read the SRLUpdateIndex record.

  StreamLogRecord *srlRecord = control->nextRecord();

  if (srlRecord && srlRecord->type == srlVersion)
    srlRecord = control->nextRecord();

  if (srlRecord)
    ASSERT(srlRecord->type == srlUpdateIndex);
  else
    Log::log(
        "Index thaw FAIL: SRLUpdateIndex record not found. "
        "DeferredIndex::virtualOffset=" I64FORMAT "\n",
        deferredIndex->virtualOffset);

  // The DeferredIndex may reside in several stream log records. Read each
  // record and rebuild the index from the nodes stored within the record.

  while (srlRecord && virtualOffset < deferredIndex->virtualOffsetAtEnd) {
    // Read the header of the deferredIndex and validate.

    ASSERT(transactionId == transId);
    ASSERT(indexId == deferredIndex->index->indexId);
    ASSERT(indexVersion == deferredIndex->index->indexVersion);

    Log::debug(
        "%d: Index thaw: transaction %ld, index %ld, %ld bytes, address %p, "
        "version %ld\n",
        log->database->deltaTime, transactionId, indexId, dataLength, this,
        indexVersion);

    IndexKey indexKey(deferredIndex->index);

    // Read each IndexKey and add it to the deferredIndex. Set ptr and end for
    // nextKey().

    ptr = data;
    end = ptr + dataLength;

    for (recordNumber = nextKey(&indexKey); recordNumber >= 0;
         recordNumber = nextKey(&indexKey))
      deferredIndex->addNode(&indexKey, recordNumber);

    for (;;) {
      // Quit if there are no more StreamLogRecords for this DeferredIndex.

      StreamLogWindow *inputWindow = control->inputWindow;

      if (!inputWindow) break;

      virtualOffset =
          inputWindow->virtualOffset + (control->input - inputWindow->buffer);

      if (virtualOffset >= deferredIndex->virtualOffsetAtEnd)
        break;  // All done.

      // Find the next StreamLogRecord of this deferredIndex.

      srlRecord = control->nextRecord();

      if (srlRecord == this && transactionId == transId &&
          indexId == deferredIndex->index->indexId)
        break;
    }
  }

  control->fini();
  window->deactivateWindow();
}

}  // namespace Changjiang
