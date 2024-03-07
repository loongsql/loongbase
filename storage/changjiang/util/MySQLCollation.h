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

#include "Collation.h"

namespace Changjiang {

// Entrypoints in the Changjiang storage handler to collation handling

extern int changjiang_strnxfrm(void *cs, const char *dst, uint dstlen,
                               int nweights, const char *src, uint srclen);

extern int changjiang_strnxfrm_space_pad(void *cs, const char *dst, uint dstlen,
                                         int nweights, const char *src,
                                         uint srclen);

extern char changjiang_get_pad_char(void *cs);
extern int changjiang_cs_is_binary(void *cs);
extern unsigned int changjiang_get_mbmaxlen(void *cs);
extern char changjiang_get_min_sort_char(void *cs);
extern uint changjiang_strnchrlen(void *cs, const char *s, uint l);
extern uint changjiang_strnxfrmlen(void *cs, const char *s, uint srclen,
                                   int partialKey, int bufSize);
extern uint changjiang_strntrunc(void *cs, int partialKey, const char *s,
                                 uint l);
extern int changjiang_strnncoll(void *cs, const char *s1, uint l1,
                                const char *s2, uint l2, char flag);
extern int changjiang_strnncollsp(void *cs, const char *s1, uint l1,
                                  const char *s2, uint l2, char flag);

class MySQLCollation : public Collation {
 public:
  MySQLCollation(JString collationName, void *arg);
  ~MySQLCollation(void);

  virtual int compare(Value *value1, Value *value2);
  virtual int makeKey(Value *value, IndexKey *key, int partialKey,
                      int maxKeyLength, bool highKey);
  virtual const char *getName();
  virtual bool starting(const char *string1, const char *string2);
  virtual bool like(const char *string, const char *pattern);
  virtual char getPadChar(void);
  virtual int truncate(Value *value, int partialLength);

  JString name;
  void *charset;
  char padChar;
  bool isBinary;
  uint mbMaxLen;
  char minSortChar;

  static inline uint computeKeyLength(uint length, const char *key,
                                      char padChar, char minSortChar) {
    for (const char *p = key + length; p > key; --p)
      if ((p[-1] != 0) && (p[-1] != padChar) && (p[-1] != minSortChar))
        return (uint)(p - key);

    return 0;
  }
};

}  // namespace Changjiang
