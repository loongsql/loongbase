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

// Sort.cpp: implementation of the Sort class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "Sort.h"
#include "SortRecord.h"
#include "SortRun.h"
#include "SortMerge.h"
#include "Stack.h"

namespace Changjiang {

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Sort::Sort(SortParameters *sortParameters, int runLen, bool flushDups) {
  runLength = runLen;
  run = new SortRun(runLength);
  flushDuplicates = flushDups;
  parameters = sortParameters;
  merge = NULL;
}

Sort::~Sort() {
  while (run) {
    SortRun *prior = run;
    run = (SortRun *)prior->next;
    delete prior;
  }

  if (merge) delete merge;
}

void Sort::add(SortRecord *record) {
  if (!run->add(record)) {
    run->sort(parameters, flushDuplicates);
    SortRun *newRun = new SortRun(runLength);
    newRun->next = run;
    run = newRun;

    if (!run->add(record)) ASSERT(false);
  }
}

void Sort::sort() {
  run->sort(parameters, flushDuplicates);
  prior = NULL;
  merge = run;
  run = NULL;

  // Build binary merge tree from runs

  while (merge->next) {
    SortStream *streams = NULL;

    while (merge)
      if (merge->next) {
        SortStream *stream = new SortMerge(parameters, merge, merge->next);
        stream->next = streams;
        streams = stream;
        merge = merge->next->next;
      } else {
        merge->next = streams;
        streams = merge;
        break;
      }

    merge = streams;
  }

  merge->prepare();
}

SortRecord *Sort::fetch() {
  for (;;) {
    SortRecord *record = merge->fetch();

    if (!record) return record;

    if (!flushDuplicates || prior == NULL ||
        record->compare(parameters, prior) != 0) {
      prior = record;
      return record;
    }
  }
}

void Sort::setDescending(int key) {
  parameters[key].direction = true;
  // direction [key] = true;
}

}  // namespace Changjiang
