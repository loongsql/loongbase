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

// Record.h: interface for the Record class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#define ALLOCATE_RECORD(n) (char *)MemMgrRecordAllocate(n, __FILE__, __LINE__)
#define DELETE_RECORD(record) MemMgrRecordDelete(record);

#include "SynchronizationObject.h"
#include "SyncObject.h"

namespace Changjiang {

enum RecordEncoding {
  noEncoding = 0,
  traditional,
  valueVector,
  byteVector,
  shortVector,
  longVector
};

// Record states

static const int recData = 0;  // record pointer is valid or record is deleted
static const int recDeleted = 1;  // record has been deleted
static const int recChilled =
    2;  // record data is temporarily stored in stream log
static const int recOnDisk = 3;    // record is on disk and must be read
static const int recLock = 4;      // this is a "record lock" and not a record
static const int recNoChill = 5;   // record is in use and should not be chilled
static const int recRollback = 6;  // record is being rolled back
static const int recUnlocked = 7;  // record is being unlocked
static const int recInserting = 8;  // record is being physically inserted
static const int recDeleting = 9;   // record is being physically deleted
static const int recPruning = 10;   // record is being pruned
static const int recEndChain = 11;  // end of chain for garbage collection
static const int recQueuedForDelete =
    12;  // end of chain for garbage collection

//#define CHECK_RECORD_ACTIVITY
#ifdef CHECK_RECORD_ACTIVITY
#define SET_THIS_RECORD_ACTIVE(_tf_) \
  { active = _tf_; }
#define SET_RECORD_ACTIVE(_rec_, _tf_) \
  { (_rec_)->active = _tf_; }
#else
#define SET_THIS_RECORD_ACTIVE(_tf_) \
  {}
#define SET_RECORD_ACTIVE(_rec_, _tf_) \
  {}
#endif

//#define COLLECT_RECORD_HISTORY
#if defined COLLECT_RECORD_HISTORY
#define REC_HISTORY __FILE__, __LINE__
#define RECORD_HISTORY(_rec_)                              \
  {                                                        \
    if (_rec_) (_rec_)->addHistory(0, __FILE__, __LINE__); \
  }
#define MAX_RECORD_HISTORY 100
#define RECORD_HISTORY_FILE_LEN 16

struct record_history {
  unsigned long threadId;
  unsigned long counter;
  long useCount;
  short delta;
  short line;
  UCHAR state;
  char file[RECORD_HISTORY_FILE_LEN];
};
#else
#define REC_HISTORY
#define RECORD_HISTORY(_rec_) \
  {}
#endif

class Format;
class Table;
class Transaction;
class TransactionState;
class Value;
class Stream;
class Database;
class RecordScavenge;
class Serialize;
class SyncObject;
CLASS(Field);

extern char *RecordAllocate(int size, const char *file, int line);
extern void RecordDelete(char *record);

class Record {
 public:
  // virtual Transaction* getTransaction();
  virtual TransactionState *getTransactionState() const;
  virtual TransId getTransactionId();
  virtual int getSavePointId();
  virtual void setSuperceded(bool flag);
  virtual Record *fetchVersion(Transaction *transaction);
  virtual void retire(void);
  virtual void scavenge(TransId targetTransactionId,
                        int oldestActiveSavePointId);
  virtual bool isVersion();
  virtual bool isSuperceded();
  virtual bool isNull(int fieldId);
  virtual Record *releaseNonRecursive(void);
  virtual Record *clearPriorVersion(void);
  virtual void setPriorVersion(Record *oldPriorVersion,
                               Record *newPriorVersion);
  virtual Record *getPriorVersion();
  virtual Record *getGCPriorVersion(void);
  virtual void print(void);
  virtual int thaw(void);
  virtual const char *getEncodedRecord();
  virtual int setRecordData(const UCHAR *dataIn, int dataLength);
  virtual void serialize(Serialize *stream);
  virtual int getSize(void);
  virtual SyncObject *getSyncThaw(void);
  virtual void queueForDelete(void);

  const UCHAR *getEncoding(int index);
  int setEncodedRecord(Stream *stream, bool interlocked);
  void getValue(int fieldId, Value *value);
  void getRawValue(int fieldId, Value *value);
  int getFormatVersion();
  void setValue(TransactionState *transaction, int id, Value *value,
                bool cloneFlag, bool copyFlag);
  void poke();
  void release();
  void addRef();
  int getBlobId(int fieldId);
  void finalize(Transaction *transaction);
  void getEncodedValue(int fieldId, Value *value);
  bool getRecord(Stream *stream);
  int getEncodedSize();
  void deleteData(void);
  void deleteData(bool now);
  void printRecord(const char *header);
  void validateData(void);
  char *allocRecordData(int length);
  void expungeRecord(void);
  int getDataMemUsage(void);
  int getMemUsage(void);

  Record(Table *table, Format *recordFormat);
  Record(Table *table, int32 recordNumber, Stream *stream);
  Record(Database *database, Serialize *stream);

  inline int hasRecord(bool forceThaw = true) {
    if (state == recChilled && forceThaw) thaw();

    return (data.record != NULL);
  }

  inline char *getRecordData() {
    if (state == recChilled) thaw();

    return data.record;
  }

 protected:
  virtual ~Record();

  struct {
    char *record;
  } data;

 public:
  uint64 generation;
  Format *format;
  volatile INTERLOCK_TYPE useCount;
  int recordNumber;
  int size;
  short highWater;
  UCHAR encoding;
  UCHAR state;

#ifdef CHECK_RECORD_ACTIVITY
  UCHAR active;  // this is for debugging only
#endif

#ifdef COLLECT_RECORD_HISTORY
  void ShowHistory(void);
  void addHistory(int delta, const char *file, int line);
  void addRef(const char *file, int line);
  void release(const char *file, int line);

  SyncObject syncHistory;
  uint historyCount;
  struct record_history history[MAX_RECORD_HISTORY];
#endif
};

}  // namespace Changjiang
