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

// NInsert.cpp: implementation of the NInsert class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <memory.h>
#include "Engine.h"
#include "NInsert.h"
#include "CompiledStatement.h"
#include "Syntax.h"
#include "Field.h"
#include "Value.h"
#include "Table.h"
#include "Statement.h"
#include "SQLError.h"
#include "NSelect.h"
#include "ResultSet.h"

namespace Changjiang {

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

NInsert::NInsert(CompiledStatement *statement, Syntax *syntax)
    : NNode(statement, Insert) {
  fields = NULL;
  select = NULL;
  numberFields = 0;
  table = statement->getTable(syntax->getChild(0));
  // Context *context =
  statement->makeContext(table, PRIV_MASK(PrivInsert));
  Syntax *node = syntax->getChild(1);
  int n = 0;

  if (node) switch (node->type) {
      case nod_list:
        allocFields(node->count);
        FOR_SYNTAX(fld, node)
        fields[n++] = statement->findField(table, fld);
        END_FOR;
        break;

      default:
        break;
    }

  Syntax *values;

  if ((values = syntax->getChild(2))) {
    if (values->count != numberFields)
      throw SQLEXCEPTION(SYNTAX_ERROR,
                         "count mismatch between field list and value list");
    NNode **child = allocChildren(values->count);
    FOR_SYNTAX(expr, values)
    *child++ = statement->compile(expr);
    END_FOR;
  }

  if ((node = syntax->getChild(3))) {
    select = (NSelect *)statement->compile(node);
    ASSERT(select->type = Select);
    if (select->numberColumns != numberFields)
      throw SQLEXCEPTION(SYNTAX_ERROR,
                         "count mismatch between field list and select list");
  } else if (!values && numberFields)
    throw SQLEXCEPTION(SYNTAX_ERROR,
                       "no valuelist has been supplied for insert statement");
}

NInsert::~NInsert() {
  if (fields) delete[] fields;
}

void NInsert::allocFields(int count) {
  if (fields) return;

  numberFields = count;
  fields = new Field *[numberFields];
  memset(fields, 0, sizeof(Field *) * count);
}

void NInsert::evalStatement(Statement *statement) {
  statement->updateStatements = true;
  Value **values = new Value *[numberFields];

  if (select) {
    try {
      select->evalStatement(statement);
      ResultSet *resultSet = statement->getResultSet();
      while (resultSet->next()) {
        for (int n = 0; n < numberFields; ++n)
          values[n] = resultSet->getValue(n + 1);
        table->insert(statement->transaction, numberFields, fields, values);
        ++statement->stats.inserts;
        ++statement->recordsUpdated;
      }

      delete[] values;
    } catch (...) {
      delete[] values;
      throw;
    }
  } else
    try {
      for (int n = 0; n < numberFields; ++n)
        values[n] = children[n]->eval(statement);

      table->insert(statement->transaction, numberFields, fields, values);
      ++statement->stats.inserts;
      ++statement->recordsUpdated;

      delete[] values;
    } catch (...) {
      delete[] values;
      throw;
    }

  return;
}

bool NInsert::references(Table *tbl) { return table == tbl; }

}  // namespace Changjiang
