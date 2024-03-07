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

// StreamSegment.cpp: implementation of the StreamSegment class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include "Engine.h"
#include "StreamSegment.h"
#include "Stream.h"

namespace Changjiang {

#ifndef CHANGJIANGDB
#undef ASSERT
#define ASSERT(a)
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

StreamSegment::StreamSegment(Stream *stream) { setStream(stream); }

StreamSegment::~StreamSegment() {}

void StreamSegment::setStream(Stream *stream) {
  remaining = stream->totalLength;
  ASSERT(remaining >= 0);

  if ((segment = stream->segments)) {
    data = segment->address;
    available = segment->length;
  } else {
    data = NULL;
    available = 0;
  }
}

void StreamSegment::advance() { advance(available); }

void StreamSegment::advance(int size) {
  // ASSERT (size >= remaining);

  for (int len = size; len;) {
    int l = MIN(available, len);
    available -= l;
    remaining -= l;
    len -= size;
    if (remaining == 0) return;
    if (available)
      data += l;
    else {
      segment = segment->next;
      data = segment->address;
      available = segment->length;
    }
  }
}

char *StreamSegment::copy(void *target, int length) {
  ASSERT(length <= remaining);
  char *targ = (char *)target;

  for (int len = length; len > 0;) {
    int l = MIN(len, available);
    memcpy(targ, data, l);
    targ += l;
    len -= l;
    advance(l);
  }

  return targ;
}

}  // namespace Changjiang
