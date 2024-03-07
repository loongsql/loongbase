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

// Parameter.cpp: implementation of the Parameter class.
//
//////////////////////////////////////////////////////////////////////

// copyright (c) 1999 - 2000 by James A. Starkey

#include <string.h>
#include <memory.h>
#include "Engine.h"
#include "Parameter.h"
#include "JString.h"

namespace Changjiang {

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Parameter::Parameter(Parameter *nxt, const char *nam, int namLen,
                     const char *val, int valLen) {
  next = nxt;
  nameLength = namLen;
  valueLength = valLen;
  name = new char[nameLength + valueLength + 2];
  memcpy(name, nam, nameLength);
  name[nameLength] = 0;
  value = name + nameLength + 1;
  memcpy(value, val, valueLength);
  value[valueLength] = 0;
  collision = NULL;
}

Parameter::~Parameter() { delete[] name; }

void Parameter::setValue(const char *newValue) {
  if (!strcmp(value, newValue)) return;

  char *oldName = name;
  valueLength = (int)strlen(newValue);
  name = new char[nameLength + valueLength + 2];
  memcpy(name, oldName, nameLength);
  name[nameLength] = 0;
  value = name + nameLength + 1;
  memcpy(value, newValue, valueLength);
  value[valueLength] = 0;
  delete[] oldName;
}

bool Parameter::isNamed(const WCString *parameter) {
  if (nameLength != parameter->count) return false;

  for (int n = 0; n < nameLength; ++n)
    if (name[n] != parameter->string[n]) return false;

  return true;
}

}  // namespace Changjiang
