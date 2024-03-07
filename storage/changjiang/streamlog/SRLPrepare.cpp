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

// SRLPrepare.cpp: implementation of the SRLPrepare class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLPrepare.h"
#include "StreamLog.h"
#include "StreamLogControl.h"
#include "StreamLogTransaction.h"
#include "SRLVersion.h"
#include "Sync.h"
#include "Thread.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLPrepare::SRLPrepare() {}

SRLPrepare::~SRLPrepare() {}

void SRLPrepare::append(TransId transId, int xidLength, const UCHAR *xid) {
  START_RECORD(srlPrepare, "SRLPrepare::append");
  putInt(transId);
  putInt(xidLength);
  putData(xidLength, xid);
  StreamLogTransaction *srlTransaction = log->getTransaction(transId);

  log->flush(false, log->nextBlockNumber, &sync);

  if (srlTransaction) srlTransaction->setState(sltPrepared);
}

void SRLPrepare::read() {
  transactionId = getInt();

  if (control->version >= srlVersion7) {
    xidLength = getInt();
    xid = getData(xidLength);
  } else
    xidLength = 0;
}

void SRLPrepare::pass1() {
  StreamLogTransaction *transaction = control->getTransaction(transactionId);
  transaction->setState(sltPrepared);

  if (xidLength) transaction->setXID(xidLength, xid);
}

void SRLPrepare::print() {
  logPrint("Prepare Transaction %ld\n", transactionId);
}

void SRLPrepare::commit() {
  StreamLogTransaction *srlTransaction = log->findTransaction(transactionId);
  srlTransaction->setFinished();
}

void SRLPrepare::rollback() {
  StreamLogTransaction *srlTransaction = log->findTransaction(transactionId);
  srlTransaction->setFinished();
}

}  // namespace Changjiang
