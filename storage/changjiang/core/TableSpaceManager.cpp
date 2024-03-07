/* Copyright � 2007-2008 MySQL AB, 2008-2009 Sun Microsystems, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

// TableSpaceManager.cpp: implementation of the TableSpaceManager class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include "Engine.h"
#include "TableSpaceManager.h"
#include "TableSpace.h"
#include "Sync.h"
#include "SQLError.h"
#include "PStatement.h"
#include "RSet.h"
#include "Database.h"
#include "Connection.h"
#include "SequenceManager.h"
#include "Sequence.h"
#include "Stream.h"
#include "EncodedDataStream.h"
#include "Value.h"
#include "Section.h"
#include "Dbb.h"
#include "StreamLog.h"
#include "StreamLogControl.h"
#include "SRLCreateTableSpace.h"
#include "SRLDropTableSpace.h"
#include "Log.h"
#include "InfoTable.h"
#include "Thread.h"

namespace Changjiang {

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

TableSpaceManager::TableSpaceManager(Database *db) {
  database = db;
  memset(nameHash, 0, sizeof(nameHash));
  memset(idHash, 0, sizeof(nameHash));
  tableSpaces = NULL;
  syncObject.setName("TableSpaceManager::syncObject");
}

TableSpaceManager::~TableSpaceManager() {
  for (TableSpace *tableSpace; (tableSpace = tableSpaces);) {
    tableSpaces = tableSpace->next;
    delete tableSpace;
  }
}

void TableSpaceManager::add(TableSpace *tableSpace) {
  Sync sync(&syncObject, "TableSpaceManager::add");
  sync.lock(Exclusive);
  int slot = tableSpace->name.hash(TS_HASH_SIZE);
  tableSpace->nameCollision = nameHash[slot];
  nameHash[slot] = tableSpace;
  slot = tableSpace->tableSpaceId % TS_HASH_SIZE;
  tableSpace->idCollision = idHash[slot];
  idHash[slot] = tableSpace;
  tableSpace->next = tableSpaces;
  tableSpaces = tableSpace;

  if (database->streamLog && !database->streamLog->recovering)
    database->streamLog->logControl->tableSpaces.append(this);
}

TableSpace *TableSpaceManager::findTableSpace(const char *name) {
  Sync syncDDL(&database->syncSysDDL, "TableSpaceManager::findTableSpace(DDL)");
  syncDDL.lock(Shared);

  Sync syncObj(&syncObject, "TableSpaceManager::findTableSpace(syncObj)");
  syncObj.lock(Shared);
  TableSpace *tableSpace;

  for (tableSpace = nameHash[JString::hash(name, TS_HASH_SIZE)]; tableSpace;
       tableSpace = tableSpace->nameCollision)
    if (tableSpace->name == name) return tableSpace;

  syncObj.unlock();
  syncObj.lock(Exclusive);

  for (tableSpace = nameHash[JString::hash(name, TS_HASH_SIZE)]; tableSpace;
       tableSpace = tableSpace->nameCollision)
    if (tableSpace->name == name) return tableSpace;

  PStatement statement = database->prepareStatement(
      "select tablespace_id, filename, comment from system.tablespaces where "
      "tablespace=?");
  statement->setString(1, name);
  RSet resultSet = statement->executeQuery();

  if (resultSet->next()) {
    int n = 1;
    int id = resultSet->getInt(n++);                   // tablespace id
    const char *fileName = resultSet->getString(n++);  // filename
    int type = TABLESPACE_TYPE_TABLESPACE;             // type (forced)

    TableSpaceInit tsInit;
    tsInit.comment = resultSet->getString(n++);  // comment

    tableSpace = new TableSpace(database, name, id, fileName, type, &tsInit);

    if (type != TABLESPACE_TYPE_REPOSITORY) try {
        tableSpace->open();
      } catch (...) {
        delete tableSpace;

        throw;
      }

    add(tableSpace);
  }

  return tableSpace;
}

TableSpace *TableSpaceManager::getTableSpace(const char *name) {
  TableSpace *tableSpace = findTableSpace(name);

  if (!tableSpace)
    throw SQLError(TABLESPACE_NOT_EXIST_ERROR, "can't find table space \"%s\"",
                   name);

  if (!tableSpace->active) tableSpace->open();

  return tableSpace;
}

TableSpace *TableSpaceManager::createTableSpace(const char *name,
                                                const char *fileName,
                                                bool repository,
                                                TableSpaceInit *tsInit) {
  Sync syncDDL(&database->syncSysDDL, "TableSpaceManager::createTableSpace");
  syncDDL.lock(Exclusive);

  int type =
      (repository) ? TABLESPACE_TYPE_REPOSITORY : TABLESPACE_TYPE_TABLESPACE;
  int id = createTableSpaceId();

  TableSpace *tableSpace =
      new TableSpace(database, name, id, fileName, type, tsInit);

  if (!repository && IO::doesFileExist(fileName)) {
    delete tableSpace;
    throw SQLError(TABLESPACE_DATAFILE_EXIST_ERROR,
                   "table space file name \"%s\" already exists\n", fileName);
  }

  bool createdFile = false;
  try {
    tableSpace->save();

    if (!repository) tableSpace->create();

    createdFile = true;
    add(tableSpace);
    database->streamLog->logControl->createTableSpace.append(tableSpace);
  } catch (...) {
    if (createdFile) IO::deleteFile(fileName);

    database->rollbackSystemTransaction();
    delete tableSpace;

    throw;
  }

  database->commitSystemTransaction();

  return tableSpace;
}

void TableSpaceManager::bootstrap(int sectionId) {
  Dbb *dbb = database->dbb;
  Section *section = dbb->findSection(sectionId);
  Stream stream;
  UCHAR buffer[1024];

  for (int n = 0; (n = dbb->findNextRecord(section, n, &stream)) >= 0; ++n) {
    stream.getSegment(0, sizeof(buffer), buffer);
    const UCHAR *p = buffer + 2;
    Value name, id, fileName, type;

    p = EncodedDataStream::decode(p, &name, true);
    p = EncodedDataStream::decode(p, &id, true);
    p = EncodedDataStream::decode(p, &fileName, true);
    p = EncodedDataStream::decode(p, &type, true);

    TableSpace *tableSpace =
        new TableSpace(database, name.getString(), id.getInt(),
                       fileName.getString(), type.getInt(), NULL);
    add(tableSpace);

    Log::debug(
        "TableSpaceManager::bootstrap() : table space %s, id %d, type %d, "
        "filename %s\n",
        (const char *)tableSpace->name, tableSpace->tableSpaceId,
        tableSpace->type, (const char *)tableSpace->filename);
    stream.clear();
  }
}

TableSpace *TableSpaceManager::findTableSpace(int tableSpaceId) {
  for (TableSpace *tableSpace = idHash[tableSpaceId % TS_HASH_SIZE]; tableSpace;
       tableSpace = tableSpace->idCollision)
    if (tableSpace->tableSpaceId == tableSpaceId) return tableSpace;

  return NULL;
}

TableSpace *TableSpaceManager::getTableSpace(int id) {
  TableSpace *tableSpace = findTableSpace(id);

  if (!tableSpace)
    throw SQLError(TABLESPACE_NOT_EXIST_ERROR, "can't find table space %d", id);

  if (!tableSpace->active) tableSpace->open();

  return tableSpace;
}

void TableSpaceManager::shutdown(TransId transId) {
  for (TableSpace *tableSpace = tableSpaces; tableSpace;
       tableSpace = tableSpace->next)
    tableSpace->shutdown(transId);
}

void TableSpaceManager::dropDatabase(void) {
  for (TableSpace *tableSpace = tableSpaces; tableSpace;
       tableSpace = tableSpace->next)
    tableSpace->dropTableSpace();
}

void TableSpaceManager::dropTableSpace(TableSpace *tableSpace) {
  Sync syncDDL(&database->syncSysDDL, "TableSpaceManager::dropTableSpace(DDL)");
  syncDDL.lock(Exclusive);

  Sync syncObj(&syncObject, "TableSpaceManager::dropTableSpace(syncObj)");
  syncObj.lock(Exclusive);

  PStatement statement = database->prepareStatement(
      "delete from system.tablespaces where tablespace=?");
  statement->setString(1, tableSpace->name);
  statement->executeUpdate();
  Transaction *transaction = database->getSystemTransaction();
  transaction->hasUpdates = true;

  syncDDL.unlock();

  int slot = tableSpace->name.hash(TS_HASH_SIZE);

  for (TableSpace **ptr = nameHash + slot; *ptr; ptr = &(*ptr)->nameCollision)
    if (*ptr == tableSpace) {
      *ptr = tableSpace->nameCollision;

      break;
    }

  syncObj.unlock();
  tableSpace->active = false;
  JString filename = tableSpace->dbb->fileName;

  database->streamLog->logControl->dropTableSpace.append(tableSpace,
                                                         transaction);
  database->commitSystemTransaction();
  IO::deleteFile(filename);
}

void TableSpaceManager::reportStatistics(void) {
  Sync sync(&syncObject, "TableSpaceManager::reportStatistics");
  sync.lock(Shared);

  for (TableSpace *tableSpace = tableSpaces; tableSpace;
       tableSpace = tableSpace->next)
    tableSpace->dbb->reportStatistics();
}

void TableSpaceManager::validate(int optionMask) {
  Sync sync(&syncObject, "TableSpaceManager::validate");
  sync.lock(Shared);

  for (TableSpace *tableSpace = tableSpaces; tableSpace;
       tableSpace = tableSpace->next)
    tableSpace->dbb->validate(optionMask);
}

void TableSpaceManager::sync() {
  Sync sync(&syncObject, "TableSpaceManager::sync");
  sync.lock(Shared);

  for (TableSpace *tableSpace = tableSpaces; tableSpace;
       tableSpace = tableSpace->next)
    tableSpace->sync();
}

void TableSpaceManager::expungeTableSpace(int tableSpaceId) {
  Sync sync(&syncObject, "TableSpaceManager::expungeTableSpace");
  sync.lock(Exclusive);
  TableSpace *tableSpace = findTableSpace(tableSpaceId);

  if (!tableSpace) return;

  TableSpace **ptr;
  int slot = tableSpace->name.hash(TS_HASH_SIZE);

  for (ptr = nameHash + slot; *ptr; ptr = &(*ptr)->nameCollision)
    if (*ptr == tableSpace) {
      *ptr = tableSpace->nameCollision;
      break;
    }

  slot = tableSpace->tableSpaceId % TS_HASH_SIZE;

  for (ptr = idHash + slot; *ptr; ptr = &(*ptr)->idCollision)
    if (*ptr == tableSpace) {
      *ptr = tableSpace->idCollision;
      break;
    }

  for (ptr = &tableSpaces; *ptr; ptr = &(*ptr)->next)
    if (*ptr == tableSpace) {
      *ptr = tableSpace->next;
      break;
    }

  if (database->streamLog)
    database->streamLog->logControl->tableSpaces.append(this);
  sync.unlock();
  // File already deleted, just close the file descriptor
  tableSpace->close();
  delete tableSpace;
}

void TableSpaceManager::reportWrites(void) {
  Sync sync(&syncObject, "TableSpaceManager::reportWrites");
  sync.lock(Shared);

  for (TableSpace *tableSpace = tableSpaces; tableSpace;
       tableSpace = tableSpace->next)
    tableSpace->dbb->reportWrites();
}

void TableSpaceManager::redoCreateTableSpace(int id, int nameLength,
                                             const char *name,
                                             int fileNameLength,
                                             const char *fileName, int type,
                                             TableSpaceInit *tsInit) {
  Sync sync(&syncObject, "TableSpaceManager::redoCreateTableSpace");
  sync.lock(Exclusive);
  TableSpace *tableSpace = NULL;

  for (tableSpace = idHash[id % TS_HASH_SIZE]; tableSpace;
       tableSpace = tableSpace->idCollision)
    if (tableSpace->tableSpaceId == id) {
      tableSpace->close();
      break;
    }

  if (!tableSpace) {
    char buffer[1024];
    memcpy(buffer, name, nameLength);
    buffer[nameLength] = 0;
    char *file = buffer + nameLength + 1;
    memcpy(file, fileName, fileNameLength);
    file[fileNameLength] = 0;
    tableSpace = new TableSpace(database, buffer, id, file, type, tsInit);
    tableSpace->needSave = true;
    add(tableSpace);
  }

  try {
    Dbb *dbb = tableSpace->dbb;

    dbb->create(tableSpace->filename, database->dbb->pageSize, 0, HdrTableSpace,
                NO_TRANSACTION, "", true);
    tableSpace->active = true;

  } catch (SQLException &exception) {
    Log::log(
        "Couldn't open table space file \"%s\" for tablespace \"%s\": %s\n",
        tableSpace->filename.getString(), tableSpace->name.getString(),
        exception.getText());

    // remove from various hashtables
    expungeTableSpace(tableSpace->tableSpaceId);
  }
}

void TableSpaceManager::initialize(void) {
  Sync syncDDL(&database->syncSysDDL, "TableSpaceManager::initialize(DDL)");
  syncDDL.lock(Exclusive);

  Sync syncObj(&syncObject, "TableSpaceManager::initialize(syncObj)");
  syncObj.lock(Shared);

  for (TableSpace *tableSpace = tableSpaces; tableSpace;
       tableSpace = tableSpace->next)
    if (tableSpace->needSave) tableSpace->save();

  database->commitSystemTransaction();
}

void TableSpaceManager::postRecovery(void) {
  Sync sync(&syncObject, "TableSpaceManager::postRecovery");
  sync.lock(Shared);

  for (TableSpace *tableSpace = tableSpaces; tableSpace;
       tableSpace = tableSpace->next)
    if (tableSpace->active && tableSpace->type == TABLESPACE_TYPE_REPOSITORY)
      tableSpace->close();
}

void TableSpaceManager::getIOInfo(InfoTable *infoTable) {
  Sync sync(&syncObject, "TableSpaceManager::getIOInfo");
  sync.lock(Shared);

  for (TableSpace *tableSpace = tableSpaces; tableSpace;
       tableSpace = tableSpace->next)
    tableSpace->getIOInfo(infoTable);
}

JString TableSpaceManager::tableSpaceType(JString name) {
  JString type;

  if (name == "CHANGJIANG_USER")
    type = "DEFAULT";
  else if (name == "CHANGJIANG_TEMPORARY")
    type = "TEMPORARY";
  else if (name == "CHANGJIANG_SYSTEM_BASE")  // cwp tbd: fix this
    type = "MASTER CATALOG";
  else
    type = "USER DEFINED";

  return type;
}

void TableSpaceManager::getTableSpaceInfo(InfoTable *infoTable) {
  Sync syncDDL(&database->syncSysDDL, "TableSpaceManager::getTableSpaceInfo");
  syncDDL.lock(Shared);

  PStatement statement = database->systemConnection->prepareStatement(
      "select tablespace, comment from system.tablespaces");
  RSet resultSet = statement->executeQuery();

  while (resultSet->next()) {
    // TABLESPACE_NAME NOT NULL
    infoTable->putString(0, resultSet->getString(1));
    // ENGINE NOT NULL
    infoTable->putString(1, "Changjiang");
    // TABLESPACE_TYPE (based upon name)
    infoTable->setNotNull(2);
    infoTable->putString(2, tableSpaceType(resultSet->getString(1)));
    // LOGFILE_GROUP_NAME
    infoTable->setNull(3);
    // EXTENT_SIZE
    infoTable->setNull(4);
    // AUTOEXTEND_SIZE
    infoTable->setNull(5);
    // MAXIMUM_SIZE
    infoTable->setNull(6);
    // NODEGROUP_ID
    infoTable->setNull(7);
    // TABLESPACE_COMMENT
    infoTable->setNotNull(8);
    infoTable->putString(8, resultSet->getString(2));
    infoTable->putRecord();
  }
}

JString TableSpaceManager::tableSpaceFileType(JString name) {
  JString type;

  if (name == "CHANGJIANG_USER" || name == "CHANGJIANG_TEMPORARY" ||
      name == "CHANGJIANG_SYSTEM_BASE")
    type = "SYSTEM DATAFILE";
  else
    type = "USER DATAFILE";

  return type;
}

void TableSpaceManager::getTableSpaceFilesInfo(InfoTable *infoTable) {
  Sync syncDDL(&database->syncSysDDL,
               "TableSpaceManager::getTableSpaceFilesInfo");
  syncDDL.lock(Shared);

  PStatement statement = database->systemConnection->prepareStatement(
      "select tablespace, filename from system.tablespaces");
  RSet resultSet = statement->executeQuery();

  while (resultSet->next()) {
    infoTable->putInt(0, 0);   // FILE_ID NOT NULL, unused for now
    infoTable->setNotNull(1);  // FILE_NAME
    infoTable->putString(1, resultSet->getString(2));
    infoTable->putString(
        2, tableSpaceFileType(resultSet->getString(1)));  // FILE_TYPE NOT NULL
    infoTable->setNotNull(3);                             // TABLESPACE_NAME
    infoTable->putString(3, resultSet->getString(1));
    infoTable->putString(4, "def");         // TABLE_CATALOG
    infoTable->setNull(5);                  // TABLE_SCHEMA
    infoTable->setNull(6);                  // TABLE_NAME
    infoTable->setNull(7);                  // LOGFILE_GROUP_NAME
    infoTable->setNull(8);                  // LOGFILE_GROUP_NUMBER
    infoTable->putString(9, "Changjiang");  // ENGINE NOT NULL
    infoTable->setNull(10);                 // FULLTEXT_KEYS
    infoTable->setNull(11);                 // DELETED_ROWS
    infoTable->setNull(12);                 // UPDATE_COUNT
    infoTable->setNull(13);                 // FREE_EXTENTS
    infoTable->setNull(14);                 // TOTAL_EXTENTS
    infoTable->putInt(15, 0);               // EXTENT_SIZE NOT NULL
    infoTable->setNull(16);                 // INITIAL_SIZE
    infoTable->setNull(17);                 // MAXIMUM_SIZE
    infoTable->setNull(18);                 // AUTOEXTEND_SIZE
    infoTable->setNull(19);                 // CREATION_TIME
    infoTable->setNull(20);                 // LAST_UPDATE_TIME
    infoTable->setNull(21);                 // LAST_ACCESS_TIME
    infoTable->setNull(22);                 // RECOVER_TIME
    infoTable->setNull(23);                 // TRANSACTION_COUNTER
    infoTable->setNull(24);                 // VERSION
    infoTable->setNull(25);                 // ROW_FORMAT
    infoTable->setNull(26);                 // TABLE_ROWS
    infoTable->setNull(27);                 // AVG_ROW_LENGTH
    infoTable->setNull(28);                 // DATA_LENGTH
    infoTable->setNull(29);                 // MAX_DATA_LENGTH
    infoTable->setNull(30);                 // INDEX_LENGTH
    infoTable->setNull(31);                 // DATA_FREE
    infoTable->setNull(32);                 // CREATE_TIME
    infoTable->setNull(33);                 // UPDATE_TIME
    infoTable->setNull(34);                 // CHECK_TIME
    infoTable->setNull(35);                 // CHECKSUM
    infoTable->putString(36, "NORMAL");     // STATUS NOT NULL
    infoTable->setNull(37);                 // EXTRA
    infoTable->putRecord();
  }
}

int TableSpaceManager::createTableSpaceId() {
  Sequence *sequence = database->sequenceManager->getSequence(
      database->getSymbol("SYSTEM"), database->getSymbol("TABLESPACE_IDS"));
  int id = (int)sequence->update(1, database->getSystemTransaction());
  return id;
}

void TableSpaceManager::openTableSpaces() {
  for (TableSpace *ts = tableSpaces; ts; ts = ts->next) {
    try {
      ts->open();
    } catch (SQLException &e) {
      Log::debug("Can't open tablespace %s, id %d : %s\n",
                 (const char *)ts->name, ts->tableSpaceId, e.getText());
    }
  }
}

}  // namespace Changjiang
