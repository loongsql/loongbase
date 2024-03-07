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

// Repository.h: interface for the Repository class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "SyncObject.h"
#include "Engine.h"  // Added by ClassView

namespace Changjiang {

static const int VOLUME_HASH_SIZE = 25;

enum RolloverPeriod {
  RolloverNone = 0,
  RolloverWeekly,
  RolloverMonthly,
};

class Database;
class Sequence;
class Value;
CLASS(Field);
class Transaction;
class TransactionState;
class BlobReference;
class RepositoryVolume;

class Repository {
 public:
  Repository(const char *repositoryName, const char *repositorySchema,
             Database *db, Sequence *seq, const char *fileName,
             const char *rollovers, int volume);
  virtual ~Repository();

  void close();
  void reportStatistics();
  void synchronize(const char *fileName, Transaction *transaction);
  void scavenge();
  void deleteBlob(int volumeNumber, int64 blobId, Transaction *transaction);
  void drop();
  void setRollover(const char *string);
  void setVolume(int volume);
  void getBlob(BlobReference *reference);
  void setFilePattern(const char *pattern);
  void setSequence(Sequence *seq);
  JString genFileName(int volume);
  void storeBlob(BlobReference *blob, TransactionState *transaction);
  Value *defaultRepository(Field *field, Value *value, Value *alt);
  void save();
  RepositoryVolume *findVolume(int volumeNumber);
  RepositoryVolume *getVolume(int volumeNumber);

  static RolloverPeriod getRolloverPeriod(const char *token);
  static int64 getFileSize(const char *token);
  static bool getToken(const char **ptr, int sizeToken, char *token);
  static void validateRollovers(const char *string);

  const char *name;
  const char *schema;
  JString filePattern;
  JString rolloverString;
  Repository *collision;
  Database *database;
  Sequence *sequence;
  int currentVolume;
  int64 maxSize;
  RolloverPeriod rolloverPeriod;
  SyncObject syncObject;
  RepositoryVolume *volumes[VOLUME_HASH_SIZE];
};

}  // namespace Changjiang
