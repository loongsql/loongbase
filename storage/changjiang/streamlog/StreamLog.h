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

// StreamLog.h: interface for the StreamLog class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "SyncObject.h"
#include "Schedule.h"
#include "Stack.h"
#include "DenseArray.h"
#include "Queue.h"
#include "Bitmap.h"

namespace Changjiang {

static const unsigned int altLogFlag = 0x80000000;
static const int srlSignature = 123456789;
static const int SLT_HASH_SIZE = 1001;
static const int SRL_WINDOW_SIZE = 1048576;
#define SRL_MIN_WINDOWS 10

// States for recovery objects

static const int objUnknown = 0;
static const int objInUse = 1;
static const int objDeleted = 2;

struct StreamLogBlock {
  uint64 blockNumber;
  uint64 readBlockNumber;
  uint32 length;
  uint32 creationTime;
  uint16 version;
  UCHAR data[1];
};

struct TableSpaceInfo {
  int tableSpaceId;
  TableSpaceInfo *collision;
  TableSpaceInfo *next;
  DenseArray<volatile long, 200> sectionUseVector;
  DenseArray<volatile long, 200> indexUseVector;
};

class StreamLogFile;
class Database;
class Dbb;
class Thread;
class StreamLogControl;
class StreamLogWindow;
class StreamLogTransaction;
class Bitmap;
class IO;
class RecoveryObjects;
class Sync;
class Transaction;
class InfoTable;
class TableSpaceManager;
class Porpoise;

class StreamLog : public Schedule, public SyncObject {
 public:
  StreamLog(Database *db, JString schedule, int maxTransactionBacklog);
  virtual ~StreamLog();

  void putVersion();
  void pageCacheFlushed(int64 flushArg);

  void releaseBuffer(UCHAR *buffer);
  UCHAR *allocBuffer();
  void shutdownNow();
  void dropDatabase();
  uint32 appendLog(IO *shadow, int lastPage);
  bool isSectionActive(int sectionId, int tableSpaceId);
  bool isIndexActive(int indexId, int tableSpaceId);

  StreamLogBlock *findLastBlock(StreamLogWindow *window);
  void initializeWriteBlock(StreamLogBlock *block);
  void checkpoint(bool force);
  virtual void execute(Scheduler *scheduler);
  void transactionDelete(StreamLogTransaction *transaction);
  uint64 getReadBlock();
  StreamLogTransaction *getTransaction(TransId transactionId);
  StreamLogTransaction *findTransaction(TransId transactionId);
  StreamLogWindow *findWindowGivenOffset(uint64 virtualOffset);
  StreamLogWindow *findWindowGivenBlock(uint64 blockNumber);
  StreamLogWindow *allocWindow(StreamLogFile *file, int64 origin);
  void initializeLog(int64 blockNumber);
  void release(StreamLogWindow *window);
  void wakeup();
  void startRecord();
  void putData(uint32 length, const UCHAR *data);
  void close();
  void shutdown();
  uint64 flush(bool forceNewWindow, uint64 commitBlockNumber, Sync *syncPtr);
  void recover();
  void start();
  void open(JString fileRoot, bool createFlag);
  void copyClone(JString fileRoot, int logOffset, int logLength);
  int recoverLimboTransactions(void);
  int recoverGetNextLimbo(int xidSize, unsigned char *xid);

  void preFlush(void);
  void wakeupFlushQueue(Thread *ourThread);

  void setSectionActive(int id, int tableSpaceId);
  void setSectionInactive(int id, int tableSpaceId);
  void setIndexActive(int id, int tableSpaceId);
  void setIndexInactive(int id, int tableSpaceId);
  void setOverflowPageValid(int pageNumber, int tableSpaceId);
  void setOverflowPageInvalid(int pageNumber, int tableSpaceId);
  bool isOverflowPageValid(int pageNumber, int tableSpaceId);
  void setTableSpaceDropped(int tableSpaceId);
  bool isTableSpaceDropped(int tableSpaceId);
  void updateSectionUseVector(uint sectionId, int tableSpaceId, int delta);
  void updateIndexUseVector(uint indexId, int tableSpaceId, int delta);
  bool sectionInUse(int sectionId, int tableSpaceId);
  bool bumpIndexIncarnation(int indexId, int tableSpaceId, int state);
  bool bumpSectionIncarnation(int sectionId, int tableSpaceId, int state);
  bool bumpPageIncarnation(int32 pageNumber, int tableSpaceId, int state);

  void redoFreePage(int32 pageNumber, int tableSpaceId);

  bool indexInUse(int indexId, int tableSpaceId);
  void overflowFlush(void);
  void createNewWindow(void);
  void setPhysicalBlock(TransId transactionId);
  void reportStatistics(void);
  void getStreamLogInfo(InfoTable *tableInfo);
  void commitByXid(int xidLength, const UCHAR *xid);
  void rollbackByXid(int xidLength, const UCHAR *xid);
  void preCommit(Transaction *transaction);
  void printWindows(void);
  void endRecord(void);
  Dbb *getDbb(int tableSpaceId);
  TableSpaceInfo *getTableSpaceInfo(int tableSpaceId);
  void preUpdate(void);
  Dbb *findDbb(int tableSpaceId);
  uint64 getWriteBlockNumber(void);
  void unblockUpdates(void);
  void blockUpdates(void);
  int getBlockSize(void);
  StreamLogWindow *setWindowInterest(void);
  void setWriteError(int sqlCode, const char *errorText);

  TableSpaceManager *tableSpaceManager;
  StreamLogFile *file1;
  StreamLogFile *file2;
  StreamLogWindow *firstWindow;
  StreamLogWindow *lastWindow;
  StreamLogWindow *freeWindows;
  StreamLogWindow *writeWindow;
  StreamLogTransaction *transactions[SLT_HASH_SIZE];
  StreamLogTransaction *nextLimboTransaction;
  Database *database;
  RecoveryObjects *recoveryPages;
  RecoveryObjects *recoverySections;
  RecoveryObjects *recoveryIndexes;
  RecoveryObjects *recoveryOverflowPages;
  Bitmap droppedTablespaces;
  Dbb *defaultDbb;
  Porpoise *porpoises;
  Thread *srlQueue;
  Thread *endSrlQueue;
  StreamLogControl *logControl;
  uint64 nextBlockNumber;
  uint64 highWaterBlock;
  uint64 lastFlushBlock;
  uint64 lastReadBlock;
  uint64 recoveryBlockNumber;
  uint64 lastBlockWritten;
  UCHAR *recordStart;
  UCHAR *writePtr;
  UCHAR *writeWarningTrack;
  StreamLogBlock *writeBlock;
  SyncObject syncWrite;
  SyncObject syncSections;
  SyncObject syncIndexes;
  SyncObject syncPorpoise;
  SyncObject syncUpdateStall;
  Stack buffers;
  UCHAR *bufferSpace;
  time_t creationTime;
  bool active;
  bool pass1;
  volatile bool finishing;
  bool recordIncomplete;
  bool recovering;
  bool blocking;
  bool writeError;
  int logicalFlushes;
  int physicalFlushes;
  int recoveryPhase;
  int eventNumber;
  int windowReads;
  int windowWrites;
  int commitsComplete;
  int backlogStalls;
  int priorWindowReads;
  int priorWindowWrites;
  int priorCommitsComplete;
  int priorBacklogStalls;
  int windowBuffers;
  int maxWindows;
  int priorCount;
  int maxTransactions;
  uint64 priorDelta;
  int priorWrites;
  int32 tracePage;
  int32 traceRecord;
  uint32 chilledRecords;
  uint64 chilledBytes;
  int32 wantToSerializePorpoises;
  int32 serializePorpoises;
  uint64 startRecordVirtualOffset;
  int logRotations;

  TableSpaceInfo *tableSpaces[SLT_HASH_SIZE];
  TableSpaceInfo *tableSpaceInfo;
  StreamLogTransaction *earliest;
  StreamLogTransaction *latest;

  Queue<StreamLogTransaction> pending;
  Queue<StreamLogTransaction> inactions;
  Queue<StreamLogTransaction> running;

 private:
  Thread *volatile writer;
};

}  // namespace Changjiang
