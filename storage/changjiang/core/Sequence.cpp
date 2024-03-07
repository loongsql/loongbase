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

// Sequence.cpp: implementation of the Sequence class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "Sequence.h"
#include "Database.h"
#include "Schema.h"
#include "SequenceManager.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Sequence::Sequence(Database *db, const char *sequenceSchema,
                   const char *sequenceName, int sequenceId) {
  schemaName = sequenceSchema;
  name = sequenceName;
  id = sequenceId;
  database = db;
  schema = database->getSchema(schemaName);
}

Sequence::~Sequence() {}

int64 Sequence::update(int64 delta, Transaction *transaction) {
  int64 value = database->updateSequence(id, delta, transaction);

  if (schema->sequenceInterval)
    value = value * schema->sequenceInterval + schema->systemId;

  return value;
}

int64 Sequence::updatePhysical(int64 delta, Transaction *transaction) {
  return database->updateSequence(id, delta, transaction);
}

void Sequence::rename(const char *newSchema, const char *newName) {
  database->sequenceManager->renameSequence(this, newSchema, newName);
}

Sequence *Sequence::recreate(void) {
  return database->sequenceManager->recreateSequence(this);
}

}  // namespace Changjiang
