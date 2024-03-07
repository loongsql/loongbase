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

#include <memory.h>
#include "Engine.h"
#include "PriorityScheduler.h"
#include "Sync.h"
#include "Thread.h"

namespace Changjiang {

PriorityScheduler::PriorityScheduler(void) : mutex("PriorityScheduler::mutex") {
  currentPriority = 0;
  count = 0;
  memset(waitingThreads, 0, sizeof(waitingThreads));
}

PriorityScheduler::~PriorityScheduler(void) {}

void PriorityScheduler::schedule(int priority) {
  Sync sync(&mutex, "PriorityScheduler::schedule");
  sync.lock(Exclusive);

  if (priority == currentPriority) {
    ++count;

    return;
  }

  if (priority > currentPriority) {
    currentPriority = priority;
    count = 1;

    return;
  }

  Thread *thread = Thread::getThread("PriorityScheduler::schedule");
  thread->queue = waitingThreads[priority];
  waitingThreads[priority] = thread;
  thread->wakeupType = None;
  sync.unlock();

  while (thread->wakeupType == None) thread->sleep();
}

void PriorityScheduler::finished(int priority) {
  Sync sync(&mutex, "PriorityScheduler::finished");
  sync.lock(Exclusive);

  // If this is below the current priority level, ignore it

  if (priority < currentPriority) return;

  // If there are other processes at this priority level, just decrement the
  // count

  if (--count > 0) return;

  for (currentPriority = PRIORITY_MAX - 1; currentPriority > 0;
       --currentPriority)
    if (waitingThreads[currentPriority]) {
      count = 0;

      for (Thread *thread; (thread = waitingThreads[currentPriority]);) {
        ++count;
        waitingThreads[currentPriority] = thread->queue;
        thread->wakeupType = Exclusive;
        thread->wake();
      }

      break;
    }
}

}  // namespace Changjiang
