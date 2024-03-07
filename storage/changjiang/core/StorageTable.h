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

#include "EncodedDataStream.h"
#include "Stream.h"
#include "IndexKey.h"
#include "SQLException.h"

namespace Changjiang {

static const int UpperBound = 1;
static const int LowerBound = 2;

static const int MaxRetryAferWait = 5;

struct StorageKey {
  int numberSegments;
  IndexKey indexKey;
};

struct StorageBlob {
  unsigned int length;
  unsigned char *data;
  int blobId;
  StorageBlob *next;
};

#ifdef TRACK_RECORDS
class Record;
class Transaction;

static const int recordHistorySize = 10;

struct RecordHistory {
  Record *record;
  Transaction *transaction;
  uint transactionId;
  int recordNumber;
};
#endif

class StorageConnection;
class StorageTableShare;
class StorageInterface;
class StorageDatabase;
class Index;
class IndexWalker;
class Record;
class SyncObject;
class Format;
class IndexWalker;
class StorageIndexDesc;

class StorageTable {
 public:
  StorageTable(StorageConnection *connection, StorageTableShare *tableShare);
  virtual ~StorageTable(void);

  void transactionEnded(void);
  void setRecord(Record *record, bool locked);
  int alterCheck(void);
  void waitForWriteComplete();
  void clearAlter(void);
  bool setAlter(void);

  virtual void setConnection(StorageConnection *connection);
  virtual void clearIndexBounds(void);
  virtual void clearRecord(void);
  virtual void clearBitmap(void);
  virtual void clearStatement(void);
  virtual int create(const char *sql, int64 autoIncrementValue);
  virtual int upgrade(const char *sql, int64 autoIncrementValue);
  virtual int open(void);
  virtual int deleteTable(void);
  virtual int deleteRow(int recordNumber);
  virtual int truncateTable(void);
  virtual int indexScan(int indexOrder);
  virtual int setCurrentIndex(int indexId);
  virtual int clearCurrentIndex();
  virtual int checkCurrentIndex();
  virtual int setIndex(StorageIndexDesc *indexDesc);
  virtual void indexEnd(void);
  virtual int setIndexBound(const unsigned char *key, int keyLength, int which);
  virtual int storeBlob(StorageBlob *blob);
  virtual void getBlob(int recordNumber, StorageBlob *blob);
  virtual void release(StorageBlob *blob);
  virtual void deleteStorageTable(void);
  virtual void freeBlob(StorageBlob *blob);
  virtual void preInsert(void);
  virtual int insert(void);

  virtual int next(int recordNumber, bool lockForUpdate);
  virtual int nextIndexed(int recordNumber, bool lockForUpdate);
  virtual int fetch(int recordNumber, bool lockForUpdate);

  virtual int updateRow(int recordNumber);
  virtual int createIndex(StorageIndexDesc *indexDesc, const char *sql);
  virtual int dropIndex(StorageIndexDesc *indexDesc, const char *sql,
                        bool online);
  virtual const unsigned char *getEncoding(int fieldIndex);
  virtual const char *getName(void);
  virtual const char *getSchemaName(void);
  virtual const char *getTableSpaceName(void);
  virtual int compareKey(const unsigned char *key, int keyLength);
  virtual int translateError(SQLException *exception, int defaultStorageError);
  virtual int isKeyNull(const unsigned char *key, int keyLength);
  virtual void setPartialKey(void);
  virtual void setReadBeforeKey(void);
  virtual void setReadAfterKey(void);
  virtual void unlockRow(void);
  virtual int optimize(void);
  virtual void setLocalTable(StorageInterface *handler);

  JString name;
  StorageTable *collision;
  StorageConnection *storageConnection;
  StorageDatabase *storageDatabase;
  StorageTableShare *share;
  StorageInterface *localTable;
  StorageIndexDesc *currentIndex;

#ifdef TRACK_RECORDS
  RecordHistory recordHistory[recordHistorySize];
#endif

  int historyIndex;
  void *bitmap;
  IndexWalker *indexWalker;
  StorageKey lowerKey;
  StorageKey upperKey;
  StorageKey *lowerBound;
  StorageKey *upperBound;
  Record *record;
  Format *format;
  EncodedDataStream dataStream;
  Stream insertStream;
  int searchFlags;
  bool recordLocked;
  bool indexesLocked;
};

}  // namespace Changjiang
