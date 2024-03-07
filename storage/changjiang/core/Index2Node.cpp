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

// Index2Node.cpp: implementation of the Index2Node class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include "Engine.h"
#include "Index2Node.h"
#include "Index2Page.h"
#include "IndexKey.h"
#include "Btn.h"

namespace Changjiang {

#define RECORD_NUMBER_SHIFT(recordNumber) \
  ((recordNumber < SHIFT(7))    ? 0       \
   : (recordNumber < SHIFT(14)) ? 7       \
   : (recordNumber < SHIFT(21)) ? 14      \
   : (recordNumber < SHIFT(28)) ? 21      \
                                : 28)

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Index2Node::Index2Node() {}

Index2Node::Index2Node(Index2Page *page) { parseNode(page->nodes); }

Index2Node::Index2Node(Btn *node) { parseNode(node); }

void Index2Node::printKey(const char *msg, UCHAR *key, bool inversion) {
  node->printKey("", offset + length, key, offset, inversion);
}

Btn *Index2Node::insert(Btn *where, int offst, int len, UCHAR *fullKey,
                        int32 recordNumber) {
  node = where;
  key = (UCHAR *)node;

  if ((offset = offst) >= 128) *key++ = (UCHAR)((offset >> 7) | 0x80);

  *key++ = (UCHAR)offset;

  if ((length = len) >= 128) *key++ = (UCHAR)((length >> 7) | 0x80);

  *key++ = (UCHAR)length;

  memcpy(key, &recordNumber, sizeof(recordNumber));
  key += sizeof(recordNumber);

  memcpy(key, fullKey + offset, length);
  nextNode = (Btn *)(key + length);

  return nextNode;
}

}  // namespace Changjiang
