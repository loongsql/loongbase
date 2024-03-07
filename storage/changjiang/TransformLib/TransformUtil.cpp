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

// TransformUtil.cpp: implementation of the TransformUtil class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include "Transform.h"
#include "TransformUtil.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

JString TransformUtil::getString(Transform *transform) {
  JString string;
  int len = transform->getLength();
  char *p = string.getBuffer(len);
  len = transform->get(len, (UCHAR *)p);
  p[len] = 0;

  return string;
}

bool TransformUtil::compareDigests(Transform *transform1,
                                   Transform *transform2) {
  UCHAR hash1[20], hash2[20];
  transform1->get(sizeof(hash1), hash1);
  transform2->get(sizeof(hash2), hash2);

  return memcmp(hash1, hash2, sizeof(hash2)) == 0;
}

}  // namespace Changjiang
