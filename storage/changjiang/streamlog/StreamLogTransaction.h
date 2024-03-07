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

// StreamLogTransaction.h: interface for the StreamLogTransaction class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

//#include "StreamLogAction.h"

namespace Changjiang {

enum sltState { sltUnknown, sltPrepared, sltCommitted, sltRolledBack };

class StreamLog;
class StreamLogWindow;
class Transaction;
class Bitmap;
struct StreamLogBlock;

class StreamLogTransaction  //: public StreamLogAction
{
 public:
  virtual uint64 getBlockNumber();
  virtual bool completedRecovery();
  virtual void preRecovery();
  virtual void doAction();
  virtual bool isRipe();
  virtual bool isXidEqual(int testLength, const UCHAR *test);

  void setFinished();
  void setState(sltState newState);
  void setStart(const UCHAR *record, StreamLogBlock *blk, StreamLogWindow *win);
  void rollback();
  void commit();
  void setPhysicalBlock();
  void setXID(int xidLength, const UCHAR *xidPtr);

  StreamLogTransaction(StreamLog *streamLog, TransId transId);
  virtual ~StreamLogTransaction();

  StreamLogTransaction *collision;
  TransId transactionId;
  volatile sltState state;
  StreamLogWindow *window;
  Transaction *transaction;
  uint64 blockNumber;
  int blockOffset;
  int recordOffset;
  int xidLength;
  UCHAR *xid;
  bool finished;
  bool allowConcurrentPorpoises;

  StreamLog *log;
  StreamLogTransaction *next;
  StreamLogTransaction *prior;
  StreamLogTransaction *earlier;
  StreamLogTransaction *later;
  Bitmap *rolledBackSavepoints;
  bool flushing;
  bool ordered;
  uint64 physicalBlockNumber;
  uint64 minBlockNumber;
  uint64 maxBlockNumber;

  void setTransaction(Transaction *transaction);
  void savepointRolledBack(int savepointId);
  bool isRolledBackSavepoint(int savepointId);
};

}  // namespace Changjiang
