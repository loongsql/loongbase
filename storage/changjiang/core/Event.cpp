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

// Event.cpp: implementation of the Event class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "Event.h"
#include "Sync.h"
#include "Thread.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Event::Event() : mutex("Event::mutex") {
  eventCount = 0;
  waiters = NULL;
}

Event::~Event() {}

bool Event::wait(int count) {
  if (isComplete(count)) return true;

  Thread *thread = Thread::getThread("Event::wait");
  Sync sync(&mutex, "Event::wait");
  sync.lock(Exclusive);
  thread->queue = (Thread *)waiters;
  waiters = thread;
  sync.unlock();

  for (;;) {
    if (thread->shutdownInProgress || isComplete(count)) {
      removeWaiter(thread);
      return !thread->shutdownInProgress;
    }

    thread->sleep();
  }
}

void Event::post() {
  INTERLOCKED_INCREMENT(eventCount);

  if (waiters) {
    Sync sync(&mutex, "Event::post");
    sync.lock(Exclusive);
    for (Thread *waiter = (Thread *)waiters; waiter; waiter = waiter->queue)
      waiter->wake();
  }
}

bool Event::isComplete(int count) {
  if (eventCount >= count) return true;

  if ((count - eventCount) > MAX_EVENT_COUNT / 2) return true;

  return false;
}

void Event::removeWaiter(Thread *thread) {
  if (!waiters) return;

  Sync sync(&mutex, "Event::removeWaiter");
  sync.lock(Exclusive);

  for (Thread **ptr = (Thread **)&waiters; *ptr; ptr = &(*ptr)->queue)
    if (*ptr == thread) {
      *ptr = thread->queue;
      return;
    }
}

}  // namespace Changjiang
