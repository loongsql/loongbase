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

// IndexRootPage.cpp: implementation of the IndexRootPage class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include <stdio.h>
#include <string.h>
#include "Engine.h"
#include "IndexRootPage.h"
#include "IndexPage.h"
#include "Dbb.h"
#include "BDB.h"
#include "Section.h"
#include "SectionPage.h"
#include "Bitmap.h"
#include "IndexNode.h"
#include "SQLError.h"
#include "Log.h"
#include "LogLock.h"
#include "IndexKey.h"
#include "StreamLogControl.h"
#include "Transaction.h"
#include "Index.h"
#include "SRLUpdateIndex.h"
#include "WalkIndex.h"

namespace Changjiang {

static IndexKey dummyKey;

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

void IndexRootPage::create(Dbb *dbb, TransId transId) {
  Bdb *bdb = dbb->fakePage(INDEX_ROOT, PAGE_sections, transId);
  BDB_HISTORY(bdb);
  IndexRootPage *sections = (IndexRootPage *)bdb->buffer;
  sections->section = -1;
  sections->level = 0;
  sections->sequence = 0;
  bdb->release(REL_HISTORY);
}

int32 IndexRootPage::createIndex(Dbb *dbb, TransId transId) {
  int sequence = -1;
  Bdb *sectionsBdb = NULL;
  SectionPage *sections = NULL;
  int skipped = 0;

  for (int32 id = dbb->nextIndex;; ++id) {
    int n = id / dbb->pagesPerSection;

    if (n != sequence) {
      sequence = n;

      if (sectionsBdb) sectionsBdb->release(REL_HISTORY);

      sectionsBdb =
          Section::getSectionPage(dbb, INDEX_ROOT, n, Exclusive, transId);
      BDB_HISTORY(sectionsBdb);
      sections = (SectionPage *)sectionsBdb->buffer;
      sequence = n;
    }

    int slot = id % dbb->pagesPerSection;

    if (sections->pages[slot] == 0) {
      if (dbb->indexInUse(id)) {
        if (!skipped) skipped = id;
      } else {
        createIndexRoot(dbb, transId, 0, id, sectionsBdb);
        sectionsBdb->release(REL_HISTORY);
        dbb->nextIndex = (skipped) ? skipped : id + 1;
        return id;
      }
    }
  }
}

bool IndexRootPage::addIndexEntry(Dbb *dbb, int32 indexId, IndexKey *key,
                                  int32 recordNumber, TransId transId) {
  IndexKey searchKey(key);
  searchKey.appendRecordNumber(recordNumber);

  if (dbb->debug & (DEBUG_KEYS | DEBUG_NODES_ADDED | DEBUG_PAGE_LEVEL)) {
    LogLock logLock;
    Log::debug("\n***** addIndexEntry(%d, %d)\n", recordNumber, indexId);
    Btn::printKey("insertion key", key, 0, false);
    Btn::printKey(" appended key", &searchKey, 0, false);
  }

  // Multiple threads may attempt to update the same index. If necessary, make
  // several attempts. The loop number 1000 is klugde to silence the Bug#37056,
  // until a proper solution is implemented . that does  not involve loops at
  // all
  for (int n = 0; n < 1000; ++n) {
    /* Find insert page and position on page */

    bool isRoot;
    Bdb *bdb = findInsertionLeaf(dbb, indexId, &searchKey, recordNumber,
                                 transId, &isRoot);

    if (!bdb) return false;

    IndexPage *page;

    for (;;) {
      page = (IndexPage *)bdb->buffer;
      Btn *node = page->findNodeInLeaf(key, NULL);
      Btn *bucketEnd = (Btn *)((char *)page + page->length);

      if (node < bucketEnd || page->nextPage == 0) break;

      ASSERT(bdb->pageNumber != page->nextPage);
      bdb = dbb->handoffPage(bdb, page->nextPage, PAGE_btree, Exclusive);
      isRoot = false;
      BDB_HISTORY(bdb);
    }

    bdb->mark(transId);

    /* If the node fits on page, we're done */

    AddNodeResult result;

    for (;;) {
      result = page->addNode(dbb, key, recordNumber);

      if (result != NextPage) break;

      bdb = dbb->handoffPage(bdb, page->nextPage, PAGE_btree, Exclusive);
      isRoot = false;
      BDB_HISTORY(bdb);
      bdb->mark(transId);
      page = (IndexPage *)bdb->buffer;
    }

    if (result == NodeAdded || result == Duplicate) {
      if (dbb->debug & (DEBUG_PAGES | DEBUG_NODES_ADDED))
        page->printPage(bdb, false);

      // page->validateInsertion (length, key, recordNumber);
      bdb->release(REL_HISTORY);

      return true;
    }

    /* Node didn't fit.  Split the page and propagate the
       split upward.  Sooner or later we'll go back and re-try
       the original insertion */

    if (splitIndexPage(dbb, indexId, bdb, transId, result, key, recordNumber,
                       isRoot))
      return true;

#ifdef _DEBUG
    if (n) {
      ++dbb->debug;
      Btn::printKey("Key", key, 0, false);
      Btn::printKey("SearchKey", &searchKey, 0, false);
      Bdb *bdb =
          findInsertionLeaf(dbb, indexId, &searchKey, recordNumber, transId);
      IndexPage *page = (IndexPage *)bdb->buffer;

      while (page->nextPage) {
        bdb = dbb->handoffPage(bdb, page->nextPage, PAGE_btree, Exclusive);
        BDB_HISTORY(bdb);
        page = (IndexPage *)bdb->buffer;
        page->printPage(bdb, false);
      }

      bdb->release(REL_HISTORY);
      --dbb->debug;
    }
#endif
  }

  ASSERT(false);
  FATAL("index split failed");

  return false;
}

Bdb *IndexRootPage::findLeaf(Dbb *dbb, int32 indexId, int32 rootPage,
                             IndexKey *indexKey, LockType lockType,
                             TransId transId) {
  Bdb *bdb = findRoot(dbb, indexId, rootPage, lockType, transId);
  BDB_HISTORY(bdb);

  if (!bdb) return NULL;

  IndexPage *page = (IndexPage *)bdb->buffer;

  if (dbb->debug & (DEBUG_PAGES | DEBUG_FIND_LEAF)) page->printPage(bdb, false);

  while (page->level > 0) {
    IndexNode node(page->findNodeInBranch(indexKey, 0));
    int32 pageNumber = node.getNumber();

    if (pageNumber == END_BUCKET) pageNumber = page->nextPage;

    if (pageNumber == 0) {
      page->printPage(bdb, false);
      // node.parseNode(page->findNodeInBranch (indexKey, 0));
      bdb->release(REL_HISTORY);
      throw SQLError(DATABASE_CORRUPTION, "index %d damaged", indexId);
    }

    bdb = dbb->handoffPage(bdb, pageNumber, PAGE_btree,
                           (page->level > 1) ? Shared : lockType);
    BDB_HISTORY(bdb);
    page = (IndexPage *)bdb->buffer;

    if (dbb->debug & (DEBUG_PAGES | DEBUG_FIND_LEAF))
      page->printPage(bdb, false);
  }

  return bdb;
}
Bdb *IndexRootPage::findInsertionLeaf(Dbb *dbb, int32 indexId,
                                      IndexKey *indexKey, int32 recordNumber,
                                      TransId transId, bool *isRoot) {
  int rootPageNumber;

  Bdb *bdb = findRoot(dbb, indexId, 0, Shared, transId);
  BDB_HISTORY(bdb);

  if (!bdb) return NULL;
  rootPageNumber = bdb->pageNumber;

  IndexPage *page = (IndexPage *)bdb->buffer;

  if (dbb->debug & (DEBUG_PAGES | DEBUG_FIND_LEAF)) page->printPage(bdb, false);

  if (page->level == 0) {
    bdb->release(REL_HISTORY);

    bdb = findRoot(dbb, indexId, 0, Exclusive, transId);
    BDB_HISTORY(bdb);
    if (!bdb) return NULL;

    page = (IndexPage *)bdb->buffer;

    if (page->level == 0) {
      if (isRoot) *isRoot = true;
      return bdb;
    }
  }

  while (page->level > 0) {
    IndexNode node(page->findNodeInBranch(indexKey, recordNumber));
    int32 pageNumber = node.getNumber();

    if (pageNumber == END_BUCKET) pageNumber = page->nextPage;

    if (pageNumber == 0) {
      page->printPage(bdb, false);
      // node.parseNode(page->findNodeInBranch (indexKey, recordNumber));
      // // try again for debugging
      bdb->release(REL_HISTORY);
      throw SQLError(DATABASE_CORRUPTION, "index %d damaged", indexId);
    }

    int level = page->level;
    int32 nextPage = page->nextPage;

    bdb = dbb->handoffPage(bdb, pageNumber, PAGE_btree,
                           (page->level > 1) ? Shared : Exclusive);
    BDB_HISTORY(bdb);
    page = (IndexPage *)bdb->buffer;

    // Verify that the new page is either a child page or the next page on
    // the same level.

    ASSERT((page->level == level - 1) || (pageNumber == nextPage));
    ASSERT(page->level > 0 || bdb->lockType == Exclusive);

    if (dbb->debug & (DEBUG_PAGES | DEBUG_FIND_LEAF))
      page->printPage(bdb, false);
  }
  if (isRoot) *isRoot = (bdb->pageNumber == rootPageNumber);
  return bdb;
}

Bdb *IndexRootPage::findRoot(Dbb *dbb, int32 indexId, int32 rootPage,
                             LockType lockType, TransId transId) {
  if (rootPage) return dbb->fetchPage(rootPage, PAGE_btree, lockType);

  ASSERT(indexId >= 0);

  Bdb *bdb = Section::getSectionPage(
      dbb, INDEX_ROOT, indexId / dbb->pagesPerSection, Shared, transId);
  BDB_HISTORY(bdb);

  ASSERT(bdb);

  SectionPage *sections = (SectionPage *)bdb->buffer;
  int32 pageNumber = sections->pages[indexId % dbb->pagesPerSection];

  ASSERT(pageNumber != 0);

  bdb = dbb->handoffPage(bdb, pageNumber, PAGE_btree, lockType);
  BDB_HISTORY(bdb);
  ASSERT(bdb);
  return bdb;
}

void IndexRootPage::scanIndex(Dbb *dbb, int32 indexId, int32 rootPage,
                              IndexKey *lowKey, IndexKey *highKey,
                              int searchFlags, TransId transId,
                              Bitmap *bitmap) {
  IndexKey key;
  uint offset = 0;

  if (dbb->debug & (DEBUG_KEYS | DEBUG_SCAN_INDEX)) {
    LogLock logLock;
    Btn::printKey("lower: ", lowKey, 0, false);
    Btn::printKey("upper: ", highKey, 0, false);
  }

  if (!lowKey) lowKey = &dummyKey;

  /* Find leaf page and position on page */

  Bdb *bdb = findLeaf(dbb, indexId, rootPage, lowKey, Shared, transId);

  if (!bdb) throw SQLError(RUNTIME_ERROR, "can't find index %d", indexId);

  IndexPage *page = (IndexPage *)bdb->buffer;

  if (dbb->debug & (DEBUG_PAGES | DEBUG_SCAN_INDEX))
    page->printPage(bdb, false);

  Btn *end = page->getEnd();
  IndexNode node(page->findNodeInLeaf(lowKey, &key), end);
  UCHAR *endKey = (highKey) ? highKey->key + highKey->keyLength : 0;

  /* If we didn't find it here, try the next page */

  while (node.node >= end) {
    if (!page->nextPage) {
      bdb->release(REL_HISTORY);

      return;
    }

    bdb = dbb->handoffPage(bdb, page->nextPage, PAGE_btree, Shared);
    BDB_HISTORY(bdb);
    page = (IndexPage *)bdb->buffer;
    end = page->getEnd();
    node.parseNode(page->findNodeInLeaf(lowKey, &key), end);
  }

  if (highKey && node.node < end) {
    ASSERT(node.offset + node.length < sizeof(lowKey->key));
    node.expandKey(&key);
    offset = page->computePrefix(&key, highKey);
  }

  /* Scan index setting bits */

  for (;;) {
    for (; node.node < end; node.getNext(end)) {
      if (highKey) {
        if (node.offset <= offset) {
          UCHAR *p = highKey->key + node.offset;
          UCHAR *q = node.key;

          for (int length = node.length; length; --length) {
            if (p >= endKey) {
              if (searchFlags & Partial)  // this is highly suspect
                break;

              bdb->release(REL_HISTORY);

              return;
            }

            if (*p < *q) {
              bdb->release(REL_HISTORY);

              return;
            }

            if (*p++ > *q++) break;

            offset = (int)(p - highKey->key);
          }
        }
      }

      int number = node.getNumber();

      if (number < 0) break;

      bitmap->set(number);
    }

    if (!page->nextPage) {
      bdb->release(REL_HISTORY);

      return;
    }

    bdb = dbb->handoffPage(bdb, page->nextPage, PAGE_btree, Shared);
    BDB_HISTORY(bdb);
    page = (IndexPage *)bdb->buffer;
    node.parseNode(page->nodes);
    end = page->getEnd();
    offset = 0;

    if (dbb->debug & (DEBUG_PAGES | DEBUG_SCAN_INDEX))
      page->printPage(bdb, false);
  }
}

bool IndexRootPage::splitIndexPage(Dbb *dbb, int32 indexId, Bdb *bdb,
                                   TransId transId, AddNodeResult addResult,
                                   IndexKey *indexKey, int recordNumber,
                                   bool isRoot) {
  IndexPage *page = (IndexPage *)bdb->buffer;
  IndexKey splitKey(indexKey->index);
  Bdb *splitBdb;

  if (addResult == SplitEnd)
    splitBdb =
        page->splitIndexPageEnd(dbb, bdb, transId, indexKey, recordNumber);
  else
    splitBdb = page->splitIndexPageMiddle(dbb, bdb, &splitKey, transId);

  IndexPage *splitPage = (IndexPage *)splitBdb->buffer;
  IndexNode splitNode(splitPage);
  splitKey.setKey(splitNode.keyLength(), splitNode.key);
  int32 splitRecordNumber = 0;

  if (splitPage->level == 0) {
    splitRecordNumber = splitNode.getNumber();
    splitKey.appendRecordNumber(splitRecordNumber);
  }

  // If splitting root page, we need to create a new level
  if (isRoot) {
    // Allocate and copy a new left-most index page
    Bdb *leftBdb = dbb->allocPage(PAGE_btree, transId);
    BDB_HISTORY(leftBdb);
    IndexPage *leftPage = (IndexPage *)leftBdb->buffer;
    memcpy(leftPage, page, page->length);
    leftBdb->setPageHeader(leftPage->pageType);

    // Create a node referencing the leftmost page. Assign to it a null key
    // and record number 0 to ensure that all nodes are inserted after it.
    IndexKey leftKey(indexKey->index);

    if (leftPage->level == 0) leftKey.appendRecordNumber(0);

    // Reinitialize the parent page

    page->level = splitPage->level + 1;
    page->version = splitPage->version;
    page->nextPage = 0;
    memset(page->superNodes, 0, sizeof(page->superNodes));
    IndexKey dummy(indexKey->index);
    dummy.keyLength = 0;
    page->length = OFFSET(IndexPage *, nodes);
    page->addNode(dbb, &dummy, END_LEVEL);
    page->addNode(dbb, &leftKey, leftBdb->pageNumber);
    page->addNode(dbb, &splitKey, splitBdb->pageNumber);

    // The order of adding these to the stream log is important.
    // Recovery must write them in this order incase recovery itself crashes.
    IndexPage::logIndexPage(splitBdb, transId, indexKey->index);
    IndexPage::logIndexPage(leftBdb, transId, indexKey->index);
    IndexPage::logIndexPage(bdb, transId, indexKey->index);

    if (dbb->debug & DEBUG_PAGE_LEVEL) {
      Log::debug(
          "\n***** splitIndexPage(%d, %d) - NEW LEVEL: Parent page = %d\n",
          recordNumber, indexId, bdb->pageNumber);
      page->printPage(page, 0, false, false);
      Log::debug(
          "\n***** splitIndexPage(%d, %d) - NEW LEVEL: Left page = %d \n",
          recordNumber, indexId, leftBdb->pageNumber);
      page->printPage(leftPage, 0, true, false);
      Log::debug(
          "\n***** splitIndexPage(%d, %d) - NEW LEVEL: Split page = %d \n",
          recordNumber, indexId, splitBdb->pageNumber);
      page->printPage(splitPage, 0, false, false);
    }

    splitBdb->release(REL_HISTORY);
    leftBdb->release(REL_HISTORY);
    bdb->release(REL_HISTORY);

    return false;
  }

  int splitPageLevel = splitPage->level;
  int splitPageNumber = splitBdb->pageNumber;

  IndexPage::logIndexPage(splitBdb, transId, indexKey->index);
  splitBdb->release(REL_HISTORY);

  IndexPage::logIndexPage(bdb, transId, indexKey->index);
  bdb->release(REL_HISTORY);

  // We need to insert the first key of the newly created parent page
  // into the parent page.
  for (;;) {
    Bdb *rootBdb = findRoot(dbb, indexId, 0, Exclusive, transId);
    int rootPageNumber = rootBdb->pageNumber;
    BDB_HISTORY(rootBdb);

    // Find parent page
    Bdb *parentBdb =
        IndexPage::findLevel(dbb, indexId, rootBdb, splitPageLevel + 1,
                             &splitKey, splitRecordNumber);
    BDB_HISTORY(parentBdb);
    parentBdb->mark(transId);
    IndexPage *parentPage = (IndexPage *)parentBdb->buffer;
    AddNodeResult result = parentPage->addNode(dbb, &splitKey, splitPageNumber);

    if (result == NodeAdded || result == Duplicate) {
      // Node added to parent page
      // Log parent page.
      if (result == NodeAdded) {
        IndexPage::logIndexPage(parentBdb, transId, indexKey->index);
      }

      parentBdb->release(REL_HISTORY);
      return false;
    }

    // Parent page needs to be split.Recurse
    ASSERT(result == SplitMiddle || result == SplitEnd || result == NextPage);

    if (splitIndexPage(dbb, indexId, parentBdb, transId, result, &splitKey,
                       splitPageNumber,
                       (parentBdb->pageNumber == rootPageNumber)))
      return true;
  }
}

bool IndexRootPage::deleteIndexEntry(Dbb *dbb, int32 indexId, IndexKey *key,
                                     int32 recordNumber, TransId transId) {
  IndexPage *page;
  IndexKey searchKey(key);
  searchKey.appendRecordNumber(recordNumber);

  if (dbb->debug & (DEBUG_KEYS | DEBUG_NODES_DELETED)) {
    Btn::printKey("deletion key", key, 0, false);
    Btn::printKey("  search key", &searchKey, 0, false);
  }

  // Try to delete node.  If necessary, chase to next page.

  for (Bdb *bdb =
           findInsertionLeaf(dbb, indexId, &searchKey, recordNumber, transId);
       bdb;
       bdb = dbb->handoffPage(bdb, page->nextPage, PAGE_btree, Exclusive)) {
    BDB_HISTORY(bdb);
    page = (IndexPage *)bdb->buffer;
    bdb->mark(transId);
    int result = page->deleteNode(dbb, key, recordNumber);

    if (result || page->nextPage == 0) {
      bdb->release(REL_HISTORY);

      return result > 0;
    }
  }

  return false;
}

void IndexRootPage::deleteIndex(Dbb *dbb, int32 indexId, TransId transId) {
  Bdb *bdb = Section::getSectionPage(
      dbb, INDEX_ROOT, indexId / dbb->pagesPerSection, Exclusive, transId);
  BDB_HISTORY(bdb);
  if (!bdb) return;

  bdb->mark(transId);
  SectionPage *sections = (SectionPage *)bdb->buffer;
  int32 firstPageNumAtNextLevel =
      sections->pages[indexId % dbb->pagesPerSection];
  sections->pages[indexId % dbb->pagesPerSection] = 0;
  dbb->nextIndex = MIN(dbb->nextIndex, indexId);
  bdb->release(REL_HISTORY);

  while (firstPageNumAtNextLevel) {
    bdb = dbb->fetchPage(firstPageNumAtNextLevel, PAGE_any, Exclusive);
    BDB_HISTORY(bdb);
    IndexPage *page = (IndexPage *)bdb->buffer;

    if (page->pageType != PAGE_btree) {
      Log::logBreak("Drop index %d: bad level page %d\n", indexId,
                    bdb->pageNumber);
      bdb->release(REL_HISTORY);

      break;
    }

    IndexNode node(page->nodes);
    firstPageNumAtNextLevel = (page->level) ? node.getNumber() : 0;

    for (;;) {
      int32 nextPage = page->nextPage;
      dbb->freePage(bdb, transId);

      if (!nextPage) break;

      bdb = dbb->fetchPage(nextPage, PAGE_any, Exclusive);
      BDB_HISTORY(bdb);
      page = (IndexPage *)bdb->buffer;

      if (page->pageType != PAGE_btree) {
        Log::logBreak("Drop index %d: bad index page %d\n", indexId,
                      bdb->pageNumber);
        bdb->release(REL_HISTORY);
        break;
      }
    }
  }
}

void IndexRootPage::debugBucket(Dbb *dbb, int indexId, int recordNumber,
                                TransId transactionId) {
  Bdb *bdb = findRoot(dbb, indexId, 0, Exclusive, transactionId);
  BDB_HISTORY(bdb);
  IndexPage *page;
  IndexNode node;
  int debug = 1;

  // Find leaf

  for (;;) {
    page = (IndexPage *)bdb->buffer;

    if (debug) page->printPage(bdb, false);

    node.parseNode(page->nodes);

    if (page->level == 0) break;

    bdb = dbb->handoffPage(bdb, node.getNumber(), PAGE_btree, Exclusive);
    BDB_HISTORY(bdb);
  }

  // Scan index

  Btn *end = page->getEnd();
  int pages = 0;
  int nodes = 0;

  /* If we didn't find it here, try the next page */

  for (;;) {
    ++pages;

    for (; node.node < end; node.getNext(end)) {
      ++nodes;
      int number = node.getNumber();

      if (number < 0) break;

      if (number == recordNumber) page->printPage(bdb, false);
    }

    if (!page->nextPage) break;

    bdb = dbb->handoffPage(bdb, page->nextPage, PAGE_btree, Exclusive);
    BDB_HISTORY(bdb);
    page = (IndexPage *)bdb->buffer;

    if (debug) page->printPage(bdb, false);

    node.parseNode(page->nodes);
    end = page->getEnd();
  }

  bdb->release(REL_HISTORY);
}

void IndexRootPage::redoIndexPage(Dbb *dbb, int32 pageNumber, int level,
                                  int32 nextPage, int length, const UCHAR *data,
                                  bool haveSuperNodes) {
  // Log::debug("redoIndexPage %d -> %d -> %d level %d, parent %d)\n",
  // priorPage, pageNumber, nextPage, level, parentPage);
  Bdb *bdb = dbb->fakePage(pageNumber, PAGE_btree, NO_TRANSACTION);
  BDB_HISTORY(bdb);

  IndexPage *indexPage = (IndexPage *)bdb->buffer;
  indexPage->level = level;
  indexPage->nextPage = nextPage;

  if (haveSuperNodes) {
    indexPage->length = length + (int32)OFFSET(IndexPage *, superNodes);
    memcpy(indexPage->superNodes, data, length);
  } else {
    indexPage->length = length + (int32)OFFSET(IndexPage *, nodes);
    memcpy(indexPage->nodes, data, length);
    memset(indexPage->superNodes, 0, sizeof(indexPage->superNodes));
  }
  bdb->release(REL_HISTORY);
}

void IndexRootPage::redoIndexDelete(Dbb *dbb, int indexId) {
  ASSERT(dbb->streamLog->recovering);
  int sequence = indexId / dbb->pagesPerSection;
  int slot = indexId % dbb->pagesPerSection;
  Bdb *bdb = Section::getSectionPage(dbb, INDEX_ROOT, sequence, Exclusive,
                                     NO_TRANSACTION);
  BDB_HISTORY(bdb);
  bdb->mark(NO_TRANSACTION);
  SectionPage *sections = (SectionPage *)bdb->buffer;
  sections->pages[slot] = 0;
  bdb->release(REL_HISTORY);
}

void IndexRootPage::indexMerge(Dbb *dbb, int indexId, SRLUpdateIndex *logRecord,
                               TransId transId) {
  IndexKey key;
  int recordNumber = logRecord->nextKey(&key);
  int duplicates = 0;
  int insertions = 0;
  int punts = 0;
  int rollovers = 0;

  for (; recordNumber != -1;) {
    // Position to insert first key (clone of addIndexEntry)

    IndexKey searchKey(key);
    searchKey.appendRecordNumber(recordNumber);

    if (dbb->debug & (DEBUG_KEYS | DEBUG_NODES_DELETED | DEBUG_PAGE_LEVEL)) {
      LogLock logLock;
      Log::debug("\n***** indexMerge(%d, %d) - Key\n", recordNumber, indexId);
      Btn::printKey("insertion key", &key, 0, false);
      Btn::printKey(" appended key", &searchKey, 0, false);
    }

    Bdb *bdb =
        findInsertionLeaf(dbb, indexId, &searchKey, recordNumber, transId);
    ASSERT(bdb);
    IndexPage *page = (IndexPage *)bdb->buffer;
    Btn *bucketEnd = (Btn *)((char *)page + page->length);
    IndexKey priorKey;
    IndexNode node(page->findInsertionPoint(&key, recordNumber, &priorKey));
    IndexKey nextKey;
    nextKey.setKey(0, node.offset, priorKey.key);
    node.expandKey(&nextKey);
    int number = node.getNumber();

    // If we need to go to the next page, do it now

    while (number == END_BUCKET && nextKey.compare(&key) > 0) {
      if (!page->nextPage) {
        bdb->release(REL_HISTORY);
        bdb = NULL;
        ++punts;
        addIndexEntry(dbb, indexId, &key, recordNumber, transId);

        if ((recordNumber = logRecord->nextKey(&key)) == -1)
          // return;
          goto exit;

        break;
      }

      nextKey.compare(&key);
      ASSERT(bdb->pageNumber != page->nextPage);
      bdb = dbb->handoffPage(bdb, page->nextPage, PAGE_btree, Exclusive);
      BDB_HISTORY(bdb);
      page = (IndexPage *)bdb->buffer;
      node.parseNode(page->findInsertionPoint(&key, recordNumber, &priorKey));
      nextKey.setKey(0, node.offset, priorKey.key);
      node.expandKey(&nextKey);
      number = node.getNumber();
      ++rollovers;
    }

    if (!bdb) continue;

    bdb->mark(transId);

    for (;;) {
      if (number == recordNumber && nextKey.compare(&key) == 0)
        ++duplicates;
      else {
        AddNodeResult result =
            page->addNode(dbb, &key, recordNumber, &node, &priorKey, &nextKey);
        if (result != NodeAdded)
        // If node doesn't fit, punt and let someone else do it
        {
          bdb->release(REL_HISTORY);
          bdb = NULL;
          ++punts;
          addIndexEntry(dbb, indexId, &key, recordNumber, transId);

          if ((recordNumber = logRecord->nextKey(&key)) == -1)
            // return;
            goto exit;
          break;
        } else /* node inserted */
          ++insertions;
      }
      priorKey.setKey(&key);

      // Get next key

      if ((recordNumber = logRecord->nextKey(&key)) == -1) break;

      // If the key is out of order, somebody screwed up.  Punt out of here

      if (key.compare(&priorKey) > 0) ASSERT(false);

      // Find the next insertion point, compute the next key, etc.

      bucketEnd = (Btn *)((char *)page + page->length);
      node.parseNode(page->findInsertionPoint(0, &key, recordNumber, &priorKey,
                                              node.node, bucketEnd));
      nextKey.setKey(0, node.offset, priorKey.key);
      node.expandKey(&nextKey);
      number = node.getNumber();

      if (number != END_LEVEL && nextKey.compare(&key) > 0) break;
    }

    if (bdb) bdb->release(REL_HISTORY);
  }

exit:;

  // Log::debug("indexMerge: index %d, %d insertions, %d punts, %d duplicates,
  // %d rollovers\n",  indexId, insertions, punts, duplicates, rollovers);
}

void IndexRootPage::redoCreateIndex(Dbb *dbb, int indexId, int pageNumber) {
  Bdb *bdb =
      Section::getSectionPage(dbb, INDEX_ROOT, indexId / dbb->pagesPerSection,
                              Exclusive, NO_TRANSACTION);
  BDB_HISTORY(bdb);
  ASSERT(bdb);

  createIndexRoot(dbb, NO_TRANSACTION, pageNumber, indexId, bdb);
  bdb->release(REL_HISTORY);
}

void IndexRootPage::createIndexRoot(Dbb *dbb, TransId transId, int pageNumber,
                                    int id, Bdb *sectionsBdb) {
  Bdb *bdb;

  if (dbb->streamLog->recovering)
    // Use given page number.
    bdb = dbb->fakePage(pageNumber, PAGE_btree, transId);
  else {
    // This is not recovery , allocate a new page and log it.
    bdb = dbb->allocPage(PAGE_btree, transId);
    dbb->streamLog->logControl->createIndex.append(
        dbb, transId, id, INDEX_CURRENT_VERSION, bdb->pageNumber);
  }
  BDB_HISTORY(bdb);

  IndexPage::initRootPage(bdb);

  // link index root to sections
  SectionPage *sections = (SectionPage *)sectionsBdb->buffer;
  sections->pages[id % dbb->pagesPerSection] = bdb->pageNumber;
  sectionsBdb->mark(transId);
  bdb->release(REL_HISTORY);
}

void IndexRootPage::analyzeIndex(Dbb *dbb, int indexId,
                                 IndexAnalysis *indexAnalysis) {
  Bdb *bdb = findRoot(dbb, indexId, 0, Shared, NO_TRANSACTION);
  BDB_HISTORY(bdb);

  if (!bdb) return;

  IndexPage *page = (IndexPage *)bdb->buffer;
  indexAnalysis->levels = page->level + 1;
  int32 nextLevel;
  bool first = true;

  for (;;) {
    if (first) {
      IndexNode node(page);
      nextLevel = node.getNumber();
    }

    if (page->level == 0) {
      ++indexAnalysis->leafPages;
      indexAnalysis->leafSpaceUsed += page->length;
    } else
      ++indexAnalysis->upperLevelPages;

    if (page->nextPage) {
      bdb = dbb->handoffPage(bdb, page->nextPage, PAGE_btree, Shared);
      BDB_HISTORY(bdb);
    } else {
      bdb->release(REL_HISTORY);

      if (page->level == 0) break;

      bdb = dbb->fetchPage(nextLevel, PAGE_btree, Shared);
      BDB_HISTORY(bdb);
      first = true;
    }

    page = (IndexPage *)bdb->buffer;
  }
}

int32 IndexRootPage::getIndexRoot(Dbb *dbb, int indexId) {
  Bdb *bdb = Section::getSectionPage(
      dbb, INDEX_ROOT, indexId / dbb->pagesPerSection, Shared, NO_TRANSACTION);
  BDB_HISTORY(bdb);
  if (!bdb) return 0;

  SectionPage *sections = (SectionPage *)bdb->buffer;
  int32 pageNumber = sections->pages[indexId % dbb->pagesPerSection];
  bdb->release(REL_HISTORY);

  return pageNumber;
}

void IndexRootPage::positionIndex(Dbb *dbb, int indexId, int32 rootPage,
                                  WalkIndex *walkIndex) {
  IndexKey *key = &walkIndex->indexKey;
  uint offset = 0;
  IndexKey *lowKey = &walkIndex->lowerBound;
  IndexKey *highKey = &walkIndex->upperBound;
  TransId transId = walkIndex->transaction->transactionId;

  if (dbb->debug & (DEBUG_KEYS | DEBUG_SCAN_INDEX)) {
    LogLock logLock;
    Btn::printKey("lower: ", lowKey, 0, false);
    Btn::printKey("upper: ", highKey, 0, false);
  }

  if (!lowKey) lowKey = &dummyKey;

  /* Find leaf page and position on page */

  Bdb *bdb = findLeaf(dbb, indexId, rootPage, lowKey, Shared, transId);

  if (!bdb) throw SQLError(RUNTIME_ERROR, "can't find index %d", indexId);

  IndexPage *page = (IndexPage *)bdb->buffer;

  if (dbb->debug & (DEBUG_PAGES | DEBUG_SCAN_INDEX))
    page->printPage(bdb, false);

  Btn *end = page->getEnd();
  IndexNode node(page->findNodeInLeaf(lowKey, key), end);

  /* If we didn't find it here, try the next page */

  while (node.node >= end) {
    if (!page->nextPage) {
      bdb->release(REL_HISTORY);

      return;
    }

    bdb = dbb->handoffPage(bdb, page->nextPage, PAGE_btree, Shared);
    BDB_HISTORY(bdb);
    page = (IndexPage *)bdb->buffer;
    end = page->getEnd();
    node.parseNode(page->findNodeInLeaf(lowKey, key), end);
  }

  if (highKey && node.node < end) {
    ASSERT(node.offset + node.length < sizeof(lowKey->key));
    node.expandKey(key);
    offset = page->computePrefix(key, highKey);
  }

  walkIndex->setNodes(
      page->nextPage,
      page->length - (int)((UCHAR *)node.node - (UCHAR *)page->nodes),
      node.node);
  bdb->release(REL_HISTORY);
}

void IndexRootPage::repositionIndex(Dbb *dbb, int indexId,
                                    WalkIndex *walkIndex) {
  Bdb *bdb = dbb->fetchPage(walkIndex->nextPage, PAGE_btree, Shared);
  IndexPage *page = (IndexPage *)bdb->buffer;
  walkIndex->setNodes(page->nextPage, page->length - OFFSET(IndexPage *, nodes),
                      page->nodes);
  bdb->release(REL_HISTORY);
}

}  // namespace Changjiang
