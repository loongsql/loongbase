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

// StringTransform.cpp: implementation of the StringTransform class.
//
//////////////////////////////////////////////////////////////////////

#include <string.h>
#include <memory.h>
#include "StringTransform.h"

namespace Changjiang {

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

StringTransform::StringTransform() {
  ptr = NULL;
  end = NULL;
  data = NULL;
}

StringTransform::~StringTransform() { delete[] data; }

StringTransform::StringTransform(const char *string, bool copyFlag) {
  data = NULL;
  setString(strlen(string), (const UCHAR *)string, copyFlag);
}

StringTransform::StringTransform(unsigned int length, const UCHAR *stuff,
                                 bool copyFlag) {
  data = NULL;
  setString(length, stuff, copyFlag);
}

void StringTransform::setString(UIPTR length, const UCHAR *stuff,
                                bool copyFlag) {
  delete[] data;
  data = NULL;

  if (copyFlag) {
    ptr = data = new UCHAR[length];
    memcpy(data, stuff, length);
  } else {
    ptr = stuff;
    data = NULL;
  }

  end = ptr + length;
}

unsigned int StringTransform::getLength() { return (unsigned int)(end - ptr); }

unsigned int StringTransform::get(unsigned int bufferLength, UCHAR *buffer) {
  if (ptr >= end) return 0;

  int len = MIN((unsigned int)(end - ptr), bufferLength);
  memcpy(buffer, ptr, len);
  ptr += len;

  return len;
}

void StringTransform::setString(const char *string, bool copyFlag) {
  setString(strlen(string), (const UCHAR *)string, copyFlag);
}

void StringTransform::reset() {}

}  // namespace Changjiang
