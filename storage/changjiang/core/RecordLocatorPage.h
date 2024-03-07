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

// SECTIONINDEXPAGE.h: interface for the SECTIONINDEXPAGE class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Page.h"

namespace Changjiang {

struct RecordIndex {
  int32 page;
  short line;
  short spaceAvailable;
};

class Validation;
class Bitmap;

struct SectionAnalysis;

class RecordLocatorPage : public Page {
 public:
  // RecordLocatorPage();
  //~RecordLocatorPage();

  void repair(Dbb *dbb, int sectionId, int sequence);
  void analyze(Dbb *dbb, SectionAnalysis *analysis, int sectionId, int Sequence,
               Bitmap *dataPages);
  bool validate(Dbb *dbb, Validation *validation, int sectionId, int sequence,
                Bitmap *dataPages);
  void deleteLine(int line, int spaceAvailable);
  int nextSpaceSlot(int priorSlot);
  int findSpaceSlot(int32 pageNumber);
  void validateSpaceSlots(void);
  void validateSpaceSlots(short linesPerPage);
  void corrupt(void);
  void printPage(void);
  void setIndexSlot(int slot, int32 page, int line, int availableSpace);
  void expungeDataPage(int32 pageNumber);
  void deleteDataPages(Dbb *dbb, TransId transId);
  void backup(EncodedDataStream *stream);
  void restore(EncodedDataStream *stream);

 protected:
  void insertSpaceSlot(int slot, int availableSpace);
  void unlinkSpaceSlot(int slot);
  void linkSpaceSlot(int from, int to);

 public:
  inline bool isSpaceSlot(int slot) {
    return elements[slot].spaceAvailable > 0;
  }

  int section;
  int sequence;
  int maxLine;
  RecordIndex elements[1];
};

}  // namespace Changjiang
