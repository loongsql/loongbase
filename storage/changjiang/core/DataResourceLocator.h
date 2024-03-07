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

// DataResourceLocator.h: interface for the DataResourceLocator class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

namespace Changjiang {

class PreparedStatement;
class ResultSet;
class Stream;
CLASS(Field);
class Table;
class ForeignKey;
class Index;
class TemplateContext;
class Connection;

class DataResourceLocator {
 public:
  static JString getLocator(ResultSet *resultSet, ForeignKey *key);
  static JString getLocator(ResultSet *resultSet, Index *index);
  static void genDrl(TemplateContext *context, Index *index);
  static void genDrl(TemplateContext *context, ForeignKey *key);
  static void copyValue(char **to, const char *from);
  PreparedStatement *prepareStatement(Connection *connection, const char *drl);
  static void copy(char **to, const char *from);
  bool getToken(char **pChar, char *token);
  DataResourceLocator();
  virtual ~DataResourceLocator();
};

}  // namespace Changjiang
