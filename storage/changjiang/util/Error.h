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

// Error.h: interface for the Error class.
//
//////////////////////////////////////////////////////////////////////

// copyright (c) 1999 - 2000 by James A. Starkey

#pragma once

namespace Changjiang {

#undef ERROR
#undef ASSERT
#define FATAL Error::error
#define ASSERT(f) \
  while (!(f)) Error::assertionFailed(#f, __FILE__, __LINE__)
#define NOT_YET_IMPLEMENTED Error::notYetImplemented(__FILE__, __LINE__)

class Error {
 public:
  static void notYetImplemented(const char *fileName, int line);
  static void debugBreak();
  static void validateHeap(const char *where);
  static void assertionFailed(const char *text, const char *fileName, int line);
  static void error(const char *text, ...);
};

}  // namespace Changjiang
