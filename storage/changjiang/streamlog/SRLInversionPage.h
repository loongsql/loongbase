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

// SRLInversionPage.h: interface for the SRLInversionPage class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "StreamLogRecord.h"

namespace Changjiang {

class SRLInversionPage : public StreamLogRecord {
 public:
  virtual void print();
  virtual void redo();
  virtual void read();
  virtual void pass2();
  virtual void pass1();
  void append(Dbb *dbb, int32 page, int32 up, int32 left, int32 right,
              int length, const UCHAR *data);
  SRLInversionPage();
  virtual ~SRLInversionPage();

  int32 pageNumber;
  int32 parent;
  int32 prior;
  int32 next;
  int32 level;
  int32 length;
  const UCHAR *data;
};

}  // namespace Changjiang
