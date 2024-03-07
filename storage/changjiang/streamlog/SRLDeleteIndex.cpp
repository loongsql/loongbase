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

// SRLDeleteIndex.cpp: implementation of the SRLDeleteIndex class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLDeleteIndex.h"
#include "SRLVersion.h"
#include "StreamLog.h"
#include "StreamLogControl.h"
#include "IndexRootPage.h"
#include "Index.h"
#include "Dbb.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLDeleteIndex::SRLDeleteIndex() {}

SRLDeleteIndex::~SRLDeleteIndex() {}

void SRLDeleteIndex::append(Dbb *dbb, TransId transId, int id, int idxVersion) {
  Sync syncIndexes(&log->syncIndexes, "SRLDeleteIndex::append(1)");
  syncIndexes.lock(Exclusive);

  START_RECORD(srlDeleteIndex, "SRLDeleteIndex::append(2)");
  putInt(dbb->tableSpaceId);
  log->getTransaction(transId);
  log->setIndexInactive(id, dbb->tableSpaceId);
  putInt(transId);
  putInt(id);
  putInt(idxVersion);
  sync.unlock();
}

void SRLDeleteIndex::read() {
  if (control->version >= srlVersion8)
    tableSpaceId = getInt();
  else
    tableSpaceId = 0;

  transactionId = getInt();
  indexId = getInt();
  indexVersion = getInt();
}

void SRLDeleteIndex::pass1() {
  log->bumpIndexIncarnation(indexId, tableSpaceId, objDeleted);
}

void SRLDeleteIndex::pass2() {
  if (!log->bumpIndexIncarnation(indexId, tableSpaceId, objDeleted)) return;

  Dbb *dbb = log->findDbb(tableSpaceId);
  if (!dbb) return;
  if (!control->isPostFlush()) return;

  IndexRootPage::redoIndexDelete(dbb, indexId);
}

void SRLDeleteIndex::redo() {
  log->bumpIndexIncarnation(indexId, tableSpaceId, objDeleted);
}

void SRLDeleteIndex::print() { logPrint("Delete Index %d\n", indexId); }

void SRLDeleteIndex::commit(void) {
  Sync sync(&log->syncIndexes, "SRLDeleteIndex::commit");
  sync.lock(Exclusive);
  Dbb *dbb = log->findDbb(tableSpaceId);

  if (!dbb) return;

  IndexRootPage::deleteIndex(dbb, indexId, transactionId);
}

}  // namespace Changjiang
