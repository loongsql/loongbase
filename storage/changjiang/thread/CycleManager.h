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

#include "SyncObject.h"

namespace Changjiang {

class Thread;
class Database;
class Record;
class RecordVersion;
class Value;

static const int syncArraySize = 64;
static const int syncArrayMask = 63;

class CycleManager {
  struct RecordList {
    Record *zombie;
    RecordList *next;
  };

  struct ValueList {
    Value **zombie;
    ValueList *next;
  };

  struct BufferList {
    char *zombie;
    BufferList *next;
  };

 public:
  CycleManager(Database *database);
  ~CycleManager(void);

  void start(void);
  void shutdown(void);
  void cycleManager(void);
  SyncObject *getSyncObject(void);
  void queueForDelete(Record *zombie);
  void queueForDelete(Value **zombie);
  void queueForDelete(char *zombie);

  static void cycleManager(void *arg);

  SyncObject **cycle1;
  SyncObject **cycle2;
  SyncObject **currentCycle;
  RecordVersion *recordVersionPurgatory;
  RecordList *recordPurgatory;
  ValueList *valuePurgatory;
  BufferList *bufferPurgatory;
  Thread *thread;
  Database *database;
};

}  // namespace Changjiang
