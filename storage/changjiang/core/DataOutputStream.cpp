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

// DataOutputStream.cpp: implementation of the DataOutputStream class.
//
//////////////////////////////////////////////////////////////////////

#include <string.h>
#include "Engine.h"
#include "DataOutputStream.h"
//#include "JavaVM.h"
//#include "JavaClass.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

DataOutputStream::DataOutputStream() {}

DataOutputStream::~DataOutputStream() {}

void DataOutputStream::writeLong(int64 value) {
  char buf[8];
  buf[0] = (UCHAR)(value >> 56);
  buf[1] = (UCHAR)(value >> 48);
  buf[2] = (UCHAR)(value >> 40);
  buf[3] = (UCHAR)(value >> 32);
  buf[4] = (UCHAR)(value >> 24);
  buf[5] = (UCHAR)(value >> 16);
  buf[6] = (UCHAR)(value >> 8);
  buf[7] = (UCHAR)(value >> 0);
  putSegment(sizeof(buf), buf, true);
}

void DataOutputStream::writeInt(int32 value) {
  char buf[4];
  buf[0] = (UCHAR)(value >> 24);
  buf[1] = (UCHAR)(value >> 16);
  buf[2] = (UCHAR)(value >> 8);
  buf[3] = (UCHAR)(value >> 0);
  putSegment(sizeof(buf), buf, true);
}

void DataOutputStream::writeShort(short value) {
  char buf[2];
  buf[0] = (UCHAR)(value >> 8);
  buf[1] = (UCHAR)(value >> 0);
  putSegment(sizeof(buf), buf, true);
}

void DataOutputStream::writeUTF(int length, WCHAR *string) {
  writeShort(length);
  putSegment(length, string);
}

/***
void DataOutputStream::writeUTF(JavaName * name)
{
        writeUTF (name->length, name->string);
}
***/

void DataOutputStream::writeUTF(const char *string) {
  writeShort((short)strlen(string));
  putSegment(string);
}

}  // namespace Changjiang
