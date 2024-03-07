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

// Parameters.cpp: implementation of the Parameters class.
//
//////////////////////////////////////////////////////////////////////

// copyright (c) 1999 - 2000 by James A. Starkey

#include <string.h>
#include "Engine.h"
#include "Parameters.h"
#include "Parameter.h"

namespace Changjiang {

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Parameters::Parameters() {
  parameters = NULL;
  count = 0;
}

Parameters::~Parameters() { clear(); }

void Parameters::putValue(const char *name, const char *value) {
  putValue(name, (int)strlen(name), value, (int)strlen(value));
}

void Parameters::putValue(const char *name, int nameLength, const char *value,
                          int valueLength) {
  ++count;
  parameters = new Parameter(parameters, name, nameLength, value, valueLength);
}

const char *Parameters::findValue(const char *name, const char *defaultValue) {
  for (Parameter *parameter = parameters; parameter;
       parameter = parameter->next)
    if (!strcasecmp(name, parameter->name)) return parameter->value;

  return defaultValue;
}

int Parameters::getCount() { return count; }

const char *Parameters::getName(int index) {
  Parameter *parameter = parameters;

  for (int n = 0; n < count; ++n, parameter = parameter->next)
    if (n == index) return parameter->name;

  return NULL;
}

const char *Parameters::getValue(int index) {
  Parameter *parameter = parameters;

  for (int n = 0; n < count; ++n, parameter = parameter->next)
    if (n == index) return parameter->value;

  return NULL;
}

void Parameters::copy(Properties *properties) {
  int count = properties->getCount();

  for (int n = 0; n < count; ++n)
    putValue(properties->getName(n), properties->getValue(n));
}

void Parameters::clear() {
  for (Parameter *parameter; (parameter = parameters);) {
    parameters = parameter->next;
    delete parameter;
  }

  count = 0;
}

int Parameters::getValueLength(int index) {
  Parameter *parameter = parameters;

  for (int n = 0; n < count; ++n, parameter = parameter->next)
    if (n == index) return parameter->valueLength;

  return 0;
}

void Parameters::setValue(const char *name, const char *value) {
  for (Parameter *parameter = parameters; parameter;
       parameter = parameter->next)
    if (!strcasecmp(name, parameter->name)) {
      parameter->setValue(value);
      return;
    }

  putValue(name, value);
}

}  // namespace Changjiang
