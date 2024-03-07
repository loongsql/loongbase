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

// TableSpaceManager.h: interface for the TableSpaceManager class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "SyncObject.h"

namespace Changjiang {

static const int TS_HASH_SIZE = 101;

struct TableSpaceInit;
class TableSpace;
class Database;
class Transaction;
class InfoTable;

class TableSpaceManager {
 public:
  TableSpaceManager(Database *db);
  virtual ~TableSpaceManager();

  TableSpace *getTableSpace(int id);
  TableSpace *findTableSpace(int id);
  void bootstrap(int sectionId);
  TableSpace *createTableSpace(const char *name, const char *fileName,
                               bool repository, TableSpaceInit *tsInit);
  TableSpace *getTableSpace(const char *name);
  TableSpace *findTableSpace(const char *name);
  void add(TableSpace *tableSpace);
  void shutdown(TransId transId);
  void dropDatabase(void);
  void dropTableSpace(TableSpace *tableSpace);
  void reportStatistics(void);
  JString tableSpaceType(JString name);
  void getIOInfo(InfoTable *infoTable);
  void getTableSpaceInfo(InfoTable *infoTable);
  JString tableSpaceFileType(JString name);
  void getTableSpaceFilesInfo(InfoTable *infoTable);
  void validate(int optionMask);
  void sync();
  void expungeTableSpace(int tableSpaceId);
  void reportWrites(void);
  void redoCreateTableSpace(int id, int nameLength, const char *name,
                            int fileNameLength, const char *fileName, int type,
                            TableSpaceInit *tsInit);
  void initialize(void);
  int createTableSpaceId();
  void openTableSpaces();

  Database *database;
  TableSpace *tableSpaces;
  TableSpace *nameHash[TS_HASH_SIZE];
  TableSpace *idHash[TS_HASH_SIZE];
  SyncObject syncObject;
  void postRecovery(void);
};

}  // namespace Changjiang
