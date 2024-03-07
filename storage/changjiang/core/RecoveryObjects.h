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

// RecoveryObjects.h: interface for the RecoveryObjects class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

static const int RPG_HASH_SIZE = 1999;

#include "SyncObject.h"

namespace Changjiang {

class RecoveryPage;
class StreamLog;

class RecoveryObjects {
 public:
  RecoveryObjects(StreamLog *streamLog);
  virtual ~RecoveryObjects();

  bool isObjectActive(int objectNumber, int tableSpaceId);
  void reset();
  bool bumpIncarnation(int objectNumber, int tableSpaceId, int state,
                       bool pass1);
  void clear();
  RecoveryPage *findInHashBucket(RecoveryPage *head, int objectNumber,
                                 int tableSpaceId);
  RecoveryPage *findRecoveryObject(int objectNumber, int tableSpaceId);
  void setActive(int objectNumber, int tableSpaceId);
  void setInactive(int objectNumber, int tableSpaceId);
  RecoveryPage *getRecoveryObject(int objectNumber, int tableSpaceId);
  void deleteObject(int objectNumber, int tableSpaceId);
  int getCurrentState(int objectNumber, int tableSpaceId);

  StreamLog *streamLog;
  RecoveryPage *recoveryObjects[RPG_HASH_SIZE];
  SyncObject syncArray[RPG_HASH_SIZE];
};

}  // namespace Changjiang
