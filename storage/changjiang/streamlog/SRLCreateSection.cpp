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

// SRLCreateSection.cpp: implementation of the SRLCreateSection class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLCreateSection.h"
#include "StreamLog.h"
#include "StreamLogTransaction.h"
#include "Database.h"
#include "Dbb.h"
#include "Log.h"
#include "StreamLogControl.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLCreateSection::SRLCreateSection() {}

SRLCreateSection::~SRLCreateSection() {}

void SRLCreateSection::append(Dbb *dbb, TransId transId, int32 sectionId) {
  START_RECORD(srlCreateSection, "SRLCreateSection::append");
  log->setSectionActive(sectionId, dbb->tableSpaceId);
  // StreamLogTransaction *trans =
  log->getTransaction(transId);
  putInt(dbb->tableSpaceId);
  putInt(transId);
  putInt(sectionId);
  sync.unlock();
}

void SRLCreateSection::read() {
  if (control->version >= srlVersion7)
    tableSpaceId = getInt();
  else
    tableSpaceId = 0;

  transactionId = getInt();
  sectionId = getInt();
}

void SRLCreateSection::pass1() {
  log->bumpSectionIncarnation(sectionId, tableSpaceId, objInUse);
}

void SRLCreateSection::pass2(void) {
  log->bumpSectionIncarnation(sectionId, tableSpaceId, objInUse);
}

void SRLCreateSection::commit() {
  // Log::debug("SRLCreateSection::commit: marking active section %d, tid %d\n",
  // sectionId, transactionId);
}

void SRLCreateSection::redo() {
  if (!log->bumpSectionIncarnation(sectionId, tableSpaceId, objInUse)) return;

  log->getDbb(tableSpaceId)->createSection(sectionId, transactionId);
}

void SRLCreateSection::print(void) {
  logPrint("Create Section %d/%d\n", sectionId, tableSpaceId);
}

}  // namespace Changjiang
