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

// PrettyPrint.cpp: implementation of the PrettyPrint class.
//
//////////////////////////////////////////////////////////////////////

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include "Engine.h"
#include "PrettyPrint.h"
#include "Stream.h"
#include "Log.h"

namespace Changjiang {

#define INDENT 2

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

PrettyPrint::PrettyPrint(int flgs, Stream *strm) {
  flags = flgs;
  stream = strm;
}

PrettyPrint::~PrettyPrint() {}

void PrettyPrint::indent(int level) {
  if (stream) {
    int count = level * INDENT;
    for (int n = 0; n < count; ++n) stream->putCharacter(' ');
  } else
    Log::debug("%*s ", level * INDENT, "");
}

void PrettyPrint::put(const char *string) {
  if (stream)
    stream->putSegment(string);
  else
    Log::debug(string);
}

void PrettyPrint::putLine(const char *string) {
  if (stream) {
    stream->putSegment(string);
    stream->putCharacter('\n');
  } else
    Log::debug("%s\n", string);
}

void PrettyPrint::format(const char *pattern, ...) {
  va_list args;
  va_start(args, pattern);
  char temp[1024];

  if (vsnprintf(temp, sizeof(temp) - 1, pattern, args) < 0)
    temp[sizeof(temp) - 1] = 0;

  put(temp);
}

}  // namespace Changjiang
