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

#pragma once

namespace Changjiang {

enum MsgType {
  Failure,
  Success,
  Shutdown,
  OpenDatabase,
  CreateDatabase,
  CloseConnection,
  PrepareTransaction,
  CommitTransaction,
  RollbackTransaction,
  PrepareStatement,
  PrepareDrl,
  CreateStatement,
  GenConnectionHtml,

  GetResultSet,
  Search,
  CloseStatement,
  ClearParameters,
  GetParameterCount,
  Execute,
  ExecuteQuery,
  ExecuteUpdate,
  SetCursorName,

  ExecutePreparedStatement,
  ExecutePreparedQuery,
  ExecutePreparedUpdate,
  SendParameters,

  GetMetaData,
  Next,
  CloseResultSet,
  GenHtml,

  NextHit,
  FetchRecord,
  CloseResultList,

  GetDatabaseMetaData,   // Connection
  GetCatalogs,           // DatabaseMetaData
  GetSchemas,            // DatabaseMetaData
  GetTables,             // DatabaseMetaData
  getTablePrivileges,    // DatabaseMetaData
  GetColumns,            // DatabaseMetaData
  GetColumnsPrivileges,  // DatabaseMetaData
  GetPrimaryKeys,        // DatabaseMetaData
  GetImportedKeys,       // DatabaseMetaData
  GetExportedKeys,       // DatabaseMetaData
  GetIndexInfo,          // DatabaseMetaData
  GetTableTypes,         // DatabaseMetaData
  GetTypeInfo,           // DatabaseMetaData
  GetMoreResults,        // Statement
  GetUpdateCount,        // Statement

  Ping,                 // Connection
  HasRole,              // Connection
  GetObjectPrivileges,  // DatabaseMetaData
  GetUserRoles,         // DatabaseMetaData
  GetRoles,             // DatabaseMetaData
  GetUsers,             // DatabaseMetaData

  OpenDatabase2,    // variation with attribute list
  CreateDatabase2,  // variation with attribute list

  InitiateService,  // Connection
  GetTriggers,      // DatabaseMetaData
  Validate,
  GetAutoCommit,
  SetAutoCommit,
  GetReadOnly,
  IsReadOnly,
  GetTransactionIsolation,
  SetTransactionIsolation,
  GetSequenceValue,
  AddLogListener,             // Connection
  DeleteLogListener,          // Connection
  GetDatabaseProductName,     // DatabaseMetaData
  GetDatabaseProductVersion,  // DatabaseMetaData
  Analyze,                    // Connection
  StatementAnalyze,           // Statement
  SetTraceFlags,              // Connection
  ServerOperation,            // Connection
  SetAttribute,               // Connection
  GetSequences,               // DatabaseMetaData
  GetHolderPrivileges,        // DatabaseMetaData
  AttachDebugger,             // Connection
  DebugRequest,               // Non-operational debug request
  GetSequenceValue2,          // Connection
  GetConnectionLimit,         // Connection
  SetConnectionLimit,         // Connection
  DeleteBlobData,             // Connection
};

}  // namespace Changjiang
