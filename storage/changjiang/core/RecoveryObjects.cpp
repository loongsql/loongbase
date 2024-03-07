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

// RecoveryObjects.cpp: implementation of the RecoveryObjects class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include <stdio.h>
#include "Engine.h"
#include "RecoveryObjects.h"
#include "RecoveryPage.h"
#include "StreamLog.h"
#include "SyncObject.h"
#include "Sync.h"

namespace Changjiang {

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

RecoveryObjects::RecoveryObjects(StreamLog *log) {
  streamLog = log;
  memset(recoveryObjects, 0, sizeof(recoveryObjects));
  for (int n = 0; n < RPG_HASH_SIZE; n++)
    syncArray[n].setName("RecoveryObjects::syncArray");
}

RecoveryObjects::~RecoveryObjects() { clear(); }

void RecoveryObjects::clear() {
  for (int n = 0; n < RPG_HASH_SIZE; ++n)
    for (RecoveryPage *object; (object = recoveryObjects[n]);) {
      recoveryObjects[n] = object->collision;
      delete object;
    }
}

bool RecoveryObjects::bumpIncarnation(int objectNumber, int tableSpaceId,
                                      int state, bool pass1) {
  RecoveryPage *object = getRecoveryObject(objectNumber, tableSpaceId);

  if (object->state != state) {
    if (pass1)
      ++object->pass1Count;
    else
      ++object->currentCount;

    object->state = state;
  }

  if (pass1) return object->pass1Count == 1;

  return object->pass1Count == object->currentCount;
}

void RecoveryObjects::reset() {
  for (int n = 0; n < RPG_HASH_SIZE; ++n)
    for (RecoveryPage *object = recoveryObjects[n]; object;
         object = object->collision) {
      object->currentCount = 0;
      object->state = 0;
    }
}

bool RecoveryObjects::isObjectActive(int objectNumber, int tableSpaceId) {
  RecoveryPage *object = findRecoveryObject(objectNumber, tableSpaceId);

  if (!object) return true;

  return object->pass1Count == object->currentCount;
}

RecoveryPage *RecoveryObjects::findInHashBucket(RecoveryPage *head,
                                                int objectNumber,
                                                int tableSpaceId) {
  for (RecoveryPage *object = head; object; object = object->collision)
    if (object->objectNumber == objectNumber &&
        object->tableSpaceId == tableSpaceId)
      return object;

  return NULL;
}

RecoveryPage *RecoveryObjects::findRecoveryObject(int objectNumber,
                                                  int tableSpaceId) {
  int slot = objectNumber % RPG_HASH_SIZE;
  Sync sync(&syncArray[slot], "RecoveryObjects::findRecoveryObject");
  sync.lock(Shared);
  return findInHashBucket(recoveryObjects[slot], objectNumber, tableSpaceId);
}

void RecoveryObjects::setActive(int objectNumber, int tableSpaceId) {
  RecoveryPage *object = findRecoveryObject(objectNumber, tableSpaceId);

  if (!object) return;

  object->pass1Count = 0;
  object->currentCount = 0;
}

void RecoveryObjects::setInactive(int objectNumber, int tableSpaceId) {
  RecoveryPage *object = getRecoveryObject(objectNumber, tableSpaceId);
  object->pass1Count = 0;
  object->currentCount = 1;
}

RecoveryPage *RecoveryObjects::getRecoveryObject(int objectNumber,
                                                 int tableSpaceId) {
  int slot = objectNumber % RPG_HASH_SIZE;
  RecoveryPage *object;

  Sync sync(&syncArray[slot], "RecoveryObjects::getRecoveryObject");
  sync.lock(Shared);
  object = findInHashBucket(recoveryObjects[slot], objectNumber, tableSpaceId);

  if (object) return object;

  // Object not found, insert (need exlusive lock for this)
  sync.unlock();
  sync.lock(Exclusive);

  // We need to traverse the collision list once again. Another thread
  // may have inserted the entry while current thread was waiting
  // for exclusive lock.
  object = findInHashBucket(recoveryObjects[slot], objectNumber, tableSpaceId);
  if (object) return object;

  // Add object to the start of the collision list
  object = new RecoveryPage(objectNumber, tableSpaceId);
  object->collision = recoveryObjects[slot];
  recoveryObjects[slot] = object;

  return object;
}

void RecoveryObjects::deleteObject(int objectNumber, int tableSpaceId) {
  int slot = objectNumber % RPG_HASH_SIZE;
  Sync sync(&syncArray[slot], "RecoveryObjects::deleteObject");
  sync.lock(Exclusive);

  for (RecoveryPage **ptr = recoveryObjects + slot, *object; (object = *ptr);
       ptr = &object->collision)
    if (object->objectNumber == objectNumber &&
        object->tableSpaceId == tableSpaceId) {
      *ptr = object->collision;
      delete object;

      return;
    }
}

int RecoveryObjects::getCurrentState(int objectNumber, int tableSpaceId) {
  RecoveryPage *object = findRecoveryObject(objectNumber, tableSpaceId);

  return (object) ? object->state : objUnknown;
}

}  // namespace Changjiang
