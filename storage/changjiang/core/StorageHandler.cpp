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
#include "StorageHandler.h"
#include "StorageConnection.h"
#include "StorageVersion.h"
#include "Connection.h"
#include "SyncObject.h"
#include "Sync.h"
#include "SQLError.h"
#include "StorageDatabase.h"
#include "StorageTableShare.h"
#include "Stream.h"
#include "Configuration.h"
#include "Threads.h"
#include "Connection.h"
#include "PStatement.h"
#include "RSet.h"
#include "InfoTable.h"
#include "CmdGen.h"
#include "Dbb.h"
#include "Database.h"

namespace Changjiang {

#define DICTIONARY_ACCOUNT "mysql"
#define DICTIONARY_PW "mysql"
#define CHANGJIANG_USER DEFAULT_TABLESPACE_PATH
#define CHANGJIANG_TEMPORARY TEMPORARY_PATH
#define WHITE_SPACE " \t\n\r"
#define PUNCTUATION_CHARS ".+-*/%()*<>=!;,?{}[]:~^|"

#define HASH(address, size) (int)(((UIPTR)address >> 2) % size)

struct StorageSavepoint {
  StorageSavepoint *next;
  StorageConnection *storageConnection;
  int savepoint;
};

static const char *createTempSpace = "upgrade tablespace " TEMPORARY_TABLESPACE
                                     " filename '" CHANGJIANG_TEMPORARY "'";
// static const char *dropTempSpace = "drop tablespace " TEMPORARY_TABLESPACE;

static const char *changjiangSchema[] = {
    //"create tablespace " DEFAULT_TABLESPACE " filename '" CHANGJIANG_USER "'
    //allocation 2000000000",
    createTempSpace,

    "upgrade table changjiang.tablespaces ("
    "    name varchar(128) not null primary	key,"
    "    pathname varchar(1024) not null)",

    "upgrade table changjiang.tables ("
    "    given_schema_name varchar(128) not null,"
    "    effective_schema_name varchar(128) not null,"
    "    given_table_name varchar(128) not null,"
    "    effective_table_name varchar(128) not null,"
    "    tablespace_name varchar(128) not null,"
    "    pathname varchar(1024) not null primary key)",

    "upgrade unique index effective on changjiang.tables "
    "(effective_schema_name, effective_table_name)",

    NULL};

class Server;
extern Server *startServer(int port, const char *configFile);

StorageHandler *storageHandler;

static char charTable[256];
static int init();
static int foo = init();

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

int init() {
  const char *p;

  for (p = WHITE_SPACE; *p; p++) charTable[(unsigned char)*p] = 1;

  for (p = PUNCTUATION_CHARS; *p; p++) charTable[(unsigned char)*p] = 1;

  return 1;
}

StorageHandler *getChangjiangStorageHandler(int lockSize) {
  if (!storageHandler) storageHandler = new StorageHandler(lockSize);

  return storageHandler;
}

void freeChangjiangStorageHandler(void) {
  if (storageHandler) {
    delete storageHandler;
    storageHandler = 0;
  }
}
void StorageHandler::setDataDirectory(const char *directory) {
  IO::setBaseDirectory(directory);
}

StorageHandler::StorageHandler(int lockSize) {
  mySqlLockSize = lockSize;
  memset(connections, 0, sizeof(connections));
  memset(storageDatabases, 0, sizeof(storageDatabases));
  memset(tables, 0, sizeof(tables));
  dictionaryConnection = NULL;
  databaseList = NULL;
  defaultDatabase = NULL;
  initialized = false;
  syncObject.setName("StorageHandler::syncObject");
  hashSyncObject.setName("StorageHandler::hashSyncObject");
  dictionarySyncObject.setName("StorageHandler::dictionarySyncObject");
  inCreateDatabase = false;
  deleteFilesOnExit = false;
}

StorageHandler::~StorageHandler(void) {
  for (int n = 0; n < tableHashSize; ++n)
    for (StorageTableShare *table; (table = tables[n]);) {
      tables[n] = table->collision;
      delete table;
    }

  for (int n = 0; n < databaseHashSize; ++n)
    for (StorageDatabase *storageDatabase;
         (storageDatabase = storageDatabases[n]);) {
      storageDatabases[n] = storageDatabase->collision;
      delete storageDatabase;
    }
}

void StorageHandler::startNfsServer(void) {
  try {
    startServer(0, NULL);
  } catch (SQLException &exception) {
    Log::log("Can't start debug server: %s\n", exception.getText());
  }
}

void StorageHandler::addNfsLogger(int mask, Logger listener, void *arg) {
  addLogListener(mask, listener, arg);
}

void StorageHandler::deleteNfsLogger(Logger listener, void *arg) {
  deleteLogListener(listener, arg);
}

void StorageHandler::shutdownHandler(void) {
  if (dictionaryConnection) {
    dictionaryConnection->commit();
    dictionaryConnection->close();
    dictionaryConnection = NULL;
  }

  for (int n = 0; n < databaseHashSize; ++n)
    for (StorageDatabase *storageDatabase = storageDatabases[n];
         storageDatabase; storageDatabase = storageDatabase->collision)
      storageDatabase->close();

  /***
  Configuration configuration(NULL);
  Connection *connection = new Connection(&configuration);
  connection->shutdown();
  connection->close();
  ***/
}

void StorageHandler::databaseDropped(StorageDatabase *storageDatabase,
                                     StorageConnection *storageConnection) {
  if (!storageDatabase && storageConnection)
    storageDatabase = storageConnection->storageDatabase;

  if (storageDatabase) {
    Sync syncHash(&hashSyncObject, "StorageHandler::databaseDropped(1)");
    int slot = JString::hash(storageDatabase->name, databaseHashSize);
    syncHash.lock(Exclusive);
    StorageDatabase **ptr;

    for (ptr = storageDatabases + slot; *ptr; ptr = &(*ptr)->collision)
      if (*ptr == storageDatabase) {
        *ptr = storageDatabase->collision;
        break;
      }

    for (ptr = &databaseList; *ptr; ptr = &(*ptr)->next)
      if (*ptr == storageDatabase) {
        *ptr = storageDatabase->next;
        break;
      }

    syncHash.unlock();
    storageDatabase->release();
  }

  Sync sync(&syncObject, "StorageHandler::databaseDropped(2)");
  sync.lock(Exclusive);

  for (int n = 0; n < connectionHashSize; ++n)
    for (StorageConnection *cnct = connections[n]; cnct; cnct = cnct->collision)
      if (cnct != storageConnection) cnct->databaseDropped(storageDatabase);

  sync.unlock();
}

void StorageHandler::remove(StorageConnection *storageConnection) {
  Sync sync(&syncObject, "StorageHandler::remove");
  sync.lock(Exclusive);
  removeConnection(storageConnection);
}

int StorageHandler::startTransaction(THD *mySqlThread, int isolationLevel) {
  Sync sync(&syncObject, "StorageHandler::commit");
  sync.lock(Shared);
  int slot = HASH(mySqlThread, connectionHashSize);

  for (StorageConnection *storageConnection = connections[slot];
       storageConnection; storageConnection = storageConnection->collision) {
    if (storageConnection->mySqlThread == mySqlThread) {
      storageConnection->startTransaction(isolationLevel);
      if (storageConnection->connection)
        storageConnection->connection->getTransaction();
      return 0;
    }
  }
  return 1;
}

int StorageHandler::commit(THD *mySqlThread) {
  Sync sync(&syncObject, "StorageHandler::commit");
  sync.lock(Shared);
  int slot = HASH(mySqlThread, connectionHashSize);

  for (StorageConnection *connection = connections[slot]; connection;
       connection = connection->collision)
    if (connection->mySqlThread == mySqlThread) {
      int ret = connection->commit();

      if (ret) return ret;
    }

  return 0;
}

int StorageHandler::prepare(THD *mySqlThread, int xidSize, const UCHAR *xid) {
  Sync sync(&syncObject, "StorageHandler::prepare");
  sync.lock(Shared);
  int slot = HASH(mySqlThread, connectionHashSize);

  for (StorageConnection *connection = connections[slot]; connection;
       connection = connection->collision)
    if (connection->mySqlThread == mySqlThread) {
      int ret = connection->prepare(xidSize, xid);

      if (ret) return ret;
    }

  return 0;
}

int StorageHandler::rollback(THD *mySqlThread) {
  Sync sync(&syncObject, "StorageHandler::rollback");
  sync.lock(Shared);
  int slot = HASH(mySqlThread, connectionHashSize);

  for (StorageConnection *connection = connections[slot]; connection;
       connection = connection->collision)
    if (connection->mySqlThread == mySqlThread) {
      int ret = connection->rollback();

      if (ret) return ret;
    }

  return 0;
}

int StorageHandler::releaseVerb(THD *mySqlThread) {
  Sync sync(&syncObject, "StorageHandler::releaseVerb");
  sync.lock(Shared);
  int slot = HASH(mySqlThread, connectionHashSize);

  for (StorageConnection *connection = connections[slot]; connection;
       connection = connection->collision)
    if (connection->mySqlThread == mySqlThread) connection->releaseVerb();

  return 0;
}

int StorageHandler::rollbackVerb(THD *mySqlThread) {
  Sync sync(&syncObject, "StorageHandler::rollbackVerb");
  sync.lock(Shared);
  int slot = HASH(mySqlThread, connectionHashSize);

  for (StorageConnection *connection = connections[slot]; connection;
       connection = connection->collision)
    if (connection->mySqlThread == mySqlThread) connection->rollbackVerb();

  return 0;
}

int StorageHandler::savepointSet(THD *mySqlThread, void *savePoint) {
  Sync sync(&syncObject, "StorageHandler::savepointSet");
  sync.lock(Shared);
  int slot = HASH(mySqlThread, connectionHashSize);
  StorageSavepoint *savepoints = NULL;

  for (StorageConnection *connection = connections[slot]; connection;
       connection = connection->collision)
    if (connection->mySqlThread == mySqlThread) {
      StorageSavepoint *savepoint = new StorageSavepoint;
      savepoint->next = savepoints;
      savepoints = savepoint;
      savepoint->storageConnection = connection;
      savepoint->savepoint = connection->savepointSet();
    }

  *((void **)savePoint) = savepoints;

  return 0;
}

int StorageHandler::savepointRelease(THD *mySqlThread, void *savePoint) {
  Sync sync(&syncObject, "StorageHandler::savepointRelease");
  sync.lock(Shared);

  for (StorageSavepoint *savepoints = *(StorageSavepoint **)savePoint,
                        *savepoint;
       (savepoint = savepoints);) {
    savepoint->storageConnection->savepointRelease(savepoint->savepoint);
    savepoints = savepoint->next;
    delete savepoint;
  }

  *((void **)savePoint) = NULL;

  return 0;
}

int StorageHandler::savepointRollback(THD *mySqlThread, void *savePoint) {
  Sync sync(&syncObject, "StorageHandler::savepointRollback");
  sync.lock(Shared);

  for (StorageSavepoint *savepoints = *(StorageSavepoint **)savePoint,
                        *savepoint;
       (savepoint = savepoints);) {
    savepoint->storageConnection->savepointRollback(savepoint->savepoint);
    savepoints = savepoint->next;
    delete savepoint;
  }

  *((void **)savePoint) = NULL;

  return 0;
}

StorageDatabase *StorageHandler::getStorageDatabase(const char *dbName,
                                                    const char *path) {
  Sync sync(&hashSyncObject, "StorageHandler::getStorageDatabase");
  int slot = JString::hash(dbName, databaseHashSize);
  StorageDatabase *storageDatabase;

  if (storageDatabases[slot]) {
    sync.lock(Shared);

    if ((storageDatabase = findDatabase(dbName))) return storageDatabase;

    sync.unlock();
  }

  sync.lock(Exclusive);

  if ((storageDatabase = findDatabase(dbName))) return storageDatabase;

  storageDatabase = new StorageDatabase(this, dbName, path);
  storageDatabase->load();
  storageDatabase->collision = storageDatabases[slot];
  storageDatabases[slot] = storageDatabase;
  storageDatabase->addRef();
  storageDatabase->next = databaseList;
  databaseList = storageDatabase;

  return storageDatabase;
}

void StorageHandler::closeDatabase(const char *path) {
  Sync sync(&hashSyncObject, "StorageHandler::closeDatabase");
  int slot = JString::hash(path, databaseHashSize);
  sync.lock(Exclusive);

  for (StorageDatabase *storageDatabase, **ptr = storageDatabases + slot;
       (storageDatabase = *ptr); ptr = &storageDatabase->collision)
    if (storageDatabase->filename == path) {
      *ptr = storageDatabase->collision;
      storageDatabase->close();
      storageDatabase->release();
      break;
    }
}

void StorageHandler::releaseText(const char *text) { delete[] text; }

int StorageHandler::commitByXid(int xidLength, const UCHAR *xid) {
  if (dictionaryConnection) dictionaryConnection->commitByXid(xidLength, xid);

  return 0;
}

int StorageHandler::rollbackByXid(int xidLength, const UCHAR *xid) {
  if (dictionaryConnection) dictionaryConnection->rollbackByXid(xidLength, xid);

  return 0;
}

Connection *StorageHandler::getDictionaryConnection(void) {
  return dictionaryConnection;
}

JString StorageHandler::genCreateTableSpace(const char *tableSpaceName,
                                            const char *filename,
                                            const char *comment) {
  CmdGen gen;
  gen.gen("create tablespace \"%s\" filename '%s' comment '%s'", tableSpaceName,
          filename, comment ? comment : "");
  return (gen.getString());
}

int StorageHandler::createTablespace(const char *tableSpaceName,
                                     const char *filename,
                                     const char *comment) {
  if (!defaultDatabase) initialize();

  if (!dictionaryConnection) return StorageErrorTablesSpaceOperationFailed;

  if (!strcasecmp(tableSpaceName, MASTER_NAME) ||
      !strcasecmp(tableSpaceName, DEFAULT_TABLESPACE) ||
      !strcasecmp(tableSpaceName, TEMPORARY_TABLESPACE)) {
    return StorageErrorTableSpaceExist;
  }

  try {
    JString tableSpace = JString::upcase(tableSpaceName);
    JString cmd = genCreateTableSpace(tableSpace, filename, comment);
    Sync sync(&dictionarySyncObject, "StorageHandler::createTablespace");
    sync.lock(Exclusive);
    Statement *statement = dictionaryConnection->createStatement();
    statement->executeUpdate(cmd);
    statement->close();
  } catch (SQLException &exception) {
    if (exception.getSqlcode() == TABLESPACE_EXIST_ERROR)
      return StorageErrorTableSpaceExist;

    if (exception.getSqlcode() == TABLESPACE_NOT_EXIST_ERROR)
      return StorageErrorTableSpaceNotExist;

    if (exception.getSqlcode() == TABLESPACE_DATAFILE_EXIST_ERROR)
      return StorageErrorTableSpaceDataFileExist;

    return StorageErrorTablesSpaceOperationFailed;
  }

  return 0;
}

int StorageHandler::deleteTablespace(const char *tableSpaceName) {
  if (!defaultDatabase) initialize();

  if (!dictionaryConnection) return StorageErrorTablesSpaceOperationFailed;

  if (!strcasecmp(tableSpaceName, MASTER_NAME) ||
      !strcasecmp(tableSpaceName, DEFAULT_TABLESPACE) ||
      !strcasecmp(tableSpaceName, TEMPORARY_TABLESPACE)) {
    return StorageErrorTablesSpaceOperationFailed;
  }

  try {
    JString tableSpace = JString::upcase(tableSpaceName);
    CmdGen gen;
    gen.gen("drop tablespace \"%s\"", (const char *)tableSpace);
    Sync sync(&dictionarySyncObject, "StorageHandler::deleteTablespace");
    sync.lock(Exclusive);
    Statement *statement = dictionaryConnection->createStatement();
    statement->executeUpdate(gen.getString());
    statement->close();
  } catch (SQLException &exception) {
    int sqlCode = exception.getSqlcode();

    if (sqlCode == TABLESPACE_NOT_EXIST_ERROR)
      return StorageErrorTableSpaceNotExist;

    if (sqlCode == TABLESPACE_NOT_EMPTY) return StorageErrorTableNotEmpty;

    return StorageErrorTablesSpaceOperationFailed;
  }

  return 0;
}

StorageTableShare *StorageHandler::findTable(const char *pathname) {
  char filename[1024];
  cleanFileName(pathname, filename, sizeof(filename));
  Sync sync(&hashSyncObject, "StorageHandler::findTable");
  int slot = JString::hash(filename, tableHashSize);
  StorageTableShare *tableShare;

  if (tables[slot]) {
    sync.lock(Shared);

    for (tableShare = tables[slot]; tableShare;
         tableShare = tableShare->collision)
      if (tableShare->pathName == filename) return tableShare;

    sync.unlock();
  }

  sync.lock(Exclusive);

  for (tableShare = tables[slot]; tableShare;
       tableShare = tableShare->collision)
    if (tableShare->pathName == filename) return tableShare;

  tableShare =
      new StorageTableShare(this, filename, NULL, mySqlLockSize, false);
  tableShare->collision = tables[slot];
  tables[slot] = tableShare;

  ASSERT(tableShare->collision != tableShare);

  return tableShare;
}

StorageTableShare *StorageHandler::preDeleteTable(const char *pathname) {
  if (!defaultDatabase) initialize();

  if (!dictionaryConnection) return NULL;

  char filename[1024];
  cleanFileName(pathname, filename, sizeof(filename));
  int slot = JString::hash(filename, tableHashSize);
  StorageTableShare *tableShare;

  if (tables[slot]) {
    Sync sync(&hashSyncObject, "StorageHandler::preDeleteTable");
    sync.lock(Shared);

    for (tableShare = tables[slot]; tableShare;
         tableShare = tableShare->collision)
      if (tableShare->pathName == filename) return tableShare;
  }

  try {
    tableShare =
        new StorageTableShare(this, filename, NULL, mySqlLockSize, false);
    JString path = tableShare->lookupPathName();
    delete tableShare;

    if (path == pathname) return findTable(pathname);
  } catch (...) {
  }

  return NULL;
}

StorageTableShare *StorageHandler::createTable(const char *pathname,
                                               const char *tableSpaceName,
                                               bool tempTable) {
  if (!defaultDatabase) initialize();

  if (!dictionaryConnection) return NULL;

  StorageTableShare *tableShare = new StorageTableShare(
      this, pathname, tableSpaceName, mySqlLockSize, tempTable);

  if (tableShare->tableExists()) {
    delete tableShare;

    return NULL;
  }

  addTable(tableShare);
  tableShare->registerTable();

  return tableShare;
}

void StorageHandler::addTable(StorageTableShare *table) {
  int slot = JString::hash(table->pathName, tableHashSize);
  Sync sync(&hashSyncObject, "StorageHandler::addTable");
  sync.lock(Exclusive);
  table->collision = tables[slot];
  tables[slot] = table;

  ASSERT(table->collision != table);
}

void StorageHandler::removeTable(StorageTableShare *table) {
  Sync sync(&hashSyncObject, "StorageHandler::removeTable");
  sync.lock(Exclusive);
  int slot = JString::hash(table->pathName, tableHashSize);

  for (StorageTableShare **ptr = tables + slot; *ptr; ptr = &(*ptr)->collision)
    if (*ptr == table) {
      *ptr = table->collision;
      break;
    }
}

StorageConnection *StorageHandler::getStorageConnection(
    StorageTableShare *tableShare, THD *mySqlThread, int mySqlThdId,
    OpenOption createFlag) {
  Sync sync(&syncObject, "StorageHandler::getStorageConnection");

  if (!defaultDatabase) initialize();

  if (!dictionaryConnection) return NULL;

  if (!tableShare->storageDatabase) tableShare->findDatabase();

  StorageDatabase *storageDatabase = defaultDatabase;
  int slot = HASH(mySqlThread, connectionHashSize);
  StorageConnection *storageConnection;

  if (connections[slot]) {
    sync.lock(Shared);

    for (storageConnection = connections[slot]; storageConnection;
         storageConnection = storageConnection->collision)
      if (storageConnection->mySqlThread ==
          mySqlThread)  // && storageConnection->storageDatabase ==
                        // tableShare->storageDatabase)
      {
        storageConnection->addRef();

        if (!tableShare->storageDatabase)
          tableShare->setDatabase(storageDatabase);

        return storageConnection;
      }

    sync.unlock();
  }

  sync.lock(Exclusive);

  for (storageConnection = connections[slot]; storageConnection;
       storageConnection = storageConnection->collision)
    if (storageConnection->mySqlThread ==
        mySqlThread)  // && storageConnection->storageDatabase ==
                      // tableShare->storageDatabase)
    {
      storageConnection->addRef();

      if (!tableShare->storageDatabase)
        tableShare->setDatabase(storageDatabase);

      return storageConnection;
    }

  storageConnection =
      new StorageConnection(this, storageDatabase, mySqlThread, mySqlThdId);
  bool success = false;

  if (createFlag != CreateDatabase)  // && createFlag != OpenTemporaryDatabase)
    try {
      storageConnection->connect();
      success = true;
    } catch (SQLException &exception) {
      // fprintf(stderr, "database open failed: %s\n", exception.getText());
      storageConnection->setErrorText(exception.getText());

      if (createFlag == OpenDatabase) {
        delete storageConnection;

        return NULL;
      }
    }

  if (!success && createFlag != OpenDatabase) try {
      storageConnection->create();
    } catch (SQLException &) {
      delete storageConnection;

      return NULL;
    }

  tableShare->setDatabase(storageDatabase);
  storageConnection->collision = connections[slot];
  connections[slot] = storageConnection;

  return storageConnection;
}

StorageDatabase *StorageHandler::findDatabase(const char *dbName) {
  int slot = JString::hash(dbName, databaseHashSize);

  for (StorageDatabase *storageDatabase = storageDatabases[slot];
       storageDatabase; storageDatabase = storageDatabase->collision)
    if (storageDatabase->name == dbName) {
      storageDatabase->addRef();

      return storageDatabase;
    }

  return NULL;
}

void StorageHandler::changeMySqlThread(StorageConnection *storageConnection,
                                       THD *newThread) {
  Sync sync(&syncObject, "StorageHandler::changeMySqlThread");
  sync.lock(Exclusive);
  removeConnection(storageConnection);
  storageConnection->mySqlThread = newThread;
  int slot = HASH(storageConnection->mySqlThread, connectionHashSize);
  storageConnection->collision = connections[slot];
  connections[slot] = storageConnection;
}

void StorageHandler::removeConnection(StorageConnection *storageConnection) {
  int slot = HASH(storageConnection->mySqlThread, connectionHashSize);

  for (StorageConnection **ptr = connections + slot; *ptr;
       ptr = &(*ptr)->collision)
    if (*ptr == storageConnection) {
      *ptr = storageConnection->collision;
      break;
    }
}

int StorageHandler::closeConnections(THD *thd) {
  int slot = HASH(thd, connectionHashSize);
  Sync sync(&syncObject, "StorageHandler::closeConnections");
  sync.lock(Shared);

  for (StorageConnection *storageConnection = connections[slot], *next;
       storageConnection; storageConnection = next) {
    next = storageConnection->collision;

    if (storageConnection->mySqlThread == thd) {
      sync.unlock();

      storageConnection->close();

      if (storageConnection->mySqlThread)
        storageConnection
            ->release();  // This is for thd->ha_data[changjiang_hton->slot]

      storageConnection->release();  // This is for storageConn
    }
  }

  return 0;
}

int StorageHandler::dropDatabase(const char *path) {
  /***
  char pathname[FILENAME_MAX];
  const char *SEPARATOR = pathname;
  char *q = pathname;

  for (const char *p = path; *p;)
          {
          char c = *p++;

          if (c == '/')
                  {
                  if (*p == 0)
                          break;

                  SEPARATOR = q + 1;
                  }

          *q++ = c;
          }

  *q = 0;
  JString dbName = JString::upcase(SEPARATOR);
  strcpy(q, StorageTableShare::getDefaultRoot());
  StorageDatabase *storageDatabase = getStorageDatabase(dbName, pathname);
  databaseDropped(storageDatabase, NULL);

  try
          {
          storageDatabase->dropDatabase();
          }
  catch (SQLException&)
          {
          }

  storageDatabase->release();
  ***/

  return 0;
}

void StorageHandler::getIOInfo(InfoTable *infoTable) {
  Sync sync(&hashSyncObject, "StorageHandler::getIOInfo");
  sync.lock(Shared);

  for (StorageDatabase *storageDatabase = databaseList; storageDatabase;
       storageDatabase = storageDatabase->next)
    storageDatabase->getIOInfo(infoTable);
}

void StorageHandler::getMemoryDetailInfo(InfoTable *infoTable) {
  MemMgrAnalyze(MemMgrSystemDetail, infoTable);
}

void StorageHandler::getMemorySummaryInfo(InfoTable *infoTable) {
  MemMgrAnalyze(MemMgrSystemSummary, infoTable);
}

void StorageHandler::getRecordCacheDetailInfo(InfoTable *infoTable) {
  MemMgrAnalyze(MemMgrRecordDetail, infoTable);
}

void StorageHandler::getRecordCacheSummaryInfo(InfoTable *infoTable) {
  MemMgrAnalyze(MemMgrRecordSummary, infoTable);
}

void StorageHandler::getTransactionInfo(InfoTable *infoTable) {
  Sync sync(&hashSyncObject, "StorageHandler::getTransactionInfo");
  sync.lock(Shared);

  for (StorageDatabase *storageDatabase = databaseList; storageDatabase;
       storageDatabase = storageDatabase->next)
    storageDatabase->getTransactionInfo(infoTable);
}

void StorageHandler::getStreamLogInfo(InfoTable *infoTable) {
  Sync sync(&hashSyncObject, "StorageHandler::getStreamLogInfo");
  sync.lock(Shared);

  for (StorageDatabase *storageDatabase = databaseList; storageDatabase;
       storageDatabase = storageDatabase->next)
    storageDatabase->getStreamLogInfo(infoTable);
}

void StorageHandler::getSyncInfo(InfoTable *infoTable) {
  SyncObject::getSyncInfo(infoTable);
}

void StorageHandler::getTransactionSummaryInfo(InfoTable *infoTable) {
  Sync sync(&hashSyncObject, "StorageHandler::getTransactionSummaryInfo");
  sync.lock(Shared);

  for (StorageDatabase *storageDatabase = databaseList; storageDatabase;
       storageDatabase = storageDatabase->next)
    storageDatabase->getTransactionSummaryInfo(infoTable);
}

void StorageHandler::getTableSpaceInfo(InfoTable *infoTable) {
  Sync sync(&hashSyncObject, "StorageHandler::getTableSpaceInfo");
  sync.lock(Shared);

  for (StorageDatabase *storageDatabase = databaseList; storageDatabase;
       storageDatabase = storageDatabase->next)
    storageDatabase->getTableSpaceInfo(infoTable);
}

void StorageHandler::getTableSpaceFilesInfo(InfoTable *infoTable) {
  Sync sync(&hashSyncObject, "StorageHandler::getTableSpaceFilesInfo");
  sync.lock(Shared);

  for (StorageDatabase *storageDatabase = databaseList; storageDatabase;
       storageDatabase = storageDatabase->next)
    storageDatabase->getTableSpaceFilesInfo(infoTable);
}

void StorageHandler::initialize(void) {
  if (initialized) return;

  Sync sync(&syncObject, "StorageHandler::initialize");
  sync.lock(Exclusive);

  if (initialized) return;

  initialized = true;
  defaultDatabase = getStorageDatabase(MASTER_NAME, MASTER_PATH);

  try {
    dictionaryConnection = defaultDatabase->getOpenConnection();
    dropTempTables();
    dictionaryConnection->commit();
  } catch (SQLException &e) {
    int err = e.getSqlcode();

    // If got one of following errors, just rethrow. No point in
    // trying to create database.
    if (err != OPEN_MASTER_ERROR) throw;

    try {
      Log::log(LogMysqlInfo, "Changjiang: unable to open system data files.");
      Log::log(LogMysqlInfo, "Changjiang: creating new system data files.");
      createDatabase();
    } catch (SQLException &e2) {
      deleteFilesOnExit = true;  // Cleanup created files

      throw SQLError(e2.getSqlcode(),
                     "create database failed : %s \n"
                     "prior open database failure was: %s",
                     e2.getText(), e.getText());
    } catch (...) {
      deleteFilesOnExit = true;
    }
  }
}

void StorageHandler::createDatabase(void) {
  // Check all files are properly opened with O_CREAT
  inCreateDatabase = true;
  defaultDatabase->createDatabase();
  dictionaryConnection = defaultDatabase->getOpenConnection();
  Statement *statement = dictionaryConnection->createStatement();
  JString createTableSpace =
      genCreateTableSpace(DEFAULT_TABLESPACE, CHANGJIANG_USER);
  statement->executeUpdate(createTableSpace);

  for (const char **ddl = changjiangSchema; *ddl; ++ddl)
    statement->executeUpdate(*ddl);
  statement->close();
  dictionaryConnection->commit();

  Database *database = dictionaryConnection->database;
  database->waitForWriteComplete(NULL);
  database->flush((int64)0);

  inCreateDatabase = false;
}

void StorageHandler::dropTempTables(void) {
  Statement *statement = dictionaryConnection->createStatement();

  try {
    PStatement select = dictionaryConnection->prepareStatement(
        "select schema,tablename from system.tables where "
        "tablespace='" TEMPORARY_TABLESPACE "'");
    RSet resultSet = select->executeQuery();
    bool hit = false;

    while (resultSet->next()) {
      CmdGen gen;
      gen.gen("drop table \"%s\".\"%s\"", resultSet->getString(1),
              resultSet->getString(2));
      statement->executeUpdate(gen.getString());
      hit = true;
    }

    // if (hit)
    // statement->executeUpdate(dropTempSpace);
  } catch (...) {
  }

  try {
    statement->executeUpdate(createTempSpace);
  } catch (SQLException &exception) {
    Log::log("Can't create temporary tablespace: %s\n", exception.getText());
  }

  statement->close();
}

void StorageHandler::setRecordMemoryMax(uint64 value) {
  if (dictionaryConnection) dictionaryConnection->setRecordMemoryMax(value);
}

void StorageHandler::setRecordScavengeThreshold(int value) {
  if (dictionaryConnection)
    dictionaryConnection->setRecordScavengeThreshold(value);
}

void StorageHandler::setRecordScavengeFloor(int value) {
  if (dictionaryConnection) dictionaryConnection->setRecordScavengeFloor(value);
}

void StorageHandler::setIndexChillThreshold(uint value) {
  if (dictionaryConnection) dictionaryConnection->setIndexChillThreshold(value);
}

void StorageHandler::setRecordChillThreshold(uint value) {
  if (dictionaryConnection)
    dictionaryConnection->setRecordChillThreshold(value);
}

void StorageHandler::cleanFileName(const char *pathname, char *filename,
                                   int filenameLength) {
  char c, prior = 0;
  char *q = filename;
  char *end = filename + filenameLength - 1;
  filename[0] = 0;

  for (const char *p = pathname; q < end && (c = *p++); prior = c)
    if (c != SEPARATOR || c != prior) *q++ = c;

  *q = 0;
}

void StorageHandler::getChangjiangVersionInfo(InfoTable *infoTable) {
  int n = 0;
  infoTable->putString(n++, CHANGJIANG_VERSION);
  infoTable->putString(n++, CHANGJIANG_DATE);
  infoTable->putRecord();
}

int StorageHandler::recoverGetNextLimbo(int xidLength, unsigned char *xid) {
  if (!defaultDatabase) initialize();

  if (Connection *connection = dictionaryConnection)
    return connection->recoverGetNextLimbo(xidLength, xid);

  return 0;
}

const char *StorageHandler::normalizeName(const char *name, int bufferSize,
                                          char *buffer) {
  char *q = buffer;
  char *end = buffer + bufferSize - 1;

  for (const char *p = name; *p && q < end; ++p)
    if (charTable[(unsigned char)*p])
      return name;
    else
      *q++ = UPPER(*p);

  *q = 0;

  return buffer;
}

}  // namespace Changjiang
