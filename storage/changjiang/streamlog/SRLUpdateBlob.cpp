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

#include "Engine.h"
#include "SRLUpdateBlob.h"
#include "StreamLogControl.h"
#include "StreamLogTransaction.h"
#include "Dbb.h"
#include "Stream.h"

namespace Changjiang {

SRLUpdateBlob::SRLUpdateBlob(void) {}

SRLUpdateBlob::~SRLUpdateBlob(void) {}

void SRLUpdateBlob::append(Dbb *dbb, int32 sectionId, TransId transId,
                           int recordNumber, Stream *stream) {
  START_RECORD(srlSmallBlob, "SRLUpdateBlob::append");
  putInt(dbb->tableSpaceId);
  putInt(sectionId);
  putInt(transId);
  putInt(recordNumber);
  putStream(stream);
}

void SRLUpdateBlob::read(void) {
  tableSpaceId = getInt();
  sectionId = getInt();
  transactionId = getInt();
  recordNumber = getInt();
  length = getInt();
  data = getData(length);
}

void SRLUpdateBlob::pass1(void) { control->getTransaction(transactionId); }

void SRLUpdateBlob::pass2(void) {}

void SRLUpdateBlob::commit(void) {}

void SRLUpdateBlob::redo(void) {
  StreamLogTransaction *transaction = control->getTransaction(transactionId);

  if (transaction->state == sltCommitted &&
      log->isSectionActive(sectionId, tableSpaceId)) {
    Stream stream;
    stream.putSegment(length, (const char *)data, false);
    Dbb *dbb = log->getDbb(tableSpaceId);
    dbb->reInsertStub(sectionId, recordNumber, transactionId);
    dbb->updateRecord(sectionId, recordNumber, &stream, transactionId, false);
  }
}

void SRLUpdateBlob::print(void) {
  logPrint("UpdateBlob trans %d, section %d, record %d, length %d \n",
           transactionId, sectionId, recordNumber, length);
}

}  // namespace Changjiang
