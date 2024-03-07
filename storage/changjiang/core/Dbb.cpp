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

// Dbb.cpp: implementation of the Dbb class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <memory.h>
#include "Engine.h"
#include "Dbb.h"
#include "Cache.h"
#include "PageInventoryPage.h"
#include "RecordLocatorPage.h"
#include "SectionPage.h"
#include "SequencePage.h"
#include "Section.h"
#include "Hdr.h"
#include "IndexRootPage.h"
#include "BDB.h"
#include "DataPage.h"
#include "DataOverflowPage.h"
#include "Inversion.h"
#include "Validation.h"
#include "Transaction.h"
#include "Log.h"
#include "SQLError.h"
#include "Database.h"
#include "Stream.h"
#include "Threads.h"
#include "IndexPage.h"
#include "InversionPage.h"
#include "Connection.h"
#include "Sync.h"
#include "StreamLog.h"
#include "StreamLogControl.h"
#include "Index.h"
#include "IndexKey.h"
#include "IndexNode.h"
#include "DatabaseClone.h"
#include "Table.h"

namespace Changjiang {

//#define STOP_RECORD	123
//#define TRACE_PAGE	109

extern uint changjiang_large_blob_threshold;

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Dbb::Dbb(Database *dbase) : sectionsMutex("Dbb::sectionMutex") {
  database = dbase;
  cache = NULL;
  sequenceSection = NULL;
  nextIndex = 0;
  nextSection = 0;
  lastPageAllocated = 0;
  debug = 0;  // DEBUG_KEYS | DEBUG_PAGES;
  inversion = new Inversion(this);
  sequenceSectionId = 0;
  shadows = NULL;
  highPage = 0;
  // recovering = false;
  defaultIndexVersion = INDEX_CURRENT_VERSION;
  tableSpaceSectionId = 0;
  tableSpaceId = 0;
  noLog = false;
  syncClone.setName("Dbb::syncClone");
  syncSequences.setName("Dbb::syncSequences");
  memset(sections, 0, sizeof(sections));
}

Dbb::Dbb(Dbb *dbb, int tblSpaceId) : sectionsMutex("Dbb::sectionMutex") {
  database = dbb->database;
  tableSpaceId = tblSpaceId;
  cache = dbb->cache;
  pageSize = dbb->pageSize;
  streamLog = dbb->streamLog;
  sequenceSection = NULL;
  nextIndex = 0;
  nextSection = 0;
  lastPageAllocated = 0;
  debug = dbb->debug;
  inversion = NULL;
  sequenceSectionId = 0;
  shadows = NULL;
  highPage = 0;
  defaultIndexVersion = dbb->defaultIndexVersion;
  noLog = false;
  memset(sections, 0, sizeof(sections));
}

Dbb::~Dbb() {
  /***
  if (cache)
          delete cache;
  ***/

  for (DatabaseCopy *shadow; (shadow = shadows);) {
    shadows = shadow->next;
    shadow->close();
    delete shadow;
  }

  Section *section;

  for (int n = 0; n < SECTION_HASH_SIZE; ++n)
    while ((section = sections[n])) {
      sections[n] = section->hash;
      delete section;
    }

  if (inversion) delete inversion;

  if (dbb) dbb->close();
}

Cache *Dbb::create(const char *fileName, int pageSz, int64 cacheSize,
                   FileType fileType, TransId transId, const char *logRoot,
                   bool useExistingFile) {
  streamLog = database->streamLog;
  odsVersion = ODS_VERSION;
  odsMinorVersion = ODS_MINOR_VERSION;
  sequence = 1;

  init(pageSz, (int)((cacheSize + pageSz - 1) / pageSz));
  if (useExistingFile)
    openFile(fileName, false);
  else
    createFile(fileName);

  try {
    Hdr::create(this, fileType, transId, logRoot);
    PageInventoryPage::create(this, transId);
    RootPage::createSectionRoot(this, transId);
    IndexRootPage::create(this, transId);
  } catch (...) {
    closeFile();
    if (!useExistingFile) deleteFile();
    throw;
  }

  return cache;
}

void Dbb::init(int pageSz, int cacheSize) {
  pageSize = pageSz;

  if (!cache && cacheSize)
    cache = new Cache(database, pageSize, cacheSize / 2, cacheSize);

  init();
}

void Dbb::init() {
  pipSlots = (short)((pageSize - OFFSET(PageInventoryPage *, freePages)) /
                     sizeof(short));
  pagesPerPip = (pipSlots)*PIP_BITS;
  pagesPerSection =
      (short)((pageSize - OFFSET(SectionPage *, pages)) / sizeof(int32));
  linesPerPage = (short)((pageSize - OFFSET(RecordLocatorPage *, elements)) /
                         sizeof(struct RecordIndex));
  sequencesPerPage =
      (short)((pageSize - OFFSET(SequencePage *, sequences)) / sizeof(int64));
  sequencesPerSection = (int)(pagesPerSection * sequencesPerPage);

  utf8 = false;
}

void Dbb::initRepository(Hdr *header) {
  init();
  sequenceSectionId = header->sequenceSectionId;
  odsVersion = header->odsVersion;
  odsMinorVersion = header->odsMinorVersion;
  highPage = PageInventoryPage::getLastPage(this);
}

Bdb *Dbb::fakePage(int32 pageNumber, PageType pageType, TransId transId) {
  ++fakes;
  return cache->fakePage(this, pageNumber, pageType, transId);
}

Bdb *Dbb::fetchPage(int32 pageNumber, PageType pageType, LockType lockType) {
  ++fetches;

  return cache->fetchPage(this, pageNumber, pageType, lockType);
}

Bdb *Dbb::trialFetch(int32 pageNumber, PageType pageType, LockType lockType) {
  Bdb *bdb = cache->trialFetch(this, pageNumber, lockType);
  BDB_HISTORY(bdb);

  if (bdb) {
    ++fetches;

    return bdb;
  }

  bdb = fakePage(pageNumber, pageType, NO_TRANSACTION);
  BDB_HISTORY(bdb);

  if (trialRead(bdb)) return bdb;

  bdb->release(REL_HISTORY);

  return NULL;
}

Bdb *Dbb::allocPage(PageType pageType, TransId transId) {
  Bdb *bdb = PageInventoryPage::allocPage(this, pageType, transId);

#ifdef TRACE_PAGE
  if (bdb->pageNumber == TRACE_PAGE)
    Log::debug("Allocating trace page %d\n", bdb->pageNumber);
#endif

  if (bdb->pageNumber > highPage) highPage = bdb->pageNumber;

  return bdb;
}

void Dbb::reallocPage(int32 pageNumber) {
  PageInventoryPage::reallocPage(this, pageNumber);
  highPage = MAX(highPage, pageNumber);
}

Bdb *Dbb::handoffPage(Bdb *bdb, int32 pageNumber, PageType pageType,
                      LockType lockType) {
  Bdb *newBdb = fetchPage(pageNumber, pageType, lockType);
  BDB_HISTORY(newBdb);
  bdb->release(REL_HISTORY);

  return newBdb;
}

int32 Dbb::createSection(TransId transId) {
  int32 sectionId = Section::createSection(this, transId);

  if (streamLog && !streamLog->recovering)
    streamLog->logControl->createSection.append(this, transId, sectionId);

  return sectionId;
}

void Dbb::createSection(int32 sectionId, TransId transId) {
  Section::createSection(this, sectionId, transId);
}

int32 Dbb::insertStub(int32 sectionId, TransactionState *transaction) {
  TransId transId = (transaction) ? transaction->transactionId : 0;
  Section *section = findSection(sectionId);

  return section->insertStub(transId);
}

int32 Dbb::insertStub(Section *section, TransactionState *transaction) {
  TransId transId = (transaction) ? transaction->transactionId : 0;

  return section->insertStub(transId);
}

void Dbb::reInsertStub(int32 sectionId, int32 recordId, TransId transId) {
  Section *section = findSection(sectionId);

  if (!section->root)
    if (!section->getSectionRoot())
      throw SQLError(DATABASE_DAMAGED, "database section %d has been lost",
                     sectionId);

  section->reInsertStub(recordId, transId);
}

void Dbb::logRecord(int32 sectionId, int32 recordId, Stream *stream,
                    Transaction *transaction) {
  if (streamLog) {
    if (stream)
      streamLog->logControl->dataUpdate.append(this, transaction, sectionId,
                                               recordId, stream);
    else
      streamLog->logControl->deleteData.append(this, transaction, sectionId,
                                               recordId);
  } else
    updateRecord(sectionId, recordId, stream, transaction->transactionId,
                 false);
}

void Dbb::updateBlob(Section *blobSection, int recordNumber, Stream *stream,
                     TransactionState *transState) {
  if (!streamLog->recovering && stream &&
      stream->totalLength < (int)changjiang_large_blob_threshold) {
    streamLog->logControl->smallBlob.append(this, blobSection->sectionId,
                                            transState->transactionId,
                                            recordNumber, stream);
    updateRecord(blobSection, recordNumber, stream, transState, false);
  } else {
    updateRecord(blobSection, recordNumber, stream, transState, true);
    transState->pendingPageWrites = true;
  }
}

void Dbb::updateRecord(int32 sectionId, int32 recordId, Stream *stream,
                       TransId transId, bool earlyWrite) {
  Section *section = findSection(sectionId);
  section->updateRecord(recordId, stream, transId, earlyWrite);

  if (!earlyWrite && !streamLog->recovering && transId)
    streamLog->setPhysicalBlock(transId);
}

void Dbb::updateRecord(Section *section, int32 recordId, Stream *stream,
                       TransactionState *transaction, bool earlyWrite) {
  TransId transId = (transaction) ? transaction->transactionId : 0;
  section->updateRecord(recordId, stream, transId, earlyWrite);

  if (!earlyWrite && !streamLog->recovering && transId)
    streamLog->setPhysicalBlock(transId);
}

void Dbb::expungeRecord(Section *section, int32 recordId) {
  section->expungeRecord(recordId);
}

Section *Dbb::findSection(int32 sectionId) {
  ASSERT(sectionId != Section::INVALID_SECTION_ID);

  int slot = sectionId % SECTION_HASH_SIZE;
  Section *section;

  Sync sync(&sectionsMutex, "Dbb::findSection");
  sync.lock(Exclusive);
  for (section = sections[slot]; section; section = section->hash)
    if (section->sectionId == sectionId) return section;

  section = new Section(this, sectionId, NO_TRANSACTION);
  section->hash = sections[slot];
  sections[slot] = section;

  return section;
}

bool Dbb::fetchRecord(int32 sectionId, int32 recordNumber, Stream *stream) {
  Section *section = findSection(sectionId);

  return section->fetchRecord(recordNumber, stream, NO_TRANSACTION);
}

bool Dbb::fetchRecord(Section *section, int32 recordNumber, Stream *stream) {
  return section->fetchRecord(recordNumber, stream, NO_TRANSACTION);
}

int32 Dbb::findNextRecord(Section *section, int32 startingRecord,
                          Stream *stream) {
  // Section *section = findSection(sectionId);

  return section->findNextRecord(startingRecord, stream);
}

int32 Dbb::createIndex(TransId transId, int indexVersion) {
  return IndexRootPage::createIndex(this, transId);
}

bool Dbb::addIndexEntry(int32 indexId, int indexVersion, IndexKey *key,
                        int32 recordNumber, TransId transId) {
#ifdef STOP_RECORD
  if (recordNumber == STOP_RECORD) ++debug;
#endif

  bool result =
      IndexRootPage::addIndexEntry(this, indexId, key, recordNumber, transId);

#ifdef STOP_RECORD
  if (recordNumber == STOP_RECORD) --debug;
#endif

  /***
  if (!recovering && streamLog)
          streamLog->logControl->indexAdd.append(indexId, key, recordNumber,
  transId);
  ***/

  return result;
}

void Dbb::flush() {
  if (!cache) return;

  cache->flush(this);
}

Cache *Dbb::open(const char *fileName, int64 cacheSize, TransId transId) {
  streamLog = database->streamLog;
  Hdr header;
  openFile(fileName, false);
  readHeader(&header);

  int n = header.pageSize;

  while (n && !(n & 1)) n >>= 1;

  if (header.fileType != HdrDatabaseFile)
    throw SQLError(VERSION_ERROR, "\"%s\" is not a Changjiang database file\n",
                   fileName);

  if (header.odsVersion > ODS_VERSION ||
      (header.odsVersion == ODS_VERSION &&
       header.odsMinorVersion > ODS_MINOR_VERSION))
    throw SQLError(VERSION_ERROR,
                   "Changjiang on disk structure version %d.%d is not "
                   "supported by version %d.%d server",
                   header.odsVersion, header.odsMinorVersion, ODS_VERSION,
                   ODS_MINOR_VERSION);

  if (header.odsVersion == ODS_VERSION2 &&
      header.odsMinorVersion < ODS_MINOR_VERSION3)
    throw SQLError(VERSION_ERROR,
                   "Changjiang on disk structure version %d.%d is not "
                   "supported by version %d.%d server",
                   header.odsVersion, header.odsMinorVersion, ODS_VERSION,
                   ODS_MINOR_VERSION);

  if (n != 1 || header.pageSize < 1024 || header.pageSize > 32768)
    throw SQLError(VERSION_ERROR, "invalid database header page size (%d)",
                   header.pageSize);

  init(header.pageSize,
       (int)((cacheSize + header.pageSize - 1) / header.pageSize));
  Bdb *bdb = fetchPage(HEADER_PAGE, PAGE_header, Exclusive);
  BDB_HISTORY(bdb);
  bdb->mark(transId);
  Hdr *headerPage = (Hdr *)bdb->buffer;

  sequence = headerPage->sequence++;
  sequenceSectionId = headerPage->sequenceSectionId;
  odsVersion = headerPage->odsVersion;
  odsMinorVersion = headerPage->odsMinorVersion;
  utf8 = headerPage->utf8 != 0;
  headerPage->state = HdrOpen;
  database->creationTime = headerPage->creationTime;
  logOffset = headerPage->logOffset;
  logLength = headerPage->logLength;
  tableSpaceSectionId = headerPage->tableSpaceSectionId;
  database->streamLogBlockSize = headerPage->streamLogBlockSize;
  defaultIndexVersion = headerPage->defaultIndexVersionNumber;

  char root[256];
  int len =
      headerPage->getHeaderVariable(this, hdrLogPrefix, sizeof(root), root);

  if (len > 0) logRoot = JString(root, len);

  bdb->release(REL_HISTORY);
  flush();

  return cache;
}

void Dbb::setODSMinorVersion(int minor) {
  odsMinorVersion = minor;
  Bdb *bdb = fetchPage(HEADER_PAGE, PAGE_header, Exclusive);
  BDB_HISTORY(bdb);
  bdb->mark(NO_TRANSACTION);
  Hdr *headerPage = (Hdr *)bdb->buffer;
  headerPage->odsMinorVersion = minor;
  bdb->release(REL_HISTORY);
  flush();
}

void Dbb::deleteIndex(int32 indexId, int indexVersion, TransId transId) {
  if (streamLog)
    streamLog->logControl->deleteIndex.append(this, transId, indexId,
                                              indexVersion);
  else
    IndexRootPage::deleteIndex(this, indexId, transId);
}

void Dbb::setDebug() { ++debug; }

void Dbb::clearDebug() { --debug; }

void Dbb::freePage(int32 pageNumber) { cache->freePage(this, pageNumber); }

void Dbb::freePage(Bdb *bdb, TransId transId) {
  int32 pageNumber = bdb->pageNumber;

#ifdef TRACE_PAGE
  if (pageNumber == TRACE_PAGE)
    Log::debug("Freeing trace page %d\n", pageNumber);
#endif

  // bdb->buffer->pageType = PAGE_free;
  bdb->setPageHeader(PAGE_free);
  cache->markClean(bdb);
  bdb->release(REL_HISTORY);

  if (streamLog && !streamLog->recovering)
    streamLog->logControl->freePage.append(this, pageNumber);

  PageInventoryPage::freePage(this, pageNumber, transId);
}

void Dbb::redoFreePage(int32 pageNumber) {
  cache->freePage(this, pageNumber);
  PageInventoryPage::freePage(this, pageNumber, NO_TRANSACTION);
}

void Dbb::deleteSection(int32 sectionId, TransId transId) {
  int slot = sectionId % SECTION_HASH_SIZE;

  if (streamLog && !streamLog->recovering)
    streamLog->logControl->dropTable.append(this, transId, sectionId);
  else
    Section::deleteSection(this, sectionId, transId);

  Section *section;

  Sync sync(&sectionsMutex, "Dbb::deleteSection");
  sync.lock(Exclusive);

  for (Section **ptr = sections + slot; (section = *ptr); ptr = &section->hash)
    if (section->sectionId == sectionId) {
      *ptr = section->hash;
      break;
    }

  delete section;
}

void Dbb::shutdown(TransId transId) {
  if (!cache) return;

  if (fileId != -1) {
    Bdb *bdb = fetchPage(HEADER_PAGE, PAGE_header, Exclusive);
    BDB_HISTORY(bdb);
    bdb->mark(transId);
    Hdr *headerPage = (Hdr *)bdb->buffer;
    headerPage->state = HdrClosed;
    bdb->release(REL_HISTORY);
    flush();
  }
}

void Dbb::validate(int optionMask) {
  /***
  if (optionMask & validateSpecial)
          {
          int indexId = 584;
          int recordNumber = 22;
          IndexRootPage::debugBucket (this, indexId, recordNumber,
  NO_TRANSACTION); return;
          }
  ***/

  Validation validation(this, optionMask);
  validation.inUse((int32)HEADER_PAGE, "HeaderPage");
  PageInventoryPage::validate(this, &validation);

  if (inversion) inversion->validate(&validation);

  Section::validateIndexes(this, &validation);
  Section::validateSections(this, &validation);
  PageInventoryPage::validateInventory(this, &validation);

  if (validation.dups) {
    Log::debug("Summary of multiply-allocated pages:\n");
    validation.phase = 1;
    validation.inUse((int32)HEADER_PAGE, "HeaderPage");
    PageInventoryPage::validate(this, &validation);

    if (inversion) inversion->validate(&validation);

    Section::validateIndexes(this, &validation);
    Section::validateSections(this, &validation);
  }
}

bool Dbb::deleteIndexEntry(int32 indexId, int indexVersion, IndexKey *key,
                           int32 recordNumber, TransId transId) {
  bool result;

  result = IndexRootPage::deleteIndexEntry(this, indexId, key, recordNumber,
                                           transId);
  if (streamLog && !streamLog->recovering)
    streamLog->logControl->indexDelete.append(this, indexId, indexVersion, key,
                                              recordNumber, transId);

  return result;
}

int Dbb::createSequence(int64 initialValue, TransId transId) {
  int id = (int)updateSequence(0, 1, transId);
  int64 value = updateSequence(id, initialValue, transId);

  if (value != initialValue) updateSequence(id, initialValue - value, transId);

  return id;
}

Bdb *Dbb::getSequencePage(int sequenceId, LockType lockType, TransId transId) {
  Bdb *bdb;

  // In the bad old days, we used the leaf level of the sequence tree for
  // sequence.  Bad Jim!

  /***
  if (odsMinorVersion < ODS_MINOR_VERSION2)
          {
          int relativePage = sequenceId / sequencesPerPage;
          int slot = sequenceId % sequencesPerPage;
          bdb = sequenceSection->getSectionPage (relativePage, lockType,
  transId); BDB_HISTORY(bdb); SequencePage *page = (SequencePage*) bdb->buffer;

          if (page->pageType == PAGE_sections)
                  {
                  bdb->mark(transId);
                  page->pageType = PAGE_sequences;
                  memset (page->sequences, 0, sequencesPerPage *
  sizeof(page->sequences[0]));
                  }
          }
  else
  ***/
  {
    Sync sync(&syncSequences, "Dbb::getSequencePage");
    sync.lock(Shared);
    int sequencePageSequence = sequenceId / sequencesPerPage;
    int32 sequencePageNumber = sequencePages.get(sequencePageSequence);

    // If we know the page number, just get it.  Otherwise lock for write, and
    // try again.  If we still can't find it, go looking for it.  At last
    // resort, create it

    if (sequencePageNumber) {
#ifdef STORAGE_ENGINE
      bdb = fetchPage(sequencePageNumber, PAGE_sequences, lockType);
#else
      bdb = fetchPage(sequencePageNumber, PAGE_any, lockType);
#endif
      BDB_HISTORY(bdb);
    } else {
      sync.unlock();
      sync.lock(Exclusive);
      sequencePageNumber = sequencePages.get(sequencePageSequence);

      if (sequencePageNumber) {
#ifdef STORAGE_ENGINE
        bdb = fetchPage(sequencePageNumber, PAGE_sequences, lockType);
#else
        bdb = fetchPage(sequencePageNumber, PAGE_any, lockType);
#endif
        BDB_HISTORY(bdb);
      } else {
        int relativePage = sequencePageSequence / sequencesPerSection;
        int sequenceSlot = sequencePageSequence % sequencesPerSection;
        Bdb *sectionBdb =
            sequenceSection->getSectionPage(relativePage, Shared, transId);
        BDB_HISTORY(sectionBdb);
        SectionPage *sectionPage = (SectionPage *)sectionBdb->buffer;

        if (!sectionPage->pages[sequenceSlot]) {
          sectionBdb->release(REL_HISTORY);
          sectionBdb =
              sequenceSection->getSectionPage(relativePage, Exclusive, transId);
          BDB_HISTORY(sectionBdb);
          sectionPage = (SectionPage *)sectionBdb->buffer;
        }

        if (!sectionPage->pages[sequenceSlot]) {
          bdb = allocPage(PAGE_sequences, transId);
          BDB_HISTORY(bdb);
          sectionBdb->mark(transId);
          sectionPage->pages[sequenceSlot] = bdb->pageNumber;
          sectionBdb->release(REL_HISTORY);

          if (!streamLog->recovering) {
            streamLog->logControl->sequencePage.append(
                this, sequencePageSequence, bdb->pageNumber);
            int32 pageNumber = bdb->pageNumber;
            bdb->release(REL_HISTORY);
            streamLog->checkpoint(false);
            bdb = fetchPage(pageNumber, PAGE_sequences, lockType);
            BDB_HISTORY(bdb);
          }
        } else {
          bdb = handoffPage(sectionBdb, sectionPage->pages[sequenceSlot],
                            PAGE_any, lockType);
          BDB_HISTORY(bdb);
        }

        sequencePages.set(sequencePageSequence, bdb->pageNumber);
      }
    }
  }

  return bdb;
}

void Dbb::redoSequencePage(int sequencePageSequence, int32 pageNumber) {
  getSequenceSection(NO_TRANSACTION);
  int relativePage = sequencePageSequence / sequencesPerSection;
  int sequenceSlot = sequencePageSequence % sequencesPerSection;
  Bdb *sectionBdb =
      sequenceSection->getSectionPage(relativePage, Exclusive, NO_TRANSACTION);
  BDB_HISTORY(sectionBdb);
  SectionPage *sectionPage = (SectionPage *)sectionBdb->buffer;
  sectionBdb->mark(NO_TRANSACTION);
  Bdb *bdb = fakePage(pageNumber, PAGE_sequences, NO_TRANSACTION);
  BDB_HISTORY(bdb);
  sectionPage->pages[sequenceSlot] = bdb->pageNumber;
  sectionBdb->release(REL_HISTORY);
  bdb->release(REL_HISTORY);
}

int64 Dbb::updateSequence(int sequenceId, int64 delta, TransId transId) {
  if (!sequenceSection) getSequenceSection(transId);

  Bdb *bdb = getSequencePage(sequenceId, (delta) ? Exclusive : Shared, transId);
  BDB_HISTORY(bdb);
  SequencePage *page = (SequencePage *)bdb->buffer;
  int slot = sequenceId % sequencesPerPage;
  int64 value;

  if (delta) {
    bdb->mark(transId);

    if (page->pageType == PAGE_sections)
      // page->pageType = PAGE_sequences;
      bdb->setPageHeader(PAGE_sequences);

    value = page->sequences[slot] += delta;
  } else
    value = page->sequences[slot];

  bdb->release(REL_HISTORY);

  if (streamLog && !streamLog->recovering && delta)
    streamLog->logControl->sequence.append(sequenceId, value);

  return value;
}

int64 Dbb::redoSequence(int sequenceId, int64 sequence) {
  // Section *section =
  getSequenceSection(0);  // TransId does not matter here.
  // int relativePage = sequenceId / sequencesPerPage;
  int slot = sequenceId % sequencesPerPage;
  Bdb *bdb = getSequencePage(sequenceId, Exclusive, 0);
  BDB_HISTORY(bdb);
  SequencePage *page = (SequencePage *)bdb->buffer;
  int64 value;

  if (sequence) {
    value = page->sequences[slot];

    if (value < sequence) {
      bdb->mark(0);
      value = page->sequences[slot] = sequence;
    }
  } else
    value = page->sequences[slot];

  bdb->release(REL_HISTORY);

  return value;
}

Section *Dbb::getSequenceSection(TransId transId) {
  // If it's already known, cool

  if (sequenceSection) return sequenceSection;

  // If it doesn't exist yet, this is a good time to create it

  if (sequenceSectionId == 0) {
    sequenceSectionId = createSection(transId);
    Bdb *bdb = fetchPage(HEADER_PAGE, PAGE_header, Exclusive);
    BDB_HISTORY(bdb);
    bdb->mark(transId);
    Hdr *headerPage = (Hdr *)bdb->buffer;
    headerPage->sequenceSectionId = sequenceSectionId;
    bdb->release(REL_HISTORY);
    cache->flush((int64)0);
  }

  // Find action section

  sequenceSection = findSection(sequenceSectionId);

  return sequenceSection;
}

void Dbb::createInversion(TransId transId) {
  inversion->createInversion(transId);
}

void Dbb::cloneFile(Database *database, const char *fileName,
                    bool createShadow) {
  DatabaseClone *shadow = new DatabaseClone(this);
  // IO *shadow = new IO;
  // shadow->pageSize = pageSize;
  // shadow->dbb = this;

  try {
    shadow->createFile(fileName);
    addShadow(shadow);
    shadow->clone();
    shadow->close();
    // cloneFile (shadow, createShadow);
  } catch (SQLException &exception) {
    Log::log("Failure during copy to %s: %s\n", fileName, exception.getText());
    deleteShadow(shadow);
    throw;
  } catch (...) {
    deleteShadow(shadow);
    throw;
  }
}

/***
void Dbb::cloneFile(DatabaseClone *shadow, bool isShadow)
{
        Sync sync (&syncClone, "Dbb::cloneFile(2)");
        sync.lock (Exclusive);
        shadow->next = shadows;
        shadows = shadow;
        sync.unlock();
        int n = 0;

        for (;;)
                {
                int lastPage = PageInventoryPage::getLastPage (this);

                if (n >= lastPage)
                        {
                        if (isShadow || !streamLog)
                                break;

                        Sync syncCache(&cache->syncObject, "Dbb::cloneFile");
                        syncCache.lock(Exclusive);
                        lastPage = PageInventoryPage::getLastPage (this);

                        if (lastPage < n)
                                continue;

                        shadow->active = false;
                        Hdr	header;
                        shadow->readHeader(&header);
                        header.logOffset = lastPage + 1;
                        header.logLength = streamLog->appendLog(shadow->shadow,
header.logOffset); shadow->writeHeader(&header);

                        break;
                        }

                for (; n < lastPage; ++n)
                        {
                        Bdb *bdb = fetchPage (n, PAGE_any, Shared);
                        BDB_HISTORY(bdb);
                        shadow->highWater = bdb->pageNumber;
                        shadow->writePage (bdb);
                        bdb->release();
                        }
                }

        shadow->highWater = 0;
        Log::log ("database file copy to \"%s\" is complete\n",
shadow->getFileName());

        if (!isShadow)
                deleteShadow (shadow);
}
***/

bool Dbb::deleteShadow(DatabaseCopy *shadow) {
  Sync sync(&syncClone, "Dbb::deleteShadow");
  sync.lock(Exclusive);

  for (DatabaseCopy **ptr = &shadows; *ptr; ptr = &(*ptr)->next)
    if (*ptr == shadow) {
      *ptr = shadow->next;
      shadow->close();
      delete shadow;

      return true;
    }

  Log::log("couldn't delete shadow/clone \"%s\" is complete\n",
           shadow->getFileName());

  return false;
}

void Dbb::printPage(int pageNumber) {
  Bdb *bdb = fetchPage(pageNumber, PAGE_any, Shared);
  BDB_HISTORY(bdb);
  printPage(bdb);
  bdb->release(REL_HISTORY);
}

void Dbb::printPage(Bdb *bdb) {
  Page *page = bdb->buffer;
  int pageNumber = bdb->pageNumber;

  switch (page->pageType) {
    case PAGE_header:  // 1
      Log::debug("Page %d is header page\n", pageNumber);
      break;

    case PAGE_sections:  // 2
    {
      SectionPage *sectionPage = (SectionPage *)page;
      Log::debug("Page %d is sections page section %d, level %d, seq %d\n",
                 pageNumber, sectionPage->section, sectionPage->level,
                 sectionPage->sequence);
    } break;

      /***
      case PAGE_section:		// 3
              Log::debug ("Page %d is section page\n");
              break;
      ***/

    case PAGE_record_locator:  // 4
    {
      RecordLocatorPage *recordLocator = (RecordLocatorPage *)page;
      Log::debug("Page %d is record locator page, section %d, seq %d\n",
                 pageNumber, recordLocator->section, recordLocator->sequence);
    } break;

    case PAGE_btree:  // 5
                      // case PAGE_btree_leaf:	// 6
      // IndexPage::printPage (bdb, false);
      {
        IndexPage *indexPage = (IndexPage *)page;
        Log::debug("Page %d is index page, next %d, lvl %d\n", pageNumber,
                   indexPage->nextPage, indexPage->level);
      }
      break;

    case PAGE_data: {  // 7
      // DataPage *dataPage = (DataPage*) page;
      Log::debug("Page %d is data page\n", pageNumber);
    } break;

    case PAGE_inventory:  // 8
      Log::debug("Page %d is page inventory page\n", pageNumber);
      break;

    case PAGE_data_overflow:  // 9
      Log::debug("Page %d is data overflow page\n", pageNumber);
      break;

    case PAGE_inversion:  // 10
      //((InversionPage*) bdb)->printPage (bdb);
      {
        InversionPage *indexPage = (InversionPage *)page;
        Log::debug("Page %d is index page, parent %d, prior %d, next %d\n",
                   pageNumber, indexPage->parentPage, indexPage->priorPage,
                   indexPage->nextPage);
      }
      break;

    case PAGE_free:  // 11 Page has been released
      Log::debug("Page %d is a free page\n", pageNumber);
      break;

    default:
      Log::debug("Page %d is unknown type %d\n", pageNumber, page->pageType);
  }
}

void Dbb::close() {
  if (fileId != -1) {
    cache->flush(this);
    closeFile();
  }
}

bool Dbb::hasDirtyPages() { return cache->hasDirtyPages(this); }

void Dbb::reportStatistics() {
  if (!Log::isActive(LogInfo)) return;

  int deltaReads = reads - priorReads;
  int deltaWrites = writes - priorWrites;
  int deltaFlushWrites = flushWrites - priorFlushWrites;
  int deltaFetches = fetches - priorFetches;
  // int deltaFakes = reads - priorFakes;

  if (!deltaReads && !deltaWrites && !deltaFetches) return;

  Log::log(
      LogInfo,
      "%d: Activity on %s: %d fetches, %d reads, %d writes, %d flushWrites\n",
      database->deltaTime, (const char *)fileName, deltaFetches, deltaReads,
      deltaWrites, deltaFlushWrites);

  priorReads = reads;
  priorWrites = writes;
  priorFetches = fetches;
  priorFakes = fakes;
  priorFlushWrites = flushWrites;
}

void Dbb::dropDatabase() {
  close();
  deleteFile();
}

void Dbb::validateCache(void) { cache->validate(); }

void Dbb::redoRecordLocatorPage(int sectionId, int sequence, int32 pageNumber,
                                bool isPostFlush) {
  Section *section = findSection(sectionId);
  section->redoRecordLocatorPage(sequence, pageNumber, isPostFlush);
}

void Dbb::redoDataPage(int sectionId, int32 pageNumber,
                       int32 locatorPageNumber) {
  Section *section = findSection(sectionId);
  section->redoDataPage(pageNumber, locatorPageNumber);
}

void Dbb::logUpdatedRecords(Transaction *transaction, RecordVersion *records,
                            bool chill) {
  if (records)
    streamLog->logControl->updateRecords.append(transaction, records, chill);
}

void Dbb::logIndexUpdates(DeferredIndex *deferredIndex) {
  streamLog->logControl->updateIndex.append(deferredIndex);
}

bool Dbb::sectionInUse(int sectionId) {
  return streamLog->sectionInUse(sectionId, tableSpaceId);
}

bool Dbb::indexInUse(int indexId) {
  return streamLog->indexInUse(indexId, tableSpaceId);
}

void Dbb::analyzeSection(int sectionId, const char *sectionName,
                         int indentation, Stream *stream) {
  Section *section = findSection(sectionId);
  SectionAnalysis numbers;
  memset(&numbers, 0, sizeof(numbers));
  section->analyze(&numbers, section->root);
  int64 space = pageSize * numbers.dataPages;

  if (space == 0) return;

  int utilization = (int)((space - numbers.spaceAvailable) * 100 / space);
  stream->indent(indentation);
  stream->format("%s (id %d, table space %d)\n", sectionName, sectionId,
                 tableSpaceId);
  indentation += 3;
  stream->indent(indentation);
  stream->format("Record locator pages: %d\n", numbers.recordLocatorPages);
  stream->indent(indentation);
  stream->format("Data pages:           %d\n", numbers.dataPages);
  stream->indent(indentation);
  stream->format("Overflow pages:       %d\n", numbers.overflowPages);
  stream->indent(indentation);
  stream->format("Records:              %d\n", numbers.records);
  stream->indent(indentation);
  stream->format("Space utilization:    %d%%\n", utilization);
}

void Dbb::analyseIndex(int32 indexId, int indexVersion, const char *indexName,
                       int indentation, Stream *stream) {
  IndexAnalysis indexAnalysis;
  memset(&indexAnalysis, 0, sizeof(indexAnalysis));

  IndexRootPage::analyzeIndex(this, indexId, &indexAnalysis);

  stream->indent(indentation);
  stream->format("Index %s (id %d, table space %d) %d levels\n", indexName,
                 indexId, indexAnalysis.levels, tableSpaceId);
  indentation += 3;
  stream->indent(indentation);
  stream->format("Upper index pages:    %d\n", indexAnalysis.upperLevelPages);
  stream->indent(indentation);
  stream->format("Index leaf pages:     %d\n", indexAnalysis.leafPages);

  if (indexAnalysis.leafPages) {
    int utilization = (int)(indexAnalysis.leafSpaceUsed * 100 /
                            (indexAnalysis.leafPages * pageSize));
    stream->indent(indentation);
    stream->format("Leaf utilization:     %d%%\n", utilization);
  }
}

void Dbb::analyzeSpace(int indentation, Stream *stream) {
  PagesAnalysis pagesAnalysis;
  memset(&pagesAnalysis, 0, sizeof(pagesAnalysis));
  PageInventoryPage::analyzePages(this, &pagesAnalysis);
  stream->indent(indentation);
  stream->format("Free Pages\n");
  indentation += 3;
  stream->indent(indentation);
  stream->format("Pages allocated:         %d\n", pagesAnalysis.allocatedPages);
  stream->indent(indentation);
  stream->format("Max allocated page:      %d\n", pagesAnalysis.maxPage);
}

void Dbb::addShadow(DatabaseCopy *shadow) {
  Sync sync(&syncClone, "Dbb::addShadow");
  sync.lock(Exclusive);
  shadow->next = shadows;
  shadows = shadow;
}

void Dbb::updateTableSpaceSection(int id) {
  Bdb *bdb = fetchPage(HEADER_PAGE, PAGE_header, Exclusive);
  BDB_HISTORY(bdb);
  bdb->mark(NO_TRANSACTION);
  Hdr *header = (Hdr *)bdb->buffer;
  header->tableSpaceSectionId = id;
  bdb->release(REL_HISTORY);
}

void Dbb::updateStreamLogBlockSize(void) {
  Bdb *bdb = fetchPage(HEADER_PAGE, PAGE_header, Exclusive);
  BDB_HISTORY(bdb);
  bdb->mark(NO_TRANSACTION);
  Hdr *header = (Hdr *)bdb->buffer;
  header->streamLogBlockSize = database->streamLogBlockSize;
  bdb->release(REL_HISTORY);
}

void Dbb::setCacheRecovering(bool state) { cache->recovering = state; }

}  // namespace Changjiang
