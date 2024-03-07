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

// RecordVersion.h: interface for the RecordVersion class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Record.h"

namespace Changjiang {

class Transaction;
class TransactionState;
class SyncObject;

class RecordVersion : public Record {
 public:
  RecordVersion(Table *tbl, Format *fmt, Transaction *tran, Record *oldVersion);
  RecordVersion(Database *database, Serialize *stream);

  virtual bool isSuperceded();
  // virtual Transaction* getTransaction();
  virtual TransactionState *getTransactionState() const;
  virtual TransId getTransactionId();
  virtual int getSavePointId();
  virtual void setSuperceded(bool flag);
  virtual Record *getPriorVersion();
  virtual Record *getGCPriorVersion(void);
  virtual void retire(void);
  virtual void scavengeSavepoint(Transaction *targetTransaction,
                                 int oldestActiveSavePoint);
  virtual bool isVersion();
  virtual void rollback(Transaction *transaction);
  virtual Record *fetchVersion(Transaction *trans);
  virtual Record *releaseNonRecursive();
  virtual Record *clearPriorVersion(void);
  virtual void setPriorVersion(Record *oldPriorVersion,
                               Record *newPriorVersion);
  virtual void setVirtualOffset(uint64 offset);
  virtual uint64 getVirtualOffset();
  virtual int thaw(void);
  virtual void print(void);
  virtual int getSize(void);
  virtual void serialize(Serialize *stream);
  virtual Transaction *findTransaction(void);

  void commit();
  bool committedBefore(TransId);
  void setTransactionState(TransactionState *newTransState);

 protected:
  virtual ~RecordVersion();
  Record *priorVersion;

 public:
  uint64 virtualOffset;  // byte offset into stream log window
  // Transaction		*transaction;
  RecordVersion *nextInTrans;
  RecordVersion *prevInTrans;
  // TransId			transactionId;
  int savePointId;
  bool superceded;

  // private:
  TransactionState *transactionState;
};

}  // namespace Changjiang
