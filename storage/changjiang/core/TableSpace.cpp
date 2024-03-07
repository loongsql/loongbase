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

// TableSpace.cpp: implementation of the TableSpace class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "TableSpace.h"
#include "Dbb.h"
#include "Database.h"
#include "SQLError.h"
#include "Hdr.h"
#include "Cache.h"
#include "PStatement.h"
#include "InfoTable.h"

namespace Changjiang {

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

TableSpace::TableSpace(Database *db, const char *spaceName, int spaceId,
                       const char *spaceFilename, int tsType,
                       TableSpaceInit *tsInit) {
  database = db;
  name = spaceName;
  tableSpaceId = spaceId;
  filename = spaceFilename;
  dbb = new Dbb(database->dbb, tableSpaceId);
  active = false;
  needSave = false;
  type = tsType;

  TableSpaceInit spaceInit;
  TableSpaceInit *init = (tsInit ? tsInit : &spaceInit);
  comment = init->comment;
}

TableSpace::~TableSpace() {
  dbb->close();
  delete dbb;
}

void TableSpace::open() {
  try {
    dbb->openFile(filename, false);
    active = true;
  } catch (SQLException &) {
    // if (dbb->doesFileExits(fileName))
    throw;

    create();

    return;
  }

  Hdr header;

  try {
    dbb->readHeader(&header);

    switch (type) {
      case TABLESPACE_TYPE_TABLESPACE:
        if (header.fileType != HdrTableSpace)
          throw SQLError(RUNTIME_ERROR,
                         "table space file \"%s\" has wrong page type (expeced "
                         "%d, got %d)\n",
                         (const char *)filename, HdrTableSpace,
                         header.fileType);
        break;

      case TABLESPACE_TYPE_REPOSITORY:
        if (header.fileType != HdrRepositoryFile)
          throw SQLError(RUNTIME_ERROR,
                         "table space file \"%s\" has wrong page type (expeced "
                         "%d, got %d)\n",
                         (const char *)filename, HdrRepositoryFile,
                         header.fileType);
        break;

      default:
        NOT_YET_IMPLEMENTED;
    }

    if (header.pageSize != dbb->pageSize)
      throw SQLError(
          RUNTIME_ERROR,
          "table space file \"%s\" has wrong page size (expeced %d, got %d)\n",
          (const char *)filename, dbb->pageSize, header.pageSize);

    dbb->initRepository(&header);
  } catch (...) {
    /***
    isOpen = false;
    isWritable = false;
    JString name = getName();
    ***/
    dbb->closeFile();
    throw;
  }
}

void TableSpace::create() {
#ifndef CHANGJIANGDB
  dbb->createPath(filename);
#endif
  dbb->create(filename, dbb->pageSize, 0, HdrTableSpace, 0, NULL);
  active = true;
  dbb->flush();
}

void TableSpace::shutdown(TransId transId) { dbb->shutdown(transId); }

void TableSpace::dropTableSpace(void) { dbb->dropDatabase(); }

bool TableSpace::fileNameEqual(const char *file) {
  // char expandedName[1024];
  // IO::expandFileName(file, sizeof(expandedName), expandedName);

  return filename == file;
}

void TableSpace::sync(void) { database->cache->syncFile(dbb, "sync"); }

void TableSpace::save(void) {
  PStatement statement = database->prepareStatement(
      "replace into system.tablespaces (tablespace, tablespace_id, filename, "
      "type, comment) values (?,?,?,?,?)");
  int n = 1;
  statement->setString(n++, name);
  statement->setInt(n++, tableSpaceId);
  statement->setString(n++, filename);
  statement->setInt(n++, type);
  statement->setString(n++, comment);
  statement->executeUpdate();
  needSave = false;
}

void TableSpace::getIOInfo(InfoTable *infoTable) {
  int n = 0;
  infoTable->putString(n++, name);
  infoTable->putInt(n++, dbb->pageSize);
  infoTable->putInt(n++, dbb->cache->numberBuffers);
  infoTable->putInt(n++, dbb->reads);
  infoTable->putInt(n++, dbb->writes);
  infoTable->putInt(n++, dbb->fetches);
  infoTable->putInt(n++, dbb->fakes);
  infoTable->putRecord();
}

void TableSpace::close(void) {
  dbb->close();
  active = false;
}

}  // namespace Changjiang
