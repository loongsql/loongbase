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

// Mutex.h: interface for the Mutex class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "SynchronizationObject.h"

#ifdef _WIN32
#include <windows.h>
#undef ERROR
#endif

#ifdef _PTHREADS
#include <pthread.h>
#endif

#ifdef SOLARIS_MT
#include <sys/mutex.h>
#include <thread.h>
#endif

namespace Changjiang {

class Mutex : public SynchronizationObject {
 public:
  void unlock();
  void release();
  void lock();
  Mutex(const char *desc);
  ~Mutex();
  Sync *holder;
  const char *description;

#ifdef _WIN32
  // void*	mutex;
  CRITICAL_SECTION criticalSection;
#endif

#ifdef _PTHREADS
  pthread_mutex_t mutex;
#endif

#ifdef SOLARIS_MT
  cond_t condition;
  mutex_t mutex;
#endif

  virtual void unlock(Sync *sync, LockType type);
  virtual void lock(Sync *sync, LockType type, int timeout);
  virtual void findLocks(LinkedList &threads, LinkedList &syncObjects);
};

}  // namespace Changjiang
