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

// SRLRecordLocator.cpp: implementation of the SRLRecordLocator class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLRecordLocator.h"
#include "StreamLog.h"
#include "StreamLogControl.h"
#include "StreamLogTransaction.h"
#include "Section.h"
#include "Dbb.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLRecordLocator::SRLRecordLocator() {}

SRLRecordLocator::~SRLRecordLocator() {}

void SRLRecordLocator::append(Dbb *dbb, TransId transId, int id, int seq,
                              int32 page) {
  START_RECORD(srlRecordLocator, "SRLRecordLocator::append");

  if (transId) {
    StreamLogTransaction *trans = log->getTransaction(transId);

    if (trans) trans->setPhysicalBlock();
  }

  putInt(dbb->tableSpaceId);
  putInt(id);
  putInt(seq);
  putInt(page);
}

void SRLRecordLocator::read() {
  if (control->version >= srlVersion8)
    tableSpaceId = getInt();
  else
    tableSpaceId = 0;

  sectionId = getInt();
  sequence = getInt();
  pageNumber = getInt();
}

void SRLRecordLocator::pass1() {
  if (log->tracePage == pageNumber) print();

  log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse);
}

void SRLRecordLocator::pass2() {
  if (log->tracePage == pageNumber) print();

  if (log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse))
    log->getDbb(tableSpaceId)
        ->redoRecordLocatorPage(sectionId, sequence, pageNumber,
                                control->isPostFlush());
}

void SRLRecordLocator::redo() {
  if (!log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse)) return;
}

void SRLRecordLocator::print() {
  logPrint("RecordLocator sectionId %d/%d, sequence %d, page %d\n", sectionId,
           tableSpaceId, sequence, pageNumber);
}

}  // namespace Changjiang
