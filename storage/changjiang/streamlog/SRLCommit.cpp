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

// SRLCommit.cpp: implementation of the SRLCommit class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLCommit.h"
#include "StreamLog.h"
#include "StreamLogControl.h"
#include "StreamLogTransaction.h"
#include "StreamLogWindow.h"
#include "Sync.h"
#include "Thread.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLCommit::SRLCommit() {}

SRLCommit::~SRLCommit() {}

void SRLCommit::append(Transaction *transaction) {
  transaction->addRef();
  START_RECORD(srlCommit, "SRLCommit::append");
  putInt(transaction->transactionId);
  uint64 commitBlockNumber = log->getWriteBlockNumber();
  StreamLogTransaction *srlTransaction =
      log->getTransaction(transaction->transactionId);
  srlTransaction->setTransaction(transaction);

  // Flush transactions with changes immediately for durability

  if (transaction->hasUpdates)
    log->flush(false, commitBlockNumber, &sync);
  else
    sync.unlock();
}

void SRLCommit::read() { transactionId = getInt(); }

void SRLCommit::print() {
  logPrint("Commit StreamLogTransaction %ld\n", transactionId);
}

void SRLCommit::pass1() {
  StreamLogTransaction *srlTransaction = control->getTransaction(transactionId);
  srlTransaction->setState(sltCommitted);
}

void SRLCommit::commit() {
  StreamLogTransaction *srlTransaction = log->findTransaction(transactionId);
  srlTransaction->setFinished();
}

}  // namespace Changjiang
