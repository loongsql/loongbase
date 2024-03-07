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

// SortRecord.cpp: implementation of the SortRecord class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "SortRecord.h"
#include "Value.h"
#include "Record.h"
#include "Collation.h"
#include "Statement.h"

namespace Changjiang {

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SortRecord::SortRecord(Statement *statement, Row *sourceRow, int keyCount,
                       int recordCount, Record **obj) {
  numberKeys = keyCount;
  numberRecords = recordCount;
  object = obj;
  row = sourceRow;

  if (numberRecords)
    keys = new Value[numberKeys];
  else {
    int numberValues = row->getNumberValues();
    keys = new Value[numberKeys + numberValues];

    for (int n = 0; n < numberValues; ++n)
      keys[numberKeys + n].setValue(row->getValue(statement, n), true);

    row = this;
  }
}

SortRecord::~SortRecord() {
  for (int n = 0; n < numberRecords; ++n) {
    Record *record = object[n];
    if (record) record->release(REC_HISTORY);
  }

  delete[] keys;
  delete[] object;
}

int SortRecord::compare(SortParameters *sortParameters, SortRecord *record) {
  for (int n = 0; n < numberKeys; ++n) {
    SortParameters *parameters = sortParameters + n;
    int c = (parameters->collation)
                ? parameters->collation->compare(keys + n, record->keys + n)
                : keys[n].compare(&record->keys[n]);
    /***
    Debug.print ("   compare " + keys [n] + " to " +
                             record.keys [n] + " giving " + c);
    ***/
    if (c)
      // return (direction) [n] ? -c : c;
      return (parameters->direction) ? -c : c;
  }

  return 0;
}

Value *SortRecord::getValue(Statement *statement, int index) {
  return keys + numberKeys + index;
}

}  // namespace Changjiang
