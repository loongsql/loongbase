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

// SRLDataPage.cpp: implementation of the SRLDataPage class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLDataPage.h"
#include "StreamLog.h"
#include "StreamLogTransaction.h"
#include "StreamLogControl.h"
#include "Section.h"
#include "Dbb.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLDataPage::SRLDataPage() {}

SRLDataPage::~SRLDataPage() {}

void SRLDataPage::append(Dbb *dbb, TransId transId, int id, int32 locatorPage,
                         int32 page) {
  START_RECORD(srlDataPage, "SRLDataPage::append");

  if (transId) {
    StreamLogTransaction *trans = log->getTransaction(transId);

    if (trans) trans->setPhysicalBlock();
  }

  putInt(dbb->tableSpaceId);
  putInt(id);
  putInt(locatorPage);
  putInt(page);

  if (page == log->tracePage) {
    sectionId = id;
    pageNumber = page;
    locatorPageNumber = locatorPage;
    tableSpaceId = dbb->tableSpaceId;
    print();
  }
}

void SRLDataPage::read() {
  if (control->version >= srlVersion7)
    tableSpaceId = getInt();
  else
    tableSpaceId = 0;

  sectionId = getInt();
  locatorPageNumber = getInt();
  pageNumber = getInt();
}

void SRLDataPage::pass1() {
  if (log->tracePage == pageNumber || log->tracePage == locatorPageNumber)
    print();

  log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse);
  log->bumpPageIncarnation(locatorPageNumber, tableSpaceId, objInUse);
  log->bumpSectionIncarnation(sectionId, tableSpaceId, objInUse);
}

void SRLDataPage::pass2() {
  if (log->tracePage == pageNumber || log->tracePage == locatorPageNumber)
    print();

  bool sectionActive =
      log->bumpSectionIncarnation(sectionId, tableSpaceId, objInUse);
  bool locatorPageActive =
      log->bumpPageIncarnation(locatorPageNumber, tableSpaceId, objInUse);

  if (log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse)) {
    if (sectionActive) {
      if (control->isPostFlush())
        log->getDbb(tableSpaceId)
            ->redoDataPage(sectionId, pageNumber, locatorPageNumber);
    } else
      log->redoFreePage(pageNumber, tableSpaceId);
  } else if (sectionActive && locatorPageActive)
    Section::redoSectionLine(log->getDbb(tableSpaceId), locatorPageNumber,
                             pageNumber);
}

void SRLDataPage::redo() {
  log->bumpSectionIncarnation(sectionId, tableSpaceId, objInUse);
  log->bumpPageIncarnation(locatorPageNumber, tableSpaceId, objInUse);
  log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse);
}

void SRLDataPage::print() {
  logPrint("DataPage section %d/%d, locator page %d, page %d, \n", sectionId,
           tableSpaceId, locatorPageNumber, pageNumber);
}

}  // namespace Changjiang
