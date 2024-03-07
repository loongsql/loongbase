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

// Session.h: interface for the Session class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

namespace Changjiang {

class Database;
class Application;
class SessionManager;
class LicenseToken;
class Thread;
class SessionQueue;

class _jobject;

class Session {
 public:
  void release();
  void addRef();
  void closed();
  LicenseToken *surrenderLicenseToken(bool voluntary);
  void deactivate();
  bool activate(bool wait);
  void zap();
  void close();
  Session(SessionManager *manager, JString id, Application *app);
  virtual ~Session();

 public:
  void releaseLicense();
  void purged();
  JString sessionId;
  // JString			clientId;
  Session *collision;
  Session *next;
  Session *prior;
  SessionQueue *sessionQueue;
  Application *application;
  SessionManager *sessionManager;
  _jobject *sessionObject;
  bool zapped;
  bool active;
  LicenseToken *licenseToken;
  int32 expiration;
  Thread *thread;
  volatile INTERLOCK_TYPE useCount;
};

}  // namespace Changjiang
