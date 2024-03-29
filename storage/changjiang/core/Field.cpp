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

// Field.cpp: implementation of the Field class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "Field.h"
#include "Table.h"
#include "PreparedStatement.h"
#include "Database.h"
#include "CollationManager.h"
#include "Collation.h"
#include "Sync.h"
#include "Value.h"
#include "Repository.h"

namespace Changjiang {

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

static const int boundaries[] = {
    1,  // Null,
    1,  // String,		// generic, null terminated
    1,  // Char,			// fixed length string, also null
        // terminated
    2,               // Varchar,		// variable length, counted string
    sizeof(short),   // Smallint,
    sizeof(int32),   // Integer,
    sizeof(int64),   // Quad
    sizeof(float),   // Float,
    sizeof(double),  // Double,
    sizeof(int64),   // Date,
    sizeof(int64),   // Timestamp,
    sizeof(int32),   // Time,
    sizeof(int32),   // AsciiBlob,
    sizeof(int32),   // BinaryBlob,
    sizeof(int32),   // BlobPtr,
    sizeof(int64),   // SqlTimestamp
    sizeof(int32),   // ClobPtr
    sizeof(int32),   // Biginteger
};

static const JdbcType jdbcTypes[] = {
    jdbcNULL,
    VARCHAR,   // String,		// generic, null terminated
    jdbcCHAR,  // Char,			// fixed length string, also null
               // terminated
    VARCHAR,     // Varchar,		// variable length, counted string
    SMALLINT,    // Smallint,
    INTEGER,     // Integer,
    BIGINT,      // Quad
    jdbcFLOAT,   // Float,
    jdbcDOUBLE,  // Double,
    jdbcDATE,    // Date,
    TIMESTAMP,   // Timestamp,
    jdbcTIME,    // Time,
    CLOB,        // AsciiBlob,
    jdbcBLOB,    // BinaryBlob,
    jdbcBLOB,    // BlobPtr,
    TIMESTAMP,   // SqlTimestamp
    jdbcCLOB,    // ClobPtr
    NUMERIC,     // Biginteger
};

static const char *typeNames[] = {
    "<null>",   // Null,
    "varchar",  // String,		// generic, null terminated
    "char",     // Char,			// fixed length string, also null
             // terminated
    "varchar",    // Varchar,		// variable length, counted string
    "smallint",   // Smallint,
    "int",        // Integer,
    "bigint",     // Quad
    "float",      // Float,
    "double",     // Double,
    "date",       // Date,
    "timestamp",  // Timestamp,
    "time",       // Time,
    "clob",       // AsciiBlob,
    "blob",       // BinaryBlob,
    "blob",       // BlobPtr,
    "timestamp",  // SqlTimestamp
    "clob",       // ClobPtr
    "numeric",    // Biginteger
};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Field::Field(Table *tbl, int fieldId, const char *fieldName, Type typ, int len,
             int prec, int scl, int flgs) {
  table = tbl;
  name = table->database->getString(fieldName);
  type = typ;
  length = len;
  id = fieldId;
  scale = scl;
  flags = flgs;
  foreignKey = NULL;
  precision = (prec) ? prec : getPrecision(type, length);
  collation = NULL;
  defaultValue = NULL;
  repository = NULL;
  domainName = NULL;
  indexPadByte = (isString()) ? ' ' : 0;
}

Field::~Field() {
  delete defaultValue;

  if (collation) collation->release();
}

const char *Field::getName() { return name; }

bool Field::getNotNull() { return flags & NOT_NULL; }

int Field::boundaryRequirement() { return boundaryRequirement(type); }

int Field::boundaryRequirement(Type type) { return boundaries[type]; }

int Field::getPhysicalLength() {
  switch (type) {
    case String:
      return length + 1;

    case Varchar:
      return length + 2;

    default:
      break;
  }

  return length;
}

int Field::getDisplaySize() {
  switch (type) {
    case Char:
    case Varchar:
    case String:
      return length;

    case Short:
      return 5;

    case Long:
      return 10;

    case Quad:
      return 20;

    case Double:
      return 10;

    default:
      return 16;
  }
}

void Field::makeSearchable(Transaction *transaction, bool populate) {
  if (flags & SEARCHABLE) return;

  flags |= SEARCHABLE;

  if (table->isCreated()) {
    update();
    if (populate) table->makeSearchable(this, transaction);
  }
}

void Field::makeNotSearchable(Transaction *transaction, bool populate) {
  if (!(flags & SEARCHABLE)) return;

  flags &= ~SEARCHABLE;

  if (table->isCreated()) {
    update();
    if (populate) table->makeNotSearchable(this, transaction);
  }
}

void Field::update() {
  Database *database = table->database;

  PreparedStatement *statement = database->prepareStatement(
      "update Fields set "
      "flags=?,datatype=?,scale=?,length=?,collationsequence=?,repositoryName=?"
      "\n"
      "where schema=? and tableName=? and field=?");

  int n = 1;
  statement->setInt(n++, flags);
  statement->setInt(n++, type);
  statement->setInt(n++, scale);
  statement->setInt(n++, length);

  if (collation)
    statement->setString(n++, collation->getName());
  else
    statement->setNull(n++, 0);

  if (repository)
    statement->setString(n++, repository->name);
  else
    statement->setNull(n++, 0);

  statement->setString(n++, table->schemaName);
  statement->setString(n++, table->name);
  statement->setString(n++, name);
  statement->executeUpdate();
  statement->close();

  database->commitSystemTransaction();
}

void Field::setNotNull() { flags |= NOT_NULL; }

JdbcType Field::getSqlType() { return jdbcTypes[type]; }

void Field::save() {
  Database *database = table->database;

  const char *sql =
      (database->fieldExtensions)
          ? "insert Fields (tableName, field, fieldId, datatype, scale, "
            "length, flags,schema,collationsequence,repositoryName, precision)"
            " values (?,?,?,?,?,?,?,?,?,?,?);"
          : "insert Fields (tableName, field, fieldId, datatype, scale, "
            "length, flags,schema,collationsequence)"
            " values (?,?,?,?,?,?,?,?,?);";

  PreparedStatement *statement = database->prepareStatement(sql);
  int n = 1;
  statement->setString(n++, table->name);
  statement->setString(n++, name);
  statement->setInt(n++, id);
  statement->setInt(n++, type);
  statement->setInt(n++, scale);
  statement->setInt(n++, length);
  statement->setInt(n++, flags);
  statement->setString(n++, table->schemaName);

  if (collation)
    statement->setString(n++, collation->getName());
  else
    statement->setNull(n++, 0);

  if (database->fieldExtensions) {
    if (repository)
      statement->setString(n++, repository->name);
    else
      statement->setNull(n++, 0);

    statement->setInt(n++, precision);
  }

  statement->executeUpdate();
  statement->release();
}

void Field::drop() {
  Database *database = table->database;

  PreparedStatement *statement = database->prepareStatement(
      "delete from Fields where tableName=? and schema=? and field=?");

  int n = 1;
  statement->setString(n++, table->name);
  statement->setString(n++, table->schemaName);
  statement->setString(n++, name);
  statement->executeUpdate();
  statement->release();
}

bool Field::isSearchable() { return (flags & SEARCHABLE) ? true : false; }

int Field::getPrecision(Type type, int length) {
  switch (type) {
    case String:
    case Char:
    case Varchar:
      return length;

    case Short:
      return 4;

    case Long:
      return 9;

    case Quad:
      return 19;

    case Float:
      return 15;

    case Double:
      return 15;

    case Date:
      return 10;

    case Timestamp:
      return 16;

    case TimeType:
      return 16;

    case Asciiblob:
    case Binaryblob:
      return 100;

    default:
      return 9;
  }
}

void Field::setCollation(Collation *newCollation) {
  collation = newCollation;
  // indexPadByte = collation->getPadChar();
  indexPadByte = 0;
}

void Field::setRepository(Repository *repo) { repository = repo; }

const char *Field::getSqlTypeName() { return typeNames[type]; }

bool Field::isString() {
  switch (type) {
    case String:
    case Varchar:
    case Asciiblob:
    case ClobPtr:
      return true;

    default:
      return false;
  }
}

}  // namespace Changjiang
