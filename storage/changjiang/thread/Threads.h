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

// Threads.h: interface for the Threads class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "SyncObject.h"
#include "Synchronize.h"
#include "ThreadQueue.h"
#ifdef _PTHREADS
#include <sys/prctl.h>
#endif

namespace Changjiang {

class Thread;

struct ThreadPending {
  void (*fn)(void *);
  void *arg;
  const char *description;
  ThreadPending *next;
  ThreadPending *prior;
};

class Threads : public Synchronize {
 public:
  Threads(Threads *parentThreads, int maximumThreads = 0);
  virtual bool shutdown(Thread *thread);
  void checkInactive(Thread *thread);
  void printThreads();
  void waitForAll();
  void clear();
  void shutdownAll();
  void exitting(Thread *thread);
  Thread *start(const char *desc, void(fn)(void *), void *arg);
  Thread *start(const char *desc, void(fn)(void *), void *arg,
                Threads *threadBarn);
  Thread *startWhenever(const char *desc, void(fn)(void *), void *arg);
  Thread *startWhenever(const char *desc, void(fn)(void *), void *arg,
                        Threads *threadBarn);
  void release();
  void addRef();
  void print();
  void leave(Thread *thread);
  void enter(Thread *thread);
  void inactive(Thread *thread);

 protected:
  virtual ~Threads();
  SyncObject syncObject;
  ThreadQueue activeThreads;
  ThreadQueue inactiveThreads;
  Threads *parent;
  volatile INTERLOCK_TYPE useCount;
  ThreadPending *firstPending;
  ThreadPending *lastPending;
  int maxThreads;
  int threadsActive;
};

}  // namespace Changjiang
