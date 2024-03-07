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

// Application.h: interface for the Application class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "TableAttachment.h"
#include "SyncObject.h"

namespace Changjiang {

static const int ALIAS_HASH_SIZE = 101;
static const int AGENT_HASH_SIZE = 101;
static const int MAX_ALIASED_QUERY_STRING = 80;

class Applications;
class Connection;
class Session;
class Image;
class Images;
class Database;
class Role;
class Module;
class PreparedStatement;
class Agent;
class Table;
class Alias;

class Application : public TableAttachment {
 public:
  Role *findRole(const char *roleName);
  Image *getImage(const char *name);
  void pushNameSpace(Connection *connection);
  Application(Applications *apps, const char *appName, Application *extends,
              const char *appClass);

 protected:
  virtual ~Application();

 public:
  void checkout(Connection *connection);
  void insertAgent(RecordVersion *record);
  void insertModule(RecordVersion *record);
  void loadAgents();
  Agent *getAgent(const char *userAgent);
  void addChild(Application *child);
  void rehash();
  virtual void tableDeleted(Table *table);
  const char *getAlias(Connection *connection, const char *queryString);
  Alias *insertAlias(const char *alias, const char *queryString);
  void initializeQueryLookup(Connection *connection);
  const char *findQueryString(Connection *connection, const char *string);
  virtual void insertCommit(Table *table, RecordVersion *record);
  void tableAdded(Table *table);
  void release();
  void addRef();

  Application *extends;
  Application *sibling;
  Application *children;
  Application *collision;
  Connection *aliasConnection;
  int pendingAliases;
  Database *database;
  JString name;
  JString className;
  Applications *applications;
  Images *images;
  Module *modules;
  const char *schema;
  volatile INTERLOCK_TYPE useCount;
  Table *modulesTable;
  Table *aliasesTable;
  Table *agentsTable;
  PreparedStatement *insertQueryAliases;
  SyncObject syncObject;
  Alias *aliases[ALIAS_HASH_SIZE];
  Alias *queryStrings[ALIAS_HASH_SIZE];
  Agent *agents[AGENT_HASH_SIZE];
  Agent *agentList;
};

}  // namespace Changjiang
