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

// SRLFreePage.cpp: implementation of the SRLFreePage class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLFreePage.h"
#include "StreamLog.h"
#include "Dbb.h"
#include "StreamLogControl.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLFreePage::SRLFreePage() {}

SRLFreePage::~SRLFreePage() {}

void SRLFreePage::append(Dbb *dbb, int32 page) {
  START_RECORD(srlFreePage, "SRLFreePage::append");
  putInt(dbb->tableSpaceId);
  putInt(page);
  sync.unlock();
}

void SRLFreePage::read() {
  if (control->version >= srlVersion8)
    tableSpaceId = getInt();
  else
    tableSpaceId = 0;

  pageNumber = getInt();
}

void SRLFreePage::pass1() {
  if (log->tracePage == pageNumber) print();

  incarnation = log->bumpPageIncarnation(pageNumber, tableSpaceId, objDeleted);
}

void SRLFreePage::pass2(void) {
  if (pageNumber == log->tracePage) print();

  if (!log->bumpPageIncarnation(pageNumber, tableSpaceId, objDeleted)) return;

  log->redoFreePage(pageNumber, tableSpaceId);
}

void SRLFreePage::redo() {
  log->bumpPageIncarnation(pageNumber, tableSpaceId, objDeleted);
}

void SRLFreePage::print() {
  logPrint("Free Page %d  tableSpaceId %d\n", pageNumber, tableSpaceId);
}

void SRLFreePage::commit(void) {
  if (pageNumber == log->tracePage) print();
}

}  // namespace Changjiang
