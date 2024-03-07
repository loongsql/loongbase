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

// FilterSetManager.cpp: implementation of the FilterSetManager class.
//
//////////////////////////////////////////////////////////////////////

#define HASH(address, size) (int)(((UIPTR)address >> 2) % size)

#include <memory.h>
#include "Engine.h"
#include "FilterSetManager.h"
#include "FilterSet.h"
#include "Database.h"
#include "PreparedStatement.h"
#include "ResultSet.h"
#include "Sync.h"

namespace Changjiang {

static const char *ddl[] = {
    "create table filtersets ("
    "filtersetname varchar (128) not null,"
    "schema varchar (128) not null,"
    "text clob,"
    "primary key (filtersetname, schema))",
    "grant select on system.filtersets to public", 0};

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

FilterSetManager::FilterSetManager(Database *db) {
  database = db;
  memset(filterSets, 0, sizeof(filterSets));
}

FilterSetManager::~FilterSetManager() {
  for (int n = 0; n < FILTERSETS_HASH_SIZE; ++n)
    for (FilterSet *filterSet; (filterSet = filterSets[n]);) {
      filterSets[n] = filterSet->collision;
      filterSet->release();
    }
}

void FilterSetManager::addFilterset(FilterSet *filterSet) {
  int slot = HASH(filterSet->name, FILTERSETS_HASH_SIZE);
  filterSet->collision = filterSets[slot];
  filterSets[slot] = filterSet;
}

FilterSet *FilterSetManager::findFilterSet(const char *schema,
                                           const char *name) {
  if (!schema) return NULL;

  FilterSet *filterSet;
  int slot = HASH(name, FILTERSETS_HASH_SIZE);

  for (filterSet = filterSets[slot]; filterSet;
       filterSet = filterSet->collision)
    if (filterSet->name == name && filterSet->schema == schema)
      return filterSet;

  PreparedStatement *statement = database->prepareStatement(
      "select text from system.filtersets where filtersetname=? and schema=?");
  statement->setString(1, name);
  statement->setString(2, schema);
  ResultSet *resultSet = statement->executeQuery();

  while (resultSet->next()) {
    filterSet = new FilterSet(database, schema, name);
    filterSet->setText(resultSet->getString(1));
    addFilterset(filterSet);
  }

  resultSet->close();
  statement->close();

  return filterSet;
}

void FilterSetManager::initialize() {
  if (!database->findTable("SYSTEM", "FILTERSETS"))
    for (const char **p = ddl; *p; ++p) database->execute(*p);
}

void FilterSetManager::deleteFilterset(FilterSet *filterSet) {
  int slot = HASH(filterSet->name, FILTERSETS_HASH_SIZE);

  for (FilterSet **ptr = filterSets + slot, *node; (node = *ptr);
       ptr = &node->collision)
    if ((*ptr == filterSet)) {
      *ptr = node->collision;
      break;
    }

  PreparedStatement *statement = database->prepareStatement(
      "delete from system.filtersets where filtersetname=? and schema=?");
  statement->setString(1, filterSet->name);
  statement->setString(2, filterSet->schema);
  statement->executeUpdate();
  statement->close();

  filterSet->release();
}

}  // namespace Changjiang
