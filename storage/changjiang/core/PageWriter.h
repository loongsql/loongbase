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

// PageWriter.h: interface for the PageWriter class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "SyncObject.h"
#include "Mutex.h"

namespace Changjiang {

class Cache;
class Database;
class Dbb;
class Thread;
class Transaction;
struct DirtyTrans;

struct DirtyPage {
  DirtyPage *nextPage;
  DirtyPage *priorPage;
  DirtyPage *pageCollision;
  DirtyTrans *transaction;
  DirtyPage *transNext;
  DirtyPage *transPrior;
  Dbb *dbb;
  int32 pageNumber;
};

struct DirtyTrans {
  DirtyTrans *collision;
  DirtyTrans *next;
  TransId transactionId;
  DirtyPage *firstPage;
  DirtyPage *lastPage;
  Thread *thread;

  void removePage(DirtyPage *page);
  void clear();
  void addPage(DirtyPage *page);
};

static const int pagesPerLinen = 200;
static const int dirtyHashSize = 101;

struct DirtyLinen {
  DirtyLinen *next;
  DirtyPage dirtyPages[pagesPerLinen];
};

struct DirtySocks {
  DirtySocks *next;
  DirtyTrans transactions[pagesPerLinen];
};

class PageWriter {
 public:
  void validate();
  void shutdown(bool panic);
  void release(DirtyTrans *transaction);
  DirtyTrans *getDirtyTransaction(TransId transactionId);
  DirtyTrans *findDirtyTransaction(TransId transactionId);
  void waitForWrites(Transaction *transaction);
  void removeElement(DirtyPage *element);
  void pageWritten(Dbb *dbb, int32 pageNumber);
  void start();
  void writer();
  static void writer(void *arg);
  void release(DirtyPage *element);
  DirtyPage *getElement();
  void writePage(Dbb *dbb, int32 pageNumber, TransId transactionId);
  PageWriter(Database *db);
  virtual ~PageWriter();

  DirtyPage *firstPage;
  DirtyPage *lastPage;
  DirtyPage *freePages;
  DirtyTrans *freeTransactions;
  DirtyLinen *dirtyLinen;
  DirtySocks *dirtySocks;
  Cache *cache;
  Database *database;
  Mutex syncObject;
  Thread *thread;
  DirtyPage *pageHash[dirtyHashSize];
  DirtyTrans *transactions[dirtyHashSize];
  bool shuttingDown;
};

}  // namespace Changjiang
