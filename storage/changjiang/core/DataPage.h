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

// DataPage.h: interface for the DataPage class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Page.h"

namespace Changjiang {

class Dbb;
class Bdb;
class Stream;
class Section;

struct RecordIndex;
struct SectionAnalysis;

struct LineIndex {
  uint16 offset;
  int16 length;  // negative means record has overflow page
};

class DataPage : public Page {
 public:
  void analyze(Dbb *dbb, SectionAnalysis *analysis);
  void validate(Dbb *dbb, Validation *validation);
  void deleteOverflowPages(Dbb *dbb, int32 overflowPageNumber, TransId transId);
  void validate(Dbb *dbb);
  int deleteLine(Dbb *dbb, int line, TransId transId);
  int storeRecord(Dbb *dbb, Bdb *bdb, RecordIndex *index, int length,
                  Stream *stream, int32 overflowPage, TransId transId,
                  bool earlyWrite);
  bool fetchRecord(Dbb *dbb, int line, Stream *stream);
  int compressPage(Dbb *dbb);
  int updateRecord(Section *section, int line, Stream *stream, TransId transId,
                   bool earlyWrite);
  int computeSpaceAvailable(int pageSize);
  void deletePage(Dbb *dbb, TransId transId);
  void print(void);
  void backup(EncodedDataStream *stream);
  void restore(EncodedDataStream *stream);

  short maxLine;
  LineIndex lineIndex[1];
};

/* Maximal length of data that can be stored in a page */
#define DATA_PAGE_MAX_AVAILABLE_SPACE(pageSize) (pageSize - sizeof(DataPage))

}  // namespace Changjiang
