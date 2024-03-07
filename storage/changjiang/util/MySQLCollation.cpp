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
#include "MySQLCollation.h"
#include "IndexKey.h"
#include "Value.h"

namespace Changjiang {

MySQLCollation::MySQLCollation(JString collationName, void *arg) {
  name = collationName;
  charset = arg;
  padChar = changjiang_get_pad_char(charset);
  isBinary = (0 != changjiang_cs_is_binary(charset));
  mbMaxLen = changjiang_get_mbmaxlen(charset);
  minSortChar = changjiang_get_min_sort_char(charset);
}

MySQLCollation::~MySQLCollation(void) {}

int MySQLCollation::compare(Value *value1, Value *value2) {
  const char *string1 = value1->getString();
  const char *string2 = value2->getString();
  uint len1 = value1->getStringLength();
  uint len2 = value2->getStringLength();

  // If the string is not BINARY, truncate the string of all 0x00, padChar, and
  // minSortChar

  if (!isBinary) {
    len1 = computeKeyLength(len1, string1, padChar, minSortChar);
    len2 = computeKeyLength(len2, string2, padChar, minSortChar);
  }

  return changjiang_strnncoll(charset, string1, len1, string2, len2, false);
}

int MySQLCollation::makeKey(Value *value, IndexKey *key, int partialKey,
                            int maxCharLength, bool highKey) {
  // Use max char length rather than binary length.
  int maxKeyLength = maxCharLength * changjiang_get_mbmaxlen(charset);

  if (partialKey > maxKeyLength) partialKey = maxKeyLength;

  char temp[MAX_PHYSICAL_KEY_LENGTH];
  int srcLen;

  if (partialKey) {
    srcLen = value->getTruncatedString(partialKey, temp);
    srcLen = changjiang_strntrunc(charset, partialKey, temp, srcLen);
  } else
    srcLen = value->getString(sizeof(temp), temp);

  if (!isBinary) srcLen = computeKeyLength(srcLen, temp, padChar, minSortChar);

  int len = 0;

  // If this is a highKey, append the pad char if the final
  // character is >= the pad char. There is no efficient way
  // of finding and checking the final character for every
  // character set, but it should be correct to pad the rest
  // of the key with the pad character anyway. This is done
  // when creating an upper bound search key to make it position
  // after all values with trailing characters lower than the
  // pad character.

  if (highKey)
    len = changjiang_strnxfrm_space_pad(
        charset, (char *)key->key, maxKeyLength,
        partialKey ? partialKey / changjiang_get_mbmaxlen(charset)
                   : maxCharLength,
        temp, srcLen);
  else
    len = changjiang_strnxfrm(
        charset, (char *)key->key, maxKeyLength,
        partialKey ? partialKey / changjiang_get_mbmaxlen(charset)
                   : maxCharLength,
        temp, srcLen);

  ASSERT(len <= maxKeyLength);
  key->keyLength = len;

  return len;
}

const char *MySQLCollation::getName() { return name; }

bool MySQLCollation::starting(const char *string1, const char *string2) {
  NOT_YET_IMPLEMENTED;
  return false;
}

bool MySQLCollation::like(const char *string, const char *pattern) {
  NOT_YET_IMPLEMENTED;
  return false;
}

char MySQLCollation::getPadChar(void) { return padChar; }

int MySQLCollation::truncate(Value *value, int partialLength) {
  const char *string = value->getString();
  int len = value->getStringLength();

  if (changjiang_cs_is_binary(charset))
    len = MIN(len, partialLength);
  else
    len = changjiang_strntrunc(charset, partialLength, string, len);

  value->truncateString(len);

  return len;
}

}  // namespace Changjiang
