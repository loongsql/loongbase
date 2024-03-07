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
// SRLCreateIndex.h: interface for the SRLCreateIndex class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "StreamLogRecord.h"

namespace Changjiang {

class SRLCreateIndex : public StreamLogRecord {
 public:
  SRLCreateIndex();
  virtual ~SRLCreateIndex();

  void append(Dbb *dbb, TransId transId, int32 indexId, int indexVersion,
              int pageNumber);
  virtual void pass1();
  virtual void pass2();
  virtual void print();
  virtual void redo();
  virtual void read();
  virtual void commit(void);

  int32 indexId;
  int indexVersion;
  int32 pageNumber;
};

}  // namespace Changjiang
