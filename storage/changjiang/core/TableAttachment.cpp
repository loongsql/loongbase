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

// TableAttachment.cpp: implementation of the TableAttachment class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "TableAttachment.h"
#include "RecordVersion.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

TableAttachment::TableAttachment(int32 flags) { mask = flags; }

TableAttachment::~TableAttachment() {}

void TableAttachment::postCommit(Table *table, RecordVersion *record) {
  if (!record->getPriorVersion())
    insertCommit(table, record);
  else if (!record->hasRecord())
    deleteCommit(table, record->getPriorVersion());
  else
    updateCommit(table, record);
}

void TableAttachment::insertCommit(Table *table, RecordVersion *record) {}

void TableAttachment::updateCommit(Table *table, RecordVersion *record) {}

void TableAttachment::deleteCommit(Table *table, Record *record) {}

void TableAttachment::preInsert(Table *table, RecordVersion *record) {}

void TableAttachment::preUpdate(Table *table, RecordVersion *record) {}

void TableAttachment::tableDeleted(Table *table) {}

void TableAttachment::preDelete(Table *table, RecordVersion *record) {}

}  // namespace Changjiang
