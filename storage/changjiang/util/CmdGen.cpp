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

#include <stdarg.h>
#include <stdio.h>
#include "CmdGen.h"

namespace Changjiang {

#ifndef NULL
#define NULL 0
#endif

#ifndef MIN
#define MIN(a, b) ((a < b) ? a : b)
#endif

#if (defined _MSC_VER && _MSC_VER < 1400)
#define vsnprintf _vsnprintf
#endif

CmdGen::CmdGen(void) { init(); }

CmdGen::~CmdGen(void) { reset(); }

void CmdGen::init(void) {
  ptr = buffer;
  remaining = sizeof(buffer);
  currentHunk = NULL;
  hunks = NULL;
  totalLength = 0;
  temp = NULL;
}

void CmdGen::gen(const char *command, ...) {
  va_list args;
  va_start(args, command);
  char temp[1024];

  if (vsnprintf(temp, sizeof(temp) - 1, command, args) < 0)
    temp[sizeof(temp) - 1] = 0;

  put(temp);
}

void CmdGen::put(const char *command) {
  size_t length = strlen(command);
  const char *p = command;

  while (length) {
    size_t len = MIN(length, remaining);

    if (len) {
      memcpy(ptr, p, len);
      ptr += len;
      p += len;
      remaining -= len;
      length -= len;
      totalLength += len;
    }

    if (length == 0) break;

    CmdHunk *hunk = new CmdHunk;
    hunk->next = NULL;

    if (currentHunk)
      currentHunk->next = hunk;
    else
      hunks = hunk;

    currentHunk = hunk;
    ptr = hunk->data;
    remaining = sizeof(hunk->data);
  }
}

const char *CmdGen::getString(void) {
  if (!hunks && remaining) {
    *ptr = 0;

    return buffer;
  }

  delete[] temp;
  temp = new char[totalLength + 1];
  memcpy(temp, buffer, sizeof(buffer));
  char *p = temp + sizeof(buffer);

  if (hunks) {
    for (CmdHunk *hunk = hunks; hunk->next; hunk = hunk->next) {
      memcpy(p, hunk->data, sizeof(hunk->data));
      p += sizeof(hunk->data);
    }

    size_t len = sizeof(currentHunk->data) - remaining;
    memcpy(p, currentHunk->data, len);
    p += len;
  }

  *p = 0;

  return temp;
}

void CmdGen::reset(void) {
  for (CmdHunk *hunk; (hunk = hunks);) {
    hunks = hunk->next;
    delete hunk;
  }

  delete[] temp;
  init();
}

}  // namespace Changjiang
