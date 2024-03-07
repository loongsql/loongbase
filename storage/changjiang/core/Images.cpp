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

// Images.cpp: implementation of the Images class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include "Engine.h"
#include "Images.h"
#include "Image.h"
#include "ImageManager.h"
#include "Application.h"
#include "Module.h"
#include "Database.h"
#include "ResultSet.h"
#include "SQLException.h"
#include "PreparedStatement.h"
#include "ResultSet.h"
#include "Sync.h"

namespace Changjiang {

#define HASH(address, size) (int)(((UIPTR)address >> 2) % size)

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Images::Images(ImageManager *mgr, const char *applicationName,
               Application *application) {
  manager = mgr;
  database = manager->database;
  name = database->getSymbol(applicationName);
  dbName = applicationName;
  children = NULL;
  memset(hashTable, 0, sizeof(hashTable));
  parent = NULL;
  images = NULL;
  modules = NULL;
  primary = NULL;

  if (application) {
    parent = manager->getImages(application->name, application->extends);
    for (Module *module = application->modules; module; module = module->next)
      addModule(module);
  }
}

Images::~Images() {
  for (Image *image; (image = images);) {
    images = image->next;
    image->release();
  }

  for (Images *images; (images = children);) {
    children = images->sibling;
    delete images;
  }
}

void Images::rehash() {
  if (parent)
    memcpy(hashTable, parent->hashTable, sizeof(hashTable));
  else
    memset(hashTable, 0, sizeof(hashTable));

  for (Images *module = modules; module; module = module->nextModule)
    for (Image *image = module->images; image; image = image->next) {
      int slot = HASH(image->name, IMAGE_HASH_SIZE);
      image->collision = hashTable[slot];
      hashTable[slot] = image;
    }

  for (Image *image = images; image; image = image->next) {
    int slot = HASH(image->name, IMAGE_HASH_SIZE);
    image->collision = hashTable[slot];
    hashTable[slot] = image;
  }

  for (Images *child = children; child; child = child->sibling) child->rehash();
}

Image *Images::findImage(const char *imageNameString) {
  const char *imageName = database->getSymbol(imageNameString);
  int slot = HASH(imageName, IMAGE_HASH_SIZE);

  for (Image *image = hashTable[slot]; image; image = image->collision)
    if (image->name == imageName) return image;

  return NULL;
}

void Images::insert(Image *image, bool rehash) {
  int slot = HASH(image->name, IMAGE_HASH_SIZE);
  image->collision = hashTable[slot];
  hashTable[slot] = image;

  if (rehash) {
    for (Images *child = children; child; child = child->sibling)
      child->rehash();
    if (primary) primary->rehash();
  }
}

void Images::load() {
  database->commitSystemTransaction();
  rehash();

  PreparedStatement *statement = database->prepareStatement(
      "select name,alias,width,height from system.images where application=?");
  statement->setString(1, dbName);
  ResultSet *resultSet = statement->executeQuery();

  while (resultSet->next()) {
    Image *image =
        new Image(database->getSymbol(resultSet->getString(1)),  // name
                  resultSet->getInt(3),                          // width
                  resultSet->getInt(4),                          // height
                  resultSet->getString(2), this);                // alias
    image->next = images;
    images = image;
    insert(image, false);
  }

  resultSet->close();
  statement->close();
}

void Images::deleteImage(const char *imageNameString) {
  const char *imageName = database->getSymbol(imageNameString);
  int slot = HASH(imageName, IMAGE_HASH_SIZE);

  for (Image *image, **ptr = hashTable + slot; (image = *ptr);
       ptr = &image->collision)
    if (image->name == imageName) {
      *ptr = image->collision;
      for (ptr = &images; *ptr; ptr = &(*ptr)->next)
        if (*ptr == image) {
          *ptr = image->next;
          break;
        }
      image->release();
      break;
    }

  for (Images *child = children; child; child = child->sibling) child->rehash();
}

void Images::addModule(Module *module) {
  Images *mod = manager->getImages(module->moduleSchema, NULL);

  for (Images *m = modules; m; m = m->nextModule)
    if (m == mod) return;

  mod->primary = this;
  mod->nextModule = modules;
  modules = mod;

  rehash();
}

}  // namespace Changjiang
