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

// SessionManager.h: interface for the SessionManager class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Synchronize.h"
#include "SyncObject.h"
#include "SessionQueue.h"

namespace Changjiang {

#define SESSION_HASH_SIZE 101

class Database;
class User;
class QueryString;
class Session;
class TemplateContext;
class Application;
class Connection;
class Java;
class JavaEnv;
class JavaNative;
class Stream;
class Client;
class LicenseToken;
class LicenseProduct;
class DateTime;

class _jobject;
class _jclass;
struct JNIEnv_;

typedef JNIEnv_ JNIEnv;

struct _jmethodID;

class SessionManager : public Synchronize {
 public:
  void release(JavaNative *javaNative, _jobject *object);
  void scheduled(Connection *connection, Application *application,
                 const char *eventName);
  void scavenge(DateTime *now);
  _jobject *getSessionObject(JavaNative *javaNative, Session *session,
                             _jobject *contextObject);
  int initiateService(Connection *connection, Session *session,
                      const char *serviceName);
  void insertPending(Session *session);
  LicenseToken *getLicenseToken(Session *target);
  void zapLinkages();
  Session *findSession(Application *application,
                       TemplateContext *templateContext, const char *cookie);
  void execute(Connection *connection, Application *application,
               TemplateContext *context, Stream *stream);
  void start(Connection *connection);
  Session *createSession(Application *application);
  void close(Session *session);
  SessionManager(Database *db);
  virtual ~SessionManager();

 protected:
  Session *findSession(Application *application, const char *sessionId);
  Session *hashTable[SESSION_HASH_SIZE];
  SyncObject syncObject;

 public:
  Session *createSession(Application *application, JString sessionId);
  Session *getSpecialSession(Application *application,
                             TemplateContext *context);
  LicenseToken *waitForLicense(Session *target);
  void removeSession(Session *session);
  void purged(Session *session);
  Database *database;
  Java *java;
  User *user;
  int nextSessionId;
  LicenseProduct *licenseProduct;
  SessionQueue pending;
  SessionQueue waiting;
  _jclass *sessionManagerClass;
  _jclass *connectionClass;
  _jclass *templateContextClass;
  _jclass *queryStringClass;
  //_jclass		*streamClass;
  _jmethodID *connectionInit;
  _jmethodID *connectionClose;
  _jmethodID *createSessionMethod;
  _jmethodID *closeSessionMethod;
  _jmethodID *executeSessionMethod;
  _jmethodID *executeInitiateService;
  _jmethodID *scheduledMethod;
  _jmethodID *templateContextInit;
  _jmethodID *queryStringInit;
  //_jmethodID	*streamInit;
};

}  // namespace Changjiang
