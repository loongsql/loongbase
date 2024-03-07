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

#pragma once

#include "IndexKey.h"

namespace Changjiang {

class Index;
class Transaction;
class Record;
class Table;

class IndexWalker {
 public:
  IndexWalker(Index *index, Transaction *transaction, int flags);
  virtual ~IndexWalker(void);

  virtual Record *getNext(bool lockForUpdate);

  Record *getValidatedRecord(int32 recordId, bool lockForUpdate);
  int compare(IndexWalker *node1, IndexWalker *node2);
  void addWalker(IndexWalker *walker);
  bool insert(IndexWalker *node);
  void rotateLeft(void);
  void rotateRight(void);
  void rebalance(void);
  bool rebalanceDelete();
  IndexWalker *getSuccessor(IndexWalker **parentPointer, bool *shallower);
  void remove(void);
  void rebalanceUpward(int delta);
  // debugging routines
  int validate(void);
  void corrupt(const char *text);

  Index *index;
  Transaction *transaction;
  Record *currentRecord;
  Table *table;
  IndexWalker *next;
  IndexWalker *higher;
  IndexWalker *lower;
  IndexWalker *parent;
  UCHAR *key;
  uint keyLength;
  int32 recordNumber;
  int32 lastRecordNumber;
  int balance;
  int searchFlags;
  bool first;
  bool firstRecord;
};

}  // namespace Changjiang
