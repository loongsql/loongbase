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

// Index2Page.h: interface for the Index2Page class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Page.h"
#include "Btn.h"

namespace Changjiang {

class Dbb;
class Bdb;
class Bitmap;
class IndexKey;

class Index2Page : public Page {
 public:
  AddNodeResult addNode(Dbb *dbb, IndexKey *key, int32 recordNumber);
  Btn *appendNode(IndexKey *indexKey, int32 recordNumber, int pageSize);
  Btn *appendNode(int length, UCHAR *key, int32 recordNumber, int pageSize);
  int deleteNode(Dbb *dbb, IndexKey *key, int32 recordNumber);
  Btn *findNodeInBranch(IndexKey *indexKey);
  Btn *findNode(IndexKey *key, IndexKey *expandedKey);
  Btn *findInsertionPoint(IndexKey *indexKey, int32 recordNumber,
                          IndexKey *expandedKey);
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

  static int computePrefix(int l1, UCHAR *v1, int l2, UCHAR *v2);
  static int computePrefix(IndexKey *key1, IndexKey *key2);
  static int keyCompare(int length1, UCHAR *key1, int length2, UCHAR *key2);
  static Bdb *findLevel(Dbb *dbb, Bdb *bdb, int level, IndexKey *indexKey,
                        int32 recordNumber);
  static Bdb *createNewLevel(Dbb *dbb, int level, int version, int32 page1,
                             int32 page2, IndexKey *key2, TransId transId);
  static void printPage(Bdb *bdb, bool inversion);
  static void printPage(Index2Page *page, int32 pageNumber, bool inversion);
  static int32 getRecordNumber(const UCHAR *ptr);
  static void logIndexPage(Bdb *bdb, TransId transId);
  static Btn *findInsertionPoint(int level, IndexKey *indexKey,
                                 int32 recordNumber, IndexKey *expandedKey,
                                 Btn *nodes, Btn *bucketEnd);

  int32 parentPage;
  int32 priorPage;
  int32 nextPage;
  // short	level;
  UCHAR level;
  UCHAR version;
  short length;
  Btn nodes[1];
};

}  // namespace Changjiang
