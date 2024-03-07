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

// SRLDropTable.cpp: implementation of the SRLDropTable class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLDropTable.h"
#include "StreamLog.h"
#include "StreamLogTransaction.h"
#include "StreamLogControl.h"
#include "Database.h"
#include "Section.h"
#include "Transaction.h"
#include "Log.h"
#include "Sync.h"
#include "Dbb.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLDropTable::SRLDropTable() {}

SRLDropTable::~SRLDropTable() {}

void SRLDropTable::append(Dbb *dbb, TransId transId, int section) {
  START_RECORD(srlDropTable, "SRLDropTable::append");
  putInt(dbb->tableSpaceId);
  log->getTransaction(transId);
  log->setSectionInactive(section, dbb->tableSpaceId);
  putInt(transId);
  putInt(section);
}

void SRLDropTable::read() {
  if (control->version >= srlVersion8)
    tableSpaceId = getInt();
  else
    tableSpaceId = 0;

  transactionId = getInt();
  sectionId = getInt();
}

void SRLDropTable::pass1() {
  log->bumpSectionIncarnation(sectionId, tableSpaceId, objDeleted);
}

void SRLDropTable::pass2(void) {
  log->bumpSectionIncarnation(sectionId, tableSpaceId, objDeleted);
}

void SRLDropTable::redo() {
  if (!log->bumpSectionIncarnation(sectionId, tableSpaceId, objDeleted)) return;

  Dbb *dbb = log->findDbb(tableSpaceId);

  if (dbb) dbb->deleteSection(sectionId, NO_TRANSACTION);
}

void SRLDropTable::print() {
  logPrint("Drop Section %d/%d\n", sectionId, tableSpaceId);
}

void SRLDropTable::commit(void) {
  Dbb *dbb = log->findDbb(tableSpaceId);

  if (dbb) Section::deleteSection(dbb, sectionId, NO_TRANSACTION);
}

void SRLDropTable::rollback(void) {}

}  // namespace Changjiang
