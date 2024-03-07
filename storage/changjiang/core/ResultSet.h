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

// ResultSet.h: interface for the ResultSet class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Values.h"
#include "LinkedList.h"
#include "SyncObject.h"
#include "TimeStamp.h"
#include "DateTime.h"  // Added by ClassView

namespace Changjiang {

CLASS(Statement);
class NSelect;
class Value;
class ResultSetMetaData;
CLASS(Field);
class Blob;
class Clob;
class BinaryBlob;
class AsciiBlob;
class TemplateContext;
class Database;
class Table;
class CompiledStatement;
class Connection;

class ResultSet {
 protected:
  ResultSet();
  ResultSet(int count);
  virtual ~ResultSet();

 public:
  void putTerQuad(int64 value, BinaryBlob *blob);
  Clob *getClob(const char *name);
  Clob *getClob(int index);
  void print(Stream *stream);
  void statementClosed();
  virtual Statement *getStatement();
  void clearConnection();
  void transactionEnded();
  const char *getSymbol(int index);
  void putTerLong(int32 value, BinaryBlob *blob);
  void connectionClosed();
  int getIndex(Field *field);
  virtual int getColumnIndex(const WCString *name);
  virtual int findColumn(const WCString *name);
  virtual int findColumn(const char *name);
  virtual bool wasNull();
  virtual Blob *getBlob(int index);
  virtual ResultSetMetaData *getMetaData();
  virtual double getDouble(int id);
  virtual void close();
  virtual int getInt(int id);
  virtual bool next();
  virtual const char *getString(int id);
  virtual int getColumnIndex(const char *name);
  virtual Value *getValue(int index);
  virtual DateTime getDate(int id);
  virtual DateTime getDate(const char *name);
  virtual int findColumnIndex(const char *name);
  virtual Blob *getRecord();
  virtual int getInt(const char *name);
  virtual int64 getLong(const char *name);
  virtual int64 getLong(int id);
  virtual void releaseJavaRef();
  virtual void addJavaRef();
  virtual double getDouble(const char *name);
  virtual float getFloat(const char *name);
  virtual float getFloat(int id);
  virtual TimeStamp getTimestamp(const char *name);
  virtual TimeStamp getTimestamp(int id);
  virtual short getShort(const char *name);
  virtual short getShort(int id);
  virtual Time getTime(const char *fieldName);
  virtual Time getTime(int id);
  Blob *getBlob(const char *name);

  void allocConversions();
  void init(int count);
  const char *getColumnName(int index);
  void clearStatement();
  void release();
  void addRef();
  Field *getField(const char *fieldName);
  bool isMember(Field *field);
  bool isMember(Table *table);
  Database *getDatabase();
  Value *getValue(const char *name);
  const char *getString(const char *name);
  void deleteBlobs();
  Field *getField(int index);
  void setValue(int column, Value *value);
  ResultSet(Statement *statement, NSelect *node, int numberColums);

  SyncObject syncObject;
  Values values;
  Statement *statement;
  CompiledStatement *compiledStatement;
  Connection *connection;
  Database *database;
  ResultSet *sibling;
  ResultSet *connectionNext;
  int32 currentRow;
  int32 numberColumns;
  int32 numberRecords;
  NSelect *select;
  char **conversions;
  ResultSetMetaData *metaData;
  LinkedList blobs;
  LinkedList clobs;
  short valueWasNull;
  bool active;
  volatile INTERLOCK_TYPE useCount;
  volatile INTERLOCK_TYPE javaCount;
  int32 javaStatementCount;
  int32 handle;
};

}  // namespace Changjiang
