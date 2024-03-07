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

// ScanDir.h: interface for the ScanDir class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#ifdef _WIN32
#ifndef _WINDOWS_
#undef ERROR
#include <windows.h>
#endif
#else
#include <dirent.h>
#endif

namespace Changjiang {

class ScanDir {
 public:
  bool isDots();
  const char *getFilePath();
  bool isDirectory();
  bool match(const char *pattern, const char *name);
  const char *getFileName();
  bool next();
  ScanDir(const char *dir, const char *pattern);
  virtual ~ScanDir();

  JString directory;
  JString pattern;
  JString fileName;
  JString filePath;
#ifdef _WIN32
  WIN32_FIND_DATA data;
  HANDLE handle;
#else
  DIR *dir;
  dirent *data;
#endif
};

}  // namespace Changjiang
