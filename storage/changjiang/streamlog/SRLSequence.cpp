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

// SRLSequence.cpp: implementation of the SRLSequence class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLSequence.h"
#include "StreamLog.h"
#include "Database.h"
#include "Dbb.h"
#include "Log.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLSequence::SRLSequence() {}

SRLSequence::~SRLSequence() {}

void SRLSequence::read() {
  sequenceId = getInt();
  sequence = getInt64();
}

void SRLSequence::append(int sequenceId, int64 sequence) {
  START_RECORD(srlSequence, "SRLSequence::append");
  putInt(sequenceId);
  putInt64(sequence);
  sync.unlock();
}

void SRLSequence::redo() {
  log->defaultDbb->redoSequence(sequenceId, sequence);
}

void SRLSequence::print(void) {
  logPrint("Sequence id %d, value " I64FORMAT "\n", sequenceId, sequence);
}

}  // namespace Changjiang
