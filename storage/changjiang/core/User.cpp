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

// User.cpp: implementation of the User class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include "Engine.h"
#include "User.h"
#include "Privilege.h"
#include "UserRole.h"
#include "Database.h"
#include "Base64Transform.h"
#include "StringTransform.h"
#include "SHATransform.h"
#include "DecodeTransform.h"
#include "EncodeTransform.h"
#include "EncryptTransform.h"
#include "TransformUtil.h"

#ifndef STORAGE_ENGINE
#include "Coterie.h"
#endif

namespace Changjiang {

#define HASH(address, size) (int)(((UIPTR)address >> 2) % size)

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

User::User(Database *db, const char *accnt, const char *passwd,
           Coterie *buddies, bool sys)
    : Role(db, db->getSymbol(""), accnt) {
  changePassword(passwd);
  system = sys;
  roleList = NULL;
  coterie = buddies;
  memset(roles, 0, sizeof(roles));
}

User::~User() {
  for (UserRole *role; (role = roleList);) {
    roleList = role->next;
    delete role;
  }
}

void User::addRole(Role *role, bool defRole, int options) {
  int slot = HASH(role->name, ROLES_HASH_SIZE);
  UserRole *userRole = new UserRole(role, defRole, options);
  userRole->collision = roles[slot];
  roles[slot] = userRole;
  userRole->next = roleList;
  roleList = userRole;
}

bool User::validatePassword(const char *passwd) {
  EncodeTransform<StringTransform, SHATransform> encode(passwd);
  StringTransform digest(sizeof(passwordDigest), passwordDigest);

  return TransformUtil::compareDigests(&encode, &digest);
}

int User::hasRole(Role *role) {
  if (!role) return 0;

  int slot = HASH(role->name, ROLES_HASH_SIZE);

  for (UserRole *userRole = roles[slot]; userRole;
       userRole = userRole->collision)
    if (userRole->role == role) return userRole->mask;

  return 0;
}

void User::changePassword(const char *passwd) {
  password = passwd;
  DecodeTransform<StringTransform, Base64Transform> transform(password);
  transform.get(sizeof(passwordDigest), passwordDigest);
}

void User::revokeRole(Role *role) {
  UserRole *userRole;
  int slot = HASH(role->name, ROLES_HASH_SIZE);
  UserRole **ptr;

  for (ptr = roles + slot; (userRole = *ptr); ptr = &userRole->collision)
    if (userRole->role == role) {
      *ptr = userRole->collision;
      break;
    }
  for (ptr = &roleList; (userRole = *ptr); ptr = &userRole->next)
    if (userRole->role == role) {
      *ptr = userRole->next;
      delete userRole;
      break;
    }
}

void User::updateRole(Role *role, bool defRole, int options) {
  int slot = HASH(role->name, ROLES_HASH_SIZE);

  for (UserRole *userRole = roles[slot]; userRole;
       userRole = userRole->collision)
    if (userRole->role == role) {
      userRole->defaultRole = defRole;
      userRole->options = options;
      return;
    }

  addRole(role, defRole, options);
}

JString User::encryptPassword(const char *password) {
  EncryptTransform<StringTransform, SHATransform, Base64Transform> encode(
      password, 0);

  return TransformUtil::getString(&encode);
}

PrivObject User::getPrivilegeType() { return PrivUser; }

bool User::validateAddress(int32 address) {
#ifdef STORAGE_ENGINE
  return true;

#else
  if (!coterie) return true;

  return coterie->validateAddress(address);
#endif
}

}  // namespace Changjiang
