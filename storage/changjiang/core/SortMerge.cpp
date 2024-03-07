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

// SortMerge.cpp: implementation of the SortMerge class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "SortMerge.h"
#include "SortRecord.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SortMerge::SortMerge(SortParameters *sortParameters, SortStream *strm1,
                     SortStream *strm2) {
  parameters = sortParameters;
  stream1 = strm1;
  stream2 = strm2;
  record1 = NULL;
  record2 = NULL;
  next = NULL;
}

SortMerge::~SortMerge() {
  if (stream1) delete stream1;

  if (stream2) delete stream2;
}

SortRecord *SortMerge::fetch() {
  SortRecord *record;

  if (!record1) {
    if ((record = record2)) record2 = stream2->fetch();
  } else if (!record2 || record1->compare(parameters, record2) < 0) {
    record = record1;
    record1 = stream1->fetch();
  } else {
    record = record2;
    record2 = stream2->fetch();
  }

  return record;
}

void SortMerge::prepare() {
  stream1->prepare();
  stream2->prepare();
  record1 = stream1->fetch();
  record2 = stream2->fetch();
}

}  // namespace Changjiang
