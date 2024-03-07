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

// BinaryBlob.h: interface for the BinaryBlob class.
//
//////////////////////////////////////////////////////////////////////

/*
 * copyright (c) 1999 - 2000 by James A. Starkey
 */

#pragma once

#include "Blob.h"
#include "Stream.h"

namespace Changjiang {

class Dbb;
class AsciiBlob;
class Section;

class BinaryBlob : public Blob, public Stream {
 public:
  BinaryBlob(Blob *blob);
  BinaryBlob(Dbb *db, int32 recordNumber, Section *blobSection);
  BinaryBlob(Clob *blob);
  BinaryBlob(int minSegmentSize);
  BinaryBlob();
  virtual ~BinaryBlob();

  void init(bool populated);
  virtual void unsetData();
  Stream *getStream();
  virtual void putSegment(int32 length, const void *buffer);
  void putSegment(Blob *blob);
  void putSegment(Clob *blob);

  virtual void *getSegment(int pos);
  virtual int getSegmentLength(int pos);
  void putSegment(int length, const char *data, bool copyFlag);
  int length();
  void getBytes(int32 pos, int32 length, void *address);
  virtual int release();
  virtual void addRef();
  void populate();

  int useCount;
  int offset;
  Dbb *dbb;
  Section *section;
  int32 recordNumber;
  bool populated;
};

}  // namespace Changjiang
