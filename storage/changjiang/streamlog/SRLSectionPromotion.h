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

#pragma once

#include "StreamLogRecord.h"

namespace Changjiang {

class SRLSectionPromotion : public StreamLogRecord {
 public:
  SRLSectionPromotion(void);
  virtual ~SRLSectionPromotion(void);

  void append(Dbb *dbb, int id, int32 rootPageNumber, int pageLength,
              const UCHAR *pageData, int32 pageNumber);
  virtual void read(void);
  virtual void pass1(void);
  virtual void pass2(void);
  virtual void redo(void);

  int sectionId;
  int length;
  int32 rootPageNumber;
  int32 pageNumber;
  const UCHAR *data;
  virtual void print(void);
};

}  // namespace Changjiang
