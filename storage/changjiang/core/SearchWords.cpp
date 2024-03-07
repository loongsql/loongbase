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

// SearchWords.cpp: implementation of the SearchWords class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "SearchWords.h"
#include "Database.h"
#include "Table.h"
#include "SymbolManager.h"
#include "PreparedStatement.h"
#include "ResultSet.h"
#include "ValueEx.h"
#include "RecordVersion.h"

namespace Changjiang {

static const char *words[] = {"the", "a",  "an",   "and",  "or", "of",  "from",
                              "to",  "in", "this", "that", "is", "are", NULL};

static const char *ddl[] = {
    "upgrade table system.stop_words (\n"
    "word varchar (20) not null primary key collation case_insensitive)",
    "grant all on system.stop_words to public", NULL};

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SearchWords::SearchWords(Database *db) : TableAttachment(POST_COMMIT) {
  database = db;
  symbols = new SymbolManager;

  for (const char **word = words; *word; ++word) symbols->getString(*word);
}

SearchWords::~SearchWords() { delete symbols; }

void SearchWords::tableAdded(Table *table) {
  if (table->isNamed("SYSTEM", "STOP_WORDS")) table->addAttachment(this);
}

void SearchWords::initialize() {
  if (!database->findTable("SYSTEM", "STOP_WORDS"))
    for (const char **p = ddl; *p; ++p) database->execute(*p);

  PreparedStatement *statement =
      database->prepareStatement("select word from system.stop_words");
  ResultSet *resultSet = statement->executeQuery();

  while (resultSet->next()) symbols->getString(resultSet->getString(1));

  resultSet->close();
  statement->close();
}

bool SearchWords::isStopWord(const char *word) {
  return symbols->findString(word) != NULL;
}

JString SearchWords::getWord(Table *table, Record *record) {
  int wordId = table->getFieldId("WORD");
  ValueEx value(record, wordId);

  return value.getString();
}

void SearchWords::postCommit(Table *table, RecordVersion *record) {
  JString word = getWord(table, record);
  symbols->getString(word);
}

}  // namespace Changjiang
