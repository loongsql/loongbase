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

// Configuration.h: interface for the Configuration class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

namespace Changjiang {

class Configuration {
 public:
  Configuration(const char *fileName);
  virtual ~Configuration();

  bool enabled(JString string);
  void release();
  void addRef();
  int64 getMemorySize(const char *string);
  uint64 getPhysicalMemory(uint64 *available = NULL, uint64 *total = NULL);
  bool getLine(void *file, int length, char *line);
  void setRecordScavengeThreshold(int threshold);
  void setRecordScavengeFloor(int floor);
  void setRecordMemoryMax(uint64 value);

  JString classpath;
  JString gcSchedule;
  JString scavengeSchedule;
  JString checkpointSchedule;
  uint64 recordMemoryMax;
  uint64 recordScavengeThreshold;
  uint64 recordScavengeFloor;
  int recordScavengeThresholdPct;
  int recordScavengeFloorPct;
  uint64 allocationExtent;
  uint64 pageCacheSize;
  int64 javaInitialAllocation;
  int64 javaSecondaryAllocation;
  uint streamLogWindows;
  JString streamLogDir;
  uint indexChillThreshold;
  uint recordChillThreshold;
  int useCount;
  int maxThreads;
  uint maxTransactionBacklog;
  short streamLogBlockSize;
  bool schedulerEnabled;
  bool useDeferredIndexHash;
  uint64 maxMemoryAddress;
};

}  // namespace Changjiang
