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

#ifndef MYSQL_SERVER
#define MYSQL_SERVER
#endif

#include <string.h>
#include "mysql_priv.h"
#include "InfoTable.h"

namespace Changjiang {

InfoTableImpl::InfoTableImpl(THD *thd, TABLE_LIST *tables, CHARSET_INFO *scs) {
  table = tables->table;
  mySqlThread = thd;
  charSetInfo = scs;
  error = 0;
}

InfoTableImpl::~InfoTableImpl(void) {}

void InfoTableImpl::putRecord(void) {
  error = schema_table_store_record(mySqlThread, table);
}

void InfoTableImpl::putInt(int column, int value) {
  table->field[column]->store(value, false);
}

void InfoTableImpl::putString(int column, const char *string) {
  table->field[column]->store(string, strlen(string), charSetInfo);
}

void InfoTableImpl::putInt64(int column, INT64 value) {
  table->field[column]->store(value, false);
}

void InfoTableImpl::putDouble(int column, double value) {
  table->field[column]->store(value);
}

void InfoTableImpl::putString(int column, unsigned int stringLength,
                              const char *string) {
  table->field[column]->store(string, stringLength, charSetInfo);
}

void InfoTableImpl::setNull(int column) { table->field[column]->set_null(); }

void InfoTableImpl::setNotNull(int column) {
  table->field[column]->set_notnull();
}

}  // namespace Changjiang
