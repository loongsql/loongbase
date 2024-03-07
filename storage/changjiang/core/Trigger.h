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

// Trigger.h: interface for the Trigger class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "LinkedList.h"

namespace Changjiang {

class Table;
class Database;
class Record;
class RecordVersion;
class Transaction;
class JavaNative;
class Java;
class TriggerRecord;
CLASS(Field);
class Connection;

class _jobject;
class _jclass;
struct JNIEnv_;

typedef JNIEnv_ JNIEnv;

struct _jmethodID;

class ChjTrigger {
 public:
  void release();
  void addRef();
  bool isEnabled(Connection *connection);
  void addTriggerClass(const char *symbol);
  Field *getField(int id);
  Field *getField(const WCString *fieldName);
  _jobject *wrapTriggerRecord(JavaNative *javaNative, TriggerRecord *record);
  static void deleteTrigger(Database *database, const char *schema,
                            const char *name);
  static JString getTableName(Database *database, const char *schema,
                              const char *name);
  void deleteTrigger();
  void fireTrigger(Transaction *transaction, int operation, Record *before,
                   RecordVersion *after);
  static void getTableTriggers(Table *table);
  void save();
  void loadClass();
  void zapLinkages();
  static void initialize(Database *database);
  ChjTrigger(JString triggerName, Table *tbl, int typeMask, int pos, bool act,
             JString cls, JString method);

 protected:
  virtual ~ChjTrigger();

 public:
  JString name;
  JString className;
  JString methodName;
  int useCount;
  Table *table;
  Database *database;
  Java *java;
  int mask;
  int position;
  bool active;
  ChjTrigger *next;
  LinkedList triggerClasses;
  _jclass *triggerClass;
  _jclass *recordClass;
  _jmethodID *recordInit;
  _jmethodID *triggerExecute;
};

}  // namespace Changjiang
