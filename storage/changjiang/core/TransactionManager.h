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

#include "SyncObject.h"
#include "Queue.h"

namespace Changjiang {

class Transaction;
class Database;
class Connection;
class Table;

class TransactionManager {
 public:
  TransactionManager(Database *database);
  ~TransactionManager(void);

  Transaction *startTransaction(Connection *connection);
  void dropTable(Table *table, Transaction *transaction);
  void truncateTable(Table *table, Transaction *transaction);
  bool hasUncommittedRecords(Table *table, Transaction *transaction);
  void waitForWriteComplete(Table *table);
  void commitByXid(int xidLength, const UCHAR *xid);
  void rollbackByXid(int xidLength, const UCHAR *xid);
  void print(void);
  TransId findOldestInActiveList() const;
  void getTransactionInfo(InfoTable *infoTable);
  void purgeTransactions();
  void purgeTransactionsWithLocks();
  void getSummaryInfo(InfoTable *infoTable);
  void reportStatistics(void);
  Transaction *findTransaction(TransId transactionId);
  void removeCommittedTransaction(Transaction *transaction);
  void removeTransaction(Transaction *transaction);
  void printBlockage(void);
  void printBlocking(Transaction *transaction, int level);

  INTERLOCK_TYPE transactionSequence;
  Database *database;
  SyncObject syncObject;
  Transaction *rolledBackTransaction;
  int committed;
  int rolledBack;
  int priorCommitted;
  int priorRolledBack;
  Queue<Transaction> activeTransactions;
  Queue<Transaction> committedTransactions;
};

}  // namespace Changjiang
