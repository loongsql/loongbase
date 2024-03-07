/* Copyright (C) 2006-2008 MySQL AB, 2008-2009 Sun Microsystems, Inc.

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

// RecordVersion.cpp: implementation of the RecordVersion class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "Database.h"
#include "Configuration.h"
#include "RecordVersion.h"
#include "Transaction.h"
#include "TransactionState.h"
#include "TransactionManager.h"
#include "Table.h"
#include "Connection.h"
#include "StreamLogControl.h"
#include "Stream.h"
#include "Dbb.h"
#include "RecordScavenge.h"
#include "Format.h"
#include "Serialize.h"

namespace Changjiang {

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

RecordVersion::RecordVersion(Table *tbl, Format *format,
                             Transaction *transaction, Record *oldVersion)
    : Record(tbl, format) {
  virtualOffset = 0;

  // transaction   = trans;
  // transactionState    = trans->transactionState;
  // transactionState->addRef();

  transactionState = NULL;
  setTransactionState(transaction->transactionState);

  // transactionId = transaction->transactionId;
  savePointId = transaction->curSavePointId;
  superceded = false;

  // Add a use count on the transaction state to ensure it lives
  // as long as the record version object

  if ((priorVersion = oldVersion)) {
    priorVersion->addRef(REC_HISTORY);
    recordNumber = oldVersion->recordNumber;

    if (priorVersion->state == recChilled) priorVersion->thaw();

    if (transactionState == priorVersion->getTransactionState())
      oldVersion->setSuperceded(true);
  } else
    recordNumber = -1;
}

RecordVersion::RecordVersion(Database *database, Serialize *stream)
    : Record(database, stream) {
  // Reconstitute a record version and recursively restore all
  // prior versions from 'stream'

  virtualOffset = stream->getInt64();
  TransId transactionId = stream->getInt();
  int priorType = stream->getInt();
  superceded = false;
  transactionState = NULL;

  if (priorType == 0)
    priorVersion = new Record(database, stream);
  else if (priorType == 1) {
    priorVersion = new RecordVersion(database, stream);

    if (priorVersion->getTransactionId() == transactionId) superceded = true;
  } else
    priorVersion = NULL;

  Transaction *transaction =
      database->transactionManager->findTransaction(transactionId);

  if (transaction) {
    setTransactionState(transaction->transactionState);
    transaction->release();
    /***
    if (!transaction->writePending)
            transaction = NULL;
    ***/
  } else {
    // Creates a transaction state object for storing the transaction id

    transactionState = new TransactionState();
    transactionState->transactionId = transactionId;
    transactionState->state = Committed;
  }
}

RecordVersion::~RecordVersion() {
  state = recDeleting;
  Record *prior = priorVersion;
  if (!COMPARE_EXCHANGE_POINTER(&priorVersion, prior, NULL))
    FATAL("~RecordVersion; Unexpected contents in priorVersion\n");

  // Avoid recursion here. May crash from too many levels
  // if the same record is updated too often and quickly.

  while (prior) prior = prior->releaseNonRecursive();

  // Release the use count on the transaction state object

  if (transactionState) transactionState->release();
}

// Release the priorRecord reference without doing it recursively.
// The caller needs to do this for what is returned it is if not null;

Record *RecordVersion::releaseNonRecursive() {
  Record *prior = NULL;

  if (useCount == 1) {
    prior = priorVersion;
    if (!COMPARE_EXCHANGE_POINTER(&priorVersion, prior, NULL))
      FATAL(
          "RecordVersion::releaseNonRecursive; Unexpected contents in "
          "priorVersion\n");
  }

  release(REC_HISTORY);

  return prior;
}

Record *RecordVersion::fetchVersion(Transaction *trans) {
  // Unless the record is at least as old as the transaction, it's not for us

  TransactionState *recTransState = transactionState;

  if (state != recLock) {
    if (IS_READ_COMMITTED(trans->isolationLevel)) {
      int state = (recTransState) ? recTransState->state : 0;

      if (state == Committed || recTransState == trans->transactionState)
        return (getRecordData()) ? this : NULL;
    } else if (recTransState->transactionId <= trans->transactionId) {
      if (trans->visible(recTransState, FOR_READING))
        return (getRecordData()) ? this : NULL;
    }
  }

  if (!priorVersion) return NULL;

  return priorVersion->fetchVersion(trans);
}

void RecordVersion::rollback(Transaction *transaction) {
  if (!superceded) format->table->rollbackRecord(this, transaction);
}

bool RecordVersion::isVersion() { return true; }

/*
 *	Parent transaction is now fully mature (and about to go
 *	away).  Cleanup any multiversion stuff.
 */

void RecordVersion::commit() {
  // transaction = NULL;
}

// Return true if this record has been committed before a certain transactionId

bool RecordVersion::committedBefore(TransId transId) {
  /***
  // The transaction pointer in this record can disapear at any time due to
  // another call to Transaction::commitRecords().  So read it locally

  Transaction *transactionPtr = transaction;

  if (transactionPtr)
          return transactionPtr->committedBefore(transId);

  // If the transaction Pointer is null, then this record is committed.
  // All we have is the starting point for these transactions.

  return (transactionId < transId);
  ***/

  return transactionState->committedBefore(transId);
}

// This is called with an exclusive lock on the recordLeaf

void RecordVersion::retire(void) {
  SET_THIS_RECORD_ACTIVE(false);
  RECORD_HISTORY(this);

  if (state == recDeleted)
    expungeRecord();  // Allow this record number to be reused

  release();
}

// Scavenge record versions replaced within a savepoint.
// this record is staying and any prior records at
// the same savepoint are leaving

void RecordVersion::scavengeSavepoint(Transaction *targetTransaction,
                                      int oldestActiveSavePointId) {
  if (!priorVersion) return;

  Record *rec = priorVersion;
  Record *ptr = NULL;

  // Remove prior record versions assigned to the savepoint being released

  for (; (rec && rec->getTransactionId() == targetTransaction->transactionId &&
          rec->getSavePointId() >= oldestActiveSavePointId);
       rec = rec->getPriorVersion()) {
    ptr = rec;
    SET_RECORD_ACTIVE(rec, false);

    targetTransaction->removeRecord((RecordVersion *)rec);
  }

  // If we didn't find anyone, there's nothing to do

  if (!ptr) return;

  // There are intermediate versions to collapse.  Make this
  // priorRecord point past the intermediate version(s) to the
  // next staying version.

  Record *prior = priorVersion;
  prior->addRef(REC_HISTORY);

  // Set this record's priorVersion to point past the leaving record(s)

  setPriorVersion(prior, rec);
  ptr->state = recEndChain;
  format->table->garbageCollect(prior, this, targetTransaction, false);
  prior->queueForDelete();
}

Record *RecordVersion::getPriorVersion() { return priorVersion; }

Record *RecordVersion::getGCPriorVersion(void) {
  return (state == recEndChain) ? NULL : priorVersion;
}

void RecordVersion::setSuperceded(bool flag) { superceded = flag; }

/***
Transaction* RecordVersion::getTransaction()
{
        return transaction;
}
***/

TransactionState *RecordVersion::getTransactionState() const {
  return transactionState;
}

bool RecordVersion::isSuperceded() { return superceded; }

// Set the priorVersion to NULL and return its pointer.
// The caller is responsible for releasing the associated useCount.

Record *RecordVersion::clearPriorVersion(void) {
  Record *prior = priorVersion;

  if (prior && prior->useCount == 1) {
    if (COMPARE_EXCHANGE_POINTER(&priorVersion, prior, NULL)) return prior;
  }

  return NULL;
}

void RecordVersion::setPriorVersion(Record *oldPriorVersion,
                                    Record *newPriorVersion) {
  if (newPriorVersion) newPriorVersion->addRef(REC_HISTORY);

  if (!COMPARE_EXCHANGE_POINTER(&priorVersion, oldPriorVersion,
                                newPriorVersion))
    FATAL(
        "RecordVersion::setPriorVersion; Unexpected contents in "
        "priorVersion\n");

  if (oldPriorVersion) oldPriorVersion->release(REC_HISTORY);
}

TransId RecordVersion::getTransactionId() {
  return transactionState->transactionId;
}

int RecordVersion::getSavePointId() { return savePointId; }

void RecordVersion::setVirtualOffset(uint64 offset) { virtualOffset = offset; }

uint64 RecordVersion::getVirtualOffset() { return (virtualOffset); }

int RecordVersion::thaw() {
  Sync syncThaw(format->table->getSyncThaw(this), "RecordVersion::thaw");
  syncThaw.lock(Exclusive);

  int bytesRestored = 0;

  // Nothing to do if the record is no longer chilled

  if (state != recChilled) return getDataMemUsage();

  // First, try to thaw from the stream log. If transaction->writePending is
  // true, then the record data can be restored from the stream log. If
  // writePending is false, then the record data has been written to the data
  // pages.

  Transaction *trans = findTransaction();

  if (trans) {
    if (trans->writePending) {
      bytesRestored = trans->thaw(this);

      if (bytesRestored == 0) trans->thaw(this);
    }

    trans->release();
  }

  // The record data is no longer available in the stream log, so zap the
  // virtual offset and restore from the data page.

  bool recordFetched = false;

  if (bytesRestored == 0) {
    Stream stream;
    Table *table = format->table;

    if (table->dbb->fetchRecord(table->dataSection, recordNumber, &stream)) {
      bytesRestored = setEncodedRecord(&stream, true);
      recordFetched = true;
    }

    if (bytesRestored > 0) {
      virtualOffset = 0;
      table->debugThawedRecords++;
      table->debugThawedBytes += bytesRestored;

      if (table->debugThawedBytes >=
          table->database->configuration->recordChillThreshold) {
        Log::debug(
            "%d: Record thaw (fetch): table %d, %ld records, %ld bytes\n",
            this->format->table->database->deltaTime, table->tableId,
            table->debugThawedRecords, table->debugThawedBytes);
        table->debugThawedRecords = 0;
        table->debugThawedBytes = 0;
      }
    }
  }

  if (state == recChilled) {
    if (data.record != NULL) state = recData;
  }

  return bytesRestored;
}

/***
char* RecordVersion::getRecordData()
{
        if (state == recChilled)
                thaw();

        return data.record;
}
***/

void RecordVersion::print(void) {
  Log::debug(
      "  %p\tId %d, enc %d, state %d, tid %d, use %d, grp %d, prior %p\n", this,
      recordNumber, encoding, state, transactionState->transactionId, useCount,
      generation, priorVersion);

  if (priorVersion) priorVersion->print();
}

int RecordVersion::getSize(void) { return sizeof(*this); }

void RecordVersion::serialize(Serialize *stream) {
  Record::serialize(stream);
  stream->putInt64(virtualOffset);
  stream->putInt(transactionState->transactionId);

  // Recursively serialize the prior version chain

  if (priorVersion) {
    stream->putInt(priorVersion->isVersion());
    priorVersion->serialize(stream);
  } else
    stream->putInt(2);
}

void RecordVersion::setTransactionState(TransactionState *newTransState) {
  if (transactionState) transactionState->release();

  transactionState = newTransState;
  transactionState->addRef();
}

Transaction *RecordVersion::findTransaction(void) {
  return format->table->database->transactionManager->findTransaction(
      transactionState->transactionId);
}

}  // namespace Changjiang
