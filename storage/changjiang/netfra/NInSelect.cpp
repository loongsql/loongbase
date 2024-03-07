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

// NInSelect.cpp: implementation of the NInSelect class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "NInSelect.h"
#include "CompiledStatement.h"
#include "Syntax.h"
#include "Value.h"
#include "Statement.h"
#include "SQLError.h"
#include "NSelect.h"
#include "ResultSet.h"
#include "NInSelectBitmap.h"
#include "Index.h"
#include "Context.h"
#include "Table.h"
#include "PrettyPrint.h"

namespace Changjiang {

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

NInSelect::NInSelect(CompiledStatement *statement, Syntax *syntax)
    : NNode(statement, InSelect) {
  expr = statement->compile(syntax->getChild(0));
  int parameters = statement->numberParameters;
  inversion = (NSelect *)statement->compileSelect(syntax->getChild(1), NULL);
  statement->numberParameters = parameters;
  select = (NSelect *)statement->compileSelect(syntax->getChild(1), expr);
  bitmap = nullptr;
}

NInSelect::~NInSelect() {}

int NInSelect::evalBoolean(Statement *statement) {
  Value *v1 = expr->eval(statement);

  if (bitmap) return bitmap->hasValue(statement, v1);

  select->evalStatement(statement);
  ResultSet *resultSet = statement->getResultSet();

  while (resultSet->next()) {
    Value *v2 = resultSet->getValue(1);
    if (v1->compare(v2) == 0) return TRUE_BOOLEAN;
  }

  resultSet->close();

  return FALSE_BOOLEAN;
}

bool NInSelect::computable(CompiledStatement *statement) {
  if (!expr->computable(statement) || !select->computable(statement))
    return false;

  return true;
}

NNode *NInSelect::makeInversion(CompiledStatement *statement, Context *context,
                                Index *index) {
  if (expr->matchField(context, index) != 0) return NULL;

  bool was = context->setComputable(false);
  bool computable = select->computable(statement);
  context->setComputable(was);

  if (!computable) return NULL;

  return bitmap = new NInSelectBitmap(this, index);
}

bool NInSelect::isInvertible(CompiledStatement *statement, Context *context) {
  Table *table = context->table;

  FOR_INDEXES(index, table)
  if (index->numberFields != 1) continue;

  if (expr->matchField(context, index) < 0) continue;

  bool was = context->setComputable(false);
  bool computable = select->computable(statement);
  context->setComputable(was);

  if (computable) return true;
  END_FOR;

  return false;
}

void NInSelect::prettyPrint(int level, PrettyPrint *pp) {
  pp->indent(level++);
  pp->put("In Select\n");
  expr->prettyPrint(level, pp);
  select->prettyPrint(level, pp);
}

}  // namespace Changjiang
