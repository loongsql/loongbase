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

#pragma once

// The Changjiang SyncHandler tracks the usage and call stacks of all
// SyncObjects. It allows you to find potential deadlocks in various call stacks
// Note: it serializes all locks and unlocks, so only use it for analysis.

namespace Changjiang {

// Uncomment this to use the SyncHandler
//#define USE_CHANGJIANG_SYNC_HANDLER

#define FOR_HASH_ITEMS(_itemType, _item, _hashBuckets, _hashSize) \
  for (int _slot1 = 0; _slot1 < _hashSize; _slot1++)              \
    for (_itemType _item = _hashBuckets[_slot1]; _item;           \
         _item = _item->collision)

#define FOR_HASH_ITEMS_2(_itemType2, _item2, _hashBuckets2, _hashSize2) \
  for (int _slot2 = 0; _slot2 < _hashSize2; _slot2++)                   \
    for (_itemType2 _item2 = _hashBuckets2[_slot2]; _item2;             \
         _item2 = _item2->collision)

#define FOR_HASH_ITEMS_TO_DELETE(_itemType, _item, _hashBuckets, _hashSize) \
  for (int _slot = 0; _slot < _hashSize; ++_slot)                           \
    for (_itemType _item; (_item = _hashBuckets[_slot]);)

static const int syncObjHashSize = 101;
static const int locationHashSize = 503;
static const int threadHashSize = 100;
static const int stackHashSize = 1000;
static const int deadlockHashSize = 100;
static const int beforeAfterSize = 60;
static const int syncStackSize = 20;

struct SyncObjectInfo {
  JString name;
  SyncObjectInfo *prev;
  SyncObjectInfo *next;
  SyncObjectInfo *collision;
  SyncObjectInfo *beforeList[beforeAfterSize];
  SyncObjectInfo *afterList[beforeAfterSize];
  SyncObjectInfo *before;
  SyncObjectInfo *after;
  bool multiple;
};

struct SyncLocationInfo {
  JString name;
  SyncObjectInfo *soi;
  LockType type;
  SyncLocationInfo *collision;
};

struct SyncThreadInfo {
  int Id;
  int height;
  bool isStackRecorded;
  SyncLocationInfo *stack[syncStackSize];
  SyncThreadInfo *collision;
};

struct LocationStackInfo {
  SyncLocationInfo *loc[syncStackSize];
  int height;
  int hash;
  int count;
  bool hasRisingLockTypes;
  LocationStackInfo *collision;
};

struct DeadlockInfo {
  SyncObjectInfo *soi[2];
  int hash;
  bool isPossible;
  DeadlockInfo *collision;
};

class SyncHandler;
extern "C" {
SyncHandler *getChangjiangSyncHandler(void);
SyncHandler *findChangjiangSyncHandler(void);
}

class SyncHandler {
 public:
  SyncHandler(void);
  virtual ~SyncHandler(void);

  void addLock(SyncObject *syncObj, const char *locationName, LockType type);
  void delLock(SyncObject *syncObj);
  void dump(void);

#ifdef USE_CHANGJIANG_SYNC_HANDLER

 private:
  SyncThreadInfo *findThread();
  SyncThreadInfo *findThread(int thdId, int slot);
  SyncThreadInfo *addThread(int thdId);
  SyncObjectInfo *findSyncObject(const char *syncObjectName);
  SyncObjectInfo *findSyncObject(const char *syncObjectName, int slot);
  SyncObjectInfo *addSyncObject(const char *syncObjectName);
  void showSyncObjects(void);
  SyncLocationInfo *findLocation(const char *locationName, LockType type,
                                 int slot);
  SyncLocationInfo *addLocation(const char *locationName, SyncObjectInfo *soi,
                                LockType type);
  void showLocations(void);
  LocationStackInfo *findStack(LocationStackInfo *stk, int slot);
  void addStack(SyncThreadInfo *thd);
  void showLocationStacks(void);
  void showLocationStack(int stackNum);
  void showLocationStack(LocationStackInfo *lsi);
  void countLocationStacks(void);

  DeadlockInfo *findDeadlock(DeadlockInfo *dli, int slot);

  void addToThread(SyncThreadInfo *thd, SyncLocationInfo *loc);
  void delFromThread(SyncThreadInfo *thd, SyncObjectInfo *soi);

  void validate(void);
  void addPossibleDeadlock(SyncObjectInfo *soi1, SyncObjectInfo *soi2);
  void removePossibleDeadlock(DeadlockInfo *dli);
  void showPossibleDeadlockStacks(void);
  void showPossibleDeadlockStack(DeadlockInfo *dli, int showOrder);

  SyncObject syncObject;

  SyncObjectInfo **syncObjects;
  SyncLocationInfo **locations;
  SyncThreadInfo **threads;
  LocationStackInfo **locationStacks;
  DeadlockInfo **possibleDeadlocks;

  int syncObjCount;
  int locationCount;
  int threadCount;
  int locationStackCount;
  int possibleDeadlockCount;
#endif
};

}  // namespace Changjiang
