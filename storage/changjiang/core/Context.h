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

// Context.h: interface for the Context class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

namespace Changjiang {

enum ContextType {
  CtxInnerJoin,
  CtxOuterJoin,
  CtxInsert,
};

class Table;
CLASS(Statement);
class Record;
class Bitmap;
class Sort;
class LinkedList;

class Context {
 public:
  void checkRecordLimits(Statement *statement);
  void setRecord(Record *record);
  void close();
  void open();
  void getTableContexts(LinkedList &list, ContextType contextType);
  bool isContextName(const char *name);
  bool fetchIndexed(Statement *statement);
  void setBitmap(Bitmap *map);
  bool fetchNext(Statement *statement);
  void initialize(Context *context);
  Context();
  bool setComputable(bool flag);
  Context(int id, Table *tbl, int32 privMask);
  virtual ~Context();

  Table *table;
  Context **viewContexts;
  int contextId;
  bool computable;
  bool eof;
  bool select;
  const char *alias;
  int32 recordNumber;
  int32 privilegeMask;
  Record *record;
  Bitmap *bitmap;
  Sort *sort;
  ContextType type;
};

}  // namespace Changjiang
