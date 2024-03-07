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

// FsbSort.h: interface for the FsbSort class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Fsb.h"

namespace Changjiang {

enum SortType {
  order,     // Sort with explicit sort order
  distinct,  // Get rid of duplicates
  simple,    // Just sort; don't worry about direction (group by)
};

class Sort;
class CompiledStatement;
class Collation;
struct SortParameters;

class FsbSort : public Fsb {
 public:
  virtual void prettyPrint(int level, PrettyPrint *pp);
  virtual void getStreams(int **ptr);
  virtual void close(Statement *statement);
  virtual Row *fetch(Statement *statement);
  virtual void open(Statement *statement);
  FsbSort(CompiledStatement *statement, NNode *expr, Fsb *source,
          SortType type);
  virtual ~FsbSort();

  Fsb *stream;
  NNode *node;
  SortType type;
  SortParameters *parameters;
  Collation **collations;
  int sortSlot;
  int numberKeys;
  int numberContexts;
  int contextIds[32];
};

}  // namespace Changjiang
