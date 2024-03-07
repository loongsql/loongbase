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

#include "CommonUtil.h"

#include <cstring>

namespace Changjiang {

void CommonUtil::GetNames(const char *table_path, char *db_buf, char *tab_buf,
                          int buf_size) {
  size_t path_len = strlen(table_path);
  unsigned last = -1, last_but_one = -1;
  for (unsigned i = 0; i < path_len; i++) {
    if (table_path[i] == '/' || table_path[i] == '\\') {
      last_but_one = last;
      last = i + 1;
    }
  }
  if (last_but_one == static_cast<unsigned>(-1) && last > 0) last_but_one = 0;
  if (last_but_one == (unsigned)(-1) || last - last_but_one > buf_size - 1 ||
      path_len - last > buf_size - 2)
    return;

  std::strncpy(db_buf, table_path + last_but_one, last - last_but_one - 1);
  std::strncpy(tab_buf, table_path + last, path_len - last);
}

}  // namespace Changjiang
