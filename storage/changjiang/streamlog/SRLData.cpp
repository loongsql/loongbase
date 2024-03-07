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

// SRLData.cpp: implementation of the SRLData class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLData.h"
#include "Stream.h"
#include "StreamLogControl.h"
#include "StreamLogTransaction.h"
#include "Dbb.h"
#include "Transaction.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLData::SRLData() {}

SRLData::~SRLData() {}

void SRLData::append(Dbb *dbb, Transaction *transaction, int32 sectionId,
                     int32 recordId, Stream *stream) {
  START_RECORD(srlDataUpdate, "SRLData::append");
  StreamLogTransaction *trans = log->getTransaction(transaction->transactionId);
  trans->setTransaction(transaction);
  ASSERT(transaction->writePending);
  putInt(dbb->tableSpaceId);
  putInt(transaction->transactionId);
  putInt(sectionId);
  putInt(recordId);
  putStream(stream);
}

void SRLData::read() {
  if (control->version >= srlVersion8)
    tableSpaceId = getInt();
  else
    tableSpaceId = 0;

  transactionId = getInt();
  sectionId = getInt();
  recordId = getInt();
  length = getInt();
  ASSERT(length >= 0);
  data = getData(length);
}

void SRLData::print() {
  char temp[40];
  logPrint("Data trans %d, section %d, record %d, length %d %s\n",
           transactionId, sectionId, recordId, length,
           format(length, data, sizeof(temp), temp));
}

void SRLData::pass1() { control->getTransaction(transactionId); }

void SRLData::redo() {
  StreamLogTransaction *transaction = control->getTransaction(transactionId);

  if (transaction->state == sltCommitted &&
      log->isSectionActive(sectionId, tableSpaceId)) {
    Stream stream;
    stream.putSegment(length, (const char *)data, false);
    log->getDbb(tableSpaceId)
        ->updateRecord(sectionId, recordId, &stream, transactionId, false);
  }
}

void SRLData::commit() { redo(); }

void SRLData::recoverLimbo(void) {}

}  // namespace Changjiang
