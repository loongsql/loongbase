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

// FsbJoin.h: interface for the FsbJoin class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Fsb.h"

namespace Changjiang {

class CompiledStatement;

class FsbJoin : public Fsb {
 public:
  virtual const char *getType();
  virtual void prettyPrint(int level, PrettyPrint *pp);
  virtual void getStreams(int **ptr);
  virtual void close(Statement *statement);
  virtual Row *fetch(Statement *statement);
  virtual void open(Statement *statement);
  void setStream(int index, Fsb *stream);
  FsbJoin(CompiledStatement *statement, int numberStreams, Row *rowSource);
  virtual ~FsbJoin();

  int count;
  int slot;
  Fsb **streams;
  Row *row;
};

}  // namespace Changjiang
