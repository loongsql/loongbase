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

#pragma once

#include "SyncObject.h"

namespace Changjiang {

//#define TRACE_TRANSACTIONS

static const int shareHashSize = 101;

class StorageConnection;
class StorageTableShare;
class StorageTable;
class StorageHandler;
class Connection;
class Table;
class User;
class Record;
class RecordVersion;
class Index;
class PreparedStatement;
class Sequence;
class Value;
class Bitmap;
class IndexWalker;

CLASS(Field);

class StorageIndexDesc;
struct StorageKey;
struct StorageBlob;
struct StorageSegment;

class StorageDatabase {
 public:
  StorageDatabase(StorageHandler *handler, const char *dbName,
                  const char *path);
  ~StorageDatabase(void);

  Connection *getConnection();
  Connection *getOpenConnection(void);
  Connection *createDatabase(void);
  Table *createTable(StorageConnection *storageConnection,
                     const char *tableName, const char *schemaName,
                     const char *sql, int64 autoIncrementValue);
  Table *upgradeTable(StorageConnection *storageConnection,
                      const char *tableName, const char *schemaName,
                      const char *sql, int64 autoIncrementValue);
  int savepointSet(Connection *connection);
  int savepointRelease(Connection *connection, int savePoint);
  int savepointRollback(Connection *connection, int savePoint);
  int deleteTable(StorageConnection *storageConnection,
                  StorageTableShare *tableShare);
  int truncateTable(StorageConnection *storageConnection,
                    StorageTableShare *tableShare);
  int renameTable(StorageConnection *storageConnection, Table *table,
                  const char *newName, const char *schemaName);
  Bitmap *indexScan(Index *index, StorageKey *lower, StorageKey *upper,
                    int searchFlags, StorageConnection *storageConnection,
                    Bitmap *bitmap);
  IndexWalker *indexPosition(Index *index, StorageKey *lower, StorageKey *upper,
                             int searchFlags,
                             StorageConnection *storageConnection);
  int makeKey(StorageIndexDesc *index, const UCHAR *key, int keyLength,
              StorageKey *storageKey, bool highKey);
  int storeBlob(Connection *connection, Table *table, StorageBlob *blob);
  void getBlob(Table *table, int recordNumber, StorageBlob *blob);
  void addRef(void);
  void release(void);
  void dropDatabase(void);
  void freeBlob(StorageBlob *blob);
  void close(void);
  void validateCache(void);
  int createIndex(StorageConnection *storageConnection, Table *table,
                  const char *sql);
  int dropIndex(StorageConnection *storageConnection, Table *table,
                const char *sql);
  int insert(Connection *connection, Table *table, Stream *stream);

  int nextRow(StorageTable *storageTable, int recordNumber, bool lockForUpdate);
  int nextIndexed(StorageTable *storageTable, void *recordBitmap,
                  int recordNumber, bool lockForUpdate);
  int nextIndexed(StorageTable *storageTable, IndexWalker *indexWalker,
                  bool lockForUpdate);
  int fetch(StorageConnection *storageConnection, StorageTable *storageTable,
            int recordNumber, bool lockForUpdate);

  int updateRow(StorageConnection *storageConnection, Table *table,
                Record *oldRecord, Stream *stream);
  int getSegmentValue(StorageSegment *segment, const UCHAR *ptr, Value *value,
                      Field *field);
  int deleteRow(StorageConnection *storageConnection, Table *table,
                int recordNumber);
  Table *findTable(const char *tableName, const char *schemaName);
  Index *findIndex(Table *table, const char *indexName);
  Sequence *findSequence(const char *name, const char *schemaName);
  int isKeyNull(StorageIndexDesc *indexDesc, const UCHAR *key, int keyLength);
  void save(void);
  void load(void);

  void clearTransactions(void);
  void traceTransaction(int transId, int committed, int blockedBy,
                        Stream *stream);

  void getIOInfo(InfoTable *infoTable);
  void getTransactionInfo(InfoTable *infoTable);
  void getStreamLogInfo(InfoTable *infoTable);
  void getTransactionSummaryInfo(InfoTable *infoTable);
  void getTableSpaceInfo(InfoTable *infoTable);
  void getTableSpaceFilesInfo(InfoTable *infoTable);

  Connection *masterConnection;
  JString name;
  JString filename;
  StorageDatabase *collision;
  StorageDatabase *next;
  StorageHandler *storageHandler;
  SyncObject syncObject;
  SyncObject syncTrace;
  User *user;
  PreparedStatement *lookupIndexAlias;
  PreparedStatement *insertTrace;
  int useCount;
};

}  // namespace Changjiang
