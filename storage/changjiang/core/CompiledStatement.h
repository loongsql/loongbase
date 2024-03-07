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

// CompiledStatement.h: interface for the CompiledStatement class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Stack.h"
#include "LinkedList.h"
#include "SyncObject.h"
#include "Types.h"

namespace Changjiang {

class Database;
class Syntax;
class SQLParse;
class NNode;
class NSelect;
CLASS(Field);
class Table;
class Context;
class NField;
class Fsb;
class LinkedList;
class Index;
class Connection;
CLASS(Statement);
class View;
class Sequence;
class FilterSet;
class FilterSetManager;
class TableFilter;
class Row;

class CompiledStatement {
 public:
  void renameTables(Syntax *syntax);
  int getRowSlot();
  int getValueSetSlot();
  void invalidate();
  Type getType(Syntax *syntax);
  void decomposeAdjuncts(Syntax *syntax, LinkedList &adjuncts);
  int getBitmapSlot();
  int countInstances();
  bool addFilter(TableFilter *filter);
  FilterSet *findFilterset(Syntax *syntax);
  void checkAccess(Connection *connection);
  Sequence *findSequence(Syntax *syntax, bool search);
  bool isInvertible(Context *context, NNode *node);
  void *markContextStack();
  void popContexts(void *mark);
  View *getView(const char *sql);
  void deleteInstance(Statement *instance);
  void addInstance(Statement *instance);
  void addRef();
  void compile(JString sqlStr);
  NNode *compile(Syntax *syntax);
  NNode *compileField(Syntax *syntax);
  NNode *compileField(Field *field, Context *context);
  NNode *compileFunction(Syntax *syntax);
  NNode *compileSelect(Syntax *syntax, NNode *inExpr);
  Fsb *compileStream(Context *context, LinkedList &conjuncts, Row *row);
  bool contextComputable(int contextId);
  Table *findTable(Syntax *syntax, bool search);
  NNode *genInversion(Context *context, Index *index, LinkedList &booleans);
  NNode *mergeInversion(NNode *node1, NNode *node2);
  NNode *isDirectReference(Context *context, NNode *node);
  NNode *genInversion(Context *context, NNode *node, LinkedList *booleans);
  int getValueSlot();
  int getGeneralSlot();
  int getSortSlot();
  Table *getTable(Syntax *syntax);
  bool references(Table *table);
  void noteUnqualifiedTable(Table *table);
  Context *compileContext(Syntax *syntax, int32 privMask);
  Context *makeContext(Table *table, int32 privMask);
  Context *popContext();
  Context *getContext(int contextId);
  void pushContext(Context *context);
  void release();
  bool validate(Connection *connection);
  JString getNameString(Syntax *syntax);

  static Table *getTable(Connection *connection, Syntax *syntax);
  static Field *findField(Table *table, Syntax *syntax);

  CompiledStatement(Connection *connection);
  virtual ~CompiledStatement();

  SyncObject syncObject;
  JString sqlString;
  Database *database;
  volatile INTERLOCK_TYPE useCount;
  SQLParse *parse;
  Syntax *syntax;
  NNode *node;
  NNode *nodeList;
  NSelect *select;
  LinkedList contexts;
  Stack filters;
  int numberParameters;
  int numberValues;
  int numberContexts;
  int numberSlots;
  int numberSorts;
  int numberBitmaps;
  int numberValueSets;
  int numberRowSlots;
  Stack contextStack;
  Stack unqualifiedTables;
  Stack unqualifiedSequences;
  Stack filteredTables;
  bool ddl;
  bool useable;
  time_t lastUse;

  Connection *connection;  // valid only during compile
  Statement *firstInstance;
  Statement *lastInstance;
  CompiledStatement *next;  // next in database
};

}  // namespace Changjiang
