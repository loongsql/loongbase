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

// FilterTree.cpp: implementation of the FilterTree class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "FilterTree.h"
#include "InversionWord.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

FilterTree::FilterTree(InversionFilter *fltr1, InversionFilter *fltr2) {
  filter1 = fltr1;
  filter2 = fltr2;
}

FilterTree::~FilterTree() {
  delete filter1;
  delete filter2;
}

void FilterTree::start() {
  filter1->start();
  filter2->start();
  word1 = filter1->getWord();
  word2 = filter2->getWord();
}

InversionWord *FilterTree::getWord() {
  InversionWord *word;

  if (!word1) {
    if ((word = word2)) word2 = filter2->getWord();
  } else if (!word2 || word1->wordNumber < word2->wordNumber) {
    word = word1;
    word1 = filter1->getWord();
  } else {
    word = word2;
    word2 = filter2->getWord();
  }

  return word;
}

}  // namespace Changjiang
