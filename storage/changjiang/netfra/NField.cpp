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

// NField.cpp: implementation of the NField class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "NField.h"
#include "Context.h"
#include "CompiledStatement.h"
#include "Index.h"
#include "Record.h"
#include "Statement.h"
#include "Value.h"
#include "Field.h"
#include "Table.h"
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

NField::NField(CompiledStatement *statement, Field *fld, Context *context)
    : NNode(statement, nField) {
  field = fld;
  contextId = context->contextId;
  valueSlot = statement->getValueSlot();
}

NField::NField(CompiledStatement *statement) : NNode(statement, nField) {}

NField::~NField() {}

bool NField::computable(CompiledStatement *statement) {
  return statement->contextComputable(contextId);
}

int NField::matchField(Context *context, Index *index) {
  if (context->contextId == contextId) return index->matchField(field);

  return -1;
}

Value *NField::eval(Statement *statement) {
  Value *value = statement->getValue(valueSlot);
  Context *context = statement->getContext(contextId);

  if (context->record)
    context->record->getValue(field->id, value);
  else
    value->setNull();

  return value;
}

Field *NField::getField() { return field; }

bool NField::isMember(Table *table) { return field->table == table; }

const char *NField::getName() { return field->getName(); }

void NField::prettyPrint(int level, PrettyPrint *pp) {
  pp->indent(level++);
  pp->format("Field %s.%s.%s (%d)\n", field->table->getSchema(),
             field->table->getName(), field->getName(), contextId);
}

void NField::gen(Stream *stream) {
  stream->format("ctx%d.", contextId);
  stream->putSegment(field->name);
}

FieldType NField::getType() {
  FieldType type;
  type.type = field->type;
  type.scale = field->scale;
  type.length = field->length;
  type.precision = field->precision;

  return type;
}

NNode *NField::copy(CompiledStatement *statement, Context *context) {
  ASSERT(context->viewContexts);
  Context *ctx = context->viewContexts[contextId];

  return new NField(statement, field, ctx);
}

bool NField::equiv(NNode *node) {
  if (type != node->type || count != node->count) return false;

  if (field != ((NField *)node)->field ||
      contextId != ((NField *)node)->contextId)
    return false;

  return true;
}

Collation *NField::getCollation() { return field->collation; }

}  // namespace Changjiang
