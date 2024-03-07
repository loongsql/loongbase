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

// RecordGroup.cpp: implementation of the RecordGroup class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include "Engine.h"
#include "RecordGroup.h"
#include "RecordLeaf.h"
#include "Record.h"
#include "Interlock.h"
#include "Table.h"
#include "Sync.h"

namespace Changjiang {

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

RecordGroup::RecordGroup(int32 recordBase) {
  base = recordBase;
  memset(records, 0, sizeof(records));
}

RecordGroup::~RecordGroup() {
  for (int n = 0; n < RECORD_SLOTS; ++n) delete records[n];
}

RecordGroup::RecordGroup(int32 recordBase, int32 id, RecordSection *section) {
  base = recordBase;
  memset(records, 0, sizeof(records));
  records[0] = section;
}

Record *RecordGroup::fetch(int32 id) {
  int slot = id / base;

  if (slot >= RECORD_SLOTS) return NULL;

  RecordSection *section = records[slot];

  if (!section) return NULL;

  return section->fetch(id % base);
}

bool RecordGroup::store(Record *record, Record *prior, int32 id,
                        RecordSection **parentPtr) {
  int slot = id / base;

  // If record doesn't fit in this group, create another level,
  // then store record in new group.

  if (slot >= RECORD_SLOTS) {
    RecordGroup *section = new RecordGroup(base * RECORD_SLOTS, id, this);

    if (COMPARE_EXCHANGE_POINTER(parentPtr, this, section))
      return section->store(record, prior, id, parentPtr);

    section->records[0] = NULL;
    delete section;

    return (*parentPtr)->store(record, prior, id, parentPtr);
  }

  RecordSection **ptr = records + slot;
  RecordSection *section = *ptr;

  if (!section) {
    if (base == RECORD_SLOTS)
      section = new RecordLeaf();
    else
      section = new RecordGroup(base / RECORD_SLOTS);

    if (!COMPARE_EXCHANGE_POINTER(ptr, NULL, section)) {
      delete section;
      section = *ptr;
    }
  }

  return section->store(record, prior, id % base, NULL);
}

void RecordGroup::pruneRecords(Table *table, int base,
                               RecordScavenge *recordScavenge) {
  int recordNumber = base * RECORD_SLOTS;

  for (RecordSection **ptr = records, **end = records + RECORD_SLOTS; ptr < end;
       ++ptr, ++recordNumber) {
    RecordSection *section = *ptr;

    if (section) section->pruneRecords(table, recordNumber, recordScavenge);
  }
}

void RecordGroup::retireRecords(Table *table, int base,
                                RecordScavenge *recordScavenge) {
  int recordNumber = base * RECORD_SLOTS;

  for (RecordSection **ptr = records, **end = records + RECORD_SLOTS; ptr < end;
       ++ptr, ++recordNumber) {
    RecordSection *section = *ptr;

    if (section) section->retireRecords(table, recordNumber, recordScavenge);
  }
}

int RecordGroup::countActiveRecords() {
  int count = 0;

  for (RecordSection **ptr = records, **end = records + RECORD_SLOTS; ptr < end;
       ++ptr) {
    RecordSection *section = *ptr;

    if (section) count += section->countActiveRecords();
  }

  return count;
}

bool RecordGroup::anyActiveRecords() {
  for (RecordSection **ptr = records, **end = records + RECORD_SLOTS; ptr < end;
       ++ptr) {
    RecordSection *section = *ptr;

    if (section)
      if (section->anyActiveRecords()) return true;
  }

  return false;
}

int RecordGroup::chartActiveRecords(int *chart) {
  int count = 0;

  for (RecordSection **ptr = records, **end = records + RECORD_SLOTS; ptr < end;
       ++ptr) {
    RecordSection *section = *ptr;

    if (section) count += section->chartActiveRecords(chart);
  }

  return count;
}

bool RecordGroup::inactive() {
  for (int slot = 0; slot < RECORD_SLOTS; slot++)
    if (records[slot]) return false;

  return true;
}

bool RecordGroup::retireSections(Table *table, int id) {
  int slot = id / base;

  if (slot < RECORD_SLOTS) {
    RecordSection *section = records[slot];

    if (section) {
      int nextId = id % base;

      // Delete inactive child sections

      if (section->retireSections(table, nextId)) {
        delete records[slot];
        records[slot] = NULL;
      }
    }
  }

  return inactive();
}

}  // namespace Changjiang
