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

// SRLIndexPage.cpp: implementation of the SRLIndexPage class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLIndexPage.h"
#include "StreamLogControl.h"
#include "StreamLogTransaction.h"
#include "Dbb.h"
#include "IndexRootPage.h"
#include "Index.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLIndexPage::SRLIndexPage() {}

SRLIndexPage::~SRLIndexPage() {}

void SRLIndexPage::append(Dbb *dbb, TransId transId, int idxVersion, int32 page,
                          int32 lvl, int32 right, int length,
                          const UCHAR *data) {
  START_RECORD(srlIndexPage, "SRLIndexPage::append");

  if (transId) {
    StreamLogTransaction *trans = log->getTransaction(transId);

    if (trans) trans->setPhysicalBlock();
  }

  putInt(dbb->tableSpaceId);
  putInt(idxVersion);
  putInt(page);
  putInt(lvl);
  putInt(right);
  putInt(length);
  putData(length, data);
}

void SRLIndexPage::read() {
  if (control->version >= srlVersion8)
    tableSpaceId = getInt();
  else
    tableSpaceId = 0;

  if (control->version >= srlVersion12)
    indexVersion = getInt();
  else
    indexVersion = INDEX_VERSION_1;

  pageNumber = getInt();
  level = getInt();
  if (control->version < srlVersion19) {
    getInt();  // parent pointer
    getInt();  // prior pointer
  }
  next = getInt();
  length = getInt();
  data = getData(length);

  if (log->tracePage &&
      (log->tracePage == pageNumber || log->tracePage == next))
    print();
}

void SRLIndexPage::pass1() {
  log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse);
}

void SRLIndexPage::pass2() {
  if (log->tracePage == pageNumber) print();

  if (log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse)) {
    if (control->isPostFlush()) {
      bool haveSuperNodes = (control->version >= srlVersion14);
      IndexRootPage::redoIndexPage(log->getDbb(tableSpaceId), pageNumber, level,
                                   next, length, data, haveSuperNodes);
    }
  }
}

void SRLIndexPage::print() {
  logPrint("Index page %d/%d, level %d, next %d\n", pageNumber, tableSpaceId,
           level, next);
}

void SRLIndexPage::redo() {
  log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse);
}

}  // namespace Changjiang
