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

// SortRun.cpp: implementation of the SortRun class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "SortRun.h"
#include "SortRecord.h"

namespace Changjiang {

#define STACK_SIZE 512

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SortRun::SortRun(int len) {
  size = 0;
  length = len;
  records = new SortRecord *[length];
  n = 0;
  next = NULL;
}

SortRun::~SortRun() {
  for (int n = 0; n < size; ++n) delete records[n];

  delete[] records;
}

bool SortRun::add(SortRecord *record) {
  if (size >= length) return false;

  records[size++] = record;

  return true;
}

SortRecord *SortRun::fetch() {
  if (n >= size) return NULL;

  return records[n++];
}

void SortRun::sort(SortParameters *sortParameters, bool flushDuplicate) {
  int i, j, r, stack = 0;
  int low[STACK_SIZE];
  int high[STACK_SIZE];
  SortRecord *temp;

  if (size > 2) {
    low[stack] = 0;
    high[stack++] = size - 1;
  }

  while (stack > 0) {
    SortRecord *key;
    /***
    if (++iteration > 10)
            {
            Debug.print ("punting");
            return;
            }
    ***/
    r = low[--stack];
    j = high[stack];
    // Debug.print ("sifting " + r + " thru " + j + ": " + range (r, j));
    key = records[r];
    // Debug.print (" key", key);
    int limit = j;
    i = r + 1;

    for (;;) {
      while (records[i]->compare(sortParameters, key) <= 0 && i < limit) ++i;

      while (records[j]->compare(sortParameters, key) >= 0 && j > r) --j;

      // Debug.print ("  i " + i, records [i]);
      // Debug.print ("  j " + j, records [j]);

      if (i >= j) break;

      temp = records[i];
      records[i] = records[j];
      records[j] = temp;
    }

    i = high[stack];
    records[r] = records[j];
    records[j] = key;
    // Debug.print (" midpoint " + j + ": " +  range (r, i));

    if ((j - 1) - r >= 2) {
      low[stack] = r;
      ASSERT(stack < STACK_SIZE);
      high[stack++] = j - 1;
    }

    if (i - (j + 1) >= 2) {
      low[stack] = j + 1;
      ASSERT(stack < STACK_SIZE);
      high[stack++] = i;
    }
  }

  for (int n = 1; n < size; ++n)
    if (records[n - 1]->compare(sortParameters, records[n]) > 0) {
      // Debug.print ("Flipping");
      temp = records[n - 1];
      records[n - 1] = records[n];
      records[n] = temp;
    }
}

void SortRun::prepare() {}

}  // namespace Changjiang
