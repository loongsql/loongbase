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

// NSelectExpr.cpp: implementation of the NSelectExpr class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "NSelectExpr.h"
#include "CompiledStatement.h"
#include "Syntax.h"
#include "Value.h"
#include "Statement.h"
#include "SQLError.h"
#include "NSelect.h"
#include "ResultSet.h"
#include "PrettyPrint.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

NSelectExpr::NSelectExpr(CompiledStatement *statement, Syntax *syntax)
    : NNode(statement, SelectExpr) {
  valueSlot = statement->getValueSlot();
  select = (NSelect *)statement->compile(syntax->getChild(0));
}

NSelectExpr::~NSelectExpr() {}

Value *NSelectExpr::eval(Statement *statement) {
  Value *value = statement->getValue(valueSlot);
  select->evalStatement(statement);
  ResultSet *resultSet = statement->getResultSet();

  if (resultSet->next())
    value->setValue(resultSet->getValue(1), true);
  else
    value->clear();

  resultSet->release();

  return value;
}

void NSelectExpr::prettyPrint(int level, PrettyPrint *pp) {
  pp->indent(level++);
  pp->put("Select Expression\n");
  select->prettyPrint(level, pp);
}

}  // namespace Changjiang
