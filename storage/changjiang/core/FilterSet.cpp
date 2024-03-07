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

// FilterSet.cpp: implementation of the FilterSet class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include "Engine.h"
#include "FilterSet.h"
#include "TableFilter.h"
#include "Table.h"
#include "SQLParse.h"
#include "Syntax.h"
#include "PreparedStatement.h"
#include "Database.h"
#include "SQLError.h"
#include "Sync.h"

namespace Changjiang {

#define HASH(address, size) (int)(((UIPTR)address >> 2) % size)

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

FilterSet::FilterSet(Database *db, const char *filterSchema,
                     const char *filterName) {
  database = db;
  name = filterName;
  schema = filterSchema;
  memset(filters, 0, sizeof(filters));
  parse = NULL;
  syntax = NULL;
  useCount = 1;
}

FilterSet::~FilterSet() { clear(); }

void FilterSet::addFilter(TableFilter *filter) {
  int slot = HASH(filter->tableName, FILTERSET_HASH_SIZE);
  filter->collision = filters[slot];
  filters[slot] = filter;
}

TableFilter *FilterSet::findFilter(Table *table) {
  for (TableFilter *filter = filters[HASH(table->name, FILTERSET_HASH_SIZE)];
       filter; filter = filter->collision)
    if (filter->tableName == table->name &&
        filter->schemaName == table->schemaName)
      return filter;

  return NULL;
}

void FilterSet::setText(const char *string) {
  clear();
  sql = stripSQL(string);
  parse = new SQLParse;
  JString cmd = "create filterset xyzzy";
  syntax = parse->parse(cmd + sql, database->symbolManager);
  Syntax *list = syntax->getChild(1);

  FOR_SYNTAX(child, list)
  Syntax *identifier = child->getChild(0);
  const char *alias = NULL;
  Syntax *aliasClause = child->getChild(2);
  if (aliasClause) alias = aliasClause->getString();
  if (identifier->count != 1)
    throw SQLEXCEPTION(DDL_ERROR,
                       "qualified statements not supported in filtersets");
  const char *tableName = identifier->getChild(0)->getString();
  TableFilter *filter =
      new TableFilter(tableName, schema, alias, child->getChild(1));
  addFilter(filter);
  END_FOR;
}

void FilterSet::save() {
  PreparedStatement *statement = database->prepareStatement(
      "replace system.filtersets (filtersetname,schema,text) values (?,?,?)");
  statement->setString(1, name);
  statement->setString(2, schema);
  statement->setString(3, sql);
  statement->executeUpdate();
  statement->close();

  database->commitSystemTransaction();
}

void FilterSet::addRef() { ++useCount; }

void FilterSet::release() {
  if (--useCount <= 0) delete this;
}

void FilterSet::clear() {
  if (parse) {
    delete parse;
    parse = NULL;
  }

  for (int n = 0; n < FILTERSET_HASH_SIZE; ++n)
    for (TableFilter *filter; (filter = filters[n]);) {
      filters[n] = filter->collision;
      filter->release();
    }
}

JString FilterSet::stripSQL(const char *source) {
  const char *sql = source;

  while (*sql && *sql != '(') ++sql;

  const char *end = sql;

  while (*end) ++end;

  while (end > sql && end[-1] != ')') --end;

  return JString(sql, (int)(end - sql));
}

}  // namespace Changjiang
