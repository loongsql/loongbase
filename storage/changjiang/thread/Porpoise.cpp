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

#include "Engine.h"
#include "Porpoise.h"
#include "StreamLog.h"
#include "Sync.h"
#include "Thread.h"
#include "Threads.h"
#include "StreamLogTransaction.h"
#include "Database.h"

namespace Changjiang {

Porpoise::Porpoise(StreamLog *streamLog) {
  log = streamLog;
  workerThread = NULL;
}

Porpoise::~Porpoise(void) {}

void Porpoise::porpoiseThread(void *arg) {
#ifdef _PTHREADS
  prctl(PR_SET_NAME, "cj_porpoise");
#endif
  ((Porpoise *)arg)->porpoiseThread();
}

void Porpoise::porpoiseThread(void) {
  Sync deadMan(&log->syncPorpoise, "Porpoise::porpoiseThread(1)");
  deadMan.lock(Shared);
  workerThread = Thread::getThread("Porpoise::porpoiseThread");
  active = true;
  Sync syncPending(&log->pending.syncObject, "Porpoise::porpoiseThread(2)");
  syncPending.lock(Exclusive);

  while (!workerThread->shutdownInProgress && !log->finishing) {
    if (!log->pending.first || !log->pending.first->isRipe()) {
      if (log->blocking) log->unblockUpdates();

      syncPending.unlock();
      active = false;
      workerThread->sleep();
      active = true;
      syncPending.lock(Exclusive);

      continue;
    }

    StreamLogTransaction *transaction = log->pending.first;
    log->pending.remove(transaction);

    setConcurrency(&syncPending, transaction->allowConcurrentPorpoises);
    syncPending.unlock();

    transaction->doAction();

    syncPending.lock(Exclusive);
    releaseConcurrency(&syncPending, transaction->allowConcurrentPorpoises);

    log->inactions.append(transaction);

    if (log->pending.count > log->maxTransactions && !log->blocking)
      log->blockUpdates();
  }

  active = false;
  workerThread = NULL;
}

void Porpoise::setConcurrency(Sync *syncPending,
                              bool allowConcurrentPorpoises) {
  // Assume that syncPending is locked exclusively.

  if (allowConcurrentPorpoises) {
    while (log->serializePorpoises < 0) {
      syncPending->unlock();
      workerThread->sleep(10);
      syncPending->lock(Exclusive);
    }

    log->serializePorpoises++;
  } else {
    log->wantToSerializePorpoises++;
    while (log->serializePorpoises) {
      syncPending->unlock();
      workerThread->sleep(10);
      syncPending->lock(Exclusive);
    }

    log->serializePorpoises = -1;
  }
}

void Porpoise::releaseConcurrency(Sync *syncPending,
                                  bool allowConcurrentPorpoises) {
  if (allowConcurrentPorpoises) {
    ASSERT(log->serializePorpoises > 0);
    log->serializePorpoises--;
  } else {
    ASSERT(log->serializePorpoises == -1);
    log->wantToSerializePorpoises--;
    log->serializePorpoises = 0;
  }

  // If there is another thread that needs to serialize the porpoises,
  // wait here until it is done.

  while (log->wantToSerializePorpoises) {
    syncPending->unlock();
    workerThread->sleep(10);
    syncPending->lock(Exclusive);
  }
}

void Porpoise::start(void) {
  log->database->threads->start("StreamLog::start", porpoiseThread, this);
}

void Porpoise::shutdown(void) {
  if (workerThread) workerThread->shutdown();
}

void Porpoise::wakeup(void) {
  if (workerThread) workerThread->wake();
}

}  // namespace Changjiang
