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

// FsbJoin.cpp: implementation of the FsbJoin class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include <stdio.h>
#include "Engine.h"
#include "FsbJoin.h"
#include "Statement.h"
#include "CompiledStatement.h"
#include "Log.h"
#include "PrettyPrint.h"

namespace Changjiang {

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

FsbJoin::FsbJoin(CompiledStatement *statement, int numberStreams,
                 Row *rowSource) {
  count = numberStreams;
  streams = new Fsb *[count];
  memset(streams, 0, sizeof(Fsb *) * count);
  slot = statement->getGeneralSlot();
  row = rowSource;
}

FsbJoin::~FsbJoin() {
  if (streams) {
    for (int n = 0; n < count; ++n)
      if (streams[n]) delete streams[n];

    delete[] streams;
  }
}

void FsbJoin::setStream(int index, Fsb *stream) { streams[index] = stream; }

void FsbJoin::open(Statement *statement) {
  statement->slots[slot] = 0;
  streams[0]->open(statement);
}

Row *FsbJoin::fetch(Statement *statement) {
  int n = statement->slots[slot];

  for (;;) {
    if (streams[n]->fetch(statement)) {
      if (n == count - 1) {
        statement->slots[slot] = n;

        return row;
      }

      streams[++n]->open(statement);
    } else {
      if (n == 0) {
        statement->slots[slot] = n;
        close(statement);

        return NULL;
      }

      streams[n--]->close(statement);
    }
  }
}

void FsbJoin::close(Statement *statement) {
  for (int n = 0; n < count; ++n) streams[n]->close(statement);
}

void FsbJoin::getStreams(int **ptr) {
  for (int n = 0; n < count; ++n) streams[n]->getStreams(ptr);
}

void FsbJoin::prettyPrint(int level, PrettyPrint *pp) {
  pp->indent(level++);
  pp->put(getType());

  for (int n = 0; n < count; ++n) streams[n]->prettyPrint(level, pp);
}

const char *FsbJoin::getType() { return "Join\n"; }

}  // namespace Changjiang
