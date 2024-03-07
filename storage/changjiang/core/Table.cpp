/* Copyright (C) 2006-2008 MySQL AB, 2008-2009 Sun Microsystems, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

// Table.cpp: implementation of the Table class.
//
//////////////////////////////////////////////////////////////////////

#include <string.h>
#include <memory.h>
#include <stdio.h>
#include "Engine.h"
#include "Table.h"
#include "Field.h"
#include "Index.h"
#include "IndexKey.h"
#include "Database.h"
#include "Dbb.h"
#include "PStatement.h"
#include "Transaction.h"
#include "Value.h"
#include "Format.h"
#include "RSet.h"
#include "RecordVersion.h"
#include "Filter.h"
#include "FilterTree.h"
#include "FilterDifferences.h"
#include "RecordLeaf.h"
#include "RecordGroup.h"
#include "SQLError.h"
#include "ForeignKey.h"
#include "Sync.h"
#include "Bitmap.h"
#include "TableAttachment.h"
#include "Privilege.h"
#include "View.h"
#include "BinaryBlob.h"
#include "AsciiBlob.h"
#include "Log.h"
#include "CollationManager.h"
#include "Connection.h"
#include "Repository.h"
#include "Interlock.h"
#include "Collation.h"
#include "TableSpace.h"
#include "RecordScavenge.h"
#include "Section.h"
#include "BackLog.h"
#include "Thread.h"
#include "CycleLock.h"
#include "CycleManager.h"

#ifndef STORAGE_ENGINE
#include "Trigger.h"
#endif

namespace Changjiang {

//#define ATOMIC_UPDATE		Exclusive
#define ATOMIC_UPDATE Shared

#undef new

static const char *relatedTables[] = {"IndexFields", "Indexes",
                                      "Fields",      "Tables",

#ifndef STORAGE_ENGINE
                                      "Triggers",
#endif

                                      NULL};

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif
static bool needUniqueCheck(Index *index, Record *record);

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Table::Table(Database *db, int id, const char *schema, const char *tableName,
             TableSpace *tblSpace)
    : PrivilegeObject(db) {
  init(id, schema, tableName, tblSpace);
}

Table::Table(Database *db, const char *schema, const char *tableName, int id,
             int version, uint64 numberRecords, TableSpace *tblSpace)
    : PrivilegeObject(db) {
  init(id, schema, tableName, tblSpace);
  formatVersion = version;
  priorCardinality = numberRecords;
  cardinality = numberRecords;
}

Table::~Table() {
  for (Field *field; (field = fields);) {
    fields = field->next;
    delete field;
  }

  delete[] fieldVector;
  Format *format;

  for (int n = 0; n < FORMAT_HASH_SIZE; ++n)
    while ((format = formats[n])) {
      formats[n] = format->hash;
      delete format;
    }

  delete[] formats;

  for (Index *index; (index = indexes);) {
    indexes = index->next;
    delete index;
  }

  for (ForeignKey *key; (key = foreignKeys);) {
    foreignKeys = key->next;
    delete key;
  }

#ifndef STORAGE_ENGINE
  for (ChjTrigger *trigger; (trigger = triggers);) {
    triggers = trigger->next;
    trigger->release();
  }
#endif

  if (backloggedRecords && recordBitmap) {
    for (int32 recordNumber = 0;
         (recordNumber = recordBitmap->nextSet(recordNumber)) >= 0;
         ++recordNumber) {
      int32 backlogId = backloggedRecords->get(recordNumber);

      if (backlogId) database->backLog->deleteRecord(backlogId);
    }

    delete backloggedRecords;
  }

  delete view;
  if (records) delete records;

  if (recordBitmap) recordBitmap->release();

  if (emptySections) emptySections->release();
}

Field *Table::findField(const char *fieldName) {
  // const char *name = database->getSymbol(fieldName);
  const char *name = database->getString(fieldName);
  Sync sync(&syncObject, "Table::findField");
  sync.lock(Shared);

  FOR_FIELDS(field, this)
  if (field->name == name) return field;
  END_FOR;

  return NULL;
}

Field *Table::findField(const WCString *fieldName) {
  return findField(database->getSymbol(fieldName));
}

Field *Table::addField(const char *name, Type type, int length, int precision,
                       int scale, int flags) {
  Sync sync(&syncObject, "Table::addField");
  sync.lock(Exclusive);

  Field *field = NEW Field(this, nextFieldId++, name, type, length, precision,
                           scale, flags);
  addField(field);

  return field;
}

Index *Table::addIndex(const char *name, int numberFields, int type) {
  Sync sync(&syncObject, "Table::addIndex");
  sync.lock(Exclusive);

  Index *index = NEW Index(this, name, numberFields, type);
  addIndex(index);

  if ((type & IndexTypeMask) == PrimaryKey) primaryKey = index;

  return index;
}

void Table::dropIndex(const char *indexName, Transaction *transaction) {
  Sync sync(&syncObject, "Table::dropIndex");
  sync.lock(Exclusive);

  Index *index = findIndex(indexName);

  if (index) deleteIndex(index, transaction);
}

void Table::renameIndexes(const char *newTableName) {
  for (Index *index = indexes; index; index = index->next) {
    if (index->type != PrimaryKey) {
      // Assume that index name is <table>$<index>

      char newIndexName[256];
      const char *p = strchr((const char *)index->name, '$');
      sprintf(newIndexName, "%s%s", newTableName, (const char *)p);
      index->rename(newIndexName);
    }
  }
}

const char *Table::getName() { return name; }

void Table::create(const char *tableType, Transaction *transaction) {
  setType(tableType);
  dataSectionId = dbb->createSection(TRANSACTION_ID(transaction));
  blobSectionId = dbb->createSection(TRANSACTION_ID(transaction));

  // Iterate all indexes, assume indexId == -1

  FOR_ALL_INDEXES(index, this);
  index->create(transaction);
  END_FOR;
}

void Table::save() {
  PreparedStatement *statement = database->prepareStatement(
      (database->fieldExtensions)
          ? "insert Tables "
            "(tableName,tableId,dataSection,blobSection,currentVersion,type,"
            "schema,viewDefinition,tablespace) values (?,?,?,?,?,?,?,?,?);"
          : "insert Tables "
            "(tableName,tableId,dataSection,blobSection,currentVersion,type,"
            "schema,viewDefinition) values (?,?,?,?,?,?,?,?);");
  statement->setString(1, name);
  statement->setInt(2, tableId);

  if (view) {
    Stream stream;
    view->gen(&stream);
    char *def = stream.getString();
    statement->setString(8, def);
    delete[] def;
  } else {
    statement->setInt(3, dataSectionId);
    statement->setInt(4, blobSectionId);
    statement->setInt(5, formatVersion);
  }

  statement->setString(6, type);
  statement->setString(7, schemaName);

  if (tableSpace) statement->setString(9, tableSpace->name);

  statement->executeUpdate();
  statement->close();

  FOR_FIELDS(field, this)
  field->save();
  END_FOR;

  if (view)
    view->save(database);
  else {
    Format *format = getFormat(formatVersion);
    format->save(this);

    FOR_INDEXES(index, this);
    if (index->savePending) index->save();
    END_FOR;

    for (ForeignKey *key = foreignKeys; key; key = key->next) {
      key->bind(database);

      if (key->foreignTable == this) key->save(database);
    }
  }
}

void Table::insert(Transaction *transaction, int count, Field **fieldVector,
                   Value **values) {
  database->preUpdate();
  CycleLock cycleLock(database);

  if (view) {
    insertView(transaction, count, fieldVector, values);

    return;
  }

  if (!dataSection) findSections();

  RecordVersion *record = NULL;
  bool insertedIntoTree = false;
  bool addedToTransaction = false;
  int32 recordNumber = -1;

  try {
    // Get current format for record

    Format *format = getFormat(formatVersion);
    record = allocRecordVersion(format, transaction, NULL);
    record->state = recInserting;

    // Handle any default values

    FOR_FIELDS(field, this)
    if (field->defaultValue)
      record->setValue(transaction->transactionState, field->id,
                       field->defaultValue, false, false);
    END_FOR;

    // Copy field values into record

    Value temp;

    for (int n = 0; n < count; ++n) {
      Field *field = fieldVector[n];
      Value *value = values[n];

      if (field->repository)
        value = field->repository->defaultRepository(field, value, &temp);

      record->setValue(transaction->transactionState, field->id, value, false,
                       false);
    }

    fireTriggers(transaction, PreInsert, NULL, record);

    // Checkin with any attachments

    FOR_OBJECTS(TableAttachment *, attachment, &attachments)
    if (attachment->mask & PRE_INSERT) attachment->preInsert(this, record);
    END_FOR;

    // We're done playing; finalize the record

    record->finalize(transaction);

    // Make insert/update atomic, then check for unique index duplicats

    recordNumber = record->recordNumber =
        dbb->insertStub(dataSection, transaction->transactionState);

    checkNullable(record);  // Verify that record is valid

    // If insertStub says it is free, then there should never be
    // anything in the record tree for this record number.

    if (!insertIntoTree(record, NULL, recordNumber))
      FATAL("Table::insert(Field, Value) cannot insertIntoTree");
    insertedIntoTree = true;

    transaction->addRecord(record);
    addedToTransaction = true;

    insertIndexes(transaction, record);
    updateInversion(record, transaction);
    fireTriggers(transaction, PostInsert, NULL, record);
    record->state = recData;
    record->release(REC_HISTORY);
  } catch (...) {
    if (insertedIntoTree)
      if (!insertIntoTree(NULL, record, recordNumber))
        FATAL("Table::insert(Field, Value) cannot backout insertIntoTree");

    if (addedToTransaction) transaction->removeRecord(record);

    if (recordNumber >= 0) {
      dbb->updateRecord(dataSection, recordNumber, NULL,
                        transaction->transactionState, false);
      dataSection->expungeRecord(recordNumber);
      record->recordNumber = -1;
    }

    garbageCollect(record, NULL, transaction, true);

    if (record) {
      SET_RECORD_ACTIVE(record, false);
      record->queueForDelete();
    }

    throw;
  }
}

Format *Table::getFormat(int version) {
  if (format && (format->version == version)) return format;

  Format *format;

  for (format = formats[version % FORMAT_HASH_SIZE]; format;
       format = format->hash)
    if (format->version == version) return format;

  Sync syncObj(&syncObject, "Table::getFormat(1)");
  syncObj.lock(Exclusive);

  Sync syncDDL(&database->syncSysDDL, "Table::getFormat(2)");
  syncDDL.lock(Shared);

  PStatement statement = database->prepareStatement(
      "select version, fieldId, dataType, offset, length, scale, maxId from "
      "system.Formats where tableId=? and version=?");
  statement->setInt(1, tableId);
  statement->setInt(2, version);
  RSet set = statement->executeQuery();
  format = NEW Format(this, set);
  syncDDL.unlock();
  addFormat(format);

  return format;
}

void Table::reformat() {
  buildFieldVector();
  Sync sync(&syncObject, "Table::reformat");
  sync.lock(Exclusive);
  database->invalidateCompiledStatements(this);

  if (format && format->validate(this)) return;

  format = NEW Format(this, ++formatVersion);
  addFormat(format);

  if (!database->formatting) {
    format->save(this);
    PreparedStatement *statement = database->prepareStatement(
        "update Tables set currentVersion=? where tableName=? and schema=?");
    int n = 1;
    statement->setInt(n++, formatVersion);
    statement->setString(n++, name);
    statement->setString(n++, schemaName);
    statement->executeUpdate();
    statement->close();
  }
}

void Table::updateRecord(RecordVersion *record) {
  FOR_OBJECTS(TableAttachment *, attachment, &attachments)
  if (attachment->mask & POST_COMMIT) attachment->postCommit(this, record);
  END_FOR;
}

int Table::numberFields() { return fieldCount; }

// fetchNext(int32 start) finds the next record in the database
// using sequential access.  It uses and balances 3 different
// ways to find the next record.  It does this concurrently.
// 1) recordBitmap is the first place it checks. It is assumed to
//    have a bit set for every record known below its highwater mark.
// 2) Record Tree;  accessed by fetch(). tracks ecords in cache.
// 3) RecordLocatorPages; accessed by databaseFetch & dbb->findNextRecord

Record *Table::fetchNext(int32 start) {
  if (view) throw SQLEXCEPTION(BUG_CHECK, "attempted physical access to view");

  if (!dataSection) findSections();

  Stream stream;
  Sync sync(&syncObject, "Table::fetchNext");
  sync.lock(Shared);
  Record *record = NULL;
  int32 recordNumber = start;

  for (;;) {
    int32 bitNumber = recordBitmap->nextSet(recordNumber);

    // If no bit and we've seen the end of the table, we're done

    if (bitNumber < 0) {
      if (eof) return NULL;

      recordNumber = recordBitmapHighWater;
    } else if (eof || bitNumber < recordBitmapHighWater) {
      // Record should exist somewhere

      if (records && (record = records->fetch(bitNumber))) {
        // Don't bother with a record that is half-way inserted.

        if (record->state == recInserting) {
          record->release(REC_HISTORY);
          recordNumber = bitNumber + 1;
          continue;
        }

        break;
      }

      if (backloggedRecords && (record = backlogFetch(bitNumber))) break;

      sync.unlock();

      // The bit is set below the highwater mark,
      // but the record is not found in the tree.
      // Look for it in the record cache.  If found,
      // add it to the tree, allowing for concurrency.
      // Try this three times before giving up.

      for (int n = 0; (record = databaseFetch(bitNumber)); ++n) {
        if (insertIntoTree(record, NULL, bitNumber)) {
          record->poke();

          return record;
        }

        SET_RECORD_ACTIVE(record, false);
        record->release(REC_HISTORY);

        sync.lock(Shared);
        if ((record = records->fetch(bitNumber))) {
          record->poke();

          return record;
        }
        sync.unlock();

        ASSERT(n < 3);
      }

      // Table::databaseFetch returned NULL.  That bit should no longer be set.
      // But Table::syncObject is not currently locked.
      // While you are reading this, another thread may be adding this record
      // and setting the bit.  So get an exclusive lock and double check that it
      // is not in the tree.

      sync.lock(Exclusive);
      if (records && (record = records->fetch(bitNumber))) {
        record->poke();  // Whoops, there it is!
        return record;
      }

      // The record did not re-appear.  Clear the bit and try the next bit.

      recordBitmap->clear(bitNumber);
      recordNumber = bitNumber + 1;
      sync.unlock();
      sync.lock(Shared);

      continue;
    }

    // We're above the high water mark; let's find the next record in the
    // database

    int32 recNumber = dbb->findNextRecord(dataSection, recordNumber, &stream);

    // If we didn't find anything else, mark table as read and try again

    if (recNumber < 0) {
      eof = true;

      if (bitNumber < 0) return NULL;

      continue;
    }

    recordBitmapHighWater = recNumber + 1;

    // If we've got that record in memory, use it instead

    if (records && (record = records->fetch(recNumber))) {
      if (recNumber <= bitNumber) break;
    } else if (backloggedRecords && (record = backlogFetch(recNumber))) {
      if (recNumber <= bitNumber) break;
    } else {
      if (stream.totalLength == 0) {
        Log::logBreak("Table::fetchNext record %d in table %s.%s disappeared\n",
                      recordNumber, (const char *)schemaName,
                      (const char *)name);
        dbb->updateRecord(dataSection, recNumber, NULL, NULL, false);
        recordNumber = recNumber + 1;

        continue;
      }

      sync.unlock();
      record = allocRecord(recNumber, &stream);

      if (insertIntoTree(record, NULL, recNumber)) {
        if (bitNumber < 0 || recNumber <= bitNumber) return record;
      } else {
        SET_RECORD_ACTIVE(record, false);
        record->release(REC_HISTORY);
      }

      sync.lock(Shared);
    }

    if ((bitNumber >= 0) && (bitNumber < recNumber) && (records) &&
        (record = records->fetch(bitNumber)))
      break;

    sync.unlock();
    sync.lock(Exclusive);
    recordBitmap->clear(bitNumber);
    recordNumber = bitNumber + 1;
  }

  record->poke();

  return record;
}

Record *Table::databaseFetch(int32 recordNumber) {
  Stream stream;
  ageGroup = database->currentGeneration;

  if (!dataSection) findSections();

  if (!dbb->fetchRecord(dataSection, recordNumber, &stream)) {
    Sync sync(&syncObject, "Table::databaseFetch");
    sync.lock(Exclusive);
    recordBitmap->clear(recordNumber);

    return NULL;
  }

  // If record has a zero length, it doesn't really exist (must have been
  // created but neither committed or backed out from a previous invocation.
  // If any case, get rid of it now!

  if (stream.totalLength == 0) {
    Log::logBreak("Table::databaseFetch record %d in table %s.%s disappeared\n",
                  recordNumber, (const char *)schemaName, (const char *)name);
    dbb->updateRecord(dataSection, recordNumber, NULL, NULL, false);

    return NULL;
  }

  Record *record;

  try {
    record = allocRecord(recordNumber, &stream);
  } catch (SQLException &exception) {
    Log::logBreak("Table::databaseFetch record %d in table %s.%s: %s\n",
                  recordNumber, (const char *)schemaName, (const char *)name,
                  exception.getText());

    switch (exception.getSqlcode()) {
      case OUT_OF_MEMORY_ERROR:
      case OUT_OF_RECORD_MEMORY_ERROR:
        throw;
    }

    return NULL;
  }

  return record;
}

void Table::deleteIndex(Index *index, Transaction *transaction) {
  if (index == primaryKey) primaryKey = NULL;

  index->deleteIndex(transaction);

  for (Index **ptr = &indexes; *ptr; ptr = &((*ptr)->next))
    if (*ptr == index) {
      *ptr = index->next;
      break;
    }

  delete index;
}

void Table::setDataSection(int32 section) { dataSectionId = section; }

void Table::setBlobSection(int32 section) { blobSectionId = section; }

void Table::loadFields() {
  const char *sql =
      (database->fieldExtensions)
          ? "select "
            "field,fieldId,dataType,length,scale,flags,collationsequence,"
            "repositoryName,domainName,precision"
            " from system.Fields where tableName=? and schema=?"
          : "select field,fieldId,dataType,length,scale,flags,collationsequence"
            " from system.Fields where tableName=? and schema=?";

  PreparedStatement *statement = database->prepareStatement(sql);
  statement->setString(1, name);
  statement->setString(2, schemaName);
  ResultSet *resultSet = statement->executeQuery();

  while (resultSet->next()) {
    const char *fieldName = resultSet->getString(1);
    const char *collationName = resultSet->getString(7);
    Collation *collation = CollationManager::getCollation(collationName);
    int id = resultSet->getInt(2);
    Type type = (Type)resultSet->getInt(3);
    int length = resultSet->getInt(4);
    int scale = resultSet->getInt(5);
    int flags = resultSet->getInt(6);
    int precision = (database->fieldExtensions) ? resultSet->getInt(10) : 0;
    Field *field =
        NEW Field(this, id, fieldName, type, length, precision, scale, flags);
    addField(field);

    if (collation) field->setCollation(collation);

    if (nextFieldId <= field->id) nextFieldId = field->id + 1;

    if (database->fieldExtensions) {
      const char *repositoryName = resultSet->getSymbol(8);

      if (repositoryName && repositoryName[0]) {
        Repository *repository =
            database->getRepository(schemaName, repositoryName);
        field->setRepository(repository);
      }
    }
  }

  buildFieldVector();
  resultSet->close();
  statement->close();
}

void Table::loadIndexes() {
  PreparedStatement *statement = database->prepareStatement(
      "select indexName,indexType,indexId,fieldCount from system.Indexes where "
      "tableName=? and schema=?");
  statement->setString(1, name);
  statement->setString(2, schemaName);
  ResultSet *set = statement->executeQuery();

  while (set->next()) {
    Index *index;
    const char *indexName = set->getString(1);

    if (!findIndex(indexName)) {
      try {
        index = NEW Index(this,
                          indexName,        // name
                          set->getInt(2),   // type
                          set->getInt(3),   // id
                          set->getInt(4));  // field count
      } catch (SQLException &exception) {
        Log::log("Index %s in %s.%s damaged: %s\n", indexName, schemaName, name,
                 exception.getText());
        index = NEW Index(this,
                          indexName,        // name
                          set->getInt(4),   // field count
                          set->getInt(2));  // type
        index->setDamaged();
      }

      if (index->type == PrimaryKey) primaryKey = index;

      addIndex(index);
    }
  }

  set->close();
  statement->close();
  ForeignKey::loadForeignKeys(database, this);
  ForeignKey::loadPrimaryKeys(database, this);
}

void Table::init(int id, const char *schema, const char *tableName,
                 TableSpace *tblSpace) {
  ageGroup = database->currentGeneration;

  if ((tableSpace = tblSpace))
    dbb = tableSpace->dbb;
  else
    dbb = database->dbb;

  tableId = id;
  setName(schema, tableName);
  view = NULL;
  fields = NULL;
  indexes = NULL;
  fieldCount = 0;
  blobSectionId = Section::INVALID_SECTION_ID;
  dataSectionId = Section::INVALID_SECTION_ID;
  blobSection = NULL;
  dataSection = NULL;
  backloggedRecords = NULL;
  nextFieldId = 0;
  setType("TABLE");
  formatVersion = 0;
  format = NULL;
  changed = false;
  deleting = false;
  foreignKeys = NULL;
  records = NULL;
  recordBitmapHighWater = 0;
  eof = false;
  markedForDelete = false;
  primaryKey = NULL;
  formats = NEW Format *[FORMAT_HASH_SIZE];
  triggers = NULL;
  memset(formats, 0, sizeof(Format *) * FORMAT_HASH_SIZE);
  maxFieldId = 0;
  fieldVector = NULL;
  recordBitmap = NEW Bitmap;
  emptySections = NEW Bitmap;
  debugThawedRecords = 0;
  debugThawedBytes = 0;
  cardinality = 0;
  priorCardinality = 0;
  alterIsActive = false;
  syncObject.setName("Table::syncObject");
  syncTriggers.setName("Table::syncTriggers");
  syncAlter.setName("Table::syncAlter");

  for (int n = 0; n < SYNC_THAW_SIZE; n++)
    syncThaw[n].setName("Table::syncThaw");
}

Record *Table::fetch(int32 recordNumber) {
  Sync sync(&syncObject, "Table::fetch");
  sync.lock(Shared);
  Record *record;

  for (;;) {
    if (records) {
      RecordSection *section = records;
      int id = recordNumber;

      while (section->base) {
        int slot = id / section->base;
        id = id % section->base;

        if (slot >= RECORD_SLOTS) goto notFound;

        if (!(section = ((RecordGroup *)section)->records[slot])) goto notFound;
      }

      if ((record = section->fetch(id))) {
        record->poke();

        return record;
      }
    }

  notFound:

    if (backloggedRecords) {
      int32 backlogId = backloggedRecords->get(recordNumber);

      if (backlogId) {
        sync.unlock();
        sync.lock(Exclusive);
        record = database->backLog->fetch(backlogId);

        if (insertIntoTree(record, NULL, recordNumber)) return record;

        SET_RECORD_ACTIVE(record, false);
        record->release(REC_HISTORY);

        continue;
      }
    }

    sync.unlock();

    if (!(record = databaseFetch(recordNumber))) return NULL;

    record->poke();

    if (insertIntoTree(record, NULL, recordNumber)) return record;

    SET_RECORD_ACTIVE(record, false);
    record->release(REC_HISTORY);
    sync.lock(Shared);
  }
}

Record *Table::treeFetch(int32 recordNumber) {
  Sync sync(&syncObject, "Table::treeFetch");
  sync.lock(Shared);

  if (!records) return NULL;

  RecordSection *section = records;
  int id = recordNumber;

  while (section->base) {
    int slot = id / section->base;
    id = id % section->base;

    if (slot >= RECORD_SLOTS) return NULL;

    if (!(section = ((RecordGroup *)section)->records[slot])) return NULL;
  }

  return section->fetch(id);
}

// Read a record chain from the backlog. If another thread
// beats us to it, just read from the tree. Try multiple
// times, if necessary.

Record *Table::backlogFetch(int32 recordNumber) {
  if (backloggedRecords) {
    int32 backlogId = backloggedRecords->get(recordNumber);
    int attempts = 5;

    while (backlogId && attempts--) {
      Record *record = database->backLog->fetch(backlogId);

      if (record) {
        if (insertIntoTree(record, NULL, recordNumber)) {
          RECORD_HISTORY(record);
          return record;
        }
        record->release(REC_HISTORY);
      }

      if ((record = fetch(recordNumber))) return record;
    }
  }

  return NULL;
}

void Table::rollbackRecord(RecordVersion *recordToRollback,
                           Transaction *transaction) {
  SET_RECORD_ACTIVE(recordToRollback, false);

  int priorState = recordToRollback->state;
  recordToRollback->state = recRollback;

  // Find the record that will become the current version.

  Record *priorRecord = recordToRollback->getPriorVersion();

  if (priorRecord) {
    priorRecord->addRef(REC_HISTORY);
    priorRecord->setSuperceded(false);
  }

  // Replace the current version of this record.

  if (!insertIntoTree(priorRecord, recordToRollback,
                      recordToRollback->recordNumber)) {
    if (priorRecord == NULL && priorState == recDeleted) return;

    // The store of this record into the record leaf failed. No way to recover.
    // While the base record is uncommitted, only that transaction can change
    // it.

    recordToRollback->printRecord("Table::rollbackRecord failed");
    FATAL("Table::rollbackRecord-insertIntoTree failed, priorState =",
          priorState);
  }

  if (!priorRecord && recordToRollback->recordNumber >= 0)
    deleteRecord(recordToRollback, transaction);

  garbageCollect(recordToRollback, priorRecord, transaction, true);

  if (backloggedRecords) deleteRecordBacklog(recordToRollback->recordNumber);

  if (priorRecord) priorRecord->release(REC_HISTORY);
}

void Table::addFormat(Format *format) {
  int slot = format->version % FORMAT_HASH_SIZE;
  format->hash = formats[slot];
  formats[slot] = format;
}

int32 Table::getBlobId(Value *value, int32 oldId, bool cloneFlag,
                       TransactionState *transaction) {
  int32 id;

  if (cloneFlag) switch (value->getType()) {
      case ClobPtr: {
        AsciiBlob *clob = (AsciiBlob *)value->getClob();
        ASSERT(oldId == 0);
        ASSERT(clob->section == blobSection);
        id = clob->recordNumber;
        clob->release();

        return id;
      }

      case BlobPtr: {
        BinaryBlob *blob = (BinaryBlob *)value->getBlob();
        ASSERT(oldId == 0);
        ASSERT(blob->section == blobSection);
        id = blob->recordNumber;
        blob->release();

        return id;
      }

      default:;
    }

  switch (value->getType()) {
    case ClobPtr: {
      Clob *clob = value->getClob();

      if (clob->isBlobReference()) {
        id = getIndirectId(clob, transaction);
        clob->release();

        return id;
      }

      clob->release();
    } break;

    case BlobPtr: {
      Blob *blob = value->getBlob();

      if (blob->isBlobReference()) {
        id = getIndirectId(blob, transaction);
        blob->release();

        return id;
      }

      blob->release();
    } break;

    default:;
  }

  if (!blobSection) blobSection = dbb->findSection(blobSectionId);

  Blob *blob = value->getBlob();
  int32 recordNumber = dbb->insertStub(blobSectionId, transaction);
  blob->length();
  dbb->updateBlob(blobSection, recordNumber, (BinaryBlob *)blob, transaction);
  blob->release();

  return recordNumber;
}

int32 Table::getIndirectId(BlobReference *reference,
                           TransactionState *transaction) {
  if (!blobSection) blobSection = dbb->findSection(blobSectionId);

  const char *repoName = database->getSymbol(reference->repositoryName);
  Repository *repository = database->getRepository(schemaName, repoName);

  if (!reference->dataUnset) repository->storeBlob(reference, transaction);

  Stream refData;
  reference->getReference(&refData);
  int32 recordNumber = dbb->insertStub(blobSectionId, transaction);
  dbb->updateBlob(blobSection, recordNumber, &refData, transaction);

  return (recordNumber) ? -recordNumber : ZERO_REPOSITORY_PLACE;
}

void Table::makeSearchable(Field *field, Transaction *transaction) {
  Record *record;
  int32 records = 0;
  int32 words = 0;

  // Look through records and record versions

  for (int32 next = 0; (record = fetchNext(next));) {
    next = record->recordNumber + 1;

    for (Record *version = record; version;
         version = version->getPriorVersion())
      if (version->hasRecord()) {
        Value value;
        version->getValue(field->id, &value);
        Filter stream(tableId, field->id, version->recordNumber, &value);
        words += database->addInversion(&stream, transaction);
        ++records;

        if (records % 100 == 0)
          Log::debug("%d records inverted with %d words\n", records, words);
      }

    record->release(REC_HISTORY);
  }

  database->flushInversion(transaction);
}

void Table::makeNotSearchable(Field *field, Transaction *transaction) {
  Record *record;

  // Look through records and record versions

  for (int32 next = 0; (record = fetchNext(next));) {
    next = record->recordNumber + 1;

    for (Record *version = record; version;
         version = version->getPriorVersion())
      if (version->hasRecord()) {
        Value value;
        version->getValue(field->id, &value);
        Filter stream(tableId, field->id, version->recordNumber, &value);
        database->removeFromInversion(&stream, transaction);
      }

    record->release(REC_HISTORY);
  }

  database->flushInversion(transaction);
}

/**
@brief		index update , combined with unique check (atomic)

                Determine if the record we intend to write will have a duplicate
conflict with any pending or visible records.
@details	For each  unique index, obtain an exclusive lock and check the
index update index if the search succeeded by not finding a duplicate. Retry if
a wait occurred. If a duplicate is found, an exception should be caught by the
caller. non-unique indexes are  updated without any check
**/

void Table::updateIndexes(Transaction *transaction, RecordVersion *record,
                          Record *oldRecord) {
  if (indexes) FOR_INDEXES(index, this);
  Sync sync(&(index->syncUnique), "Table::updateIndexes");

  if (needUniqueCheck(index, record))
    for (;;) {
      sync.lock(Exclusive);

      if (!checkUniqueIndex(index, transaction, record, &sync)) break;
    }

  index->update(oldRecord, record, transaction);
  END_FOR;
}

/**
@brief		Uniqueness check combined with index insert (atomic)
**/

void Table::insertIndexes(Transaction *transaction, RecordVersion *record) {
  if (indexes) {
    Sync syncTable(&syncObject, "Table::insertIndexes");

    FOR_INDEXES(index, this);
    Sync syncUnique(&index->syncUnique, "Table::insertIndexes");

    if (needUniqueCheck(index, record))
      for (;;) {
        syncUnique.lock(Exclusive);

        if (!checkUniqueIndex(index, transaction, record, &syncUnique)) break;
      }

    // Block concurrent DDL with a shared lock. Double-check the
    // index id in case the index was deleted.

    syncTable.lock(Shared);

    if (index->indexId != -1) index->insert(record, transaction);

    syncTable.unlock();
    END_FOR;
  }
}

void Table::update(Transaction *transaction, Record *oldRecord,
                   int numberFields, Field **updateFields, Value **values) {
  database->preUpdate();
  RecordVersion *record = NULL;
  bool updated = false;
  int recordNumber = oldRecord->recordNumber;
  CycleLock cycleLock(database);

  try {
    // Find current record format and create new record version

    Format *format = getFormat(formatVersion);
    record = allocRecordVersion(format, transaction, oldRecord);

    // Copy field values from old record version

    FOR_FIELDS(field, this)
    Value value;
    int id = field->id;
    oldRecord->getValue(id, &value);
    record->setValue(transaction->transactionState, id, &value, true, false);
    END_FOR;

    // Copy field values being changed

    Value temp;

    for (int n = 0; n < numberFields; ++n) {
      Field *field = updateFields[n];
      Value *value = values[n];

      if (field->repository)
        value = field->repository->defaultRepository(field, value, &temp);

      record->setValue(transaction->transactionState, field->id, value, false,
                       false);
    }

    // Fire pre-operation triggers

    fireTriggers(transaction, PreUpdate, oldRecord, record);

    // Make sure no constraints are violated

    checkNullable(record);

    // Checkin with any table attachments

    FOR_OBJECTS(TableAttachment *, attachment, &attachments)
    if (attachment->mask & PRE_UPDATE) attachment->preUpdate(this, record);
    END_FOR;

    // OK, finalize the record

    record->finalize(transaction);

    // Make insert/update atomic, then check for unique index duplicats

    validateAndInsert(transaction, record);
    transaction->addRecord(record);
    updated = true;
    updateIndexes(transaction, record, oldRecord);

    updateInversion(record, transaction);
    fireTriggers(transaction, PostUpdate, oldRecord, record);

    // If this is a re-update in the same transaction and the same savepoint,
    // carefully remove the prior version.

    record->scavengeSavepoint(transaction, transaction->curSavePointId);
    record->release(REC_HISTORY);
  } catch (...) {
    if (updated) {
      transaction->removeRecord(record);
      insertIntoTree(oldRecord, record, recordNumber);
    }

    garbageCollect(record, oldRecord, transaction, true);

    if (record) {
      Record *prior = record->getPriorVersion();
      if (prior) prior->setSuperceded(false);

      if (record->state == recLock) record->deleteData();

      SET_RECORD_ACTIVE(record, false);
      record->queueForDelete();
    }

    throw;
  }
}

void Table::reIndexInversion(Transaction *transaction) {
  CycleLock cycleLock(database);
  bool hits = false;

  FOR_FIELDS(field, this)
  if (field->flags & SEARCHABLE) {
    hits = true;
    break;
  }
  END_FOR;

  if (!hits) return;

  Record *record;

  for (int32 next = 0; (record = fetchNext(next));) {
    next = record->recordNumber + 1;

    {
      for (Record *version = record; version;
           version = version->getPriorVersion())
        if (version->hasRecord()) FOR_FIELDS(field, this)
      if (field->flags & SEARCHABLE) {
        Value value;
        version->getValue(field->id, &value);
        Filter stream(tableId, field->id, version->recordNumber, &value);
        // value.getStream(&stream, false);
        database->addInversion(&stream, transaction);
      }
      END_FOR;
    }

    record->release(REC_HISTORY);
  }
}

bool Table::isCreated() { return dataSectionId != Section::INVALID_SECTION_ID; }

Index *Table::getPrimaryKey() { return primaryKey; }

void Table::addForeignKey(ForeignKey *key) {
  Sync sync(&syncObject, "Table::addForeignKey");
  sync.lock(Exclusive);

  key->next = foreignKeys;
  foreignKeys = key;
}

Field *Table::findField(int id) {
  if (id <= maxFieldId) return fieldVector[id];

  return NULL;
}

ForeignKey *Table::findForeignKey(Field *field, bool foreign) {
  Sync sync(&syncObject, "Table::findForeignKey");
  sync.lock(Shared);

  for (ForeignKey *key = foreignKeys; key; key = key->next) {
    key->bind(database);

    if (key->isMember(field, foreign)) return key;
  }

  return NULL;
}

bool Table::indexExists(ForeignKey *foreignKey) {
  Sync sync(&syncObject, "Table::indexExists");
  sync.lock(Shared);

  FOR_INDEXES(index, this);
  if (index->numberFields == foreignKey->numberFields) {
    int n;

    for (n = 0; n < index->numberFields; ++n)
      if (index->fields[n] != foreignKey->foreignFields[n]) break;

    if (n == index->numberFields) return true;
  }
  END_FOR;

  return false;
}

ForeignKey *Table::findForeignKey(ForeignKey *key) {
  Sync sync(&syncObject, "Table::findForeignKey");
  sync.lock(Shared);

  for (ForeignKey *foreignKey = foreignKeys; foreignKey;
       foreignKey = foreignKey->next)
    if (foreignKey->matches(key, database)) return foreignKey;

  return NULL;
}

void Table::deleteRecord(Transaction *transaction, Record *orgRecord) {
  database->preUpdate();
  CycleLock cycleLock(database);

  Record *candidate = fetch(orgRecord->recordNumber);

  if (!candidate) return;

  RECORD_HISTORY(candidate);

  checkAncestor(candidate, orgRecord);
  RecordVersion *record;
  bool wasLock = false;

  if (candidate->state == recLock &&
      candidate->getTransactionState() == transaction->transactionState) {
    if (candidate->getSavePointId() == transaction->curSavePointId) {
      record = (RecordVersion *)candidate;
      ASSERT(record->getPriorVersion() == orgRecord);
      wasLock = true;
    } else
      record = allocRecordVersion(NULL, transaction, candidate);
  } else {
    Record *oldVersion = candidate->fetchVersion(transaction);

    if (oldVersion != candidate) {
      candidate->release(REC_HISTORY);

      throw SQLError(UPDATE_CONFLICT,
                     "delete conflict in table %s.%s record %d", schemaName,
                     name, orgRecord->recordNumber);
    }

    ASSERT(candidate->hasRecord());
    record = allocRecordVersion(NULL, transaction, candidate);
    candidate->release(REC_HISTORY);
  }

  record->state = recDeleted;
  fireTriggers(transaction, PreDelete, orgRecord, NULL);

  // Do any necessary cascading

  for (ForeignKey *key = foreignKeys; key; key = key->next) {
    key->bind(database);

    if (key->primaryTable == this && key->deleteRule == importedKeyCascade)
      key->cascadeDelete(transaction, orgRecord);
  }

  // Checkin with any attachments

  FOR_OBJECTS(TableAttachment *, attachment, &attachments)
  if (attachment->mask & PRE_DELETE) attachment->preDelete(this, record);
  END_FOR;

  if (wasLock) {
    record->state = recDeleted;
    --transaction->deletedRecords;
  } else {
    try {
      validateAndInsert(transaction, record);
    } catch (...) {
      SET_RECORD_ACTIVE(record, false);
      record->release(REC_HISTORY);

      throw;
    }

    transaction->addRecord(record);
  }

  dataSection->reserveRecordNumber(record->recordNumber);
  record->release(REC_HISTORY);
  fireTriggers(transaction, PostDelete, orgRecord, NULL);
}

int Table::getFieldId(const char *name) {
  Field *field = findField(name);

  if (!field) return -1;

  return field->id;
}

void Table::updateInversion(Record *record, Transaction *transaction) {
  FOR_FIELDS(field, this)
  if (field->flags & SEARCHABLE) {
    Value value;
    record->getValue(field->id, &value);
    Filter stream(tableId, field->id, record->recordNumber, &value);
    database->addInversion(&stream, transaction);
  }
  END_FOR;
}

void Table::drop(Transaction *transaction) {
  FOR_OBJECTS(TableAttachment *, attachment, &attachments)
  attachment->tableDeleted(this);
  END_FOR;

  markedForDelete = true;
  PrivilegeObject::drop();

  for (ForeignKey *key; (key = foreignKeys);) {
    try {
      key->bind(database);
    } catch (SQLException &exception) {
      Log::log("Error dropping foreign key for table %s.%s: %s\n", schemaName,
               name, exception.getText());
    }

    key->deleteForeignKey();
  }

  Transaction *sysTransaction = database->getSystemTransaction();

  for (Index *index = indexes; index; index = index->next)
    index->deleteIndex(sysTransaction);

  PreparedStatement *statement = database->prepareStatement(
      "delete from ForeignKeys where primaryTableId=? or foreignTableId=?");
  statement->setInt(1, tableId);
  statement->setInt(2, tableId);
  statement->executeUpdate();
  statement->close();

  for (const char **tbl = relatedTables; *tbl; ++tbl) {
    char sql[512];
    snprintf(sql, sizeof(sql),
             "delete from system.%s where schema=? and tableName=?", *tbl);
    statement = database->prepareStatement(sql);
    statement->setString(1, schemaName);
    statement->setString(2, name);
    statement->executeUpdate();
    statement->close();
  }

  statement = database->prepareStatement("delete from Formats where tableId=?");
  statement->setInt(1, tableId);
  statement->executeUpdate();
  statement->close();

  if (view) view->drop(database);

  database->commitSystemTransaction();
}

void Table::truncate(Transaction *transaction) {
  deleting = true;

  // Delete data and blob sections

  expunge(transaction);

  // Recreate data and blob sections

  dataSectionId = dbb->createSection(TRANSACTION_ID(transaction));
  blobSectionId = dbb->createSection(TRANSACTION_ID(transaction));
  findSections();

  emptySections->clear();
  recordBitmap->clear();

  cardinality = 0;
  priorCardinality = cardinality;

  // Update system.tables with new section ids and cardinality

  PreparedStatement *statement = database->prepareStatement(
      "update system.tables set dataSection=?,"
      " blobSection=?, cardinality=? where tableId=?");
  statement->setInt(1, dataSectionId);
  statement->setInt(2, blobSectionId);
  statement->setLong(3, cardinality);
  statement->setInt(4, tableId);
  statement->executeUpdate();
  statement->close();

  if (records) {
    delete records;
    records = NULL;
  }

  rebuildIndexes(transaction, true);

  // Reset remaining Table attributes

  ageGroup = database->currentGeneration;
  debugThawedRecords = 0;
  debugThawedBytes = 0;
  alterIsActive = false;
  deleting = false;
}

void Table::checkNullable(Record *record) {
  Value value;

  FOR_FIELDS(field, this)
  if (field->getNotNull()) {
    record->getValue(field->id, &value);

    if (value.isNull())
      throw SQLEXCEPTION(RUNTIME_ERROR, "illegal null in field %s in table %s",
                         field->getName(), getName());
  }
  END_FOR;
}

void Table::addField(Field *field) {
  Field **ptr;

  for (ptr = &fields; *ptr; ptr = &((*ptr)->next))
    ;

  field->next = *ptr;
  *ptr = field;
  ++fieldCount;
  maxFieldId = MAX(maxFieldId, field->id);
}

void Table::addIndex(Index *index) {
  Index **ptr;

  for (ptr = &indexes; *ptr; ptr = &((*ptr)->next))
    ;

  index->next = *ptr;
  *ptr = index;
}

void Table::dropIndex(Index *index) {
  Sync sync(&syncObject, "Table::dropIndex");
  sync.lock(Exclusive);

  for (Index **ptr = &indexes; *ptr; ptr = &(*ptr)->next)
    if (*ptr == index) {
      *ptr = index->next;
      break;
    }
}

void Table::addAttachment(TableAttachment *attachment) {
  attachments.appendUnique(attachment);
}

void Table::dropField(Field *field) {
  if (primaryKey && primaryKey->isMember(field))
    throw SQLEXCEPTION(DDL_ERROR,
                       "can't drop field %s in %s.%s -- member of primary key",
                       (const char *)field->name, (const char *)schemaName,
                       (const char *)name);

  for (Index *index = indexes; index; index = index->next)
    if (index->isMember(field))
      throw SQLEXCEPTION(DDL_ERROR,
                         "can't drop field %s in %s.%s -- member of index %s",
                         (const char *)field->name, (const char *)schemaName,
                         (const char *)name, (const char *)index->name);

  for (ForeignKey *key = foreignKeys; key; key = key->next) {
    key->bind(database);
    if (key->isMember(field, true))
      throw SQLEXCEPTION(
          DDL_ERROR, "can't drop field %s in %s.%s -- foreign key for %s.%s",
          (const char *)field->name, (const char *)schemaName,
          (const char *)name, (const char *)key->primaryTable->schemaName,
          (const char *)key->primaryTable->name);
  }

  for (Field **ptr = &fields; *ptr; ptr = &(*ptr)->next)
    if (*ptr == field) {
      *ptr = field->next;
      --fieldCount;
      break;
    }

  database->invalidateCompiledStatements(this);
  field->drop();
  delete field;
}

ForeignKey *Table::dropForeignKey(ForeignKey *key) {
  for (ForeignKey *hit, **ptr = &foreignKeys; (hit = *ptr); ptr = &hit->next)
    if (hit->matches(key, database)) {
      *ptr = hit->next;
      return hit;
    }

  Log::log("Table::dropForeignKey: foreign key lost\n");
  return NULL;
}

const char *Table::getSchema() { return schemaName; }

void Table::populateIndex(Index *index, Transaction *transaction) {
  Record *record;
  CycleLock cycleLock(database);

  for (int32 next = 0, count = 0; (record = fetchNext(next)); ++count) {
    next = record->recordNumber + 1;

    for (Record *version = record; version;
         version = version->getPriorVersion())
      if (version->hasRecord()) index->insert(version, transaction);

    record->release(REC_HISTORY);

#ifdef _DEBUG
    if (count && count % 100000 == 0)
      Log::debug("populateIndex: %d records indexed\n", count);
#endif
  }

  transaction->hasUpdates = true;
}

PrivObject Table::getPrivilegeType() { return PrivTable; }

Index *Table::findIndex(const char *indexName) {
  for (Index *index = indexes; index; index = index->next)
    if (index->name == indexName) return index;

  return NULL;
}

void Table::setView(View *viewObject) { view = viewObject; }

// Prune old invisible records from this table and inventory the rest.

void Table::pruneRecords(RecordScavenge *recordScavenge) {
  if (!records) return;

  Sync syncObj(&syncObject, "Table::pruneRecords");
  syncObj.lock(Shared);
  CycleLock cyleLock(database);

  if (records) records->pruneRecords(this, 0, recordScavenge);
}

void Table::retireRecords(RecordScavenge *recordScavenge) {
  if (!records) return;

  Sync syncObj(&syncObject, "Table::retireRecords");
  syncObj.lock(Shared);
  CycleLock cyleLock(database);

  if (!records) return;

  emptySections->clear();
  records->retireRecords(this, 0, recordScavenge);
  syncObj.unlock();

  // Get an exclusive lock only if there are empty leaf nodes. Find and
  // delete the empty nodes using the stored record numbers as identifiers.

  if (emptySections->count > 0) {
    syncObj.lock(Exclusive);

    // Delete these newly emptied RecordLeaf sections

    for (int sectionNumber = 0;
         (sectionNumber = emptySections->nextSet(0)) >= 0;) {
      int recordNumber = sectionNumber * RECORD_SLOTS;
      records->retireSections(this, recordNumber);
      emptySections->clear(sectionNumber);
    }

    // Check if there are any sections/active records left in this table.

    if (!records->anyActiveRecords()) {
      delete records;
      records = NULL;
    }
  }

  return;
}

bool Table::insertIntoTree(Record *record, Record *prior, int recordNumber) {
  ageGroup = database->currentGeneration;

  Sync sync(&syncObject, "Table::insert");

  if (!record || !records)
    sync.lock(Exclusive);
  else
    sync.lock(Shared);

  if (!records) records = NEW RecordLeaf;

  // Bump the record use count on the assumption that the
  // store will succeed.  Release it later if it fails.

  if (record) record->addRef(REC_HISTORY);

  if (records->store(record, prior, recordNumber, &records)) {
    if (prior) {
      SET_RECORD_ACTIVE(prior, false);
      prior->release(REC_HISTORY);
    }

    if (record) {
      SET_RECORD_ACTIVE(record, true);

      if (!recordBitmap->setSafe(recordNumber)) {
        sync.unlock();
        sync.lock(Exclusive);
        recordBitmap->set(recordNumber);
      }
    }

    return true;
  }

  // The store() failed.

  if (record) record->release(REC_HISTORY);

  return false;
}

bool Table::duplicateBlob(Value *blob, int fieldId, Record *recordChain) {
  bool isDuplicate = false;

  if (!recordChain) return isDuplicate;

  Section *section;
  int recordNumber = 0;

  switch (blob->getType()) {
    case BlobPtr: {
      BinaryBlob *data = (BinaryBlob *)blob->getBlob();
      section = data->section;
      recordNumber = data->recordNumber;
      data->release();  // Release for the data pointer.
      break;
    }

    case ClobPtr: {
      AsciiBlob *data = (AsciiBlob *)blob->getClob();
      section = data->section;
      recordNumber = data->recordNumber;
      data->release();  // Release for the data pointer.
      break;
    }

    default:
      return isDuplicate;
  }

  for (Record *record = recordChain; record; record = record->getPriorVersion())
    if (record->hasRecord()) {
      Value value;
      record->getValue(fieldId, &value);

      switch (value.getType()) {
        case BlobPtr: {
          BinaryBlob *data = (BinaryBlob *)value.getBlob();

          if (data->section == section && data->recordNumber == recordNumber)
            isDuplicate = true;

          data->release();
          break;
        }

        case ClobPtr: {
          AsciiBlob *data = (AsciiBlob *)value.getBlob();

          if (data->section == section && data->recordNumber == recordNumber)
            isDuplicate = true;

          data->release();
          break;
        }

        default:
          break;
      }
    }

  return isDuplicate;
}

void Table::expungeBlob(Value *blob) {
  Section *section;
  int recordNumber = 0;

  switch (blob->getType()) {
    case BlobPtr: {
      BinaryBlob *data = (BinaryBlob *)blob->getBlob();
      section = data->section;
      recordNumber = data->recordNumber;
      data->release();  // Release for the data pointer.
      break;
    }

    case ClobPtr: {
      AsciiBlob *data = (AsciiBlob *)blob->getClob();
      section = data->section;
      recordNumber = data->recordNumber;
      data->release();  // Release for the data pointer.
      break;
    }

    default:
      return;
  }

  // Log::debug ("Expunging blob %d/%d\n", blob->data.blobId.sectionId,
  // blob->data.blobId.recordNumber);

  if (recordNumber < 0)
    recordNumber = (recordNumber == ZERO_REPOSITORY_PLACE) ? 0 : -recordNumber;

  ASSERT(section);
  dbb->updateRecord(section, recordNumber, NULL, NULL, true);
  dbb->expungeRecord(section, recordNumber);
}

void Table::garbageCollect(Record *leaving, Record *staying,
                           Transaction *transaction, bool quiet) {
  if (!leaving && !staying) return;

  Sync sync(&syncObject, "Table::garbageCollect(Obj)");
  sync.lock(Shared);
  CycleLock cycleLock(database);

  // Clean up field indexes

  FOR_INDEXES(index, this);
  index->garbageCollect(leaving, staying, transaction, quiet);
  END_FOR;

  // Clean up inversion

  FOR_FIELDS(field, this)
  if (field->flags & SEARCHABLE)
    garbageCollectInversion(field, leaving, staying, transaction);
  END_FOR;

  // Garbage collect blobs

  FOR_FIELDS(field, this)
  if (field->type == Asciiblob || field->type == Binaryblob) {
    Bitmap blobs;
    Record *record;
    Value value;

    for (record = leaving; record && record != staying;
         record = record->getGCPriorVersion())
      if (record->hasRecord()) {
        record->getRawValue(field->id, &value);

        if ((value.getType() == Asciiblob || value.getType() == Binaryblob))
          blobs.set(value.getBlobId());
      }

    for (record = staying; record; record = record->getPriorVersion())
      if (record->hasRecord()) {
        record->getRawValue(field->id, &value);

        if ((value.getType() == Asciiblob || value.getType() == Binaryblob))
          blobs.clear(value.getBlobId());
      }

    for (int blobId = 0; (blobId = blobs.nextSet(blobId)) >= 0; ++blobId) {
      BinaryBlob *blob = getBinaryBlob(blobId);
      value.setValue(blob);
      blob->release();
      expungeBlob(&value);
    }
  }
  END_FOR
}

#ifndef STORAGE_ENGINE
void Table::zapLinkages() {
  Sync sync(&syncTriggers, "Table::zapLinkages");
  sync.lock(Shared);

  for (ChjTrigger *trigger = triggers; trigger; trigger = trigger->next)
    trigger->zapLinkages();
}
#endif

void Table::addTrigger(ChjTrigger *trigger) {
#ifndef STORAGE_ENGINE
  Sync sync(&syncTriggers, "Table::addTrigger");
  sync.lock(Exclusive);
  ChjTrigger **ptr = &triggers;

  for (; *ptr; ptr = &(*ptr)->next)
    if (trigger->position < (*ptr)->position) break;

  trigger->next = *ptr;
  *ptr = trigger;
#endif
}

void Table::fireTriggers(Transaction *transaction, int operation,
                         Record *before, RecordVersion *after) {
#ifndef STORAGE_ENGINE
  Sync sync(&syncTriggers, "Table::fireTriggers");
  sync.lock(Shared);

  for (ChjTrigger *trigger = triggers; trigger; trigger = trigger->next)
    if (trigger->active) {
      if ((trigger->mask & operation) &&
          trigger->isEnabled(transaction->connection))
        trigger->fireTrigger(transaction, operation, before, after);

      if (trigger->mask & (PreCommit | PostCommit))
        transaction->commitTriggers = true;
    }
#endif
}

void Table::loadStuff() {
  loadFields();
  loadIndexes();

#ifndef STORAGE_ENGINE
  if (!isNamed("SYSTEM", "TRIGGERS") && !isNamed("SYSTEM", "TRIGGERCLASSES"))
    ChjTrigger::getTableTriggers(this);
#endif
}

ChjTrigger *Table::findTrigger(const char *name) {
#ifndef STORAGE_ENGINE
  Sync sync(&syncTriggers, "Table::findTrigger");
  sync.lock(Shared);

  for (ChjTrigger *trigger = triggers; trigger; trigger = trigger->next)
    if (trigger->name == name) return trigger;
#endif

  return NULL;
}

void Table::dropTrigger(ChjTrigger *trigger) {
#ifndef STORAGE_ENGINE
  Sync sync(&syncTriggers, "Table::dropTrigger");
  sync.lock(Exclusive);

  for (ChjTrigger **ptr = &triggers; *ptr; ptr = &(*ptr)->next)
    if (*ptr == trigger) {
      *ptr = trigger->next;
      break;
    }

  trigger->release();
#endif
}

int Table::nextColumnId(int previous) {
  for (int n = MAX(0, previous + 1); n <= maxFieldId; ++n)
    if (fieldVector[n]) return n;

  return -1;
}

int Table::nextPrimaryKeyColumn(int previous) {
  if (!primaryKey) return -1;

  if (previous < 0) return primaryKey->fields[0]->id;

  int max = primaryKey->numberFields - 1;

  for (int n = 0; n < max; ++n)
    if (primaryKey->fields[n]->id == previous)
      return primaryKey->fields[n + 1]->id;

  return -1;
}

void Table::buildFieldVector() {
  delete[] fieldVector;
  fieldVector = NEW Field * [maxFieldId + 1];
  memset(fieldVector, 0, sizeof(Field *) * (maxFieldId + 1));

  for (Field *field = fields; field; field = field->next)
    fieldVector[field->id] = field;
}

void Table::postCommit(Transaction *transaction, RecordVersion *record) {
  RecordVersion *after = (record->hasRecord()) ? record : NULL;

  try {
    fireTriggers(transaction, PostCommit, record->getPriorVersion(), after);
  } catch (...) {
  }
}

void Table::garbageCollectInversion(Field *field, Record *leaving,
                                    Record *staying, Transaction *transaction) {
  InversionFilter *leave = getFilters(field, leaving, staying);

  if (!leave) return;

  InversionFilter *stay = getFilters(field, staying, NULL);

  if (stay) leave = NEW FilterDifferences(leave, stay);

  database->removeFromInversion(leave, transaction);
  delete leave;
}

InversionFilter *Table::getFilters(Field *field, Record *records,
                                   Record *limit) {
  InversionFilter *inversionFilter = NULL;

  for (Record *record = records; record && record != limit;
       record = record->getGCPriorVersion())
    if (record->hasRecord()) {
      Value value;
      record->getValue(field->id, &value);

      if (!value.isNull()) {
        Filter *filter =
            NEW Filter(tableId, field->id, record->recordNumber, &value);

        if (inversionFilter)
          inversionFilter = NEW FilterTree(inversionFilter, filter);
        else
          inversionFilter = filter;
      }
    }

  return inversionFilter;
}

void Table::reIndex(Transaction *transaction) {
  Record *record;

  for (int32 next = 0; (record = fetchNext(next));) {
    next = record->recordNumber + 1;

    for (Record *version = record; version;
         version = version->getPriorVersion())
      if (version->hasRecord()) FOR_INDEXES(index, this);
    index->insert(version, transaction);
    END_FOR;

    record->release(REC_HISTORY);
  }
}

void Table::setType(const char *typeName) {
  type = database->getSymbol(typeName);
}

void Table::checkDrop() {
  ForeignKey *key;

  for (key = foreignKeys; key; key = key->next) {
    try {
      key->bind(database);
    } catch (SQLException &exception) {
      Log::log("problem during table drop: %s\n", exception.getText());
      continue;
    }
    if (key->primaryTable == this)  // && key->foreignTable != this)
    {
      throw SQLEXCEPTION(DDL_ERROR,
                         "can't drop table %s.%s -- foreign key for %s.%s",
                         (const char *)schemaName, (const char *)name,
                         (const char *)key->foreignTable->schemaName,
                         (const char *)key->foreignTable->name);
      key->foreignTable->bind(this);
    } else
      key->primaryTable->bind(this);
  }

  PreparedStatement *statement = database->prepareStatement(
      "select viewName,viewSchema from system.view_tables where tableName=? "
      "and schema=?");
  statement->setString(1, name);
  statement->setString(2, schemaName);
  ResultSet *resultSet = statement->executeQuery();
  JString view;
  JString viewSchema;
  bool hit;

  if (hit = resultSet->next()) {
    view = resultSet->getString(1);
    viewSchema = resultSet->getString(2);
  }

  resultSet->close();
  statement->close();

  if (hit)
    throw SQLEXCEPTION(DDL_ERROR,
                       "can't drop table %s.%s -- referenced in view for %s.%s",
                       (const char *)schemaName, (const char *)name,
                       (const char *)viewSchema, (const char *)view);
}

bool Table::isDuplicate(Index *index, Record *record1, Record *record2) {
  Value val1, val2;

  if (!record1->hasRecord() || !record2->hasRecord()) return false;

  for (int n = 0; n < index->numberFields; ++n) {
    int partialLength = index->getPartialLength(n);
    Field *field = index->fields[n];
    record1->getValue(field->id, &val1);
    record2->getValue(field->id, &val2);

    if (val1.isNull() || val2.isNull()) return false;

    if (field->collation) {
      if (partialLength) {
        field->collation->truncate(&val1, partialLength);
        field->collation->truncate(&val2, partialLength);
      }

      if (field->collation->compare(&val1, &val2) != 0) return false;
    } else {
      if (partialLength) {
        val1.truncateString(partialLength);
        val2.truncateString(partialLength);
      }

      if (val1.compare(&val2) != 0) return false;
    }
  }

  return true;
}

/**
@brief		Determine if the record we intend to write will have a duplicate
conflict with any pending or visible records within a single index.
@details	For each record number found in a scanIndex, call
checkUniqueRecordVersion. Return true if a wait occured. Return false if no
duplicate was found.
**/

bool Table::checkUniqueIndex(Index *index, Transaction *transaction,
                             RecordVersion *record, Sync *sync) {
  Bitmap bitmap;
  IndexKey indexKey(index);
  index->makeKey(record, &indexKey);
  index->scanIndex(&indexKey, &indexKey, false, NULL, &bitmap);

  for (int32 recordNumber = 0;
       (recordNumber = bitmap.nextSet(recordNumber)) >= 0; ++recordNumber) {
    int retry = checkUniqueRecordVersion(recordNumber, index, transaction,
                                         record, sync);

    if (retry) return true;  // restart the search since a wait occurred.
  }

  return false;  // Did not find a duplicate in this index
}

/**
@brief		Determine if the record we intend to write will have a duplicate
conflict with any pending or visible recordVersions for a single index and
record Number.
@details	Search through the record version , call
checkUniqueRecordVersion. Return true if a wait occured. Return false if no
duplicate was found. Throw an exception if a duplicate WAS found
**/

bool Table::checkUniqueRecordVersion(int32 recordNumber, Index *index,
                                     Transaction *transaction,
                                     RecordVersion *record, Sync *syncUnique) {
  Record *rec;
  Record *oldRecord = record->getPriorVersion();
  // Transaction *activeTransaction = NULL;
  TransactionState *activeTransState = NULL;
  State state = CommittedVisible;

  if (oldRecord && recordNumber == oldRecord->recordNumber)
    return false;  // Check next record number.

  // This flag is used to skip all records in the chain between the
  // first younger committed record and the first older committed record.

  bool foundFirstCommitted = false;

  if (!(rec = fetch(recordNumber))) return false;  // Check next record number.

  for (Record *dup = rec; dup; dup = dup->getPriorVersion()) {
    if (dup == record) continue;  // Check next record version

    // Get the record's transaction state. Don't wait yet.

    state = transaction->getRelativeState(dup, DO_NOT_WAIT);

    // Check for a deleted record or a record lock

    if (!dup->hasRecord()) {
      // If the record is a lock record, keep looking for a dup.

      if (dup->state == recLock) continue;  // Next record version.

      if (dup->state == recRollback) continue;  // Next record version.

      // The record has been deleted.
      ASSERT(dup->state == recDeleted);

      switch (state) {
        case CommittedVisible:
        case Us:
          // No conflict with a visible deleted record.
          rec->release(REC_HISTORY);

          if (activeTransState) activeTransState->release();

          return false;  // Check next record number.

        case CommittedInvisible:
          // This state only happens for consistent read
          ASSERT(IS_CONSISTENT_READ(transaction->isolationLevel));
          foundFirstCommitted = true;

          continue;  // Next record version.

        case Active:
          // A pending transaction deleted a record.
          // Keep looking for a possible duplicate conflict,
          // either visible, or pending at a savepoint.

          activeTransState = dup->getTransactionState();
          activeTransState->addRef();

          continue;

        default:
          continue;  // record was deleted, keep looking for a dup.
      }
    }

    // We can skip CommittedInvisible record versions between the first
    // one and the record version visible to this transaction.

    if ((state == CommittedInvisible) && foundFirstCommitted) continue;

    if (state == RolledBack) continue;  // check next record version

    if (isDuplicate(index, record, dup)) {
      if (state == Active) {
        dup->addRef(REC_HISTORY);
        syncUnique->unlock();  // release lock before wait

        // Wait for that transaction, then restart checkUniqueIndexes()

        state = transaction->getRelativeState(dup, WAIT_IF_ACTIVE);
        dup->release(REC_HISTORY);  // We are done with this now.

        if (state != Deadlock) {
          rec->release(REC_HISTORY);

          if (activeTransState) activeTransState->release();

          return true;  // retry after a wait
        }
      }

      else if (activeTransState) {
        syncUnique->unlock();  // release lock before wait

        state = transaction->getRelativeState(activeTransState, WAIT_IF_ACTIVE);

        if (state != Deadlock) {
          activeTransState->release();
          rec->release(REC_HISTORY);

          return true;  // retry after a wait
        }
      }

      // Found a duplicate conflict or a deadlock.

      rec->release(REC_HISTORY);

      if (activeTransState) activeTransState->release();

      const char *text = "duplicate values for key %s in table %s.%s";
      int code = UNIQUE_DUPLICATE;

      if (state == Deadlock) {
        text = "deadlock on key %s in table %s.%s";
        code = DEADLOCK;
      }

      SQLEXCEPTION exception(code, text, (const char *)index->name,
                             (const char *)schemaName, (const char *)name);
      exception.setObject(schemaName, index->name);

      throw exception;
    }

    // This record was not a duplicate.  Keep looking?

    if (state == Active) {
      // This pending record is not a duplicate but an older version is.
      // Only wait on this record if the duplicate is visible or pending
      // at a savepoint.

      if (!activeTransState) {
        activeTransState = dup->getTransactionState();

        if (activeTransState) activeTransState->addRef();
      }

      continue;  // check next record version
    }

    // If the record is pending by us, then this record version is the only
    // one we need to look at.

    if (state == Us) {
      rec->release(REC_HISTORY);

      if (activeTransState) activeTransState->release();

      return false;  // Check next record number.
    }

    if (state == CommittedInvisible)
      foundFirstCommitted = true;  // continue checking record versions.

    if (state == CommittedVisible) {
      rec->release(REC_HISTORY);

      if (activeTransState) activeTransState->release();

      return false;  // Check next record number
    }
  }  // for each record version...

  if (rec) rec->release(REC_HISTORY);

  if (activeTransState) activeTransState->release();

  return false;  // Check next record number
}

bool Table::dropForeignKey(int fieldCount, Field **fields, Table *references) {
  for (ForeignKey *key = foreignKeys; key; key = key->next) {
    key->bind(database);

    if (key->foreignTable != this) continue;

    if (references && key->primaryTable != references) continue;

    if (fieldCount != key->numberFields) continue;

    bool hit = true;

    for (int n = 0; n < fieldCount; ++n)
      if (key->foreignFields[n] != fields[n]) {
        hit = false;
        break;
      }

    if (hit) {
      key->deleteForeignKey();
      return true;
    }
  }

  return false;
}

bool Table::foreignKeyMember(ForeignKey *key) {
  for (ForeignKey *foreignKey = foreignKeys; foreignKey;
       foreignKey = foreignKey->next)
    if (foreignKey == key) return true;

  return false;
}

int Table::countActiveRecords() {
  Sync sync(&syncObject, "Table::countActiveRecords");
  sync.lock(Shared);

  if (!records) return 0;

  return records->countActiveRecords();
}

int Table::chartActiveRecords(int *chart) {
  Sync sync(&syncObject, "Table::countActiveRecords");
  sync.lock(Shared);

  if (!records) return 0;

  return records->chartActiveRecords(chart);
}

void Table::rebuildIndex(Index *index, Transaction *transaction) {
  index->rebuildIndex(transaction);
  populateIndex(index, transaction);
}

void Table::validateBlobs(int optionMask) {
  Field *field;

  // See if there are any blobs

  for (field = fields; field; field = field->next)
    if (field->type == Asciiblob || field->type == Binaryblob) break;

  // If there are no blobs, we're wasting our time

  if (!field) return;

  Bitmap references;
  Record *record;
  int32 next;
  CycleLock cycleLock(database);

  for (next = 0; (record = fetchNext(next));) {
    next = record->recordNumber + 1;

    {
      for (Record *version = record; version;
           version = version->getPriorVersion())
        if (version->hasRecord())
          for (field = fields; field; field = field->next)
            if (field->type == Asciiblob || field->type == Binaryblob) {
              int id = version->getBlobId(field->id);

              if (id >= 0) references.set(id);
            }
    }

    record->release(REC_HISTORY);
  }

  Bitmap inventory;

  if (!blobSection) blobSection = dbb->findSection(blobSectionId);

  for (next = 0; (next = dbb->findNextRecord(blobSection, next, NULL)) >= 0;
       ++next) {
    inventory.set(next);

    if (!references.isSet(next)) {
      Log::debug("Orphan blob %d, table %s.%s, blob section %d\n", next,
                 schemaName, name, blobSectionId);

      if (optionMask & validateRepair)
        dbb->updateRecord(blobSection, next, (Stream *)NULL,
                          (TransactionState *)NULL, false);
    }
  }

  for (next = 0; (next = references.nextSet(next)) >= 0; ++next)
    if (!inventory.isSet(next)) {
      Log::debug("Lost blob %d, table %s.%s, section %d\n", next, schemaName,
                 name, blobSectionId);
      /***
      if (optionMask & validateRepair)
              xxx
      ***/
    }
}

void Table::collationChanged(Field *field) {
  FOR_INDEXES(index, this);
  if (index->isMember(field)) index->rebuild = true;
  END_FOR;
}

void Table::rebuildIndexes(Transaction *transaction, bool force) {
  FOR_INDEXES(index, this);
  if (index->rebuild || force) {
    index->rebuild = false;
    rebuildIndex(index, transaction);
  }
  END_FOR;
}

void Table::clearIndexesRebuild() {
  FOR_INDEXES(index, this);
  index->rebuild = false;
  END_FOR;
}

void Table::deleteRecord(RecordVersion *record, Transaction *transaction) {
  if (record->recordNumber >= 0)
    dbb->logRecord(dataSectionId, record->recordNumber, NULL, transaction);
}

void Table::bind(Table *table) {
  for (ForeignKey *key = foreignKeys; key; key = key->next)
    key->bindTable(table);
}

void Table::insertView(Transaction *transaction, int count, Field **fieldVector,
                       Value **values) {
  throw SQLEXCEPTION(COMPILE_ERROR,
                     "attempt to insert into non-updatable view %s.%s",
                     schemaName, name);
}

void Table::deleteRecord(int recordNumber) {
  dbb->logRecord(dataSectionId, recordNumber, NULL, 0);
}

void Table::refreshFields() {
  const char *sql =
      (database->fieldExtensions)
          ? "select field, fieldId, dataType, length, scale, flags, collationsequence, precision\
				from system.Fields where tableName=? and schema=?"
          : "select field, fieldId, dataType, length, scale, flags, collationsequence\
				from system.Fields where tableName=? and schema=?";

  PreparedStatement *statement = database->prepareStatement(sql);
  statement->setString(1, name);
  statement->setString(2, schemaName);
  ResultSet *set = statement->executeQuery();
  bool changed = false;

  while (set->next()) {
    const char *fieldName = set->getString(1);

    if (!findField(fieldName)) {
      changed = true;
      const char *collationName = set->getString(7);
      Collation *collation = CollationManager::getCollation(collationName);

      Field *field =
          NEW Field(this,
                    set->getInt(2),          // id
                    fieldName,               // name
                    (Type)(set->getInt(3)),  // type
                    set->getInt(4),          // length
                    ((database->fieldExtensions) ? set->getInt(7) : 0),
                    set->getInt(5),   // scale
                    set->getInt(6));  // flags
      addField(field);

      if (collation) field->setCollation(collation);

      if (nextFieldId <= field->id) nextFieldId = field->id + 1;
    }
  }

  if (changed) buildFieldVector();

  set->close();
  statement->close();
  statement = database->prepareStatement(
      "select max(version) from system.formats where tableId=?");
  statement->setInt(1, tableId);
  set = statement->executeQuery();

  if (set->next()) formatVersion = set->getInt(1);

  set->close();
  statement->close();
}

AsciiBlob *Table::getAsciiBlob(int recordId) {
  if (!blobSection) blobSection = dbb->findSection(blobSectionId);

  AsciiBlob *blob = NEW AsciiBlob(dbb, recordId, blobSection);

  if (recordId < 0) getIndirectBlob(recordId, blob);

  return blob;
}

BinaryBlob *Table::getBinaryBlob(int recordId) {
  if (!blobSection) blobSection = dbb->findSection(blobSectionId);

  BinaryBlob *blob = NEW BinaryBlob(dbb, recordId, blobSection);

  if (recordId < 0) getIndirectBlob(recordId, blob);

  return blob;
}

void Table::getIndirectBlob(int recordId, BlobReference *blob) {
  if (!blobSection) blobSection = dbb->findSection(blobSectionId);

  int recordNumber = (recordId == ZERO_REPOSITORY_PLACE) ? 0 : -recordId;
  Stream stream;
  dbb->fetchRecord(blobSection, recordNumber, &stream);
  blob->setReference(stream.totalLength, &stream);
  blob->setRepository(
      database->getRepository(schemaName, blob->repositoryName));
}

uint Table::insert(Transaction *transaction, Stream *stream) {
  database->preUpdate();
  RecordVersion *record = NULL;
  bool insertedIntoTree = false;
  bool addedToTransaction = false;
  int32 recordNumber = -1;

  if (!dataSection) findSections();

  try {
    // Get current format for record

    Format *fmt = format;

    if (!fmt) fmt = format = getFormat(formatVersion);

    record = allocRecordVersion(fmt, transaction, NULL);
    record->state = recInserting;
    record->setEncodedRecord(stream, false);
    recordNumber = record->recordNumber =
        dbb->insertStub(dataSection, transaction->transactionState);

    // Make insert/update atomic, then check for unique index duplicats
    // If insertStub says it is free, then there should never be
    // anything in the record tree for this record number.

    if (!insertIntoTree(record, NULL, recordNumber))
      FATAL("Table::insert(Stream) cannot InsertIntoTree");
    insertedIntoTree = true;

    transaction->addRecord(record);
    addedToTransaction = true;

    insertIndexes(transaction, record);
    record->state = recData;
    record->release(REC_HISTORY);
  } catch (...) {
    if (insertedIntoTree)
      if (!insertIntoTree(NULL, record, recordNumber))
        FATAL("Table::insert(Stream) cannot backout insertIntoTree");

    if (addedToTransaction) transaction->removeRecord(record);

    if (recordNumber >= 0) {
      dbb->updateRecord(dataSection, recordNumber, NULL,
                        transaction->transactionState, false);
      dataSection->expungeRecord(recordNumber);
      record->recordNumber = -1;
    }

    garbageCollect(record, NULL, transaction, true);

    if (record) {
      SET_RECORD_ACTIVE(record, false);
      record->queueForDelete();
    }

    throw;
  }

  return recordNumber;
}

void Table::update(Transaction *transaction, Record *orgRecord,
                   Stream *stream) {
  database->preUpdate();

  Record *candidate = fetch(orgRecord->recordNumber);

  if (!candidate) return;

  RECORD_HISTORY(candidate);

  checkAncestor(candidate, orgRecord);
  Record *oldRecord = candidate;

  if (candidate->getTransactionState() == transaction->transactionState) {
    if (candidate->state == recLock) oldRecord = oldRecord->getPriorVersion();
  } else
    oldRecord = candidate->fetchVersion(transaction);

  if (!oldRecord) {
    ASSERT(false);
    candidate->release(REC_HISTORY);

    return;
  }

  RecordVersion *record = NULL;
  bool updated = false;

  if (candidate->state == recLock &&
      candidate->getTransactionState() == transaction->transactionState) {
    if (candidate->getSavePointId() == transaction->curSavePointId) {
      record = (RecordVersion *)
          candidate;  // Use the lock record for the new version.
      oldRecord->addRef(REC_HISTORY);
    } else
      oldRecord = candidate;
  } else if (candidate != oldRecord) {
    oldRecord->addRef(REC_HISTORY);
    candidate->release(REC_HISTORY);
  }

  try {
    // Find current record format and create new record version

    Format *format = getFormat(formatVersion);

    if (record)
      record->format = format;
    else
      record = allocRecordVersion(format, transaction, oldRecord);

    record->setEncodedRecord(stream, false);

    // Fire pre-operation triggers

    // fireTriggers(transaction, PreUpdate, oldRecord, record);

    // Make sure no constraints are violated

    // checkNullable(record);

    // Checkin with any table attachments

    FOR_OBJECTS(TableAttachment *, attachment, &attachments)
    if (attachment->mask & PRE_UPDATE) attachment->preUpdate(this, record);
    END_FOR;

    // updateInversion(record, transaction);

    if (record->state == recLock)
      record->state = recData;
    else {
      validateAndInsert(transaction, record);
      transaction->addRecord(record);
    }

    updated = true;
    // Make insert/update atomic, then check for unique index duplicats
    updateIndexes(transaction, record, oldRecord);
    // fireTriggers(transaction, PostUpdate, oldRecord, record);

    // If this is a re-update in the same transaction and the same savepoint,
    // carefully remove the prior version.

    record->scavengeSavepoint(transaction, transaction->curSavePointId);
    record->release(REC_HISTORY);

    oldRecord->release(
        REC_HISTORY);  // This reference originated in this function.
  } catch (...) {
    if (updated) {
      transaction->removeRecord(record);

      if (!insertIntoTree(oldRecord, record, record->recordNumber))
        Log::debug("record backout failed after failed update\n");
    }

    garbageCollect(record, oldRecord, transaction, true);

    if (record) {
      if (record->getPriorVersion())
        record->getPriorVersion()->setSuperceded(false);

      if (record->state == recLock) record->deleteData();

      SET_RECORD_ACTIVE(record, false);
      oldRecord->release(REC_HISTORY);
      record->queueForDelete();
    }

    throw;
  }
}

void Table::rename(const char *newSchema, const char *newName) {
  try {
    for (const char **tbl = relatedTables; *tbl; ++tbl) {
      char sql[512];
      snprintf(sql, sizeof(sql),
               "update system.%s "
               "  set schema=?, tableName=? "
               "  where schema=? and tableName=?",
               *tbl);
      PreparedStatement *statement = database->prepareStatement(sql);
      statement->setString(1, newSchema);
      statement->setString(2, newName);
      statement->setString(3, schemaName);
      statement->setString(4, name);
      statement->executeUpdate();
      statement->close();
    }

    database->commitSystemTransaction();
    Index *primaryKey = getPrimaryKey();
    database->renameTable(this, newSchema, newName);

    renameIndexes(newName);

    if (primaryKey) primaryKey->rename(getPrimaryKeyName());
  } catch (...) {
    database->rollbackSystemTransaction();
    throw;
  }
}

int Table::storeBlob(Transaction *transaction, uint32 length,
                     const UCHAR *data) {
  if (!blobSection) blobSection = dbb->findSection(blobSectionId);

  int32 recordNumber =
      dbb->insertStub(blobSection, transaction->transactionState);
  Stream stream;
  stream.putSegment((int)length, (const char *)data, false);
  dbb->updateBlob(blobSection, recordNumber, &stream,
                  transaction->transactionState);

  return recordNumber;
}

void Table::getBlob(int recordNumber, Stream *stream) {
  if (!blobSection) blobSection = dbb->findSection(blobSectionId);

  dbb->fetchRecord(blobSection, recordNumber, stream);
}

void Table::expunge(Transaction *transaction) {
  if (transaction) transaction->hasUpdates = true;

  if (dataSectionId != Section::INVALID_SECTION_ID) {
    dbb->deleteSection(dataSectionId, TRANSACTION_ID(transaction));
    dataSectionId = Section::INVALID_SECTION_ID;
    dataSection = NULL;
  }

  if (blobSectionId != Section::INVALID_SECTION_ID) {
    dbb->deleteSection(blobSectionId, TRANSACTION_ID(transaction));
    blobSectionId = Section::INVALID_SECTION_ID;
    blobSection = NULL;
  }
}

JString Table::getPrimaryKeyName(void) {
  JString indexName;
  indexName.Format("%s..PRIMARY_KEY", (const char *)name);

  return indexName;
}

/**
@brief		Validate that this record can be inserted.
@details	Make sure this record can be inserted without conflict from
another pending version of the same record.
**/

void Table::validateAndInsert(Transaction *transaction, RecordVersion *record) {
  Sync syncTable(&syncObject, "Table::validateAndInsert");

  Record *prior = record->getPriorVersion();

  for (int n = 0; n < 10; ++n) {
    if (prior) {
      syncTable.lock(Exclusive);
      Record *current = fetch(record->recordNumber);

      if (current) {
        if (current == prior)
          current->release(REC_HISTORY);
        else {
          // The current record is not our prior. If it is committed, we have
          // an update conflict.  If not, wait on that trans and, if it is not
          // committed, try again.  (transState == NULL) means committed.

          TransactionState *transState = current->getTransactionState();
          if (!transState)
            throw SQLError(UPDATE_CONFLICT,
                           "update conflict in table %s.%s record %d",
                           schemaName, name, record->recordNumber);

          transState->addRef();
          current->release(REC_HISTORY);
          syncTable.unlock();

          if (transaction->waitForTransaction(transState)) {
            current = fetch(record->recordNumber);

            if (current == prior)
              current->release(REC_HISTORY);
            else {
              transaction->blockedBy = transState->transactionId;
              transState->release();
              throw SQLError(UPDATE_CONFLICT,
                             "update conflict in table %s.%s record %d",
                             schemaName, name, record->recordNumber);
            }
          }

          transState->release();
        }
      }
    }

    if (insertIntoTree(record, prior, record->recordNumber)) return;

    if (n >= 7)
      Log::debug("Table::validateAndInsert: things going badly (%d)\n", n);

    SET_RECORD_ACTIVE(record, false);
  }

  throw SQLError(UPDATE_CONFLICT,
                 "unexpected update conflict in table %s.%s record %d",
                 schemaName, name, record->recordNumber);
}

int Table::getFormatVersion() {
  Format *format = getFormat(formatVersion);

  return format->version;
}

bool Table::hasUncommittedRecords(Transaction *transaction) {
  return database->hasUncommittedRecords(this, transaction);
}

void Table::waitForWriteComplete() { database->waitForWriteComplete(this); }

void Table::unlockRecord(int recordNumber, int verbMark) {
  CycleLock cycleLock(database);
  Record *record = fetch(recordNumber);

  if (record) {
    if (record->state == recLock)
      unlockRecord((RecordVersion *)record, verbMark);

    record->release(REC_HISTORY);
  }
}

void Table::unlockRecord(RecordVersion *record, int verbMark) {
  if (record->state != recLock) return;

  // A lock record that has superceded=true is already unlocked

  if (record->isSuperceded()) return;

  // Only unlock records at the current savepoint

  if (record->savePointId < verbMark) return;

  Record *prior = record->getPriorVersion();
  if (insertIntoTree(prior, record, record->recordNumber))
    record->setSuperceded(true);
  else
    Log::debug("Table::unlockRecord: record lock not in record tree\n");
}

void Table::checkAncestor(Record *current, Record *oldRecord) {
  for (Record *record = current; record; record = record->getPriorVersion())
    if (record == oldRecord) return;

  current->printRecord("current record");
  oldRecord->printRecord("old record");
  Value value1;
  Value value2;
  current->getValue(0, &value1);
  oldRecord->getValue(0, &value2);
  ASSERT(false);
}

// Table::fetchForUpdate - Create a lock record if necessary and
// return the active record.
// Unlike the sister routine, fetchVersion, this function will release
// the refCount on source if nothing is returned.  But since this
// does not have a catch, no functions below it should throw an exception.

Record *Table::fetchForUpdate(Transaction *transaction, Record *source,
                              bool usingIndex) {
  //  Find the record that will be locked

  int recordNumber = source->recordNumber;

  // If we already have this locked or updated, get the active version

  if (source->getTransactionState() == transaction->transactionState) {
    if (source->state == recDeleted) {
      source->release(REC_HISTORY);

      return NULL;
    }

    if (source->state != recLock) return source;

    // The source record (base record) is a lockRecord.
    // There is only one lock record.  It is at the savepoint
    // where it was locked. The prior record is the real record.

    Record *prior = source->getPriorVersion();
    prior->addRef(REC_HISTORY);
    source->release(REC_HISTORY);

    return prior;
  }

  // The record number is not currently locked.

  for (;;) {
    // Try to avoid getting a lock if there is no way we will be updating this
    // record.

    if (!transaction->needToLock(source)) {
      source->release(REC_HISTORY);

      return NULL;
    }

    // We may need to lock the record

    State state = transaction->getRelativeState(source, WAIT_IF_ACTIVE);

    switch (state) {
      case CommittedInvisible:
        // CommittedInvisible only happens for consistent read.

        ASSERT(IS_CONSISTENT_READ(transaction->isolationLevel));
        source->release(REC_HISTORY);
        Log::debug(
            "Table::fetchForUpdate: Update Conflict: TransId=%d, "
            "RecordNumber=%d, Table %s.%s\n",
            transaction->transactionId, source->recordNumber, schemaName, name);
        throw SQLError(UPDATE_CONFLICT, "update conflict in table %s.%s",
                       schemaName, name);

      case CommittedVisible: {
        if (source->state == recDeleted) {
          source->release(REC_HISTORY);

          return NULL;
        }

        if (source->state == recChilled && !source->thaw()) {
          source->release(REC_HISTORY);

          return NULL;
        }

        // Lock the record

        if (dbb->debug & DEBUG_RECORD_LOCKS)
          Log::debug(
              "Table::fetchForUpdate: TransactionId=%d, isolationLevel=%d, "
              "recordNumber=%d\n",
              transaction->transactionId, transaction->isolationLevel,
              recordNumber);

        RecordVersion *lockRecord =
            allocRecordVersion(NULL, transaction, source);
        lockRecord->state = recLock;

        if (insertIntoTree(lockRecord, source, recordNumber)) {
          transaction->addRecord(lockRecord);
          lockRecord->release(REC_HISTORY);

          return source;
        }

        SET_RECORD_ACTIVE(lockRecord, false);
        lockRecord->release(REC_HISTORY);
      } break;

      case Deadlock:
        source->release(REC_HISTORY);
        throw SQLError(DEADLOCK, "Deadlock on table %s.%s, tid %d", schemaName,
                       name, transaction->transactionId);

      case WasActive:
      case RolledBack:
        break;  // need to re-fetch the base record

      default:
        source->release(REC_HISTORY);
        Log::debug("Table::fetchForUpdate: unexpected state %d\n", state);
        throw SQLError(RUNTIME_ERROR, "unexpected transaction state %d", state);
    }

    source->release(REC_HISTORY);

    source = fetch(recordNumber);

    if (source == NULL) return NULL;
  }
}

int64 Table::estimateCardinality(void) { return cardinality; }

void Table::optimize(Connection *connection) {
  uint64 count = 0;
  int recordNumber = 0;
  Transaction *transaction = connection->getTransaction();

  // Fetch every base record in this table. Count each
  // base record that has a version visible to this transaction.
  // Be sure to release the base record afterwards.

  for (Record *record; (record = fetchNext(recordNumber));) {
    recordNumber = record->recordNumber + 1;
    Record *recordVersion = record->fetchVersion(transaction);

    if (recordVersion) ++count;

    RECORD_HISTORY(record);
    record->release(REC_HISTORY);  // no need to keep it around
  }

  cardinality = count;

  // Disable index optimization until a more
  // efficient method is implemented using
  // the IndexWalker (Bug#36442)
#if 0
	FOR_INDEXES(index, this);
		index->optimize(count, connection);
	END_FOR;
#endif

  database->commitSystemTransaction();
}

void Table::clearAlter(void) {
  if (alterIsActive) {
    Sync sync(&syncAlter, "Table::clearAlter");
    sync.lock(Exclusive);
    alterIsActive = false;
  }
}

bool Table::setAlter(void) {
  Sync sync(&syncAlter, "Table::setAlter");
  sync.lock(Exclusive);

  if (alterIsActive) return false;

  alterIsActive = true;

  return true;
}

#undef new

// Allocate a RecordVersion object from the record cache.
// Use an non-thread-safe increment of recordPoolAllocCount.  It allows
// full concurrency by multiple threads but it may miss a check every
// now and then.  This keeps the code from doing this check every time.
// It is done about every 128 allocations from the record cache.

RecordVersion *Table::allocRecordVersion(Format *format,
                                         Transaction *transaction,
                                         Record *priorVersion) {
  for (int n = 1;; ++n) {
    try {
      return POOL_NEW(database->recordVersionPool)
          RecordVersion(this, format, transaction, priorVersion);
    }

    catch (SQLException &exception) {
      if (exception.getSqlcode() != OUT_OF_RECORD_MEMORY_ERROR ||
          n > OUT_OF_RECORD_MEMORY_RETRIES)
        throw;

      database->signalScavenger(true);

      // Give the scavenger thread a chance to release memory.
      // Increase the wait time per iteration.

      Thread *thread = Thread::getThread("Database::ticker");
      thread->sleep(n * SCAVENGE_WAIT_MS);
    }
  }

  return NULL;
}

// Allocate a Record object from the record cache.
// Use an non-thread-safe increment of recordPoolAllocCount.  It allows
// full concurrency by multiple threads but it may miss a check every
// now and then.  This keeps the code from doing this check every time.
// It is done about every 128 allocations from the record cache.

Record *Table::allocRecord(int recordNumber, Stream *stream) {
  for (int n = 1;; ++n) {
    try {
      return POOL_NEW(database->recordPool) Record(this, recordNumber, stream);
    }

    catch (SQLException &exception) {
      if (exception.getSqlcode() != OUT_OF_RECORD_MEMORY_ERROR ||
          n > OUT_OF_RECORD_MEMORY_RETRIES)
        throw;

      database->signalScavenger(true);

      // Give the scavenger thread a chance to release memory.
      // Increase the wait time per iteration.

      Thread *thread = Thread::getThread("Database::ticker");
      thread->sleep(n * SCAVENGE_WAIT_MS);
    }
  }

  return NULL;
}

Format *Table::getCurrentFormat(void) {
  return getFormat(formatVersion);
  ;
}

void Table::findSections(void) {
  ASSERT(dataSectionId != Section::INVALID_SECTION_ID &&
         blobSectionId != Section::INVALID_SECTION_ID);

  if (!dataSection) {
    dataSection = dbb->findSection(dataSectionId);
    dataSection->table = this;
  }
  ASSERT(dataSection->sectionId == dataSectionId);

  if (!blobSection) {
    blobSection = dbb->findSection(blobSectionId);
  }
  ASSERT(blobSection->sectionId == blobSectionId);
}

bool Table::validateUpdate(int32 recordNumber, TransId transactionId) {
  CycleLock cycleLock(database);

  if (deleting) return false;

  Record *record = treeFetch(recordNumber);
  Record *initial = record;

  while (record) {
    if (record->getTransactionId() == transactionId) {
      record->release(REC_HISTORY);

      return true;
    }

    TransactionState *transactionState = record->getTransactionState();

    // if (transactionState->getState() == Committed)
    if (transactionState->isCommitted()) {
      SET_RECORD_ACTIVE(record, false);
      record->release(REC_HISTORY);

      return false;
    }

    Record *next = record->getPriorVersion();

    if (!next) {
      Log::debug("Table::validateUpdate: bad record\n");
      initial->print();
      Record *newRecord = fetch(recordNumber);
      Log::debug("Table::validateUpdate: currentRecord\n");
      newRecord->print();
      ASSERT(false);
    }

    next->addRef();
    record->release(REC_HISTORY);
    record = next;
  }

  return true;
}

void Table::expungeRecord(int32 recordNumber) {
  dataSection->expungeRecord(recordNumber);
}

// Write out the current record chain to the backlog tablespace.

bool Table::backlogRecord(RecordVersion *record, Bitmap *backlogBitmap) {
  Sync sync(&syncObject, "Table::backlogRecord");
  sync.lock(Exclusive);

  if (!backloggedRecords) backloggedRecords = new SparseArray<int32, BL_SIZE>;

  int32 backlogId = backloggedRecords->get(record->recordNumber);
  bool inserted = false;

  // Update if already backlogged, else insert

  if (backlogId)
    database->backLog->update(backlogId, record);
  else {
    backlogId = database->backLog->save(record);
    backloggedRecords->set(record->recordNumber, backlogId);
    inserted = true;
  }

  // Now that the record is backlogged, replace it with a null
  // base record in the record tree

  if (insertIntoTree(NULL, record, record->recordNumber)) {
    if (backlogBitmap) backlogBitmap->set(backlogId);
    return true;
  }

  // If the tree insert failed, then there must be a new base
  // record and the old backlog record is no good.
  // Remove the record number from the sparse array and
  // remove the backlog record from the repository.

  backloggedRecords->set(record->recordNumber, 0);
  database->backLog->deleteRecord(backlogId);

  // Remove from caller's backlog bitmap

  if (backlogBitmap) backlogBitmap->clear(backlogId);

  return false;
}

void Table::deleteRecordBacklog(int32 recordNumber) {
  Sync sync(&syncObject, "Table::deleteRecordBacklog");
  sync.lock(Shared);
  int32 backlogId = backloggedRecords->get(recordNumber);

  if (backlogId) {
    sync.unlock();
    sync.lock(Exclusive);
    backloggedRecords->set(recordNumber, 0);
    database->backLog->deleteRecord(backlogId);
  }
}

SyncObject *Table::getSyncThaw(Record *record) {
  int lockNumber = record->recordNumber % SYNC_THAW_SIZE;
  return syncThaw + lockNumber;
}

SyncObject *Table::getSyncThaw(int recordNumber) {
  int lockNumber = recordNumber % SYNC_THAW_SIZE;
  return syncThaw + lockNumber;
}

static bool needUniqueCheck(Index *index, Record *record) {
  Record *oldRecord = record->getPriorVersion();
  return (INDEX_IS_UNIQUE(index->type) &&
          (!oldRecord || index->changed(record, oldRecord)));
}

void Table::queueForDelete(Record *record) {
  database->cycleManager->queueForDelete(record);
}

void Table::queueForDelete(Value **zombie) {
  database->cycleManager->queueForDelete(zombie);
}

void Table::queueForDelete(char *zombie) {
  database->cycleManager->queueForDelete(zombie);
}

}  // namespace Changjiang
