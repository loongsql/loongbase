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

// SRLDelete.cpp: implementation of the SRLDelete class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLDelete.h"
#include "StreamLogControl.h"
#include "StreamLogTransaction.h"
#include "Dbb.h"
#include "Transaction.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLDelete::SRLDelete() {}

SRLDelete::~SRLDelete() {}

void SRLDelete::append(Dbb *dbb, Transaction *transaction, int32 sectionId,
                       int32 recordId) {
  START_RECORD(srlDelete, "SRLDelete::append");
  StreamLogTransaction *trans = log->getTransaction(transaction->transactionId);
  trans->setTransaction(transaction);
  ASSERT(transaction->writePending);
  putInt(dbb->tableSpaceId);
  putInt(transaction->transactionId);
  putInt(sectionId);
  putInt(recordId);
  sync.unlock();
}

void SRLDelete::read() {
  if (control->version >= srlVersion8)
    tableSpaceId = getInt();
  else
    tableSpaceId = 0;

  transactionId = getInt();
  sectionId = getInt();
  recordId = getInt();
}

void SRLDelete::print() {
  logPrint("Delete: transaction %d, section %d/%d, record %d\n", transactionId,
           sectionId, tableSpaceId, recordId);
}

void SRLDelete::pass1() {}

void SRLDelete::redo() {
  StreamLogTransaction *transaction = control->getTransaction(transactionId);

  if (transaction->state == sltCommitted &&
      log->isSectionActive(sectionId, tableSpaceId))
    log->getDbb(tableSpaceId)
        ->updateRecord(sectionId, recordId, NULL, transactionId, false);
}

void SRLDelete::commit() {
  Sync sync(&log->syncSections, "SRLDelete::commit");
  sync.lock(Shared);
  redo();
}

}  // namespace Changjiang
