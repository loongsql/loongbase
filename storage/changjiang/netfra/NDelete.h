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

// NDelete.h: interface for the NDelete class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "NNode.h"
#include "Row.h"
//#include "Value.h"

namespace Changjiang {

class Fsb;

class NDelete : public NNode, public Row {
 public:
  NDelete(CompiledStatement *statement, Syntax *syntax, NType nType);
  void compile(CompiledStatement *statement, Syntax *syntax);
  // Value* getValue (Statement *statement, int index);
  bool references(Table *table);
  virtual void evalStatement(Statement *statement);
  NDelete(CompiledStatement *statement, Syntax *syntax);
  virtual ~NDelete();

  int contextId;
  Fsb *stream;
  Table *table;
  const char *cursorName;
  // Value		nullValue;
};

}  // namespace Changjiang
