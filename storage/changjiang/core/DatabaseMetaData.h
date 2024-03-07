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

// DatabaseMetaData.h: interface for the DatabaseMetaData class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

namespace Changjiang {

#define NETFRASERVER "NetfraServer"

class ResultSet;
class Connection;

class DatabaseMetaData {
 public:
  ResultSet *getSchemaParameters();
  ResultSet *getRepositories(const char *catalog, const char *schema,
                             const char *namePattern);
  ResultSet *getDomains(const char *catalog, const char *schema,
                        const char *fieldName);
  virtual ResultSet *getExportedKeys(const char *catalog, const char *schema,
                                     const char *tableName);
  virtual ResultSet *getObjectPrivileges(const char *catalog,
                                         const char *schemaPattern,
                                         const char *namePattern,
                                         int objectType);
  virtual ResultSet *getUserRoles(const char *catalog, const char *schema,
                                  const char *rolePattern, const char *user);
  virtual ResultSet *getRoles(const char *catalog, const char *schema,
                              const char *rolePattern);
  virtual ResultSet *getUsers(const char *catalog, const char *userPattern);
  virtual ResultSet *getImportedKeys(const char *catalog, const char *schema,
                                     const char *tableName);
  virtual void release();
  virtual void addRef();
  virtual ResultSet *getTables(const char *catalog, const char *schemaPattern,
                               const char *tableNamePattern, int typeCount,
                               const char **types);
  virtual ResultSet *getTables(const char *catalog, const char *schemaPattern,
                               const char *tableNamePattern,
                               const char **types);
  virtual ~DatabaseMetaData();
  virtual ResultSet *getIndexInfo(const char *catalog, const char *schema,
                                  const char *tableName, bool b1, bool v2);
  virtual ResultSet *getPrimaryKeys(const char *catalog, const char *schema,
                                    const char *table);
  virtual ResultSet *getColumns(const char *catalog, const char *schema,
                                const char *table, const char *fieldName);
  virtual ResultSet *getTriggers(const char *catalog, const char *schema,
                                 const char *table, const char *triggerPattern);
  virtual ResultSet *getHolderPrivileges(const char *catalog,
                                         const char *schemaPattern,
                                         const char *namePattern,
                                         int objectType);
  virtual ResultSet *getSequences(const char *catalog, const char *schema,
                                  const char *sequence);
  virtual const char *getDatabaseProductName();
  virtual const char *getDatabaseProductVersion();

  DatabaseMetaData(Connection *connection);

  Connection *connection;
  JString databaseVersion;
  int useCount;
  int32 handle;
};

}  // namespace Changjiang
