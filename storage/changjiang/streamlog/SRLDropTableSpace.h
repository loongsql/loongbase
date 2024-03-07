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

// SRLDropTableSpace.h: interface for the SRLDropTableSpace class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "StreamLogRecord.h"

namespace Changjiang {

class TableSpace;
class Transaction;

class SRLDropTableSpace : public StreamLogRecord {
 public:
  SRLDropTableSpace();
  virtual ~SRLDropTableSpace();

  virtual void redo();
  virtual void commit();
  virtual void pass2();
  virtual void pass1();
  virtual void read();
  void append(TableSpace *tableSpace, Transaction *transaction);
};

}  // namespace Changjiang
