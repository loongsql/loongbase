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

// BinaryBlob.cpp: implementation of the BinaryBlob class.
//
//////////////////////////////////////////////////////////////////////

/*
 * copyright (c) 1999 - 2000 by James A. Starkey
 */

#include "Engine.h"
#include "BinaryBlob.h"
#include "AsciiBlob.h"
#include "SQLException.h"
#include "Dbb.h"

#ifdef CHANGJIANGDB
#include "Dbb.h"
#include "Repository.h"
#include "Section.h"
#endif

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

BinaryBlob::BinaryBlob() { init(true); }

BinaryBlob::BinaryBlob(int minSegmentSize) : Stream(minSegmentSize) {
  init(true);
}

#ifdef CHANGJIANGDB
BinaryBlob::BinaryBlob(Dbb *db, int32 recNumber, Section *blobSection) {
  init(false);
  dbb = db;
  recordNumber = recNumber;
  section = blobSection;
}
#endif

BinaryBlob::BinaryBlob(Clob *blob) {
  init(true);

  if (blob->dataUnset)
    unsetData();
  else
    Stream::putSegment(blob);

  copy(blob);
}

BinaryBlob::BinaryBlob(Blob *blob) {
  init(true);

  // Stream::putSegment (blob);
  if (blob->dataUnset)
    unsetData();
  else
    Stream::putSegment(blob);

  copy(blob);
}

void BinaryBlob::init(bool pop) {
  useCount = 1;
  offset = 0;
  populated = pop;
  dbb = NULL;
}

BinaryBlob::~BinaryBlob() {}

void BinaryBlob::addRef() { ++useCount; }

int BinaryBlob::release() {
  if (--useCount == 0) {
    delete this;
    return 0;
  }

  return useCount;
}

void BinaryBlob::getBytes(int32 pos, int32 length, void *address) {
  if (!populated) populate();

  Stream::getSegment(pos, length, address);
}

int BinaryBlob::length() {
  if (dataUnset) return -1;

  if (!populated) populate();

  return totalLength;
}

void BinaryBlob::putSegment(int length, const char *data, bool copyFlag) {
  Stream::putSegment(length, data, copyFlag);
}

int BinaryBlob::getSegmentLength(int pos) {
  if (!populated) populate();

  return Stream::getSegmentLength(pos);
}

void *BinaryBlob::getSegment(int pos) { return Stream::getSegment(pos); }

void BinaryBlob::populate() {
  if (populated) return;

  populated = true;

#ifdef CHANGJIANGDB
  if (repository && isBlobReference()) try {
      repository->getBlob(this);
    } catch (SQLException &exception) {
      totalLength = exception.getSqlcode();
    }
  else if (dbb)
    dbb->fetchRecord(section, recordNumber, this);
#endif
}

void BinaryBlob::putSegment(Blob *blob) { Stream::putSegment(blob); }

void BinaryBlob::putSegment(Clob *blob) { Stream::putSegment(blob); }

void BinaryBlob::putSegment(int32 length, const void *buffer) {
  putSegment(length, (const char *)buffer, true);
}

Stream *BinaryBlob::getStream() {
  if (dataUnset) return NULL;

  if (!populated) populate();

  return this;
}

void BinaryBlob::unsetData() { dataUnset = true; }

}  // namespace Changjiang
