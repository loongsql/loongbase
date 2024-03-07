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

class THD;

namespace Changjiang {

#define MASTER_NAME "CHANGJIANG_MASTER"
#define MASTER_PATH "changjiang_master.cts"
#define DEFAULT_TABLESPACE "CHANGJIANG_USER"
#define DEFAULT_TABLESPACE_PATH "changjiang_user.cts"
#define TEMPORARY_TABLESPACE "CHANGJIANG_TEMPORARY"
#define TEMPORARY_PATH "changjiang_temporary.cts"

static const int connectionHashSize = 101;

enum OpenOption {
  CreateDatabase,
  OpenDatabase,
  OpenOrCreateDatabase,
  OpenTemporaryDatabase
};

static const int TABLESPACE_INTERNAL = 0;
static const int TABLESPACE_SCHEMA = 1;

typedef void(Logger)(int, const char *, void *arg);

struct IOAnalysis;
class StorageConnection;
class StorageHandler;
class StorageDatabase;
class StorageTableShare;
class SyncObject;
class Connection;
class InfoTable;
class PreparedStatement;
// class THD;

extern "C" {
StorageHandler *getChangjiangStorageHandler(int lockSize);
void freeChangjiangStorageHandler(void);
}

static const int databaseHashSize = 101;
static const int tableHashSize = 101;

class StorageHandler {
 public:
  StorageHandler(int lockSize);
  virtual ~StorageHandler(void);
  virtual void startNfsServer(void);
  virtual void addNfsLogger(int mask, Logger listener, void *arg);
  virtual void deleteNfsLogger(Logger listener, void *arg);

  virtual void shutdownHandler(void);
  virtual void databaseDropped(StorageDatabase *storageDatabase,
                               StorageConnection *storageConnection);
  virtual int startTransaction(THD *mySqlThread, int isolationLevel);
  virtual int commit(THD *mySqlThread);
  virtual int rollback(THD *mySqlThread);
  virtual int releaseVerb(THD *mySqlThread);
  virtual int rollbackVerb(THD *mySqlThread);

  virtual int savepointSet(THD *mySqlThread, void *savePoint);
  virtual int savepointRelease(THD *mySqlThread, void *savePoint);
  virtual int savepointRollback(THD *mySqlThread, void *savePoint);
  virtual void releaseText(const char *text);

  virtual int recoverGetNextLimbo(int xidSize, unsigned char *xid);
  virtual int commitByXid(int xidLength, const unsigned char *xid);
  virtual int rollbackByXid(int xidLength, const unsigned char *xis);

  virtual Connection *getDictionaryConnection(void);
  virtual int createTablespace(const char *tableSpaceName, const char *filename,
                               const char *comment = NULL);
  virtual int deleteTablespace(const char *tableSpaceName);

  virtual StorageTableShare *findTable(const char *pathname);
  virtual StorageTableShare *createTable(const char *pathname,
                                         const char *tableSpaceName,
                                         bool tempTable);
  virtual StorageConnection *getStorageConnection(StorageTableShare *tableShare,
                                                  THD *mySqlThread,
                                                  int mySqlThdId,
                                                  OpenOption createFlag);

  virtual void getIOInfo(InfoTable *infoTable);
  virtual void getMemoryDetailInfo(InfoTable *infoTable);
  virtual void getMemorySummaryInfo(InfoTable *infoTable);
  virtual void getRecordCacheDetailInfo(InfoTable *infoTable);
  virtual void getRecordCacheSummaryInfo(InfoTable *infoTable);
  virtual void getTransactionInfo(InfoTable *infoTable);
  virtual void getStreamLogInfo(InfoTable *infoTable);
  virtual void getSyncInfo(InfoTable *infoTable);
  virtual void getTransactionSummaryInfo(InfoTable *infoTable);
  virtual void getTableSpaceInfo(InfoTable *infoTable);
  virtual void getTableSpaceFilesInfo(InfoTable *infoTable);

  virtual void setIndexChillThreshold(uint value);
  virtual void setRecordChillThreshold(uint value);
  virtual void setRecordMemoryMax(uint64 size);
  virtual void setRecordScavengeThreshold(int value);
  virtual void setRecordScavengeFloor(int value);
  virtual StorageTableShare *preDeleteTable(const char *pathname);
  virtual void getChangjiangVersionInfo(InfoTable *infoTable);
  virtual const char *normalizeName(const char *name, int bufferSize,
                                    char *buffer);

  StorageDatabase *getStorageDatabase(const char *dbName, const char *path);
  void remove(StorageConnection *storageConnection);
  void closeDatabase(const char *path);
  int prepare(THD *mySqlThread, int xidSize, const unsigned char *xid);
  StorageDatabase *findTablespace(const char *name);
  void removeTable(StorageTableShare *table);
  void addTable(StorageTableShare *table);
  StorageDatabase *findDatabase(const char *dbName);
  void changeMySqlThread(StorageConnection *storageConnection, THD *newThread);
  void removeConnection(StorageConnection *storageConnection);
  int closeConnections(THD *thd);
  int dropDatabase(const char *path);
  void initialize(void);
  void createDatabase(void);
  void dropTempTables(void);
  void cleanFileName(const char *pathname, char *filename, int filenameLength);
  JString genCreateTableSpace(const char *tableSpaceName, const char *filename,
                              const char *comment = NULL);

  StorageConnection *connections[connectionHashSize];
  StorageDatabase *defaultDatabase;
  SyncObject syncObject;
  SyncObject hashSyncObject;
  SyncObject dictionarySyncObject;
  StorageDatabase *storageDatabases[databaseHashSize];
  StorageDatabase *databaseList;
  StorageTableShare *tables[tableHashSize];
  Connection *dictionaryConnection;
  int mySqlLockSize;
  bool initialized;
  static void setDataDirectory(const char *directory);
};

}  // namespace Changjiang
