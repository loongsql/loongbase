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

#include <memory.h>
#include <string.h>
#include <stdio.h>
#include "Engine.h"
#include "StorageTableShare.h"
#include "StorageDatabase.h"
#include "StorageHandler.h"
#include "SyncObject.h"
#include "Sync.h"
#include "Sequence.h"
#include "Index.h"
#include "Table.h"
#include "Field.h"
#include "Interlock.h"
#include "CollationManager.h"
#include "MySQLCollation.h"
#include "Connection.h"
#include "PreparedStatement.h"
#include "ResultSet.h"
#include "SQLException.h"
#include "CmdGen.h"

namespace Changjiang {

static const char *CHANGJIANG_TEMPORARY = "/changjiang_temporary";
static const char *DB_ROOT = ".cts";

#if defined(_WIN32) && MYSQL_VERSION_ID < 0x50100
#define IS_SLASH(c) (c == '/' || c == '\\')
#else
#define IS_SLASH(c) (c == '/')
#endif

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

extern void changjiang_lock_init(void *lock);
extern void changjiang_lock_deinit(void *lock);

StorageIndexDesc::StorageIndexDesc() {
  id = 0;
  unique = 0;
  primaryKey = 0;
  numberSegments = 0;
  index = NULL;
  segmentRecordCounts = NULL;
  next = NULL;
  name[0] = '\0';
  rawName[0] = '\0';
};

StorageIndexDesc::StorageIndexDesc(const StorageIndexDesc *indexDesc) {
  if (indexDesc)
    *this = *indexDesc;
  else {
    id = 0;
    unique = 0;
    primaryKey = 0;
    numberSegments = 0;
    segmentRecordCounts = NULL;
    name[0] = '\0';
    rawName[0] = '\0';
  }

  index = NULL;
  next = NULL;
  prev = NULL;
};

StorageIndexDesc::~StorageIndexDesc(void) {}

bool StorageIndexDesc::operator==(const StorageIndexDesc &indexDesc) const {
  if (!(id == indexDesc.id && unique == indexDesc.unique &&
        primaryKey == indexDesc.primaryKey &&
        numberSegments == indexDesc.numberSegments))
    return false;

  if (strcmp(indexDesc.name, name) != 0) return false;

  /***
  const char *q = indexDesc.name;
  const char *p = name;

  for (; (*p && *q); ++p, ++q)
          if (*p != *q)
                  return false;
  ***/

  return true;
}

bool StorageIndexDesc::operator!=(const StorageIndexDesc &indexDesc) const {
  return !(*this == indexDesc);
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

StorageTableShare::StorageTableShare(StorageHandler *handler, const char *path,
                                     const char *tableSpaceName, int lockSize,
                                     bool tempTbl) {
  storageHandler = handler;
  storageDatabase = NULL;
  impure = new UCHAR[lockSize];
  changjiang_lock_init(impure);

  initialized = false;
  table = NULL;
  format = NULL;
  syncObject = new SyncObject;
  syncObject->setName("StorageTableShare::syncObject");
  syncIndexMap = new SyncObject;
  syncIndexMap->setName("StorageTableShare::syncIndexMap");
  sequence = NULL;
  tempTable = tempTbl;
  setPath(path);
  indexes = NULL;

  if (tempTable)
    tableSpace = TEMPORARY_TABLESPACE;
  else if (tableSpaceName && tableSpaceName[0])
    tableSpace = JString::upcase(tableSpaceName);
  else
    tableSpace = schemaName;
}

StorageTableShare::~StorageTableShare(void) {
  delete syncObject;
  delete syncIndexMap;

  changjiang_lock_deinit(impure);
  delete[] impure;

  if (storageDatabase) storageDatabase->release();

  for (StorageIndexDesc *indexDesc; (indexDesc = indexes);) {
    indexes = indexDesc->next;
    delete indexDesc;
  }
}

void StorageTableShare::lock(bool exclusiveLock) {
  syncObject->lock(NULL, (exclusiveLock) ? Exclusive : Shared);
}

void StorageTableShare::unlock(void) { syncObject->unlock(); }

void StorageTableShare::lockIndexes(bool exclusiveLock) {
  syncIndexMap->lock(NULL, (exclusiveLock) ? Exclusive : Shared);
}

void StorageTableShare::unlockIndexes(void) { syncIndexMap->unlock(); }

int StorageTableShare::open(void) {
  if (!table) {
    table = storageDatabase->findTable(name, schemaName);

    if (table) format = table->getCurrentFormat();

    sequence = storageDatabase->findSequence(name, schemaName);
  }

  if (!table) return StorageErrorTableNotFound;

  return 0;
}

int StorageTableShare::create(StorageConnection *storageConnection,
                              const char *sql, int64 autoIncrementValue) {
  try {
    table = storageDatabase->createTable(storageConnection, name, schemaName,
                                         sql, autoIncrementValue);
  } catch (SQLException &exception) {
    int sqlcode = exception.getSqlcode();
    switch (sqlcode) {
      case TABLESPACE_NOT_EXIST_ERROR:
        return StorageErrorTableSpaceNotExist;
      default:
        return StorageErrorTableExits;
    }
  }
  if (!table) return StorageErrorTableExits;

  format = table->getCurrentFormat();

  if (autoIncrementValue)
    sequence = storageDatabase->findSequence(name, schemaName);

  return 0;
}

int StorageTableShare::upgrade(StorageConnection *storageConnection,
                               const char *sql, int64 autoIncrementValue) {
  if (!(table = storageDatabase->upgradeTable(
            storageConnection, name, schemaName, sql, autoIncrementValue)))
    return StorageErrorTableExits;

  format = table->getCurrentFormat();

  if (autoIncrementValue)
    sequence = storageDatabase->findSequence(name, schemaName);

  return 0;
}

int StorageTableShare::deleteTable(StorageConnection *storageConnection) {
  int res = storageDatabase->deleteTable(storageConnection, this);

  if (res == 0 || res == StorageErrorTableNotFound) {
    unRegisterTable();

    storageHandler->removeTable(this);

    delete this;
  }

  return res;
}

int StorageTableShare::truncateTable(StorageConnection *storageConnection) {
  int res = storageDatabase->truncateTable(storageConnection, this);

  sequence = storageDatabase->findSequence(name, schemaName);

  return res;
}

const char *StorageTableShare::cleanupFieldName(const char *name, char *buffer,
                                                int bufferLength,
                                                bool doubleQuotes) {
  char *q = buffer;
  char *end = buffer + bufferLength - 1;
  const char *p = name;
  bool quotes = false;

  for (; *p && q < end; ++p) {
    if (*p == '"') {
      if (doubleQuotes) {
        *q++ = UPPER(*p);
      }

      quotes = !quotes;
    }

    if (quotes)
      *q++ = *p;
    else
      *q++ = UPPER(*p);
  }

  *q = 0;

  return buffer;
}

const char *StorageTableShare::cleanupTableName(const char *name, char *buffer,
                                                int bufferLength, char *schema,
                                                int schemaLength) {
  char *q = buffer;
  const char *p = name;
  // Parse new type path:
  // Old format: /tmp/#sql42267_8_7
  // New format: /data/ldy/soft/loongbase/tmp/30007/#sql42267_8_7
  char prev_schema[256] = {0};
  bool is_set_schema = false;

  while (*p == '.') ++p;

  for (; *p; ++p)
    if (*p == '/' || *p == '\\') {
      *q = 0;
      if (is_set_schema) {
        strcpy(prev_schema, schema);
      }
      strcpy(schema, buffer);
      q = buffer;
      is_set_schema = true;
    } else
      *q++ = *p;  // UPPER(*p);

  *q = 0;

  if ((q = strchr(buffer, '.'))) *q = 0;

  // Fix temptable
  if (strcmp(prev_schema, "tmp") == 0) {
    strcpy(schema, prev_schema);
  }

  return buffer;
}

char *StorageTableShare::createIndexName(const char *rawName, char *indexName,
                                         bool primary) {
  if (primary)
    strcpy(indexName, table->getPrimaryKeyName().getString());
  else {
    char nameBuffer[indexNameSize];
    cleanupFieldName(rawName, nameBuffer, sizeof(nameBuffer), true);
    sprintf(indexName, "%s$%s", name.getString(), nameBuffer);
  }

  return indexName;
}

int StorageTableShare::createIndex(StorageConnection *storageConnection,
                                   StorageIndexDesc *indexDesc,
                                   const char *sql) {
  if (!table) open();

  // Lock out other clients before locking the table

  Sync syncIndex(syncIndexMap, "StorageTableShare::createIndex(1)");
  syncIndex.lock(Exclusive);

  Sync syncObj(syncObject, "StorageTableShare::createIndex(2)");
  syncObj.lock(Exclusive);

  int ret = storageDatabase->createIndex(storageConnection, table, sql);

  if (!ret) ret = setIndex(indexDesc);

  return ret;
}

void StorageTableShare::addIndex(StorageIndexDesc *indexDesc) {
  if (!getIndex(indexDesc->id)) {
    if (indexes) {
      indexDesc->next = indexes;
      indexDesc->prev = NULL;
      indexes->prev = indexDesc;
    }

    indexes = indexDesc;
  }
}

void StorageTableShare::deleteIndex(int indexId) {
  for (StorageIndexDesc *indexDesc = indexes; indexDesc;
       indexDesc = indexDesc->next)
    if (indexDesc->id == indexId) {
      if (indexDesc->prev)
        indexDesc->prev->next = indexDesc->next;
      else
        indexes = indexDesc->next;

      if (indexDesc->next) indexDesc->next->prev = indexDesc->prev;

      delete indexDesc;
      break;
    }
}

int StorageTableShare::dropIndex(StorageConnection *storageConnection,
                                 StorageIndexDesc *indexDesc, const char *sql,
                                 bool online) {
  if (!table) open();

  // Lock out other clients before locking the table

  Sync syncIndex(syncIndexMap, "StorageTableShare::dropIndex(1)");
  syncIndex.lock(Exclusive);

  Sync syncObj(syncObject, "StorageTableShare::dropIndex(2)");
  syncObj.lock(Exclusive);

  int ret = storageDatabase->dropIndex(storageConnection, table, sql);

  // If index not found during online drop index, do not return an error

  if (ret == StorageErrorNoIndex && online) ret = 0;

  // Remove index description from index mapping

  deleteIndex(indexDesc->id);

  return ret;
}

bool StorageTableShare::validateIndex(int indexId,
                                      StorageIndexDesc *indexTarget) {
  StorageIndexDesc *indexDesc = getIndex(indexId);

  if (!indexDesc || *indexDesc != *indexTarget) return false;

  Index *index = table->findIndex(indexDesc->name);

  if (!index) return false;

  if (index != indexDesc->index) return false;

  if (indexTarget->primaryKey && index->type != PrimaryKey) return false;

  if (index->recordsPerSegment != indexDesc->segmentRecordCounts) return false;

  return true;
}

void StorageTableShare::deleteIndexes() {
  for (StorageIndexDesc *indexDesc; (indexDesc = indexes);) {
    indexes = indexDesc->next;
    delete indexDesc;
  }
}

int StorageTableShare::numberIndexes() {
  int indexCount = 0;

  for (StorageIndexDesc *indexDesc = indexes; indexDesc;
       indexDesc = indexDesc->next, indexCount++)
    ;

  return indexCount;
}

int StorageTableShare::renameTable(StorageConnection *storageConnection,
                                   const char *newName) {
  char tableName[256];
  char schemaName[256];
  cleanupTableName(newName, tableName, sizeof(tableName), schemaName,
                   sizeof(schemaName));
  int ret = storageDatabase->renameTable(storageConnection, table,
                                         JString::upcase(tableName),
                                         JString::upcase(schemaName));

  if (ret) return ret;

  unRegisterTable();
  storageHandler->removeTable(this);
  setPath(newName);
  registerTable();
  storageHandler->addTable(this);

  return ret;
}

int StorageTableShare::setIndex(const StorageIndexDesc *indexInfo) {
  int ret = 0;

  if (!getIndex(indexInfo->id)) {
    // Find the corresponding Changjiang index else fail

    Index *index;

    if (indexInfo->primaryKey)
      index = table->primaryKey;
    else
      index = table->findIndex(indexInfo->name);

    if (index) {
      StorageIndexDesc *indexDesc = new StorageIndexDesc(indexInfo);
      indexDesc->index = index;
      indexDesc->segmentRecordCounts = indexDesc->index->recordsPerSegment;
      addIndex(indexDesc);
    } else
      ret = StorageErrorNoIndex;
  }

  return ret;
}

StorageIndexDesc *StorageTableShare::getIndex(int indexId) {
  if (!indexes) return NULL;

  for (StorageIndexDesc *indexDesc = indexes; indexDesc;
       indexDesc = indexDesc->next)
    if (indexDesc->id == indexId) {
      ASSERT(indexDesc->index != NULL);
      return indexDesc;
    }

  return NULL;
}

StorageIndexDesc *StorageTableShare::getIndex(int indexId,
                                              StorageIndexDesc *indexDesc) {
  if (!indexes) return NULL;

  Sync sync(syncIndexMap, "StorageTableShare::getIndex");
  sync.lock(Shared);

  StorageIndexDesc *index = getIndex(indexId);

  if (index) *indexDesc = *index;

  return index;
}

int StorageTableShare::getIndexId(const char *schemaName,
                                  const char *indexName) {
  if (!indexes) return -1;

  for (StorageIndexDesc *indexDesc = indexes; indexDesc;
       indexDesc = indexDesc->next) {
    Index *index = indexDesc->index;

    if (index)
      if (strcmp(index->getIndexName(), indexName) == 0 &&
          strcmp(index->getSchemaName(), schemaName) == 0)
        return indexDesc->id;
  }

  return -1;
}

int StorageTableShare::haveIndexes(int indexCount) {
  if (!indexes) return false;

  int n = 0;
  for (StorageIndexDesc *indexDesc = indexes; indexDesc;
       indexDesc = indexDesc->next, n++) {
    if (!indexDesc->index) return false;
  }

  return (n == indexCount);
}

INT64 StorageTableShare::getSequenceValue(int delta) {
  if (!sequence) return 0;

  return sequence->update(delta, NULL);
}

int StorageTableShare::setSequenceValue(INT64 value) {
  if (!sequence) return StorageErrorNoSequence;

  Sync sync(syncObject, "StorageTableShare::setSequenceValue");
  sync.lock(Exclusive);
  INT64 current = sequence->update(0, NULL);

  if (value > current) sequence->update(value - current, NULL);

  return 0;
}

void StorageTableShare::setTablePath(const char *path, bool tmp) {
  if (pathName.IsEmpty()) pathName = path;

  tempTable = tmp;
}

void StorageTableShare::registerCollation(const char *collationName,
                                          void *arg) {
  JString name = JString::upcase(collationName);
  Collation *collation = CollationManager::findCollation(name);

  if (collation) {
    collation->release();

    return;
  }

  collation = new MySQLCollation(name, arg);
  CollationManager::addCollation(collation);
}

void StorageTableShare::load(void) {
  Sync sync(&storageHandler->dictionarySyncObject, "StorageTableShare::load");
  sync.lock(Exclusive);
  Connection *connection = storageHandler->getDictionaryConnection();
  if (!connection) return;
  PreparedStatement *statement = connection->prepareStatement(
      "select "
      "given_schema_name,given_table_name,effective_schema_name,effective_"
      "table_name,tablespace_name "
      "from changjiang.tables where pathname=?");
  statement->setString(1, pathName);
  ResultSet *resultSet = statement->executeQuery();

  if (resultSet->next()) {
    int n = 1;
    givenSchema = resultSet->getString(n++);
    givenName = resultSet->getString(n++);
    schemaName = resultSet->getString(n++);
    name = resultSet->getString(n++);
    tableSpace = resultSet->getString(n++);
  }

  resultSet->close();
  statement->close();
  connection->commit();
}

void StorageTableShare::registerTable(void) {
  Connection *connection = NULL;
  PreparedStatement *statement = NULL;

  try {
    Sync sync(&storageHandler->dictionarySyncObject,
              "StorageTableShare::registerTable");
    sync.lock(Exclusive);
    connection = storageHandler->getDictionaryConnection();
    statement = connection->prepareStatement(
        "replace changjiang.tables "
        "(given_schema_name,given_table_name,effective_schema_name,effective_"
        "table_name,tablespace_name, pathname)"
        " values (?,?,?,?,?,?)");
    int n = 1;
    statement->setString(n++, givenSchema);
    statement->setString(n++, givenName);
    statement->setString(n++, schemaName);
    statement->setString(n++, name);
    statement->setString(n++, tableSpace);
    statement->setString(n++, pathName);
    statement->executeUpdate();
    statement->close();
    connection->commit();
  } catch (SQLException &) {
    if (statement) statement->close();

    if (connection) connection->commit();
  }
}

void StorageTableShare::unRegisterTable(void) {
  Sync sync(&storageHandler->dictionarySyncObject,
            "StorageTableShare::unRegisterTable");
  sync.lock(Exclusive);
  Connection *connection = storageHandler->getDictionaryConnection();
  PreparedStatement *statement = connection->prepareStatement(
      "delete from changjiang.tables where pathname=?");
  statement->setString(1, pathName);
  statement->executeUpdate();
  statement->close();
  connection->commit();
}

void StorageTableShare::getDefaultPath(char *buffer) {
  const char *slash = NULL;
  const char *p;

  for (p = pathName; *p; p++)
    if (IS_SLASH(*p)) slash = p;

  if (!slash) slash = p;

  IPTR len = slash - pathName + 1;

  if (tempTable) len += sizeof(CHANGJIANG_TEMPORARY);

  char *q = buffer;

  for (p = pathName; p < slash;) {
    char c = *p++;
    *q++ = (IS_SLASH(c)) ? '/' : c;
  }

  if (tempTable)
    for (p = CHANGJIANG_TEMPORARY; *p;) *q++ = *p++;

  strcpy(q, DB_ROOT);
}

void StorageTableShare::setPath(const char *path) {
  pathName = path;
  char tableName[256];
  char schema[256];
  cleanupTableName(path, tableName, sizeof(tableName), schema, sizeof(schema));
  givenName = tableName;
  givenSchema = schema;
  tableSpace = JString::upcase(givenSchema);
  name = JString::upcase(tableName);
  schemaName = JString::upcase(schema);
}

void StorageTableShare::findDatabase(void) {
  load();
  const char *dbName =
      (tableSpace.IsEmpty()) ? MASTER_NAME : (const char *)tableSpace;
  storageDatabase = storageHandler->findDatabase(dbName);
}

const char *StorageTableShare::getDefaultRoot(void) { return DB_ROOT; }

void StorageTableShare::setDatabase(StorageDatabase *db) {
  if (storageDatabase) storageDatabase->release();

  if ((storageDatabase = db)) storageDatabase->addRef();
}

uint64 StorageTableShare::estimateCardinality(void) {
  return table->estimateCardinality();
}

bool StorageTableShare::tableExists(void) {
  JString path = lookupPathName();

  return !path.IsEmpty();
}

JString StorageTableShare::lookupPathName(void) {
  Sync sync(&storageHandler->dictionarySyncObject,
            "StorageTableShare::lookupPathName");
  sync.lock(Exclusive);
  Connection *connection = storageHandler->getDictionaryConnection();
  PreparedStatement *statement = connection->prepareStatement(
      "select pathname from changjiang.tables where effective_schema_name=? "
      "and effective_table_name=?");
  int n = 1;
  statement->setString(n++, schemaName);
  statement->setString(n++, name);
  ResultSet *resultSet = statement->executeQuery();
  JString path;

  if (resultSet->next()) path = resultSet->getString(1);

  statement->close();
  connection->commit();

  return path;
}

int StorageTableShare::getFieldId(const char *fieldName) {
  Field *field = table->findField(fieldName);

  if (!field) return -1;

  return field->id;
}

}  // namespace Changjiang
