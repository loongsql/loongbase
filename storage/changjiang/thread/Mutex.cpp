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

#ifdef _WIN32
#include <windows.h>
#undef ERROR
#undef ASSERT
#endif

#include <stdio.h>

#include "Engine.h"
#include "Mutex.h"

namespace Changjiang {

#ifndef ASSERT
#define ASSERT(c)
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Mutex::Mutex(const char *desc) {
#ifdef _WIN32
  // mutex = CreateMutex (NULL, false, NULL);
  InitializeCriticalSection(&criticalSection);
#endif

#ifdef _PTHREADS
  int ret = pthread_mutex_init(&mutex, NULL);
  ASSERT(ret == 0);
#endif

#ifdef SOLARIS_MT
  int ret = mutex_init(&mutex, USYNC_THREAD, NULL);
#endif

  holder = NULL;
  description = desc;
}

Mutex::~Mutex() {
#ifdef _WIN32
  // CloseHandle (mutex);
  DeleteCriticalSection(&criticalSection);
#endif

#ifdef _PTHREADS
  // int ret =
  pthread_mutex_destroy(&mutex);
#endif

#ifdef SOLARIS_MT
  int ret = mutex_destroy(&mutex);
#endif
}

void Mutex::lock() {
#ifdef _WIN32
  // int result = WaitForSingleObject (mutex, INFINITE);
  EnterCriticalSection(&criticalSection);
#endif

#ifdef _PTHREADS
  int ret = pthread_mutex_lock(&mutex);

  // The following code is added to get more information about why
  // the call to pthread_mutex_lock fails in some out-of-memory situations,
  // see bug 40155.

  if (ret != 0) {
    fprintf(stderr,
            "[Changjiang] Error: Mutex::lock: %s: pthread_mutex_lock returned "
            "errno %d\n",
            description, ret);
    fflush(stderr);
  }
  ASSERT(ret == 0);
#endif

#ifdef SOLARIS_MT
  int ret = mutex_lock(&mutex);
#endif
}

void Mutex::release() {
#ifdef _WIN32
  // ReleaseMutex (mutex);
  LeaveCriticalSection(&criticalSection);
#endif

#ifdef _PTHREADS
  int ret = pthread_mutex_unlock(&mutex);
  ASSERT(ret == 0);
#endif

#ifdef SOLARIS_MT
  int ret = mutex_unlock(&mutex);
#endif
}

void Mutex::unlock(Sync *sync, LockType type) {
  holder = NULL;
  release();
}

void Mutex::unlock() {
  holder = NULL;
  release();
}

void Mutex::lock(Sync *sync, LockType type, int timeout) {
  lock();
  holder = sync;
}

void Mutex::findLocks(LinkedList &threads, LinkedList &syncObjects) {}

}  // namespace Changjiang
