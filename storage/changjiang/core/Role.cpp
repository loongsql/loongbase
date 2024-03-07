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

// Role.cpp: implementation of the Role class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <memory.h>
#include "Engine.h"
#include "Role.h"
#include "Privilege.h"
#include "Table.h"
#include "Database.h"
#include "PreparedStatement.h"
#include "ResultSet.h"
#include "Sync.h"

namespace Changjiang {

#define HASH(address, size) (int)(((UIPTR)address >> 2) % size)

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Role::Role(Database *db, const char *roleSchema, const char *roleName)
    : PrivilegeObject(db) {
  setName(roleSchema, roleName);
  useCount = 1;
  memset(privileges, 0, sizeof(privileges));
  syncObject.setName("Role::syncObject");
}

Role::~Role() {
  Privilege *priv;

  for (int n = 0; n < ROLE_PRIV_SIZE; ++n)
    while ((priv = privileges[n])) {
      privileges[n] = priv->collision;
      delete priv;
    }
}

int32 Role::getPrivileges(PrivilegeObject *object) {
  Privilege *priv = getPrivilege(object);

  return priv->privilegeMask;
}

Privilege *Role::getPrivilege(PrivilegeObject *object) {
  Sync sync(&syncObject, "Role::getPrivilege");
  sync.lock(Shared);

  int slot = HASH(object, ROLE_PRIV_SIZE);
  slot = ABS(slot);
  Privilege *priv;

  for (priv = privileges[slot]; priv; priv = priv->collision)
    if (priv->object == object) return priv;

  sync.unlock();
  sync.lock(Exclusive);

  for (priv = privileges[slot]; priv; priv = priv->collision)
    if (priv->object == object) return priv;

  const char *objectSchema = object->schemaName;
  const char *objectName = object->name;
  PrivObject type = object->getPrivilegeType();
  int32 mask = 0;

  if (!database->formatting) {
    PreparedStatement *statement = database->prepareStatement(
        "select privilegeMask from system.privileges where "
        "holderType=? and "
        "holderSchema=? and "
        "holderName=? and "
        "objectType=? and "
        "objectSchema=? and "
        "objectName=?");

    int n = 1;
    statement->setInt(n++, getPrivilegeType());
    statement->setString(n++, schemaName);
    statement->setString(n++, name);
    statement->setInt(n++, type);
    statement->setString(n++, objectSchema);
    statement->setString(n++, objectName);
    ResultSet *resultSet = statement->executeQuery();
    bool hit = false;

    while (resultSet->next()) {
      mask = resultSet->getInt(1);
      hit = true;
    }

    /***
    if (mask == 0)
            {
            resultSet->close();
            resultSet = statement->executeQuery();
            resultSet->next();
            }
    ***/

    resultSet->close();
    statement->close();
  }

  priv = new Privilege(type, object, mask);
  priv->collision = privileges[slot];
  privileges[slot] = priv;

  return priv;
}

void Role::addRef() { ++useCount; }

void Role::release() {
  if (--useCount == 0) delete this;
}

PrivObject Role::getPrivilegeType() { return PrivRole; }

void Role::dropObject(PrivilegeObject *object) {
  Sync sync(&syncObject, "Role::dropObject");
  sync.lock(Exclusive);

  for (int n = 0; n < ROLE_PRIV_SIZE; ++n)
    for (Privilege **ptr = privileges + n; *ptr;) {
      Privilege *privilege = *ptr;

      if (privilege->object == object) {
        *ptr = privilege->collision;
        delete privilege;
      } else
        ptr = &privilege->collision;
    }
}

}  // namespace Changjiang
