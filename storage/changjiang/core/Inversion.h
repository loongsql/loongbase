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

// Inversion.h: interface for the Inversion class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "SyncObject.h"

namespace Changjiang {

#define MAX_INV_KEY 128
#define MAX_INV_WORD 100

static const int INVERSION_VERSION_NUMBER = 0;

class Dbb;
class InversionFilter;
class Bdb;
class ResultList;
class Validation;
class IndexKey;

class Inversion {
 public:
  void removeWord(int keyLength, UCHAR *key, TransId transId);
  void removeFromInversion(InversionFilter *filter, TransId transId);
  void validate(Validation *validation);
  void flush(TransId transId);
  static int compare(UCHAR *key1, UCHAR *key2);
  void sort(UCHAR **records, int size);
  void summary();
  void deleteInversion(TransId transId);
  Bdb *findInversion(IndexKey *indexKey, LockType lockType);
  void propagateSplit(int level, IndexKey *indexKey, int32 pageNumber,
                      TransId transId);
  void updateInversionRoot(int32 pageNumber, TransId transId);
  Bdb *findRoot(LockType lockType);
  void addWord(int keyLength, UCHAR *key, TransId transId);
  void init();
  void createInversion(TransId transId);
  int32 addInversion(InversionFilter *filter, TransId transId, bool insertFlag);
  Inversion(Dbb *db);
  virtual ~Inversion();

  SyncObject syncObject;
  bool inserting;
  Dbb *dbb;
  UCHAR **sortBuffer;
  int records;
  int size;
  UCHAR *lastRecord;
  int wordsAdded;
  int wordsRemoved;
  int runs;
  int state;
};

}  // namespace Changjiang
