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

// ThreadQueue.cpp: implementation of the ThreadQueue class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "ThreadQueue.h"
#include "Thread.h"

namespace Changjiang {

#ifndef ASSERT
#define ASSERT(arg)
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

ThreadQueue::ThreadQueue() {
  first = NULL;
  last = NULL;
}

ThreadQueue::~ThreadQueue() {}

void ThreadQueue::insert(Thread *thread) {
  thread->addRef();

  if ((thread->prior = last))
    last->next = thread;
  else {
    ASSERT(!first);
    first = thread;
  }

  thread->next = NULL;
  last = thread;
}

void ThreadQueue::remove(Thread *thread) {
  if (thread->prior)
    thread->prior->next = thread->next;
  else {
    ASSERT(first == thread);
    first = thread->next;
  }

  if (thread->next)
    thread->next->prior = thread->prior;
  else {
    ASSERT(last == thread);
    last = thread->prior;
  }

  thread->next = NULL;
  thread->prior = NULL;
  thread->release();
}

bool ThreadQueue::isMember(Thread *candidate) {
  for (Thread *thread = first; thread; thread = thread->next)
    if (thread == candidate) return true;

  return false;
}

}  // namespace Changjiang
