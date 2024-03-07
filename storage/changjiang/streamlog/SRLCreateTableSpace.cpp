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

// SRLCreateTableSpace.cpp: implementation of the SRLCreateTableSpace class.
//
//////////////////////////////////////////////////////////////////////

#include <string.h>
#include "Engine.h"
#include "SRLCreateTableSpace.h"
#include "TableSpace.h"
#include "Database.h"
#include "TableSpaceManager.h"
#include "StreamLogControl.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLCreateTableSpace::SRLCreateTableSpace() {}

SRLCreateTableSpace::~SRLCreateTableSpace() {}

void SRLCreateTableSpace::append(TableSpace *tableSpace) {
  START_RECORD(srlCreateTableSpace, "SRLData::append");
  putInt(tableSpace->tableSpaceId);
  const char *p = tableSpace->name;
  int len = (int)strlen(p);
  putInt(len);
  putData(len, (const UCHAR *)p);
  p = tableSpace->filename;
  len = (int)strlen(p);
  putInt(len);
  putData(len, (const UCHAR *)p);
  putInt(tableSpace->type);
  p = tableSpace->comment;
  len = (int)strlen(p);
  putInt(len);
  putData(len, (const UCHAR *)p);
}

void SRLCreateTableSpace::read() {
  tableSpaceId = getInt();
  nameLength = getInt();
  name = (const char *)getData(nameLength);
  filenameLength = getInt();
  filename = (const char *)getData(filenameLength);

  if (control->version >= srlVersion11)
    type = getInt();
  else
    type = TABLESPACE_TYPE_TABLESPACE;

  if (control->version >= srlVersion15) {
    commentLength = getInt();
    comment = (const char *)getData(commentLength);
  } else {
    commentLength = 0;
    comment = NULL;
  }
}

void SRLCreateTableSpace::pass1() {}

void SRLCreateTableSpace::pass2() {
  if (log->database->tableSpaceManager->findTableSpace(tableSpaceId)) return;

  TableSpaceInit tsInit;
  tsInit.comment = comment;
  log->database->tableSpaceManager->redoCreateTableSpace(
      tableSpaceId, nameLength, name, filenameLength, filename, type, &tsInit);
}

void SRLCreateTableSpace::commit() {}

void SRLCreateTableSpace::redo() {}

void SRLCreateTableSpace::print(void) {
  logPrint("Create Table Space %d, name %.*s, filename %.*s\n", tableSpaceId,
           nameLength, name, filenameLength, filename);
}

}  // namespace Changjiang
