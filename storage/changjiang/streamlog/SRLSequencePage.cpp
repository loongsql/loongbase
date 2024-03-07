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

// SRLSequence.cpp: implementation of the SRLSequence class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLSequencePage.h"
#include "Dbb.h"
#include "StreamLogControl.h"

namespace Changjiang {

SRLSequencePage::SRLSequencePage(void) {}

SRLSequencePage::~SRLSequencePage(void) {}

void SRLSequencePage::append(Dbb *dbb, int pageSeq, int32 page) {
  START_RECORD(srlSequencePage, "SRLSequencePage::append");
  putInt(dbb->tableSpaceId);
  putInt(pageSeq);
  putInt(page);
  sync.unlock();
}

void SRLSequencePage::read(void) {
  if (control->version >= srlVersion8)
    tableSpaceId = getInt();
  else
    tableSpaceId = 0;

  pageSequence = getInt();
  pageNumber = getInt();

  if (pageNumber == log->tracePage) print();
}

void SRLSequencePage::pass1(void) {
  log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse);
}

void SRLSequencePage::pass2(void) {
  log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse);
}

void SRLSequencePage::redo(void) {
  if (log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse))
    log->getDbb(tableSpaceId)->redoSequencePage(pageSequence, pageNumber);
}

void SRLSequencePage::print(void) {
  logPrint("Sequence Page: sequence %d, page %d/%d\n", pageSequence, pageNumber,
           tableSpaceId);
}

}  // namespace Changjiang
