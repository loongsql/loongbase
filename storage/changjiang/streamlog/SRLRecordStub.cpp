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

// SRLRecordStub.cpp: implementation of the SRLRecordStub class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLRecordStub.h"
#include "StreamLogControl.h"
#include "StreamLogTransaction.h"
#include "Dbb.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLRecordStub::SRLRecordStub() {}

SRLRecordStub::~SRLRecordStub() {}

void SRLRecordStub::append(Dbb *dbb, TransId transId, int32 section,
                           int32 record) {
  START_RECORD(srlRecordStub, "SRLRecordStub::append");
  putInt(dbb->tableSpaceId);
  putInt(transId);
  putInt(section);
  putInt(record);
  getTransaction(transId);
  sync.unlock();
}

void SRLRecordStub::read() {
  if (control->version >= srlVersion8)
    tableSpaceId = getInt();
  else
    tableSpaceId = 0;

  transactionId = getInt();
  sectionId = getInt();
  recordId = getInt();
}

void SRLRecordStub::print() {
  logPrint("Record stub: transaction %d, section %d/%d, record %d\n",
           transactionId, sectionId, tableSpaceId, recordId);
}

void SRLRecordStub::pass1() { control->getTransaction(transactionId); }

void SRLRecordStub::redo() {
  if (!log->isSectionActive(sectionId, tableSpaceId)) return;

  StreamLogTransaction *transaction = control->getTransaction(transactionId);

  if ((transaction->state == sltCommitted) ||
      (transaction->state == sltPrepared))
    log->getDbb(tableSpaceId)->reInsertStub(sectionId, recordId, transactionId);
}

}  // namespace Changjiang
