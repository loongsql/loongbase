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

// Trigger.cpp: implementation of the Trigger class.
//
//////////////////////////////////////////////////////////////////////

#include <string.h>
#include "Engine.h"
#include "Trigger.h"
#include "Database.h"
#include "Table.h"
#include "Connection.h"
#include "Database.h"
#include "SQLError.h"
#include "ResultSet.h"
#include "PreparedStatement.h"
#include "RecordVersion.h"
#include "TriggerRecord.h"
#include "Transaction.h"
#include "Sync.h"

#ifndef STORAGE_ENGINE
#include "JavaVM.h"
#include "Java.h"
#include "JavaNative.h"
#include "JavaEnv.h"
#include "JavaThread.h"
#include "JavaObject.h"
#endif

#undef RECORD

namespace Changjiang {

#define CONNECTION "netfrastructure/sql/NfsConnection"
#define RECORD "netfrastructure/sql/Record"
#define NFSRECORD "netfrastructure/sql/NfsRecord"
#define SIG(sig) "L" sig ";"

static const char *ddl[] = {
    //"drop table system.triggerclasses",
    "upgrade table system.triggers (\n"
    "schema varchar (128) not null,\n"
    "tableName varchar (128) not null,\n"
    "triggerName varchar (128) not null,\n"
    "type_mask integer,\n"
    "position smallint,\n"
    "active smallint,\n"
    "classname varchar (256),\n"
    "methodname varchar (256),\n"
    "primary key (schema,tableName,triggerName))",
    "upgrade table system.triggerclasses (\n"
    "schema varchar (128) not null,\n"
    "triggerName varchar (128) not null,\n"
    "triggerClass varchar (128) not null,\n"
    "primary key (schema,triggerName,triggerClass))",
    "grant select on system.triggers to public",
    "grant select on system.triggerclasses to public", NULL};
#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

ChjTrigger::ChjTrigger(JString triggerName, Table *tbl, int typeMask, int pos,
                       bool act, JString cls, JString method) {
  name = triggerName;
  table = tbl;
  database = table->database;
  java = database->java;
  mask = typeMask;
  position = pos;
  active = act;
  zapLinkages();
  methodName = method;
  className = cls;
  useCount = 1;
}

ChjTrigger::~ChjTrigger() {}

void ChjTrigger::initialize(Database *database) {
  Table *table = database->findTable("SYSTEM", "TRIGGERCLASSES");

  if (!table || !table->findField(database->getSymbol("TRIGGERCLASS")))
    for (const char **sql = ddl; *sql; ++sql) database->execute(*sql);
}

void ChjTrigger::zapLinkages() {
  triggerClass = NULL;
  triggerExecute = NULL;
  recordClass = NULL;
  recordInit = NULL;
}

void ChjTrigger::loadClass() {}

void ChjTrigger::save() {
  PreparedStatement *statement = database->prepareStatement(
      "replace into system.triggers "
      "(schema,triggerName,tableName,type_mask,position,active,classname,"
      "methodname)"
      "values (?,?,?,?,?,?,?,?)");
  int n = 1;
  statement->setString(n++, table->schemaName);
  statement->setString(n++, name);
  statement->setString(n++, table->name);
  statement->setInt(n++, mask);
  statement->setInt(n++, position);
  statement->setInt(n++, active);
  statement->setString(n++, className);
  statement->setString(n++, methodName);
  statement->executeUpdate();
  statement->close();

  statement = database->prepareStatement(
      "replace into system.triggerclasses "
      "(schema,triggerName,triggerclass)"
      "values (?,?,?)");
  n = 1;
  statement->setString(n++, table->schemaName);
  statement->setString(n++, name);

  FOR_OBJECTS(const char *, triggerClass, &triggerClasses)
  statement->setString(n, triggerClass);
  statement->executeUpdate();
  END_FOR;

  statement->close();
}

void ChjTrigger::getTableTriggers(Table *table) {
  Database *database = table->database;

  PreparedStatement *statement = database->prepareStatement(
      "select triggerName,type_mask,position,active,classname,methodname "
      "from system.triggers where schema=? and tableName=?");
  int n = 1;
  statement->setString(n++, table->schemaName);
  statement->setString(n++, table->name);
  ResultSet *resultSet = statement->executeQuery();

  PreparedStatement *trigClasses = database->prepareStatement(
      "select triggerClass from system.triggerClasses where schema=? and "
      "triggerName=?");
  trigClasses->setString(1, table->schemaName);

  while (resultSet->next()) {
    int n = 1;
    JString name = resultSet->getString(n++);
    int type = resultSet->getInt(n++);
    int position = resultSet->getInt(n++);
    bool active = resultSet->getInt(n++) != 0;
    JString className = resultSet->getString(n++);
    JString methodName = resultSet->getString(n++);
    ChjTrigger *trigger = new ChjTrigger(name, table, type, position, active,
                                         className, methodName);
    trigClasses->setString(2, name);
    ResultSet *classes = trigClasses->executeQuery();

    while (classes->next()) trigger->addTriggerClass(classes->getSymbol(1));
    classes->close();

    table->addTrigger(trigger);
  }

  resultSet->close();
  statement->close();
  trigClasses->close();
}

void ChjTrigger::fireTrigger(Transaction *transaction, int operation,
                             Record *prior, RecordVersion *post) {
#ifdef STORAGE_ENGINE
  throw SQLError(RUNTIME_ERROR,
                 "triggers are not support in Changjiang storage engine");
#else
  TriggerRecord before(this, transaction, prior, operation, false);
  TriggerRecord after(this, transaction, post, operation,
                      (operation & (PreInsert | PreUpdate)) != 0);

  if (java->checkClassReload()) java->reloadClasses();

  JavaNative jni("Trigger::fireTrigger", java->javaEnv);

  if (!triggerClass) {
    char classPath[256], *p = classPath;
    for (const char *q = className; *q; ++q) *p++ = (*q == '.') ? '/' : *q;
    *p = 0;
    triggerClass = jni.FindClass(classPath);
    if (jni.ExceptionOccurred()) java->throwCException(&jni);
  }

  if (!triggerExecute) {
    triggerExecute = jni.GetStaticMethodID(triggerClass, methodName,
                                           "(" SIG(CONNECTION) SIG(RECORD)
                                               SIG(RECORD) "I)V");
    if (jni.ExceptionOccurred()) java->throwCException(&jni);
  }

  jobject beforeObject = 0;
  jobject afterObject = 0;

  if (prior) {
    beforeObject = wrapTriggerRecord(&jni, &before);
    if (jni.ExceptionOccurred()) java->throwCException(&jni);
  }

  if (post) {
    afterObject = wrapTriggerRecord(&jni, &after);
    if (jni.ExceptionOccurred()) java->throwCException(&jni);
  }

  jni.CallStaticVoidMethod(triggerClass, triggerExecute,
                           java->wrapConnection(&jni, transaction->connection),
                           beforeObject, afterObject, operation);

  if (jni.ExceptionOccurred()) java->throwCException(&jni);
#endif
}

JString ChjTrigger::getTableName(Database *database, const char *schema,
                                 const char *name) {
  PreparedStatement *statement = database->prepareStatement(
      "select tableName from system.triggers where schema=? and triggerName=?");
  int n = 1;
  statement->setString(n++, schema);
  statement->setString(n++, name);
  ResultSet *resultSet = statement->executeQuery();
  JString tableName;

  while (resultSet->next()) tableName = resultSet->getString(1);

  resultSet->close();
  statement->close();

  return tableName;
}

void ChjTrigger::deleteTrigger() {
  addRef();
  table->dropTrigger(this);
  deleteTrigger(database, table->schemaName, name);
  release();
}

void ChjTrigger::deleteTrigger(Database *database, const char *schema,
                               const char *name) {
  PreparedStatement *statement = database->prepareStatement(
      "delete from system.triggers where schema=? and triggerName=?");
  int n = 1;
  statement->setString(n++, schema);
  statement->setString(n++, name);
  statement->executeUpdate();
  statement->close();

  statement = database->prepareStatement(
      "delete from system.triggerclasses where schema=? and triggerName=?");
  n = 1;
  statement->setString(n++, schema);
  statement->setString(n++, name);
  statement->executeUpdate();
  statement->close();
}

_jobject *ChjTrigger::wrapTriggerRecord(JavaNative *javaNative,
                                        TriggerRecord *record) {
#ifdef STORAGE_ENGINE
  return NULL;
#else
  if (!recordClass) {
    recordClass = javaNative->FindClass(NFSRECORD);
    if (!recordClass) java->throwCException(javaNative);
    recordInit = javaNative->GetMethodID(recordClass, "<init>", "()V");
    if (!recordInit) java->throwCException(javaNative);
  }

  return java->createExternalObject(javaNative, recordClass, recordInit,
                                    record);
#endif
}

Field *ChjTrigger::getField(const WCString *fieldName) {
  Field *field = table->findField(fieldName);

  if (!field)
    throw SQLError(RUNTIME_ERROR, "field \"%s\" not defined in table \"%s.%s\"",
                   database->getSymbol(fieldName),
                   (const char *)table->schemaName, (const char *)table->name);

  return field;
}

Field *ChjTrigger::getField(int id) {
  Field *field = (id >= 0) ? table->findField(id) : NULL;

  if (!field)
    throw SQLError(RUNTIME_ERROR, "field id %d not defined in table \"%s.%s\"",
                   id, (const char *)table->schemaName,
                   (const char *)table->name);

  return field;
}

void ChjTrigger::addTriggerClass(const char *symbol) {
  triggerClasses.appendUnique((void *)symbol);
}

bool ChjTrigger::isEnabled(Connection *connection) {
  FOR_OBJECTS(const char *, triggerClass, &connection->disabledTriggerClasses)
  if (triggerClasses.isMember((void *)triggerClass)) return false;
  END_FOR;

  return true;
}

void ChjTrigger::addRef() { ++useCount; }

void ChjTrigger::release() {
  if (--useCount == 0) delete this;
}

}  // namespace Changjiang
