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

// Configuration.cpp: implementation of the Configuration class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

#ifndef _WIN32
#include <unistd.h>
#else
#define PATH_MAX 1024
#endif

#ifdef __FreeBSD__
#include <sys/types.h>
#endif

#if defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/sysctl.h>
#endif

#include "Engine.h"
#include "Configuration.h"
#include "MemoryManager.h"
#include "SQLError.h"
#include "Log.h"
#include "IOx.h"

namespace Changjiang {

#ifndef ULL
#define ULL(a) ((uint64)a)
#endif

#ifdef STORAGE_ENGINE
#define CONFIG_FILE "changjiang.conf"

#define PARAMETER_UINT(_name, _text, _min, _default, _max, _flags, \
                       _update_function)                           \
  extern uint changjiang_##_name;
#define PARAMETER_BOOL(_name, _text, _default, _flags, _update_function) \
  extern bool changjiang_##_name;
#include "StorageParameters.h"
#undef PARAMETER_UINT
#undef PARAMETER_BOOL

extern uint64 max_memory_address;

extern uint64 changjiang_record_memory_max;
extern uint changjiang_allocation_extent;
extern uint64 changjiang_page_cache_size;
// extern uint		changjiang_debug_mask;
extern char *changjiang_checkpoint_schedule;
extern char *changjiang_scavenge_schedule;
extern char *changjiang_stream_log_dir;

#else

#define CONFIG_FILE "netfraserver.conf"
#define PARAMETER_UINT(_name, _text, _min, _default, _max, _flags, \
                       _update_function)                           \
  uint changjiang_##_name = _default;
#define PARAMETER_BOOL(_name, _text, _default, _flags, _update_function) \
  bool changjiang_##_name = _default;
#include "StorageParameters.h"
#undef PARAMETER_UINT
#undef PARAMETER_BOOL

// Determine the largest memory address, assume 64-bits max

static const uint64 MSB = ULL(1) << ((sizeof(void *) * 8 - 1) & 63);
uint64 max_memory_address = MSB | (MSB - 1);
#endif

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

static const char RECORD_MEMORY_UPPER[] = "250mb";
static const char PAGE_CACHE_MEMORY[] = "4mb";
static const uint32 ONE_MB = 1024 * 1024;
static const uint64 MIN_PAGE_CACHE = 2097152;
static const uint64 MIN_RECORD_MEMORY = 5000000;
static const int MIN_SCAVENGE_THRESHOLD = 10;
static const int MIN_SCAVENGE_FLOOR = 10;
static const uint64 MAX_TRANSACTION_BACKLOG = 1000;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Configuration::Configuration(const char *configFile) {
  checkpointSchedule = "7,37 * * * * *";
  scavengeSchedule = "15,45 * * * * *";
  streamLogBlockSize = changjiang_stream_log_block_size;
  maxTransactionBacklog = MAX_TRANSACTION_BACKLOG;

#ifdef STORAGE_ENGINE
  recordMemoryMax = changjiang_record_memory_max;
  recordScavengeThresholdPct = changjiang_record_scavenge_threshold;
  recordScavengeFloorPct = changjiang_record_scavenge_floor;
  allocationExtent = changjiang_allocation_extent;
  streamLogWindows = changjiang_stream_log_buffers;
  pageCacheSize = changjiang_page_cache_size;
  indexChillThreshold = changjiang_index_chill_threshold;
  recordChillThreshold = changjiang_record_chill_threshold;
  useDeferredIndexHash = (changjiang_use_deferred_index_hash != 0);

  if (changjiang_checkpoint_schedule)
    checkpointSchedule = changjiang_checkpoint_schedule;

  if (changjiang_scavenge_schedule)
    scavengeSchedule = changjiang_scavenge_schedule;

  if (changjiang_stream_log_dir) {
    char fullPath[PATH_MAX];
    IO::expandFileName(changjiang_stream_log_dir, sizeof(fullPath), fullPath,
                       NULL);
    streamLogDir = fullPath;

    // Append path separator, if missing
    size_t len = strlen(fullPath);

    if (len && (fullPath[len - 1] != SEPARATOR)) streamLogDir += SEPARATOR;
  }
#else
  recordMemoryMax = getMemorySize(RECORD_MEMORY_UPPER);
  recordScavengeThresholdPct = 67;
  recordScavengeFloorPct = 33;
  recordScavengeThreshold =
      (recordMemoryMax * recordScavengeThresholdPct) / 100;
  recordScavengeFloor = (recordMemoryMax * recordScavengeFloorPct) / 100;
  streamLogWindows = 10;  // same as SRL_MIN_WINDOWS
  allocationExtent = 10;
  pageCacheSize = getMemorySize(PAGE_CACHE_MEMORY);
  indexChillThreshold = 4 * ONE_MB;
  recordChillThreshold = 5 * ONE_MB;
  changjiang_lock_wait_timeout = 0;
#endif

  maxMemoryAddress = max_memory_address;
  javaInitialAllocation = 0;
  javaSecondaryAllocation = 0;
  maxThreads = 0;
  schedulerEnabled = true;
  gcSchedule = "0,30 * * * * *";
  useCount = 1;

#ifndef STORAGE_ENGINE
  // Handle initialization file

  const char *fileName = (configFile) ? configFile : CONFIG_FILE;
  FILE *file = fopen(fileName, "r");

  if (!file) {
    if (configFile)
      throw SQLEXCEPTION(RUNTIME_ERROR, "can't open configuration file \"%s\"",
                         configFile);
  } else {
    char line[1024];

    while (getLine(file, sizeof(line), line)) {
      const char *p = line;

      while (*p == ' ' || *p == '\t') ++p;

      if (*p == '#') continue;

      const char *start = p;

      while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '=' &&
             *p != ':' && *p != '\r')
        ++p;

      if (p == start) continue;

      JString parameter(start, p - start);

      while (*p == ' ' || *p == '\t' || *p == '=' || *p == ':') ++p;

      start = p;

      while (*p && *p != '#' && *p != '\n' && *p != '\r') ++p;

      JString value = JString(start, p - start);

      if (parameter.equalsNoCase("classpath"))
        classpath = value;
      else if (parameter.equalsNoCase("min_record_memory"))
        recordScavengeFloor = getMemorySize(value);
      else if (parameter.equalsNoCase("max_record_memory"))
        recordMemoryMax = getMemorySize(value);
      else if (parameter.equalsNoCase("page_cache_size"))
        pageCacheSize = getMemorySize(value);
      else if (parameter.equalsNoCase("java_initial_allocation"))
        javaInitialAllocation = getMemorySize(value);
      else if (parameter.equalsNoCase("java_secondary_allocation"))
        javaSecondaryAllocation = getMemorySize(value);
      else if (parameter.equalsNoCase("gcSchedule"))
        gcSchedule = value;
      else if (parameter.equalsNoCase("scavengeSchedule"))
        scavengeSchedule = value;
      else if (parameter.equalsNoCase("checkpointSchedule"))
        checkpointSchedule = value;
      else if (parameter.equalsNoCase("max_threads"))
        maxThreads = atoi(value);
      else if (parameter.equalsNoCase("stream_log_block_size"))
        streamLogBlockSize = atoi(value);
      else if (parameter.equalsNoCase("scrub"))
        Log::scrubWords(value);
      else if (parameter.equalsNoCase("scheduler"))
        schedulerEnabled = enabled(value);
      else if (parameter.equalsNoCase("use_deferred_index_hash"))
        useDeferredIndexHash = (0 != atoi(value));
      else
        throw SQLEXCEPTION(DDL_ERROR, "unknown config parameter \"%s\"",
                           (const char *)parameter);
    }

    fclose(file);
  }
#endif

  pageCacheSize = MAX(pageCacheSize, MIN_PAGE_CACHE);
  setRecordMemoryMax(recordMemoryMax);

#ifdef STORAGE_ENGINE
  changjiang_page_cache_size = pageCacheSize;
#endif
}

Configuration::~Configuration() {}

bool Configuration::getLine(void *file, int length, char *line) {
  char *p = line;
  char *end = line + length - 1;
  char buffer[1024];

  for (;;) {
    if (!fgets(buffer, sizeof(buffer), (FILE *)file)) return false;

    const char *q = buffer;

    while (*q == ' ' || *q == '\t') ++q;

    while (q < end && *q && *q != '\n') *p++ = *q++;

    if (p[-1] != '\\') {
      *p = 0;

      return true;
    }

    --p;
  }
}

int64 Configuration::getMemorySize(const char *string) {
  int64 n = 0;

  for (const char *p = string; *p;) {
    char c = *p++;
    if (c >= '0' && c <= '9')
      n = n * 10 + c - '0';
    else if (c == 'g' || c == 'G')
      n *= 1024 * 1024 * 1024;
    else if (c == 'm' || c == 'M')
      n *= 1024 * 1024;
    else if (c == 'k' || c == 'K')
      n *= 1024;
  }

  return n;
}

uint64 Configuration::getPhysicalMemory(uint64 *available, uint64 *total) {
  uint64 availableMemory = 0;
  uint64 totalMemory = 0;

#ifdef _WIN32
  MEMORYSTATUSEX stat;
  DWORD error = 0;

  memset(&stat, 0, sizeof(stat));
  stat.dwLength = sizeof(stat);

  if (GlobalMemoryStatusEx(&stat) != 0) {
    availableMemory = stat.ullAvailPhys;
    totalMemory = stat.ullTotalPhys;
  } else
    error = GetLastError();

#elif defined(__APPLE__) || defined(__FreeBSD__)
  size_t availableMem = 0;
  size_t len = sizeof availableMem;
  static int mib[2] = {CTL_HW, HW_USERMEM};
  sysctl(mib, 2, &availableMem, &len, NULL, 0);

  // For physical RAM size on Apple we are using HW_MEMSIZE key,
  // because HW_PHYSMEM does not report correct RAM sizes above 2GB.

#ifdef __APPLE__
  uint64_t physMem = 0;
  mib[1] = HW_MEMSIZE;
#elif __FreeBSD__
  size_t physMem = 0;
  mib[1] = HW_PHYSMEM;
#endif

  len = sizeof physMem;
  sysctl(mib, 2, &physMem, &len, NULL, 0);

  availableMemory = (uint64)availableMem;
  totalMemory = (uint64)physMem;

#else
  int64 pageSize = (int64)sysconf(_SC_PAGESIZE);
  int64 physPages = (int64)sysconf(_SC_PHYS_PAGES);
  int64 avPhysPages = (int64)sysconf(_SC_AVPHYS_PAGES);

  if (pageSize > 0 && physPages > 0 && avPhysPages > 0) {
    availableMemory = (uint64)(pageSize * avPhysPages);
    totalMemory = (uint64)(pageSize * physPages);
  }
#endif

  if (available) *available = availableMemory;

  if (total) *total = totalMemory;

  return totalMemory;
}

void Configuration::addRef() { ++useCount; }

void Configuration::release() {
  if (--useCount == 0) delete this;
}

bool Configuration::enabled(JString string) {
  if (string.equalsNoCase("enabled") || string.equalsNoCase("yes") ||
      string.equalsNoCase("on"))
    return true;

  if (string.equalsNoCase("disabled") || string.equalsNoCase("no") ||
      string.equalsNoCase("off"))
    return false;

  throw SQLEXCEPTION(DDL_ERROR, "unknown enable value \"%s\"",
                     (const char *)string);
}

void Configuration::setRecordScavengeThreshold(int threshold) {
  recordScavengeThresholdPct = MAX(threshold, MIN_SCAVENGE_THRESHOLD);
  recordScavengeThreshold =
      (recordMemoryMax * recordScavengeThresholdPct) / 100;
  recordScavengeFloor =
      (recordScavengeThreshold * recordScavengeFloorPct) / 100;

#ifdef STORAGE_ENGINE
  changjiang_record_scavenge_threshold = recordScavengeThresholdPct;
#endif
}

void Configuration::setRecordScavengeFloor(int floor) {
  recordScavengeFloorPct = MAX(floor, MIN_SCAVENGE_FLOOR);
  recordScavengeFloor =
      (recordScavengeThreshold * recordScavengeFloorPct) / 100;

#ifdef STORAGE_ENGINE
  changjiang_record_scavenge_floor = recordScavengeFloorPct;
#endif
}

void Configuration::setRecordMemoryMax(uint64 value) {
  recordMemoryMax = MAX(value, MIN_RECORD_MEMORY);
  recordMemoryMax = MIN(value, maxMemoryAddress);

  setRecordScavengeThreshold(recordScavengeThresholdPct);

  MemMgrSetMaxRecordMember(recordMemoryMax);

#ifdef STORAGE_ENGINE
  changjiang_record_memory_max = recordMemoryMax;
#endif
}

}  // namespace Changjiang
