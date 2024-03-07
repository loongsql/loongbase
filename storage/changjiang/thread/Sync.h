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

// Sync.h: interface for the Sync class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "SynchronizationObject.h"

namespace Changjiang {

class LinkedList;

class Sync {
 public:
  Sync(SynchronizationObject *obj, const char *location);
  virtual ~Sync();

  void print(const char *label);
  void findLocks(LinkedList &threads, LinkedList &syncObjects);
  // void	lock (LockType type, const char *fromWhere);
  // void	print(int level);
  void setObject(SynchronizationObject *obj);
  void unlock();
  void lock(LockType type);
  void lock(LockType type, int timeout);

  SynchronizationObject *syncObject;
  LockType state;
  LockType request;
  Sync *prior;
  const char *location;
  const char *marked;
  void mark(const char *text);
};

}  // namespace Changjiang
