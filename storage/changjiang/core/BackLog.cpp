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

#include "Engine.h"
#include "BackLog.h"
#include "Database.h"
#include "TableSpaceManager.h"
#include "Dbb.h"
#include "Section.h"
#include "Index.h"
#include "IndexRootPage.h"
#include "Transaction.h"
#include "Serialize.h"
#include "RecordVersion.h"
#include "Bitmap.h"
#include "Format.h"
#include "Table.h"
#include "Log.h"

namespace Changjiang {

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

BackLog::BackLog(Database *db, const char *fileName) : database(db) {
  dbb = new Dbb(database->dbb, 0);
#ifndef CHANGJIANGDB
  dbb->createPath(fileName);
#endif
  dbb->create(fileName, dbb->pageSize, 0, HdrTableSpace, 0, NULL);
  dbb->noLog = true;
  dbb->tableSpaceId = TABLESPACE_ID_BACKLOG;  // reserved for internal use
  int32 sectionId = Section::createSection(dbb, NO_TRANSACTION);
  section = new Section(dbb, sectionId, NO_TRANSACTION);
  recordsBacklogged = 0;
  recordsReconstituted = 0;
  priorBacklogged = 0;
  priorReconstituted = 0;
}

BackLog::~BackLog(void) {
  delete section;
  delete dbb;
}

int32 BackLog::save(RecordVersion *record) {
  // Write the record and entire prior version chain
  // into a single backlog record

  Serialize stream;
  record->serialize(&stream);
  int32 backlogId = section->insertStub(NO_TRANSACTION);
  section->updateRecord(backlogId, &stream, NO_TRANSACTION, false);
  ++recordsBacklogged;

  return backlogId + 1;
}

RecordVersion *BackLog::fetch(int32 backlogId) {
  Serialize stream;
  section->fetchRecord(backlogId - 1, &stream, NO_TRANSACTION);
  RecordVersion *record = new RecordVersion(database, &stream);
  ++recordsReconstituted;

  return record;
}

void BackLog::deleteRecord(int32 backlogId) {
  section->expungeRecord(backlogId - 1);
}

void BackLog::update(int32 backlogId, RecordVersion *record) {
  Serialize stream;
  record->serialize(&stream);
  section->updateRecord(backlogId - 1, &stream, NO_TRANSACTION, false);
}

void BackLog::rollbackRecords(Bitmap *records, Transaction *transaction) {
  for (int backlogId = 0; (backlogId = records->nextSet(backlogId)) >= 0;
       ++backlogId) {
    RecordVersion *record = fetch(backlogId);

    if (record->getTransactionId() != transaction->transactionId) {
      record->release(REC_HISTORY);

      continue;
    }

    Table *table = record->format->table;

    if (!table->insertIntoTree(record, NULL, record->recordNumber)) {
      record->release(REC_HISTORY);
      int32 recordNumber = record->recordNumber;
      Record *rec = table->fetch(recordNumber);

      if (rec) {
        if (rec->getTransactionId() == transaction->transactionId)
          record->rollback(transaction);
        else
          record->release(REC_HISTORY);
      }

      continue;
    }

    record->rollback(transaction);
    SET_RECORD_ACTIVE(record, false);
    record->release(REC_HISTORY);
  }
}

void BackLog::reportStatistics(void) {
  if (recordsBacklogged == priorBacklogged &&
      recordsReconstituted == priorReconstituted)
    return;

  int deltaBacklogged = recordsBacklogged - priorBacklogged;
  int deltaReconstituted = recordsReconstituted = priorReconstituted;
  priorBacklogged = recordsBacklogged;
  priorReconstituted = recordsReconstituted;

  Log::log(LogInfo,
           "%d: Backlog: %d records backlogged, %d records reconstituted\n",
           database->deltaTime, deltaBacklogged, deltaReconstituted);
}

}  // namespace Changjiang
