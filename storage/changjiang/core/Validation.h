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

// Validation.h: interface for the Validation class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "SyncObject.h"
#include "Page.h"
#include "Bitmap.h"

namespace Changjiang {

class Dbb;
class Bdb;

class Validation {
 public:
  bool minutia();
  bool isRepair();
  bool inUse(Bdb *bdb, const char *descrption);
  bool isPageType(Bdb *bdb, PageType type, const char *errorText, ...);
  bool error(const char *text, ...);
  bool warning(const char *text, ...);
  void sectionInUse(int sectionId);
  bool inUse(int32 pageNumber, const char *description);

  Validation(Dbb *db, int validationOptions);
  virtual ~Validation();

  Dbb *dbb;
  Bitmap pages;
  Bitmap sections;
  Bitmap duplicates;
  int phase;
  int options;
  int indexId;
  int32 highPage;
  int32 stopPage;
  bool dups;
  int errors;
  int pageCounts[(int)PAGE_max];
};

}  // namespace Changjiang
