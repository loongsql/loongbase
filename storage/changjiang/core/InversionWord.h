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

// InversionWord.h: interface for the InversionWord class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

namespace Changjiang {

#define MAX_INV_KEY 128
#define MAX_INV_WORD 100

class InversionWord {
 public:
  bool isEqual(InversionWord *word2);
  int makeKey(UCHAR *key);
  InversionWord();
  virtual ~InversionWord();

  char word[MAX_INV_KEY];
  ULONG tableId;
  ULONG fieldId;
  ULONG recordNumber;
  ULONG wordNumber;
  int wordLength;
};

}  // namespace Changjiang
