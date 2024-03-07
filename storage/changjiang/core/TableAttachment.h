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

// TableAttachment.h: interface for the TableAttachment class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

namespace Changjiang {

#define PRE_INSERT 1
#define POST_INSERT 2
#define PRE_UPDATE 4
#define POST_UPDATE 8
#define PRE_DELETE 0x10
#define POST_DELETE 0x20
#define POST_COMMIT 0x40

class Record;
class RecordVersion;
class Table;

class TableAttachment {
 public:
  virtual void preDelete(Table *table, RecordVersion *record);
  virtual void tableDeleted(Table *table);
  virtual void preUpdate(Table *table, RecordVersion *record);
  virtual void preInsert(Table *table, RecordVersion *record);
  virtual void deleteCommit(Table *table, Record *record);
  virtual void updateCommit(Table *table, RecordVersion *record);
  virtual void insertCommit(Table *table, RecordVersion *record);
  virtual void postCommit(Table *table, RecordVersion *record);
  TableAttachment(int32 flags);
  virtual ~TableAttachment();

  int32 mask;
};

}  // namespace Changjiang
