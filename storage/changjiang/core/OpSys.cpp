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

// OpSys.cpp: implementation of the OpSys class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _WIN32
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

#include "Engine.h"
#include "OpSys.h"
#include "Log.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

OpSys::OpSys() {}

OpSys::~OpSys() {}

void OpSys::logParameters() {
#ifdef RUSAGE_SELF
  rusage usage;
  rlimit size;

  if (getrusage(RUSAGE_SELF, &usage) == 0)
    Log::log("minflt %d, maxflt %d, swp %d\n", usage.ru_minflt, usage.ru_majflt,
             usage.ru_nswap);

#ifdef RLIMIT_AS

  if (getrlimit(RLIMIT_AS, &size) == 0 && size.rlim_cur != (rlim_t)-1)
    Log::log("size %d/%d\n", size.rlim_cur, size.rlim_max);

#endif
#endif
}

}  // namespace Changjiang
