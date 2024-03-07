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

// SequenceResultSet.cpp: implementation of the SequenceResultSet class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "SequenceResultSet.h"
#include "Value.h"
#include "Database.h"
#include "Connection.h"
#include "Statement.h"
#include "SequenceManager.h"
#include "Sequence.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SequenceResultSet::SequenceResultSet(ResultSet *source)
    : MetaDataResultSet(source) {
  sequenceManager = resultSet->database->sequenceManager;
}

SequenceResultSet::~SequenceResultSet() {}

bool SequenceResultSet::next() {
  if (!MetaDataResultSet::next()) return false;

  // int id = resultSet->getInt (4);
  const char *schema = resultSet->getSymbol(2);
  const char *sequenceName = resultSet->getSymbol(3);
  Sequence *sequence = sequenceManager->getSequence(schema, sequenceName);
  Transaction *transaction = resultSet->statement->transaction;
  int64 val = sequence->updatePhysical(0, transaction);
  Value value;
  value.setValue(val);
  resultSet->setValue(3, &value);

  return true;
}

}  // namespace Changjiang
