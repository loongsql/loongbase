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

// NMatching.h: interface for the NMatching class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "NNode.h"

namespace Changjiang {

class Bitmap;
class NField;

class NMatching : public NNode {
 public:
  virtual void close(Statement *statement);
  virtual void prettyPrint(int level, PrettyPrint *pp);
  virtual bool computable(CompiledStatement *statement);
  virtual bool isInvertible(CompiledStatement *statement, Context *context);
  virtual int evalBoolean(Statement *statement);
  virtual Bitmap *evalInversion(Statement *statement);
  NMatching(CompiledStatement *statement, Syntax *syntax);
  virtual ~NMatching();

  NField *field;
  NNode *expr;
  int slot;
};

}  // namespace Changjiang
