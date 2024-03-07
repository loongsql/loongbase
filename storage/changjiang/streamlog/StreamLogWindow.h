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

// StreamLogWindow.h: interface for the StreamLogWindow class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "SyncObject.h"

namespace Changjiang {

class StreamLogFile;
class StreamLog;
struct StreamLogBlock;

class StreamLogWindow {
 public:
  StreamLogWindow(StreamLog *streamLog, StreamLogFile *logFile,
                  int64 logOrigin);
  virtual ~StreamLogWindow();

  void deactivateWindow();
  void activateWindow(bool read);
  void setBuffer(UCHAR *newBuffer);
  int64 getNextFileOffset();
  int64 getNextVirtualOffset(void);
  StreamLogBlock *nextAvailableBlock(StreamLogBlock *block);
  StreamLogBlock *nextBlock(StreamLogBlock *block);
  StreamLogBlock *findBlock(uint64 blockNumber);
  StreamLogBlock *findLastBlock(StreamLogBlock *block);
  StreamLogBlock *readFirstBlock();
  void setPosition(StreamLogFile *logFile, int64 logOrigin);
  void addRef(void);
  void release(void);
  void setLastBlock(StreamLogBlock *block);
  uint64 getVirtualOffset();
  void write(StreamLogBlock *block);
  void print(void);
  bool validate(StreamLogBlock *block);
  bool validate(const UCHAR *pointer);
  void setInterest(void);
  void clearInterest(void);

  inline StreamLogBlock *firstBlock() { return (StreamLogBlock *)buffer; }

  StreamLogWindow *next;
  StreamLogWindow *prior;
  StreamLogFile *file;
  StreamLogBlock *lastBlock;
  StreamLog *log;
  UCHAR *buffer;
  UCHAR *bufferEnd;
  UCHAR *warningTrack;
  int64 origin;
  uint32 currentLength;
  uint32 bufferLength;
  uint32 sectorSize;
  uint64 firstBlockNumber;
  uint64 lastBlockNumber;
  int lastBlockOffset;
  int inUse;
  int useCount;
  uint64 virtualOffset;
  volatile INTERLOCK_TYPE interestCount;
};

}  // namespace Changjiang
