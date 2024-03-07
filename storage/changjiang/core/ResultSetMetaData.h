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

// ResultSetMetaData.h: interface for the ResultSetMetaData class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Types.h"

namespace Changjiang {

static const int columnNoNulls = 0;
static const int columnNullable = 1;
static const int columnNullableUnknown = 2;

class ResultSet;
CLASS(Statement);

class ResultSetMetaData {
 public:
  const char *getRepositoryName(int index);
  const char *getColumnTypeName(int index);
  void checkResultSet();
  void resultSetClosed();
  void releaseJavaRef();
  void addJavaRef();
  virtual JdbcType getColumnType(int index);
  virtual const char *getTableName(int index);
  virtual const char *getColumnName(int index);
  virtual int getColumnCount();
  virtual const char *getSchemaName(int index);
  virtual const char *getQuery();
  virtual int getScale(int index);
  virtual int getPrecision(int index);
  virtual int getColumnDisplaySize(int column);
  virtual const char *getCatalogName(int index);
  virtual bool isSearchable(int index);
  virtual bool isNullable(int index);
  virtual const char *getCollationSequence(int index);
  virtual const char *getDefaultValue(int index);

  ResultSetMetaData(ResultSet *results);

 protected:
  virtual ~ResultSetMetaData();
  void release();
  void addRef();

  ResultSet *resultSet;
  JString sqlString;
  volatile INTERLOCK_TYPE useCount;
};

}  // namespace Changjiang
