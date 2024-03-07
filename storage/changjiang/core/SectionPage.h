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

// SectionPage.h: interface for the SectionPage class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Page.h"

namespace Changjiang {

class Dbb;
class Bdb;
class Validation;
class Bitmap;

struct SectionAnalysis;

#define SECTION_FULL 1

class SectionPage : public Page {
 public:
  void analyze(Dbb *dbb, SectionAnalysis *analysis, int sectionId, int sequence,
               Bitmap *dataPages);
  void validateIndexes(Dbb *dbb, Validation *validation, int base);
  void validateSections(Dbb *dbb, Validation *validation, int base);
  void validate(Dbb *dbb, Validation *validation, int sectionId, int sequence,
                Bitmap *dataPages);
  void backup(EncodedDataStream *stream);
  void restore(EncodedDataStream *stream);

  int section;
  int sequence; /* sequence in level */
  short level;  /*	0 = root; */
  short flags;
  int32 pages[1];
};

}  // namespace Changjiang
