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

// PrivilegeObject.cpp: implementation of the PrivilegeObject class.
//
//////////////////////////////////////////////////////////////////////

#include <string.h>
#include "Engine.h"
#include "PrivilegeObject.h"
#include "Database.h"
#include "RoleModel.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

PrivilegeObject::PrivilegeObject(Database *db) {
  database = db;
  catalogName = "";
}

PrivilegeObject::~PrivilegeObject() {}

void PrivilegeObject::drop() { database->roleModel->dropObject(this); }

void PrivilegeObject::setName(const char *objectSchema,
                              const char *objectName) {
  ASSERT(database->isSymbol(objectSchema));
  ASSERT(database->isSymbol(objectName));
  schemaName = objectSchema;
  name = objectName;
}

bool PrivilegeObject::isNamed(const char *objectSchema,
                              const char *objectName) {
  return !strcmp(objectSchema, schemaName) && !strcmp(objectName, name);
}

}  // namespace Changjiang
