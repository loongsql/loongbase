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

// NUpdate.cpp: implementation of the NUpdate class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "NUpdate.h"
#include "Syntax.h"
#include "CompiledStatement.h"
#include "Connection.h"
#include "Statement.h"
#include "NField.h"
#include "Context.h"
#include "Fsb.h"
#include "Table.h"
#include "Index.h"
#include "SQLError.h"
#include "Privilege.h"

namespace Changjiang {

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

NUpdate::NUpdate(CompiledStatement *statement, Syntax *syntax)
    : NNode(statement, Update) {
  fields = NULL;
  node = NULL;
  context =
      statement->compileContext(syntax->getChild(0), PRIV_MASK(PrivUpdate));
  table = context->table;
  contextId = context->contextId;
  statement->pushContext(context);
  node = statement->compile(syntax->getChild(1));
  numberAssignments = node->count;
  fields = new Field *[numberAssignments];
  // Index *primaryKey = table->getPrimaryKey();

  for (int n = 0; n < numberAssignments; ++n) {
    NNode *assignment = node->children[n];
    NField *field = (NField *)assignment->children[0];
    fields[n] = field->field;
  }

  Syntax *boolean = syntax->getChild(2);

  if (boolean && boolean->type == nod_cursor) {
    cursorName = boolean->getChild(0)->getString();
    stream = NULL;
  } else {
    LinkedList conjuncts;

    if (boolean) {
      NNode *stuff = statement->compile(boolean);
      stuff->decomposeConjuncts(conjuncts);
    }
    stream = statement->compileStream(context, conjuncts, this);
  }

  statement->popContext();
}

NUpdate::~NUpdate() {
  if (fields) delete[] fields;

  if (stream) delete stream;
}

void NUpdate::evalStatement(Statement *statement) {
  statement->updateStatements = true;
  Value **values = new Value *[numberAssignments];

  try {
    if (!stream) {
      Statement *parent = statement->connection->findStatement(cursorName);
      if (!parent)
        throw SQLEXCEPTION(RUNTIME_ERROR, "can't find cursor %s",
                           (const char *)cursorName);
      Transaction *transaction = statement->transaction;
      NNode **assignments = node->children;

      for (int n = 0; n < numberAssignments; ++n) {
        NNode *assignment = assignments[n];
        NNode *expr = assignment->children[1];
        values[n] = expr->eval(statement);
      }

      Context *context = parent->getUpdateContext();

      if (!context || context->table != table)
        throw SQLEXCEPTION(RUNTIME_ERROR, "statement %s is not updatable",
                           (const char *)cursorName);
      table->update(transaction, context->record, numberAssignments, fields,
                    values);
      ++statement->recordsUpdated;
      ++statement->stats.updates;
      delete[] values;
      return;
    }

    stream->open(statement);
    Transaction *transaction = statement->transaction;
    Context *context = statement->getContext(contextId);
    NNode **assignments = node->children;

    while (stream->fetch(statement)) {
      for (int n = 0; n < numberAssignments; ++n) {
        NNode *assignment = assignments[n];
        NNode *expr = assignment->children[1];
        values[n] = expr->eval(statement);
      }
      table->update(transaction, context->record, numberAssignments, fields,
                    values);
      ++statement->stats.updates;
      ++statement->recordsUpdated;
    }

    stream->close(statement);
    delete[] values;

    return;
  } catch (...) {
    delete[] values;
    throw;
  }
}

bool NUpdate::references(Table *tbl) { return table == tbl; }

}  // namespace Changjiang
