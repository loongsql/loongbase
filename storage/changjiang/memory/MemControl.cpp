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
#include "MemControl.h"
#include "MemMgr.h"

namespace Changjiang {

MemControl::MemControl(void) {
  count = 0;
  maxMemory = 0;
}

MemControl::~MemControl(void) {}

// Verify that the requested memory will not exceed the memory limit.
//
// If a limit has not been set for the group of memory pools, compare
// against the total of individual pool limits.
//
// Note that allocation may occur during static initialization,
// before the MemControl has been fully initialized and the memory
// limits set.

bool MemControl::poolExtensionCheck(uint size) {
  uint64 inUse = size;

  // If non-zero, MemControl::maxMemory supercedes the
  // total of the individual pool limits.

  if (maxMemory) {
    for (int n = 0; n < count; ++n) inUse += pools[n]->currentMemory;

    return inUse < maxMemory;
  } else {
    // If no group maximum is set, total the individual pool
    // limits. Only consider pools for which a limit has been set.

    uint64 poolMax = 0;
    for (int n = 0; n < count; ++n) {
      if (pools[n]->maxMemory) {
        inUse += pools[n]->currentMemory;
        poolMax += pools[n]->maxMemory;
      }
    }

    return poolMax ? (inUse < poolMax) : true;
  }
}

void MemControl::addPool(MemMgr *pool) {
  pools[count++] = pool;
  pool->memControl = this;
}

// Set the memory limit for the group of pools

void MemControl::setMaxSize(uint64 size) { maxMemory = size; }

// Set the memory limit for a specific pool. Note that the
// size is only set for the first pool matching mgrId.

void MemControl::setMaxSize(int mgrId, uint64 size) {
  uint64 groupTotal = 0;

  for (int n = 0; n < count; ++n) groupTotal += pools[n]->maxMemory;

  // Set the memory limit after checking for overflow

  for (int n = 0; n < count; ++n) {
    if (pools[n]->id == mgrId) {
      uint64 remaining = MaxTotalMemory - groupTotal;
      pools[n]->maxMemory = (size < remaining ? size : remaining);
      break;
    }
  }
}

// Total memory in use for the pools in this group,
// specified by poolMask. Default is all pools.

uint64 MemControl::getCurrentMemory(int poolMask) {
  uint64 inUse = 0;

  for (int n = 0; n < count; ++n)
    if (pools[n]->id & poolMask) inUse += pools[n]->currentMemory;

  return inUse;
}

}  // namespace Changjiang
