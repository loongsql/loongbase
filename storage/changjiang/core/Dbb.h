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

// Dbb.h: interface for the Dbb class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "IOx.h"
#include "HdrState.h"
#include "PageType.h"
#include "SyncObject.h"
#include "SparseArray.h"
#include "Mutex.h"

namespace Changjiang {

#define TRANSACTION_ID(transaction) \
  ((transaction) ? transaction->transactionId : 0)

// default parameters

#define PAGE_SIZE 4096
#define CACHE_SIZE 1024
#define SECTION_HASH_SIZE 997

// Selective debugging

#define DEBUG_NONE 0
#define DEBUG_KEYS 1 << 0
#define DEBUG_PAGES 1 << 1
#define DEBUG_SYSTEM_TABLES 1 << 2
#define DEBUG_NODES_ADDED 1 << 3
#define DEBUG_NODES_DELETED 1 << 4
#define DEBUG_SPLIT_PAGE 1 << 5
#define DEBUG_FIND_LEAF 1 << 6
#define DEBUG_FIND_LEVEL 1 << 7
#define DEBUG_SCAN_INDEX 1 << 8
#define DEBUG_INDEX_MERGE 1 << 9
#define DEBUG_INVERSION 1 << 10
#define DEBUG_DEFERRED_INDEX 1 << 11
#define DEBUG_RECORD_SCAVENGE 1 << 12
#define DEBUG_PAGE_LEVEL 1 << 13
#define DEBUG_RECORD_LOCKS 1 << 14
#define DEBUG_ALL -1

static const int FillLevels = 5;

struct SectionAnalysis {
  int32 recordLocatorPages;
  int32 dataPages;
  int32 overflowPages;
  int32 spaceAvailable;
  int32 records;
};

struct IndexAnalysis {
  int levels;
  int upperLevelPages;
  int leafPages;
  int leafNodes;
  int fillLevels[FillLevels];
  int64 leafSpaceUsed;
};

struct PagesAnalysis {
  int maxPage;
  int allocatedPages;
  int totalPages;
};

class Cache;
class Section;
class Bitmap;
class Stream;
class Filter;
class ResultList;
class Inversion;
class Database;
class Validation;
class StreamLog;
class Transaction;
class TransactionState;
class IndexKey;
class RecordVersion;
class DeferredIndex;
class DatabaseCopy;
class DatabaseClone;

class Dbb : public IO {
 public:
  Dbb(Database *database);
  Dbb(Dbb *dbb, int tableSpaceId);
  virtual ~Dbb();

  // database oriented functions
  void dropDatabase();
  void reportStatistics();
  Cache *open(const char *fileName, int64 cacheSize, TransId transId);
  Cache *create(const char *fileName, int pageSize, int64 cacheSize,
                FileType fileType, TransId transId, const char *logRoot,
                bool useExistingFile = false);
  void close();
  void init(int pageSz, int cacheSize);
  void init();
  void initRepository(Hdr *header);
  void setODSMinorVersion(int minor);
  void validate(int optionMask);
  void shutdown(TransId transId);
  void clearDebug();
  void setDebug();
  void cloneFile(Database *database, const char *fileName, bool createShadow);
  void addShadow(DatabaseCopy *shadow);
  bool deleteShadow(DatabaseCopy *shadow);

  // Cache oriened functions
  void validateCache(void);
  void setCacheRecovering(bool state);
  bool hasDirtyPages();
  void flush();

  // Sequence oriented functions
  int createSequence(QUAD initialValue, TransId transId);
  int64 updateSequence(int sequenceId, int64 delta, TransId transId);
  Section *getSequenceSection(TransId transId);

  // Index oriented functions
  bool indexInUse(int indexId);
  bool addIndexEntry(int32 indexId, int indexVersion, IndexKey *key,
                     int32 recordNumber, TransId transId);
  int32 createIndex(TransId transId, int indexVersion);
  void deleteIndex(int32 indexId, int indexVersion, TransId transId);
  bool deleteIndexEntry(int32 indexId, int indexVersion, IndexKey *key,
                        int32 recordNumber, TransId transId);
  void analyseIndex(int32 indexId, int indexVersion, const char *indexName,
                    int indentation, Stream *stream);
  void createInversion(TransId transId);

  // Record oriented functions
  int32 findNextRecord(Section *section, int32 startingRecord, Stream *stream);
  bool fetchRecord(int32 sectionId, int32 recordNumber, Stream *stream);
  bool fetchRecord(Section *section, int32 recordNumber, Stream *stream);
  void updateRecord(int32 sectionId, int32 recordId, Stream *stream,
                    TransId transId, bool earlyWrite);
  void updateRecord(Section *section, int32 recordId, Stream *stream,
                    TransactionState *transaction, bool earlyWrite);
  void expungeRecord(Section *section, int32 recordNumber);
  void updateBlob(Section *blobSection, int recordNumber, Stream *blob,
                  TransactionState *transaction);

  // Page oriented functions
  void reallocPage(int32 pageNumber);
  Bdb *allocPage(PageType pageType, TransId transId);
  Bdb *fetchPage(int32 pageNumber, PageType pageType, LockType lockType);
  Bdb *fakePage(int32 pageNumber, PageType pageType, TransId transId);
  Bdb *trialFetch(int32 pageNumber, PageType pageType, LockType lockType);
  Bdb *handoffPage(Bdb *bdb, int32 pageNumber, PageType pageType,
                   LockType lockType);
  Bdb *getSequencePage(int sequenceId, LockType lockType, TransId transId);
  void freePage(int32 pageNumber);
  void freePage(Bdb *bdb, TransId transId);
  void printPage(int pageNumber);
  void printPage(Bdb *bdb);

  // Section Page functions
  bool sectionInUse(int sectionId);
  Section *findSection(int32 sectionId);
  int32 insertStub(int32 sectionId, TransactionState *transaction);
  int32 insertStub(Section *section, TransactionState *transaction);
  void reInsertStub(int32 sectionId, int32 recordId, TransId transId);
  int32 createSection(TransId transId);
  void createSection(int32 sectionId, TransId transId);
  void deleteSection(int32 sectionId, TransId transId);
  void analyzeSection(int sectionId, const char *sectionName, int indentation,
                      Stream *stream);
  void analyzeSpace(int indentation, Stream *stream);
  void updateTableSpaceSection(int id);

  // Stream Log oriented functions
  void logRecord(int32 sectionId, int32 recordId, Stream *stream,
                 Transaction *transaction);
  void logUpdatedRecords(Transaction *transaction, RecordVersion *records,
                         bool chill = false);
  void logIndexUpdates(DeferredIndex *deferredIndex);
  void updateStreamLogBlockSize(void);
  void redoSequencePage(int pageSequence, int32 pageNumber);
  int64 redoSequence(int sequenceId, int64 sequence);
  void redoDataPage(int sectionId, int32 pageNumber, int32 locatorPageNumber);
  void redoRecordLocatorPage(int sectionId, int sequence, int32 pageNumber,
                             bool isPostFlush);
  void redoFreePage(int32 pageNumber);

  Cache *cache;
  Database *database;
  int tableSpaceId;
  int pagesPerPip;
  short pipSlots;
  int32 lastPageAllocated;
  int32 nextSection;
  int32 nextIndex;
  int32 highPage;
  int32 tableSpaceSectionId;
  short pagesPerSection;
  short linesPerPage;
  short sequencesPerPage;
  int sequencesPerSection;
  bool utf8;
  bool noLog;
  Section *sections[SECTION_HASH_SIZE];
  Mutex sectionsMutex;
  int debug;
  int sequence;
  int odsVersion;
  int odsMinorVersion;
  int logOffset;
  int logLength;
  int defaultIndexVersion;
  int32 sequenceSectionId;
  Section *sequenceSection;
  HdrState priorState;
  Inversion *inversion;
  DatabaseCopy *shadows;
  SyncObject syncClone;
  SyncObject syncSequences;
  StreamLog *streamLog;
  JString logRoot;
  SparseArray<int32, 100> sequencePages;
};

}  // namespace Changjiang
