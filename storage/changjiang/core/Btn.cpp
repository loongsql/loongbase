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

// Btn.cpp: implementation of the Btn class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <memory.h>
#include "Engine.h"
#include "Btn.h"
#include "InversionPage.h"
#include "Log.h"
#include "IndexKey.h"
//#include "IndexNode.h"

namespace Changjiang {

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

void Btn::printKey(const char *msg, int length, UCHAR *key, int prefix,
                   bool inversion) {
  int n;
  UCHAR c;
  UCHAR *p = key;

#define PRINT_FIRST_N_BYTES 16
#define PRINT_LAST_N_BYTES 6

  if (msg) Log::debug("%s: ", msg);

  const char *sep = "";

  for (n = 0; n < MIN(length, PRINT_FIRST_N_BYTES); ++n) {
    if (n == prefix) sep = "*";

    c = *p++;
    Log::debug("%s%.2x", sep, c);
    sep = ",";
  }

  Log::debug(" \"");
  p = key;

  for (n = 0; n < MIN(length, PRINT_FIRST_N_BYTES); ++n) {
    if (n == prefix) Log::debug("*");

    c = *p++;

    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == ' ')
      Log::debug("%c", c);
    else
      Log::debug(".");
  }

  Log::debug("\"");

  if ((PRINT_LAST_N_BYTES) && (length > PRINT_FIRST_N_BYTES)) {
    int lastByteCount = MIN(PRINT_LAST_N_BYTES, length - PRINT_LAST_N_BYTES);
    Log::debug("  Last%d:", lastByteCount);
    p = key + length - lastByteCount;
    for (n = length - 1 - lastByteCount; n < length; n++) {
      if (n == prefix) sep = "*";

      c = *p++;
      Log::debug("%s%.2x", sep, c);
      sep = ",";
    }
  }

  if (inversion && length) {
    p = key;
    int32 numbers[4];
    while (*p++)
      ;
    for (int n = 0; n < 4; ++n) numbers[n] = Inv::decode(&p);

    Log::debug(" (%d,%d,%d,%d)", numbers[0], numbers[1], numbers[2],
               numbers[3]);
  } else if (length < 8) {
    union {
      double dbl;
      int64 quad;
      UCHAR chars[8];
    } stuff;

    stuff.quad = 0;

#ifdef _BIG_ENDIAN
    for (UCHAR *q = stuff.chars, *p = key; p < key + length;) *q++ = *p++;
#else
    for (UCHAR *q = stuff.chars + 8, *p = key; p < key + length;) *--q = *p++;
#endif

    if (stuff.quad < 0) stuff.quad ^= QUAD_CONSTANT(0x8000000000000000);

    int n = (int)stuff.dbl;
    Log::debug(" [%d]", n);
  }

  if (msg) Log::debug("\n");
}

void Btn::printKey(const char *msg, IndexKey *key, int prefix, bool inversion) {
  if (key)
    printKey(msg, key->keyLength, key->key, prefix, inversion);
  else if (msg)
    Log::debug("%s: ***null***\n", msg);
}

}  // namespace Changjiang
