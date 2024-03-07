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

#include "Engine.h"
#include "CycleLock.h"
#include "Database.h"
#include "CycleManager.h"
#include "Thread.h"

namespace Changjiang {

CycleLock::CycleLock(Database *database) {
  cycleManager = database->cycleManager;
  syncObject = cycleManager->getSyncObject();
  thread = Thread::getThread("CycleLock::CycleLock");

  // If there already is a cycle manager, let him worry about all this

  if ((chain = thread->cycleLock))
    locked = false;
  else {
    syncObject->lock(NULL, Shared);
    locked = true;
    thread->cycleLock = this;
  }
}

CycleLock::~CycleLock(void) {
  if (locked) syncObject->unlock(NULL, Shared);

  if (!chain) thread->cycleLock = NULL;
}

// Called by somebody down stack about to do a long term wait

CycleLock *CycleLock::unlock(void) {
  Thread *thread = Thread::getThread("CycleLock::CycleLock");
  CycleLock *cycleLock = thread->cycleLock;
  ASSERT(cycleLock);
  cycleLock->unlockCycle();

  return cycleLock;
}

void CycleLock::unlockCycle(void) {
  if (locked) {
    syncObject->unlock(NULL, Shared);
    locked = false;
  }

  if (chain) chain->unlockCycle();
}

void CycleLock::lockCycle(void) {
  if (chain)
    chain->lockCycle();
  else {
    syncObject = cycleManager->getSyncObject();
    syncObject->lock(NULL, Shared);
    locked = true;
  }
}

bool CycleLock::isLocked(void) {
  Thread *thread = Thread::getThread("CycleLock::CycleLock");

  if (!thread->cycleLock) return false;

  return thread->cycleLock->locked;
}

}  // namespace Changjiang
