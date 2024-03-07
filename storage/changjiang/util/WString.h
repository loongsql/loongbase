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

// WString.h: interface for the WString class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#ifdef _WIN32
#include <wchar.h>
typedef wchar_t WCHAR;
#else
typedef unsigned short WCHAR;
#endif

namespace Changjiang {

class JString;

class WString {
 public:
  static JString getString(const WCHAR *name, int length);
  WString(const WCHAR *str);
  WString(const char *string);
  WString();
  WString(WString &source);
  void setString(const WCHAR *wString, int len);
  int length();
  operator JString();
  WString &operator=(const WCHAR *str);
  WString &operator=(const char *str);
  operator const WCHAR *();
  void assign(const WCHAR *str);
  void assign(const char *str);
  void release();
  ~WString();

  static int length(const WCHAR *string);
  static int hash(const WCHAR *string, int length, int hashSize);

  WCHAR *string;
};

}  // namespace Changjiang
