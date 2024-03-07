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

// Synchronize.h: interface for the Synchronize class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <Mutex.h>

#ifdef _PTHREADS
#include <pthread.h>
#endif

//#define SYNCHRONIZE_FREEZE

namespace Changjiang {

#ifdef SYNCHRONIZE_FREEZE
#define DEBUG_FREEZE \
  if (synchronizeFreeze) Synchronize::freeze();
extern INTERLOCK_TYPE synchronizeFreeze;
#else
#define DEBUG_FREEZE
#endif

#ifdef CHANGJIANGDB
#define LOG_DEBUG Log::debug
#define DEBUG_BREAK Log::debugBreak
#else
#define LOG_DEBUG printf
#define DEBUG_BREAK printf
#endif

class Synchronize {
 public:
  virtual void shutdown();
  virtual void wake();
  virtual bool sleep(int milliseconds);
  virtual bool sleep(int milliseconds, Mutex *callersMutex);
  virtual void sleep();
  Synchronize();
  virtual ~Synchronize();

  bool shutdownInProgress;
  bool sleeping;
  volatile bool wakeup;
  int64 waitTime;

#ifdef _WIN32
  void *event;
#endif

#ifdef _PTHREADS
  pthread_cond_t condition;
  pthread_mutex_t mutex;
#endif
  static void freeze(void);
  static void freezeSystem(void);
};

}  // namespace Changjiang
