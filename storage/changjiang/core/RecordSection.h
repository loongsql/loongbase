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

// RecordSection.h: interface for the RecordSection class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

namespace Changjiang {

class Record;
class Table;
class RecordScavenge;

#define RECORD_SLOTS 100

class RecordSection {
 public:
  virtual bool retireSections(Table *table, int id) = 0;
  virtual bool inactive() = 0;
  virtual ~RecordSection();

  virtual Record *fetch(int32 id) = 0;
  virtual bool store(Record *record, Record *prior, int32 id,
                     RecordSection **parentPtr) = 0;
  virtual void pruneRecords(Table *table, int base,
                            RecordScavenge *recordScavenge) = 0;
  virtual void retireRecords(Table *table, int base,
                             RecordScavenge *recordScavenge) = 0;
  virtual int countActiveRecords() = 0;
  virtual bool anyActiveRecords() = 0;
  virtual int chartActiveRecords(int *chart) = 0;

  int32 base;
};

}  // namespace Changjiang
