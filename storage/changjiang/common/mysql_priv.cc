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

#include "mysql_priv.h"
#include "tzfile.h"

#include "mysql/psi/mysql_memory.h"

namespace Changjiang {

void my_hash_free(HASH *tree) {
  for (std::unordered_map<std::string, void *>::iterator iter =
           tree->htmap.begin();
       iter != tree->htmap.end(); iter++) {
    if (tree->free) (*tree->free)((uchar *)iter->second);
  }
  tree->htmap.clear();
  tree->records = 0;
  tree->free = 0;
  tree->blength = 0;
}

uchar *my_hash_search(HASH *info, const uchar *key, size_t length) {
  return (uchar *)(info->htmap[std::string((const char *)key)]);
}

bool my_hash_insert(HASH *info, const uchar *data) {
  size_t length;
  info->htmap[std::string((const char *)(info->get_key(data, &length, 0)))] =
      (void *)data;
  return 0;
}

bool my_hash_delete(HASH *hash, uchar *record) {
  size_t length;
  hash->htmap[std::string((const char *)(hash->get_key(record, &length, 0)))] =
      NULL;
  if (hash->free) (*hash->free)((uchar *)record);
  return 0;
}

bool _my_hash_init(HASH *hash, uint growth_size, CHARSET_INFO *charset,
                   ulong default_array_elements, size_t key_offset,
                   size_t key_length, my_hash_get_key get_key,
                   void (*free_element)(void *), uint flags CALLER_INFO_PROTO) {
  hash->records = 0;
  hash->key_offset = key_offset;
  hash->key_length = key_length;
  hash->blength = 1;
  hash->get_key = get_key;
  hash->free = free_element;
  hash->flags = flags;
  hash->charset = charset;
  return 0;
}

static const uint mon_starts[2][MONS_PER_YEAR] = {
    {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334},
    {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335}};

#define LEAPS_THRU_END_OF(y) ((y) / 4 - (y) / 100 + (y) / 400)

my_time_t sec_since_epoch_TIME(MYSQL_TIME *t) {
  long days = t->year * DAYS_PER_NYEAR - EPOCH_YEAR * DAYS_PER_NYEAR +
              LEAPS_THRU_END_OF(t->year - 1) -
              LEAPS_THRU_END_OF(EPOCH_YEAR - 1);
  days += mon_starts[isleap(t->year)][t->month - 1];
  days += t->day - 1;

  return ((days * HOURS_PER_DAY + t->hour) * MINS_PER_HOUR + t->minute) *
             SECS_PER_MIN +
         t->second;
}

PSI_memory_key key_memory_changjiang;
static PSI_memory_info all_changjiang_memory[] = {
    {&key_memory_changjiang, "changjiang", 0, 0, PSI_DOCUMENT_ME},
};

void init_changjiang_psi_keys() {
  const char *category MY_ATTRIBUTE((unused)) = "changjiang";
  int count MY_ATTRIBUTE((unused));
  count = array_elements(all_changjiang_memory);
  mysql_memory_register(category, all_changjiang_memory, count);
}

}  // namespace Changjiang
