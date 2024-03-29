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

// FilterTree.h: interface for the FilterTree class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "InversionFilter.h"

namespace Changjiang {

class InversionWord;

class FilterTree : public InversionFilter {
 public:
  virtual InversionWord *getWord();
  virtual void start();
  FilterTree(InversionFilter *filter1, InversionFilter *filter2);
  virtual ~FilterTree();

  InversionFilter *filter1;
  InversionFilter *filter2;
  InversionWord *word1;
  InversionWord *word2;
};

}  // namespace Changjiang
