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

// CollationManager.cpp: implementation of the CollationManager class.
//
//////////////////////////////////////////////////////////////////////

#include <string.h>
#include "Engine.h"
#include "CollationManager.h"
#include "CollationCaseless.h"
#include "CollationUnknown.h"
#include "Field.h"
#include "SQLError.h"
#include "Sync.h"

namespace Changjiang {

static CollationCaseless collationCaseless;
static CollationManager collationManager;

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CollationManager::CollationManager() {
  memset(hashTable, 0, sizeof(hashTable));
  add(&collationCaseless);
  syncObject.setName("CollationManager::syncObject");
}

CollationManager::~CollationManager() {}

Collation *CollationManager::getCollation(const char *name) {
  if (!name || !name[0]) return NULL;

  if (strcasecmp(name, "CASE_SENSITIVE") == 0) return NULL;

  Collation *collation = collationManager.findCollation(name);

  if (!collation)
    // addCollation(new CollationUnknown(&collationManager, name));
    return new CollationUnknown(&collationManager, name);

  return collation;
}

Collation *CollationManager::findCollation(const char *collationName) {
  return collationManager.find(collationName);
}

void CollationManager::addCollation(Collation *collation) {
  Sync sync(&collationManager.syncObject, "CollationManager::addCollation");
  sync.lock(Exclusive);
  collationManager.add(collation);
}

void CollationManager::add(Collation *collation) {
  int slot = JString::hash(collation->getName(), COLLATION_HASH_SIZE);
  collation->collision = hashTable[slot];
  hashTable[slot] = collation;
}

Collation *CollationManager::find(const char *collationName) {
  Sync sync(&syncObject, "CollationManager::find");
  sync.lock(Shared);
  int slot = JString::hash(collationName, COLLATION_HASH_SIZE);

  for (Collation *collation = hashTable[slot]; collation;
       collation = collation->collision)
    if (strcmp(collation->getName(), collationName) == 0) {
      collation->addRef();

      return collation;
    }

  return NULL;
}

void CollationManager::flush(void) {
  Sync sync(&syncObject, "CollationManager::flush");
  sync.lock(Exclusive);

  for (int n = 0; n < COLLATION_HASH_SIZE; ++n)
    for (Collation *collation; (collation = hashTable[n]);) {
      hashTable[n] = collation->collision;
      collation->release();
    }
}

}  // namespace Changjiang
