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

// SRLRollback.cpp: implementation of the SRLRollback class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLRollback.h"
#include "StreamLogControl.h"
#include "StreamLogTransaction.h"
#include "Sync.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLRollback::SRLRollback() {}

SRLRollback::~SRLRollback() {}

void SRLRollback::append(Transaction *transaction) {
  START_RECORD(srlRollback, "SRLRollback::append");
  putInt(transaction->transactionId);
  uint64 commitBlockNumber = log->nextBlockNumber;

  if (transaction->hasUpdates)
    log->flush(false, commitBlockNumber, &sync);
  else
    sync.unlock();
}

void SRLRollback::read() { transactionId = getInt(); }

void SRLRollback::pass1() {
  StreamLogTransaction *transaction = control->getTransaction(transactionId);
  transaction->setState(sltRolledBack);
}

void SRLRollback::print() {
  logPrint("Rollback transaction %ld\n", transactionId);
}

void SRLRollback::rollback() {
  StreamLogTransaction *transaction = log->findTransaction(transactionId);
  transaction->setFinished();
}

}  // namespace Changjiang
