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

// Collation.h: interface for the Collation class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

namespace Changjiang {

class Value;
class IndexKey;

class Collation {
 public:
  Collation();

  virtual int compare(Value *value1, Value *value2) = 0;
  virtual int makeKey(Value *value, IndexKey *key, int partialKey,
                      int maxKeyLength, bool highKey) = 0;
  virtual bool starting(const char *string1, const char *string2) = 0;
  virtual bool like(const char *string, const char *pattern) = 0;
  virtual char getPadChar(void) = 0;
  virtual const char *getName() = 0;
  virtual int truncate(Value *value, int partialLength) = 0;

  void addRef(void);
  void release(void);

 protected:
  virtual ~Collation();

 public:
  Collation *collision;
  int useCount;
};

}  // namespace Changjiang
