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

// ForeignKey.h: interface for the ForeignKey class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

namespace Changjiang {

static const int importedKeyCascade = 0;
static const int importedKeyRestrict = 1;
static const int importedKeySetNull = 2;
static const int importedKeyNoAction = 3;
static const int importedKeySetDefault = 4;
static const int importedKeyInitiallyDeferred = 5;
static const int importedKeyInitiallyImmediate = 6;
static const int importedKeyNotDeferrable = 7;

class Table;
CLASS(Field);
class Database;
class ResultSet;
class Transaction;
class Record;

class ForeignKey {
 public:
  void cascadeDelete(Transaction *transaction, Record *oldRecord);
  void setDeleteRule(int rule);
  void bindTable(Table *table);
  void deleteForeignKey();
  void create();
  void drop();
  bool matches(ForeignKey *key, Database *database);
  bool isMember(Field *field, bool foreign);
  void bind(Database *database);
  static void loadForeignKeys(Database *database, Table *table);
  static void loadPrimaryKeys(Database *database, Table *table);
  void loadRow(Database *database, ResultSet *resultSet);
  void save(Database *database);

  ForeignKey();
  ForeignKey(ForeignKey *key);
  ForeignKey(int cnt, Table *primary, Table *foreign);
  virtual ~ForeignKey();

  int numberFields;
  Table *primaryTable;
  Table *foreignTable;
  Field **primaryFields;
  Field **foreignFields;
  int primaryTableId;
  int foreignTableId;
  int *primaryFieldIds;
  int *foreignFieldIds;
  int deleteRule;
  JString deleteStatement;
  ForeignKey *next;  // next in Table

 protected:
};

}  // namespace Changjiang
