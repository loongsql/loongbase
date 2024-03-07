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

// Fsb.cpp: implementation of the Fsb class.
//
//////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

#include <stdio.h>
#include "Engine.h"
#include "Fsb.h"
#include "Log.h"
#include "Stream.h"
#include "NNode.h"

namespace Changjiang {
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Fsb::Fsb() {}

Fsb::~Fsb() {}

void Fsb::open(Statement *statement) {}

Row *Fsb::fetch(Statement *statement) { return NULL; }

void Fsb::close(Statement *statement) {}

void Fsb::getStreams(int **ptr) {}

void Fsb::prettyPrint(int level, PrettyPrint *pp) {}

int Fsb::getStreamIndex(Statement *statement) { return 0; }

}  // namespace Changjiang
