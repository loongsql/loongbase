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

// BERDecode.h: interface for the BERDecode class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "JString.h"

namespace Changjiang {

static const int INTEGER = 0x2;
static const int BIT_STRING = 0x3;
static const int OCTET_STRING = 0x4;
static const int NULL_TAG = 0x5;
static const int OBJECT_TAG = 0x6;
static const int SEQUENCE = 0x10;   // 16
static const int SET = 0x11;        // 17
static const int PRINTABLE = 0x13;  // 19;
static const int T61String = 0x14;  // 20;
static const int IA5String = 0x16;  // 22;
static const int UTCTime = 0x17;    // 23;

static const int CONSTRUCTED = 0x20;
static const int TAG_MASK = 0x1f;

static const int MAX_LEVELS = 16;

class BERItem;
class Transform;

class BERDecode {
 public:
  int getInteger();
  BERDecode(int length, const UCHAR *gook);
  int getRawLength();
  void print(int level);
  static JString getTagString(int tagNumber);
  void print();
  void popUp();
  void pushDown();
  void next(int requiredTagNumber);
  BERDecode(BERDecode *parent);
  bool next();
  void setSource(Transform *src);
  BERDecode(Transform *src);
  int getType();
  int getLength();
  UCHAR getOctet();
  virtual ~BERDecode();

  const UCHAR *end;
  const UCHAR *ptr;
  int contentLength;
  bool definiteLength;
  UCHAR tagClass;
  UCHAR tagNumber;
  const UCHAR *content;
  Transform *source;
  UCHAR *buffer;
  int level;
  const UCHAR *state[2 * MAX_LEVELS];
};

}  // namespace Changjiang
