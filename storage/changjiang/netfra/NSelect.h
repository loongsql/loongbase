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

// NSelect.h: interface for the NSelect class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "NNode.h"
#include "Row.h"

namespace Changjiang {

struct NSelectEnv {
  LinkedList *sourceContexts;
  LinkedList *tables;
  LinkedList *conjuncts;
};

class Context;
class Fsb;
class ResultSet;
class Value;
class Table;

class NSelect : public NNode, public Row {
 public:
  NSelect(CompiledStatement *statement, Syntax *syntax, NNode *inExpr);
  virtual ~NSelect();

  void pushContexts();
  NNode *getValue(const char *name);
  int compileJoin(Syntax *node, bool innerJoin, NSelectEnv *env);
  int getIndex(Field *field);
  int getColumnIndex(const char *columnName);
  const char *getColumnName(int index);
  bool next(Statement *statement, ResultSet *resultSet);
  void evalStatement(Statement *statement);

  virtual Field *getField(int index);
  virtual bool computable(CompiledStatement *statement);
  virtual int getNumberValues();
  virtual Value *getValue(Statement *statement, int index);
  virtual Field *getField(const char *fieldName);
  virtual bool references(Table *table);
  virtual bool isMember(Table *table);
  virtual void prettyPrint(int level, PrettyPrint *pp);

  int numberContexts;
  int numberColumns;
  int countSlot;
  const char **columnNames;
  Context **contexts;
  int contextMap;
  NNode *values;
  NNode *orgBoolean;
  NNode *groups;
  NSelect *select;
  Fsb *stream;
  NSelect **unionBranches;
  int *groupSlots;
  bool statistical;
  CompiledStatement *compiledStatement;
};

}  // namespace Changjiang
