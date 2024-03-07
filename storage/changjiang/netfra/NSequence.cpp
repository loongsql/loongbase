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

// NSequence.cpp: implementation of the NSequence class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "NSequence.h"
#include "Database.h"
#include "Value.h"
#include "Statement.h"
#include "Sequence.h"
#include "Syntax.h"
#include "SQLError.h"
#include "CompiledStatement.h"

namespace Changjiang {

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

NSequence::NSequence(CompiledStatement *statement, Syntax *syntax)
    : NNode(statement, NextValue, 0) {
  Syntax *identifier = syntax->getChild(0);
  sequence = statement->findSequence(identifier, true);

  if (!sequence)
    throw SQLEXCEPTION(
        COMPILE_ERROR, "can't find sequence \"%s\"",
        identifier->getChild(identifier->count - 1)->getString());
}

NSequence::NSequence(CompiledStatement *statement, Sequence *seq)
    : NNode(statement, NextValue) {
  sequence = seq;
}

NSequence::~NSequence() {}

NNode *NSequence::copy(CompiledStatement *statement, Context *context) {
  return new NSequence(statement, sequence);
}

Value *NSequence::eval(Statement *statement) {
  Value *value = statement->getValue(valueSlot);
  value->setValue(sequence->update(1, statement->transaction));

  return value;
}

FieldType NSequence::getType() {
  FieldType type;
  type.type = Quad;
  type.precision = 19;
  type.scale = 0;
  type.length = sizeof(int64);

  return type;
}

const char *NSequence::getName() { return sequence->name; }

}  // namespace Changjiang
