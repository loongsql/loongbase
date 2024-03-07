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

// StringTransform.h: interface for the StringTransform class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Transform.h"

namespace Changjiang {

class StringTransform : public Transform {
 public:
  virtual void reset();
  void setString(const char *string, bool copyFlag = false);
  virtual unsigned int get(unsigned int bufferLength, UCHAR *buffer);
  virtual unsigned int getLength();
  void setString(UIPTR length, const UCHAR *stuff, bool copyFlag = false);
  StringTransform(unsigned int length, const UCHAR *data,
                  bool copyFlag = false);
  StringTransform(const char *string, bool copyFlag = true);
  StringTransform();
  virtual ~StringTransform();

  const UCHAR *ptr;
  const UCHAR *end;
  UCHAR *data;
};

}  // namespace Changjiang
