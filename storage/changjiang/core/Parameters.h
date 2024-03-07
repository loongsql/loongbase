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

// Parameters.h: interface for the Parameters class.
//
//////////////////////////////////////////////////////////////////////

// copyright (c) 1999 - 2000 by James A. Starkey

#pragma once

START_NAMESPACE
#include "Properties.h"
END_NAMESPACE

namespace Changjiang {

class Parameter;

class Parameters : public Properties {
 public:
  void setValue(const char *name, const char *value);
  virtual int getValueLength(int index);
  void clear();
  void copy(Properties *properties);
  virtual const char *getValue(int index);
  virtual const char *getName(int index);
  virtual int getCount();
  virtual const char *findValue(const char *name, const char *defaultValue);
  virtual void putValue(const char *name, int nameLength, const char *value,
                        int valueLength);
  virtual void putValue(const char *name, const char *value);
  Parameters();
  virtual ~Parameters();

  Parameter *parameters;
  int count;
};

}  // namespace Changjiang
