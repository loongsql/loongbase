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

// Base64Transform.cpp: implementation of the Base64Transform class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Base64Transform.h"

namespace Changjiang {

static char digits[64];
static char lookup[128];

static int initialize();
static int foo = initialize();

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

int initialize() {
  int n = 0;
  int c;

  for (const char *p = "./"; *p;) {
    lookup[(int)*p] = n;
    digits[n++] = *p++;
  }

  for (c = '0'; c <= '9'; ++c) {
    lookup[c] = n;
    digits[n++] = c;
  }

  for (c = 'A'; c <= 'Z'; ++c) {
    lookup[c] = n;
    digits[n++] = c;
  }

  for (c = 'a'; c <= 'z'; ++c) {
    lookup[c] = n;
    digits[n++] = c;
  }

  return 0;
}

Base64Transform::Base64Transform(bool encodeFlag, Transform *src) {
  encode = encodeFlag;
  source = src;
  ignoreStrays = false;
  reset();
}

Base64Transform::~Base64Transform() {}

unsigned int Base64Transform::getLength() {
  int len = source->getLength();

  return (encode) ? (len + 2) / 3 * 4 + 2 : (len + 3) / 4 * 3;
}

unsigned int Base64Transform::get(unsigned int bufferLength, UCHAR *buffer) {
  UCHAR *p = buffer;
  UCHAR *endBuffer = buffer + bufferLength;

  if (encode) {
    if (tail) {
      while (p < endBuffer && *tail) *p++ = *tail++;

      return (unsigned int)(p - buffer);
    }
    while (p < endBuffer) {
      if (bitsRemaining < 6) {
        if (ptr >= end) {
          ptr = input;
          int len = source->get(sizeof(input), input);
          inputLength += len;
          end = ptr + len;

          if (len == 0) {
            int mod = inputLength % 3;
            if (mod) {
              UCHAR *q = tail = input;
              int n;
              for (n = 0; n < mod; ++n) {
                if (bitsRemaining < 6) {
                  bits <<= 8;
                  bitsRemaining += 8;
                }
                bitsRemaining -= 6;
                *q++ = digits[(bits >> bitsRemaining) & 0x3f];
              }
              for (n = 3; n > mod; --n) *q++ = '*';
              *q = 0;
              inputLength = 0;
              while (p < endBuffer && *tail) *p++ = *tail++;
            }
            break;
          }
        }
        bits = (bits << 8) | *ptr++;
        bitsRemaining += 8;
      }
      bitsRemaining -= 6;
      *p++ = digits[(bits >> bitsRemaining) & 0x3f];
    }
  } else {
    if (ptr < end && *ptr == '*') bitsRemaining = 0;

    while (p < endBuffer) {
      if (bitsRemaining >= 8) {
        bitsRemaining -= 8;
        *p++ = (UCHAR)(bits >> bitsRemaining);
      }
      if (ptr >= end) {
        ptr = input;
        int len = source->get(sizeof(input), input);
        end = ptr + len;
        if (len == 0) {
          if (ignoreStrays && p == buffer) break;
          if (bitsRemaining && p < endBuffer) {
            *p++ = bits;
            bitsRemaining = 0;
          }
          break;
        }
      }
      UCHAR c = *ptr++;
      UCHAR n = lookup[c];
      if (digits[n] == c) {
        bits <<= 6;
        bits |= n;
        bitsRemaining += 6;
      } else if (c == '*')
        bitsRemaining = 0;
    }
  }

  return (unsigned int)(p - buffer);
}

void Base64Transform::reset() {
  ptr = end = input;
  bitsRemaining = 0;
  bits = 0;
  inputLength = 0;
  tail = NULL;
}

}  // namespace Changjiang
