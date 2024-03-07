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

// PreparedStatement.h: interface for the PreparedStatement class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Statement.h"

namespace Changjiang {

#ifndef __ENGINE_H
typedef __int64 int64;
#endif

class Database;
class Blob;
class DateTime;
class TimeStamp;
class Time;
class Blob;
class Clob;

class PreparedStatement : public Statement  //,  public DbPreparedStatement
{
 public:
  virtual void setTime(int index, Time *value);
  virtual bool isPreparedResultSet();
  void setString(int index, const WCString *string);
  void setSqlString(const WCString *sqlString);
  virtual void setShort(int index, short value);
  int32 getTerLong(const char **p);
  virtual void setLong(int index, int64 value);
  virtual void setDate(int index, DateTime *value);
  virtual void setBoolean(int index, int value);
  virtual void setByte(int index, char value);
  virtual void setBytes(int index, int length, const char *bytes);
  virtual void setValue(int index, Value *value);
  virtual bool execute();
  virtual void setClob(int index, Clob *blob);
  // virtual Blob* setBlob (int index);
  virtual void setDouble(int index, double value);
  virtual ResultSet *executeQuery();
  virtual int executeUpdate();
  virtual void setInt(int index, int value);
  virtual void setString(int index, const char *string);
  virtual void setSqlString(const char *sqlStr);
  virtual void setNull(int index, int type);
  virtual void setRecord(int length, const char *bytes);
  virtual void setTimestamp(int index, TimeStamp *value);
  virtual void setFloat(int index, float value);
  virtual void setBlob(int index, Blob *value);
  PreparedStatement(Connection *connection, Database *dbb);

 protected:
  virtual ~PreparedStatement();
};

}  // namespace Changjiang
