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

// SRLBlobUpdate.cpp: implementation of the SRLBlobUpdate class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLBlobUpdate.h"
#include "StreamLogControl.h"
#include "StreamLogTransaction.h"
#include "Dbb.h"
#include "Section.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLBlobUpdate::SRLBlobUpdate() {}

SRLBlobUpdate::~SRLBlobUpdate() {}

void SRLBlobUpdate::append(Dbb *dbb, TransId transId, int32 locPage,
                           int locLine, int32 blobPage, int blobLine) {
  START_RECORD(srlLargBlob, "SRLBlobUpdate::append");
  getTransaction(transId);
  putInt(dbb->tableSpaceId);
  putInt(transId);
  putInt(locPage);
  putInt(locLine);
  putInt(blobPage);
  putInt(blobLine);
}

void SRLBlobUpdate::read() {
  if (control->version >= srlVersion8)
    tableSpaceId = getInt();
  else
    tableSpaceId = 0;

  transactionId = getInt();
  locatorPage = getInt();
  locatorLine = getInt();
  dataPage = getInt();
  dataLine = getInt();

  if (log->tracePage == locatorPage || (dataPage && log->tracePage == dataPage))
    print();
}

void SRLBlobUpdate::pass1(void) {
  log->bumpPageIncarnation(locatorPage, tableSpaceId, objInUse);
  log->bumpPageIncarnation(dataPage, tableSpaceId, objInUse);
}

void SRLBlobUpdate::pass2(void) {
  bool ret1 = log->bumpPageIncarnation(locatorPage, tableSpaceId, objInUse);
  bool ret2 = log->bumpPageIncarnation(dataPage, tableSpaceId, objInUse);

  if (ret1) {
    StreamLogTransaction *transaction = log->findTransaction(transactionId);
    Dbb *dbb = log->getDbb(tableSpaceId);

    if (control->isPostFlush()) {
      if (transaction && ret2 && transaction->state == sltCommitted)
        Section::redoBlobUpdate(dbb, locatorPage, locatorLine, dataPage,
                                dataLine);
      else
        Section::redoBlobUpdate(dbb, locatorPage, locatorLine, 0, 0);
    }
  }
}

void SRLBlobUpdate::redo(void) {
  log->bumpPageIncarnation(locatorPage, tableSpaceId, objInUse);
  log->bumpPageIncarnation(dataPage, tableSpaceId, objInUse);
}

void SRLBlobUpdate::print() {
  logPrint("Blob tid %d, locator page %d/%d, line %d, blob page %d/%d\n",
           transactionId, locatorPage, tableSpaceId, locatorLine, dataPage,
           dataLine);
}

}  // namespace Changjiang
