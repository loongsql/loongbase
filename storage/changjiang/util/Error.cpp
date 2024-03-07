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

// Error.cpp: implementation of the Error class.
//
//////////////////////////////////////////////////////////////////////

// copyright (c) 1999 - 2000 by James A. Starkey

#ifdef _WIN32
#include <windows.h>
#endif

#include <stdarg.h>
#include <stdio.h>
#include <signal.h>
#include "Engine.h"
#include "Error.h"
#include "SQLError.h"
#include "Log.h"
//#include "MemMgr.h"

//#define CHECK_HEAP

#ifdef CHECK_HEAP
#include <crtdbg.h>
#endif

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

void Error::error(const char *string, ...) {
  char buffer[256];
  va_list args;
  va_start(args, string);

  if (vsnprintf(buffer, sizeof(buffer) - 1, string, args) < 0)
    buffer[sizeof(buffer) - 1] = 0;

#ifdef CHANGJIANGDB

  // Always write unrecoverable error info to the error log

  fprintf(stderr, "[Changjiang] Error: %s\n", buffer);
  Log::logBreak("Bugcheck: %s\n", buffer);
  // MemMgrLogDump();
#endif

  debugBreak();

  throw SQLEXCEPTION(BUG_CHECK, buffer);
}

void Error::assertionFailed(const char *text, const char *fileName, int line) {
  error("assertion (%s) failed at line %d in file %s\n", text, line, fileName);
}

void Error::validateHeap(const char *where) {
#ifdef CHECK_HEAP
  if (!_CrtCheckMemory()) Log::debug("***> memory corrupted at %s!!!\n", where);
#endif
}

void Error::debugBreak() {
#ifdef _WIN32
  __debugbreak();
#endif
  raise(SIGABRT);
}

void Error::notYetImplemented(const char *fileName, int line) {
#ifdef CHANGJIANGDB
  Log::logBreak("feature not yet implemented at line %d in file %s\n", line,
                fileName);
#endif

  throw SQLEXCEPTION(FEATURE_NOT_YET_IMPLEMENTED,
                     "feature not yet implemented at line %d in file %s\n",
                     line, fileName);
}

}  // namespace Changjiang
