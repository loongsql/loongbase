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

// Scan.h: interface for the Scan class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "ScanType.h"

namespace Changjiang {

#define TABLE_ID 0
#define RECORD_NUMBER 1
#define FIELD_ID 2
#define WORD_NUMBER 3

class Dbb;
class Bdb;
class InversionPage;
class Database;
class SearchWords;

struct Inv;

class Scan {
 public:
  bool validateWord(int32 *numbers);
  int32 *getNextValid();
  bool validate(Scan *prior);
  int32 *getNumbers();
  Scan(SearchWords *searchWords);
  Scan(ScanType typ, const char *word, SearchWords *searchWords);
  virtual ~Scan();

  bool hit(int32 wordPosition);
  int32 *fetch();
  void addWord(const char *word);
  void setKey(const char *word);
  int compare(int count, int32 *nums);
  bool hit();
  void print();
  int32 *getNext();
  void fini();
  bool start(Database *database);

  Bdb *bdb;
  Inv *node;
  int keyLength;
  int expandedLength;
  InversionPage *page;
  UCHAR caseMask;
  UCHAR searchMask;
  UCHAR key[MAX_INV_WORD];
  UCHAR expandedKey[MAX_INV_WORD];
  bool eof;
  bool frequentWord;
  bool frequentTail;
  UCHAR *rootEnd;
  Scan *next;
  ScanType type;
  Inversion *inversion;
  SearchWords *searchWords;
  Dbb *dbb;
  int32 numbers[4];
  int hits;
  UCHAR guardByte;
};

}  // namespace Changjiang
