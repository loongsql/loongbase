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

// FsbSieve.cpp: implementation of the FsbSieve class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "FsbSieve.h"
#include "NNode.h"
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

FsbSieve::FsbSieve(NNode *node, Fsb *source) {
  boolean = node;
  stream = source;
}

FsbSieve::~FsbSieve() { delete stream; }

void FsbSieve::open(Statement *statement) { stream->open(statement); }

Row *FsbSieve::fetch(Statement *statement) {
  for (;;) {
    Row *row = stream->fetch(statement);

    if (!row) return NULL;

    int result = boolean->evalBoolean(statement);

    if (result && result != NULL_BOOLEAN) return row;
  }
}

void FsbSieve::close(Statement *statement) {
  boolean->close(statement);
  stream->close(statement);
}

void FsbSieve::getStreams(int **ptr) { stream->getStreams(ptr); }

void FsbSieve::prettyPrint(int level, PrettyPrint *pp) {
  pp->indent(level++);
  pp->put("Boolean sieve\n");
  boolean->prettyPrint(level, pp);
  stream->prettyPrint(level, pp);
}

}  // namespace Changjiang
