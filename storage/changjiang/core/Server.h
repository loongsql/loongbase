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

// Server.h: interface for the Server class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "MsgType.h"
#include "SyncObject.h"

namespace Changjiang {

class Database;
class Socket;
CLASS(Protocol);
class Connection;
class Threads;
class ResultSet;
class Parameters;
class Thread;
class Configuration;
CLASS(Statement);
class PreparedStatement;
class DatabaseMetaData;
class ResultList;

class Server {
 public:
  void notInStorageEngine();
  void stopServerThreads(bool panic);
  ResultList *getResultList();
  ResultSet *findResultSet();
  DatabaseMetaData *getDbMetaData();
  Statement *getStatement();
  PreparedStatement *getPreparedStatement();
  void deleteBlobData();
  int32 getLicenseIP();
  void getConnectionLimit();
  void setConnectionLimit();
  void getAutoCommit();
  void setAutoCommit();
  void getSequenceValue2();
  void debugRequest();
  void attachDebugger();
  void getHolderPrivileges();
  void getSequences();
  void addRef();
  void shutdownServer(bool panic);
  void setAttribute();
  void serverOperation();
  void setTraceFlags();
  void statementAnalyze();
  void analyze();
  void getDatabaseProductVersion();
  void getDatabaseProductName();
  void acceptLogConnection();
  static void acceptLogConnection(void *arg);
  void logListener(int mask, const char *text);
  static void logListener(int mask, const char *text, void *arg);
  void addListener();
  void closeResultSet();
  void closeStatement();
  void getSequenceValue();
  void validate();
  void getTriggers();
  void initiateService();
  void createDatabase2();
  void openDatabase2();
  void getParameters(Parameters *parameters);
  void getUsers();
  void getRoles();
  void getUserRoles();
  void getObjectPrivileges();
  void hasRole();
  void ping();
  void init();
  void cleanup();
  void getUpdateCount();
  void getMoreResults();
  void getIndexInfo();
  void getImportedKeys();
  void getPrimaryKeys();
  void getColumns();
  void getTables();
  void getDatabaseMetaData();
  void shutdownAgent(bool panic);
  void agentShutdown(Server *server);
  void genConnectionHtml();
  void executePreparedUpdate();
  void setCursorName();
  void prepareDrl();
  void getDrlResultSet();
  void executeQuery();
  void fetchRecord();
  void nextHit();
  void executePreparedStatement();
  void dispatch(MsgType type);
  void getResultSet();
  void search();
  void executeUpdate();
  void execute();
  void createStatement();
  void closeConnection();
  void rollbackTransaction();
  void prepareTransaction();
  void commitTransaction();
  void next();
  void getMetaData();
  void sendResultSet(ResultSet *resultSet);
  void executePreparedQuery();
  void prepareStatement();
  void agentLoop();
  void startAgent();
  static void agentLoop(void *lpParameter);
  static void getConnections(void *lpParameter);
  void getConnections();

  virtual void shutdown(bool panic);
  virtual int waitForExit();
  virtual void release();
  virtual bool isActive();

  Server(int port, const char *configFile);
  Server(Server *server, Protocol *proto);

 protected:
  virtual ~Server();

  SyncObject syncObject;
  Connection *connection;
  Protocol *protocol;
  Thread *thread;
  Thread *waitThread;
  Threads *threads;
  Server *parent;
  Socket *serverSocket;
  Protocol *logSocket;
  Protocol *acceptLogSocket;
  Server *activeServers;
  Server *nextServer;
  Configuration *configuration;
  char *string1;
  char *string2;
  char *string3;
  char *string4;
  char *string5;
  char *string6;
  int useCount;
  int port;
  bool active;
  bool first;
  bool forceShutdown;
  bool panicMode;
  volatile bool serverShutdown;
  // int32		ipAddress;
};

}  // namespace Changjiang
