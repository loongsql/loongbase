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

// IndexRootPage.h: interface for the IndexRootPage class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "RootPage.h"
#include "Index2Page.h"
#include "SynchronizationObject.h"

namespace Changjiang {

class Dbb;
class Bdb;
class Bitmap;
class Btn;
class IndexKey;
class SRLUpdateIndex;
struct IndexAnalysis;

class Index2RootPage : public RootPage {
 public:
  static void debugBucket(Dbb *dbb, int indexId, int recordNumber,
                          TransId transactionId);
  static void deleteIndex(Dbb *dbb, int32 indexId, TransId transId);
  static bool deleteIndexEntry(Dbb *dbb, int32 indexId, IndexKey *key,
                               int32 recordNumber, TransId transId);
  static bool splitIndexPage(Dbb *dbb, int32 indexId, Bdb *bdb, TransId transId,
                             AddNodeResult addResult, IndexKey *indexKey,
                             int recordNumber);
  static void scanIndex(Dbb *dbb, int32 indexId, int32 rootPage, IndexKey *low,
                        IndexKey *high, int searchFlags, TransId transId,
                        Bitmap *bitmap);
  static Bdb *findRoot(Dbb *dbb, int32 indexId, int32 rootPage,
                       LockType lockType, TransId transId);
  static Bdb *findLeaf(Dbb *dbb, int32 indexId, int32 rootPage, IndexKey *key,
                       LockType lockType, TransId transId);
  static Bdb *findInsertionLeaf(Dbb *dbb, int32 indexId, IndexKey *key,
                                int32 recordNumber, TransId transId);
  static bool addIndexEntry(Dbb *dbb, int32 indexId, IndexKey *key,
                            int32 recordNumber, TransId transId);
  static int32 createIndex(Dbb *dbb, TransId transId);
  static void create(Dbb *dbb, TransId transId);
  static void indexMerge(Dbb *dbb, int indexId, SRLUpdateIndex *indexNodes,
                         TransId transId);
  static Bdb *createIndexRoot(Dbb *dbb, TransId transId);
  static void analyzeIndex(Dbb *dbb, int indexId, IndexAnalysis *indexAnalysis);
  static int32 getIndexRoot(Dbb *dbb, int indexId);

  static void redoIndexPage(Dbb *dbb, int32 pageNumber, int32 parentPageNumber,
                            int level, int32 prior, int32 next, int length,
                            const UCHAR *data);
  static void setIndexRoot(Dbb *dbb, int indexId, int32 pageNumber,
                           TransId transId);
  static void redoIndexDelete(Dbb *dbb, int indexId);
  static void redoCreateIndex(Dbb *dbb, int indexId);
};

}  // namespace Changjiang
