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

// ScanDir.cpp: implementation of the ScanDir class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <string.h>

#ifndef _WIN32
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "Engine.h"
#include "ScanDir.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

ScanDir::ScanDir(const char *direct, const char *pat) {
  directory = direct;
  pattern = pat;
#ifdef _WIN32
  handle = NULL;
#else
  dir = opendir(direct);
  data = NULL;
#endif
}

ScanDir::~ScanDir() {
#ifdef _WIN32
  if (handle) FindClose(handle);
#else
  if (dir) closedir(dir);
#endif
}

bool ScanDir::next() {
#ifdef _WIN32
  if (handle == NULL) {
    handle = FindFirstFile(directory + "\\" + pattern, &data);
    return handle != INVALID_HANDLE_VALUE;
  }

  return FindNextFile(handle, &data) != 0;
#else
  if (!dir) return false;

  while ((data = readdir(dir))) {
    if (match(pattern, data->d_name)) return true;
  }

  return false;
#endif
}

const char *ScanDir::getFileName() {
#ifdef _WIN32
  fileName = data.cFileName;
#else
  fileName = data->d_name;
#endif

  return fileName;
}

const char *ScanDir::getFilePath() {
#ifdef _WIN32
  filePath = directory + "\\" + data.cFileName;
#else
  filePath = directory + "/" + data->d_name;
#endif

  return filePath;
}

bool ScanDir::match(const char *pattern, const char *name) {
  if (*pattern == '*') {
    if (!pattern[1]) return true;
    for (const char *p = name; *p; ++p)
      if (match(pattern + 1, p)) return true;
    return false;
  }

  if (*pattern != *name) return false;

  if (!*pattern) return true;

  return match(pattern + 1, name + 1);
}

bool ScanDir::isDirectory() {
#ifdef _WIN32
  return (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
  if (!data) return false;

#ifdef DT_DIR
  if (data->d_type == DT_DIR) return true;
#endif

  struct stat buf;

  if (stat(getFilePath(), &buf)) return false;

  return S_ISDIR(buf.st_mode);
#endif
}

bool ScanDir::isDots() { return getFileName()[0] == '.'; }

}  // namespace Changjiang
