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

namespace Changjiang {

class BigInt;

class ScaledBinary {
 public:
  ScaledBinary(void);
  ~ScaledBinary(void);
  static uint getByte(int position, const char *ptr);
  static uint getBinaryGroup(int start, int bytes, const char *ptr);
  static int numberBytes(int digits);
  static int numberBytes(int precision, int fractions);
  static void putBinaryNumber(int64 number, int digits, char **ptr,
                              bool isFraction);
  static void putBinaryGroup(uint number, int digits, char **ptr);
  static void getBigNumber(int start, int digits, const char *ptr,
                           bool isFraction, BigInt *bigInt);
  static int64 getBinaryNumber(int start, int digits, const char *ptr,
                               bool isFraction);

  static int64 getInt64FromBinaryDecimal(const char *ptr, int precision,
                                         int scale);
  static void getBigIntFromBinaryDecimal(const char *ptr, int precision,
                                         int scale, BigInt *bigInt);

  static void putBigInt(BigInt *bigInt, char *ptr, int precision, int scale);
  static void putBinaryDecimal(int64 number, char *ptr, int precision,
                               int scale);
  static int64 scaleInt64(int64 number, int delta);
};

}  // namespace Changjiang
