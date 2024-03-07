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

// Unicode.h: interface for the Unicode class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

namespace Changjiang {

typedef unsigned char *Utf8Ptr;

class Unicode {
 public:
  static unsigned short getUnicode(unsigned int c);
  static void fixup(const char *from, char *to);
  static int validate(const char *string);
  static char *convert(int length, const char *stuff, char *utf8);
  static int getUtf8Length(int length, const unsigned short *chars);
  static int getUtf8Length(int length, const char *utf8);
  static unsigned short *convert(const char *utf8, unsigned short *utf16);
  static unsigned short *convert(int length, const char *utf8,
                                 unsigned short *utf16);
  static void convert(int inLength, const char **inPtr, int outLength,
                      char **outPtr);
  static int getNumberCharacters(int length, const char *ptr);
  static int getNumberCharacters(const char *p);
  static UCHAR *convert(int length, const unsigned short *utf16, char *utf8);

  static int getUtf8Length(uint32 code) {
    return (code <= 0x7f)        ? 1
           : (code <= 0x7ff)     ? 2
           : (code <= 0xffff)    ? 3
           : (code <= 0x1fffff)  ? 4
           : (code <= 0x3ffffff) ? 5
                                 : 6;
  }
};

}  // namespace Changjiang
