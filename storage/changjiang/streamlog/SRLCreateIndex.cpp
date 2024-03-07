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
// SRLCreateIndex.cpp: implementation of the SRLCreateIndex class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLCreateIndex.h"
#include "StreamLogControl.h"
#include "Index.h"
#include "IndexRootPage.h"
#include "Dbb.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLCreateIndex::SRLCreateIndex() {}

SRLCreateIndex::~SRLCreateIndex() {}

void SRLCreateIndex::append(Dbb *dbb, TransId transId, int32 id, int idxVersion,
                            int pageNumber) {
  START_RECORD(srlCreateIndex, "SRLCreateIndex::append");
  log->getTransaction(transId);
  log->setIndexActive(id, dbb->tableSpaceId);
  putInt(dbb->tableSpaceId);
  putInt(id);
  putInt(idxVersion);
  putInt(transId);
  putInt(pageNumber);
  sync.unlock();
}

void SRLCreateIndex::read() {
  if (control->version >= srlVersion8)
    tableSpaceId = getInt();
  else
    tableSpaceId = 0;

  indexId = getInt();
  indexVersion = getInt();
  transactionId = getInt();
  if (control->version >= srlVersion17)
    pageNumber = getInt();
  else
    pageNumber = 0;  // we'll probably suck in recovery
}

void SRLCreateIndex::pass1() {
  log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse);
  log->bumpIndexIncarnation(indexId, tableSpaceId, objInUse);
}

void SRLCreateIndex::pass2() {
  log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse);
  if (!log->bumpIndexIncarnation(indexId, tableSpaceId, objInUse)) return;

  if (!control->isPostFlush()) return;

  IndexRootPage::redoCreateIndex(log->getDbb(tableSpaceId), indexId,
                                 pageNumber);
}

void SRLCreateIndex::redo() {
  log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse);
  log->bumpIndexIncarnation(indexId, tableSpaceId, objInUse);
}

void SRLCreateIndex::print() { logPrint("Create Index %d\n", indexId); }

void SRLCreateIndex::commit(void) {}

}  // namespace Changjiang
