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

// Protocol.h: interface for the Protocol class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Socket.h"
#include "MsgType.h"

namespace Changjiang {

#define PORT 1858
#define PROTOCOL_VERSION_1 1
#define PROTOCOL_VERSION_2 2
#define PROTOCOL_VERSION_3 3
#define PROTOCOL_VERSION_4 4
#define PROTOCOL_VERSION_5 5
#define PROTOCOL_VERSION_6 6
#define PROTOCOL_VERSION_7 7
#define PROTOCOL_VERSION_8 8
#define PROTOCOL_VERSION_9 9
#define PROTOCOL_VERSION_10 10
#define PROTOCOL_VERSION_11 11
#define PROTOCOL_VERSION_12 12
#define PROTOCOL_VERSION PROTOCOL_VERSION_12

/*
 * History
 *
 *		Version 4	4/4/2001		Passing dates as QUADs w/
 *millisecond units Version 5	7/2/2001		Added CloseStatement,
 *GetSequenceValue Version 6	1/20/2002		Added
 *Connection.setTraceFlags Version 7	2/19/2002		Added
 *Connect.SetAttribute, Connection.ServerOp
 *		Version 8	3/29/2002		Added GetHolderPrivileges,
 *GetSequences Version 9	4/1/2003		Added AttachDebugger and
 *DebugRequest Version 10	6/12/2003		Added
 *Connection.getLimits/setLimits Version 11	10/4/2003		Added
 *repository info to blobs and clobs Version 12	10/29/2003		Added
 *Connection.deleteBlobData
 */

class Socket;
class DbResultSet;
class Value;
class SQLException;
class Database;
class BlobReference;

START_NAMESPACE

class Protocol : public Socket {
 public:
  Protocol();
  Protocol(socket_t sock, sockaddr_in *addr);
  virtual ~Protocol();

  void getRepository(BlobReference *blob);
  void putValues(Database *database, int count, Value *values);
  int32 createStatement();
  void putMsg(MsgType type, int32 handle);
  void getValue(Value *value);
  void putValue(Database *database, Value *value);
  void shutdown();
  void sendFailure(SQLException *exception);
  void sendSuccess();
  bool getResponse();
  MsgType getMsg();
  void putMsg(MsgType type);
  int32 openDatabase(const char *fileName);

  int protocolVersion;
};
END_NAMESPACE

}  // namespace Changjiang
