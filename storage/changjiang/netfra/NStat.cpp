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

// NStat.cpp: implementation of the NStat class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "NStat.h"
#include "Syntax.h"
#include "Value.h"
#include "Statement.h"
#include "CompiledStatement.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

NStat::NStat(CompiledStatement *statement, NType typ, Syntax *syntax)
    : NNode(statement, typ, 1) {
  Syntax *expr = syntax->getChild(1);

  if (expr) children[0] = statement->compile(expr);

  if (type == Avg) countSlot = statement->getValueSlot();

  valueSlot = statement->getValueSlot();
  distinct = syntax->getChild(0) != NULL;
}

NStat::~NStat() {}

void NStat::reset(Statement *statement) {
  Value *value = statement->getValue(valueSlot);

  switch (type) {
    case Sum:
    case Count:
      value->setValue((int)0);
      break;

    case Avg:
      statement->getValue(countSlot)->setValue((int)0);
    default:
      value->clear();
  }
}

bool NStat::isStatistical() { return true; }

void NStat::increment(Statement *statement) {
  Value *value = statement->getValue(valueSlot);
  Value *incr = NULL;
  Type valueType = value->getType();
  Type incrType = Null;

  if (children[0]) {
    incr = children[0]->eval(statement);
    incrType = incr->getType();
  }

  switch (type) {
    case Count:
      if (!incr || incrType != Null)
        value->setValue((int)(value->getInt() + 1));
      break;

    case Max:
      if ((incrType != Null) && (valueType == Null || incr->compare(value) > 0))
        value->setValue(incr, true);
      break;

    case Min:
      if ((incrType != Null) && (valueType == Null || incr->compare(value) < 0))
        value->setValue(incr, true);
      break;

    case Avg: {
      Value *count = statement->getValue(countSlot);

      if (incrType == Null) break;

      count->add(1);
    }
    case Sum:
      if (incrType != Null) value->add(incr);
      break;

    default:
      break;
  }
}

Value *NStat::eval(Statement *statement) {
  Value *value = statement->getValue(valueSlot);

  if (type == Avg) {
    Value *count = statement->getValue(countSlot);
    if (count->getInt() == 0)
      value->setNull();
    else
      value->divide(count);
  }

  return value;
}

const char *NStat::getName() {
  switch (type) {
    case Count:
      return "COUNT";
    case Max:
      return "MAX";
    case Min:
      return "MIN";
    case Sum:
      return "SUM";
    case Avg:
      return "AVG";

    default:
      return "";
  }
}

}  // namespace Changjiang
