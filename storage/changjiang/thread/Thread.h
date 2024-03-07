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

// Thread.h: interface for the Thread class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#ifdef _WIN32
#define THREAD_ID unsigned long
#define THREAD_RET unsigned long
#else
#define _stdcall
#endif

#ifdef _PTHREADS
#include <pthread.h>
#define THREAD_ID pthread_t
#define THREAD_RET void *
#endif

#include "Synchronize.h"
#include "SynchronizationObject.h"

namespace Changjiang {

class Threads;
class Sync;
class SyncObject;
class SyncWait;
class LinkedList;
class JavaThread;
class CycleLock;

struct TimeZone;

class Thread : public Synchronize {
 public:
  Thread(const char *desc);
  // Thread(const char *desc, Threads *threads, void (*fn)(void*), void *arg,
  // Threads *barn);
  Thread(const char *desc, Threads *barn);
  virtual ~Thread();

  void setTimeZone(const TimeZone *timeZone);
  const char *getWhere();
  void print(const char *label);
  void print();
  void findLocks(LinkedList &threads, LinkedList &syncObjects);
  void clearLock(Sync *sync);
  void setLock(Sync *sync);
  void release();
  void addRef();
  void createThread(void (*fn)(void *), void *arg);
  // void			print(int level);
  void grantLock(SyncObject *lock);
  void init(const char *description);
  void start(const char *desc, void (*fn)(void *), void *arg);
  void thread();
  void setThreadBarn(Threads *newBarn);

  static THREAD_RET _stdcall thread(void *parameter);
  static void deleteThreadObject();
  static void validateLocks();
  static Thread *getThread(const char *desc);
  static Thread *findThread();

  static void lockExitMutex(void);
  static void unlockExitMutex(void);

  void *argument;
  void (*volatile function)(void *);
  void *threadHandle;

  THREAD_ID threadId;
  Threads *threadBarn;
  Thread *next;         // next thread in "thread barn"
  Thread *prior;        // next thread in "thread barn"
  Thread *queue;        // next thread in wait que (see SyncObject)
  Thread *srlQueue;     // stream log queue
  LockType lockType;    // requested lock type (see SyncObject)
  LockType wakeupType;  // used by StreamLog::flush

  volatile bool lockGranted;
  volatile bool licenseWakeup;
  volatile int32 activeLocks;
  Sync *locks;
  Sync *lockPending;
  bool marked;
  int pageMarks;
  int eventNumber;  // for debugging
  int random;
  int backoff;
  const char *description;
  const char *where;
  const TimeZone *defaultTimeZone;
  JavaThread *javaThread;
  CycleLock *cycleLock;
  uint64 commitBlockNumber;

 protected:
  static void setThread(Thread *thread);

  volatile INTERLOCK_TYPE useCount;
};

}  // namespace Changjiang
