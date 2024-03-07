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
#include "SRLBlobDelete.h"
#include "StreamLogControl.h"
#include "StreamLogTransaction.h"
#include "Dbb.h"
#include "Section.h"

namespace Changjiang {

SRLBlobDelete::SRLBlobDelete(void) {}

SRLBlobDelete::~SRLBlobDelete(void) {}

void SRLBlobDelete::append(Dbb *dbb, int32 locPage, int locLine, int32 blobPage,
                           int blobLine) {
  START_RECORD(srlBlobDelete, "SRLBlobDelete::append");
  putInt(dbb->tableSpaceId);
  putInt(locPage);
  putInt(locLine);
  putInt(blobPage);
  putInt(blobLine);
}

void SRLBlobDelete::read(void) {
  tableSpaceId = getInt();
  locatorPage = getInt();
  locatorLine = getInt();
  dataPage = getInt();
  dataLine = getInt();

  if (locatorPage == log->tracePage || dataPage == log->tracePage) print();
}

void SRLBlobDelete::pass1(void) {
  log->bumpPageIncarnation(locatorPage, tableSpaceId, objInUse);
  log->bumpPageIncarnation(dataPage, tableSpaceId, objInUse);
}

void SRLBlobDelete::pass2(void) {
  bool ret1 = log->bumpPageIncarnation(locatorPage, tableSpaceId, objInUse);
  bool ret2 = log->bumpPageIncarnation(dataPage, tableSpaceId, objInUse);

  if (ret1) {
    StreamLogTransaction *transaction = log->findTransaction(transactionId);
    Dbb *dbb = log->getDbb(tableSpaceId);

    if (control->isPostFlush())
      Section::redoBlobDelete(
          dbb, locatorPage, locatorLine, dataPage, dataLine,
          ret2 && transaction && transaction->state == sltCommitted);
  }
}

void SRLBlobDelete::redo(void) {
  log->bumpPageIncarnation(locatorPage, tableSpaceId, objInUse);
  log->bumpPageIncarnation(dataPage, tableSpaceId, objInUse);
}

void SRLBlobDelete::print(void) {
  logPrint("BlobDelete locator page %d/%d, line %d, blob page %d/%d\n",
           locatorPage, tableSpaceId, locatorLine, dataPage, dataLine);
}

}  // namespace Changjiang
