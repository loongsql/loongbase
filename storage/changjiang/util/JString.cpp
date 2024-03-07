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

/*
 *	PROGRAM:		Virtual Data Manager
 *	MODULE:			JString.cpp
 *	DESCRIPTION:	Transportable flexible string
 *
 * copyright (c) 1997 - 2000 by James A. Starkey
 */

#include <memory.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include "Engine.h"
#include "Unicode.h"

namespace Changjiang {

#ifndef ASSERT
#define ASSERT(expr)
#endif

#define ISLOWER(c) (c >= 'a' && c <= 'z')
#define UPPER(c) ((ISLOWER(c)) ? c - 'a' + 'A' : c)

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

JString::JString() {
  /**************************************
   *
   *		J S t r i n g
   *
   **************************************
   *
   * Functional description
   *		Initialize string object.
   *
   **************************************/

  string = NULL;
  isStaticString = false;
}

JString::JString(const char *stuff) {
  /**************************************
   *
   *		J S t r i n g
   *
   **************************************
   *
   * Functional description
   *		Initialize string object.
   *
   **************************************/

  string = NULL;
  isStaticString = false;
  setString(stuff);
}

JString::JString(const JString &source) {
  /**************************************
   *
   *		J S t r i n g
   *
   **************************************
   *
   * Functional description
   *		Copy constructor.
   *
   **************************************/

  if ((string = source.string)) ++((int *)string)[-1];
  isStaticString = false;
}

JString::~JString() {
  /**************************************
   *
   *		~ J S t r i n g
   *
   **************************************
   *
   * Functional description
   *		Initialize string object.
   *
   **************************************/

  // Error::validateHeap ("JString::~JString 1");
  release();
  // Error::validateHeap ("JString::~JString 1");
}

void JString::append(const char *stuff) {
  /**************************************
   *
   *		a p p e n d
   *
   **************************************
   *
   * Functional description
   *		Append string.
   *
   **************************************/

  if (!string) {
    setString(stuff);

    return;
  }

  char *temp = string;
  ++((int *)temp)[-1];
  int l1 = (int)strlen(temp);
  int l2 = (int)strlen(stuff);
  release();
  alloc(l1 + l2);
  memcpy(string, temp, l1);
  memcpy(string + l1, stuff, l2);
  temp -= sizeof(int);

  if (--((int *)temp)[0] == 0) delete[] temp;
}

void JString::append(const char *stuff, int length) {
  /**************************************
   *
   *		a p p e n d
   *
   **************************************
   *
   * Functional description
   *		Append string of specified length.
   *
   **************************************/

  if (!string) {
    setString(stuff, length);

    return;
  }

  char *temp = string;
  ++((int *)temp)[-1];
  int l1 = (int)strlen(temp);
  release();
  alloc(l1 + length);
  memcpy(string, temp, l1);
  memcpy(string + l1, stuff, length);
  temp -= sizeof(int);

  if (--((int *)temp)[0] == 0) delete[] temp;
}

void JString::setString(const char *stuff) {
  /**************************************
   *
   *		s e t S t r i n g
   *
   **************************************
   *
   * Functional description
   *		Append string.
   *
   **************************************/

  if (stuff)
    setString(stuff, (int)strlen(stuff));
  else
    release();
}

void JString::Format(const char *stuff, ...) {
  /**************************************
   *
   *		f o r m a t
   *
   **************************************
   *
   * Functional description
   *		Append string.
   *
   **************************************/
  va_list args;
  va_start(args, stuff);
  char temp[1024];

  // Error::validateHeap ("JString::Format 1");
  // int length =
  vsnprintf(temp, sizeof(temp) - 1, stuff, args);
  setString(temp);
  // Error::validateHeap ("JString::Format 2");
}

JString &JString::operator=(const char *stuff) {
  /**************************************
   *
   *		o p e r a t o r   c h a r =
   *
   **************************************
   *
   * Functional description
   *		Return string as string.
   *
   **************************************/

  setString(stuff);

  return *this;
}

JString &JString::operator=(const JString &source) {
  /**************************************
   *
   *		o p e r a t o r   c h a r =
   *
   **************************************
   *
   * Functional description
   *		Return string as string.
   *
   **************************************/

  release();

  if ((string = source.string)) ++((int *)string)[-1];

  return *this;
}

JString &JString::operator+=(const char *stuff) {
  /**************************************
   *
   *		o p e r a t o r   c h a r + =
   *
   **************************************
   *
   * Functional description
   *		Return string as string.
   *
   **************************************/

  append(stuff);

  return *this;
}

JString &JString::operator+=(char c) {
  /**************************************
   *
   *		o p e r a t o r   c h a r + =
   *
   **************************************
   *
   * Functional description
   *		Return string as string.
   *
   **************************************/

  append((const char *)&c, sizeof(c));

  return *this;
}

JString &JString::operator+=(const JString &stuff) {
  /**************************************
   *
   *		o p e r a t o r   c h a r + =
   *
   **************************************
   *
   * Functional description
   *		Return string as string.
   *
   **************************************/

  append(stuff.string);

  return *this;
}

JString operator+(const JString &string1, const char *string2) {
  /**************************************
   *
   *		o p e r a t o r   c h a r +
   *
   **************************************
   *
   * Functional description
   *		Return string as string.
   *
   **************************************/

  JString s = string1;
  s.append(string2);

  return s;
}

void JString::release() {
  /**************************************
   *
   *		r e l e a s e
   *
   **************************************
   *
   * Functional description
   *		Clean out string.
   *
   **************************************/

  // Error::validateHeap ("JString::release");

  if (!string) return;

  if (isStaticString) {
    string = NULL;
    isStaticString = false;
    return;
  }

  string -= sizeof(int);

  if (--((int *)string)[0] == 0) delete[] string;

  string = NULL;
}

bool JString::operator==(const char *stuff) {
  if (string) return strcmp(string, stuff) == 0;

  return strcmp("", stuff) == 0;
}

bool JString::operator==(const WCString *stuff) {
  if (!string) return stuff->count == 0;

  for (int n = 0; n < stuff->count; ++n)
    if (stuff->string[n] != string[n]) return 0;

  return string[stuff->count] == 0;
}

bool JString::operator!=(const char *stuff) {
  if (string) return strcmp(string, stuff) != 0;

  return strcmp("", stuff) != 0;
}

JString JString::before(char c) {
  const char *p;

  for (p = string; *p && *p != c;) ++p;

  if (!*p) return *this;

  JString stuff;
  stuff.setString(string, (int)(p - string));

  return stuff;
}

const char *JString::after(char c) {
  const char *p;

  for (p = string; *p && *p++ != c;)
    ;

  return p;
}

bool JString::IsEmpty() { return !string || !string[0]; }

int JString::hash(const char *string, int tableSize) {
  int value = 0, c;

  while ((c = (unsigned)*string++)) {
    if (ISLOWER(c)) c -= 'a' - 'A';

    value = value * 11 + c;
  }

  if (value < 0) value = -value;

  return value % tableSize;
}

int JString::hash(const WCString *string, int tableSize) {
  int value = 0, c;

  for (const unsigned short *p = string->string, *end = p + string->count;
       p < end; ++p) {
    c = *p;

    if (ISLOWER(c)) c -= 'a' - 'A';

    value = value * 11 + c;
  }

  if (value < 0) value = -value;

  return value % tableSize;
}

int JString::hash(int tableSize) {
  if (!string) return 0;

  return hash(string, tableSize);
}

void JString::setString(const char *source, int length) {
  alloc(length);
  memcpy(string, source, length);
  isStaticString = false;
}

void JString::setStringStatic(const char *source) {
  string = const_cast<char *>(source);
  isStaticString = true;
}

int JString::findSubstring(const char *string, const char *sub) {
  for (const char *p = string; *p; ++p) {
    const char *s, *q;

    for (q = p, s = sub; *s && *q == *s; ++s, ++q)
      ;

    if (!*s) return (int)(p - string);
  }

  return -1;
}

JString JString::upcase(const char *source) {
  JString string;
  int len = (int)strlen(source);
  string.alloc(len);

  for (int n = 0; n < len; ++n) {
    char c = source[n];
    string.string[n] = UPPER(c);
  }

  return string;
}

JString JString::upcase(const WCString *source) {
  JString string;
  int len = source->count;
  string.alloc(len);

  for (int n = 0; n < len; ++n) {
    char c = (char)source->string[n];
    string.string[n] = UPPER(c);
  }

  return string;
}

void JString::alloc(int length) {
  release();
  string = new char[length + 1 + sizeof(int)];
  *((int *)string) = 1;
  string += sizeof(int);
  string[length] = 0;
}

bool JString::equalsNoCase(const char *string2) {
  if (!string) return string2[0] == 0;

  const char *p;

  for (p = string; *p && *string2; ++p, ++string2)
    if (UPPER(*p) != UPPER(*string2)) return false;

  return *p == *string2;
}

bool JString::equalsNoCase(const WCString *string2) {
  const char *p = string;
  const unsigned short *q = string2->string;
  const unsigned short *end = q + string2->count;

  for (; q < end; ++p, ++q)
    if (UPPER(*p) != UPPER(*q)) return false;

  return *p == 0;
}

int JString::length() {
  if (!string) return 0;

  const char *p;

  for (p = string; *p; ++p)
    ;

  return (int)(p - string);
}

JString::JString(const char *source, int length) {
  string = NULL;
  setString(source, length);
}

char *JString::getBuffer(int length) {
  alloc(length);

  return string;
}

void JString::releaseBuffer() {}

JString::JString(const WCString *source) {
  string = NULL;

  if (source) setString(source);
}

void JString::setString(const WCString *source) {
  int length = Unicode::getUtf8Length(source->count, source->string);
  alloc(length);
  Unicode::convert(source->count, (const unsigned short *)source->string,
                   string);
}

}  // namespace Changjiang
