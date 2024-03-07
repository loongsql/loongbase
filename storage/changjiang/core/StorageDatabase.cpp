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

#include <string.h>
#include <stdio.h>
#include <memory.h>
#include "Engine.h"
#include "StorageDatabase.h"
#include "StorageConnection.h"
#include "SyncObject.h"
#include "Sync.h"
#include "SQLError.h"
#include "Threads.h"
#include "StorageHandler.h"
#include "StorageTable.h"
#include "StorageTableShare.h"
#include "TableSpaceManager.h"
#include "Sync.h"
#include "Threads.h"
#include "Configuration.h"
#include "Connection.h"
#include "Database.h"
#include "Table.h"
#include "Field.h"
#include "User.h"
#include "RoleModel.h"
#include "Value.h"
#include "RecordVersion.h"
#include "Transaction.h"
#include "Statement.h"
#include "Bitmap.h"
#include "PStatement.h"
#include "RSet.h"
#include "Sequence.h"
#include "StorageConnection.h"
#include "MySqlEnums.h"
#include "ScaledBinary.h"
#include "Dbb.h"
#include "CmdGen.h"
#include "IndexWalker.h"
#include "CycleLock.h"
//#include "SyncTest.h"

namespace Changjiang {

#define ACCOUNT "mysql"
#define PASSWORD "mysql"

static Threads *threads;
static Configuration *configuration;

static const char *ddl[] = {"upgrade sequence mystorage.indexes",

                            NULL};

#ifdef TRACE_TRANSACTIONS
static const char *traceTable =
    "upgrade table changjiang.transactions ("
    "     transaction_id int not null primary key,"
    "     committed int,"
    "     blocked_by int,"
    "     statements clob)";
#endif

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

StorageDatabase::StorageDatabase(StorageHandler *handler, const char *dbName,
                                 const char *path) {
  name = dbName;
  filename = path;
  storageHandler = handler;
  masterConnection = NULL;
  user = NULL;
  lookupIndexAlias = NULL;
  useCount = 1;
  insertTrace = NULL;

  if (!threads) {
    threads = new Threads(NULL);
    // SyncTest syncTest;
    // syncTest.test();
  }
  syncObject.setName("StorageDatabase::syncObject");
  syncTrace.setName("StorageDatabase::syncTrace");
}

StorageDatabase::~StorageDatabase(void) {
  if (lookupIndexAlias) lookupIndexAlias->release();

  if (masterConnection) masterConnection->release();

  if (user) user->release();
}

Connection *StorageDatabase::getConnection() {
  if (!configuration) configuration = new Configuration(NULL);

  return new Connection(configuration);
}

Connection *StorageDatabase::getOpenConnection(void) {
  try {
    if (!masterConnection) masterConnection = getConnection();

    if (!masterConnection->database) {
      masterConnection->openDatabase(name, filename, ACCOUNT, PASSWORD, NULL,
                                     threads);
      clearTransactions();
    }
  } catch (...) {
    if (masterConnection) {
      masterConnection->close();
      masterConnection = NULL;
    }

    throw;
  }

  return masterConnection->clone();
}

Connection *StorageDatabase::createDatabase(void) {
  try {
    masterConnection = getConnection();
#ifndef CHANGJIANGDB
    IO::createPath(filename);
#endif
    masterConnection->createDatabase(name, filename, ACCOUNT, PASSWORD,
                                     threads);
    Statement *statement = masterConnection->createStatement();

    for (const char **sql = ddl; *sql; ++sql) statement->execute(*sql);

    statement->release();
  } catch (...) {
    if (masterConnection) {
      masterConnection->close();
      masterConnection = NULL;
    }

    throw;
  }

  return masterConnection->clone();
}

Table *StorageDatabase::createTable(StorageConnection *storageConnection,
                                    const char *tableName,
                                    const char *schemaName, const char *sql,
                                    int64 autoIncrementValue) {
  Database *database = masterConnection->database;

  if (!user)
    if ((user = database->findUser(ACCOUNT))) user->addRef();

  Statement *statement = masterConnection->createStatement();

  try {
    Table *table = database->findTable(schemaName, tableName);

    if (table) database->dropTable(table, masterConnection->getTransaction());

    statement->execute(sql);

    if (autoIncrementValue) {
      char buffer[1024];
      snprintf(buffer, sizeof(buffer),
               "create sequence  %s.\"%s\" start with " I64FORMAT, schemaName,
               tableName, autoIncrementValue - 1);
      statement->execute(buffer);
    }

    statement->release();
  } catch (SQLException &exception) {
    statement->release();
    storageConnection->setErrorText(&exception);

    throw;
  }

  return findTable(tableName, schemaName);
}

Table *StorageDatabase::upgradeTable(StorageConnection *storageConnection,
                                     const char *tableName,
                                     const char *schemaName, const char *sql,
                                     int64 autoIncrementValue) {
  Database *database = masterConnection->database;

  if (!user)
    if ((user = database->findUser(ACCOUNT))) user->addRef();

  Statement *statement = masterConnection->createStatement();

  try {
    // Table *table = database->findTable(schemaName, tableName);
    statement->execute(sql);

    if (autoIncrementValue) {
      char buffer[1024];
      snprintf(buffer, sizeof(buffer),
               "create sequence  %s.\"%s\" start with " I64FORMAT, schemaName,
               tableName, autoIncrementValue - 1);
      statement->execute(buffer);
    }

    statement->release();
  } catch (SQLException &exception) {
    statement->release();
    storageConnection->setErrorText(&exception);

    return NULL;
  }

  return findTable(tableName, schemaName);
}

Table *StorageDatabase::findTable(const char *tableName,
                                  const char *schemaName) {
  return masterConnection->database->findTable(schemaName, tableName);
}

int StorageDatabase::insert(Connection *connection, Table *table,
                            Stream *stream) {
  CycleLock cycleLock(connection->database);

  return table->insert(connection->getTransaction(), stream);
}

int StorageDatabase::nextRow(StorageTable *storageTable, int recordNumber,
                             bool lockForUpdate) {
  StorageConnection *storageConnection = storageTable->storageConnection;
  Connection *connection = storageConnection->connection;
  Table *table = storageTable->share->table;
  Transaction *transaction = connection->getTransaction();
  Record *candidate = NULL;
  Record *record = NULL;
  CycleLock cycleLock(connection->database);

  try {
    for (;;) {
      candidate = table->fetchNext(recordNumber);

      if (!candidate) return StorageErrorRecordNotFound;

      RECORD_HISTORY(candidate);

      record = (lockForUpdate)
                   ? table->fetchForUpdate(transaction, candidate, false)
                   : candidate->fetchVersion(transaction);

      if (!record) {
        if (!lockForUpdate) candidate->release(REC_HISTORY);

        recordNumber = candidate->recordNumber + 1;

        continue;
      }

      if (!lockForUpdate && candidate != record) {
        record->addRef(REC_HISTORY);
        candidate->release(REC_HISTORY);
      }

      recordNumber = record->recordNumber;
      RECORD_HISTORY(record);
      storageTable->setRecord(record, lockForUpdate);

      return recordNumber;
    }
  } catch (SQLException &exception) {
    if (record && record != candidate) record->release(REC_HISTORY);

    if (candidate && !lockForUpdate) candidate->release(REC_HISTORY);

    int sqlcode = storageConnection->setErrorText(&exception);

    switch (sqlcode) {
      case UPDATE_CONFLICT:
        return StorageErrorUpdateConflict;

      case OUT_OF_MEMORY_ERROR:
        return StorageErrorOutOfMemory;

      case OUT_OF_RECORD_MEMORY_ERROR:
        return StorageErrorOutOfRecordMemory;

      case DEADLOCK:
        return StorageErrorDeadlock;

      case LOCK_TIMEOUT:
        return StorageErrorLockTimeout;
    }

    return StorageErrorRecordNotFound;
  }
}

int StorageDatabase::fetch(StorageConnection *storageConnection,
                           StorageTable *storageTable, int recordNumber,
                           bool lockForUpdate) {
  Table *table = storageTable->share->table;
  Connection *connection = storageConnection->connection;
  Transaction *transaction = connection->getTransaction();
  Record *candidate = NULL;
  ;
  CycleLock cycleLock(table->database);

  try {
    candidate = table->fetch(recordNumber);

    if (!candidate) return StorageErrorRecordNotFound;

    RECORD_HISTORY(candidate);

    Record *record = (lockForUpdate)
                         ? table->fetchForUpdate(transaction, candidate, false)
                         : candidate->fetchVersion(transaction);

    if (!record) {
      if (!lockForUpdate) candidate->release(REC_HISTORY);

      return StorageErrorRecordNotFound;
    }

    if (!lockForUpdate && record != candidate) {
      record->addRef(REC_HISTORY);
      candidate->release(REC_HISTORY);
    }

    RECORD_HISTORY(record);
    storageTable->setRecord(record, lockForUpdate);

    return 0;
  } catch (SQLException &exception) {
    if (candidate && !lockForUpdate) candidate->release(REC_HISTORY);

    int sqlcode = storageConnection->setErrorText(&exception);

    switch (sqlcode) {
      case UPDATE_CONFLICT:
        return StorageErrorUpdateConflict;
      case OUT_OF_MEMORY_ERROR:
        return StorageErrorOutOfMemory;
      case OUT_OF_RECORD_MEMORY_ERROR:
        return StorageErrorOutOfRecordMemory;
      case DEADLOCK:
        return StorageErrorDeadlock;
      case LOCK_TIMEOUT:
        return StorageErrorLockTimeout;
    }

    return StorageErrorRecordNotFound;
  }
}

int StorageDatabase::nextIndexed(StorageTable *storageTable, void *recordBitmap,
                                 int recordNumber, bool lockForUpdate) {
  if (!recordBitmap) return StorageErrorRecordNotFound;

  StorageConnection *storageConnection = storageTable->storageConnection;
  Connection *connection = storageConnection->connection;
  Table *table = storageTable->share->table;
  Transaction *transaction = connection->getTransaction();
  Record *candidate = NULL;
  CycleLock cycleLock(connection->database);

  try {
    Bitmap *bitmap = (Bitmap *)recordBitmap;

    for (;;) {
      recordNumber = bitmap->nextSet(recordNumber);

      if (recordNumber < 0) return StorageErrorRecordNotFound;

      candidate = table->fetch(recordNumber);
      RECORD_HISTORY(candidate);
      ++recordNumber;

      if (candidate) {
        Record *record = (lockForUpdate) ? table->fetchForUpdate(
                                               transaction, candidate, true)
                                         : candidate->fetchVersion(transaction);

        if (record) {
          recordNumber = record->recordNumber;

          if (!lockForUpdate && candidate != record) {
            record->addRef(REC_HISTORY);
            candidate->release(REC_HISTORY);
          }

          RECORD_HISTORY(record);
          storageTable->setRecord(record, lockForUpdate);

          return recordNumber;
        }

        if (!lockForUpdate) candidate->release(REC_HISTORY);
      }
    }
  } catch (SQLException &exception) {
    if (candidate && !lockForUpdate) candidate->release(REC_HISTORY);

    storageConnection->setErrorText(&exception);
    int errorCode = exception.getSqlcode();

    switch (errorCode) {
      case UPDATE_CONFLICT:
        return StorageErrorUpdateConflict;

      case OUT_OF_MEMORY_ERROR:
        return StorageErrorOutOfMemory;

      case OUT_OF_RECORD_MEMORY_ERROR:
        return StorageErrorOutOfRecordMemory;

      case DEADLOCK:
        return StorageErrorDeadlock;

      case LOCK_TIMEOUT:
        return StorageErrorLockTimeout;
    }

    return StorageErrorRecordNotFound;
  }
}

int StorageDatabase::nextIndexed(StorageTable *storageTable,
                                 IndexWalker *indexWalker, bool lockForUpdate) {
  CycleLock cycleLock(storageTable->share->table->database);

  try {
    Record *record = indexWalker->getNext(lockForUpdate);

    if (!record) return StorageErrorRecordNotFound;

    RECORD_HISTORY(record);
    storageTable->setRecord(record, lockForUpdate);
    return record->recordNumber;
  } catch (SQLException &exception) {
    StorageConnection *storageConnection = storageTable->storageConnection;
    storageConnection->setErrorText(&exception);
    int errorCode = exception.getSqlcode();

    switch (errorCode) {
      case UPDATE_CONFLICT:
        return StorageErrorUpdateConflict;

      case OUT_OF_MEMORY_ERROR:
        return StorageErrorOutOfMemory;

      case OUT_OF_RECORD_MEMORY_ERROR:
        return StorageErrorOutOfRecordMemory;

      case DEADLOCK:
        return StorageErrorDeadlock;

      case LOCK_TIMEOUT:
        return StorageErrorLockTimeout;

      default:
        ASSERT(false);
    }
  }
}

int StorageDatabase::savepointSet(Connection *connection) {
  Transaction *transaction = connection->getTransaction();

  return transaction->createSavepoint();
}

int StorageDatabase::savepointRollback(Connection *connection, int savePoint) {
  CycleLock cycleLock(connection->database);

  Transaction *transaction = connection->getTransaction();
  transaction->rollbackSavepoint(savePoint);

  return 0;
}

int StorageDatabase::savepointRelease(Connection *connection, int savePoint) {
  CycleLock cycleLock(connection->database);

  Transaction *transaction = connection->getTransaction();
  transaction->releaseSavepoint(savePoint);

  return 0;
}

int StorageDatabase::deleteTable(StorageConnection *storageConnection,
                                 StorageTableShare *tableShare) {
  const char *schemaName = tableShare->schemaName;
  const char *tableName = tableShare->name;
  Connection *connection = storageConnection->connection;
  CmdGen gen;
  gen.gen("drop table %s.\"%s\"", schemaName, tableName);
  Statement *statement = connection->createStatement();

  try {
    statement->execute(gen.getString());
  } catch (SQLException &exception) {
    int errorCode = exception.getSqlcode();
    storageConnection->setErrorText(&exception);

    switch (errorCode) {
      case NO_SUCH_TABLE:
        return StorageErrorTableNotFound;

      case UNCOMMITTED_UPDATES:
        return StorageErrorUncommittedUpdates;
    }

    return 200 - errorCode;
  }

  // Drop sequence, if any.  If none, this will throw an exception.  Ignore it

  int res = 0;

  if (connection->findSequence(schemaName, tableName)) try {
      gen.reset();
      gen.gen("drop sequence %s.\"%s\"", schemaName, tableName);
      statement->execute(gen.getString());
    } catch (SQLException &exception) {
      storageConnection->setErrorText(&exception);
      res = 200 - exception.getSqlcode();
    }

  statement->release();

  return res;
}

int StorageDatabase::truncateTable(StorageConnection *storageConnection,
                                   StorageTableShare *tableShare) {
  Connection *connection = storageConnection->connection;
  Transaction *transaction = connection->transaction;
  Database *database = connection->database;

  int res = 0;

  try {
    database->truncateTable(tableShare->table, tableShare->sequence,
                            transaction);
  } catch (SQLException &exception) {
    int errorCode = exception.getSqlcode();
    storageConnection->setErrorText(&exception);

    switch (errorCode) {
      case NO_SUCH_TABLE:
        return StorageErrorTableNotFound;

      case UNCOMMITTED_UPDATES:
        return StorageErrorUncommittedUpdates;
    }

    res = 200 - exception.getSqlcode();
  }

  return res;
}

int StorageDatabase::deleteRow(StorageConnection *storageConnection,
                               Table *table, int recordNumber) {
  Connection *connection = storageConnection->connection;
  Transaction *transaction = connection->transaction;
  Record *candidate = NULL, *record = NULL;
  CycleLock cycleLock(connection->database);

  try {
    candidate = table->fetch(recordNumber);

    if (!candidate) return StorageErrorRecordNotFound;

    RECORD_HISTORY(candidate);

    if (candidate->state == recLock)
      record = candidate->getPriorVersion();
    else if (candidate->getTransactionState() == transaction->transactionState)
      record = candidate;
    else
      record = candidate->fetchVersion(transaction);

    if (record != candidate) {
      record->addRef(REC_HISTORY);
      candidate->release(REC_HISTORY);
    }

    table->deleteRecord(transaction, record);
    record->release(REC_HISTORY);

    return 0;
  } catch (SQLException &exception) {
    int code;
    int sqlCode = exception.getSqlcode();

    if (record)
      record->release(REC_HISTORY);
    else if (candidate)
      candidate->release(REC_HISTORY);

    switch (sqlCode) {
      case DEADLOCK:
        code = StorageErrorDeadlock;
        break;

      case LOCK_TIMEOUT:
        code = StorageErrorLockTimeout;
        break;

      case UPDATE_CONFLICT:
        code = StorageErrorUpdateConflict;
        break;

      case OUT_OF_MEMORY_ERROR:
        code = StorageErrorOutOfMemory;
        break;

      default:
        code = StorageErrorRecordNotFound;
    }

    storageConnection->setErrorText(&exception);

    return code;
  }
}

int StorageDatabase::updateRow(StorageConnection *storageConnection,
                               Table *table, Record *oldRecord,
                               Stream *stream) {
  Connection *connection = storageConnection->connection;
  CycleLock cycleLock(connection->database);
  table->update(connection->getTransaction(), oldRecord, stream);

  return 0;
}

int StorageDatabase::createIndex(StorageConnection *storageConnection,
                                 Table *table, const char *sql) {
  Connection *connection = storageConnection->connection;
  Statement *statement = connection->createStatement();

  try {
    statement->execute(sql);
  } catch (SQLException &exception) {
    storageConnection->setErrorText(&exception);
    statement->release();

    if (exception.getSqlcode() == INDEX_OVERFLOW)
      return StorageErrorIndexOverflow;

    return StorageErrorNoIndex;
  }

  statement->release();

  return 0;
}

int StorageDatabase::dropIndex(StorageConnection *storageConnection,
                               Table *table, const char *sql) {
  Connection *connection = storageConnection->connection;
  Statement *statement = connection->createStatement();

  try {
    statement->execute(sql);
  } catch (SQLException &exception) {
    storageConnection->setErrorText(&exception);
    statement->release();

    if (exception.getSqlcode() == INDEX_OVERFLOW)
      return StorageErrorIndexOverflow;

    return StorageErrorNoIndex;
  }

  statement->release();

  return 0;
}

int StorageDatabase::renameTable(StorageConnection *storageConnection,
                                 Table *table, const char *tableName,
                                 const char *schemaName) {
  Connection *connection = storageConnection->connection;

  try {
    Database *database = connection->database;
    Sequence *sequence =
        connection->findSequence(table->schemaName, table->name);

    Sync syncDDL(&database->syncSysDDL, "StorageDatabase::renameTable(1)");
    syncDDL.lock(Exclusive);

    Sync syncTables(&database->syncTables, "StorageDatabase::renameTable(2)");
    syncTables.lock(Exclusive);

    table->rename(schemaName, tableName);

    if (sequence) sequence->rename(schemaName, tableName);

    syncTables.unlock();
    syncDDL.unlock();

    database->commitSystemTransaction();

    return 0;
  } catch (SQLException &exception) {
    storageConnection->setErrorText(&exception);

    return StorageErrorDupKey;
  }
}

Index *StorageDatabase::findIndex(Table *table, const char *indexName) {
  return table->findIndex(indexName);
}

Bitmap *StorageDatabase::indexScan(Index *index, StorageKey *lower,
                                   StorageKey *upper, int searchFlags,
                                   StorageConnection *storageConnection,
                                   Bitmap *bitmap) {
  if (!index) return NULL;

  if (lower) lower->indexKey.index = index;

  if (upper) upper->indexKey.index = index;

  return index->scanIndex(
      (lower) ? &lower->indexKey : NULL, (upper) ? &upper->indexKey : NULL,
      searchFlags, storageConnection->connection->getTransaction(), bitmap);
}

IndexWalker *StorageDatabase::indexPosition(
    Index *index, StorageKey *lower, StorageKey *upper, int searchFlags,
    StorageConnection *storageConnection) {
  if (!index) return NULL;

  if (lower) lower->indexKey.index = index;

  if (upper) upper->indexKey.index = index;

  return index->positionIndex((lower) ? &lower->indexKey : NULL,
                              (upper) ? &upper->indexKey : NULL, searchFlags,
                              storageConnection->connection->getTransaction());
}

int StorageDatabase::makeKey(StorageIndexDesc *indexDesc, const UCHAR *key,
                             int keyLength, StorageKey *storageKey,
                             bool highKey) {
  int segmentNumber = 0;
  Value vals[MAX_KEY_SEGMENTS];
  Value *values[MAX_KEY_SEGMENTS];
  Index *index = indexDesc->index;

  if (!index) return StorageErrorBadKey;

  try {
    for (const UCHAR *p = key, *end = key + keyLength;
         p < end && segmentNumber < indexDesc->numberSegments;
         ++segmentNumber) {
      StorageSegment *segment = indexDesc->segments + segmentNumber;
      int nullFlag = (segment->nullBit) ? *p++ : 0;
      values[segmentNumber] = vals + segmentNumber;
      int len = getSegmentValue(segment, p, values[segmentNumber],
                                index->fields[segmentNumber]);

      if (nullFlag) {
        values[segmentNumber]->setNull();
        if (index->indexVersion < INDEX_VERSION_2) {
          // Older index version do not handle NULLs correctly -it is not the
          // smallest value. Thus, we cannot use values past NULL and need to
          // break here.
          break;
        }
      }

      p += len;
    }

    index->makeKey(segmentNumber, values, &storageKey->indexKey, highKey);
    storageKey->numberSegments = segmentNumber;

    return 0;
  } catch (SQLError &) {
    return StorageErrorBadKey;
  }
}

int StorageDatabase::isKeyNull(StorageIndexDesc *indexDesc, const UCHAR *key,
                               int keyLength) {
  int segmentNumber = 0;

  for (const UCHAR *p = key, *end = key + keyLength;
       p < end && segmentNumber < indexDesc->numberSegments; ++segmentNumber) {
    StorageSegment *segment = indexDesc->segments + segmentNumber;
    int nullFlag = (segment->nullBit) ? *p++ : 0;

    if (!nullFlag) return false;

    switch (segment->type) {
      case HA_KEYTYPE_VARBINARY1:
      case HA_KEYTYPE_VARBINARY2:
      case HA_KEYTYPE_VARTEXT1:
      case HA_KEYTYPE_VARTEXT2:
        p += segment->length + 2;
        break;

      default:
        p += segment->length;
    }
  }

  return true;
}

int StorageDatabase::storeBlob(Connection *connection, Table *table,
                               StorageBlob *blob) {
  return table->storeBlob(connection->getTransaction(), blob->length,
                          blob->data);
}

void StorageDatabase::getBlob(Table *table, int recordNumber,
                              StorageBlob *blob) {
  Stream stream;
  table->getBlob(recordNumber, &stream);
  blob->length = stream.totalLength;
  blob->data = new UCHAR[blob->length];
  stream.getSegment(0, blob->length, blob->data);
}

Sequence *StorageDatabase::findSequence(const char *name,
                                        const char *schemaName) {
  return masterConnection->findSequence(schemaName, name);
}

void StorageDatabase::addRef(void) { ++useCount; }

void StorageDatabase::release(void) {
  if (--useCount == 0) delete this;
}

void StorageDatabase::close(void) {
  if (masterConnection) {
    if (user) {
      user->release();
      user = NULL;
    }

    masterConnection->shutdownDatabase();
    masterConnection = NULL;
  }
}

void StorageDatabase::dropDatabase(void) {
  if (user) {
    user->release();
    user = NULL;
  }

  if (!masterConnection) {
    Connection *connection = getOpenConnection();
    connection->release();
  }

  masterConnection->dropDatabase();
  masterConnection->release();
  masterConnection = NULL;
}

void StorageDatabase::freeBlob(StorageBlob *blob) {
  delete[] blob->data;
  blob->data = NULL;
}

void StorageDatabase::validateCache(void) {
  if (masterConnection) masterConnection->database->validateCache();
}

int StorageDatabase::getSegmentValue(StorageSegment *segment, const UCHAR *ptr,
                                     Value *value, Field *field) {
  int length = segment->length;

  switch (segment->keyFormat) {
    case KEY_FORMAT_LONG_INT: {
      int32 temp =
          (int32)(((int32)((UCHAR)ptr[0])) + (((int32)((UCHAR)ptr[1]) << 8)) +
                  (((int32)((UCHAR)ptr[2]) << 16)) +
                  (((int32)((int16)ptr[3]) << 24)));
      value->setValue(temp);
    } break;

    case KEY_FORMAT_SHORT_INT: {
      short temp =
          (int16)(((short)((UCHAR)ptr[0])) + ((short)((short)ptr[1]) << 8));
      value->setValue(temp);
    } break;

    case KEY_FORMAT_ULONGLONG: {
      uint64 temp = (uint64)((uint64)(((uint32)((UCHAR)ptr[0])) +
                                      (((uint32)((UCHAR)ptr[1])) << 8) +
                                      (((uint32)((UCHAR)ptr[2])) << 16) +
                                      (((uint32)((UCHAR)ptr[3])) << 24)) +
                             (((uint64)(((uint32)((UCHAR)ptr[4])) +
                                        (((uint32)((UCHAR)ptr[5])) << 8) +
                                        (((uint32)((UCHAR)ptr[6])) << 16) +
                                        (((uint32)((UCHAR)ptr[7])) << 24)))
                              << 32));

      // If the MSB is set, we have to set the
      // value as a BigInt, if it is not set, we
      // can use int64

      if (temp & 0x8000000000000000ULL) {
        BigInt bigInt;
        bigInt.set(temp);
        value->setValue(&bigInt);
      } else {
        value->setValue((int64)temp);
      }

    } break;

    case KEY_FORMAT_LONGLONG:
      if (segment->type_converter) {
        int64 temp = segment->type_converter((const char *)ptr);
        value->setValue(temp);
      } else {
        int64 temp = (int64)((uint64)(((uint32)((UCHAR)ptr[0])) +
                                      (((uint32)((UCHAR)ptr[1])) << 8) +
                                      (((uint32)((UCHAR)ptr[2])) << 16) +
                                      (((uint32)((UCHAR)ptr[3])) << 24)) +
                             (((uint64)(((uint32)((UCHAR)ptr[4])) +
                                        (((uint32)((UCHAR)ptr[5])) << 8) +
                                        (((uint32)((UCHAR)ptr[6])) << 16) +
                                        (((uint32)((UCHAR)ptr[7])) << 24)))
                              << 32));
        value->setValue(temp);
      }
      break;

    case KEY_FORMAT_FLOAT: {
      float temp;
#ifdef _BIG_ENDIAN
      ((UCHAR *)&temp)[0] = ptr[3];
      ((UCHAR *)&temp)[1] = ptr[2];
      ((UCHAR *)&temp)[2] = ptr[1];
      ((UCHAR *)&temp)[3] = ptr[0];
#else
      memcpy(&temp, ptr, sizeof(temp));
#endif
      value->setValue(temp);
    } break;

    case KEY_FORMAT_DOUBLE: {
      double temp;
#ifdef _BIG_ENDIAN
      ((UCHAR *)&temp)[0] = ptr[7];
      ((UCHAR *)&temp)[1] = ptr[6];
      ((UCHAR *)&temp)[2] = ptr[5];
      ((UCHAR *)&temp)[3] = ptr[4];
      ((UCHAR *)&temp)[4] = ptr[3];
      ((UCHAR *)&temp)[5] = ptr[2];
      ((UCHAR *)&temp)[6] = ptr[1];
      ((UCHAR *)&temp)[7] = ptr[0];
#else
      memcpy(&temp, ptr, sizeof(temp));
#endif
      value->setValue(temp);
    } break;

    case KEY_FORMAT_VARBINARY:
    case KEY_FORMAT_VARTEXT: {
      unsigned short len = (unsigned short)(((short)((UCHAR)ptr[0])) +
                                            ((short)((short)ptr[1]) << 8));
      value->setString(len, (const char *)ptr + 2, false);
      length += 2;
    } break;

    case KEY_FORMAT_TEXT:
    case KEY_FORMAT_BINARY_STRING:
      value->setString(length, (const char *)ptr, false);
      break;

    case KEY_FORMAT_BINARY_NEWDECIMAL: {
      BigInt bigInt;
      ScaledBinary::getBigIntFromBinaryDecimal(
          (const char *)ptr, field->precision, field->scale, &bigInt);
      value->setValue(&bigInt);
    } break;
    case KEY_FORMAT_BINARY_INTEGER: {
      int64 number = 0;

      for (int n = 0; n < length; ++n) number = number << 8 | *ptr++;

      value->setValue(number);
    } break;

    case KEY_FORMAT_ULONG_INT: {
      uint32 temp = (uint32)(((uint32)((UCHAR)ptr[0])) +
                             (((uint32)((UCHAR)ptr[1]) << 8)) +
                             (((uint32)((UCHAR)ptr[2]) << 16)) +
                             (((uint32)((UCHAR)ptr[3]) << 24)));

      value->setValue((int64)temp);
    } break;

    case KEY_FORMAT_TIMESTAMP: {
      uint32 temp = (uint32)(((uint32)((UCHAR)ptr[0])) +
                             (((uint32)((UCHAR)ptr[1]) << 8)) +
                             (((uint32)((UCHAR)ptr[2]) << 16)) +
                             (((uint32)((UCHAR)ptr[3]) << 24)));

      value->setValue((int64)temp * 1000);
    } break;

    case KEY_FORMAT_INT8:
      value->setValue(*(signed char *)ptr);
      break;

    case KEY_FORMAT_USHORT_INT: {
      unsigned short temp = (unsigned short)(((uint16)((UCHAR)ptr[0])) +
                                             ((uint16)((UCHAR)ptr[1]) << 8));
      value->setValue(temp);
    } break;

    case KEY_FORMAT_UINT24: {
      uint32 temp =
          (uint32)(((uint32)((UCHAR)ptr[0])) + ((uint32)((UCHAR)ptr[1]) << 8) +
                   ((uint32)((UCHAR)ptr[2]) << 16));
      value->setValue((int32)temp);
    } break;

    case KEY_FORMAT_INT24: {
      int32 temp = (int32)((((UCHAR)ptr[2]) & 128)
                               ? (((uint32)((UCHAR)ptr[0])) +
                                      ((uint32)((UCHAR)ptr[1]) << 8) +
                                      ((uint32)((UCHAR)ptr[2]) << 16) |
                                  ((uint32)255L << 24))
                               : (((uint32)((UCHAR)ptr[0])) +
                                  ((uint32)((UCHAR)ptr[1]) << 8) +
                                  ((uint32)((UCHAR)ptr[2]) << 16)));
      value->setValue(temp);
    } break;

    default:
      NOT_YET_IMPLEMENTED;
  }

  return length;
}

void StorageDatabase::save(void) {
  Sync sync(&storageHandler->dictionarySyncObject, "StorageTableShare::save");
  sync.lock(Exclusive);
  Connection *connection = storageHandler->getDictionaryConnection();
  PreparedStatement *statement = connection->prepareStatement(
      "replace changjiang.tablespaces (name, pathname) values (?,?)");
  int n = 1;
  statement->setString(n++, name);
  statement->setString(n++, filename);
  statement->executeUpdate();
  statement->close();
  connection->commit();
}

void StorageDatabase::load(void) {
  Sync sync(&storageHandler->dictionarySyncObject, "StorageDatabase::load");
  sync.lock(Exclusive);
  Connection *connection = storageHandler->getDictionaryConnection();

  if (connection) {
    PreparedStatement *statement = connection->prepareStatement(
        "select pathname from changjiang.tablespaces where name=?");
    statement->setString(1, name);
    ResultSet *resultSet = statement->executeQuery();

    if (resultSet->next()) filename = resultSet->getString(1);

    resultSet->close();
    statement->close();
    connection->commit();
  }
}

void StorageDatabase::clearTransactions(void) {
#ifdef TRACE_TRANSACTIONS

  Sync sync(&syncTrace, "StorageDatabase::clearTransactions");
  sync.lock(Exclusive);
  Statement *statement = masterConnection->createStatement();
  statement->execute(traceTable);
  statement->release();

  PreparedStatement *preparedStatement =
      masterConnection->prepareStatement("delete from changjiang.transactions");
  preparedStatement->executeUpdate();
  preparedStatement->close();
  masterConnection->commit();
#endif
}

void StorageDatabase::traceTransaction(int transId, int committed,
                                       int blockedBy, Stream *stream) {
  try {
    Sync sync(&syncTrace, "StorageDatabase::traceTransaction");
    sync.lock(Exclusive);
    char buffer[10000];
    int length = stream->getSegment(0, sizeof(buffer) - 1, buffer);
    buffer[length] = 0;

    if (!insertTrace)
      insertTrace = masterConnection->prepareStatement(
          "insert into changjiang.transactions "
          "(transaction_id,committed,blocked_by,statements) values (?,?,?,?)");

    int n = 1;
    insertTrace->setInt(n++, transId);
    insertTrace->setInt(n++, committed);
    insertTrace->setInt(n++, blockedBy);
    insertTrace->setString(n++, buffer);
    insertTrace->executeUpdate();
    masterConnection->commit();
  } catch (SQLException &) {
  }
}

void StorageDatabase::getIOInfo(InfoTable *infoTable) {
  if (masterConnection && masterConnection->database) {
    masterConnection->database->getIOInfo(infoTable);
    masterConnection->database->tableSpaceManager->getIOInfo(infoTable);
  }
}

void StorageDatabase::getTransactionInfo(InfoTable *infoTable) {
  if (masterConnection && masterConnection->database)
    masterConnection->database->getTransactionInfo(infoTable);
}

void StorageDatabase::getStreamLogInfo(InfoTable *infoTable) {
  if (masterConnection && masterConnection->database)
    masterConnection->database->getStreamLogInfo(infoTable);
}

void StorageDatabase::getTransactionSummaryInfo(InfoTable *infoTable) {
  if (masterConnection && masterConnection->database)
    masterConnection->database->getTransactionSummaryInfo(infoTable);
}

void StorageDatabase::getTableSpaceInfo(InfoTable *infoTable) {
  if (masterConnection && masterConnection->database)
    masterConnection->database->getTableSpaceInfo(infoTable);
}

void StorageDatabase::getTableSpaceFilesInfo(InfoTable *infoTable) {
  if (masterConnection && masterConnection->database)
    masterConnection->database->getTableSpaceFilesInfo(infoTable);
}

}  // namespace Changjiang
