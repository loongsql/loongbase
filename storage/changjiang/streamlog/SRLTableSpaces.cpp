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

#include "Engine.h"
#include "SRLTableSpaces.h"
#include "TableSpaceManager.h"
#include "TableSpace.h"
#include "Database.h"

namespace Changjiang {

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

static TableSpaceManager *tableSpaceManager;

SRLTableSpaces::SRLTableSpaces() { tableSpaceManager = NULL; }

SRLTableSpaces::~SRLTableSpaces() {}

TableSpaceManager *SRLTableSpaces::getTableSpaceManager() {
  return tableSpaceManager;
}

// Serialize TableSpaceManager to the the stream log
void SRLTableSpaces::append(TableSpaceManager *manager) {
  START_RECORD(srlTableSpaces, "SRLTableSpaces::append");

  int count = 0;
  for (TableSpace *tableSpace = manager->tableSpaces; tableSpace;
       tableSpace = tableSpace->next)
    count++;

  putInt(count);

  for (TableSpace *tableSpace = manager->tableSpaces; tableSpace;
       tableSpace = tableSpace->next) {
    putInt(tableSpace->tableSpaceId);
    putInt(tableSpace->type);

    int len = tableSpace->name.length() + 1;
    putInt(len);
    putData(len, (const UCHAR *)tableSpace->name.getString());

    len = tableSpace->filename.length() + 1;
    putInt(len);
    putData(len, (const UCHAR *)tableSpace->filename.getString());
  }

  log->flush(false, log->nextBlockNumber, &sync);
}

// Deserialize TableSpaceManager from StreamLog and store
// it in recoveryTablespaceManager
void SRLTableSpaces::read() {
  int count = getInt();

  if (log->recoveryPhase == 1) {
    delete tableSpaceManager;
    tableSpaceManager = new TableSpaceManager(log->database);
  }

  for (int i = 0; i < count; i++) {
    int id = getInt();
    int type = getInt();
    int len = getInt();
    char *name = (char *)getData(len);
    len = getInt();
    char *filename = (char *)getData(len);
    if (log->recoveryPhase == 1)
      tableSpaceManager->add(
          new TableSpace(log->database, name, id, filename, type, NULL));
  }
}

void SRLTableSpaces::print() {
  logPrint("SRLTableSpaces\n");
  TableSpace *tableSpace;
  for (tableSpace = tableSpaceManager->tableSpaces; tableSpace;
       tableSpace = tableSpace->next) {
    logPrint("name = %s, id = %d, filename = %s\n",
             tableSpace->name.getString(), tableSpace->tableSpaceId,
             tableSpace->filename.getString());
  }
}

}  // namespace Changjiang
