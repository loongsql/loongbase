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

// Index2Node.h: interface for the Index2Node class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <memory.h>
#include "Btn.h"

namespace Changjiang {

//#define RECORD_NUMBER_LENGTH(recordNumber)	sizeof(int32)

class Index2Page;

class Index2Node {
 public:
  inline void expandKey(IndexKey *indexKey);
  Index2Node();
  Index2Node(Index2Page *page);
  Index2Node(Btn *node);

  Btn *insert(Btn *where, int offst, int len, UCHAR *fullKey, int32 nmbr);
  void printKey(const char *msg, UCHAR *key, bool inversion);
  int expandKey(UCHAR *keyPtr);
  Btn *getNext();
  int32 getNumber();
  Btn *parseNode(Btn *indexNode);

  inline int keyLength() { return offset + length; }

  inline static int nodeLength(int offset, int length, int32 number) {
    return 2 + length + sizeof(int32);
  }

  inline int getNumberLength() { return sizeof(int32); }

  Btn *node;
  Btn *nextNode;
  uint offset;
  uint length;
  UCHAR *key;
  UCHAR *numberPtr;
};

inline Btn *Index2Node::parseNode(Btn *indexNode) {
  node = indexNode;
  key = (UCHAR *)indexNode;
  offset = *key++;
  length = *key++;
  numberPtr = key;
  key += sizeof(int32);
  nextNode = (Btn *)(key + length);

  return nextNode;
}

inline int32 Index2Node::getNumber() {
  int32 number;
  memcpy(&number, numberPtr, sizeof(number));

  return number;
}

inline Btn *Index2Node::getNext() { return parseNode(nextNode); }

inline int Index2Node::expandKey(UCHAR *keyPtr) {
  ASSERT(offset + length <= MAX_PHYSICAL_KEY_LENGTH);

  if (length) memcpy(keyPtr + offset, key, length);

  return offset + length;
}

inline void Index2Node::expandKey(IndexKey *indexKey) {
  ASSERT(offset + length <= MAX_PHYSICAL_KEY_LENGTH);

  if (length) memcpy(indexKey->key + offset, key, length);

  indexKey->keyLength = offset + length;
}

}  // namespace Changjiang
