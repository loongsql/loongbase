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

// MetaDataResultSet.cpp: implementation of the MetaDataResultSet class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "MetaDataResultSet.h"
#include "SQLError.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

MetaDataResultSet::MetaDataResultSet(ResultSet *source)
    : ResultSet(source->numberColumns) {
  resultSet = source;
  resultSet->addJavaRef();
  resultSet->release();
  select = resultSet->select;
}

MetaDataResultSet::~MetaDataResultSet() {
  if (resultSet) resultSet->releaseJavaRef();
}

Value *MetaDataResultSet::getValue(const char *name) {
  if (!active)
    throw SQLEXCEPTION(RUNTIME_ERROR, "no active row in result set ");

  int index = resultSet->getColumnIndex(name);

  return getValue(index + 1);
}

Value *MetaDataResultSet::getValue(int index) {
  return resultSet->getValue(index);
}

bool MetaDataResultSet::next() {
  deleteBlobs();
  values.clear();
  valueWasNull = -1;

  for (int n = 0; n < numberColumns; ++n)
    if (conversions[n]) {
      delete conversions[n];
      conversions[n] = NULL;
    }

  return resultSet->next();
}

bool MetaDataResultSet::wasNull() { return resultSet->wasNull(); }

void MetaDataResultSet::close() {
  if (resultSet) {
    resultSet->releaseJavaRef();
    resultSet = NULL;
  }

  ResultSet::close();
}

Statement *MetaDataResultSet::getStatement() {
  if (!resultSet)
    throw SQLError(RUNTIME_ERROR,
                   "No statement is currently associated with ResultSet");

  return resultSet->getStatement();
}

}  // namespace Changjiang
