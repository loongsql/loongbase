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

#include "errmsg.h"
#include "m_string.h"
#include "my_base.h"
#include "mysql_version.h"

#include "include/mysql/plugin.h"
#include "include/my_dbug.h"

#include "sql/binlog.h"
#include "sql/item.h"
#include "sql/item_strfunc.h"
#include "sql/item_sum.h"
#include "sql/item_timefunc.h"
#include "sql/log.h"
#include "sql/nested_join.h"
#include "sql/query_result.h"
#include "sql/query_options.h"
#include "sql/rpl_replica.h"
#include "sql/sql_data_change.h"
#include "sql/sql_error.h"
#include "sql/sql_exchange.h"
#include "sql/sql_insert.h"
#include "sql/sql_optimizer.h"
#include "sql/sql_time.h"
#include "sql/sql_tmp_table.h"
#include "sql/sql_union.h"
#include "sql/tztime.h"

#define CALLER_INFO_PROTO /* nothing */
#define CALLER_INFO       /* nothing */

#include "storage/changjiang/common/hash.h"

extern char mysql_real_data_home[];

namespace Changjiang {

typedef class Item COND;
typedef struct TABLE st_table;
// changjiang
typedef bool my_bool;
#define DBUG_ASSERT(A) assert(A)
#define test(a) ((a) ? 1 : 0)
#define SKIP_OPEN_TABLE 0  // do not open table

extern char *log_error_file_ptr;

#define memcpy_fixed(A, B, C) memcpy((A), (B), (C))
#define bmove(d, s, n) memmove((d), (s), (n))

typedef decimal_digit_t dec1;
typedef longlong dec2;

#define DIG_PER_DEC1 9
#define DIG_MASK 100000000
#define DIG_BASE 1000000000
#define DIG_MAX (DIG_BASE - 1)
#define DIG_BASE2 ((dec2)DIG_BASE * (dec2)DIG_BASE)
#define ROUND_UP(X) (((X) + DIG_PER_DEC1 - 1) / DIG_PER_DEC1)

#define LL(A) A##LL
#define ULL(A) A##ULL

#ifndef strmov
// extern	char *strmov(char *dst,const char *src);
#define strmov strcpy
#endif

/** Struct to handle simple linked lists. */
typedef struct st_sql_list {
  uint elements;
  uchar *first;
  uchar **next;

  st_sql_list() {} /* Remove gcc warning */
  inline void empty() {
    elements = 0;
    first = 0;
    next = &first;
  }
  inline void link_in_list(uchar *element, uchar **next_ptr) {
    elements++;
    (*next) = element;
    next = next_ptr;
    *next = 0;
  }
  inline void save_and_clear(struct st_sql_list *save) {
    *save = *this;
    empty();
  }
  inline void push_front(struct st_sql_list *save) {
    *save->next = first; /* link current list last */
    first = save->first;
    elements += save->elements;
  }
  inline void push_back(struct st_sql_list *save) {
    if (save->first) {
      *next = save->first;
      next = save->next;
      elements += save->elements;
    }
  }
} SQL_LIST;

void init_changjiang_psi_keys();
extern PSI_memory_key key_memory_changjiang;

}  // namespace Changjiang
