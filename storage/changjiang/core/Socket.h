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

// Socket.h: interface for the Socket class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#undef ERROR

#ifdef _WIN32
#include <winsock.h>
#define socket_t SOCKET
#else
#define socket_t int
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

namespace Changjiang {

#ifndef int64
#ifdef _WIN32
typedef __int64 int64;
#else
typedef long long int64;
#endif
typedef int int32;
typedef unsigned int uint32;
typedef int64 int64;
#endif

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

static const int CLOSED_SOCKET = -1;

CLASS(Protocol);

class Socket  // : public Synchronize
{
 public:
  void setWriteTimeout(int milliseconds);
  void setReadTimeout(int milliseconds);
  int getSocketError();
  static void close(socket_t socket);
  static bool validateLocalAddress(int32 localAddress);
  static int32 translateAddress(const char *address);
  int32 getPartnerAddress();
  void putByte(char c);
  static JString getLocalName();
  int getLocalPort();
  void swapBytes(int length, void *bytes);
  void setSwapBytes(bool flag);
  int32 getSocketAddress();
  void writeString(const char *string);
  char getByte();
  virtual void shutdown();
  void create();
  void init();
  static int getInetFamily();
  void listen(int count = 5);
  bool getBoolean();
  void putBoolean(bool value);

#ifdef CHANGJIANGDB
  Protocol *acceptProtocol();
#else
  Socket *acceptSocket();
#endif

  int64 getQuad();
  void putQuad(int64 quad);
  double getDouble();
  void putDouble(double value);
  short getShort();
  void putShort(short value);
  void putString(const char *string);
  int32 getHandle();
  void putHandle(int32 handle);
  char *getString();
  void putString(int length, const char *string);
  static int initialize();
  void connect(const char *host, int port);
  void close();
  int getLong();
  void putLong(int value);
  void putBytes(int count, const void *buffer);
  void getBytes(int count, void *buffer);
  int flush();
  int read();
  Socket(socket_t sock, sockaddr_in *addr);
  void bind(int ipAddress, int port);
  Socket();
  virtual ~Socket();

  struct sockaddr_in address;

  socket_t socket;
  int bufferLength;
  char *readBuffer;
  char *writeBuffer;
  char *writePtr;  // put output pointer
  char *readPtr;   // next avail byte
  char *end;       // end of read buffer
  // fd_set	fd_reads;
  void *socketEvent;
  void *shutdownEvent;
  bool shutdownInProgress;
  bool swap;
};

}  // namespace Changjiang
