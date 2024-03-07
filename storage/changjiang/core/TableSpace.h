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

// TableSpace.h: interface for the TableSpace class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

namespace Changjiang {

static const int TABLESPACE_TYPE_TABLESPACE = 0;
static const int TABLESPACE_TYPE_REPOSITORY = 1;

struct TableSpaceInit {
  JString comment;
  TableSpaceInit() : comment("") {}
};

class Dbb;
class Database;
class InfoTable;

class TableSpace {
 public:
  TableSpace(Database *database, const char *spaceName, int spaceId,
             const char *spaceFilename, int tsType, TableSpaceInit *tsInit);
  virtual ~TableSpace();

  void create();
  void open();
  void close(void);
  void shutdown(TransId transId);
  void dropTableSpace(void);
  bool fileNameEqual(const char *file);
  void sync(void);
  void save(void);
  void getIOInfo(InfoTable *infoTable);

  JString name;
  JString filename;
  TableSpace *nameCollision;
  TableSpace *idCollision;
  TableSpace *next;
  Dbb *dbb;
  Database *database;

  int tableSpaceId;
  int type;
  bool active;
  bool needSave;

  JString comment;
};

}  // namespace Changjiang
