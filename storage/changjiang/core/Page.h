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

// Page.h: interface for the Page class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "PageType.h"

namespace Changjiang {

#ifdef STORAGE_ENGINE
#define HAVE_PAGE_NUMBER
#endif

// Hardwired page numbers

#define HEADER_PAGE 0
#define PIP_PAGE 1
#define SECTION_ROOT 2
#define INDEX_ROOT 3

static const int END_BUCKET = -1;
static const int END_LEVEL = -2;

enum AddNodeResult {
  NodeAdded = 0,
  Duplicate,
  SplitMiddle,
  SplitEnd,
  NextPage
};

class Validation;
class Dbb;
class EncodedDataStream;

class Page {
 public:
  int16 pageType;
  uint16 checksum;

#ifdef HAVE_PAGE_NUMBER
  int32 pageNumber;
  int32 pageUnused;
#endif
};

}  // namespace Changjiang
