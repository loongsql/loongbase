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

// Field.h: interface for the Field class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Types.h"

namespace Changjiang {

#define NOT_NULL 1
#define SEARCHABLE 2
#define CASE_INSENSITIVE 4

class Table;
class ForeignKey;
class Transaction;
class Collation;
class Value;
class Repository;

enum JdbcType;

START_NAMESPACE
class Field {
 public:
  bool isString();
  const char *getSqlTypeName();
  void setRepository(Repository *repo);
  void setCollation(Collation *newCollation);
  void makeNotSearchable(Transaction *transaction, bool populate);
  static int getPrecision(Type type, int length);
  bool isSearchable();
  void drop();
  void save();
  JdbcType getSqlType();
  void setNotNull();
  void update();
  void makeSearchable(Transaction *transaction, bool populate);
  int getDisplaySize();
  int getPhysicalLength();
  static int boundaryRequirement(Type type);
  int boundaryRequirement();
  bool getNotNull();
  const char *getName();
  Field(Table *tbl, int fieldId, const char *fieldName, Type typ, int len,
        int precision, int scale, int flags);
  virtual ~Field();

  Table *table;
  Collation *collation;
  Repository *repository;
  const char *name;
  const char *domainName;
  ForeignKey *foreignKey;  // used only during DDL processing
  Type type;
  Field *next;  // next in table
  Value *defaultValue;
  int length;
  int id;
  int precision;
  int scale;
  int flags;
  char indexPadByte;
};
END_NAMESPACE

}  // namespace Changjiang
