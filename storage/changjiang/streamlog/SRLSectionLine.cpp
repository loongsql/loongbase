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
#include "SRLSectionLine.h"
#include "StreamLog.h"
#include "Section.h"
#include "Dbb.h"
#include "StreamLogControl.h"

namespace Changjiang {

SRLSectionLine::SRLSectionLine(void) {}

SRLSectionLine::~SRLSectionLine(void) {}

void SRLSectionLine::append(Dbb *dbb, int32 sectionPageNumber,
                            int32 dataPageNumber) {
  START_RECORD(srlSectionLine, "SRLSectionLine::append");
  putInt(dbb->tableSpaceId);
  putInt(sectionPageNumber);
  putInt(dataPageNumber);
  sync.unlock();
}

void SRLSectionLine::read(void) {
  if (control->version >= srlVersion8)
    tableSpaceId = getInt();
  else
    tableSpaceId = 0;

  pageNumber = getInt();
  dataPageNumber = getInt();
}

void SRLSectionLine::pass1(void) {
  if (pageNumber == log->tracePage || dataPageNumber == log->tracePage) print();

  log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse);
  log->bumpPageIncarnation(dataPageNumber, tableSpaceId, objInUse);
}

void SRLSectionLine::pass2(void) {
  if (pageNumber == log->tracePage || dataPageNumber == log->tracePage) print();

  log->bumpPageIncarnation(dataPageNumber, tableSpaceId, objInUse);

  if (log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse))
    Section::redoSectionLine(log->getDbb(tableSpaceId), pageNumber,
                             dataPageNumber);
}

void SRLSectionLine::redo(void) {
  if (pageNumber == log->tracePage || dataPageNumber == log->tracePage) print();

  log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse);
  log->bumpPageIncarnation(dataPageNumber, tableSpaceId, objInUse);
}

void SRLSectionLine::print(void) {
  logPrint("SectionLine: locator page %d/%d, data page %d\n", pageNumber,
           tableSpaceId, dataPageNumber);
}

}  // namespace Changjiang
