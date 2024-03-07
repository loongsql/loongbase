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

// IndexPage.h: interface for the IndexPage class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Page.h"
#include "Btn.h"
#include "IndexNode.h"

namespace Changjiang {

static const int SUPERNODES = 16;

class Dbb;
class Bdb;
class Bitmap;
class IndexKey;
class Index;

class IndexPage : public Page {
 public:
  AddNodeResult addNode(Dbb *dbb, IndexKey *key, int32 recordNumber);
  AddNodeResult addNode(Dbb *dbb, IndexKey *key, int32 recordNumber,
                        IndexNode *node, IndexKey *priorKey, IndexKey *nextKey);
  Btn *appendNode(IndexKey *indexKey, int32 recordNumber, int pageSize);
  int deleteNode(Dbb *dbb, IndexKey *key, int32 recordNumber);
  Btn *findNodeInBranch(IndexKey *indexKey, int32 recordNumber);
  Btn *findNodeInLeaf(IndexKey *key, IndexKey *foundKey);
  Btn *findInsertionPoint(IndexKey *indexKey, int32 recordNumber,
                          IndexKey *expandedKey);
  Btn *findInsertionPoint(int level, IndexKey *indexKey, int32 recordNumber,
                          IndexKey *expandedKey, Btn *from, Btn *bucketEnd);
  Bdb *splitPage(Dbb *dbb, Bdb *bdb, TransId transId);
  Bdb *splitIndexPageEnd(Dbb *dbb, Bdb *bdb, TransId transId,
                         IndexKey *insertKey, int recordNumber);
  Bdb *splitIndexPageMiddle(Dbb *dbb, Bdb *bdb, IndexKey *splitKey,
                            TransId transId);
  bool isLastNode(Btn *node);

  void analyze(int pageNumber);
  // void		validateInsertion (int keyLength, UCHAR * key, int32
  // recordNumber);
  void validateNodes(Dbb *dbb, Validation *validation, Bitmap *children,
                     int32 parentPageNumber);
  void validate(Dbb *dbb, Validation *validation, Bitmap *pages,
                int32 parentPageNumber);
  void validate(void *before);
  bool checkAddSuperNode(int pageSize, IndexNode *node, IndexKey *indexKey,
                         int recordNumber, int offset, bool *makeNextSuper);
  void moveMemory(void *dst, void *src);
  void addSupernode(Btn *where);
  bool deleteSupernode(Btn *where);
  Btn *findSupernode(int level, UCHAR *key, size_t len, int32 recordNumber,
                     Btn *after, bool *match);
  Btn *findPriorNodeForSupernode(Btn *where, IndexKey *priorKey);
  Btn *getEnd(void);
  static void initRootPage(Bdb *bdb);

  void backup(EncodedDataStream *stream);
  void restore(EncodedDataStream *stream);

  static int computePrefix(IndexKey *key1, IndexKey *key2);
  static Bdb *findLevel(Dbb *dbb, int32 indexId, Bdb *bdb, int level,
                        IndexKey *indexKey, int32 recordNumber);
  static void printPage(Bdb *bdb, bool inversion);
  static void printPage(IndexPage *page, int32 pageNumber, bool inversion);
  static void printPage(IndexPage *page, int32 pageNum, bool printDetail,
                        bool inversion);
  static void printNode(int i, IndexPage *page, int32 pageNumber,
                        IndexNode &node, bool inversion = false);
  static void printNode(IndexPage *page, int32 pageNumber, Btn *node,
                        bool inversion = false);
  static int32 getRecordNumber(const UCHAR *ptr);
  static void logIndexPage(Bdb *bdb, TransId transId, Index *index);

  int32 unused[2];  // used to be parent and prior pages
  int32 nextPage;
  // short	level;
  UCHAR level;
  UCHAR version;
  uint16 length;
  short superNodes[SUPERNODES];
  Btn nodes[1];
};

}  // namespace Changjiang
