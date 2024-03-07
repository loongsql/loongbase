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

// RoleModel.h: interface for the RoleModel class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "PrivType.h"

namespace Changjiang {

#define ROLE_HASH_SIZE 101

class Database;
class User;
class Role;
class PrivilegeObject;
class Coterie;
class Table;

class RoleModel {
 public:
  void renameTable(Table *table, const char *newSchema, const char *newName);
  void deletePrivileges(Role *role);
  void dropUser(User *user);
  void initialize();
  void dropObject(PrivilegeObject *object);
  void removePrivilege(Role *role, PrivilegeObject *object, int32 mask);
  User *getSystemUser();
  void insertPrivilege(Role *role, PrivilegeObject *object, int32 mask);
  void addUserPrivilege(User *user, PrivilegeObject *object, int32 mask);
  void dropRole(Role *role);
  void revokeUserRole(User *user, Role *role);
  void changePassword(User *user, const char *password, bool encrypted,
                      Coterie *coterie);
  void insertUser(User *user);
  bool updatePrivilege(Role *role, PrivilegeObject *object, int32 mask);
  User *getUser(const char *userName);
  Role *getRole(const char *schema, const char *name);
  void addUserRole(User *user, Role *role, bool defaultRole, int options);
  void addPrivilege(Role *role, PrivilegeObject *object, int32 mask);
  void createRole(User *owner, const char *schema, const char *name);
  Role *findRole(const char *schema, const char *name);
  User *findUser(const char *name);
  User *createUser(const char *name, const char *password, bool encrypted,
                   Coterie *coterie);
  void createTables();
  RoleModel(Database *db);
  virtual ~RoleModel();

  void dropCoterie(Coterie *coterie);
  void insertCoterie(Coterie *coterie);
  Coterie *createCoterie(const char *name);
  Coterie *getCoterie(const char *name);
  Coterie *findCoterie(const char *name);
  void updateCoterie(Coterie *coterie);

  Database *database;
  User *users[ROLE_HASH_SIZE];
  Role *roles[ROLE_HASH_SIZE];
  Coterie *coteries;
  bool tablesCreated;
  User *systemUser;
  User *publicUser;
};

}  // namespace Changjiang
