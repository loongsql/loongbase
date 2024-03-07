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

// MemoryResultSetColumn.cpp: implementation of the MemoryResultSetColumn class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "MemoryResultSetColumn.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

MemoryResultSetColumn::MemoryResultSetColumn(
    const char *columnName, const char *columnLabel, int columnType,
    int columnDisplaySize, int columnPrecision, int columnScale) {
  name = columnName;
  label = columnLabel;
  displaySize = columnDisplaySize;
  type = columnType;
  precision = columnPrecision;
  scale = columnScale;
}

MemoryResultSetColumn::~MemoryResultSetColumn() {}

}  // namespace Changjiang
