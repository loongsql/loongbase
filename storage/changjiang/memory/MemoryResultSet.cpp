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

// MemoryResultSet.cpp: implementation of the MemoryResultSet class.
//
//////////////////////////////////////////////////////////////////////

#include <string.h>
#include "Engine.h"
#include "MemoryResultSet.h"
#include "MemoryResultSetColumn.h"
#include "MemoryResultSetMetaData.h"
#include "Value.h"
#include "SQLError.h"

namespace Changjiang {

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

MemoryResultSet::MemoryResultSet() {
  row = NULL;
  numberRows = 0;
  currentRow = 0;
}

MemoryResultSet::~MemoryResultSet() {
  FOR_OBJECTS(Values *, values, &rows)
  delete values;
  END_FOR;

  FOR_OBJECTS(MemoryResultSetColumn *, object, &columns)
  delete object;
  END_FOR;
}

void MemoryResultSet::addColumn(const char *name, const char *label, int type,
                                int displaySize, int precision, int scale) {
  MemoryResultSetColumn *column = new MemoryResultSetColumn(
      name, label, type, displaySize, precision, scale);
  columns.append(column);
  ++numberColumns;
}

void MemoryResultSet::addRow() {
  row = new Values;
  row->alloc(numberColumns);
  rows.append(row);
  ++numberRows;
}

void MemoryResultSet::setString(int index, const char *string) {
  row->values[index - 1].setString(string, true);
}

void MemoryResultSet::setInt(int index, int value) {
  row->values[index - 1].setValue(value);
}

void MemoryResultSet::setDouble(int index, double value) {
  row->values[index - 1].setValue(value);
}

bool MemoryResultSet::next() {
  if (currentRow >= numberRows) {
    active = false;
    return false;
  }

  if (conversions)
    for (int n = 0; n < numberColumns; ++n)
      if (conversions[n]) {
        delete conversions[n];
        conversions[n] = NULL;
      }

  row = (Values *)rows.getElement(currentRow++);
  active = true;

  return true;
}

const char *MemoryResultSet::getString(int id) {
  Value *value = getValue(id);

  if (!conversions) allocConversions();

  if (conversions[id - 1]) return conversions[id - 1];

  return value->getString(conversions + id - 1);
}

int MemoryResultSet::findColumnIndex(const char *name) {
  return getColumnIndex(name);
}

int MemoryResultSet::findColumn(const WCString *name) {
  return getColumnIndex(name);
}

int MemoryResultSet::getColumnIndex(const char *name) {
  int n = 0;

  FOR_OBJECTS(MemoryResultSetColumn *, column, &columns)
  if (column->name.equalsNoCase(name)) return n;
  ++n;
  END_FOR;

  return -1;
}

int MemoryResultSet::getColumnIndex(const WCString *name) {
  int n = 0;

  FOR_OBJECTS(MemoryResultSetColumn *, column, &columns)
  if (column->name.equalsNoCase(name)) return n;
  ++n;
  END_FOR;

  return -1;
}

Value *MemoryResultSet::getValue(int index) {
  if (!active)
    throw SQLEXCEPTION(RUNTIME_ERROR, "no active row in result set ");

  if (index < 1 || index > numberColumns)
    throw SQLEXCEPTION(RUNTIME_ERROR, "invalid column index for result set");

  Value *value = &row->values[index - 1];
  valueWasNull = value->isNull();

  return value;
}

ResultSetMetaData *MemoryResultSet::getMetaData() {
  if (!metaData) metaData = new MemoryResultSetMetaData(this);

  return metaData;
}

MemoryResultSetColumn *MemoryResultSet::getColumn(int index) {
  void *column = columns.getElement(index - 1);

  if (!column)
    throw SQLEXCEPTION(RUNTIME_ERROR, "invalid column index for result set");

  return (MemoryResultSetColumn *)column;
}

}  // namespace Changjiang
