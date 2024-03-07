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

// Bitmap.h: interface for the Bitmap class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Interlock.h"

namespace Changjiang {

#define CLUMP_BITS 5
#define INDEX_BITS 5
#define VECTOR_BITS 7
#define BITS_PER_CLUMP 32
#define CLUMPS 32
#define BITS_PER_SEGMENT (CLUMPS * BITS_PER_CLUMP)
#define BITMAP_VECTOR_SIZE 128

typedef INTERLOCK_TYPE BitClump;

/* Bitmap Segment */

struct Bms {
  INTERLOCK_TYPE count;
  BitClump clump[CLUMPS];
};

class Bitmap {
 public:
  Bitmap();
  virtual ~Bitmap();

  void orSegments(Bms *segment1, Bms *segment2);
  void clear();
  void clear(int32 number);
  bool isSet(int32 number);
  void orBitmap(Bitmap *bitmap);
  void andBitmap(Bitmap *bitmap);
  int32 nextSet(int32 start);
  void set(int32 number);
  bool setSafe(int32 bitNumber);
  void release();
  void addRef();

  INTERLOCK_TYPE count;

  static void unitTest(void);
  static inline int32 pseudoRand(int32 seed, uint32 modulus) {
    return (int32)(((7177 * (int64)seed) + 1777) % (int64)modulus);
  }

 protected:
  void decompose(int32 number, uint *indexes);
  void **allocVector(void *firstItem);
  void deleteVector(int lvl, void **vector);
  void swap(Bitmap *bitmap);
  bool andSegments(Bms *segment1, Bms *segment2);

  int32 unary;
  int level;
  void **vector;
  int useCount;
};

}  // namespace Changjiang
