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

// NConstant.cpp: implementation of the NConstant class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "NConstant.h"
#include "Stream.h"
#include "Log.h"
#include "PrettyPrint.h"

namespace Changjiang {

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

NConstant::NConstant(CompiledStatement *statement, const char *string)
    : NNode(statement, Constant) {
  value.setString(string, true);
}

NConstant::~NConstant() {}

Value *NConstant::eval(Statement *statement) { return &value; }

NConstant::NConstant(CompiledStatement *statement, int number)
    : NNode(statement, Constant) {
  value.setValue(number);
}

NConstant::NConstant(CompiledStatement *statement, Value *val)
    : NNode(statement, Constant) {
  value.setValue(val, false);
}

void NConstant::prettyPrint(int level, PrettyPrint *pp) {
  pp->indent(level++);
  char *temp;
  pp->format("\"%s\"\n", value.getString(&temp));

  if (temp) delete[] temp;
}

void NConstant::gen(Stream *stream) {
  char *temp = NULL;

  switch (value.getType()) {
    case String:
    case Char:
    case Date:
      stream->putCharacter('\'');
      stream->putSegment(value.getString(&temp));
      stream->putCharacter('\'');
      break;

    default:
      stream->putSegment(value.getString(&temp));
  }

  if (temp) delete[] temp;
}

FieldType NConstant::getType() {
  FieldType type;
  type.type = value.getType();
  type.scale = value.getScale();
  type.length = value.getStringLength();
  type.precision = type.length;

  return type;
}

NNode *NConstant::copy(CompiledStatement *statement, Context *context) {
  return new NConstant(statement, &value);
}

bool NConstant::equiv(NNode *node) {
  if (type != node->type || count != node->count) return false;

  if (value.compare(&((NConstant *)node)->value) != 0) return false;

  return true;
}

}  // namespace Changjiang
