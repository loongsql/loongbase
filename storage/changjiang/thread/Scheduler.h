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

// Scheduler.h: interface for the Scheduler class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "SyncObject.h"

namespace Changjiang {

class Database;
class Thread;
class Schedule;
class AppEvent;
class User;
// class PreparedStatement;
class Application;

class Scheduler {
 public:
  void release();
  void addRef();
  void deleteEvents(Application *application);
  void initialize();
  void loadEvents(Application *application);
  AppEvent *removeEvent(const char *appName, const char *eventName);
  void updateSchedule(const char *appName, const char *eventName, User *user,
                      const char *schedule);
  void start();
  void addEvent(Schedule *schedule);
  static void schedule(void *lpParameter);
  void schedule();
  void shutdown(bool panic);
  Scheduler(Database *db);

 protected:
  virtual ~Scheduler();

 public:
  Database *database;
  Thread *thread;
  bool shutdownInProgress;
  Schedule *next;
  SyncObject syncObject;
  AppEvent *events;
  int useCount;
};

}  // namespace Changjiang
