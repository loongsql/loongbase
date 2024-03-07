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

// SequenceManager.h: interface for the SequenceManager class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "SyncObject.h"

namespace Changjiang {

#define SEQUENCE_HASH_SIZE 101

class Database;
class Sequence;

class SequenceManager {
 public:
  Sequence *getSequence(const char *schema, const char *name);
  void deleteSequence(const char *schema, const char *name);
  Sequence *createSequence(const char *schema, const char *name,
                           int64 initialValue);
  Sequence *findSequence(const char *schema, const char *name);
  void initialize();
  SequenceManager(Database *db);
  virtual ~SequenceManager();

 protected:
  Database *database;
  Sequence *sequences[SEQUENCE_HASH_SIZE];
  SyncObject syncObject;

 public:
  void renameSequence(Sequence *sequence, const char *newSchema,
                      const char *newName);
  Sequence *recreateSequence(Sequence *oldSequence);
};

}  // namespace Changjiang
