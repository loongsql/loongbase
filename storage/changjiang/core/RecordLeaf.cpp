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

// RecordLeaf.cpp: implementation of the RecordLeaf class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <memory.h>
#include "Engine.h"
#include "RecordLeaf.h"
#include "RecordGroup.h"
#include "RecordVersion.h"
#include "Table.h"
#include "Sync.h"
#include "Interlock.h"
#include "Bitmap.h"
#include "RecordScavenge.h"

namespace Changjiang {

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

RecordLeaf::RecordLeaf() {
  base = 0;
  memset(records, 0, sizeof(records));
  syncObject.setName("RecordLeaf::syncObject");
}

RecordLeaf::~RecordLeaf() {
  for (int n = 0; n < RECORD_SLOTS; ++n)
    if (records[n]) {
#ifdef CHECK_RECORD_ACTIVITY
      for (Record *rec = records[n]; rec; rec = rec->getPriorVersion())
        rec->active = false;
#endif

      records[n]->release(REC_HISTORY);
    }
}

Record *RecordLeaf::fetch(int32 id) {
  if (id >= RECORD_SLOTS) return NULL;

  Sync sync(&syncObject, "RecordLeaf::fetch");
  sync.lock(Shared);

  Record *record = records[id];

  if (record) record->addRef(REC_HISTORY);

  return record;
}

bool RecordLeaf::store(Record *record, Record *prior, int32 id,
                       RecordSection **parentPtr) {
  // If this doesn't fit, create a new level above use, then store
  // record in new record group.

  if (id >= RECORD_SLOTS) {
    RecordGroup *group = new RecordGroup(RECORD_SLOTS, 0, this);

    if (COMPARE_EXCHANGE_POINTER(parentPtr, this, group))
      return group->store(record, prior, id, parentPtr);

    group->records[0] = NULL;
    delete group;

    return (*parentPtr)->store(record, prior, id, parentPtr);
  }

  // If we're adding a new version, don't bother with a lock.  Otherwise we need
  // to lock out simultaneous fetches to avoid a potential race between addRef()
  // and release().

  if (record && record->getPriorVersion() == prior) {
    if (!COMPARE_EXCHANGE_POINTER(records + id, prior, record)) return false;
  } else {
    Sync sync(&syncObject, "RecordLeaf::store");
    sync.lock(Exclusive);

    if (!COMPARE_EXCHANGE_POINTER(records + id, prior, record)) return false;
  }

  return true;
}

// Prune old invisible record versions from the end of record chains.
// The visible versions at the front of the list are kept.
// Also, inventory each record slot on this leaf.

void RecordLeaf::pruneRecords(Table *table, int base,
                              RecordScavenge *recordScavenge) {
  Record **ptr, **end;

  // Get a shared lock since we are just traversing the tree.
  // pruneRecords does not empty any slots in a record leaf.

  Sync sync(&syncObject, "RecordLeaf::pruneRecords(syncObject)");
  sync.lock(Shared);

  for (ptr = records, end = records + RECORD_SLOTS; ptr < end; ++ptr) {
    Record *record = *ptr;

    if (record) {
      Record *oldestVisible = recordScavenge->inventoryRecord(record);

      // Prune invisible records.

      if (oldestVisible) {
        // Detach the older records from the oldest visible.

        Record *prior = oldestVisible->clearPriorVersion();

        for (Record *prune = prior; prune; prune = prune->getPriorVersion()) {
          if (prune->useCount != 1) {
            // Give up, re-attach and do not prune this chain this time.
            oldestVisible->setPriorVersion(NULL, prior);
            prior = NULL;
            break;
          }

          recordScavenge->recordsPruned++;
          recordScavenge->spacePruned += prune->getMemUsage();
        }

        if (prior) {
          SET_RECORD_ACTIVE(prior, false);
          table->garbageCollect(prior, record, NULL, false);
          prior->queueForDelete();
        }
      }
    }
  }
}

void RecordLeaf::retireRecords(Table *table, int base,
                               RecordScavenge *recordScavenge) {
  int slotsWithRecords = 0;
  Record **ptr, **end;

  Sync sync(&syncObject, "RecordLeaf::retireRecords(syncObject)");
  sync.lock(Shared);

  // Get a shared lock to find at least one record to scavenge

  for (ptr = records, end = records + RECORD_SLOTS; ptr < end; ++ptr) {
    Record *record = *ptr;

    if (record && recordScavenge->canBeRetired(record)) break;
  }

  if (ptr >= end) return;

  // We can retire at least one record from this leaf;
  // Get an exclusive lock and retire as many as possible.

  sync.unlock();
  sync.lock(Exclusive);

  for (ptr = records; ptr < end; ++ptr) {
    Record *record = *ptr;

    if (record) {
      if ((recordScavenge->canBeRetired(record)) &&
          (COMPARE_EXCHANGE_POINTER(ptr, record, NULL))) {
        ++recordScavenge->recordsRetired;
        recordScavenge->spaceRetired += record->getMemUsage();
        record->retire();
      } else {
        slotsWithRecords++;
        ++recordScavenge->recordsRemaining;
        recordScavenge->spaceRemaining += record->getMemUsage();
      }
    }
  }

  // If this node is empty, store the base record number for use as an
  // identifier when the leaf node is scavenged later.

  if ((slotsWithRecords == 0) && (table->emptySections))
    table->emptySections->set(base);

  return;
}

bool RecordLeaf::retireSections(Table *table, int id) { return inactive(); }

bool RecordLeaf::inactive() { return (!anyActiveRecords()); }

int RecordLeaf::countActiveRecords() {
  int count = 0;

  for (Record **ptr = records, **end = records + RECORD_SLOTS; ptr < end; ++ptr)
    if (*ptr) ++count;

  return count;
}

bool RecordLeaf::anyActiveRecords() {
  for (Record **ptr = records, **end = records + RECORD_SLOTS; ptr < end; ++ptr)
    if (*ptr) return true;

  return false;
}

int RecordLeaf::chartActiveRecords(int *chart) {
  int count = countActiveRecords();
  chart[count]++;

  return count;
}

}  // namespace Changjiang
