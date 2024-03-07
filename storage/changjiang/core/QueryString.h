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

// QueryString.h: interface for the QueryString class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

namespace Changjiang {

#define QUERY_HASH_SIZE 11

class Parameter;
struct WCString;

class QueryString {
 public:
  void release();
  void addRef();
  Parameter *findParameter(const WCString *name);
  const char *getParameter(const WCString *name);
  QueryString();
  const char *getParameter(const char *name, const char *defaultValue);
  static int getParameter(const char *queryString, const char *name,
                          int bufferLength, char *buffer);
  QueryString(const char *string);
  virtual ~QueryString();
  void setString(const char *string);
  char getHex(const char **ptr);

  const char *queryString;
  Parameter *variables[QUERY_HASH_SIZE];
  volatile INTERLOCK_TYPE useCount;
};

}  // namespace Changjiang
