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

// ImageManager.h: interface for the ImageManager class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "TableAttachment.h"

namespace Changjiang {

#define IMAGES_HASH_SIZE 101

class Images;
class Image;
class Database;
class Application;

class ImageManager : public TableAttachment {
 public:
  virtual void preDelete(Table *table, RecordVersion *record);
  void checkAccess(Table *table, RecordVersion *record);
  virtual void preUpdate(Table *table, RecordVersion *record);
  virtual void updateCommit(Table *table, RecordVersion *record);
  virtual void deleteCommit(Table *table, Record *record);
  Images *findImages(Table *table, Record *record);
  Images *findImages(const char *name);
  virtual void insertCommit(Table *table, RecordVersion *record);
  void assignAlias(Table *table, RecordVersion *record);
  virtual void preInsert(Table *table, RecordVersion *record);
  void tableAdded(Table *table);
  Images *getImages(const char *name, Application *extends);
  ImageManager(Database *db);
  virtual ~ImageManager();

  Database *database;
  Images *hashTable[IMAGES_HASH_SIZE];
  int applicationId;
  int nameId;
  int typeId;
  int imageId;
  int aliasId;
  int widthId;
  int heightId;
  int nextAlias;
};

}  // namespace Changjiang
