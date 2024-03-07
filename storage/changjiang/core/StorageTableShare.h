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

#include "JString.h"
#include "SyncObject.h"

namespace Changjiang {

#ifndef _WIN32
#define __int64 long long
#endif

typedef __int64 INT64;

static const int MaxIndexSegments = 16;
static const int indexNameSize = 257;

class StorageDatabase;
class StorageConnection;
class StorageHandler;
class Table;
class Index;
class SyncObject;
class Sequence;
class SyncObject;
class Format;

enum KeyFormat {
  KEY_FORMAT_LONG_INT = 1,
  KEY_FORMAT_SHORT_INT,
  KEY_FORMAT_LONGLONG,
  KEY_FORMAT_ULONGLONG,
  KEY_FORMAT_FLOAT,
  KEY_FORMAT_DOUBLE,
  KEY_FORMAT_VARBINARY,
  KEY_FORMAT_VARTEXT,
  KEY_FORMAT_BINARY_STRING,
  KEY_FORMAT_BINARY_NEWDECIMAL,
  KEY_FORMAT_BINARY_INTEGER,
  KEY_FORMAT_TEXT,
  KEY_FORMAT_ULONG_INT,
  KEY_FORMAT_TIMESTAMP,
  KEY_FORMAT_INT8,
  KEY_FORMAT_USHORT_INT,
  KEY_FORMAT_UINT24,
  KEY_FORMAT_INT24,
  KEY_FORMAT_OTHER
};

// Used for float datetime/time/timestamp
typedef int64 (*mysql_to_cj_converter)(const char *keyBytes);

struct StorageSegment {
  short type;
  KeyFormat keyFormat;
  short nullPosition;
  int offset;
  int length;
  unsigned char nullBit;
  void *mysql_charset;
  mysql_to_cj_converter type_converter = nullptr;
};

// StorageIndexDesc maps a server-side index to a Changjiang index
class StorageIndexDesc {
 public:
  StorageIndexDesc();
  StorageIndexDesc(const StorageIndexDesc *indexDesc);
  virtual ~StorageIndexDesc(void);
  bool operator==(const StorageIndexDesc &indexDesc) const;
  bool operator!=(const StorageIndexDesc &indexDesc) const;

  int id;
  int unique;
  int primaryKey;
  int numberSegments;
  char name[indexNameSize];     // clean name
  char rawName[indexNameSize];  // original name
  Index *index;
  uint64 *segmentRecordCounts;
  StorageSegment segments[MaxIndexSegments];
  StorageIndexDesc *next;
  StorageIndexDesc *prev;
};

enum StorageError {
  StorageErrorRecordNotFound = -1,
  StorageErrorDupKey = -2,
  StorageErrorTableNotFound = -3,
  StorageErrorNoIndex = -4,
  StorageErrorBadKey = -5,
  StorageErrorTableExits = -6,
  StorageErrorNoSequence = -7,
  StorageErrorUpdateConflict = -8,
  StorageErrorUncommittedUpdates = -9,  // specific for drop table
  StorageErrorDeadlock = -10,
  StorageErrorTruncation = -11,
  StorageErrorUncommittedRecords = -12,  // more general; used for alter table
  StorageErrorIndexOverflow = -13,       // key value is too long
  StorageWarningSerializable = -101,
  StorageWarningReadUncommitted = -102,
  StorageErrorTablesSpaceOperationFailed = -103,
  StorageErrorOutOfMemory =
      -104,  // memory pool limit reached or system memory exhausted
  StorageErrorOutOfRecordMemory =
      -105,  // memory pool limit reached or system memory exhausted
  StorageErrorLockTimeout = -106,
  StorageErrorTableSpaceExist = -107,
  StorageErrorTableNotEmpty = -108,
  StorageErrorTableSpaceNotExist = -109,
  StorageErrorDeviceFull = -110,
  StorageErrorTableSpaceDataFileExist = -111,
  StorageErrorIOErrorStreamLog = -112,
  StorageErrorInternalError = -113
};

static const int StoreErrorIndexShift = 10;
static const int StoreErrorIndexMask = 0x3ff;

class StorageTableShare {
 public:
  // StorageTableShare(StorageDatabase *db, const char *tableName, const char
  // *schemaName, int impureSize);
  StorageTableShare(StorageHandler *handler, const char *path,
                    const char *tableSpaceName, int lockSize, bool tempTbl);
  virtual ~StorageTableShare(void);

  virtual void lock(bool exclusiveLock = false);
  virtual void unlock(void);
  virtual void lockIndexes(bool exclusiveLock = false);
  virtual void unlockIndexes(void);
  virtual int createIndex(StorageConnection *storageConnection,
                          StorageIndexDesc *indexDesc, const char *sql);
  virtual int dropIndex(StorageConnection *storageConnection,
                        StorageIndexDesc *indexDesc, const char *sql,
                        bool online);
  virtual bool validateIndex(int indexId, StorageIndexDesc *indexTarget);
  virtual void deleteIndexes();
  virtual int numberIndexes();
  virtual int renameTable(StorageConnection *storageConnection,
                          const char *newName);
  virtual INT64 getSequenceValue(int delta);
  virtual int setSequenceValue(INT64 value);
  virtual int haveIndexes(int indexCount);
  virtual const char *cleanupFieldName(const char *name, char *buffer,
                                       int bufferLength, bool doubleQuotes);
  virtual void setTablePath(const char *path, bool tempTable);
  virtual void registerCollation(const char *collationName, void *arg);

  int open(void);
  void addIndex(StorageIndexDesc *indexDesc);
  void deleteIndex(int indexId);
  int setIndex(const StorageIndexDesc *indexInfo);
  void clearIndex(StorageIndexDesc *indexDesc);
  StorageIndexDesc *getIndex(int indexId);
  StorageIndexDesc *getIndex(int indexId, StorageIndexDesc *indexDesc);
  int getIndexId(const char *schemaName, const char *indexName);
  int create(StorageConnection *storageConnection, const char *sql,
             int64 autoIncrementValue);
  int upgrade(StorageConnection *storageConnection, const char *sql,
              int64 autoIncrementValue);
  int deleteTable(StorageConnection *storageConnection);
  int truncateTable(StorageConnection *storageConnection);
  void load(void);
  void registerTable(void);
  void unRegisterTable(void);
  void setPath(const char *path);
  void getDefaultPath(char *buffer);
  void findDatabase(void);
  void setDatabase(StorageDatabase *db);
  uint64 estimateCardinality(void);
  bool tableExists(void);
  JString lookupPathName(void);

  static const char *getDefaultRoot(void);
  static const char *cleanupTableName(const char *name, char *buffer,
                                      int bufferLength, char *schema,
                                      int schemaLength);
  char *createIndexName(const char *rawName, char *indexName,
                        bool primary = false);

  JString name;
  JString schemaName;
  JString pathName;
  JString tableSpace;
  JString givenName;
  JString givenSchema;
  StorageTableShare *collision;
  unsigned char *impure;
  int initialized;
  SyncObject *syncObject;
  SyncObject *syncIndexMap;
  StorageDatabase *storageDatabase;
  StorageHandler *storageHandler;
  Table *table;
  StorageIndexDesc *indexes;
  Sequence *sequence;
  Format *format;  // format for insertion
  bool tempTable;
  int getFieldId(const char *fieldName);
};

}  // namespace Changjiang
