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

// SyncObject.h: interface for the SyncObject class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "SynchronizationObject.h"

#if !defined(INTERLOCK_TYPE) && defined(_WIN32)
#define INTERLOCK_TYPE long
#elif !defined(INTERLOCK_TYPE)
#define INTERLOCK_TYPE int
#endif

#ifdef POSIX_THREADS
#include <pthread.h>
#endif

#ifdef SOLARIS_MT
#include <sys/mutex.h>
#include <thread.h>
#endif

#include "Mutex.h"

namespace Changjiang {

#define TRACE_SYNC_OBJECTS

class SyncObject;
class Sync;
class Thread;
class LinkedList;
class Stream;
class InfoTable;

class SyncObject : public SynchronizationObject {
 public:
  SyncObject();
  virtual ~SyncObject();

  void print();
  void stalled(Thread *thread);
  void printEvents(int level);
  void postEvent(Thread *thread, const char *what, Thread *granting);
  void downGrade(LockType type);
  bool isLocked();
  void sysServiceFailed(const char *server, int code);
  void bumpWaiters(int delta);
  void grantLocks(void);
  // void		assertionFailed(void);
  int getState(void);
  int getCollisionCount(void);
  void validate(LockType lockType);
  void unlock(void);
  bool ourExclusiveLock(void);
  void frequentStaller(Thread *thread, Sync *sync);
  void setName(const char *name);
  const char *getName(void);
  const char *getLocation(void);
  void timedout(int timeout);
  void backoff(Thread *thread);

  virtual void unlock(Sync *sync, LockType type);
  virtual void lock(Sync *sync, LockType type, int timeout = 0);
  virtual void findLocks(LinkedList &threads, LinkedList &syncObjects);

  static void analyze(Stream *stream);
  static void getSyncInfo(InfoTable *infoTable);
  static void dump(void);

  inline Thread *getExclusiveThread() { return exclusiveThread; };

 protected:
  void wait(LockType type, Thread *thread, Sync *sync, int timeout);

  int32 monitorCount;
  Mutex mutex;
  Thread *volatile queue;
  Thread *volatile exclusiveThread;
  volatile INTERLOCK_TYPE readers;
  volatile INTERLOCK_TYPE waiters;
  volatile INTERLOCK_TYPE lockState;
  int stalls;

#ifdef TRACE_SYNC_OBJECTS
  int objectId;
  INTERLOCK_TYPE sharedCount;
  INTERLOCK_TYPE collisionCount;
  int exclusiveCount;
  int waitCount;
  int queueLength;
  const char *location;
  const char *name;
#endif
};

}  // namespace Changjiang
