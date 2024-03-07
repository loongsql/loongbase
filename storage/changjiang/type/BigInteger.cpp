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

// BigInteger.cpp: implementation of the BigInteger class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "BigInteger.h"

namespace Changjiang {

#define REF(r) (*(int64 *)(r.bytes))

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

int BigInteger::getResultLength(BigOp op, BigInteger a, BigInteger b) {
  return 8;
}

void BigInteger::add(BigInteger a, BigInteger b, BigInteger result) {
  REF(result) = REF(a) + REF(b);
}

void BigInteger::subtract(BigInteger a, BigInteger b, BigInteger result) {
  REF(result) = REF(a) - REF(b);
}

void BigInteger::multiply(BigInteger a, BigInteger b, BigInteger result) {
  REF(result) = REF(a) * REF(b);
}

void BigInteger::divide(BigInteger a, BigInteger b, BigInteger result) {
  REF(result) = REF(a) / REF(b);
}

void BigInteger::remainder(BigInteger a, BigInteger b, BigInteger result) {
  REF(result) = REF(a) % REF(b);
}

void BigInteger::gcd(BigInteger a, BigInteger b, BigInteger result) {
  NOT_YET_IMPLEMENTED;
}

void BigInteger::generatePrime(BigInteger a, BigInteger result) {
  NOT_YET_IMPLEMENTED;
}

void BigInteger::modInversion(BigInteger a, BigInteger b, BigInteger result) {
  NOT_YET_IMPLEMENTED;
}

}  // namespace Changjiang
