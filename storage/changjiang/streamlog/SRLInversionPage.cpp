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

// SRLInversionPage.cpp: implementation of the SRLInversionPage class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLInversionPage.h"
#include "StreamLogControl.h"
#include "Dbb.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLInversionPage::SRLInversionPage() {}

SRLInversionPage::~SRLInversionPage() {}

void SRLInversionPage::append(Dbb *dbb, int32 page, int32 up, int32 left,
                              int32 right, int length, const UCHAR *data) {
  START_RECORD(srlInversionPage, "SRLInversionPage::append");
  putInt(dbb->tableSpaceId);
  putInt(page);
  putInt(up);
  putInt(left);
  putInt(right);
  putInt(length);
  putData(length, data);

  sync.unlock();
}

void SRLInversionPage::pass1() {
  log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse);
}

void SRLInversionPage::pass2() {
  log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse);
}

void SRLInversionPage::read() {
  if (control->version >= srlVersion8)
    tableSpaceId = getInt();
  else
    tableSpaceId = 0;

  pageNumber = getInt();
  parent = getInt();
  prior = getInt();
  next = getInt();
  length = getInt();
  data = getData(length);

  if (log->tracePage && (log->tracePage == pageNumber ||
                         log->tracePage == prior || log->tracePage == next))
    print();
}

void SRLInversionPage::redo() {
  if (!log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse)) return;
}

void SRLInversionPage::print() {
  logPrint("Inversion page %d, parent %d, prior %d, next %d\n", pageNumber,
           parent, prior, next);
}

}  // namespace Changjiang
