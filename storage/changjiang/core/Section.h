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

// Section.h: interface for the Section class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "SyncObject.h"
#include "SparseArray.h"

namespace Changjiang {

class Dbb;
class Bdb;
class DataPage;
class Stream;
class Validation;
class Bitmap;
class RecordLocatorPage;
class SectionPage;
class Table;

struct RecordIndex;
struct SectionAnalysis;

class Section {
 public:
  static const int INVALID_SECTION_ID = -1;
  void redoDataPage(int32 pageNumber, int32 locatorPageNumber);
  Section(Dbb *dbb, int32 id, TransId transId);
  virtual ~Section();

  void reInsertStub(int32 recordNumber, TransId transId);
  void markFull(bool isFull, int sequence, TransId transId);
  void expungeRecord(int32 recordNumber);
  void analyze(SectionAnalysis *analysis, int pageNumber);
  int storeTail(Stream *stream, int maxLength, int *pLength, TransId transId,
                bool earlyWrite);
  int32 findNextRecord(int32 startingRecord, Stream *stream);
  int32 findNextRecord(int32 pageNumber, int32 startingRecord, Stream *stream);
  bool fetchRecord(int32 recordNumber, Stream *stream, TransId transId);
  void storeRecord(RecordLocatorPage *recordLocatorPage, int32 indexPageNumber,
                   RecordIndex *index, Stream *stream, TransId transId,
                   bool earlyWrite);
  int deleteLine(Bdb *bdb, int line, int32 sectionPageNumber, TransId transId,
                 RecordLocatorPage *locatorPage, int locatorLine);
  void updateRecord(int32 recordId, Stream *stream, TransId transId,
                    bool earlyWrite);
  void reserveRecordNumber(int32 recordNumber);
  int32 insertStub(TransId transId);
  int32 getSectionRoot();
  Bdb *getSectionPage(int sequence, LockType lockType, TransId transId);
  Bdb *fetchLocatorPage(int32 root, int32 recordNumber, LockType lockType,
                        TransId transId);
  // void		setRecordFlag(int32 recordNumber);
  // void		clearRecordFlag(int32 recordNumber);
  // bool		checkRecordFlag(int32 recordNumber);

  static void redoSectionPromotion(Dbb *dbb, int sectionId,
                                   int32 rootPageNumber, int pageLength,
                                   const UCHAR *pageData, int32 newPageNumber);
  void redoRecordLocatorPage(int sequence, int32 pageNumber, bool isPostFlush);
  static void redoSectionLine(Dbb *dbb, int32 pageNumber, int32 dataPageNumber);
  static void redoBlobDelete(Dbb *dbb, int32 locatorPage, int locatorLine,
                             int32 dataPage, int dataLine, bool dataPageActive);

  static Bdb *getSectionPage(Dbb *dbb, int32 root, int32 sequence,
                             LockType lockType, TransId transId);
  static int32 createSection(Dbb *dbb, TransId transId);
  static bool decomposeSequence(Dbb *dbb, int32 sequence, int level,
                                int *slots);
  static int32 getMaxPage(int32 root, Dbb *dbb);
  static void deleteSectionLevel(Dbb *dbb, int32 pageNumber, TransId transId);
  static void deleteSection(Dbb *dbb, int32 sectionId, TransId transId);
  static void createSection(Dbb *dbb, int32 sectionId, TransId transId);
  static void redoSectionPage(Dbb *dbb, int32 parentPage, int32 pageNumber,
                              int slot, int sectionId, int sequence, int level);
  static bool dataPageInUse(Dbb *dbb, int32 recordLocatorPage, int32 dataPage);
  static void redoBlobUpdate(Dbb *dbb, int32 locatorPage, int locatorLine,
                             int32 dataPage, int dataLine);

  static void validate(RecordLocatorPage *locatorPage, Bdb *dataPageBdb);
  static void validateIndexes(Dbb *dbb, Validation *validation);
  static void validate(Dbb *dbb, Validation *validation, int sectionId,
                       int rootPage);
  static void validateSections(Dbb *dbb, Validation *validation);

  SyncObject syncObject;
  SyncObject syncInsert;
  int32 sectionId;
  int32 nextLine;
  int32 root;
  Section *hash;
  Dbb *dbb;
  Table *table;  // if known
  Bitmap *reservedRecordNumbers;
  Bitmap *freeLines;
  // Bitmap		*records;					// save for
  // debugging
  short level;
  SparseArray<int, 100> sectionPages;
};

}  // namespace Changjiang
