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

// RepositoryVolume.h: interface for the RepositoryVolume class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "SyncObject.h"

namespace Changjiang {

class Repository;
class Dbb;
class Database;
class Transaction;
class TransactionState;
class BlobReference;
class IndexKey;
class Section;
class TableSpace;

class RepositoryVolume {
 public:
  RepositoryVolume(Repository *repo, int volume, JString file);
  virtual ~RepositoryVolume();

  void setName(const char *name);
  JString getName();
  void deleteBlob(int64 blobId, Transaction *transaction);
  void getBlob(BlobReference *reference);
  void create();
  int64 getRepositorySize();
  void storeBlob(BlobReference *blob, TransactionState *transaction);
  void close();
  void reportStatistics();
  void storeBlob(int64 blobId, Stream *stream, TransactionState *transaction);
  void synchronize(int64 id, Stream *stream, Transaction *transaction);
  int64 reverseKey(UCHAR *key);
  void synchronize(Transaction *transaction);
  void scavenge();

 protected:
  int compare(Stream *stream1, Stream *stream2);
  void fetchRecord(int recordNumber, Stream *stream);
  int getRecordNumber(IndexKey *indexKey);
  int getRecordNumber(int64 blobId);
  void makeWritable();
  void open();
  int makeKey(int64 value, IndexKey *indexKey);

 public:
  int volumeNumber;
  int32 rootPage;
  Repository *repository;
  RepositoryVolume *collision;
  TableSpace *tableSpace;
  SyncObject syncObject;
  JString fileName;
  Section *section;
  Dbb *dbb;
  Database *database;
  time_t lastAccess;
  bool isOpen;
  bool isWritable;
};

}  // namespace Changjiang
