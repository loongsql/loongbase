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

// SRLSectionPage.cpp: implementation of the SRLSectionPage class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLSectionPage.h"
#include "StreamLog.h"
#include "Section.h"
#include "Dbb.h"
#include "StreamLogControl.h"
#include "StreamLogTransaction.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLSectionPage::SRLSectionPage() {}

SRLSectionPage::~SRLSectionPage() {}

void SRLSectionPage::append(Dbb *dbb, TransId transId, int32 parent, int32 page,
                            int slot, int id, int seq, int lvl) {
  ASSERT(dbb->tableSpaceId >= 0);
  START_RECORD(srlSectionPage, "SRLSectionPage::append");

  if (transId) {
    StreamLogTransaction *trans = log->getTransaction(transId);

    if (trans) trans->setPhysicalBlock();
  }

  putInt(dbb->tableSpaceId);
  putInt(parent);
  putInt(page);
  putInt(slot);
  putInt(id);
  putInt(seq);
  putInt(lvl);
}

void SRLSectionPage::read() {
  if (control->version >= srlVersion8)
    tableSpaceId = getInt();
  else
    tableSpaceId = 0;

  parentPage = getInt();
  pageNumber = getInt();
  sectionSlot = getInt();
  sectionId = getInt();
  sequence = getInt();
  level = getInt();

  /***
  if ((log->tracePage && log->tracePage == pageNumber) ||
          (log->tracePage && log->tracePage && log->tracePage == parentPage))
          print();
  ***/
}

void SRLSectionPage::pass1() {
  if ((pageNumber && log->tracePage == pageNumber) ||
      (parentPage && log->tracePage == parentPage))
    print();

  log->bumpPageIncarnation(parentPage, tableSpaceId, objInUse);

  if (pageNumber) log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse);
}

void SRLSectionPage::pass2() {
  if ((pageNumber && log->tracePage == pageNumber) ||
      (parentPage && log->tracePage == parentPage))
    print();

  bool ret = true;

  if (pageNumber)
    ret = log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse);

  if (log->bumpPageIncarnation(parentPage, tableSpaceId, objInUse) && ret) {
    if (control->isPostFlush())
      Section::redoSectionPage(log->getDbb(tableSpaceId), parentPage,
                               pageNumber, sectionSlot, sectionId, sequence,
                               level);
  }
}

void SRLSectionPage::redo() {
  log->bumpPageIncarnation(parentPage, tableSpaceId, objInUse);

  if (pageNumber) log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse);
}

void SRLSectionPage::print() {
  logPrint(
      "SectionPage section %d/%d, parent %d, page %d, slot %d, sequence %d, "
      "level %d\n",
      sectionId, tableSpaceId, parentPage, pageNumber, sectionSlot, sequence,
      level);
}

}  // namespace Changjiang
