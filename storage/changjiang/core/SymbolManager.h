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

// SymbolManager.h: interface for the SymbolManager class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "SyncObject.h"

namespace Changjiang {

#define SYMBOL_HASH_SIZE 503

struct Symbol {
  Symbol *collision;
  char symbol[1];
};

struct SymbolSection {
  SymbolSection *next;
  char space[5000];
};

class SymbolManager {
 public:
  const char *getString(const char *string);
  const char *findString(const char *string);
  const char *getSymbol(const WCString *string);
  bool isSymbol(const char *string);
  const char *getSymbol(const char *string);
  const char *findSymbol(const char *string);
  SymbolManager();
  virtual ~SymbolManager();

  SymbolSection *sections;
  Symbol *hashTable[SYMBOL_HASH_SIZE];
  char *next;
  SyncObject syncObject;
};

}  // namespace Changjiang
