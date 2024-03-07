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

// CollationManager.h: interface for the CollationManager class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "SyncObject.h"

namespace Changjiang {

static const int COLLATION_HASH_SIZE = 101;

class Collation;

class CollationManager {
 public:
  CollationManager();
  virtual ~CollationManager();

  Collation *find(const char *collationName);

  static Collation *getCollation(const char *name);
  static Collation *findCollation(const char *collationName);
  static void addCollation(Collation *collation);

 protected:
  void add(Collation *collation);

 public:
  Collation *hashTable[COLLATION_HASH_SIZE];
  SyncObject syncObject;
  void flush(void);
};

}  // namespace Changjiang
