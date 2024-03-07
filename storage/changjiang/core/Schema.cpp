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

// Schema.cpp: implementation of the Schema class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "Schema.h"
#include "Database.h"
#include "PreparedStatement.h"
#include "ResultSet.h"
#include "Sync.h"
#include "SQLError.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Schema::Schema(Database *db, const char *schemaName) {
  name = schemaName;
  database = db;
  sequenceInterval = 0;
  systemId = 0;
  refresh();
}

Schema::~Schema() {}

void Schema::update() {
  PreparedStatement *statement = database->prepareStatement(
      "replace system.schemas (schema,sequence_interval,system_id) values "
      "(?,?,?)");
  int n = 1;
  statement->setString(n++, name);
  statement->setInt(n++, sequenceInterval);
  statement->setInt(n++, systemId);
  statement->executeUpdate();

  database->commitSystemTransaction();
}

void Schema::setInterval(int newInterval) {
  if (newInterval <= sequenceInterval) return;

  sequenceInterval = newInterval;
  update();
}

void Schema::setSystemId(int newId) {
  if (newId >= sequenceInterval)
    throw SQLError(
        DDL_ERROR,
        "schema system id cannot equal or exceed sequence interval\n");

  systemId = newId;
  update();
}

void Schema::refresh() {
  PreparedStatement *statement = database->prepareStatement(
      "select sequence_interval, system_id from system.schemas where schema=?");
  statement->setString(1, name);
  ResultSet *resultSet = statement->executeQuery();

  while (resultSet->next()) {
    sequenceInterval = resultSet->getInt(1);
    systemId = resultSet->getInt(2);
  }

  resultSet->close();
  statement->close();
}

}  // namespace Changjiang
