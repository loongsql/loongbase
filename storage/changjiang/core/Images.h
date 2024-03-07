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

// Images.h: interface for the Images class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

namespace Changjiang {

#define IMAGE_HASH_SIZE 101

class Database;
class Image;
class ImageManager;
class Extends;
class Application;
class Module;

class Images {
 public:
  void addModule(Module *module);
  void deleteImage(const char *imageName);
  void load();
  void insert(Image *image, bool rehash);
  Image *findImage(const char *imageName);
  void rehash();
  Images(ImageManager *manager, const char *applicationName,
         Application *application);
  virtual ~Images();

  const char *name;  // application name
  JString dbName;    // name in database
  Database *database;
  Image *images;
  Image *hashTable[IMAGE_HASH_SIZE];
  Images *parent;
  Images *children;
  Images *sibling;
  Images *collision;
  Images *modules;
  Images *nextModule;
  Images *primary;
  ImageManager *manager;
};

}  // namespace Changjiang
