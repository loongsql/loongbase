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

// NRecordNumber.cpp: implementation of the NRecordNumber class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "NRecordNumber.h"
#include "Context.h"
#include "CompiledStatement.h"
#include "Statement.h"
#include "Record.h"
#include "Value.h"
#include "SQLError.h"

namespace Changjiang {

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

NRecordNumber::NRecordNumber(CompiledStatement *statement, Context *context)
    : NNode(statement, RecordNumber) {
  contextId = context->contextId;
  valueSlot = statement->getValueSlot();
}

NRecordNumber::~NRecordNumber() {}

Value *NRecordNumber::eval(Statement *statement) {
  Value *value = statement->getValue(valueSlot);
  Context *context = statement->getContext(contextId);

  if (!context->record)
    throw SQLError(INTERNAL_ERROR, "missing record context for record_number");

  value->setValue(context->record->recordNumber);

  return value;
}

bool NRecordNumber::computable(CompiledStatement *statement) {
  return statement->contextComputable(contextId);
}

NRecordNumber::NRecordNumber(CompiledStatement *statement)
    : NNode(statement, RecordNumber) {}

const char *NRecordNumber::getName() { return "RECORD_NUMBER"; }

}  // namespace Changjiang
