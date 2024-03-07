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

// StreamLogFile.h: interface for the StreamLogFile class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Mutex.h"

namespace Changjiang {

class Database;
struct StreamLogBlock;

class StreamLogFile {
 public:
  void dropDatabase();
  void zap();
  uint32 read(int64 position, uint32 length, UCHAR *data);
  void write(int64 position, uint32 length, const StreamLogBlock *data);
  void close();
  void open(JString filename, bool creat);
  void truncate(int64 size);
  int64 size(void);
  StreamLogFile(Database *db);
  virtual ~StreamLogFile();

  int64 offset;
  int64 highWater;
  int64 writePoint;
  JString fileName;
  uint32 sectorSize;
  Mutex syncObject;
  Database *database;
  bool forceFsync;
  bool created;

#ifdef _WIN32
  void *handle;
#else
  int handle;
#endif
};

}  // namespace Changjiang
