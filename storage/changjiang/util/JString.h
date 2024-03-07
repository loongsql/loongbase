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

/**********************************************************************
 *	PROGRAM:		Virtual Data Manager
 *	MODULE:			JString.h
 *	DESCRIPTION:	Transportable flexible string
 *
 * copyright (c) 1997 - 2000 by James A. Starkey

JString is a relatively simple, portable, lightweight string class
modeled on the Microsoft MFC CString.

A JString, cut to essentials, is a managed pointer to a null terminated
string.  The four bytes immediately preceding the string are a use count
which is automatically incremented when the JString is copied to another
JString and decremented when a JString value is changed or its destructor
is called.  When the use count goes to zero the actual memory is deleted.
The use count makes assignment of one JString to another a very cheap
operation.  It also makes a return type of JString fast and safe.

        JString function ()
                {
                char buffer[bigEngough];
                sprintf (buffer, "%x is an address", buffer);
                return buffer;  // automatically converted to a JString before
returning
                }

Assignment to a JString from either a null terminated string or another
JString is trivial: perform the assignment.  The JString assignment operator
for a character string will compute the length of the string plus the
terminating type plus the count word, allocate that amount of memory, set
the use count to one, and copy the string.  The assignment operator for
a JString, however, merely copies the pointer and increments the use count.

        JString fred = "This is a quoted string";
        JString marth = fred;
        fred = "something else";      // value is changed.

For more exotic purposes, there is a JString::Format method that performs
an internal vsnprintf call to format the string, then assigns the resulting
formatted string to itself.

        JString jstring;
        jstring.Format ("%s/%d", directory, filename);

JString also has a "const char*" cast operator, so it can be used in any
context that expects a const string pointer.  In a variable length argument
list like printf, there must be a formal cast (const char*) to force the
use of the cast operator.

        printf("%s is %d bytes long\n", (const char*) jstring, strlen(jstring));

There are also concatenation operators:

        JString path = directory;
        path += "/";
        path += "file.name";

The concatenation operator isn't magic -- the method allocates a new
string of the right length, copies both operands, set the new use count,
a decrements the use count of the old string.

JStrings can also be compared for string (not address) equality with the
comparison operator to either other JString's  or character strings.

JString also has methods for up and down casing and case insensitive
comparisons, but these are limited to Ascii and probably shouldn't be used.

Finally, JString has a fat brother WString for 16 bit Unicode strings
with approximate the same semantics.  WString is really intended for
use with the Netfrastructure Java Virtual Machine and *should not* be
used for general purpose Unicode, particularly since wide characters
on Linux are 32, not 16, bits wide.
**********************************************************************/

#pragma once

namespace Changjiang {

#define ALLOC_FUDGE 100

struct WCString {
  int count;
  const unsigned short *string;
};

class JString {
 public:
  static JString upcase(const WCString *source);
  static int hash(const WCString *string, int tableSize);
  bool equalsNoCase(const WCString *string2);
  void setString(const WCString *source);
  void releaseBuffer();
  char *getBuffer(int length);
  int length();
  bool equalsNoCase(const char *string2);
  static JString upcase(const char *source);
  static int findSubstring(const char *string, const char *sub);
  int hash(int tableSize);
  static int hash(const char *string, int tableSize);
  bool IsEmpty();
  const char *after(char c);
  JString before(char c);

  bool operator==(const char *string);
  bool operator==(const WCString *stuff);
  bool operator!=(const char *stuff);

  JString();
  JString(const char *source, int length);
  JString(const char *string);
  JString(const JString &stringSrc);
  JString(const WCString *source);
  ~JString();

  void append(const char *);
  void append(const char *, int length);
  void setString(const char *);
  void setString(const char *source, int length);
  void setStringStatic(const char *source);
  void Format(const char *, ...);

  inline const char *getString() { return (string) ? string : ""; }

  inline operator const char *() { return (string) ? string : ""; }

  JString &operator=(const char *string);
  JString &operator=(const JString &string);
  JString &operator+=(char c);
  JString &operator+=(const char *string);
  JString &operator+=(const JString &string);

  friend JString operator+(const JString &string1, const char *string2);

 protected:
  void alloc(int length);
  void release();

  char *string;
  bool isStaticString;
};

}  // namespace Changjiang
