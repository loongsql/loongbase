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

// SearchHit.cpp: implementation of the SearchHit class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SearchHit.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SearchHit::SearchHit() { hits = 0; }

/***
SearchHit::~SearchHit()
{

}
***/

SearchHit::SearchHit(int32 table, int32 record, double goodness) {
  tableId = table;
  recordNumber = record;
  score = goodness;
  hits = 1;
  wordScore = 0;
}

void SearchHit::setWordMask(int wordMask) {
  int count = 0;

  for (int mask = 1; wordMask; mask <<= 1)
    if (wordMask & mask) {
      ++count;
      wordMask &= ~mask;
    }

  double boost = (count - wordScore) * 20;
  // printf ("hit %d:%d:\t%g + %g -> %g\n", tableId, recordNumber, score, boost,
  // boost + score);
  score += boost;
  wordScore = count;
}

}  // namespace Changjiang
