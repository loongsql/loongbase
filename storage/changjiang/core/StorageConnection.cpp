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
#include "StorageConnection.h"
#include "StorageTable.h"
#include "StorageTableShare.h"
#include "StorageDatabase.h"
#include "StorageHandler.h"
#include "Connection.h"
#include "Database.h"
#include "SyncObject.h"
#include "Sync.h"
#include "SQLError.h"
#include "Threads.h"
#include "Thread.h"
#include "Stream.h"
#include "Transaction.h"

namespace Changjiang {

class Server;
extern Server *startServer(int port, const char *configFile);

// From handler.h

enum enum_tx_isolation {
  ISO_READ_UNCOMMITTED,
  ISO_READ_COMMITTED,
  ISO_REPEATABLE_READ,
  ISO_SERIALIZABLE
};

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

StorageConnection::StorageConnection(StorageHandler *handler,
                                     StorageDatabase *db, THD *mySqlThd,
                                     int mySqlThdId) {
  storageHandler = handler;
  mySqlThread = mySqlThd;
  connection = NULL;
  transactionActive = false;
  useCount = 1;
  implicitTransactionCount = 0;
  verbMark = 0;
  prepared = false;
  storageDatabase = db;
  mySqlThreadId = mySqlThdId;
  storageDatabase->addRef();

#ifdef TRACE_TRANSACTIONS
  traceStream = new Stream;
#else
  traceStream = NULL;
#endif
}

StorageConnection::~StorageConnection(void) {
  disconnect();
  delete traceStream;
}

/***
StorageTable* StorageConnection::getStorageTable(const char* name, int
impureSize, bool tempTable)
{
        if (!storageDatabase)
                return NULL;

        StorageTableShare *share = storageDatabase->getTableShare(name,
impureSize, tempTable);

        return getStorageTable(share);
}
***/

StorageTable *StorageConnection::getStorageTable(StorageTableShare *share) {
  StorageTable *table = new StorageTable(this, share);

  return table;
}

void StorageConnection::connect(void) {
  connection = storageDatabase->getOpenConnection();
  connection->mySqlThreadId = mySqlThreadId;
  database = connection->database;
}

void StorageConnection::create(void) {
  connection = storageDatabase->createDatabase();
  database = connection->database;
}

bool StorageConnection::matches(const char *pathname) {
  const char *p = pathname;
  const char *q = path;

  for (; *p && *q && *p == *q; ++p, ++q)
    ;

  return *q == 0 && *p == '/';
}

void StorageConnection::remove(StorageTable *storageTable) { release(); }

void StorageConnection::close(void) { release(); }

int StorageConnection::commit(void) {
  int errorCode = 0;

  if (connection) {
    if (traceStream) {
      if (connection->transaction)
        storageDatabase->traceTransaction(
            connection->transaction->transactionId, true, 0, traceStream);

      traceStream->clear();
    }

    try {
      connection->commit();
    } catch (SQLException &exception) {
      errorCode = translateError(exception, StorageErrorIOErrorStreamLog);
    }
  }

  transactionActive = false;
  implicitTransactionCount = 0;
  verbMark = 0;

  return errorCode;
}

int StorageConnection::prepare(int xidLength, const UCHAR *xid) {
  if (connection) connection->prepare(xidLength, xid);

  prepared = true;

  return 0;
}

int StorageConnection::rollback(void) {
  int errorCode = 0;

  if (connection) {
    if (traceStream) {
      if (connection->transaction)
        storageDatabase->traceTransaction(
            connection->transaction->transactionId, false,
            connection->transaction->blockedBy, traceStream);

      traceStream->clear();
    }

    try {
      connection->rollback();
    } catch (SQLException &exception) {
      errorCode = translateError(exception, StorageErrorIOErrorStreamLog);
    }
  }

  transactionActive = false;
  implicitTransactionCount = 0;
  verbMark = 0;

  return errorCode;
}

int StorageConnection::startTransaction(int isolationLevel) {
  if (transactionActive) return false;

  if (connection) connection->setTransactionIsolation(isolationLevel);

  transactionActive = true;
  prepared = false;

  return true;
}

int StorageConnection::savepointSet() {
  return storageDatabase->savepointSet(connection);
}

int StorageConnection::savepointRelease(int savePoint) {
  return storageDatabase->savepointRelease(connection, savePoint);
}

int StorageConnection::savepointRollback(int savePoint) {
  return storageDatabase->savepointRollback(connection, savePoint);
}

void StorageConnection::addRef(void) { ++useCount; }

void StorageConnection::release(void) {
  if (--useCount == 0) {
    /*
    Temporary: Do not always delete the StorageConnection.  It may have a
    transaction object that is associated with a particular MySqlThread.
    But Changjiang does not have a session object yet,  so if MySQL reuses the
    NfsStorageConnection for a different MySqlThread, which happens often,
    then Changjiang may not have any pointers to the StorageConnection object.
    So let these leak for now, until we have a storageSession class.
    */

    if (mySqlThread == NULL) delete this;
  }
}

void StorageConnection::dropDatabase(void) {
  storageHandler->databaseDropped(storageDatabase, this);
  storageDatabase->dropDatabase();
  disconnect();
}

const char *StorageConnection::findNameSegment(const char *buffer,
                                               const char *tail) {
  const char *p = tail;

  while (p > buffer && p[-1] != '/' && p[-1] != '\\') --p;

  return p;
}

const char *StorageConnection::skipSeparator(const char *buffer,
                                             const char *tail) {
  const char *p = tail;

  while (p > buffer && (p[-1] == '/' || p[-1] == '\\')) --p;

  return p;
}

/***
void StorageConnection::shutdown(void)
{
}
***/

void StorageConnection::expunge(void) { delete this; }

void StorageConnection::databaseDropped(StorageDatabase *database) {
  if (database == storageDatabase) disconnect();
}

void StorageConnection::validate(void) { storageDatabase->validateCache(); }

void StorageConnection::disconnect(void) {
  storageHandler->remove(this);

  if (storageDatabase) {
    storageDatabase->release();
    storageDatabase = NULL;
  }

  if (connection) {
    connection->release();
    connection = NULL;
  }

  Thread::deleteThreadObject();
}

int StorageConnection::startImplicitTransaction(int isolationLevel) {
  ++implicitTransactionCount;

  if (!transactionActive) {
    startTransaction(isolationLevel);
    transactionActive = true;

    return true;
  }

  return false;
}

int StorageConnection::endImplicitTransaction(void) {
  int errorCode = 0;

  if (implicitTransactionCount > 0 && --implicitTransactionCount == 0) {
    errorCode = commit();
  }

  return errorCode;
}

int StorageConnection::markVerb(void) {
  if (!verbMark) {
    verbMark = savepointSet();

    return true;
  }

  return false;
}

int StorageConnection::rollbackVerb(void) {
  if (verbMark) {
    try {
      savepointRollback(verbMark);
      verbMark = 0;
    } catch (SQLException &exception) {
      return translateError(exception, StorageErrorIOErrorStreamLog);
    }
  }

  return 0;
}

void StorageConnection::releaseVerb(void) {
  if (verbMark) {
    savepointRelease(verbMark);
    verbMark = 0;
  }

  if (connection) connection->setCurrentStatement(NULL);
}

void StorageConnection::setErrorText(const char *text) {
  lastErrorText.setString(text);
}

const char *StorageConnection::getLastErrorString(void) {
  return lastErrorText;
}

int StorageConnection::setErrorText(SQLException *exception) {
  setErrorText(exception->getText());

  return exception->getSqlcode();
}

int StorageConnection::getMaxKeyLength(void) {
  return database->getMaxKeyLength();
}

void StorageConnection::setMySqlThread(THD *thd) {
  storageHandler->changeMySqlThread(this, thd);
}

void StorageConnection::setCurrentStatement(const char *text) {
  if (connection) connection->setCurrentStatement(text);

  if (traceStream && text) {
    traceStream->putSegment(text);
    traceStream->putCharacter('\n');
  }
}

Transaction *StorageConnection::getTransaction(void) {
  return connection->getTransaction();
}

void StorageConnection::validate(int options) {
  int flags = 0;

  if (options & VALIDATE_REPAIR) flags |= validateRepair;

  connection->validate(flags);
}

int StorageConnection::translateError(SQLException &exception,
                                      int defaultStorageError) {
  // This method is inspired by the corresponding method in
  // StorageTable::translateError.

  int errorCode;
  int sqlCode = exception.getSqlcode();

  switch (sqlCode) {
    case IO_ERROR_STREAMLOG:
      errorCode = StorageErrorIOErrorStreamLog;
      break;

    default:
      errorCode = defaultStorageError;
  }

  setErrorText(&exception);

  return errorCode;
}

}  // namespace Changjiang
