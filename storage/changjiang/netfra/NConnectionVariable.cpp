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

// NConnectionVariable.cpp: implementation of the NConnectionVariable class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "NConnectionVariable.h"
#include "CompiledStatement.h"
#include "Statement.h"
#include "Value.h"
#include "Connection.h"
#include "PrettyPrint.h"

namespace Changjiang {

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

NConnectionVariable::NConnectionVariable(CompiledStatement *statement,
                                         const char *variable)
    : NNode(statement, ConnectionVariable) {
  valueSlot = statement->getValueSlot();
  name = variable;
}

NConnectionVariable::~NConnectionVariable() {}

Value *NConnectionVariable::eval(Statement *statement) {
  Value *value = statement->getValue(valueSlot);
  const char *p = statement->connection->getAttribute(name, NULL);

  if (p)
    value->setString(p, true);
  else
    value->clear();

  return value;
}

bool NConnectionVariable::computable(CompiledStatement *statement) {
  return true;
}

NNode *NConnectionVariable::copy(CompiledStatement *statement,
                                 Context *context) {
  return new NConnectionVariable(statement, name);
}

void NConnectionVariable::prettyPrint(int level, PrettyPrint *pp) {
  pp->indent(level++);
  pp->format("Connection Variable %s\n", name);
}

}  // namespace Changjiang
