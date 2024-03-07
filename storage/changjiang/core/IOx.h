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

// IO.h: interface for the IO class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "JString.h"
#include "SyncObject.h"

// Because of struct stat, this function can't be in name space.
int fileStat_cj(const char *fileName, struct stat *stats = NULL,
                int *errnum = NULL);

namespace Changjiang {

static const int WRITE_TYPE_FORCE = 0;
static const int WRITE_TYPE_REUSE = 1;
static const int WRITE_TYPE_SHUTDOWN = 2;
static const int WRITE_TYPE_PAGE_WRITER = 3;
static const int WRITE_TYPE_CLONE = 4;
static const int WRITE_TYPE_FLUSH = 5;
static const int WRITE_TYPE_MAX = 6;

static const uint16 NO_CHECKSUM_MAGIC = 0;

class Bdb;
class Hdr;
class Dbb;
class Page;

class IO {
 public:
  IO();
  ~IO();

  bool trialRead(Bdb *bdb);
  void deleteFile();
  void writeHeader(Hdr *header);
  int read(int length, UCHAR *buffer);
  void write(uint32 length, const UCHAR *data);
  static bool doesFileExist(const char *fileName);
  // static int	fileStat(const char *fileName, struct stat *stats = NULL, int
  // *errnum = NULL);
  void declareFatalError();
  void seek(int pageNumber);
  void closeFile();
  void readHeader(Hdr *header);
  void writePage(Bdb *buffer, int type);
  void writePages(int32 pageNumber, int length, const UCHAR *data, int type);
  void readPage(Bdb *page);
  bool createFile(const char *name);
  static void setBaseDirectory(const char *path);
  static void setWriteFlags(int fileId, bool *forceFsync);
  bool openFile(const char *name, bool readOnly);
  void longSeek(int64 offset);
  void read(int64 offset, int length, UCHAR *buffer);
  void write(int64 offset, int length, const UCHAR *buffer);
  int pread(int64 offset, int length, UCHAR *buffer);
  int pwrite(int64 offset, int length, const UCHAR *buffer);
  void sync(void);
  void reportWrites(void);

  void tracePage(Bdb *bdb);
  void traceOperation(int operation);
  static void trace(int fd, int pageNumber, int pageType, int pageId);
  static void traceOpen(void);
  static void traceClose(void);
  static uint16 computeChecksum(Page *page, size_t pageSize);
  void validateChecksum(Page *page, size_t pageSize, int64 fileOffset);
  static void createPath(const char *fileName);
  static const char *baseName(const char *path);
  static void expandFileName(const char *fileName, int length, char *buffer,
                             const char **baseFileName = NULL);
  static void deleteFile(const char *fileName);
  static int getWriteMode(int attempt);

  JString fileName;
  SyncObject syncObject;
  int fileId;
  int pageSize;
  uint reads;
  uint writes;
  uint flushWrites;
  uint writesSinceSync;
  uint fetches;
  uint fakes;
  uint priorReads;
  uint priorWrites;
  uint priorFlushWrites;
  uint priorFetches;
  uint priorFakes;
  uint writeTypes[WRITE_TYPE_MAX];
  bool fatalError;
  bool isReadOnly;
  bool forceFsync;
  bool created;

  // private:
  Dbb *dbb;  // this is a crock and should be phased out
};
extern bool deleteFilesOnExit;
extern bool inCreateDatabase;

}  // namespace Changjiang
