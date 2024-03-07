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

// FsbExhaustive.cpp: implementation of the FsbExhaustive class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "FsbExhaustive.h"
#include "Context.h"
#include "Table.h"
#include "Statement.h"
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

FsbExhaustive::FsbExhaustive(Context *context, Row *rowSource) {
  contextId = context->contextId;
  table = context->table;
  row = rowSource;
}

FsbExhaustive::~FsbExhaustive() {}

void FsbExhaustive::open(Statement *statement) {
  statement->getContext(contextId)->open();
}

Row *FsbExhaustive::fetch(Statement *statement) {
  if (statement->getContext(contextId)->fetchNext(statement)) return row;

  return NULL;
}

void FsbExhaustive::close(Statement *statement) {
  statement->getContext(contextId)->close();
}

void FsbExhaustive::getStreams(int **ptr) { *(*ptr)++ = contextId; }

void FsbExhaustive::prettyPrint(int level, PrettyPrint *pp) {
  pp->indent(level++);
  pp->format("Exhaustive %s.%s (%d)\n", table->getSchema(), table->getName(),
             contextId);
}

}  // namespace Changjiang
