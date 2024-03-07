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

// NRepair.cpp: implementation of the NRepair class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "NRepair.h"
#include "Syntax.h"
#include "CompiledStatement.h"
#include "Connection.h"
#include "Statement.h"
#include "Context.h"
#include "Fsb.h"
#include "Table.h"
#include "SQLError.h"
#include "Privilege.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

NRepair::NRepair(CompiledStatement *statement, Syntax *syntax)
    : NDelete(statement, syntax, Repair) {}

NRepair::~NRepair() {}

void NRepair::evalStatement(Statement *statement) {
  statement->updateStatements = true;
  stream->open(statement);
  // Transaction *transaction = statement->transaction;
  Context *context = statement->getContext(contextId);

  for (;;) {
    try {
      if (!stream->fetch(statement)) break;
    } catch (SQLException &) {
      table->deleteRecord(context->recordNumber);
      ++statement->recordsUpdated;
      ++context->recordNumber;
    }
  }

  stream->close(statement);

  return;
}

}  // namespace Changjiang
