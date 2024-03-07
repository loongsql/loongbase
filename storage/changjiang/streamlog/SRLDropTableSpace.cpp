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

// SRLDropTableSpace.cpp: implementation of the SRLDropTableSpace class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "SRLDropTableSpace.h"
#include "TableSpace.h"
#include "TableSpaceManager.h"
#include "SRLVersion.h"
#include "StreamLogControl.h"
#include "Transaction.h"
#include "StreamLogTransaction.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLDropTableSpace::SRLDropTableSpace() {}

SRLDropTableSpace::~SRLDropTableSpace() {}

void SRLDropTableSpace::append(TableSpace *tableSpace,
                               Transaction *transaction) {
  START_RECORD(srlDropTableSpace, "SRLDropTableSpace::append");

  StreamLogTransaction *streamLogTransaction =
      log->getTransaction(transaction->transactionId);

  streamLogTransaction->allowConcurrentPorpoises = false;

  putInt(tableSpace->tableSpaceId);
  putInt(transaction->transactionId);
}

void SRLDropTableSpace::read() {
  tableSpaceId = getInt();

  if (control->version >= srlVersion10)
    transactionId = getInt();
  else
    transactionId = 0;
}

void SRLDropTableSpace::pass1() { log->setTableSpaceDropped(tableSpaceId); }

void SRLDropTableSpace::pass2() {
  log->tableSpaceManager->expungeTableSpace(tableSpaceId);
}

void SRLDropTableSpace::commit() {
  log->tableSpaceManager->expungeTableSpace(tableSpaceId);
}

void SRLDropTableSpace::redo() {}

}  // namespace Changjiang
