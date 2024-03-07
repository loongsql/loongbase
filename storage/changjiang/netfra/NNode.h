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

// NNode.h: interface for the NNode class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Types.h"

namespace Changjiang {

#define TRUE_BOOLEAN 1
#define FALSE_BOOLEAN 0
#define NULL_BOOLEAN -1
//#define INDENT				2

CLASS(Statement);
class CompiledStatement;
class Syntax;
class Table;
CLASS(Field);
class Index;
class LinkedList;
class Context;
class Stream;
class Bitmap;
class Value;
class Collation;
class Stream;
class PrettyPrint;

enum NType {
  Nothing,
  Insert,
  StatementParameter,
  nField,
  Select,
  Add,
  Subtract,
  Multiply,
  Divide,
  Mod,
  Negate,
  Concat,
  Count,
  Min,
  Max,
  Sum,
  Avg,
  Case,
  CaseSearch,
  And,
  Or,
  Not,
  Eql,
  Neq,
  Gtr,
  Geq,
  Lss,
  Leq,
  Like,
  Starting,
  Containing,
  Matching,
  List,
  Between,
  Assign,
  NNull,
  IsNull,
  IsActiveRole,
  NotNull,
  BitmapNode,
  BitmapAnd,
  BitmapOr,
  Constant,
  Update,
  Delete,
  Repair,
  RecordNumber,
  BitSet,
  ValueAlias,
  NextValue,
  InSelect,
  InList,
  InSelectBitmap,
  InListBitmap,
  Exists,
  SelectExpr,
  Replace,
  ConnectionVariable,
  LogBoolean,
  Cast,
  BlobRef,
  Upper,
  Lower
};

class NNode {
 public:
  virtual bool isUniqueIndex();
  virtual void close(Statement *statement);
  virtual void open(Statement *statement);
  virtual void printNode(int level, NNode *child, PrettyPrint *pp);
  void indent(int level, Stream *stream);
  virtual void prettyPrint(int level, PrettyPrint *pp);
  virtual bool isInvertible(CompiledStatement *statement, Context *context);
  virtual bool isRedundantIndex(LinkedList *indexes);
  virtual void fini();
  virtual Collation *getCollation();
  virtual void evalStatement(Statement *statement);
  virtual int evalBoolean(Statement *statement);
  virtual Bitmap *evalInversion(Statement *statement);
  virtual bool isMember(NNode *node);
  virtual bool equiv(NNode *node);
  virtual NNode *makeInversion(CompiledStatement *statement, Context *context,
                               Index *index);
  virtual void increment(Statement *statement);
  virtual bool isStatistical();
  virtual void reset(Statement *statement);
  virtual NNode *copy(CompiledStatement *statement, Context *context);
  virtual FieldType getType();
  virtual void gen(Stream *stream);
  bool containing(const char *string, const char *pattern);
  NNode **copyVector(int numberNodes, NNode **nodes);
  void init(CompiledStatement *statement);
  // void printNode (int level, NNode *node);
  void indent(int level);
  bool like(const char *string, const char *pattern);
  virtual const char *getName();
  virtual bool references(Table *table);
  virtual bool isMember(Table *table);
  virtual Field *getField();
  virtual int matchField(Context *context, Index *index);
  virtual void matchIndex(CompiledStatement *statement, Context *context,
                          Index *index, NNode **min, NNode **max);
  virtual bool computable(CompiledStatement *statement);
  virtual void decomposeConjuncts(LinkedList &conjuncts);
  virtual void setChild(int index, NNode *node);
  NNode(CompiledStatement *statement, NType nType, int numberChildren);
  NNode(CompiledStatement *statement, NType nType);
  virtual ~NNode();

  virtual Value *eval(Statement *statement);
  NNode **allocChildren(int number);

  NType type;
  int count;
  int valueSlot;
  NNode **children;
  NNode *nextNode;
  Collation *collation;
  CompiledStatement *statement;
};

}  // namespace Changjiang
