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

// StreamLogTransaction.cpp: implementation of the StreamLogTransaction class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include "Engine.h"
#include "StreamLogTransaction.h"
#include "StreamLog.h"
#include "StreamLogControl.h"
#include "StreamLogWindow.h"
#include "Transaction.h"
#include "Database.h"
#include "Bitmap.h"

namespace Changjiang {

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

StreamLogTransaction::StreamLogTransaction(
    StreamLog *streamLog, TransId transId)  //: StreamLogAction(streamLog)
{
  log = streamLog;
  flushing = false;

  transactionId = transId;
  state = sltUnknown;
  window = NULL;
  finished = false;
  ordered = false;
  allowConcurrentPorpoises = true;
  transaction = NULL;
  rolledBackSavepoints = NULL;
  blockNumber = maxBlockNumber = minBlockNumber = physicalBlockNumber = 0;
  xidLength = 0;
  xid = NULL;
}

StreamLogTransaction::~StreamLogTransaction() {
  if (transaction) transaction->release();

  if (window) window->release();

  log->transactionDelete(this);
  delete[] xid;
  delete rolledBackSavepoints;
}

void StreamLogTransaction::commit() {
  ASSERT(!transaction || transaction->transactionId == transactionId);
  StreamLogControl control(log);
  window->activateWindow(true);
  StreamLogBlock *block = (StreamLogBlock *)(window->buffer + blockOffset);
  control.setWindow(window, block, recordOffset);
  window->deactivateWindow();
  finished = false;
  int windows = 0;

  for (StreamLogWindow *w = window; w; w = w->next) ++windows;

  log->maxWindows = MAX(log->maxWindows, windows);
  int skipped = 0;
  int processed = 0;

  for (StreamLogRecord *record; !finished && (record = control.nextRecord());)
    if (record->transactionId == transactionId) {
      ++processed;
      record->commit();
    } else
      ++skipped;

  ++log->commitsComplete;

  if (transaction) {
    transaction->fullyCommitted();
    transaction->release();
    transaction = NULL;
  }
}

void StreamLogTransaction::rollback() {
  StreamLogControl control(log);
  window->activateWindow(true);
  StreamLogBlock *block = (StreamLogBlock *)(window->buffer + blockOffset);
  control.setWindow(window, block, recordOffset);
  window->deactivateWindow();
  finished = false;

  for (StreamLogRecord *record; !finished && (record = control.nextRecord());)
    if (record->transactionId == transactionId) record->rollback();
}

void StreamLogTransaction::setStart(const UCHAR *record, StreamLogBlock *block,
                                    StreamLogWindow *win) {
  if (win != window) {
    win->addRef();

    if (window) window->release();
  }

  window = win;
  blockNumber = block->blockNumber;
  minBlockNumber = blockNumber;
  maxBlockNumber = blockNumber;
  blockOffset = (int)((UCHAR *)block - window->buffer);
  recordOffset = (int)(record - block->data);
}

void StreamLogTransaction::setState(sltState newState) { state = newState; }

void StreamLogTransaction::setFinished() { finished = true; }

bool StreamLogTransaction::isRipe() {
  return (state == sltCommitted || state == sltRolledBack);
}

void StreamLogTransaction::doAction() {
  if (state == sltCommitted)
    commit();
  else
    rollback();
}

void StreamLogTransaction::preRecovery() {
  if (state == sltUnknown) setState(sltRolledBack);
}

bool StreamLogTransaction::completedRecovery() {
  if (state == sltPrepared) return false;

  return true;
}

uint64 StreamLogTransaction::getBlockNumber() { return blockNumber; }

void StreamLogTransaction::setPhysicalBlock() {
  uint64 nextBlockNumber = log->nextBlockNumber;

  if (nextBlockNumber > physicalBlockNumber) {
    physicalBlockNumber = nextBlockNumber;
    maxBlockNumber = MAX(physicalBlockNumber, maxBlockNumber);
  }
}

void StreamLogTransaction::setXID(int length, const UCHAR *xidPtr) {
  xidLength = length;
  xid = new UCHAR[xidLength];
  memcpy(xid, xidPtr, xidLength);
}

bool StreamLogTransaction::isXidEqual(int testLength, const UCHAR *test) {
  if (testLength != xidLength) return false;

  return memcmp(xid, test, xidLength) == 0;
}

void StreamLogTransaction::setTransaction(Transaction *trans) {
  if (!transaction) {
    transaction = trans;
    transaction->addRef();
  }
}

void StreamLogTransaction::savepointRolledBack(int savepointId) {
  if (!rolledBackSavepoints) rolledBackSavepoints = new Bitmap;

  rolledBackSavepoints->set(savepointId);
}

bool StreamLogTransaction::isRolledBackSavepoint(int savepointId) {
  if (!rolledBackSavepoints) return false;

  return rolledBackSavepoints->isSet(savepointId);
}

}  // namespace Changjiang
