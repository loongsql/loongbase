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

// EncodedRecord.cpp: implementation of the EncodedRecord class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "EncodedRecord.h"
#include "Stream.h"
#include "Table.h"
#include "Value.h"
#include "Transaction.h"

namespace Changjiang {

static const UCHAR lengthShifts[] = {0, 8, 16, 24, 32, 40, 48, 56};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

EncodedRecord::EncodedRecord(Table *tbl, Transaction *trans, Stream *stream)
    : EncodedDataStream(stream) {
  table = tbl;
  transaction = trans;
}

EncodedRecord::~EncodedRecord() {}

void EncodedRecord::encodeAsciiBlob(Value *value) {
  int32 val = table->getBlobId(value, 0, false, transaction->transactionState);
  EncodedDataStream::encodeAsciiBlob(val);

  /***
  int count = BYTE_COUNT(val);
  stream->putCharacter((char) (edsClobLen0 + count));

  while (--count >= 0)
          stream->putCharacter((char) (val >> (lengthShifts [count])));
  ***/
}

void EncodedRecord::encodeBinaryBlob(Value *value) {
  int32 val = table->getBlobId(value, 0, false, transaction->transactionState);
  EncodedDataStream::encodeBinaryBlob(val);

  /***
  int count = BYTE_COUNT(val);
  stream->putCharacter((char) (edsBlobLen0 + count));

  while (--count >= 0)
          stream->putCharacter((char) (val >> (lengthShifts [count])));
  ***/
}

}  // namespace Changjiang
