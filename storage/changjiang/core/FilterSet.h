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

// FilterSet.h: interface for the FilterSet class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

namespace Changjiang {

#define FILTERSET_HASH_SIZE 101

class TableFilter;
class Table;
class SQLParse;
class Syntax;
class Database;

class FilterSet {
 public:
  void release();
  void addRef();
  void save();
  void setText(const char *string);
  TableFilter *findFilter(Table *table);
  void addFilter(TableFilter *filter);
  FilterSet(Database *db, const char *filterSchema, const char *filterName);
  JString stripSQL(const char *source);

 protected:
  virtual ~FilterSet();

 public:
  void clear();
  int useCount;
  Database *database;
  const char *name;
  const char *schema;
  JString sql;
  Syntax *syntax;
  SQLParse *parse;
  FilterSet *collision;
  TableFilter *filters[FILTERSET_HASH_SIZE];
};

}  // namespace Changjiang
