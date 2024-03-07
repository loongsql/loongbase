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

#include "Engine.h"
#include "CollationUnknown.h"
#include "CollationManager.h"
#include "SQLError.h"

namespace Changjiang {

CollationUnknown::CollationUnknown(CollationManager *collationManager,
                                   const char *collationName) {
  name = collationName;
  manager = collationManager;
  collation = NULL;
}

CollationUnknown::~CollationUnknown(void) {
  if (collation) collation->release();
}

int CollationUnknown::compare(Value *value1, Value *value2) {
  if (!collation) getCollation();

  return collation->compare(value1, value2);
}

int CollationUnknown::makeKey(Value *value, IndexKey *key, int partialKey,
                              int maxKeyLength, bool highKey) {
  if (!collation) getCollation();

  return collation->makeKey(value, key, partialKey, maxKeyLength, highKey);
}

const char *CollationUnknown::getName() { return name; }

bool CollationUnknown::starting(const char *string1, const char *string2) {
  if (!collation) getCollation();

  return collation->starting(string1, string2);
}

bool CollationUnknown::like(const char *string, const char *pattern) {
  if (!collation) getCollation();

  return collation->like(string, pattern);
}

void CollationUnknown::getCollation(void) {
  if (!collation) {
    Collation *col = manager->find(name);

    if (!col || col == this)
      throw SQLError(DDL_ERROR, "unknown collation type \"%s\"",
                     (const char *)name);

    collation = col;
  }
}

char CollationUnknown::getPadChar(void) {
  if (!collation) getCollation();

  return collation->getPadChar();
}

int CollationUnknown::truncate(Value *value, int partialLength) {
  if (!collation) getCollation();

  return collation->truncate(value, partialLength);
}

}  // namespace Changjiang
