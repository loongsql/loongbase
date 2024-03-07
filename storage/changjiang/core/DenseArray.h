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

#include <memory.h>

namespace Changjiang {

template <typename T, uint width>
class DenseArray {
#ifdef _DEBUG
#undef THIS_FILE
  const char *THIS_FILE;
#endif

 public:
  DenseArray() {
#ifdef _DEBUG
    THIS_FILE = __FILE__;
#endif

    length = width;
    vector = new T[length];
    zap();
  };

  ~DenseArray() { delete[] vector; };

  void extend(uint newLength) {
    if (newLength < length) return;

    T *oldVector = vector;
    vector = new T[newLength];
    memcpy((void *)vector, (void *)oldVector, length * sizeof(T));
    memset((void *)(vector + length), 0, (newLength - length) * sizeof(T));
    length = newLength;
  };

  void zap() { memset((void *)vector, 0, length * sizeof(T)); };

  T get(uint index) {
    if (index >= length) return 0;

    return vector[index];
  };

  T *vector;
  uint length;
};

}  // namespace Changjiang
