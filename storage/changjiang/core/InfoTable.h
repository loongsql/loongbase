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

#pragma once

#ifdef _WIN32
typedef __int64 INT64;
#else
typedef long long INT64;
#endif

class THD;
struct CHARSET_INFO;
struct TABLE_LIST;
struct TABLE;

namespace Changjiang {

class InfoTable {
 public:
  virtual ~InfoTable() { ; }
  virtual void putRecord(void) = 0;
  virtual void putInt(int column, int value) = 0;
  virtual void putInt64(int column, INT64 value) = 0;
  virtual void putDouble(int column, double value) = 0;
  virtual void putString(int column, const char *string) = 0;
  virtual void putString(int column, unsigned int stringLength,
                         const char *string) = 0;
  virtual void setNull(int column) = 0;
  virtual void setNotNull(int column) = 0;
};

class InfoTableImpl : public InfoTable {
 public:
  InfoTableImpl(THD *thd, TABLE_LIST *tables, CHARSET_INFO *scs);
  virtual ~InfoTableImpl(void);

  virtual void putRecord(void);
  virtual void putInt(int column, int value);
  virtual void putInt64(int column, INT64 value);
  virtual void putString(int column, const char *string);
  virtual void putString(int column, unsigned int stringLength,
                         const char *string);
  virtual void putDouble(int column, double value);
  virtual void setNull(int column);
  virtual void setNotNull(int column);

  int error;
  TABLE *table;
  THD *mySqlThread;
  CHARSET_INFO *charSetInfo;
};

}  // namespace Changjiang
