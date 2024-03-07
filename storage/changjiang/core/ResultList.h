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

// ResultList.h: interface for the ResultList class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "SearchHit.h"

namespace Changjiang {

class Database;
class ResultSet;
class PreparedStatement;
CLASS(Statement);
class Connection;
class Bitmap;
CLASS(Field);

class ResultList {
 public:
  const char *getSchemaName();
  void init(Connection *cnct);
  ResultList(Connection *cnct, Field *field);
  void clearStatement();
  virtual const char *getTableName();
  double getScore();
  virtual int getCount();
  virtual void close();
  virtual ResultSet *fetchRecord();
  virtual bool next();
  virtual void print();
  void release();
  void addRef();
  void sort();
  SearchHit *add(int32 tableId, int32 recordNumber, int32 fieldId,
                 double score);
  ResultList(Statement *stmt);

  int count;
  int32 handle;
  volatile INTERLOCK_TYPE useCount;
  Connection *connection;
  Database *database;
  SearchHit *nextHit;
  Statement *parent;
  PreparedStatement *statement;
  SearchHit hits;
  Bitmap *tableFilter;
  ResultList *sibling;
  Field *fieldFilter;

  // protected:			// allocated on stack, so destructor must be
  // accessible
  virtual ~ResultList();
};

}  // namespace Changjiang
