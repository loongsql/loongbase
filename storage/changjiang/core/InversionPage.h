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

// InversionPage.h: interface for the InversionPage class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Page.h"

namespace Changjiang {

#define NEXT_INV(node) ((Inv *)(node->key + node->length))
#define INV_SIZE (sizeof(Inv) - 1)

struct Inv {
  UCHAR offset;
  UCHAR length;
  UCHAR key[1];

 public:
  static bool validate(Inv *node, int keyLength, UCHAR *expandedKey);
  static void printKey(int length, UCHAR *key);
  static void encode(ULONG number, UCHAR **ptr);
  static int32 decode(UCHAR **ptr);
  void printKey(UCHAR *expandedKey);
};

class Dbb;
class Bdb;
class Bitmap;
class IndexKey;

class InversionPage : public Page {
 public:
  static void logPage(Bdb *bdb);
  void analyze(int pageNumber);
  void removeNode(Dbb *dbb, int keyLength, UCHAR *key);
  void validate(Dbb *dbb, Validation *validation, Bitmap *pages);
  void validate();
  void printPage(Bdb *bdb);
  Bdb *splitInversionPage(Dbb *dbb, Bdb *bdb, IndexKey *indexKey,
                          TransId transId);
  Inv *findNode(int keyLength, UCHAR *key, UCHAR *expandedKey,
                int *expandedKeyLength);
  bool addNode(Dbb *dbb, IndexKey *indexKey);
  int computePrefix(int l1, UCHAR *v1, int l2, UCHAR *v2);
  void backup(EncodedDataStream *stream);
  void restore(EncodedDataStream *stream);

  int32 parentPage;
  int32 priorPage;
  int32 nextPage;
  short length;
  Inv nodes[1];
};

}  // namespace Changjiang
