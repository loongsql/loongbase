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

// Schedule.h: interface for the Schedule class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "JString.h"

namespace Changjiang {

class ScheduleElement;
class Scheduler;

#define SCHEDULE_ELEMENTS 6

class Schedule {
 public:
  void release();
  void addRef();
  virtual void getNextEvent();
  virtual void execute(Scheduler *scheduler);
  Schedule(const char *string);

 protected:
  virtual ~Schedule();

 public:
  Schedule *next;
  Schedule *prior;
  bool deleted;
  time_t eventTime;

 private:
  volatile INTERLOCK_TYPE useCount;
  JString string;
  ScheduleElement *elements[SCHEDULE_ELEMENTS];

 public:
  static int validateSchedule(const char *scheduleString);
};

}  // namespace Changjiang
