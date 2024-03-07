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

// NMatching.cpp: implementation of the NMatching class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "NMatching.h"
#include "CompiledStatement.h"
#include "Syntax.h"
#include "Field.h"
#include "Table.h"
#include "Statement.h"
#include "SQLError.h"
#include "Bitmap.h"
#include "SearchHit.h"
#include "NField.h"
#include "Value.h"
#include "ResultList.h"
#include "Database.h"
#include "Context.h"
#include "PrettyPrint.h"

namespace Changjiang {

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

NMatching::NMatching(CompiledStatement *statement, Syntax *syntax)
    : NNode(statement, Matching) {
  field = (NField *)statement->compile(syntax->getChild(0));

  if (field->type != nField)
    throw SQLError(SYNTAX_ERROR,
                   "matching expression must reference table column");

  if (!(field->field->flags & SEARCHABLE))
    throw SQLError(SYNTAX_ERROR, "column %s in table %s is not searchable",
                   field->field->name, field->field->table->name);

  expr = statement->compile(syntax->getChild(1));
  slot = statement->getBitmapSlot();

  Context *context = statement->getContext(field->contextId);
  bool wasComputable = context->computable;
  context->computable = false;

  if (!expr->computable(statement))
    throw SQLEXCEPTION(COMPILE_ERROR,
                       "matching expression must be relative constant");

  context->computable = wasComputable;
}

NMatching::~NMatching() {}

Bitmap *NMatching::evalInversion(Statement *statement) {
  Bitmap *bitmap = statement->bitmaps[slot];

  if (bitmap) {
    bitmap->release();
    statement->bitmaps[slot] = NULL;
  }

  Value *value = expr->eval(statement);
  char *temp;
  const char *string = value->getString(&temp);
  ResultList resultList(statement->connection, field->field);
  statement->database->search(&resultList, string);
  bitmap = statement->bitmaps[slot] = new Bitmap;
  bitmap->addRef();

  for (SearchHit *hit = resultList.hits.next; hit != &resultList.hits;
       hit = hit->next)
    bitmap->set(hit->recordNumber);

  if (temp) delete[] temp;

  return bitmap;
}

int NMatching::evalBoolean(Statement *statement) {
  Bitmap *bitmap = statement->bitmaps[slot];

  if (!bitmap) {
    bitmap = evalInversion(statement);
    bitmap->release();
  }

  Context *context = statement->getContext(field->contextId);

  if (bitmap->isSet(context->recordNumber - 1)) return TRUE_BOOLEAN;

  return FALSE_BOOLEAN;
}

bool NMatching::isInvertible(CompiledStatement *statement, Context *context) {
  return field->contextId == context->contextId && expr->computable(statement);
}

bool NMatching::computable(CompiledStatement *statement) {
  return field->computable(statement) && expr->computable(statement);
  // return expr->computable (statement);
}

void NMatching::prettyPrint(int level, PrettyPrint *pp) {
  pp->indent(level++);
  pp->put("Matching\n");
  printNode(level, field, pp);
  printNode(level, expr, pp);
}

void NMatching::close(Statement *statement) {
  Bitmap *bitmap = statement->bitmaps[slot];

  if (bitmap) {
    bitmap->release();
    statement->bitmaps[slot] = NULL;
  }
}

}  // namespace Changjiang
