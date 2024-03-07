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

// Stream.h: interface for the Stream class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "JString.h"
#include "WString.h"

namespace Changjiang {

#define FIXED_SEGMENT_SIZE 1024

struct Segment {
  int length;
  char *address;
  Segment *next;
  char tail[FIXED_SEGMENT_SIZE];
};

class Blob;
class Clob;

class Stream {
 public:
  void transfer(Stream *stream);
  int compare(Stream *stream);
  void truncate(int length);
  virtual void format(const char *pattern, ...);
  void setMalloc(bool flag);
  void putSegment(int length, const WCHAR *chars);
  void putSegment(Stream *stream);
  virtual void putSegment(const char *string);
  virtual void putSegment(int length, const char *address, bool copy);
  void putSegment(Blob *blob);
  void putSegment(Clob *blob);
  void putCharacter(char c);

  virtual void setSegment(Segment *segment, int length, void *address);
  virtual int getSegment(int offset, int length, void *address);
  virtual int getSegment(int offset, int len, void *ptr, char delimiter);
  void *getSegment(int offset);
  int getSegmentLength(int offset);

  JString getJString();
  virtual char *getString();
  void clear();
  virtual int getLength();
  virtual char *alloc(int length);

  Segment *allocSegment(int tail);
  void setMinSegment(int length);

#ifdef CHANGJIANGDB
  char *decompress(int tableId, int recordNumber);
  void compress(int length, void *address);
  void printShorts(const char *msg, int length, short *data);
  void printChars(const char *msg, int length, const char *data);
#endif

  Stream(int minSegmentSize = FIXED_SEGMENT_SIZE);
  virtual ~Stream();

  int totalLength;
  int minSegment;
  int currentLength;
  int decompressedLength;
  int useCount;
  bool copyFlag;
  bool useMalloc;
  Segment first;
  Segment *segments;
  Segment *current;
  void indent(int spaces);
};

}  // namespace Changjiang
