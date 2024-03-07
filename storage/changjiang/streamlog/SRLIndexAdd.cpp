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

// SRLDataPage.cpp: implementation of the SRLDataPage class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLIndexAdd.h"
#include "IndexKey.h"
#include "StreamLogControl.h"
#include "StreamLogTransaction.h"
#include "Dbb.h"
#include "Transaction.h"
#include "Index.h"

// SRLIndexAdd.cpp: implementation of the SRLIndexAdd class.
//
//////////////////////////////////////////////////////////////////////

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLIndexAdd::SRLIndexAdd() {}

SRLIndexAdd::~SRLIndexAdd() {}

void SRLIndexAdd::append(Dbb *dbb, int32 indexId, int idxVersion, IndexKey *key,
                         int32 recordNumber, TransId transactionId) {
  START_RECORD(srlIndexAdd, "SRLIndexAdd::append");
  putInt(dbb->tableSpaceId);
  log->getTransaction(transactionId);
  putInt(transactionId);
  putInt(indexId);
  putInt(idxVersion);
  putInt(recordNumber);
  putInt(key->keyLength);
  log->putData(key->keyLength, key->key);
}

void SRLIndexAdd::read() {
  if (control->version >= srlVersion8)
    tableSpaceId = getInt();
  else
    tableSpaceId = 0;

  transactionId = getInt();
  indexId = getInt();
  indexVersion = getInt();
  recordId = getInt();
  length = getInt();
  data = getData(length);
}

void SRLIndexAdd::redo() {
  if (!log->isIndexActive(indexId, tableSpaceId)) return;

  // StreamLogTransaction *transaction =
  control->getTransaction(transactionId);
  IndexKey indexKey(length, data);
  log->getDbb(tableSpaceId)
      ->addIndexEntry(indexId, indexVersion, &indexKey, recordId,
                      NO_TRANSACTION);
}

void SRLIndexAdd::pass1() { control->getTransaction(transactionId); }

void SRLIndexAdd::print() {
  logPrint("Index Add transaction %d, indexId %d/%d, recordId %d, length %d\n",
           transactionId, indexId, tableSpaceId, recordId, length);
}

}  // namespace Changjiang
